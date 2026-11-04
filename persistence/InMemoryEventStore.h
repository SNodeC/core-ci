#pragma once
#include "EventRow.h"
#include "EventStore.h"

#include <deque>
#include <vector>

namespace coreci::persistence {
    class InMemoryEventStore : public EventStore {
    public:
        enum class Failure { None, BeforeStore, AfterStore };
        explicit InMemoryEventStore(std::vector<EventRow> rows = {});
        void open(std::function<void()> onReady, std::function<void(const std::string&)> onError) override;
        void append(const Event&, std::function<void()> onCommitted, std::function<void(const std::string&)> onError) override;
        void loadAll(std::function<void(std::uint64_t, const Event&)> onEvent,
                     std::function<void()> onDone,
                     std::function<void(const std::string&)> onError) override;
        void failNextAppend(Failure failure);
        void defer(bool enabled);
        bool completeOne();
        std::size_t pending() const;
        const std::vector<EventRow>& rows() const;

    private:
        void perform(std::function<void()> operation);
        std::vector<EventRow> stored;
        std::uint64_t nextSeq = 1;
        Failure failure = Failure::None;
        bool deferred = false;
        std::deque<std::function<void()>> operations;
    };
} // namespace coreci::persistence
