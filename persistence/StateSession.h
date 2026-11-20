#pragma once
#include "Input.h"
#include "StateStore.h"

#include <deque>
#include <memory>

namespace coreci::persistence {
    struct Accepted {};
    struct Ignored {
        std::string reason;
    };
    struct Unavailable {
        std::string reason;
    };
    using Result = std::variant<Accepted, Ignored, Invalid, Unavailable>;
    class StateSession {
    public:
        using Reply = std::function<void(Result)>;
        StateSession(StateStore& store, Options options, std::function<void(std::uint64_t)> changed, std::function<bool()> operational);
        void start(std::function<void()> ready, std::function<void(const StoreError&)> error);
        void enqueue(Event input, Reply reply);
        void recover(const std::string& reason);
        const WaveController* engine() const;

    private:
        struct Pending {
            Event input;
            Reply reply;
        };
        bool live(std::uint64_t epoch) const;
        void pump();
        void failed(const StoreError& error);
        StateStore& store;
        Options options;
        std::function<void(std::uint64_t)> changed;
        std::function<bool()> operational;
        std::function<void()> ready;
        std::function<void(const StoreError&)> error;
        std::unique_ptr<WaveController> current;
        std::deque<Pending> pending;
        Reply active;
        std::uint64_t epoch = 0;
        bool writing = false, draining = false;
    };
} // namespace coreci::persistence
