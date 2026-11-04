#pragma once
#include "core/Events.h"

#include <functional>

namespace coreci::persistence {
    class EventStore {
    public:
        virtual ~EventStore() = default;
        struct StoreError {
            enum class Kind { Unavailable, Corrupt } kind;
            std::string message;
        };
        virtual void open(std::function<void()> onReady, std::function<void(const StoreError&)> onError) = 0;
        // An error means the commit outcome is unknown.
        virtual void append(const Event&, std::function<void()> onCommitted, std::function<void(const std::string&)> onError) = 0;
        virtual void loadAll(std::function<void(std::uint64_t seq, const Event&)> onEvent,
                             std::function<void()> onDone,
                             std::function<void(const StoreError&)> onError) = 0;
    };
} // namespace coreci::persistence
