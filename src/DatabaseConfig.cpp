#include "DatabaseConfig.h"

#include "Error.h"
#include "PrivateFile.h"

namespace coreci {
    DatabaseConfig::DatabaseConfig(utils::SubCommand* parent)
        : SubCommand(parent, this, "Database") {
        setConfigurable(addOptionVariable(
                            "--mode", mode, "Server ownership", "mode", std::string("unmanaged"), CLI::IsMember({"managed", "unmanaged"})),
                        true);
        const std::pair<const char*, std::string&> strings[]{{"host", host},
                                                             {"socket", socket},
                                                             {"user", user},
                                                             {"name", name},
                                                             {"password-file", passwordFile},
                                                             {"managed-executable", managedExecutable},
                                                             {"managed-config-file", managedConfigFile},
                                                             {"managed-datadir", managedDatadir},
                                                             {"managed-pid-file", managedPidFile}};
        for (const auto& [option, value] : strings) {
            setConfigurable(addOptionVariable(std::string("--") + option, value, option, "string", CLI::TypeValidator<std::string>()),
                            true);
        }
        setConfigurable(addOptionVariable("--port", port, "TCP port", "port", 3306u, CLI::Range(1u, 65535u)), true);
    }

    database::mariadb::MariaDBConnectionDetails DatabaseConfig::connection() const {
        require(!user.empty() && !name.empty() && (!socket.empty() || !host.empty()), "configure the database user, schema and endpoint");
        auto password = readPrivateFile(passwordFile, 4096);
        require(password.find('\0') == std::string::npos, "database password contains a NUL byte");
        if (!password.empty() && password.back() == '\n') {
            password.pop_back();
        }
        return {.connectionName = "core-ci",
                .hostname = host,
                .username = user,
                .password = password,
                .database = name,
                .port = port,
                .socket = socket,
                .flags = 0};
    }
} // namespace coreci
