#include "DatabaseConfig.h"

#include <array>
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace coreci {

    DatabaseConfig::DatabaseConfig(utils::SubCommand* parent)
        : SubCommand(parent, this, "Database") {
        required(setConfigurable(
            addOptionVariable("--mode", mode, "Server ownership mode", "managed|unmanaged", CLI::IsMember({"managed", "unmanaged"})),
            false));
        setConfigurable(addOptionVariable("--host", host, "Explicit TCP hostname", "hostname", CLI::TypeValidator<std::string>()), true);
        setConfigurable(addOptionVariable("--port", port, "TCP port", "uint16", 3306U, CLI::Range(1, 65535)), true);
        setConfigurable(
            addOptionVariable(
                "--socket", socket, "Explicit UNIX socket (takes precedence over TCP)", "path", CLI::TypeValidator<std::string>()),
            true);
        required(setConfigurable(
            addOptionVariable("--user", user, "Restricted application account", "string", CLI::TypeValidator<std::string>()), true));
        required(
            setConfigurable(addOptionVariable("--name", name, "Application schema", "string", CLI::TypeValidator<std::string>()), true));
        required(setConfigurable(addOptionVariable("--password-file",
                                                   passwordFile,
                                                   "Private password file; empty file for explicit socket authentication",
                                                   "path",
                                                   CLI::ExistingFile),
                                 true));
        setConfigurable(
            addOptionVariable("--managed-executable", managedExecutable, "Explicit private mariadbd executable", "path", CLI::ExistingFile),
            true);
        setConfigurable(addOptionVariable(
                            "--managed-config-file", managedConfigFile, "Separate private server configuration", "path", CLI::ExistingFile),
                        true);
        setConfigurable(addOptionVariable(
                            "--managed-datadir", managedDatadir, "Pre-provisioned private data directory", "path", CLI::ExistingDirectory),
                        true);
        setConfigurable(
            addOptionVariable("--managed-pid-file", managedPidFile, "Private server PID file", "path", CLI::TypeValidator<std::string>()),
            true);
    }

    database::mariadb::MariaDBConnectionDetails DatabaseConfig::connection() const {
        if (name.empty() || user.empty()) {
            throw std::runtime_error("configure a nonempty database name and user");
        }

        if (socket.empty() && host.empty()) {
            throw std::runtime_error("configure an explicit database socket or host");
        }

        const int fd = ::open(passwordFile.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "open database password file");
        }

        struct stat info{};
        const bool privateFile =
            ::fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && (info.st_mode & 0077) == 0 && info.st_uid == ::geteuid();
        std::array<char, 4097> buffer{};
        const ssize_t size = privateFile ? ::read(fd, buffer.data(), buffer.size()) : -1;
        ::close(fd);

        if (!privateFile || size < 0 || static_cast<std::size_t>(size) == buffer.size()) {
            throw std::runtime_error("database password file must be a private owned regular file of at most 4096 bytes");
        }

        std::string password(buffer.data(), static_cast<std::size_t>(size));
        if (password.find('\0') != std::string::npos) {
            throw std::runtime_error("database password file contains an unsupported NUL byte");
        }
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
