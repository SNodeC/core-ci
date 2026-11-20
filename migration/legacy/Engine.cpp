#include "Engine.h"

#include <algorithm>
#include <type_traits>

namespace coreci::legacy {
    std::string stateName(State state) {
        const char* names[] = {"Idle", "Planned", "Building", "Built", "Published", "Failed", "Blocked"};
        return names[static_cast<unsigned>(state)];
    }
    std::string phaseName(WavePhase phase) {
        const char* names[] = {"Dispatching", "Running", "Finished"};
        return names[static_cast<unsigned>(phase)];
    }
    Engine::Engine(Options options)
        : options(std::move(options)) {
    }
    bool Engine::newerTag(const TagObserved& tag, const Pipeline& candidate) const {
        const auto version = Version::parse(tag.tag);
        const auto old = released.find(tag.package);
        return candidate.packages().contains(tag.package) && version && !version->prerelease() &&
               (old == released.end() || !old->second || !sameSource(tag.package, candidate.packages().at(tag.package).sourceRepo) ||
                *version > old->second->version);
    }
    void Engine::dirty() {
        quietUntil = *engineTime + options.waveDelay;
    }
    void Engine::changeGraph(const GraphChanged& event) {
        bool changed = false;
        for (const auto& [name, pkg] : event.pipeline.packages()) {
            const bool sourceChanged = !sameSource(name, pkg.sourceRepo);
            if (sourceChanged) {
                released[name].reset();
                changed = true;
            }
            sources[name] = pkg.sourceRepo;
            for (const auto& target : pkg.targets) {
                changed = !graph.packages().contains(name) || !graph.packages().at(name).targets.contains(target) || changed;
                auto& state = unitStates[Unit{name, target}];
                if (sourceChanged) {
                    state.failure.reset();
                    state.autoRetried.reset();
                    state.retryAfter.reset();
                }
            }
            if (!graph.packages().contains(name) || graph.packages().at(name).prerequisites != pkg.prerequisites) {
                changed = true;
            }
        }
        graph = event.pipeline;
        for (const auto& tag : event.tags) {
            if (newerTag(tag, graph)) {
                released[tag.package] = Released{*Version::parse(tag.tag), tag.tag, tag.sourceSha};
                changed = true;
            }
        }
        if (changed) {
            dirty();
        }
    }
    const Wave* Engine::activeWave() const {
        const auto* wave = latestWave();
        return wave && wave->phase != WavePhase::Finished ? wave : nullptr;
    }
    const Wave* Engine::latestWave() const {
        return latest.empty() ? nullptr : &waveStates.at(latest);
    }
    const PlanUnit* Engine::selection(const Unit& unit) const {
        const PlanUnit* result = nullptr;
        const auto state = unitStates.find(unit);
        if (state != unitStates.end() && waveStates.contains(state->second.waveId)) {
            const auto& plan = waveStates.at(state->second.waveId).plan;
            if (plan.contains(unit.target)) {
                for (const auto& entry : plan.at(unit.target)) {
                    if (entry.unit == unit && entry.build) {
                        result = &entry;
                    }
                }
            }
        }
        return result;
    }
    Classification Engine::classify(const Event& event) const {
        const auto now = [&](std::int64_t at) {
            return std::max(engineTime.value_or(at), at);
        };
        return std::visit(
            [&](const auto& e) -> Classification {
                using E = std::decay_t<decltype(e)>;
                bool allowed = false;
                if constexpr (std::is_same_v<E, TagObserved>) {
                    allowed = newerTag(e, graph);
                } else if constexpr (std::is_same_v<E, GraphChanged>) {
                    allowed = graph != e.pipeline;
                    for (const auto& tag : e.tags) {
                        allowed = allowed || newerTag(tag, e.pipeline);
                    }
                } else if constexpr (std::is_same_v<E, Resumed>) {
                    allowed = true;
                } else if constexpr (std::is_same_v<E, Tick>) {
                    const auto next = nextDeadline();
                    allowed = next && now(e.at) >= *next;
                } else if constexpr (std::is_same_v<E, RetryRequested> || std::is_same_v<E, RebuildRequested>) {
                    const Unit unit{e.package, e.target};
                    const auto state = unitStates.find(unit);
                    allowed = graph.packages().contains(e.package) && graph.packages().at(e.package).targets.contains(e.target) &&
                              state != unitStates.end() && !state->second.requested &&
                              state->second.state == (std::is_same_v<E, RetryRequested> ? State::Failed : State::Published);
                } else if (const auto found = waveStates.find(e.waveId); found != waveStates.end()) {
                    const auto& wave = found->second;
                    if constexpr (std::is_same_v<E, WaveDispatchAccepted> || std::is_same_v<E, WaveDispatchRejected>) {
                        allowed =
                            wave.phase == WavePhase::Dispatching && !wave.dispatchAccepted && !wave.retryAt && e.attempt == wave.attempt;
                    } else if constexpr (std::is_same_v<E, WaveStarted>) {
                        allowed = !wave.finished && !wave.runId && e.runId > 0 && e.runAttempt > 0;
                    } else if constexpr (std::is_same_v<E, WaveExpired>) {
                        allowed = !wave.finished && now(e.at) >= wave.deadline;
                    } else if constexpr (std::is_same_v<E, WaveFinished>) {
                        allowed = wave.runId == e.runId && (!wave.finished || wave.awaitingOwnerFinish);
                    } else {
                        for (const auto& [target, entries] : wave.plan) {
                            for (const auto& entry : entries) {
                                if (entry.build && entry.buildId == e.buildId) {
                                    const auto& outcome = unitStates.at(entry.unit);
                                    const auto state =
                                        wave.awaitingOwnerFinish && outcome.failure && !outcome.failure->buildFailed && !outcome.publishEnd
                                            ? (outcome.artifact.empty() ? State::Planned : State::Built)
                                            : outcome.state;
                                    if constexpr (std::is_same_v<E, UnitBuildStarted>) {
                                        allowed = outcome.waveId == wave.id && state == State::Planned;
                                    } else if constexpr (std::is_same_v<E, UnitBuildSucceeded> || std::is_same_v<E, UnitBuildFailed>) {
                                        allowed = outcome.waveId == wave.id && (state == State::Planned || state == State::Building);
                                    } else if constexpr (std::is_same_v<E, UnitPublishSucceeded>) {
                                        allowed = outcome.waveId == wave.id
                                                      ? state == State::Built
                                                      : wave.runId &&
                                                            (!outcome.published || std::stoull(wave.id.substr(1)) >
                                                                                       std::stoull(outcome.published->waveId.substr(1)));
                                    } else {
                                        allowed = outcome.waveId == wave.id && state == State::Built;
                                    }
                                }
                            }
                        }
                    }
                }
                return allowed ? Classification{Accept{}} : Classification{Ignore{"unknown, duplicate or stale event"}};
            },
            event);
    }
    std::uint64_t Engine::nextRevision(const Unit& unit) const {
        const auto& version = released.at(unit.package)->version;
        const auto& previous = unitStates.at(unit).lastRevision;
        return std::max<std::uint64_t>(previous.contains(version) ? previous.at(version) : 0,
                                       graph.packages().at(unit.package).revisionBase) +
               1;
    }
    Plan Engine::plan() const {
        Plan result;
        for (const auto& [target, definition] : graph.targets()) {
            std::map<std::string, PlanUnit> selected;
            std::vector<PlanUnit> entries;
            bool builds = false;
            for (const auto& name : graph.topologicalOrder(target)) {
                const auto& pkg = graph.packages().at(name);
                const auto& release = released.at(name);
                const Unit unit{name, target};
                const auto& state = unitStates.at(unit);
                bool available = release.has_value(), cascade = false;
                Inputs inputs;
                for (const auto& prerequisite : pkg.prerequisites) {
                    const auto q = selected.find(prerequisite);
                    available = available && q != selected.end();
                    if (q != selected.end()) {
                        inputs.emplace(prerequisite, Input{q->second.release.version, q->second.revision});
                        cascade = cascade || q->second.build;
                    }
                }
                if (available) {
                    const bool build = state.requested || cascade || !state.published ||
                                       !sameIdentity(state.published->source, pkg.sourceRepo) ||
                                       state.published->version != release->version || state.published->inputs != inputs;
                    const bool failed = state.failure && state.failure->version == release->version && state.failure->inputs == inputs;
                    if (!build || !failed || state.requested || cascade) {
                        PlanUnit entry{
                            unit, pkg.sourceRepo, *release, build ? nextRevision(unit) : state.published->revision, inputs, {}, build};
                        selected.emplace(name, entry);
                        entries.push_back(std::move(entry));
                        builds = builds || build;
                    }
                }
            }
            if (builds) {
                result.emplace(target, std::move(entries));
            }
        }
        return result;
    }
    void Engine::request(const Unit& unit) {
        unitStates.at(unit).requested = true;
        unitStates.at(unit).retryAfter.reset();
        if (graph.packages().contains(unit.package) && graph.packages().at(unit.package).targets.contains(unit.target)) {
            std::set<std::string> requested{unit.package};
            for (const auto& name : graph.topologicalOrder(unit.target)) {
                auto& state = unitStates.at({name, unit.target});
                const auto& prerequisites = graph.prerequisites(name);
                if (state.state == State::Blocked && std::any_of(prerequisites.begin(), prerequisites.end(), [&](const auto& q) {
                        return requested.contains(q);
                    })) {
                    state.requested = true;
                    requested.insert(name);
                }
            }
        }
        dirty();
    }
    void Engine::fail(const PlanUnit& entry, const std::string& reason, bool buildFailed, bool automatic) {
        auto& state = unitStates.at(entry.unit);
        state.state = State::Failed;
        state.failure = Failure{entry.release.version, entry.inputs, reason, entry.revision, *engineTime, buildFailed};
        state.retryAfter.reset();
        if (!state.buildEnd) {
            state.buildEnd = *engineTime;
        }
        if (automatic && options.retryDelay > 0 && state.autoRetried != std::pair{entry.release.version, entry.inputs}) {
            state.retryAfter = *engineTime + options.retryDelay;
        }
    }
    Event Engine::record(Event event) const {
        event.dispatchDeadline = std::visit(
                                     [&](const auto& e) {
                                         return std::max(engineTime.value_or(e.at), e.at);
                                     },
                                     event) +
                                 options.waveDeadline;
        return event;
    }
    void Engine::finish(Wave& wave, const std::string& conclusion, bool expired) {
        for (const auto& [target, entries] : wave.plan) {
            std::set<std::string> blocked;
            for (const auto& entry : entries) {
                if (entry.build && !expired && unitStates.at(entry.unit).waveId == wave.id) {
                    auto& state = unitStates.at(entry.unit);
                    const bool prerequisiteFailed = std::any_of(entry.inputs.begin(), entry.inputs.end(), [&](const auto& input) {
                        return blocked.contains(input.first);
                    });
                    if (state.state == State::Planned && prerequisiteFailed) {
                        state.state = State::Blocked;
                        state.retryAfter.reset();
                    } else if (state.state == State::Planned || state.state == State::Building || state.state == State::Built) {
                        fail(entry, "no result");
                    }
                    if (state.state == State::Blocked || (state.failure && state.failure->buildFailed)) {
                        blocked.insert(entry.unit.package);
                    }
                }
            }
        }
        wave.phase = WavePhase::Finished;
        wave.finished = *engineTime;
        wave.conclusion = conclusion;
        wave.retryAt.reset();
        wave.awaitingOwnerFinish = expired;
    }
    void Engine::handle(const Event& event, std::uint64_t seq) {
        if (std::holds_alternative<Accept>(classify(event))) {
            if (const auto* success = std::get_if<UnitBuildSucceeded>(&event)) {
                for (const auto& [target, entries] : waveStates.at(success->waveId).plan) {
                    for (const auto& entry : entries) {
                        if (entry.buildId == success->buildId) {
                            builds[{success->waveId, target}][entry.buildId] = true;
                        }
                    }
                }
            }
            if (const auto* failure = std::get_if<UnitBuildFailed>(&event)) {
                for (const auto& [target, entries] : waveStates.at(failure->waveId).plan) {
                    for (const auto& entry : entries) {
                        if (entry.buildId == failure->buildId) {
                            builds[{failure->waveId, target}][entry.buildId] = false;
                        }
                    }
                }
            }
            std::visit(
                [&](const auto& e) {
                    using E = std::decay_t<decltype(e)>;
                    engineTime = std::max(engineTime.value_or(e.at), e.at);
                    if constexpr (std::is_same_v<E, GraphChanged>) {
                        changeGraph(e);
                    } else if constexpr (std::is_same_v<E, TagObserved>) {
                        released[e.package] = Released{*Version::parse(e.tag), e.tag, e.sourceSha};
                        dirty();
                    } else if constexpr (std::is_same_v<E, RetryRequested> || std::is_same_v<E, RebuildRequested>) {
                        request({e.package, e.target});
                    } else if constexpr (std::is_same_v<E, Tick> || std::is_same_v<E, Resumed>) {
                        if (activeWave()) {
                            auto& wave = waveStates.at(latest);
                            if (*engineTime >= wave.deadline && !event.dispatchDeadline) {
                                finish(wave, "timeout");
                                wave.awaitingOwnerFinish = true;
                            } else if (*engineTime < wave.deadline && ((wave.retryAt && *engineTime >= *wave.retryAt) ||
                                                                       (!wave.retryAt && wave.phase == WavePhase::Dispatching &&
                                                                        *engineTime >= wave.dispatchedAt + options.waveStartTimeout))) {
                                ++wave.attempt;
                                wave.dispatchAccepted = false;
                                if (wave.retryAt) {
                                    wave.deadline = event.dispatchDeadline.value_or(*engineTime + 23400);
                                }
                                wave.retryAt.reset();
                                wave.dispatchedAt = *engineTime;
                            }
                        }
                        for (const auto& [name, pkg] : graph.packages()) {
                            for (const auto& target : pkg.targets) {
                                const Unit unit{name, target};
                                auto& state = unitStates.at(unit);
                                if (state.failure && state.retryAfter && *engineTime >= *state.retryAfter) {
                                    state.autoRetried = {state.failure->version, state.failure->inputs};
                                    request(unit);
                                }
                            }
                        }
                    } else {
                        auto& wave = waveStates.at(e.waveId);
                        if constexpr (std::is_same_v<E, WaveExpired>) {
                            finish(wave, "timeout", true);
                        } else if constexpr (std::is_same_v<E, WaveDispatchRejected>) {
                            const bool permanent = e.httpStatus == 401 || e.httpStatus == 403 || e.httpStatus == 404 || e.httpStatus == 422;
                            if (permanent || wave.attempt >= options.maxDispatchAttempts) {
                                for (const auto& [target, entries] : wave.plan) {
                                    for (const auto& entry : entries) {
                                        if (entry.build) {
                                            fail(entry,
                                                 "dispatch rejected (" + std::to_string(e.httpStatus) + "): " + e.message,
                                                 false,
                                                 !permanent);
                                        }
                                    }
                                }
                                finish(wave, "dispatch rejected");
                            } else {
                                std::int64_t delay = 30;
                                for (unsigned i = 1; i < wave.attempt && delay < 1800; ++i) {
                                    delay = std::min<std::int64_t>(delay * 2, 1800);
                                }
                                wave.retryAt = *engineTime + delay;
                            }
                        } else {
                            if constexpr (std::is_same_v<E, WaveDispatchAccepted>) {
                                wave.dispatchAccepted = true;
                            } else if constexpr (std::is_same_v<E, WaveStarted>) {
                                wave.phase = WavePhase::Running;
                                wave.runId = e.runId;
                                wave.runAttempt = e.runAttempt;
                                wave.started = *engineTime;
                            } else if constexpr (std::is_same_v<E, WaveFinished>) {
                                finish(wave, e.conclusion);
                            } else {
                                wave.phase = wave.finished ? WavePhase::Finished : WavePhase::Running;
                                for (const auto& [target, entries] : wave.plan) {
                                    for (const auto& entry : entries) {
                                        if (entry.build && entry.buildId == e.buildId) {
                                            auto& state = unitStates.at(entry.unit);
                                            if (state.waveId == wave.id) {
                                                state.failure.reset();
                                                state.retryAfter.reset();
                                            }
                                            if constexpr (std::is_same_v<E, UnitBuildStarted>) {
                                                state.state = State::Building;
                                                state.buildStart = *engineTime;
                                                state.buildEnd.reset();
                                            } else if constexpr (std::is_same_v<E, UnitBuildSucceeded>) {
                                                state.state = State::Built;
                                                state.artifact = e.artifact;
                                                state.buildEnd = *engineTime;
                                            } else if constexpr (std::is_same_v<E, UnitBuildFailed>) {
                                                state.buildEnd = *engineTime;
                                                fail(entry, e.reason, true);
                                            } else if constexpr (std::is_same_v<E, UnitPublishSucceeded>) {
                                                state.published = Published{entry.release.version,
                                                                            entry.revision,
                                                                            entry.inputs,
                                                                            entry.repo,
                                                                            wave.id,
                                                                            *engineTime,
                                                                            e.feedCommit};
                                                if (state.waveId == wave.id) {
                                                    state.state = State::Published;
                                                    state.publishEnd = *engineTime;
                                                }
                                            } else if constexpr (std::is_same_v<E, UnitPublishFailed>) {
                                                state.publishEnd = *engineTime;
                                                fail(entry, e.reason);
                                            }
                                        }
                                    }
                                }
                            }
                            wave.retryAt.reset();
                        }
                    }
                },
                event);
            if (!activeWave() && quietUntil && *engineTime >= *quietUntil) {
                auto planned = plan();
                quietUntil.reset();
                if (!planned.empty()) {
                    Wave wave;
                    wave.id = "w" + std::to_string(seq);
                    wave.plan = std::move(planned);
                    wave.feedBranch = options.feedBranch;
                    wave.workflow = options.workflow;
                    wave.dispatchedAt = *engineTime;
                    wave.deadline = event.dispatchDeadline.value_or(*engineTime + 23400);
                    for (auto& [target, entries] : wave.plan) {
                        for (auto& entry : entries) {
                            if (entry.build) {
                                entry.buildId = wave.id + "~" + entry.unit.package + "~" + target;
                                auto& state = unitStates.at(entry.unit);
                                state.state = State::Planned;
                                state.waveId = wave.id;
                                state.order = seq;
                                state.artifact.clear();
                                state.buildStart.reset();
                                state.buildEnd.reset();
                                state.publishEnd.reset();
                                state.failure.reset();
                                state.retryAfter.reset();
                                state.requested = false;
                                state.lastRevision[entry.release.version] = entry.revision;
                            }
                        }
                    }
                    latest = wave.id;
                    waveStates.emplace(latest, std::move(wave));
                }
            }
        }
    }
    std::vector<Command> Engine::commands() const {
        std::vector<Command> result;
        const auto* wave = activeWave();
        if (wave && wave->phase == WavePhase::Dispatching && !wave->dispatchAccepted && !wave->retryAt && *engineTime < wave->deadline) {
            result.push_back({wave->id, wave->attempt, wave->workflow, wave->feedBranch});
        }
        return result;
    }
    std::optional<std::int64_t> Engine::nextDeadline() const {
        std::optional<std::int64_t> result;
        const auto add = [&](std::optional<std::int64_t> value) {
            if (value && (!result || *value < *result)) {
                result = value;
            }
        };
        if (const auto* wave = activeWave()) {
            add(wave->deadline);
            add(wave->retryAt);
            if (wave->phase == WavePhase::Dispatching && !wave->retryAt) {
                add(wave->dispatchedAt + options.waveStartTimeout);
            }
        } else {
            add(quietUntil);
        }
        for (const auto& [name, pkg] : graph.packages()) {
            for (const auto& target : pkg.targets) {
                add(unitStates.at({name, target}).retryAfter);
            }
        }
        return result;
    }
    bool Engine::sameSource(const std::string& package, const Repo& source) const {
        return sources.contains(package) && sameIdentity(sources.at(package), source);
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
    const std::map<std::string, Wave>& Engine::waves() const {
        return waveStates;
    }
    std::optional<std::int64_t> Engine::time() const {
        return engineTime;
    }
} // namespace coreci::legacy
