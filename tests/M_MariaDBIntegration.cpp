#include "Fixture.h"
#include "adapters/mariadb/MariaDBStateStore.h"
#include "persistence/StateSession.h"

#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <cstdlib>

using namespace coreci;
using namespace coreci::persistence;

namespace {
    std::string env(const char* name) {
        const auto* value = std::getenv(name);
        return value ? value : "";
    }
    class Integration {
    public:
        explicit Integration(database::mariadb::MariaDBConnectionDetails details)
            : details(std::move(details))
            , admin(this->details, [](const auto&) {
            }) {
        }
        void start() {
            admin.connect();
            std::string tables = "coreci_schema,coreci_log";
            for (const auto& table : stateTables()) {
                tables += "," + table.name;
            }
            sql("DROP TABLE IF EXISTS " + tables, [this] {
                fresh();
            });
        }
        bool finished = false;

    private:
        void sql(const std::string& query, std::function<void()> done) {
            admin.exec(query, std::move(done), [this](const auto&, auto) {
                fail();
            });
        }
        void fail() {
            CHECK(false);
            core::SNodeC::stop();
        }
        void fresh() {
            store = std::make_unique<mariadb::MariaDBStateStore>(details);
            session = std::make_unique<StateSession>(
                *store,
                Options{0, 100, 3, 0},
                [](auto) {
                },
                [] {
                    return true;
                });
            session->start(
                [this] {
                    History expected({0, 100, 3, 0});
                    expected.load(100);
                    const auto wave = expected.wave();
                    expected.apply(WaveStarted{101, wave.id, 42, 1});
                    for (const auto& entry : wave.plan.at("t1")) {
                        expected.apply(UnitBuildSucceeded{102, wave.id, entry.buildId});
                    }
                    expected.apply(TargetFinished{103, wave.id, "t1", "artifact"});
                    baseline = expected.engine.stored();
                    const auto before = session->engine()->stored();
                    store->commit(
                        before,
                        baseline,
                        "fixture state transaction",
                        [this] {
                            store->load(
                                [this](StoredState loaded) {
                                    CHECK(loaded == baseline);
                                    CHECK_EQ(WaveController({}, loaded).commands().size(), 1U);
                                    reconnect();
                                },
                                [this](const auto&) {
                                    fail();
                                });
                        },
                        [this](const auto&) {
                            fail();
                        });
                },
                [this](const auto&) {
                    fail();
                });
        }
        void reconnect() {
            core::timer::Timer::singleshotTimer(
                [this] {
                    session.reset();
                    store.reset();
                    store = std::make_unique<mariadb::MariaDBStateStore>(details);
                    store->open(
                        [this] {
                            store->load(
                                [this](StoredState loaded) {
                                    CHECK(loaded == baseline);
                                    CHECK(WaveController({}, loaded).commands()[0].publication);
                                    // Display history is not an input to state loading.
                                    sql("UPDATE coreci_log SET description=_utf8mb4 X'610062'", [this] {
                                        store->load(
                                            [this](StoredState state) {
                                                CHECK(state == baseline);
                                                atomicFailure();
                                            },
                                            [this](const auto&) {
                                                fail();
                                            });
                                    });
                                },
                                [this](const auto&) {
                                    fail();
                                });
                        },
                        [this](const auto&) {
                            fail();
                        });
                },
                0);
        }
        void atomicFailure() {
            sql("RENAME TABLE coreci_units TO unavailable_units", [this] {
                auto candidate = baseline;
                ++candidate.sequence;
                candidate.unitStates.begin()->second.requested = true;
                store->commit(
                    baseline,
                    candidate,
                    "must roll back",
                    [this] {
                        fail();
                    },
                    [this](const auto& error) {
                        CHECK(error.kind == StoreError::Kind::Unavailable);
                        sql("RENAME TABLE unavailable_units TO coreci_units", [this] {
                            store->open(
                                [this] {
                                    store->load(
                                        [this](StoredState loaded) {
                                            CHECK(loaded == baseline);
                                            corrupt();
                                        },
                                        [this](const auto&) {
                                            fail();
                                        });
                                },
                                [this](const auto&) {
                                    fail();
                                });
                        });
                    });
            });
        }
        void corrupt() {
            // Reject NUL before typed decoding, then orphan relationships.
            sql("UPDATE coreci_units SET artifact=_utf8mb4 X'610062' WHERE package='a'", [this] {
                store->load(
                    [this](auto) {
                        fail();
                    },
                    [this](const auto& error) {
                        CHECK(error.kind == StoreError::Kind::Corrupt);
                        sql("UPDATE coreci_units SET artifact='' WHERE package='a'", [this] {
                            store->open(
                                [this] {
                                    sql("INSERT INTO coreci_inputs VALUES ('invalid','','a','t1','q','1.0.0',1)", [this] {
                                        store->load(
                                            [this](auto) {
                                                fail();
                                            },
                                            [this](const auto& error) {
                                                CHECK(error.kind == StoreError::Kind::Corrupt);
                                                sql("DELETE FROM coreci_inputs WHERE context='invalid'", [this] {
                                                    schema(0);
                                                });
                                            });
                                    });
                                },
                                [this](const auto&) {
                                    fail();
                                });
                        });
                    });
            });
        }
        void schema(unsigned index) {
            const std::vector<std::string> invalid{"UPDATE coreci_schema SET version=5",
                                                   "UPDATE coreci_schema SET version=123",
                                                   "DELETE FROM coreci_schema",
                                                   "INSERT INTO coreci_schema VALUES (7),(8)"};
            if (index == invalid.size()) {
                finished = true;
                core::timer::Timer::singleshotTimer(
                    [this] {
                        store.reset();
                        admin.abort();
                        core::SNodeC::stop();
                    },
                    0);
            } else {
                sql(invalid[index], [this, index] {
                    store->open(
                        [this] {
                            fail();
                        },
                        [this, index](const auto& error) {
                            CHECK(error.kind == StoreError::Kind::Corrupt);
                            schema(index + 1);
                        });
                });
            }
        }
        database::mariadb::MariaDBConnectionDetails details;
        database::mariadb::MariaDBClient admin;
        std::unique_ptr<mariadb::MariaDBStateStore> store;
        std::unique_ptr<StateSession> session;
        StoredState baseline;
    };
} // namespace
int main(int argc, char** argv) {
    int result = 77;
    if (!env("CORECI_TEST_DB_HOST").empty() && !env("CORECI_TEST_DB_USER").empty() && !env("CORECI_TEST_DB_NAME").empty()) {
        core::SNodeC::init(argc, argv);
        database::mariadb::MariaDBConnectionDetails details;
        details.connectionName = "state-integration";
        details.hostname = env("CORECI_TEST_DB_HOST");
        details.port = static_cast<unsigned>(std::stoul(env("CORECI_TEST_DB_PORT")));
        details.socket = MYSQL_UNIX_ADDR;
        details.username = env("CORECI_TEST_DB_USER");
        details.password = env("CORECI_TEST_DB_PASSWORD");
        details.database = env("CORECI_TEST_DB_NAME");
        Integration test(details);
        test.start();
        core::SNodeC::start();
        CHECK(test.finished);
        result = checkResult();
    }
    return result;
}
