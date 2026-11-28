#pragma once
#include "StateStore.h"

#include <deque>
#include <memory>

namespace coreci::persistence {
    class StateSession {
    public:
        using Reply = std::function<void(Result)>;
        using Transition = std::function<Result(Json&)>;
        StateSession(StateStore& store, Options options, std::function<void()> changed, std::function<bool()> operational);
        void start(std::function<void()> ready, std::function<void(const StoreError&)> error);
        void enqueue(std::string description, Transition apply, Reply reply = {});
        void recover(const std::string& reason);
        const Json* state() const;
        const Scheduler& scheduler() const;

    private:
        struct Pending {
            std::string description;
            Transition apply;
            Reply reply;
        };
        bool live(std::uint64_t epoch) const;
        void pump();
        void failed(const StoreError& error);
        StateStore& store;
        Scheduler driver;
        std::function<void()> changed;
        std::function<bool()> operational;
        std::function<void()> ready;
        std::function<void(const StoreError&)> error;
        std::unique_ptr<Json> current;
        std::deque<Pending> pending;
        Reply active;
        std::uint64_t epoch = 0;
        bool writing = false, draining = false;
    };
} // namespace coreci::persistence
