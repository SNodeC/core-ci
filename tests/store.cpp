#include "sql.h"

#include <core/EventReceiver.h>
#include <core/SNodeC.h>
#include <fstream>
#include <iostream>
#include <utils/Config.h>

ci::Json projectGraph(const ci::Json& graph) {
    ci::Json result = {{"repositories", ci::projectRepositories(graph.at("repositories"))}, {"executions", ci::Json::object()}};
    for (std::size_t i = 0; i < graph.at("executions").size(); ++i) {
        const auto& work = graph.at("executions").at(i);
        result["executions"][work.at("id").get<std::string>()] = ci::projectWork(work, i);
    }
    return result;
}

int main(int argc, char* argv[]) {
    ci::Store store;
    int result = 0;
    unsigned int effects = 0;
    unsigned int completions = 0;
    unsigned int connections = 0;
    unsigned int notifications = 0;
    std::size_t firstWrites = 0;
    ci::Json displayed = {{"repositories", ci::Json::object()}, {"executions", ci::Json::object()}};
    bool started = false;
    const std::string mode = std::getenv("CI_TEST_MODE");
    const std::string socket = std::getenv("CI_TEST_SOCKET");
    const std::string database = std::getenv("CI_TEST_DATABASE");
    const ci::Done finish = [&](const std::string& error) {
        if (!error.empty() || connections != 1) {
            std::cerr << error << " (native connections=" << connections << ")\n";
            result = 1;
        }
        core::SNodeC::stop();
    };
    const auto boundary = [&](const std::function<void(MYSQL*)>& operation) {
        std::unique_ptr<MYSQL, decltype(&mysql_close)> reader(mysql_init(nullptr), mysql_close);
        ci::require(mysql_real_connect(reader.get(), "localhost", "root", "", database.c_str(), 0, std::getenv("CI_TEST_BACKEND"), 0) !=
                        nullptr,
                    "Independent SQL connection failed");
        operation(reader.get());
    };
    store.dispatch = [&](const ci::Json& actions) {
        boundary([&](MYSQL* reader) {
            auto persisted = fixture::sqlGraph(fixture::sqlRows(reader));
            for (const auto& action : actions) {
                const auto& active = ci::current(fixture::work(persisted, action.at("execution")));
                ci::require(active.at("id") == action.at("attempt") && active.at("assignment") == action.at("assignment") &&
                                active.at(action.at("stage").get<std::string>() + "_assignment") == action.at("command"),
                            "Mutation authority must be visible on an independent connection before dispatch");
                for (const auto& input : active.at("assignment").at("dependencies")) {
                    const auto& prerequisite = ci::current(fixture::work(persisted, input.at("execution")));
                    ci::Json expected = prerequisite.at("receipt");
                    ci::require(expected.dump().size() > 60000 && active.at("assignment").dump().size() < 10000,
                                "Large index inventory remains durable without bloating build inputs");
                    expected.erase("indexes");
                    ci::require(prerequisite.at("state") == "published" && prerequisite.at("id") == input.at("attempt") &&
                                    expected == input.at("receipt"),
                                "Prerequisite proof and dependent authority commit together");
                }
                ++effects;
            }
        });
    };
    store.changed = [&](const ci::Json& updates) {
        if (mode == "projection") {
            ci::require(updates.is_null(), "Projection failure is an internal resync marker");
            ++notifications;
        } else {
            ci::require(updates.is_array() && !updates.empty(), "Only nonempty field updates are notified");
            displayed = displayed.patch(updates);
            ++notifications;
            boundary([&](MYSQL* reader) {
                ci::require(projectGraph(fixture::sqlGraph(fixture::sqlRows(reader))) == displayed,
                            "Updates reconstruct independently visible committed state");
            });
        }
    };
    core::SNodeC::init(argc, argv);
    core::EventReceiver::atNextTick([&]() {
        const database::mariadb::MariaDBConnectionDetails details = {.connectionName = "test",
                                                                     .hostname = "localhost",
                                                                     .username = "root",
                                                                     .password = "",
                                                                     .database = database,
                                                                     .port = 0,
                                                                     .socket = socket,
                                                                     .flags = 0};
        store.db = std::make_unique<database::mariadb::MariaDBClient>(
            details,
            [&](const database::mariadb::MariaDBState& state) {
                if (state.connected && state.error == 0) {
                    ++connections;
                }
                if (state.connected && !started) {
                    started = true;
                    store.initialize([&](const std::string& error) {
                        try {
                            ci::require(error.empty(), "Initialize: " + error);
                            boundary(fixture::audit);
                            if (mode == "restart") {
                                store.read(ci::checked(
                                    [&](const ci::Json&, const std::string& message) {
                                        finish(message);
                                    },
                                    [&](const ci::Json& snapshot) {
                                        ci::Json graph = snapshot.at("graph");
                                        ci::require(effects == 0 && ci::commands(graph).empty(),
                                                    "Restart must not replay committed assignments");
                                        ci::require(graph.at("executions").size() == 3, "Exact durable selections survived restart");
                                        finish("");
                                    }));
                            } else {
                                if (mode.starts_with("reject")) {
                                    boundary([](MYSQL* reader) {
                                        const int status =
                                            mysql_query(reader,
                                                        "CREATE TRIGGER reject_write BEFORE INSERT ON attempt FOR EACH ROW "
                                                        "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='isolated test rejection'");
                                        ci::require(status == 0, "Install isolated rejection: " + std::string(mysql_error(reader)));
                                    });
                                }
                                const auto operation = [mode](ci::Json& graph) {
                                    ci::Json selected = {{"Fixture/base", fixture::selected("base", ci::Json::array())},
                                                         {"Fixture/client", fixture::selected("client", ci::Json::array({"base"}))},
                                                         {"Fixture/other", fixture::selected("other", ci::Json::array())}};
                                    ci::Json authorized = ci::selectRequest(graph, selected, "Fixture/base");
                                    ci::selectWork(graph, selected, "Fixture/other", fixture::target());
                                    for (const auto& command : ci::commands(graph)) {
                                        authorized.push_back(command);
                                    }
                                    if (mode.ends_with("projection")) {
                                        graph["executions"][0]["attempts"][0].erase("created");
                                    }
                                    return authorized;
                                };
                                const ci::Done changed = [&](const std::string& message) {
                                    try {
                                        if (mode.starts_with("reject") || mode.starts_with("unknown")) {
                                            ci::require(!message.empty() && effects == 0 && notifications == 0,
                                                        "Failed/unknown commit cannot dispatch or notify");
                                            boundary([&](MYSQL* reader) {
                                                const auto graph = fixture::sqlGraph(fixture::sqlRows(reader));
                                                ci::require(graph.at("executions").size() == (mode.starts_with("unknown") ? 3 : 0),
                                                            "Actual commit outcome retained");
                                            });
                                            finish("");
                                        } else if (mode == "projection") {
                                            ci::require(message.empty() && effects == 2 && notifications == 1,
                                                        "Projection failure cannot suppress committed dispatch or done");
                                            finish("");
                                        } else {
                                            ci::require(message.empty(), message);
                                            boundary([&](MYSQL* reader) {
                                                const auto count = fixture::writes(reader);
                                                ci::require(firstWrites == 0 || count == firstWrites, "Duplicate operation writes no rows");
                                                firstWrites = count;
                                            });
                                            ++completions;
                                            if (completions == (mode == "internal" ? 1U : 20U)) {
                                                ci::require(effects == 2,
                                                            "Concurrent duplicate selections dispatch every root exactly once");
                                                ci::require(notifications == 1, "Nineteen duplicate requests cannot write or notify");
                                                if (mode == "internal") {
                                                    const auto hidden = [](ci::Json& graph) {
                                                        ci::current(graph["executions"][0])["facts"]["internal_only"] = "durable";
                                                        return ci::Json::array();
                                                    };
                                                    auto committedWrites = std::make_shared<std::size_t>(0);
                                                    const auto verify = [&, committedWrites]() {
                                                        boundary([&](MYSQL* reader) {
                                                            const auto raw = fixture::sqlGraph(fixture::sqlRows(reader));
                                                            const auto count = fixture::writes(reader);
                                                            ci::require((*committedWrites == 0 || *committedWrites == count) &&
                                                                            notifications == 1 && effects == 2 &&
                                                                            raw.at("executions")
                                                                                    .at(0)
                                                                                    .at("attempts")
                                                                                    .back()
                                                                                    .at("facts")
                                                                                    .at("internal_only") == "durable",
                                                                        "Internal facts commit silently; repeating them cannot write");
                                                            *committedWrites = count;
                                                        });
                                                    };
                                                    store.change(hidden, [&, hidden, verify](const std::string& error) {
                                                        ci::require(error.empty(), error);
                                                        verify();
                                                        store.change(hidden, [&, verify](const std::string& repeated) {
                                                            ci::require(repeated.empty(), repeated);
                                                            verify();
                                                            finish("");
                                                        });
                                                    });
                                                } else {
                                                    store.read(ci::checked(
                                                        [&](const ci::Json&, const std::string& error) {
                                                            finish(error);
                                                        },
                                                        [&](const ci::Json& snapshot) {
                                                            const std::string base = snapshot.at("graph").at("executions").at(0).at("id");
                                                            store.change(
                                                                [base](ci::Json& graph) {
                                                                    return fixture::finishBuild(graph, base);
                                                                },
                                                                [&, base](const std::string& error) {
                                                                    if (!error.empty()) {
                                                                        finish(error);
                                                                    } else {
                                                                        store.change(
                                                                            [base](ci::Json& graph) {
                                                                                ci::Json indexes = ci::Json::array();
                                                                                for (int index = 0; index < 300; ++index) {
                                                                                    indexes.push_back(
                                                                                        {{"path",
                                                                                          "rpm/stable/repodata/" + std::string(70, 'a') +
                                                                                              std::to_string(index) + ".xml.gz"},
                                                                                         {"size", 1},
                                                                                         {"sha256", std::string(64, '9')}});
                                                                                }
                                                                                return fixture::publish(graph, base, indexes);
                                                                            },
                                                                            [&](const std::string& failure) {
                                                                                finish(failure.empty() && effects != 4
                                                                                           ? "Atomic dependent release missing"
                                                                                           : failure);
                                                                            });
                                                                    }
                                                                });
                                                        }));
                                                }
                                            }
                                        }
                                    } catch (const std::exception& failure) {
                                        finish(failure.what());
                                    }
                                };
                                store.change(operation, changed);
                                if (mode == "race") {
                                    for (int duplicate = 1; duplicate < 20; ++duplicate) {
                                        store.change(operation, changed);
                                    }
                                }
                            }
                        } catch (const std::exception& failure) {
                            finish(failure.what());
                        }
                    });
                } else if (state.error != 0 && !started) {
                    finish("Initial connection: " + state.errorMessage);
                }
            },
            "database");
        store.db->connect();
    });
    core::SNodeC::start();
    std::cout << "MariaDB " << mode << ": " << effects << " committed dispatch effects\n";
    return result;
}
