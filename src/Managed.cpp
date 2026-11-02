#include "Managed.h"

#include "Controller.h"

#include <Log.h>
#include <core/system/socket.h>
#include <cstring>
#include <filesystem>
#include <spawn.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace ci {

    class ManagedDatabase::Input final : public core::eventreceiver::ReadEventReceiver {
    public:
        Input(int descriptor, const std::function<void()>& readable, const std::function<void()>& stopping, Input*& owner)
            : ReadEventReceiver("managed-database",
                                snode::log::Scope{.boundary = snode::log::Boundary::System, .component = "database", .identity = {}},
                                TIMEOUT::DISABLE)
            , descriptor(descriptor)
            , readable(readable)
            , stopping(stopping)
            , owner(owner) {
            require(enable(descriptor), "Cannot observe managed database descriptor");
        }

        void close() {
            disable();
        }

    private:
        ~Input() override {
            ::close(descriptor);
            owner = nullptr;
        }

        void readEvent() override {
            readable();
        }

        void shutdownEvent(const core::ShutdownContext&) override {
            stopping();
        }

        void unobservedEvent() override {
            delete this;
        }

        int descriptor;
        std::function<void()> readable;
        std::function<void()> stopping;
        Input*& owner;
    };

    ManagedDatabase::~ManagedDatabase() {
        stop();
        if (!notificationPath.empty()) {
            ::unlink(notificationPath.c_str());
        }
    }

    void ManagedDatabase::stop() {
        if (notification != nullptr) {
            notification->close();
        }
        if (child > 0) {
            ::kill(child, SIGTERM);
        }
    }

    void ManagedDatabase::start(const std::string& directory, const std::string& socket, const std::function<void()>& ready) {
        const auto path = std::filesystem::canonical(directory);
        require(std::filesystem::is_directory(path / "mysql"), "Managed datadir must already be initialized");
        require(!std::filesystem::exists(socket), "Managed socket must not already exist");
        const std::string notificationName = (path / "core-ci.notify").string();
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        require(notificationName.size() < sizeof(address.sun_path), "Managed notification path too long");
        std::memcpy(address.sun_path, notificationName.c_str(), notificationName.size() + 1);
        const int descriptor = core::system::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        require(descriptor >= 0, "Cannot open database readiness socket");
        const int bound = core::system::bind(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        if (bound != 0) {
            ::close(descriptor);
        }
        require(bound == 0, "Cannot bind database readiness socket");
        notificationPath = notificationName;
        notification = new Input(
            descriptor,
            [this, descriptor, ready]() {
                char bytes[4096];
                const ssize_t count = core::system::recv(descriptor, bytes, sizeof(bytes), MSG_DONTWAIT);
                if (count > 0) {
                    const std::string message = "\n" + std::string(bytes, static_cast<std::size_t>(count)) + "\n";
                    if (message.find("\nREADY=1\n") != std::string::npos) {
                        notification->close();
                        ::unlink(notificationPath.c_str());
                        notificationPath.clear();
                        ready();
                    }
                }
            },
            [this]() {
                stop();
            },
            notification);
        std::vector<std::string> environment;
        for (char** entry = environ; *entry != nullptr; ++entry) {
            const std::string value(*entry);
            if (!value.starts_with("NOTIFY_SOCKET=") && !value.starts_with("WATCHDOG_") && !value.starts_with("LISTEN_")) {
                environment.push_back(value);
            }
        }
        environment.push_back("NOTIFY_SOCKET=" + notificationPath);
        std::vector<char*> env;
        for (auto& value : environment) {
            env.push_back(value.data());
        }
        env.push_back(nullptr);
        std::vector<std::string> arguments = {CORE_CI_MARIADBD,
                                              "--no-defaults",
                                              "--datadir=" + path.string(),
                                              "--socket=" + socket,
                                              "--pid-file=" + (path / "core-ci.pid").string(),
                                              "--skip-networking",
                                              "--user=" + std::to_string(::geteuid())};
        std::vector<char*> argv;
        for (auto& value : arguments) {
            argv.push_back(value.data());
        }
        argv.push_back(nullptr);
        const int error = ::posix_spawn(&child, CORE_CI_MARIADBD, nullptr, nullptr, argv.data(), env.data());
        require(error == 0, "Cannot spawn managed database: " + std::string(std::strerror(error)));
        const int pid = static_cast<int>(::syscall(SYS_pidfd_open, child, 0));
        require(pid >= 0, "Cannot observe managed database process");
        process = new Input(
            pid,
            [this]() {
                int status = 0;
                const int exited = ::waitpid(child, &status, WNOHANG);
                if (exited == child || (exited < 0 && errno == ECHILD)) {
                    child = -1;
                    process->close();
                    if (notification != nullptr) {
                        snode::log::application().error() << "Managed database exited before readiness";
                        notification->close();
                    }
                }
            },
            [this]() {
                stop();
            },
            process);
    }

} // namespace ci
