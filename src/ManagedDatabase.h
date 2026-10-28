#pragma once

#include "DatabaseConfig.h"

#include <core/eventreceiver/ReadEventReceiver.h>
#include <functional>
#include <sys/types.h>

namespace coreci {
    class ManagedDatabase {
    public:
        ManagedDatabase();
        ~ManagedDatabase();
        void start(const DatabaseConfig& config, const std::function<void()>& onReady);
        void stop();

    private:
        class Watch : public core::eventreceiver::ReadEventReceiver {
        public:
            Watch(ManagedDatabase& owner, std::function<void()> onRead);
            ~Watch() override;
            void attach(int descriptor);
            void close();
            int fd = -1;

        private:
            void readEvent() override;
            void unobservedEvent() override;
            void destruct() override;
            void shutdownEvent(const core::ShutdownContext&) override;
            ManagedDatabase& owner;
            std::function<void()> onRead;
        };
        void notified();
        void exited();
        pid_t pid = -1;
        Watch notification;
        Watch process;
        std::function<void()> ready;
    };
} // namespace coreci
