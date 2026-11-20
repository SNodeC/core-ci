#pragma once
#include "core/WaveController.h"

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
        virtual void load(std::function<void(StoredState)> done, std::function<void(const StoreError&)> error) = 0;
        // Failure means an unknown commit outcome; reload before any further decisions.
        virtual void commit(const StoredState& before,
                            const StoredState& after,
                            const std::string& description,
                            std::function<void()> done,
                            std::function<void(const StoreError&)> error) = 0;
    };
} // namespace coreci::persistence
