#ifndef CORE_CI_MANAGED_DATABASE_H
#define CORE_CI_MANAGED_DATABASE_H

#include <core/eventreceiver/ReadEventReceiver.h>
#include <functional>
#include <sys/types.h>

namespace coreci {

    class DatabaseConfig;

    class ManagedDatabase {
    public:
        ManagedDatabase();
        ~ManagedDatabase();

        ManagedDatabase(const ManagedDatabase&) = delete;
        ManagedDatabase& operator=(const ManagedDatabase&) = delete;

        void start(const DatabaseConfig& config, const std::function<void()>& onReady);
        void stop();

    private:
        class Watch : public core::eventreceiver::ReadEventReceiver {
        public:
            Watch(ManagedDatabase& owner, bool childExit);
            ~Watch() override;
            void attach(int fd);
            void close();

        private:
            void readEvent() override;
            void unobservedEvent() override;
            void shutdownEvent(const core::ShutdownContext& context) override;
            void destruct() override;

            ManagedDatabase& owner;
            bool childExit;
            int fd = -1;
        };

        void receiveReady();
        void childExited();

        Watch notification;
        Watch child;
        pid_t pid = -1;
        std::function<void()> ready;
    };

} // namespace coreci

#endif
