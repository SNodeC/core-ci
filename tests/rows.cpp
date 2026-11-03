#include "sql.h"

#include <chrono>
#include <core/EventReceiver.h>
#include <core/SNodeC.h>
#include <fstream>
#include <iostream>
#include <map>

int main(int argc, char* argv[]) {
    using Clock = std::chrono::steady_clock;
    ci::Store store;
    int result = 0;
    bool started = false;
    ci::Json input;
    if (const char* path = std::getenv("CI_ROWS_INPUT")) {
        std::ifstream file(path);
        file >> input;
    }
    const std::string database = std::getenv("CI_TEST_DATABASE");
    const std::string socket = std::getenv("CI_TEST_SOCKET");
    const ci::Done finish = [&](const std::string& error) {
        if (!error.empty()) {
            std::cerr << error << '\n';
            result = 1;
        }
        core::SNodeC::stop();
    };
    const auto verify = [&](const std::string& error) {
        if (!error.empty() && std::getenv("CI_ROWS_EXPECT_CLOSED") != nullptr) {
            ci::require(!store.db, "Rollback failure releases the native client");
            std::size_t rejected = 0;
            store.read([&](const ci::Json&, const std::string& failure) {
                ci::require(failure == "Database unavailable", "Read fails closed");
                ++rejected;
            });
            store.initialize([&](const std::string& failure) {
                ci::require(failure == "Database unavailable", "Initialize fails closed");
                ++rejected;
            });
            store.change(
                [](ci::Json&) {
                    ci::require(false, "Unavailable Store must not call operation");
                    return ci::Json::array();
                },
                [&](const std::string& failure) {
                    ci::require(failure == "Database unavailable", "Change fails closed");
                    ++rejected;
                });
            ci::require(rejected == 3, "Every DB-backed entry point reports unavailability");
            std::cout << "abort_release_null_guards=passed" << '\n';
        }
        ci::checked(
            [&](const ci::Json&, const std::string& message) {
                finish(message);
            },
            [&](const ci::Json&) {
                const auto beginning = Clock::now();
                store.read(ci::checked(
                    [&](const ci::Json&, const std::string& failure) {
                        finish(failure);
                    },
                    [&, beginning](const ci::Json& snapshot) {
                        const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - beginning).count();
                        const auto& graph = snapshot.at("graph");
                        ci::require(input.is_null() || input.dump() == graph.dump(), "Raw graph dump must roundtrip exactly");
                        std::map<std::string, std::size_t> documents;
                        if (!input.is_null()) {
                            for (std::size_t i = 0; i < input.at("executions").size(); ++i) {
                                const auto& expected = input.at("executions").at(i).at("attempts");
                                const auto& actual = graph.at("executions").at(i).at("attempts");
                                for (std::size_t j = 0; j < expected.size(); ++j) {
                                    for (const std::string field : {"assignment", "build_manifest", "build_result", "receipt"}) {
                                        if (expected.at(j).contains(field)) {
                                            ci::require(ci::hash(expected.at(j).at(field).dump()) ==
                                                            ci::hash(actual.at(j).at(field).dump()),
                                                        "Canonical document hash preserved");
                                            ++documents[field];
                                        }
                                    }
                                }
                            }
                            std::cout << "document_hash_counts=" << ci::Json(documents).dump() << '\n';
                        }
                        if (const char* path = std::getenv("CI_ROWS_OUTPUT")) {
                            std::ofstream(path) << graph.dump();
                        }
                        if (std::getenv("CI_ROWS_ORACLE") != nullptr) {
                            std::unique_ptr<MYSQL, decltype(&mysql_close)> reader(mysql_init(nullptr), mysql_close);
                            ci::require(mysql_real_connect(reader.get(), "localhost", "root", "", database.c_str(), 0, socket.c_str(), 0) !=
                                            nullptr,
                                        "Independent oracle connection");
                            const auto rows = fixture::sqlRows(reader.get());
                            ci::require(fixture::sqlGraph(rows).dump() == graph.dump(), "Independent SQL oracle must equal raw dump");
                            std::size_t logical = 0;
                            for (const auto& [table, entries] : rows.items()) {
                                for (const auto& [id, row] : entries.items()) {
                                    logical += (table == "graph_root" ? id.size() : ci::Json(id).dump().size()) +
                                               row.at("body").dump().size() + (table == "graph_root" ? 0 : row.at("refs").dump().size());
                                    for (const auto& [key, value] : row.items()) {
                                        if (key != "body" && key != "refs") {
                                            logical += value.dump().size();
                                        }
                                    }
                                }
                            }
                            std::cout << "logical_body_refs_identity_bytes=" << logical << '\n';
                        }
                        std::cout << "read_ms=" << elapsed << " canonical_bytes=" << graph.dump().size()
                                  << " executions=" << graph.at("executions").size() << '\n';
                        finish("");
                    }));
            })({}, error);
    };
    core::SNodeC::init(argc, argv);
    core::EventReceiver::atNextTick([&]() {
        const database::mariadb::MariaDBConnectionDetails details = {.connectionName = "rows-fixture",
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
                if (state.connected && !started) {
                    started = true;
                    store.initialize([&](const std::string& error) {
                        if (error.empty() && !input.is_null()) {
                            const auto beginning = Clock::now();
                            store.change(
                                [&](ci::Json& graph) {
                                    graph = input;
                                    return ci::Json::array();
                                },
                                [&, beginning](const std::string& failure) {
                                    std::cout << "import_ms=" << std::chrono::duration<double, std::milli>(Clock::now() - beginning).count()
                                              << '\n';
                                    verify(failure);
                                });
                        } else {
                            verify(error);
                        }
                    });
                } else if (state.error != 0 && !started) {
                    finish("Fixture connection failed");
                }
            },
            "database");
        store.db->connect();
    });
    core::SNodeC::start();
    return result;
}
