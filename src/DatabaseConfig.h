#ifndef CORE_CI_DATABASE_CONFIG_H
#define CORE_CI_DATABASE_CONFIG_H

#include <database/mariadb/MariaDBConnectionDetails.h>
#include <string>
#include <string_view>
#include <utils/SubCommand.h>

namespace coreci {

    class DatabaseConfig : public utils::SubCommand {
    public:
        constexpr static std::string_view NAME{"database"};
        constexpr static std::string_view DESCRIPTION{"Explicit MariaDB connection and private-server ownership"};

        explicit DatabaseConfig(utils::SubCommand* parent);

        database::mariadb::MariaDBConnectionDetails connection() const;

        std::string mode;
        std::string host;
        unsigned int port = 3306;
        std::string socket;
        std::string user;
        std::string name;
        std::string passwordFile;
        std::string managedExecutable;
        std::string managedConfigFile;
        std::string managedDatadir;
        std::string managedPidFile;
    };

} // namespace coreci

#endif
