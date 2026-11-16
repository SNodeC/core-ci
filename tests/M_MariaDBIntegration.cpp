#include "Fixture.h"
#include "adapters/mariadb/MariaDBEventStore.h"
#include "core/Status.h"
#include "persistence/EventQueue.h"
#include "tests/Status.h"

#include <charconv>
#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <cstdlib>
#include <memory>
using namespace coreci;
using namespace coreci::persistence;
using namespace coreci::mariadb;

Pipeline pipeline() {
    return testPipeline();
}
Engine engine() {
    return Engine({5, 100, 8, 0});
}
class Integration {
public:
    explicit Integration(database::mariadb::MariaDBConnectionDetails details)
        : details(std::move(details)) {
    }
    void start() {
        admin = std::make_unique<database::mariadb::MariaDBClient>(details, [this](const auto& state) {
            if (state.error != 0) {
                error(state.errorMessage);
            }
        });
        admin->connect();
        sql("DROP TABLE IF EXISTS "
            "coreci_events,coreci_graph_packages,coreci_graph_targets,coreci_graph_prerequisites,coreci_graph_releases",
            [this] {
                sql("DROP TABLE IF EXISTS coreci_schema", [this] {
                    first();
                });
            });
    }
    bool finished = false;

private:
    struct Step {
        Event event;
        std::vector<Command> commands;
    };
    void later(std::function<void()> callback) {
        // Leave the active framework callback before destroying its owners.
        core::timer::Timer::singleshotTimer(std::move(callback), 0);
    }
    void cleanup() {
        later([this] {
            queue.reset();
            store.reset();
            admin->abort();
            admin.reset();
        });
    }
    void error(const std::string& message) {
        std::cerr << __FILE__ << ':' << __LINE__ << ": expected successful MariaDB operation got " << message << '\n';
        CHECK(false);
        cleanup();
    }
    void sql(const std::string& statement, std::function<void()> done) {
        admin->exec(statement, std::move(done), [this](const std::string& message, unsigned int) {
            error(message);
        });
    }
    void fresh(std::function<void()> done) {
        store = std::make_unique<MariaDBEventStore>(details);
        queue = std::make_unique<EventQueue>(
            *store,
            engine,
            [] {
                return Resumed{100};
            },
            [](auto, const auto&) {
                CHECK(false);
            });
        queue->start(std::move(done), [this](const auto& failure) {
            error(failure.message);
        });
    }
    void first() {
        fresh([this] {
            CHECK(queue->engine()->commands().empty());
            std::vector<TagObserved> tags;
            for (const auto& p : {"b", "c", "d", "e"}) {
                tags.push_back({100, p, "v1.0.0", std::string(40, 'a'), Origin::Webhook});
            }
            steps.push_back({GraphChanged{100, pipeline(), tags}, {}});
            steps.push_back({TagObserved{100, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}, {}});
            steps.push_back({Tick{105}, {DispatchWave{"w4", 1, {"SNodeC", "Packages", "main"}, "feed"}}});
            steps.push_back({WaveDispatchAccepted{106, "w4", 1}, {}});
            steps.push_back({WaveStarted{107, "w4", 101, 2}, {}});
            steps.push_back({UnitBuildStarted{108, "w4", "w4~a~t1"}, {}});
            steps.push_back({UnitBuildSucceeded{109, "w4", "w4~a~t1", "artifact"}, {}});
            steps.push_back({UnitPublishSucceeded{110, "w4", "w4~a~t1", std::string(40, 'b')}, {}});
            next();
        });
    }
    void next() {
        if (index == steps.size()) {
            before = recordedStatus(*queue->engine());
            CHECK(queue->engine()->activeWave());
            later([this] {
                queue.reset(); // destroys the first engine
                store.reset(); // destroys the first SNode.C client and its connection
                fresh([this] {
                    CHECK_EQ(recordedStatus(*queue->engine()), before);
                    CHECK(queue->engine()->commands().empty());
                    CHECK(queue->engine()->activeWave());
                    checkRows();
                });
            });
        } else {
            queue->enqueue(steps[index].event, [this](Result result) {
                CHECK(std::holds_alternative<Accepted>(result));
                CHECK(queue->engine()->commands() == steps[index].commands);
                ++index;
                next();
            });
        }
    }
    void checkRows() {
        std::uint64_t previous = 0;
        auto last = std::make_shared<std::uint64_t>(previous);
        auto count = std::make_shared<std::size_t>(0);
        store->loadAll(
            [last, count](auto seq, const auto&) {
                CHECK(seq > *last);
                *last = seq;
                ++*count;
            },
            [this, count] {
                CHECK_EQ(*count, steps.size() + 2); // both persisted Resumed events
                graphRows();
            },
            [this](const auto& failure) {
                error(failure.message);
            });
    }
    void graphRows() {
        Pipeline::Builder b;
        b.target({"t"})
            .package({"a", {"SNodeC", "lower", "master"}, {"t"}, {}, 100})
            .package({"A", {"SNodeC", "upper", "master"}, {"t"}, {"a"}, 1000000});
        const auto expected = b.build();
        store->append(
            GraphChanged{200, expected},
            [this, expected](std::uint64_t committedSeq) {
                CHECK(committedSeq > 0);
                const auto found = std::make_shared<bool>(false);
                store->loadAll(
                    [expected, found, committedSeq](auto seq, const Event& event) {
                        if (const auto* changed = std::get_if<GraphChanged>(&event); changed && changed->at == 200) {
                            CHECK_EQ(seq, committedSeq);
                            CHECK(changed->pipeline == expected);
                            CHECK_EQ(changed->pipeline.packages().at("a").sourceRepo.owner, "SNodeC");
                            CHECK_EQ(changed->pipeline.packages().at("a").revisionBase, 100U);
                            CHECK_EQ(changed->pipeline.packages().at("A").revisionBase, 1000000U);
                            *found = true;
                            CHECK_EQ(changed->pipeline.packages().size(), 2U);
                        }
                    },
                    [this, found] {
                        CHECK(*found);
                        atomicFailure();
                    },
                    [this](const auto& failure) {
                        error(failure.message);
                    });
            },
            [this](const auto& failure) {
                error(failure);
            });
    }
    void atomicFailure() {
        sql("RENAME TABLE coreci_graph_targets TO coreci_graph_targets_unavailable", [this] {
            const auto restore = [this](bool failed) {
                CHECK(failed);
                later([this] {
                    sql("RENAME TABLE coreci_graph_targets_unavailable TO coreci_graph_targets", [this] {
                        store->open(
                            [this] {
                                const auto graphs = std::make_shared<unsigned>(0);
                                store->loadAll(
                                    [graphs](auto, const Event& event) {
                                        if (std::holds_alternative<GraphChanged>(event)) {
                                            ++*graphs;
                                        }
                                    },
                                    [this, graphs] {
                                        CHECK_EQ(*graphs, 2U);
                                        std::cout << "GraphChanged transaction error: connection aborted; no partial event or graph rows "
                                                     "persisted\n";
                                        orphanRow();
                                    },
                                    [this](const auto& failure) {
                                        error(failure.message);
                                    });
                            },
                            [this](const auto& failure) {
                                error(failure.message);
                            });
                    });
                });
            };
            store->append(
                GraphChanged{201, pipeline()},
                [restore](std::uint64_t) {
                    restore(false);
                },
                [restore](const auto&) {
                    restore(true);
                });
        });
    }
    void orphanRow() {
        sql("INSERT INTO coreci_graph_targets (event_seq,package,target) VALUES (9999,'a','t')", [this] {
            store->loadAll(
                [](auto, const auto&) {
                    CHECK(false);
                },
                [this] {
                    error("orphan accepted");
                },
                [this](const auto& failure) {
                    CHECK(failure.kind == EventStore::StoreError::Kind::Corrupt);
                    CHECK(failure.message.find("seq 9999:") != std::string::npos);
                    sql("DELETE FROM coreci_graph_targets WHERE event_seq=9999", [this] {
                        store->open(
                            [this] {
                                nulRow();
                            },
                            [this](const auto& failure) {
                                error(failure.message);
                            });
                    });
                });
        });
    }
    void nulRow() {
        sql("INSERT INTO coreci_events (seq,recorded_at,event_type,package,tag,source_sha,origin,event_at) "
            "VALUES (9001,UTC_TIMESTAMP(3),_utf8mb4 X'5461674f62736572766564',_utf8mb4 X'610062',"
            "_utf8mb4 X'76312e302e30',_utf8mb4 X'61616161616161616161616161616161616161616161616161616161616161616161616161616161',"
            "_utf8mb4 X'776562686f6f6b',100)",
            [this] {
                store->loadAll(
                    [](auto seq, const auto&) {
                        CHECK(seq != 9001);
                    },
                    [this] {
                        error("embedded NUL was accepted");
                    },
                    [this](const auto& failure) {
                        CHECK(failure.kind == EventStore::StoreError::Kind::Corrupt);
                        CHECK(failure.message.find("seq 9001:") != std::string::npos);
                        sql("DELETE FROM coreci_events WHERE seq = 9001", [this] {
                            sql("DELETE FROM coreci_schema", [this] {
                                missingSchema();
                            });
                        });
                    });
            });
    }
    void missingSchema() {
        store->open(
            [this] {
                error("events without schema version were accepted");
            },
            [this](const auto& failure) {
                CHECK(failure.kind == EventStore::StoreError::Kind::Corrupt);
                sql("INSERT INTO coreci_schema (version) VALUES (1)", [this] {
                    unknownSchema();
                });
            });
    }
    void unknownSchema() {
        store->open(
            [this] {
                error("unknown schema version was accepted");
            },
            [this](const auto& failure) {
                CHECK(failure.kind == EventStore::StoreError::Kind::Corrupt);
                sql("DELETE FROM coreci_schema", [this] {
                    sql("INSERT INTO coreci_schema (version) VALUES (2)", [this] {
                        store->open(
                            [this] {
                                error("version 2 was accepted");
                            },
                            [this](const auto& error) {
                                CHECK(error.kind == EventStore::StoreError::Kind::Corrupt);
                                sql("INSERT INTO coreci_schema (version) VALUES (3)", [this] {
                                    multipleSchema();
                                });
                            });
                    });
                });
            });
    }
    void multipleSchema() {
        store->open(
            [this] {
                error("multiple schema versions were accepted");
            },
            [this](const auto& failure) {
                CHECK(failure.kind == EventStore::StoreError::Kind::Corrupt);
                sql("DROP TABLE "
                    "coreci_events,coreci_graph_packages,coreci_graph_targets,coreci_graph_prerequisites,coreci_graph_"
                    "releases",
                    [this] {
                        sql("DROP TABLE coreci_schema", [this] {
                            finished = true;
                            std::cout << "S1 replay, fresh-client restart, Resumed and corruption checks completed\n";
                            cleanup();
                        });
                    });
            });
    }
    database::mariadb::MariaDBConnectionDetails details;
    std::unique_ptr<database::mariadb::MariaDBClient> admin;
    std::unique_ptr<MariaDBEventStore> store;
    std::unique_ptr<EventQueue> queue;
    std::vector<Step> steps;
    std::size_t index = 0;
    std::string before;
};
std::string env(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? "" : value;
}
int main(int argc, char** argv) {
    int result = 77;
    if (env("CORECI_TEST_DB_HOST").empty() || env("CORECI_TEST_DB_USER").empty() || env("CORECI_TEST_DB_NAME").empty()) {
        std::cout << "Skipped: CORECI_TEST_DB_HOST, CORECI_TEST_DB_USER and CORECI_TEST_DB_NAME must be set\n";
    } else {
        database::mariadb::MariaDBConnectionDetails details;
        details.connectionName = "coreci-integration";
        details.hostname = env("CORECI_TEST_DB_HOST");
        // SNode.C passes socket.c_str() rather than nullptr: use the installed client default.
        details.socket = MYSQL_UNIX_ADDR;
        details.username = env("CORECI_TEST_DB_USER");
        details.password = env("CORECI_TEST_DB_PASSWORD");
        details.database = env("CORECI_TEST_DB_NAME");
        const auto port = env("CORECI_TEST_DB_PORT");
        bool valid = true;
        if (!port.empty()) {
            const auto parsed = std::from_chars(port.data(), port.data() + port.size(), details.port);
            valid = parsed.ec == std::errc{} && parsed.ptr == port.data() + port.size() && details.port <= 65535;
        }
        CHECK(valid);
        if (valid) {
            core::SNodeC::init(argc, argv);
            Integration test(details);
            test.start();
            CHECK_EQ(core::SNodeC::start(), 0);
            CHECK(test.finished);
        }
        result = checkResult();
    }
    return result;
}
