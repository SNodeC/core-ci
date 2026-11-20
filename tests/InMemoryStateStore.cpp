#include "InMemoryStateStore.h"

#include "Check.h"

#include <utility>

namespace coreci::test {
    void InMemoryStateStore::schedule(std::function<void()> action) {
        if (deferred) {
            pending.push_back(std::move(action));
        } else {
            action();
        }
    }
    bool InMemoryStateStore::complete() {
        const bool available = !pending.empty();
        if (available) {
            auto action = std::move(pending.front());
            pending.pop_front();
            action();
        }
        return available;
    }
    void InMemoryStateStore::open(std::function<void()> ready, std::function<void(const persistence::StoreError&)>) {
        schedule(std::move(ready));
    }
    void InMemoryStateStore::load(std::function<void(StoredState)> done, std::function<void(const persistence::StoreError&)>) {
        schedule([this, done] {
            done(state);
        });
    }
    void InMemoryStateStore::commit(const StoredState& before,
                                    const StoredState& after,
                                    const std::string& description,
                                    std::function<void()> done,
                                    std::function<void(const persistence::StoreError&)> error) {
        CHECK(state == before);
        const auto mode = std::exchange(failure, Failure::None);
        schedule([this, after, description, done, error, mode] {
            if (mode != Failure::BeforeCommit) {
                state = persistence::decodeState(persistence::encodeState(after));
                log.push_back(description);
            }
            if (mode == Failure::None) {
                done();
            } else {
                error({persistence::StoreError::Kind::Unavailable, "injected unknown commit outcome"});
            }
        });
    }
} // namespace coreci::test
