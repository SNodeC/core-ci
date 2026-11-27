#include "WaveController.h"

#include "PublicationController.h"

#include <algorithm>
#include <type_traits>

namespace coreci {
    std::string stateName(State state) {
        const char* names[] = {"Idle", "Planned", "Building", "Built", "Published", "Failed", "Blocked", "Publishing"};
        return names[static_cast<unsigned>(state)];
    }
    std::string phaseName(WavePhase phase) {
        const char* names[] = {"Dispatching", "Running", "Finished"};
        return names[static_cast<unsigned>(phase)];
    }
    WaveController::WaveController(Options options, StoredState state)
        : StoredState(std::move(state))
        , options(std::move(options)) {
    }
    bool WaveController::newerTag(const TagObserved& tag, const Pipeline& candidate) const {
        const auto version = Version::parse(tag.tag);
        const auto old = released.find(tag.package);
        return candidate.packages().contains(tag.package) && version && !version->prerelease() &&
               (old == released.end() || !old->second || !sameSource(tag.package, candidate.packages().at(tag.package).sourceRepo) ||
                *version > old->second->version);
    }
    void WaveController::dirty() {
        quietUntil = *engineTime + options.waveDelay;
    }
    void WaveController::changeGraph(const GraphChanged& event) {
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
            if (!graph.packages().contains(name) || graph.packages().at(name).prerequisites != pkg.prerequisites ||
                graph.packages().at(name).ciTree != pkg.ciTree) {
                changed = true;
                for (const auto& target : pkg.targets) {
                    unitStates.at({name, target}).autoRetried.reset();
                }
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
    const Wave* WaveController::activeWave() const {
        const auto* wave = latestWave();
        return wave && wave->phase != WavePhase::Finished ? wave : nullptr;
    }
    const Wave* WaveController::latestWave() const {
        return latest.empty() ? nullptr : &waveStates.at(latest);
    }
    const PlanUnit* WaveController::selection(const Unit& unit, const std::string& waveId) const {
        const PlanUnit* result = nullptr;
        const auto state = unitStates.find(unit);
        const auto id = waveId.empty() && state != unitStates.end() ? state->second.waveId : waveId;
        if (waveStates.contains(id)) {
            const auto& plan = waveStates.at(id).plan;
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
    Classification WaveController::classify(const Event& event) const {
        const bool allowed = std::visit(
            [&](const auto& e) {
                using E = std::decay_t<decltype(e)>;
                bool accept = false;
                if constexpr (requires { e.publicationId; }) {
                    accept = PublicationController::classify(*this, event);
                } else if constexpr (std::is_same_v<E, TagObserved>) {
                    accept = newerTag(e, graph);
                } else if constexpr (std::is_same_v<E, GraphChanged>) {
                    accept = graph != e.pipeline;
                    for (const auto& tag : e.tags) {
                        accept = accept || newerTag(tag, e.pipeline);
                    }
                } else if constexpr (std::is_same_v<E, DispatchPaused>) {
                    accept = dispatchPaused != e.paused;
                } else if constexpr (std::is_same_v<E, Tick>) {
                    const auto next = nextDeadline();
                    accept = next && std::max(engineTime.value_or(e.at), e.at) >= *next;
                } else if constexpr (std::is_same_v<E, RetryRequested> || std::is_same_v<E, RebuildRequested>) {
                    const auto state = unitStates.find({e.package, e.target});
                    accept = graph.packages().contains(e.package) && graph.packages().at(e.package).targets.contains(e.target) &&
                             state != unitStates.end() && !state->second.requested &&
                             state->second.state == (std::is_same_v<E, RetryRequested> ? State::Failed : State::Published);
                } else {
                    const auto found = waveStates.find(e.waveId);
                    if (found != waveStates.end()) {
                        const auto& wave = found->second;
                        if constexpr (std::is_same_v<E, WaveDispatchAccepted> || std::is_same_v<E, WaveDispatchRejected>) {
                            accept = wave.phase == WavePhase::Dispatching && !wave.dispatchAccepted && !wave.retryAt &&
                                     e.attempt == wave.attempt && !wave.runId;
                        } else if constexpr (std::is_same_v<E, WaveStarted>) {
                            accept = !wave.runId && e.runId > 0 && e.runAttempt > 0;
                        } else if constexpr (std::is_same_v<E, WaveFinished>) {
                            accept = wave.runId == e.runId && !wave.runFinished;
                        } else if constexpr (std::is_same_v<E, WaveExpired>) {
                            accept = !wave.expired && std::max(engineTime.value_or(e.at), e.at) >= wave.deadline;
                        } else if constexpr (std::is_same_v<E, TargetFinished>) {
                            const auto target = waveTargets.find({wave.id, e.target});
                            accept = wave.runId && target != waveTargets.end() && !target->second.finished;
                        } else {
                            for (const auto& [target, entries] : wave.plan) {
                                for (const auto& entry : entries) {
                                    if (entry.build && entry.buildId == e.buildId) {
                                        const auto& state = unitStates.at(entry.unit);
                                        const auto& result = waveTargets.at({wave.id, target});
                                        const bool pending =
                                            !result.builds.contains(entry.buildId) && (state.waveId != wave.id || !state.buildEnd);
                                        accept = wave.runId && pending &&
                                                 (!std::is_same_v<E, UnitBuildStarted> || (state.waveId == wave.id && !state.buildStart));
                                    }
                                }
                            }
                        }
                    }
                }
                return accept;
            },
            event);
        return allowed ? Classification{Accept{}} : Classification{Ignore{"unknown, duplicate or stale input"}};
    }
    std::uint64_t WaveController::nextRevision(const Unit& unit) const {
        const auto& version = released.at(unit.package)->version;
        const auto& previous = unitStates.at(unit).lastRevision;
        return std::max<std::uint64_t>(previous.contains(version) ? previous.at(version) : 0,
                                       graph.packages().at(unit.package).revisionBase) +
               1;
    }
    Plan WaveController::plan() {
        Plan result;
        for (const auto& target : graph.targets()) {
            std::map<std::string, PlanUnit> selected;
            std::vector<PlanUnit> entries;
            std::set<std::string> blockedPackages;
            bool builds = false;
            for (const auto& name : graph.topologicalOrder(target)) {
                const auto& pkg = graph.packages().at(name);
                const auto& release = released.at(name);
                const Unit unit{name, target};
                const auto& state = unitStates.at(unit);
                bool available = release.has_value(), cascade = false;
                Inputs inputs;
                std::string blocked;
                for (const auto& [prerequisite, range] : pkg.prerequisites) {
                    const auto q = selected.find(prerequisite);
                    available = available && q != selected.end();
                    if (q != selected.end()) {
                        inputs.emplace(prerequisite, Input{q->second.release.version, q->second.revision});
                        cascade = cascade || q->second.build;
                        if (!q->second.release.version.satisfies(range)) {
                            blocked = "dependency " + prerequisite + " " + q->second.release.version.text() + " outside " + range;
                        } else if (blockedPackages.contains(prerequisite) && blocked.empty()) {
                            blocked = "dependency " + prerequisite + " blocked";
                        }
                    }
                }
                if (available) {
                    const auto* published = state.published ? selection(unit, state.published->waveId) : nullptr;
                    const auto* previous = selection(unit);
                    const bool build = state.requested || cascade || !state.published || !published || published->ciTree != pkg.ciTree ||
                                       !sameIdentity(state.published->source, pkg.sourceRepo) ||
                                       state.published->version != release->version || state.published->inputs != inputs;
                    const bool failed = state.failure && previous && previous->ciTree == pkg.ciTree &&
                                        state.failure->version == release->version && state.failure->inputs == inputs;
                    if (!blocked.empty()) {
                        auto& blockedState = unitStates.at(unit);
                        blockedState.state = State::Blocked;
                        blockedState.waveId.clear();
                        blockedState.failure = Failure{release->version, inputs, blocked, 0, *engineTime};
                        blockedState.retryAfter.reset();
                        blockedState.requested = false;
                        blockedPackages.insert(name);
                    }
                    if (!blocked.empty() || !build || !failed || state.requested || cascade) {
                        PlanUnit entry{unit,
                                       pkg.sourceRepo,
                                       *release,
                                       blocked.empty() ? (build ? nextRevision(unit) : state.published->revision) : 0,
                                       inputs,
                                       {},
                                       build && blocked.empty(),
                                       pkg.ciCommit,
                                       pkg.ciTree};
                        selected.emplace(name, entry);
                        if (blocked.empty()) {
                            entries.push_back(std::move(entry));
                            builds = builds || build;
                        }
                    }
                }
            }
            if (builds) {
                result.emplace(target, std::move(entries));
            }
        }
        return result;
    }
    void WaveController::request(const Unit& unit) {
        unitStates.at(unit).requested = true;
        unitStates.at(unit).retryAfter.reset();
        if (graph.packages().contains(unit.package) && graph.packages().at(unit.package).targets.contains(unit.target)) {
            std::set<std::string> requested{unit.package};
            for (const auto& name : graph.topologicalOrder(unit.target)) {
                auto& state = unitStates.at({name, unit.target});
                const auto& prerequisites = graph.prerequisites(name);
                if (state.state == State::Blocked && std::any_of(prerequisites.begin(), prerequisites.end(), [&](const auto& q) {
                        return requested.contains(q.first);
                    })) {
                    state.requested = true;
                    requested.insert(name);
                }
            }
        }
        dirty();
    }
    void WaveController::fail(const PlanUnit& entry, const std::string& reason, bool buildFailed, bool automatic) {
        auto& state = unitStates.at(entry.unit);
        state.state = State::Failed;
        state.failure = Failure{entry.release.version, entry.inputs, reason, entry.revision, *engineTime, buildFailed};
        state.retryAfter.reset();
        if (automatic && options.retryDelay > 0 && state.autoRetried != std::pair{entry.release.version, entry.inputs}) {
            state.retryAfter = *engineTime + options.retryDelay;
        }
    }
    void WaveController::finishTargets(Wave& wave) {
        for (const auto& [target, entries] : wave.plan) {
            auto& result = waveTargets.at({wave.id, target});
            if (result.finished || wave.runFinished || wave.expired) {
                std::set<std::string> blocked;
                bool built = false;
                for (const auto& entry : entries) {
                    if (entry.build && unitStates.at(entry.unit).waveId == wave.id) {
                        auto& state = unitStates.at(entry.unit);
                        const bool prerequisiteFailed = std::any_of(entry.inputs.begin(), entry.inputs.end(), [&](const auto& q) {
                            return blocked.contains(q.first);
                        });
                        if (state.state == State::Planned && prerequisiteFailed) {
                            state.state = State::Blocked;
                            state.retryAfter.reset();
                        } else if (state.state == State::Planned || state.state == State::Building ||
                                   (state.state == State::Built && result.artifact.empty())) {
                            fail(entry, "no result", true);
                        }
                        built = built || state.state == State::Built || state.state == State::Publishing || state.state == State::Published;
                        if (state.state == State::Blocked || (state.failure && state.failure->buildFailed)) {
                            blocked.insert(entry.unit.package);
                        }
                    }
                }
                if (!built) {
                    result.status = State::Failed;
                }
            }
        }
    }
    void WaveController::completeWaves() {
        for (auto& [id, wave] : waveStates) {
            bool complete = wave.runFinished || wave.expired;
            for (const auto& [target, entries] : wave.plan) {
                auto& result = waveTargets.at({id, target});
                bool pending = false, published = true;
                for (const auto& entry : entries) {
                    if (entry.build) {
                        const auto& state = unitStates.at(entry.unit);
                        pending = pending || (state.waveId == id && (state.state == State::Planned || state.state == State::Building ||
                                                                     state.state == State::Built || state.state == State::Publishing));
                        published = published && ((state.published && state.published->waveId == id) ||
                                                  std::any_of(publicationStates.begin(), publicationStates.end(), [&](const auto& p) {
                                                      const auto outcome = p.second.results.find(entry.buildId);
                                                      return outcome != p.second.results.end() && outcome->second;
                                                  }));
                    }
                }
                if (!pending && (published || result.status != State::Blocked)) {
                    result.status = published || result.status == State::Published ? State::Published : State::Failed;
                }
                complete = complete && !pending;
            }
            if (complete && wave.phase != WavePhase::Finished) {
                wave.phase = WavePhase::Finished;
                wave.finished = *engineTime;
                wave.retryAt.reset();
            }
        }
    }
    void WaveController::advance() {
        if (const auto* active = activeWave()) {
            auto& wave = waveStates.at(active->id);
            if (!dispatchPaused && wave.deadline == 0) {
                wave.dispatchedAt = *engineTime;
                wave.deadline = *engineTime + options.waveDeadline;
                wave.startDeadline = *engineTime + options.waveStartTimeout;
            }
            if (wave.deadline != 0 && !wave.expired && *engineTime >= wave.deadline) {
                wave.expired = true;
                wave.conclusion = "timeout";
                finishTargets(wave);
            } else if (!dispatchPaused && !wave.expired && wave.phase == WavePhase::Dispatching &&
                       ((wave.retryAt && *engineTime >= *wave.retryAt) || (!wave.retryAt && *engineTime >= wave.startDeadline))) {
                ++wave.attempt;
                wave.dispatchAccepted = false;
                wave.retryAt.reset();
                wave.dispatchedAt = *engineTime;
                wave.startDeadline = *engineTime + options.waveStartTimeout;
            }
        }
        PublicationController::advance(*this, options);
        completeWaves();
        for (const auto& [name, pkg] : graph.packages()) {
            for (const auto& target : pkg.targets) {
                const Unit unit{name, target};
                auto& state = unitStates.at(unit);
                if (state.failure && state.retryAfter && *engineTime >= *state.retryAfter) {
                    state.autoRetried = {state.failure->version, state.failure->inputs};
                    if (!PublicationController::retry(*this, unit)) {
                        request(unit);
                    }
                }
            }
        }
        PublicationController::schedule(*this, options);
        if (!activeWave() && quietUntil && *engineTime >= *quietUntil) {
            auto planned = plan();
            quietUntil.reset();
            if (!planned.empty()) {
                Wave wave;
                wave.id = "w" + std::to_string(sequence);
                wave.plan = std::move(planned);
                wave.feedBranch = options.feedBranch;
                wave.workflow = options.workflow;
                if (!dispatchPaused) {
                    wave.dispatchedAt = *engineTime;
                    wave.deadline = *engineTime + options.waveDeadline;
                    wave.startDeadline = *engineTime + options.waveStartTimeout;
                }
                for (auto& [target, entries] : wave.plan) {
                    waveTargets.emplace(std::pair{wave.id, target}, WaveTarget{});
                    for (auto& entry : entries) {
                        if (entry.build) {
                            entry.buildId = wave.id + "~" + entry.unit.package + "~" + target;
                            auto& state = unitStates.at(entry.unit);
                            state.state = State::Planned;
                            state.waveId = wave.id;
                            state.order = sequence;
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
    void WaveController::handle(const Event& event, std::uint64_t seq) {
        if (std::holds_alternative<Accept>(classify(event))) {
            sequence = std::max(sequence + 1, seq);
            std::visit(
                [&](const auto& e) {
                    using E = std::decay_t<decltype(e)>;
                    engineTime = std::max(engineTime.value_or(e.at), e.at);
                    if constexpr (requires { e.publicationId; }) {
                        PublicationController::apply(*this, options, event);
                    } else if constexpr (std::is_same_v<E, DispatchPaused>) {
                        dispatchPaused = e.paused;
                    } else if constexpr (std::is_same_v<E, GraphChanged>) {
                        changeGraph(e);
                    } else if constexpr (std::is_same_v<E, TagObserved>) {
                        released[e.package] = Released{*Version::parse(e.tag), e.tag, e.sourceSha};
                        dirty();
                    } else if constexpr (std::is_same_v<E, RetryRequested> || std::is_same_v<E, RebuildRequested>) {
                        const Unit unit{e.package, e.target};
                        if (std::is_same_v<E, RebuildRequested> || !PublicationController::retry(*this, unit)) {
                            request(unit);
                        }
                    } else if constexpr (!std::is_same_v<E, Tick>) {
                        auto& wave = waveStates.at(e.waveId);
                        if constexpr (std::is_same_v<E, WaveDispatchAccepted>) {
                            wave.dispatchAccepted = true;
                        } else if constexpr (std::is_same_v<E, WaveDispatchRejected>) {
                            const bool permanent = e.httpStatus == 401 || e.httpStatus == 403 || e.httpStatus == 404 || e.httpStatus == 422;
                            if (permanent || wave.attempt >= options.maxDispatchAttempts) {
                                for (const auto& [target, entries] : wave.plan) {
                                    for (const auto& entry : entries) {
                                        if (entry.build) {
                                            fail(entry,
                                                 "dispatch rejected (" + std::to_string(e.httpStatus) + "): " + e.message,
                                                 true,
                                                 !permanent);
                                        }
                                    }
                                }
                                wave.runFinished = true;
                                wave.conclusion = "dispatch rejected";
                            } else {
                                wave.retryAt = *engineTime + std::min<std::int64_t>(30LL << std::min(wave.attempt - 1, 6U), 1800);
                            }
                        } else if constexpr (std::is_same_v<E, WaveStarted>) {
                            wave.runId = e.runId;
                            wave.runAttempt = e.runAttempt;
                            wave.started = *engineTime;
                            if (!wave.finished) {
                                wave.phase = WavePhase::Running;
                            }
                        } else if constexpr (std::is_same_v<E, WaveFinished>) {
                            wave.runFinished = true;
                            wave.conclusion = e.conclusion;
                            finishTargets(wave);
                        } else if constexpr (std::is_same_v<E, WaveExpired>) {
                            wave.expired = true;
                            wave.conclusion = "timeout";
                            finishTargets(wave);
                        } else if constexpr (std::is_same_v<E, TargetFinished>) {
                            auto& target = waveTargets.at({wave.id, e.target});
                            target.finished = true;
                            target.artifact = e.artifact;
                            target.status = e.artifact.empty() ? State::Failed : State::Built;
                            if (!e.artifact.empty() && target.publicationId.empty()) {
                                for (const auto& entry : wave.plan.at(e.target)) {
                                    auto& state = unitStates.at(entry.unit);
                                    if (state.waveId == wave.id && target.builds.contains(entry.buildId) &&
                                        target.builds.at(entry.buildId)) {
                                        state.state = State::Built;
                                        state.failure.reset();
                                        state.retryAfter.reset();
                                    }
                                }
                            }
                            finishTargets(wave);
                        } else {
                            for (const auto& [target, entries] : wave.plan) {
                                for (const auto& entry : entries) {
                                    if (entry.build && entry.buildId == e.buildId) {
                                        auto& state = unitStates.at(entry.unit);
                                        auto& result = waveTargets.at({wave.id, target});
                                        if constexpr (std::is_same_v<E, UnitBuildSucceeded>) {
                                            result.builds[entry.buildId] = true;
                                            if (!result.publicationId.empty() &&
                                                publicationStates.at(result.publicationId).run.phase == WavePhase::Finished) {
                                                result.publicationId.clear();
                                            }
                                        }
                                        if constexpr (std::is_same_v<E, UnitBuildFailed>) {
                                            result.builds[entry.buildId] = false;
                                        }
                                        if (state.waveId == wave.id) {
                                            state.failure.reset();
                                            state.retryAfter.reset();
                                            if constexpr (std::is_same_v<E, UnitBuildStarted>) {
                                                state.state = State::Building;
                                                state.buildStart = *engineTime;
                                            } else if constexpr (std::is_same_v<E, UnitBuildSucceeded>) {
                                                state.state = State::Built;
                                                state.buildEnd = *engineTime;
                                            } else if constexpr (std::is_same_v<E, UnitBuildFailed>) {
                                                state.buildEnd = *engineTime;
                                                fail(entry, e.reason, true);
                                            }
                                        }
                                        if (result.finished) {
                                            finishTargets(wave);
                                        }
                                    }
                                }
                            }
                        }
                    }
                },
                event);
            advance();
        }
    }
    std::vector<Command> WaveController::commands() const {
        std::vector<Command> result;
        const auto* wave = activeWave();
        if (!dispatchPaused && wave && !wave->expired && wave->phase == WavePhase::Dispatching && !wave->dispatchAccepted &&
            !wave->retryAt) {
            result.push_back({wave->id, false, wave->attempt, wave->workflow, wave->feedBranch});
        }
        for (const auto& [id, publication] : publicationStates) {
            const auto& run = publication.run;
            if (!dispatchPaused && run.phase == WavePhase::Dispatching && !run.dispatchAccepted && !run.retryAt && !run.expired) {
                result.push_back({id, true, run.attempt, run.workflow, run.feedBranch});
            }
        }
        return result;
    }
    std::optional<std::int64_t> WaveController::nextDeadline() const {
        std::optional<std::int64_t> result;
        const auto add = [&](std::optional<std::int64_t> value) {
            if (value && (!result || *value < *result)) {
                result = value;
            }
        };
        const auto runDeadline = [&](const Wave& run) {
            if (run.deadline != 0 && !run.expired && run.phase != WavePhase::Finished) {
                add(run.deadline);
                add(dispatchPaused ? std::nullopt : run.retryAt);
                if (!dispatchPaused && run.phase == WavePhase::Dispatching && !run.retryAt) {
                    add(run.startDeadline);
                }
            }
        };
        if (const auto* wave = activeWave()) {
            runDeadline(*wave);
        } else {
            add(quietUntil);
        }
        for (const auto& [id, publication] : publicationStates) {
            runDeadline(publication.run);
        }
        for (const auto& [unit, state] : unitStates) {
            add(state.retryAfter);
        }
        return result;
    }
    const StoredState& WaveController::stored() const {
        return *this;
    }
    const std::map<std::string, Publication>& WaveController::publications() const {
        return publicationStates;
    }
    bool WaveController::sameSource(const std::string& package, const Repo& source) const {
        return sources.contains(package) && sameIdentity(sources.at(package), source);
    }
    const Pipeline& WaveController::pipeline() const {
        return graph;
    }
    const std::map<std::string, std::optional<Released>>& WaveController::releases() const {
        return released;
    }
    const std::map<Unit, UnitState>& WaveController::units() const {
        return unitStates;
    }
    const std::map<std::string, Wave>& WaveController::waves() const {
        return waveStates;
    }
    std::optional<std::int64_t> WaveController::time() const {
        return engineTime;
    }
} // namespace coreci
