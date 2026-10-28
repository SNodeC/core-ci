#include "Database.h"

#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <iostream>
#include <memory>
#include <utils/Config.h>
#include <utils/SubCommand.h>

namespace {
    class Probe : public utils::SubCommand {
    public:
        static constexpr std::string_view NAME{"probe"};
        static constexpr std::string_view DESCRIPTION{"Isolated database boundary check"};
        explicit Probe(utils::SubCommand* parent)
            : SubCommand(parent, this, "Database boundary") {
            required(addOptionVariable("--socket", socket, "Fixture socket", "path", CLI::TypeValidator<std::string>()));
            required(addOptionVariable("--case", mode, "Boundary to exercise", "string", CLI::TypeValidator<std::string>()));
        }
        std::string socket;
        std::string mode;
    };
} // namespace

int main(int argc, char* argv[]) {
    const auto* config = utils::Config::configRoot.newSubCommand<Probe>();
    core::SNodeC::init(argc, argv);
    std::unique_ptr<database::mariadb::MariaDBClient> client;
    std::unique_ptr<coreci::Database> database;
    bool passed = true;
    bool started = false;
    unsigned int applied = 0;
    unsigned int completed = 0;
    unsigned int rejected = 0;
    unsigned int noops = 0;
    const auto verify = [&] {
        database->query("SELECT JSON_OBJECT('revision',revision) FROM probe", [&](const auto& result) {
            const unsigned int expected = config->mode == "writes" ? 13 : 1;
            passed = passed && result.error.empty() && result.rows.size() == 1 && result.rows.front().at("revision") == expected;
            passed = passed && applied == expected && completed == expected && rejected == (config->mode == "writes" ? 0u : 4099u);
            passed = passed && noops == (config->mode == "writes" ? 0u : 4097u);
            database.reset();
            client.reset();
        });
    };
    const auto complete = [&](const coreci::DatabaseResult& result) {
        passed = passed && result.error.empty() && !result.uncertain;
        ++completed;
        ++applied;
    };
    client = std::make_unique<database::mariadb::MariaDBClient>(
        database::mariadb::MariaDBConnectionDetails{.connectionName = "database-boundary",
                                                    .hostname = "",
                                                    .username = "root",
                                                    .password = "",
                                                    .database = "boundary",
                                                    .port = 0,
                                                    .socket = config->socket,
                                                    .flags = 0},
        [&](const auto& state) {
            if (state.error && (!started || config->mode != "lost-ack")) {
                passed = false;
                std::cerr << state.errorMessage << '\n';
                database.reset();
                client.reset();
            } else if (state.connected && !started) {
                started = true;
                if (config->mode == "lost-ack") {
                    database->write(
                        [] {
                            return "UPDATE probe SET revision=1 WHERE revision=0";
                        },
                        [&](const auto& result) {
                            std::cerr << "lost-ack write error=" << result.error << " uncertain=" << result.uncertain
                                      << " allowed=" << database->mutationsAllowed() << '\n';
                            passed = passed && !result.error.empty() && result.uncertain && !database->mutationsAllowed();
                            database->write(
                                [&] {
                                    ++applied;
                                    return "UPDATE probe SET revision=2";
                                },
                                [&](const auto& blocked) {
                                    std::cerr << "blocked producer error=" << blocked.error << " uncertain=" << blocked.uncertain
                                              << " derived=" << applied << '\n';
                                    passed = passed && blocked.uncertain && !blocked.error.empty() && applied == 0;
                                    core::timer::Timer::singleshotTimer(
                                        [&] {
                                            database.reset();
                                            client.reset();
                                        },
                                        0);
                                });
                        });
                } else if (config->mode == "writes") {
                    for (unsigned int index = 0; index < 12; ++index) {
                        database->write(
                            [&] {
                                return "UPDATE probe SET revision=revision+1 WHERE revision=" + std::to_string(applied);
                            },
                            [&, index](const auto& result) {
                                complete(result);
                                if (index == 0) {
                                    database->write(
                                        [&] {
                                            return "UPDATE probe SET revision=revision+1 WHERE revision=" + std::to_string(applied);
                                        },
                                        [&](const auto& nested) {
                                            complete(nested);
                                            verify();
                                        });
                                }
                            });
                    }
                } else {
                    const auto failure = [&](const auto& result) {
                        passed = passed && !result.error.empty() && !result.uncertain && database->mutationsAllowed();
                        ++rejected;
                    };
                    database->write(
                        []() -> std::string {
                            throw std::invalid_argument("invalid transition");
                        },
                        failure);
                    database->write(
                        [] {
                            return "UPDATE probe SET revision=1 WHERE revision=999";
                        },
                        failure);
                    database->write(
                        [] {
                            return "INVALID SQL";
                        },
                        failure);
                    for (unsigned int index = 0; index < 4096; ++index) {
                        database->write(
                            [] {
                                return std::string{};
                            },
                            [&](const auto& result) {
                                passed = passed && result.error.empty() && !result.uncertain;
                                ++noops;
                            });
                        database->write(
                            []() -> std::string {
                                throw std::invalid_argument("rejected graph event");
                            },
                            failure);
                    }
                    database->write(
                        [] {
                            return std::string{};
                        },
                        [&](const auto& result) {
                            passed = passed && result.error.empty() && !result.uncertain;
                            ++noops;
                        });
                    database->write(
                        [] {
                            return "UPDATE probe SET revision=revision+1 WHERE revision=0";
                        },
                        [&](const auto& result) {
                            complete(result);
                            verify();
                        });
                }
            }
        });
    database = std::make_unique<coreci::Database>(*client);
    client->connect();
    const auto status = core::SNodeC::start();
    std::cout << (passed && started ? "PASS " : "FAIL ") << config->mode << '\n';
    return passed && started ? status : 1;
}
