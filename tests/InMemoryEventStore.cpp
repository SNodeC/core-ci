#include "InMemoryEventStore.h"

#include <algorithm>

namespace coreci::persistence {
    InMemoryEventStore::InMemoryEventStore(std::vector<EventRow> rows)
        : stored(std::move(rows)) {
        for (const auto& row : stored) {
            nextSeq = std::max(nextSeq, row.seq + 1);
        }
    }
    void InMemoryEventStore::perform(std::function<void()> operation) {
        if (deferred) {
            operations.push_back(std::move(operation));
        } else {
            operation();
        }
    }
    void InMemoryEventStore::open(std::function<void()> onReady, std::function<void(const StoreError&)>) {
        perform(std::move(onReady));
    }
    void InMemoryEventStore::append(const Event& event,
                                    std::function<void(std::uint64_t)> onCommitted,
                                    std::function<void(const std::string&)> onError) {
        const auto row = encode(event, nextSeq++);
        const auto outcome = failure;
        failure = Failure::None;
        perform([this, row, outcome, onCommitted = std::move(onCommitted), onError = std::move(onError)] {
            if (std::holds_alternative<Invalid>(row)) {
                onError(std::get<Invalid>(row).reason);
            } else {
                if (outcome != Failure::BeforeStore) {
                    stored.push_back(std::get<EventRow>(row));
                }
                if (outcome == Failure::None) {
                    onCommitted(std::get<EventRow>(row).seq);
                } else {
                    onError("injected append error: outcome unknown");
                }
            }
        });
    }
    void InMemoryEventStore::loadAll(std::function<void(std::uint64_t, const Event&)> onEvent,
                                     std::function<void()> onDone,
                                     std::function<void(const StoreError&)> onError) {
        perform([this, onEvent = std::move(onEvent), onDone = std::move(onDone), onError = std::move(onError)] {
            auto snapshot = stored;
            std::sort(snapshot.begin(), snapshot.end(), [](const auto& a, const auto& b) {
                return a.seq < b.seq;
            });
            bool valid = true;
            for (std::size_t i = 0; i < snapshot.size() && valid; ++i) {
                const auto event = decode(snapshot[i]);
                valid = std::holds_alternative<Event>(event);
                if (valid) {
                    onEvent(snapshot[i].seq, std::get<Event>(event));
                } else {
                    onError({StoreError::Kind::Corrupt, std::get<Invalid>(event).reason});
                }
            }
            if (valid) {
                onDone();
            }
        });
    }
    void InMemoryEventStore::failNextAppend(Failure nextFailure) {
        failure = nextFailure;
    }
    void InMemoryEventStore::defer(bool enabled) {
        deferred = enabled;
    }
    bool InMemoryEventStore::completeOne() {
        const bool available = !operations.empty();
        if (available) {
            auto operation = std::move(operations.front());
            operations.pop_front();
            operation();
        }
        return available;
    }
    std::size_t InMemoryEventStore::pending() const {
        return operations.size();
    }
    const std::vector<EventRow>& InMemoryEventStore::rows() const {
        return stored;
    }
} // namespace coreci::persistence
