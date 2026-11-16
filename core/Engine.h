#pragma once
#include "Commands.h"
#include "Events.h"

#include <optional>

namespace coreci {
    enum class State { Idle, Planned, Building, Built, Published, Failed, Blocked };
    std::string stateName(State state);
    enum class WavePhase { Dispatching, Running, Finished };
    std::string phaseName(WavePhase phase);
    struct Released {
        Version version;
        std::string tag, sourceSha;
        bool operator==(const Released&) const = default;
    };
    struct PlanUnit {
        Unit unit;
        Repo repo;
        Released release;
        std::uint64_t revision;
        Inputs inputs;
        std::string buildId;
        bool build;
        bool operator==(const PlanUnit&) const = default;
    };
    using Plan = std::map<std::string, std::vector<PlanUnit>>;
    struct Wave {
        std::string id;
        Plan plan;
        std::string feedBranch;
        Repo workflow;
        WavePhase phase = WavePhase::Dispatching;
        std::optional<std::uint64_t> runId;
        std::optional<unsigned> runAttempt;
        unsigned attempt = 1;
        std::optional<std::int64_t> retryAt;
        std::int64_t dispatchedAt = 0, deadline = 0;
        bool dispatchAccepted = false;
        std::optional<std::int64_t> started, finished;
        std::string conclusion;
        bool operator==(const Wave&) const = default;
    };
    struct Published {
        Version version;
        std::uint64_t revision;
        Inputs inputs;
        Repo source;
        std::string waveId;
        std::int64_t at;
        std::string feedCommit;
        bool operator==(const Published&) const = default;
    };
    struct Failure {
        Version version;
        Inputs inputs;
        std::string reason;
        std::uint64_t revision;
        std::int64_t at;
        bool buildFailed = false;
        bool operator==(const Failure&) const = default;
    };
    struct UnitState {
        State state = State::Idle;
        std::string waveId, artifact;
        std::uint64_t order = 0;
        std::optional<std::int64_t> buildStart, buildEnd, publishEnd;
        std::optional<Published> published;
        std::optional<Failure> failure;
        std::optional<std::int64_t> retryAfter;
        std::optional<std::pair<Version, Inputs>> autoRetried;
        std::map<Version, std::uint64_t> lastRevision;
        bool requested = false;
        bool operator==(const UnitState&) const = default;
    };
    struct Options {
        std::int64_t waveDelay = 120, waveDeadline = 23400;
        unsigned maxDispatchAttempts = 8;
        std::int64_t retryDelay = 600;
        std::string feedBranch = "feed";
        Repo workflow{"SNodeC", "Packages", "main"};
        std::int64_t waveStartTimeout = 900;
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
        explicit Engine(Options options = {});
        Classification classify(const Event& event) const;
        void handle(const Event& event, std::uint64_t seq = 0);
        std::vector<Command> commands() const;
        std::optional<std::int64_t> nextDeadline() const;
        std::uint64_t nextRevision(const Unit& unit) const;
        bool sameSource(const std::string& package, const Repo& source) const;
        const Pipeline& pipeline() const;
        const std::map<std::string, std::optional<Released>>& releases() const;
        const std::map<Unit, UnitState>& units() const;
        const std::map<std::string, Wave>& waves() const;
        const Wave* activeWave() const;
        const Wave* latestWave() const;
        const PlanUnit* selection(const Unit& unit) const;
        std::optional<std::int64_t> time() const;

    private:
        bool newerTag(const TagObserved& tag, const Pipeline& candidate) const;
        void changeGraph(const GraphChanged& event);
        Plan plan() const;
        void dirty();
        void request(const Unit& unit);
        void fail(const PlanUnit& unit, const std::string& reason, bool buildFailed = false, bool automatic = true);
        void finish(Wave& wave, const std::string& conclusion);
        Pipeline graph;
        Options options;
        std::map<std::string, std::optional<Released>> released;
        std::map<std::string, Repo> sources;
        std::map<Unit, UnitState> unitStates;
        std::map<std::string, Wave> waveStates;
        std::string latest;
        std::optional<std::int64_t> engineTime, quietUntil;
    };
} // namespace coreci
