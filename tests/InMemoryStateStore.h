#pragma once
#include "persistence/StateSql.h"

#include <deque>

namespace coreci::test {
    class InMemoryStateStore : public persistence::StateStore {
    public:
        enum class Failure { None, BeforeCommit, AfterCommit };
        void open(std::function<void()> ready, std::function<void(const persistence::StoreError&)>) override;
        void load(std::function<void(StoredState)> done, std::function<void(const persistence::StoreError&)>) override;
        void commit(const StoredState& before,
                    const StoredState& after,
                    const std::string& description,
                    std::function<void()> done,
                    std::function<void(const persistence::StoreError&)> error) override;
        bool complete();
        StoredState state;
        Failure failure = Failure::None;
        bool deferred = false;
        std::vector<std::string> log;

    private:
        void schedule(std::function<void()> action);
        std::deque<std::function<void()>> pending;
    };
} // namespace coreci::test
