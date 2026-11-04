#include "Check.h"
#include "adapters/mariadb/MariaDBEventStore.h"
#include "core/Status.h"
#include "persistence/EventQueue.h"

#include <charconv>
#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <cstdlib>
#include <memory>
using namespace coreci;
using namespace coreci::persistence;
using namespace coreci::mariadb;

Pipeline pipeline() {
    Pipeline::Builder b;
    b.feed({"f1", {"owner", "feeds", "publish.yml", "stable"}})
        .feed({"f2", {"owner", "feeds", "publish.yml", "stable"}})
        .target({"t1", Family::Debian, "f1"})
        .target({"t2", Family::Fedora, "f2"})
        .package({"a", {"owner", "a"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {}})
        .package({"b", {"owner", "b"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"a"}})
        .package({"c", {"owner", "c"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"a"}})
        .package({"d", {"owner", "d"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"b", "c"}})
        .package({"e", {"owner", "e"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1"}, {"d"}});
    return b.build();
}
Engine engine() {
    return Engine(pipeline(), {100, 50, 8, {}});
}
Command build(const std::string& package, const std::string& target, Inputs inputs = {}) {
    return DispatchBuild{package + "." + target + ".1.0.0.r1",
                         1,
                         package,
                         {"owner", package},
                         "v1.0.0",
                         *Version::parse("1.0.0"),
                         1,
                         target,
                         inputs,
                         {"owner", "recipes", "build.yml", "trunk"}};
}
Command publish(const std::string& package, const std::string& target) {
    return DispatchPublish{package + "." + target + ".1.0.0.r1",
                           1,
                           package,
                           *Version::parse("1.0.0"),
                           1,
                           target,
                           target == "t1" ? "f1" : "f2",
                           {"owner", "recipes"},
                           101,
                           "artifact",
                           {"owner", "feeds", "publish.yml", "stable"}};
}
Inputs inputs(const std::string& package) {
    return {{package, {*Version::parse("1.0.0"), 1}}};
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
        sql("DROP TABLE IF EXISTS coreci_events", [this] {
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
            [this](const auto& commands) {
                emitted.insert(emitted.end(), commands.begin(), commands.end());
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
            CHECK(emitted.empty());
            for (const auto& p : {"b", "c", "d", "e"}) {
                steps.push_back({TagObserved{100, p, "v1.0.0", std::string(40, 'a'), Origin::Webhook}, {}});
            }
            steps.push_back({TagObserved{100, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}, {build("a", "t1"), build("a", "t2")}});
            addBuild("a", "t1");
            addPublish("a", "t1", {build("b", "t1", inputs("a")), build("c", "t1", inputs("a"))});
            addBuild("b", "t1");
            addPublish("b", "t1", {});
            addBuild("c", "t1");
            const Inputs diamond{{"b", {*Version::parse("1.0.0"), 1}}, {"c", {*Version::parse("1.0.0"), 1}}};
            addPublish("c", "t1", {build("d", "t1", diamond)});
            addBuild("a", "t2");
            addPublish("a", "t2", {build("b", "t2", inputs("a")), build("c", "t2", inputs("a"))});
            addBuild("d", "t1"); // restart with both build and publish dispatches pending
            next();
        });
    }
    void addBuild(const std::string& p, const std::string& t) {
        const auto id = p + "." + t + ".1.0.0.r1";
        steps.push_back({DispatchAccepted{100, id, Kind::Build, 1}, {}});
        steps.push_back({BuildStarted{100, id, 101, 1}, {}});
        steps.push_back({BuildSucceeded{100, id, 101, "artifact"}, {publish(p, t)}});
    }
    void addPublish(const std::string& p, const std::string& t, std::vector<Command> commands) {
        const auto id = p + "." + t + ".1.0.0.r1";
        steps.push_back({DispatchAccepted{100, id, Kind::Publish, 1}, {}});
        steps.push_back({PublishSucceeded{100, id, 201, std::string(40, 'b')}, std::move(commands)});
    }
    void next() {
        if (index == steps.size()) {
            before = statusText(*queue->engine());
            CHECK_EQ(statuses(*queue->engine()).at({"d", "t2"}), "Waiting(on b@t2)");
            later([this] {
                queue.reset(); // destroys the first engine
                store.reset(); // destroys the first SNode.C client and its connection
                emitted.clear();
                fresh([this] {
                    CHECK_EQ(statusText(*queue->engine()), before);
                    CHECK((emitted ==
                           std::vector<Command>{publish("d", "t1"), build("b", "t2", inputs("a")), build("c", "t2", inputs("a"))}));
                    checkRows();
                });
            });
        } else {
            emitted.clear();
            queue->enqueue(steps[index].event, [this](Result result) {
                CHECK(std::holds_alternative<Accepted>(result));
                CHECK(emitted == steps[index].commands);
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
                nulRow();
            },
            [this](const auto& failure) {
                error(failure.message);
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
                sql("INSERT INTO coreci_schema (version) VALUES (2)", [this] {
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
                sql("INSERT INTO coreci_schema (version) VALUES (1)", [this] {
                    multipleSchema();
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
                sql("DROP TABLE coreci_events", [this] {
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
    std::vector<Command> emitted;
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
