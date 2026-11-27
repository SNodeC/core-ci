#pragma once
#include "core/Scheduler.h"

#include <functional>

namespace coreci::persistence {
    struct StoreError {
        enum class Kind { Unavailable, Corrupt } kind;
        std::string message;
    };

    class StateStore {
    public:
        virtual ~StateStore() = default;
        virtual void open(std::function<void()> ready, std::function<void(const StoreError&)> error) = 0;
        virtual void load(std::function<void(Json)> done, std::function<void(const StoreError&)> error) = 0;
        virtual void commit(const Json& before,
                            const Json& after,
                            const std::string& description,
                            std::function<void()> done,
                            std::function<void(const StoreError&)> error) = 0;
    };
} // namespace coreci::persistence
