#include "ManagedDatabase.h"

#include "Error.h"

#include <Log.h>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <log/SemanticLogger.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

extern char** environ;
namespace coreci {
    ManagedDatabase::Watch::Watch(ManagedDatabase& owner, std::function<void()> onRead)
        : ReadEventReceiver("core-ci database", logger::LogScope{}, TIMEOUT::DISABLE)
        , owner(owner)
        , onRead(std::move(onRead)) {
    }
    ManagedDatabase::Watch::~Watch() {
        close();
    }
    void ManagedDatabase::Watch::attach(int descriptor) {
        require(descriptor >= 0, "cannot open managed database notification descriptor");
        fd = descriptor;
        require(enable(fd), "cannot observe managed database descriptor");
    }
    void ManagedDatabase::Watch::close() {
        if (isEnabled()) {
            disable();
        }
        if (fd >= 0) {
            ::close(std::exchange(fd, -1));
        }
    }
    void ManagedDatabase::Watch::readEvent() {
        onRead();
    }
    void ManagedDatabase::Watch::unobservedEvent() {
    }
    void ManagedDatabase::Watch::destruct() {
    }
    void ManagedDatabase::Watch::shutdownEvent(const core::ShutdownContext&) {
        owner.stop();
    }
    ManagedDatabase::ManagedDatabase()
        : notification(*this,
                       [this] {
                           notified();
                       })
        , process(*this, [this] {
            exited();
        }) {
    }
    ManagedDatabase::~ManagedDatabase() {
        stop();
        int status = 0;
        while (pid > 0 && ::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
    }
    void ManagedDatabase::start(const DatabaseConfig& config, const std::function<void()>& onReady) {
        const auto directory = std::filesystem::canonical(config.managedDatadir);
        struct stat metadata{};
        require(pid < 0 && !config.managedExecutable.empty() && !config.managedConfigFile.empty() &&
                    ::stat(directory.c_str(), &metadata) == 0 && metadata.st_uid == ::geteuid() && (metadata.st_mode & 0077) == 0 &&
                    std::filesystem::canonical(std::filesystem::path(config.socket).parent_path()) == directory &&
                    std::filesystem::canonical(std::filesystem::path(config.managedPidFile).parent_path()) == directory &&
                    !std::filesystem::exists(config.socket) && !std::filesystem::exists(config.managedPidFile),
                "managed MariaDB requires private owned paths and absent socket/PID endpoints");
        const auto name = "core-ci-db-" + std::to_string(::getpid());
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path + 1, name.data(), name.size());
        notification.attach(::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
        const int credentials = 1;
        require(::setsockopt(notification.fd, SOL_SOCKET, SO_PASSCRED, &credentials, sizeof(credentials)) == 0 &&
                    ::bind(notification.fd, reinterpret_cast<sockaddr*>(&address), offsetof(sockaddr_un, sun_path) + name.size() + 1) == 0,
                "cannot bind managed database readiness socket");
        std::vector<std::string> environment;
        for (char** entry = environ; *entry; ++entry) {
            if (!std::string_view(*entry).starts_with("NOTIFY_SOCKET=")) {
                environment.emplace_back(*entry);
            }
        }
        environment.push_back("NOTIFY_SOCKET=@" + name);
        std::vector<std::string> arguments{config.managedExecutable,
                                           "--defaults-file=" + config.managedConfigFile,
                                           "--datadir=" + directory.string(),
                                           "--socket=" + config.socket,
                                           "--pid-file=" + config.managedPidFile,
                                           "--skip-networking"};
        const auto pointers = [](auto& strings) {
            std::vector<char*> result;
            for (auto& text : strings) {
                result.push_back(text.data());
            }
            result.push_back(nullptr);
            return result;
        };
        auto argv = pointers(arguments);
        auto env = pointers(environment);
        ready = onReady;
        pid_t spawned = -1;
        require(::posix_spawn(&spawned, argv.front(), nullptr, nullptr, argv.data(), env.data()) == 0, "cannot spawn managed MariaDB");
        pid = spawned;
        process.attach(static_cast<int>(::syscall(SYS_pidfd_open, pid, 0)));
    }
    void ManagedDatabase::stop() {
        ready = nullptr;
        notification.close();
        process.close();
        if (pid > 0) {
            ::kill(pid, SIGTERM);
        }
    }
    void ManagedDatabase::notified() {
        std::array<char, 4096> bytes{};
        alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(ucred))> credentials{};
        iovec vector{bytes.data(), bytes.size()};
        msghdr message{};
        message.msg_iov = &vector;
        message.msg_iovlen = 1;
        message.msg_control = credentials.data();
        message.msg_controllen = credentials.size();
        const auto size = ::recvmsg(notification.fd, &message, MSG_DONTWAIT);
        bool owned = false;
        for (auto* header = CMSG_FIRSTHDR(&message); size > 0 && header; header = CMSG_NXTHDR(&message, header)) {
            if (header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_CREDENTIALS && header->cmsg_len >= CMSG_LEN(sizeof(ucred))) {
                ucred sender{};
                std::memcpy(&sender, CMSG_DATA(header), sizeof(sender));
                owned = sender.pid == pid;
            }
        }
        if (owned && (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) == 0 &&
            ("\n" + std::string(bytes.data(), size) + "\n").find("\nREADY=1\n") != std::string::npos && ready) {
            notification.close();
            const auto callback = std::exchange(ready, nullptr);
            callback();
        }
    }
    void ManagedDatabase::exited() {
        int status = 0;
        if (::waitpid(pid, &status, WNOHANG) == pid) {
            pid = -1;
            snode::log::application().error() << "Managed MariaDB exited: " << status;
            stop();
        }
    }
} // namespace coreci
