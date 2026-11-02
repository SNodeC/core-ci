#pragma once

#include <core/eventreceiver/ReadEventReceiver.h>
#include <functional>
#include <string>

namespace ci {

    class ManagedDatabase {
    public:
        ManagedDatabase() = default;
        ~ManagedDatabase();
        void start(const std::string& directory, const std::string& socket, const std::function<void()>& ready);
        void stop();

    private:
        class Input;
        Input* notification = nullptr;
        Input* process = nullptr;
        int child = -1;
        std::string notificationPath;
    };

} // namespace ci
