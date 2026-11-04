#include "Engine.h"

#include <algorithm>
#include <type_traits>

namespace coreci {
    Engine::Engine(Pipeline pipeline, Options options)
        : graph(std::move(pipeline))
        , options(options) {
        for (const auto& [name, pkg] : graph.packages()) {
            released.emplace(name, std::nullopt);
            for (const auto& target : pkg.targets) {
                unitStates.emplace(Unit{name, target}, UnitState{});
            }
        }
        for (const auto& [name, feed] : graph.feeds()) {
            feedStates.emplace(name, FeedState{});
        }
    }
    std::optional<Unit> Engine::find(const std::string& buildId) const {
        std::optional<Unit> result;
        for (const auto& [unit, state] : unitStates) {
            if (state.inflight && state.inflight->buildId == buildId) {
                result = unit;
            }
        }
        return result;
    }
    Classification Engine::classify(const Event& event) const {
        return std::visit(
            [&](const auto& e) -> Classification {
                using E = std::decay_t<decltype(e)>;
                Classification result = Ignore{"event not applicable"};
                if constexpr (std::is_same_v<E, TagObserved>) {
                    if (graph.packages().contains(e.package)) {
                        const auto version = Version::parse(e.tag);
                        const auto& current = released.at(e.package);
                        if (version && (!version->prerelease() || graph.packages().at(e.package).acceptPrereleases) &&
                            (!current || *version > current->version)) {
                            result = Accept{};
                        } else {
                            result = Ignore{"tag is not a newer accepted version"};
                        }
                    } else {
                        result = Ignore{"unknown package"};
                    }
                } else if constexpr (std::is_same_v<E, Resumed>) {
                    result = Accept{};
                } else if constexpr (std::is_same_v<E, Tick>) {
                    const auto deadline = nextDeadline();
                    if (deadline && std::max(engineTime.value_or(e.at), e.at) >= *deadline) {
                        result = Accept{};
                    } else {
                        result = Ignore{"no deadline due"};
                    }
                } else if constexpr (std::is_same_v<E, RetryRequested>) {
                    const auto it = unitStates.find({e.package, e.target});
                    if (it != unitStates.end() && it->second.failure && !it->second.inflight) {
                        result = Accept{};
                    } else {
                        result = Ignore{"unit has no retryable failure"};
                    }
                } else {
                    const auto unit = find(e.buildId);
                    result = Ignore{"unknown or stale buildId"};
                    if (unit) {
                        const auto& f = *unitStates.at(*unit).inflight;
                        const bool buildPhase = f.phase == Phase::Dispatching || f.phase == Phase::Building;
                        const bool publishPhase = f.phase == Phase::PublishDispatching || f.phase == Phase::Publishing;
                        bool accepted = false;
                        if constexpr (std::is_same_v<E, DispatchAccepted> || std::is_same_v<E, DispatchRejected>) {
                            accepted = (e.kind == Kind::Build ? f.phase == Phase::Dispatching : f.phase == Phase::PublishDispatching) &&
                                       e.attempt == f.attempt && !f.retryAt;
                        } else if constexpr (std::is_same_v<E, BuildStarted>) {
                            accepted = buildPhase && !f.runId;
                        } else if constexpr (std::is_same_v<E, BuildSucceeded> || std::is_same_v<E, BuildFailed>) {
                            accepted = buildPhase && (!f.runId || *f.runId == e.runId);
                        } else {
                            accepted = publishPhase;
                        }
                        result = accepted ? Classification{Accept{}} : Classification{Ignore{"wrong phase, attempt or runId"}};
                    }
                }
                return result;
            },
            event);
    }
    std::map<Unit, Derived> Engine::derive() const {
        std::map<Unit, Derived> result;
        for (const auto& [target, definition] : graph.targets()) {
            for (const auto& package : graph.topologicalOrder(target)) {
                const Unit unit{package, target};
                const auto& state = unitStates.at(unit);
                auto& d = result[unit];
                Inputs inputs;
                bool available = true;
                bool settled = true;
                auto prerequisites = graph.prerequisites(package);
                std::sort(prerequisites.begin(), prerequisites.end());
                for (const auto& prerequisite : prerequisites) {
                    const Unit q{prerequisite, target};
                    const auto& qState = unitStates.at(q);
                    const auto& qDerived = result.at(q);
                    if (qState.published) {
                        inputs.emplace(prerequisite, Input{qState.published->version, qState.published->revision});
                    } else {
                        available = false;
                    }
                    settled = settled && qDerived.settled;
                    if (!d.waiting && !qDerived.settled) {
                        d.waiting = q;
                    }
                    if (!d.blocker && (qDerived.failed || qDerived.blocker)) {
                        d.blocker = qDerived.failed ? std::optional<Unit>{q} : qDerived.blocker;
                    }
                }
                if (available) {
                    d.inputs = std::move(inputs);
                }
                const auto& release = released.at(package);
                d.satisfied = release && state.published && d.inputs && state.published->version == release->version &&
                              state.published->inputs == *d.inputs;
                d.failed = release && state.failure && d.inputs && state.failure->version == release->version &&
                           state.failure->inputs == *d.inputs;
                d.settled = d.satisfied && !state.inflight && settled;
                d.ready = release && !d.satisfied && !state.inflight && !d.failed && settled;
            }
        }
        return result;
    }
    Command Engine::dispatch(const Unit& unit) const {
        const auto& f = *unitStates.at(unit).inflight;
        const auto& pkg = graph.packages().at(unit.package);
        Command command;
        if (f.phase == Phase::Dispatching) {
            command = DispatchBuild{
                f.buildId, f.attempt, unit.package, pkg.sourceRepo, f.tag, f.version, f.revision, unit.target, f.inputs, pkg.buildWorkflow};
        } else {
            const auto& feed = graph.feeds().at(graph.targets().at(unit.target).feed);
            command = DispatchPublish{f.buildId,
                                      f.attempt,
                                      unit.package,
                                      f.version,
                                      f.revision,
                                      unit.target,
                                      feed.id,
                                      {pkg.buildWorkflow.owner, pkg.buildWorkflow.repo},
                                      *f.runId,
                                      f.artifact,
                                      feed.workflow};
        }
        return command;
    }
    void Engine::clear(const Unit& unit) {
        auto& feed = feedStates.at(graph.targets().at(unit.target).feed);
        std::erase(feed.queue, unit);
        if (feed.active == unit) {
            feed.active.reset();
        }
        unitStates.at(unit).inflight.reset();
    }
    void Engine::fail(const Unit& unit, const std::string& reason) {
        auto& state = unitStates.at(unit);
        state.failure = Failure{state.inflight->version, state.inflight->inputs, reason};
        clear(unit);
    }
    void Engine::schedule(std::vector<Command>& commands) {
        auto derived = derive();
        for (const auto& [unit, d] : derived) {
            const auto& f = unitStates.at(unit).inflight;
            if (f && f->phase == Phase::AwaitingPublish &&
                (f->version != released.at(unit.package)->version || !d.inputs || f->inputs != *d.inputs)) {
                clear(unit);
            }
        }
        derived = derive();
        std::size_t running = 0;
        for (const auto& [unit, state] : unitStates) {
            if (state.inflight && (state.inflight->phase == Phase::Dispatching || state.inflight->phase == Phase::Building)) {
                ++running;
            }
        }
        for (const auto& [target, definition] : graph.targets()) {
            for (const auto& package : graph.topologicalOrder(target)) {
                const Unit unit{package, target};
                if (derived.at(unit).ready && (!options.maxConcurrentBuilds || running < *options.maxConcurrentBuilds)) {
                    auto& state = unitStates.at(unit);
                    const auto& release = *released.at(package);
                    const auto revision = ++state.lastRevision[release.version];
                    const auto id = package + "." + target + "." + release.version.text() + ".r" + std::to_string(revision);
                    state.inflight = Inflight{
                        id, release.version, release.tag, revision, *derived.at(unit).inputs, Phase::Dispatching, 1, {}, {}, {}, {}};
                    commands.push_back(dispatch(unit));
                    ++running;
                }
            }
        }
        for (auto& [name, feed] : feedStates) {
            if (!feed.active && !feed.queue.empty()) {
                const auto unit = feed.queue.front();
                feed.queue.pop_front();
                feed.active = unit;
                auto& f = *unitStates.at(unit).inflight;
                f.phase = Phase::PublishDispatching;
                f.attempt = 1;
                commands.push_back(dispatch(unit));
            }
        }
    }
    std::vector<Command> Engine::handle(const Event& event) {
        std::vector<Command> commands;
        if (std::holds_alternative<Accept>(classify(event))) {
            std::visit(
                [&](const auto& e) {
                    using E = std::decay_t<decltype(e)>;
                    engineTime = std::max(engineTime.value_or(e.at), e.at);
                    if constexpr (std::is_same_v<E, TagObserved>) {
                        released.at(e.package) = Released{*Version::parse(e.tag), e.tag};
                    } else if constexpr (std::is_same_v<E, RetryRequested>) {
                        unitStates.at({e.package, e.target}).failure.reset();
                    } else if constexpr (std::is_same_v<E, Tick> || std::is_same_v<E, Resumed>) {
                        for (const auto& [target, definition] : graph.targets()) {
                            for (const auto& package : graph.topologicalOrder(target)) {
                                const Unit unit{package, target};
                                auto& f = unitStates.at(unit).inflight;
                                if (f) {
                                    if constexpr (std::is_same_v<E, Resumed>) {
                                        if ((f->phase == Phase::Dispatching || f->phase == Phase::PublishDispatching) && !f->retryAt) {
                                            commands.push_back(dispatch(unit));
                                        }
                                    } else {
                                        if (f->retryAt && *engineTime >= *f->retryAt) {
                                            ++f->attempt;
                                            f->retryAt.reset();
                                            commands.push_back(dispatch(unit));
                                        } else if (f->deadline && *engineTime >= *f->deadline) {
                                            fail(unit, f->phase == Phase::Building ? "build timeout" : "publish timeout");
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        const auto unit = *find(e.buildId);
                        auto& state = unitStates.at(unit);
                        auto& f = *state.inflight;
                        if constexpr (std::is_same_v<E, DispatchRejected>) {
                            if (e.httpStatus == 401 || e.httpStatus == 403 || e.httpStatus == 404 || e.httpStatus == 422 ||
                                f.attempt == options.maxDispatchAttempts) {
                                fail(unit, "dispatch rejected (" + std::to_string(e.httpStatus) + "): " + e.message);
                            } else {
                                std::int64_t delay = 30;
                                for (unsigned i = 1; i < f.attempt && delay < 1800; ++i) {
                                    delay = std::min<std::int64_t>(delay * 2, 1800);
                                }
                                f.retryAt = *engineTime + delay;
                            }
                        } else {
                            if (f.phase == Phase::Dispatching || f.phase == Phase::PublishDispatching) {
                                const bool build = f.phase == Phase::Dispatching;
                                f.phase = build ? Phase::Building : Phase::Publishing;
                                f.retryAt.reset();
                                f.deadline = *engineTime + (build ? options.buildTimeout : options.publishTimeout);
                            }
                            if constexpr (std::is_same_v<E, BuildStarted>) {
                                f.runId = e.runId;
                            } else if constexpr (std::is_same_v<E, BuildSucceeded>) {
                                f.runId = e.runId;
                                f.artifact = e.artifact;
                                const auto d = derive().at(unit);
                                if (f.version == released.at(unit.package)->version && d.inputs && f.inputs == *d.inputs) {
                                    f.phase = Phase::AwaitingPublish;
                                    f.deadline.reset();
                                    feedStates.at(graph.targets().at(unit.target).feed).queue.push_back(unit);
                                } else {
                                    clear(unit);
                                }
                            } else if constexpr (std::is_same_v<E, PublishSucceeded>) {
                                state.published = Published{f.version, f.revision, f.inputs};
                                state.failure.reset();
                                clear(unit);
                            } else if constexpr (std::is_same_v<E, BuildFailed> || std::is_same_v<E, PublishFailed>) {
                                fail(unit, e.reason);
                            }
                        }
                    }
                },
                event);
            schedule(commands);
        }
        return commands;
    }
    std::optional<std::int64_t> Engine::nextDeadline() const {
        std::optional<std::int64_t> result;
        for (const auto& [unit, state] : unitStates) {
            if (state.inflight) {
                const auto& f = *state.inflight;
                for (const auto& time : {f.retryAt, f.deadline}) {
                    if (time && (!result || *time < *result)) {
                        result = time;
                    }
                }
            }
        }
        return result;
    }
    const Pipeline& Engine::pipeline() const {
        return graph;
    }
    const std::map<std::string, std::optional<Released>>& Engine::releases() const {
        return released;
    }
    const std::map<Unit, UnitState>& Engine::units() const {
        return unitStates;
    }
    const std::map<std::string, FeedState>& Engine::feeds() const {
        return feedStates;
    }
    std::optional<std::int64_t> Engine::time() const {
        return engineTime;
    }
} // namespace coreci
