#include "DatabaseConfig.h"
#include "ManagedDatabase.h"

#include <Log.h>
#include <core/SNodeC.h>
#include <database/mariadb/MariaDBClient.h>
#include <exception>
#include <express/legacy/in/WebApp.h>
#include <memory>
#include <utility>
#include <utils/Config.h>

int main(int argc, char* argv[]) {
    auto* databaseConfig = utils::Config::configRoot.newSubCommand<coreci::DatabaseConfig>();

    const express::legacy::in::WebApp app("web");
    app.getConfig()->Local::setHost("127.0.0.1")->setPort(8080);

    core::SNodeC::init(argc, argv);

    std::unique_ptr<coreci::ManagedDatabase> managed;
    std::unique_ptr<database::mariadb::MariaDBClient> database;
    bool initialConnection = true;

    try {
        database = std::make_unique<database::mariadb::MariaDBClient>(
            databaseConfig->connection(), [&](const database::mariadb::MariaDBState& state) {
                if (state.error != 0) {
                    snode::log::application().error() << "MariaDB connection failed: " << state.errorMessage << " [" << state.error << "]";
                    if (std::exchange(initialConnection, false)) {
                        database.reset();
                        if (managed) {
                            managed->stop();
                        }
                    }
                } else if (state.connected && std::exchange(initialConnection, false)) {
                    snode::log::application().info() << "Initial MariaDB connection established";

                    app.get("/api/status", [&] APPLICATION(req, res) {
                        database->query(
                            "SELECT 1",
                            [res](const MYSQL_ROW row) {
                                if (row != nullptr) {
                                    res->type("application/json");
                                    res->send(R"({"database":"available"})");
                                }
                            },
                            [res](const std::string&, unsigned int) {
                                res->status(503);
                                res->type("application/json");
                                res->send(R"({"database":"unavailable"})");
                            });
                    });

                    app.listen([&](const auto& address, const core::socket::State& listenerState) {
                        if (listenerState == core::socket::State::OK) {
                            snode::log::application().info() << "Listening on " << address.toString();
                        } else if (listenerState == core::socket::State::ERROR || listenerState == core::socket::State::FATAL) {
                            snode::log::application().error() << "Listener failed: " << listenerState.what();
                            database.reset();
                            if (managed) {
                                managed->stop();
                            }
                        }
                    });
                }
            });

        if (databaseConfig->mode == "managed") {
            managed = std::make_unique<coreci::ManagedDatabase>();
            managed->start(*databaseConfig, [&]() {
                database->connect();
            });
        } else {
            database->connect();
        }
    } catch (const std::exception& error) {
        snode::log::application().error() << "Database startup failed: " << error.what();
        database.reset();
        if (managed) {
            managed->stop();
        }
    }

    return core::SNodeC::start();
}
