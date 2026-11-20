#include "legacy/Engine.h"

namespace coreci::legacy {
    ::coreci::StoredState Engine::convert() const {
        ::coreci::StoredState state;
        state.graph = graph;
        state.sources = sources;
        state.latest = latest;
        state.engineTime = engineTime;
        state.quietUntil = quietUntil;
        const auto inputs = [](const Inputs& old) {
            ::coreci::Inputs result;
            for (const auto& [name, input] : old) {
                result.emplace(name, ::coreci::Input{input.version, input.revision});
            }
            return result;
        };
        for (const auto& [package, release] : released) {
            state.released[package] =
                release ? std::optional<::coreci::Released>{{release->version, release->tag, release->sourceSha}} : std::nullopt;
        }
        for (const auto& [key, unit] : unitStates) {
            ::coreci::UnitState converted;
            converted.state = static_cast<::coreci::State>(unit.state);
            converted.waveId = unit.waveId;
            converted.artifact = unit.artifact;
            converted.order = unit.order;
            converted.buildStart = unit.buildStart;
            converted.buildEnd = unit.buildEnd;
            converted.publishEnd = unit.publishEnd;
            converted.retryAfter = unit.retryAfter;
            converted.requested = unit.requested;
            converted.lastRevision = unit.lastRevision;
            if (unit.published) {
                const auto& p = *unit.published;
                converted.published = ::coreci::Published{
                    p.version, p.revision, inputs(p.inputs), p.source, p.waveId, p.at, p.feedCommit, waveStates.at(p.waveId).runId};
            }
            if (unit.failure) {
                const auto& f = *unit.failure;
                converted.failure = ::coreci::Failure{f.version, inputs(f.inputs), f.reason, f.revision, f.at, f.buildFailed};
            }
            if (unit.autoRetried) {
                converted.autoRetried = std::pair{unit.autoRetried->first, inputs(unit.autoRetried->second)};
            }
            state.sequence = std::max(state.sequence, unit.order);
            state.unitStates.emplace(key, std::move(converted));
        }
        for (const auto& [id, wave] : waveStates) {
            ::coreci::Wave converted;
            converted.id = id;
            converted.feedBranch = wave.feedBranch;
            converted.workflow = wave.workflow;
            converted.phase = static_cast<::coreci::WavePhase>(wave.phase);
            converted.runId = wave.runId;
            converted.runAttempt = wave.runAttempt;
            converted.attempt = wave.attempt;
            converted.retryAt = wave.retryAt;
            converted.dispatchedAt = wave.dispatchedAt;
            converted.deadline = wave.deadline;
            converted.startDeadline = wave.dispatchedAt + options.waveStartTimeout;
            converted.dispatchAccepted = wave.dispatchAccepted;
            converted.started = wave.started;
            converted.finished = wave.finished;
            converted.conclusion = wave.conclusion;
            converted.expired = wave.awaitingOwnerFinish;
            converted.runFinished = wave.finished.has_value() && !wave.awaitingOwnerFinish;
            for (const auto& [target, entries] : wave.plan) {
                ::coreci::WaveTarget result;
                result.artifactAvailable = false;
                result.status = wave.finished ? ::coreci::State::Published : ::coreci::State::Planned;
                if (outcomes.contains({id, target})) {
                    for (const auto& [buildId, outcome] : outcomes.at({id, target})) {
                        if (outcome.first) {
                            result.builds[buildId] = *outcome.first;
                        }
                        if (wave.finished && outcome.second != State::Published) {
                            result.status = outcome.second == State::Blocked && result.status != ::coreci::State::Failed
                                                ? ::coreci::State::Blocked
                                                : ::coreci::State::Failed;
                        }
                    }
                }
                result.finished = wave.finished.has_value();
                for (const auto& entry : entries) {
                    converted.plan[target].push_back({entry.unit,
                                                      entry.repo,
                                                      {entry.release.version, entry.release.tag, entry.release.sourceSha},
                                                      entry.revision,
                                                      inputs(entry.inputs),
                                                      entry.buildId,
                                                      entry.build});
                }
                state.waveTargets.emplace(std::pair{id, target}, std::move(result));
            }
            state.waveStates.emplace(id, std::move(converted));
        }
        return state;
    }
} // namespace coreci::legacy
