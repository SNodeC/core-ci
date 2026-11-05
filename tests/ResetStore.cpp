#include <core/SNodeC.h>
#include <cstdlib>
#include <database/mariadb/MariaDBClient.h>
#include <iostream>
std::string env(const char* name) {
    const auto* value = std::getenv(name);
    return value == nullptr ? "" : value;
}
int main(int argc, char** argv) {
    int result = 77;
    if (!env("CORECI_TEST_DB_HOST").empty() && !env("CORECI_TEST_DB_USER").empty() && !env("CORECI_TEST_DB_NAME").empty()) {
        core::SNodeC::init(argc, argv);
        database::mariadb::MariaDBConnectionDetails details;
        details.connectionName = "smoke-reset";
        details.hostname = env("CORECI_TEST_DB_HOST");
        details.username = env("CORECI_TEST_DB_USER");
        details.password = env("CORECI_TEST_DB_PASSWORD");
        details.database = env("CORECI_TEST_DB_NAME");
        details.port = env("CORECI_TEST_DB_PORT").empty() ? 0 : static_cast<unsigned>(std::stoul(env("CORECI_TEST_DB_PORT")));
        details.socket = MYSQL_UNIX_ADDR;
        bool done = false;
        database::mariadb::MariaDBClient client(details, [&](const auto& state) {
            if (state.error != 0) {
                std::cerr << "reset connection failed\n";
            }
        });
        const auto failed = [&](const auto&, unsigned int) {
            client.abort();
        };
        client.connect();
        client.exec(
            "DROP TABLE IF EXISTS coreci_events,coreci_graph_feeds,coreci_graph_packages,coreci_graph_targets,coreci_graph_prerequisites",
            [&] {
                client.exec(
                    "DROP TABLE IF EXISTS coreci_schema",
                    [&] {
                        done = true;
                        client.abort();
                    },
                    failed);
            },
            failed);
        const int loop = core::SNodeC::start();
        result = done && loop == 0 ? 0 : 1;
    } else {
        std::cout << "Skipped: database variables unset\n";
    }
    return result;
}
