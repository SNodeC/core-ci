#include "ManagedDatabase.h"

#include "DatabaseConfig.h"

#include <Log.h>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <log/SemanticLogger.h>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace coreci {

    ManagedDatabase::Watch::Watch(ManagedDatabase& owner, bool childExit)
        : ReadEventReceiver(
              "core-ci managed database",
              logger::LogScope{
                  logger::LogOrigin::Application, logger::LogBoundary::System, "core-ci.database", "managed", logger::LogRole::Unknown, {}},
              TIMEOUT::DISABLE)
        , owner(owner)
        , childExit(childExit) {
    }

    ManagedDatabase::Watch::~Watch() {
        close();
    }

    void ManagedDatabase::Watch::attach(int descriptor) {
        fd = descriptor;
        if (!enable(fd)) {
            throw std::runtime_error("register managed database descriptor");
        }
    }

    void ManagedDatabase::Watch::close() {
        if (isEnabled()) {
            disable();
        }
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

    void ManagedDatabase::Watch::readEvent() {
        if (childExit) {
            owner.childExited();
        } else {
            owner.receiveReady();
        }
    }

    void ManagedDatabase::Watch::unobservedEvent() {
        // The concrete managed-server resource owns these watchers through framework shutdown.
    }

    void ManagedDatabase::Watch::destruct() {
        // The framework does not own the containing managed-server resource.
    }

    void ManagedDatabase::Watch::shutdownEvent([[maybe_unused]] const core::ShutdownContext& context) {
        owner.stop();
    }

    ManagedDatabase::ManagedDatabase()
        : notification(*this, false)
        , child(*this, true) {
    }

    ManagedDatabase::~ManagedDatabase() {
        stop();

        // Destruction occurs after SNodeC::start() returns, never inside an event handler.
        if (pid > 0) {
            int status = 0;
            pid_t result = -1;
            do {
                result = ::waitpid(pid, &status, 0);
            } while (result < 0 && errno == EINTR);
        }
    }

    void ManagedDatabase::start(const DatabaseConfig& config, const std::function<void()>& onReady) {
        if (pid > 0 || config.managedExecutable.empty() || config.managedConfigFile.empty() || config.managedDatadir.empty() ||
            config.socket.empty() || config.managedPidFile.empty()) {
            throw std::runtime_error("managed mode requires explicit executable, separate config, datadir, socket and PID file");
        }

        const auto datadir = std::filesystem::canonical(config.managedDatadir);
        const auto socketParent = std::filesystem::canonical(std::filesystem::path(config.socket).parent_path());
        const auto pidParent = std::filesystem::canonical(std::filesystem::path(config.managedPidFile).parent_path());
        struct stat info{};
        if (::stat(datadir.c_str(), &info) != 0 || info.st_uid != ::geteuid() || (info.st_mode & 0077) != 0 || socketParent != datadir ||
            pidParent != datadir || std::filesystem::exists(config.socket) || std::filesystem::exists(config.managedPidFile)) {
            throw std::runtime_error("managed datadir must be private and owned; absent socket/PID endpoints must be directly inside it");
        }

        const std::string socketName = "core-ci-mariadb-" + std::to_string(::getpid());
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path + 1, socketName.data(), socketName.size());

        const int descriptor = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (descriptor < 0) {
            throw std::system_error(errno, std::generic_category(), "create MariaDB notification socket");
        }
        notification.attach(descriptor);

        const int enabled = 1;
        const auto addressSize = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + socketName.size());
        if (::setsockopt(descriptor, SOL_SOCKET, SO_PASSCRED, &enabled, sizeof(enabled)) != 0 ||
            ::bind(descriptor, reinterpret_cast<const sockaddr*>(&address), addressSize) != 0) {
            throw std::system_error(errno, std::generic_category(), "bind MariaDB notification socket");
        }

        std::vector<std::string> environment;
        for (char** entry = environ; *entry != nullptr; ++entry) {
            if (std::string_view(*entry).find("NOTIFY_SOCKET=") != 0) {
                environment.emplace_back(*entry);
            }
        }
        environment.emplace_back("NOTIFY_SOCKET=@" + socketName);
        std::vector<char*> environmentPointers;
        for (auto& entry : environment) {
            environmentPointers.push_back(entry.data());
        }
        environmentPointers.push_back(nullptr);

        std::vector<std::string> arguments{config.managedExecutable,
                                           "--defaults-file=" + config.managedConfigFile,
                                           "--datadir=" + datadir.string(),
                                           "--socket=" + config.socket,
                                           "--pid-file=" + config.managedPidFile,
                                           "--skip-networking"};
        std::vector<char*> argumentPointers;
        for (auto& argument : arguments) {
            argumentPointers.push_back(argument.data());
        }
        argumentPointers.push_back(nullptr);

        ready = onReady;
        const int error =
            ::posix_spawn(&pid, config.managedExecutable.c_str(), nullptr, nullptr, argumentPointers.data(), environmentPointers.data());
        if (error != 0) {
            pid = -1;
            throw std::system_error(error, std::generic_category(), "start private MariaDB");
        }

        const int pidDescriptor = static_cast<int>(::syscall(SYS_pidfd_open, pid, 0));
        if (pidDescriptor < 0) {
            throw std::system_error(errno, std::generic_category(), "observe owned MariaDB child");
        }
        child.attach(pidDescriptor);
    }

    void ManagedDatabase::stop() {
        ready = {};
        notification.close();
        child.close();
        if (pid > 0) {
            ::kill(pid, SIGTERM);
        }
    }

    void ManagedDatabase::receiveReady() {
        std::array<char, 4096> bytes{};
        alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(ucred))> control{};
        iovec vector{bytes.data(), bytes.size()};
        msghdr message{};
        message.msg_iov = &vector;
        message.msg_iovlen = 1;
        message.msg_control = control.data();
        message.msg_controllen = control.size();

        const ssize_t size = ::recvmsg(notification.getRegisteredFd(), &message, MSG_DONTWAIT);
        bool owned = false;
        if (size > 0 && (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) == 0) {
            for (cmsghdr* header = CMSG_FIRSTHDR(&message); header != nullptr; header = CMSG_NXTHDR(&message, header)) {
                if (header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_CREDENTIALS &&
                    header->cmsg_len >= CMSG_LEN(sizeof(ucred))) {
                    ucred credentials{};
                    std::memcpy(&credentials, CMSG_DATA(header), sizeof(credentials));
                    owned = credentials.pid == pid;
                }
            }
            const std::string body = "\n" + std::string(bytes.data(), static_cast<std::size_t>(size)) + "\n";
            if (owned && body.find("\nREADY=1\n") != std::string::npos && ready) {
                notification.close();
                auto callback = std::move(ready);
                callback();
            }
        }
    }

    void ManagedDatabase::childExited() {
        int status = 0;
        const pid_t result = ::waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            pid = -1;
            snode::log::application().error() << "Owned MariaDB process exited; status " << status;
            stop();
        }
    }

} // namespace coreci
