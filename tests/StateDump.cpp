#include "MigrationFixture.h"
#include "adapters/mariadb/MariaDBStateStore.h"

#include <core/SNodeC.h>
#include <cstdlib>

namespace {
    std::string env(const char* name) {
        const auto* value = std::getenv(name);
        return value ? value : "";
    }
} // namespace
int main(int argc, char** argv) {
    CHECK_EQ(argc, 2);
    bool loaded = false;
    if (argc == 2) {
        const auto expected = readJson(argv[1]);
        // This helper takes test arguments, not daemon command-line options.
        char* arguments[]{argv[0], nullptr};
        core::SNodeC::init(1, arguments);
        database::mariadb::MariaDBConnectionDetails details;
        details.connectionName = "migration-check";
        details.hostname = env("CORECI_TEST_DB_HOST");
        details.port = static_cast<unsigned>(std::stoul(env("CORECI_TEST_DB_PORT")));
        details.username = env("CORECI_TEST_DB_USER");
        details.password = env("CORECI_TEST_DB_PASSWORD");
        details.database = env("CORECI_TEST_DB_NAME");
        details.socket = MYSQL_UNIX_ADDR;
        coreci::mariadb::MariaDBStateStore store(details);
        const auto error = [&](const auto& failure) {
            std::cerr << failure.message << '\n';
            CHECK(false);
            core::SNodeC::stop();
        };
        store.open(
            [&] {
                store.load(
                    [&](coreci::StoredState state) {
                        const auto actual = snapshot(coreci::WaveController({}, state));
                        CHECK(actual == expected);
                        if (actual != expected) {
                            std::cerr << Json::diff(expected, actual).dump(2).substr(0, 3000) << '\n';
                        }
                        loaded = true;
                        core::SNodeC::stop();
                    },
                    error);
            },
            error);
        core::SNodeC::start();
    }
    CHECK(loaded);
    return checkResult();
}
