#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <cstdint>
#include <cstdlib>
#include <database/mariadb/MariaDBClient.h>
#include <iostream>
#include <memory>
std::string env(const char* name) {
    const auto* value = std::getenv(name);
    return value == nullptr ? "" : value;
}
int main(int argc, char** argv) {
    int result = 77;
    if (!env("CORECI_TEST_DB_HOST").empty() && !env("CORECI_TEST_DB_USER").empty() && !env("CORECI_TEST_DB_NAME").empty()) {
        core::SNodeC::init(argc, argv);
        database::mariadb::MariaDBConnectionDetails details;
        details.connectionName = "smoke-interrupt";
        details.hostname = env("CORECI_TEST_DB_HOST");
        details.username = env("CORECI_TEST_DB_USER");
        details.password = env("CORECI_TEST_DB_PASSWORD");
        details.database = env("CORECI_TEST_DB_NAME");
        details.socket = MYSQL_UNIX_ADDR;
        details.port = env("CORECI_TEST_DB_PORT").empty() ? 0 : static_cast<unsigned>(std::stoul(env("CORECI_TEST_DB_PORT")));
        database::mariadb::MariaDBClient waiter(details,
                                                [](const auto&) {
                                                }),
            killer(details, [](const auto&) {
            });
        std::unique_ptr<core::timer::Timer> hold, poll;
        std::uint64_t victim = 0, waiterId = 0;
        unsigned attempts = 0;
        bool locked = false, acquired = false;
        const auto failed = [&](const auto&, unsigned) {
            waiter.abort();
            killer.abort();
        };
        std::function<void()> waiting;
        waiting = [&] {
            killer.query(
                "SELECT COUNT(*) FROM information_schema.PROCESSLIST WHERE ID=" + std::to_string(waiterId) + " AND STATE='User lock'",
                [&](const MYSQL_ROW row) {
                    if (row != nullptr && row[0] != nullptr && std::string(row[0]) == "1") {
                        locked = true;
                    } else if (row == nullptr && locked) {
                        killer.exec(
                            "KILL CONNECTION " + std::to_string(victim),
                            [] {
                            },
                            failed);
                    } else if (row == nullptr && ++attempts < 1000) {
                        poll = std::make_unique<core::timer::Timer>(core::timer::Timer::singleshotTimer(waiting, 0.01));
                    } else if (row == nullptr) {
                        waiter.abort();
                        killer.abort();
                    }
                },
                failed);
        };
        waiter.connect();
        killer.connect();
        killer.query(
            "SELECT IS_USED_LOCK(CONCAT('coreci:',DATABASE()))",
            [&](const MYSQL_ROW row) {
                if (row != nullptr && row[0] != nullptr) {
                    victim = std::stoull(row[0]);
                } else if (row == nullptr && victim != 0) {
                    waiter.query(
                        "SELECT CONNECTION_ID()",
                        [&](const MYSQL_ROW connection) {
                            if (connection != nullptr) {
                                waiterId = std::stoull(connection[0]);
                            } else {
                                waiter.query(
                                    "SELECT GET_LOCK(CONCAT('coreci:',DATABASE()),15)",
                                    [&](const MYSQL_ROW lock) {
                                        if (lock != nullptr) {
                                            acquired = lock[0] != nullptr && std::string(lock[0]) == "1";
                                        } else if (acquired) {
                                            std::cout << "LOCKED" << std::endl;
                                            killer.abort();
                                            hold = std::make_unique<core::timer::Timer>(core::timer::Timer::singleshotTimer(
                                                [&] {
                                                    waiter.abort();
                                                },
                                                5));
                                        }
                                    },
                                    failed);
                                waiting();
                            }
                        },
                        failed);
                } else if (row == nullptr) {
                    waiter.abort();
                    killer.abort();
                }
            },
            failed);
        const int loop = core::SNodeC::start();
        result = acquired && loop == 0 ? 0 : 1;
    }
    return result;
}
