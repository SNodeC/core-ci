#pragma once
#include "EventRow.h"
#include "EventStore.h"
#include "core/Engine.h"

#include <deque>
#include <memory>

namespace coreci::persistence {
    struct Accepted {
        bool operator==(const Accepted&) const = default;
    };
    struct Ignored {
        std::string reason;
        bool operator==(const Ignored&) const = default;
    };
    struct Unavailable {
        std::string reason;
        bool operator==(const Unavailable&) const = default;
    };
    using Result = std::variant<Accepted, Ignored, Invalid, Unavailable>;
    class EventQueue {
    public:
        using Reply = std::function<void(Result)>;
        // Callers keep the queue/store alive until their outstanding callbacks finish.
        // app/ owns stamping, including the supplied Resumed event factory.
        EventQueue(EventStore& store,
                   std::function<Engine()> makeEngine,
                   std::function<Resumed()> resumed,
                   std::function<void(const std::vector<Command>&)> commands,
                   std::function<void(std::uint64_t, const std::string&)> warning);
        void start(std::function<void()> onReady, std::function<void(const std::string&)> onError);
        void enqueue(Event event, Reply reply);
        const Engine* engine() const;

    private:
        struct Pending {
            Event event;
            Reply reply;
        };
        enum class Mode { Unavailable, Loading, Resuming, Ready };
        void rebuild();
        void fatal(const std::string& error);
        void pump();
        void rejectPending(const std::string& error);
        EventStore& store;
        std::function<Engine()> makeEngine;
        std::function<Resumed()> resumed;
        std::function<void(const std::vector<Command>&)> commands;
        std::function<void(std::uint64_t, const std::string&)> warning;
        std::function<void()> onReady;
        std::function<void(const std::string&)> onError;
        std::unique_ptr<Engine> current;
        std::deque<Pending> pending;
        Mode mode = Mode::Unavailable;
        bool writing = false, draining = false;
    };
} // namespace coreci::persistence
