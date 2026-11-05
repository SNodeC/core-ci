#pragma once
#include "Commands.h"
#include "Events.h"

#include <deque>
#include <optional>

namespace coreci {
    enum class Phase { Dispatching, Building, AwaitingPublish, PublishDispatching, Publishing };
    struct Released {
        Version version;
        std::string tag;
        bool operator==(const Released&) const = default;
    };
    struct Published {
        Version version;
        std::uint64_t revision;
        Inputs inputs;
        bool operator==(const Published&) const = default;
    };
    struct Inflight {
        std::string buildId;
        Version version;
        std::string tag;
        std::uint64_t revision;
        Inputs inputs;
        Phase phase = Phase::Dispatching;
        unsigned attempt = 1;
        std::optional<std::int64_t> retryAt, deadline;
        std::optional<std::uint64_t> runId;
        std::string artifact;
        bool operator==(const Inflight&) const = default;
    };
    struct Failure {
        Version version;
        Inputs inputs;
        std::string reason;
        bool operator==(const Failure&) const = default;
    };
    struct UnitState {
        std::optional<Published> published;
        std::optional<Inflight> inflight;
        std::optional<Failure> failure;
        std::map<Version, std::uint64_t> lastRevision;
        std::string lastFeed;
        bool operator==(const UnitState&) const = default;
    };
    struct FeedState {
        std::deque<Unit> queue;
        std::optional<Unit> active;
        bool operator==(const FeedState&) const = default;
    };
    struct Derived {
        std::optional<Inputs> inputs;
        bool satisfied = false, settled = false, ready = false, failed = false;
        std::optional<Unit> waiting, blocker;
    };
    struct Options {
        std::int64_t buildTimeout, publishTimeout;
        unsigned maxDispatchAttempts = 8;
        std::optional<std::size_t> maxConcurrentBuilds;
    };
    struct Accept {
        bool operator==(const Accept&) const = default;
    };
    struct Ignore {
        std::string reason;
        bool operator==(const Ignore&) const = default;
    };
    using Classification = std::variant<Accept, Ignore>;
    class Engine {
    public:
        explicit Engine(Options options);
        Classification classify(const Event& event) const;
        std::vector<Command> handle(const Event& event);
        std::optional<std::int64_t> nextDeadline() const;
        std::map<Unit, Derived> derive() const;
        const Pipeline& pipeline() const;
        const std::map<std::string, std::optional<Released>>& releases() const;
        const std::map<Unit, UnitState>& units() const;
        const std::map<std::string, FeedState>& feeds() const;
        std::optional<std::int64_t> time() const;

    private:
        void changeGraph(const Pipeline& pipeline);
        bool active(const Unit& unit) const;
        std::optional<Unit> find(const std::string& buildId) const;
        Command dispatch(const Unit& unit) const;
        void clear(const Unit& unit);
        void fail(const Unit& unit, const std::string& reason);
        void schedule(std::vector<Command>& commands);
        Pipeline graph;
        Options options;
        std::map<std::string, std::optional<Released>> released;
        std::map<Unit, UnitState> unitStates;
        std::map<std::string, std::pair<Repo, Workflow>> lastIdentity;
        std::map<std::string, FeedState> feedStates;
        std::optional<std::int64_t> engineTime;
    };
} // namespace coreci
