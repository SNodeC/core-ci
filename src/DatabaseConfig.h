#pragma once

#include <database/mariadb/MariaDBConnectionDetails.h>
#include <utils/SubCommand.h>

namespace coreci {
    class DatabaseConfig : public utils::SubCommand {
    public:
        static constexpr std::string_view NAME{"database"};
        static constexpr std::string_view DESCRIPTION{"MariaDB connection and optional private server"};
        explicit DatabaseConfig(utils::SubCommand* parent);
        database::mariadb::MariaDBConnectionDetails connection() const;
        std::string mode;
        std::string host;
        std::string socket;
        std::string user;
        std::string name;
        std::string passwordFile;
        std::string managedExecutable;
        std::string managedConfigFile;
        std::string managedDatadir;
        std::string managedPidFile;
        unsigned int port = 3306;
    };
} // namespace coreci
