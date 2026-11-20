#include "PublicationController.h"

#include <algorithm>

namespace coreci {
    bool PublicationController::classify(const WaveController& s, const Event& event) {
        const bool result = std::visit(
            [&](const auto& e) {
                using E = std::decay_t<decltype(e)>;
                bool allowed = false;
                if constexpr (requires { e.publicationId; }) {
                    const auto found = s.publicationStates.find(e.publicationId);
                    if (found != s.publicationStates.end()) {
                        const auto& publication = found->second;
                        const auto& run = publication.run;
                        if constexpr (std::is_same_v<E, PublicationStarted>) {
                            allowed = !run.runId && e.runId > 0 && e.runAttempt > 0;
                        } else if constexpr (std::is_same_v<E, PublicationFinished>) {
                            allowed = run.runId == e.runId && !run.runFinished;
                        } else if constexpr (std::is_same_v<E, PublicationDispatchAccepted> ||
                                             std::is_same_v<E, PublicationDispatchRejected>) {
                            allowed = run.phase == WavePhase::Dispatching && !run.runId && !run.dispatchAccepted && !run.retryAt &&
                                      e.attempt == run.attempt;
                        } else {
                            for (const auto& item : publication.items) {
                                if (std::find(item.buildIds.begin(), item.buildIds.end(), e.buildId) != item.buildIds.end()) {
                                    for (const auto& entry : s.waveStates.at(item.waveId).plan.at(item.target)) {
                                        if (entry.buildId == e.buildId) {
                                            const auto& unit = s.unitStates.at(entry.unit);
                                            const auto& target = s.waveTargets.at({item.waveId, item.target});
                                            allowed = run.runId && !publication.results.contains(e.buildId) &&
                                                      (!unit.published || std::stoull(unit.published->waveId.substr(1)) <
                                                                              std::stoull(item.waveId.substr(1))) &&
                                                      (std::is_same_v<E, UnitPublishSucceeded> ||
                                                       (target.publicationId == run.id && unit.waveId == item.waveId &&
                                                        unit.state == State::Publishing));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                return allowed;
            },
            event);
        return result;
    }
    void PublicationController::finish(WaveController& s, Publication& publication, const Options& options, const std::string& reason) {
        auto& run = publication.run;
        run.phase = WavePhase::Finished;
        run.finished = *s.engineTime;
        run.retryAt.reset();
        for (const auto& item : publication.items) {
            auto& target = s.waveTargets.at({item.waveId, item.target});
            for (const auto& entry : s.waveStates.at(item.waveId).plan.at(item.target)) {
                if (std::find(item.buildIds.begin(), item.buildIds.end(), entry.buildId) != item.buildIds.end()) {
                    auto& unit = s.unitStates.at(entry.unit);
                    if (target.publicationId == run.id && unit.waveId == item.waveId && unit.state == State::Publishing) {
                        s.fail(entry, reason, false, options.retryDelay > 0);
                        unit.publishEnd = *s.engineTime;
                    }
                }
            }
            target.status = State::Failed;
        }
    }
    void PublicationController::apply(WaveController& s, const Options& options, const Event& event) {
        std::visit(
            [&](const auto& e) {
                using E = std::decay_t<decltype(e)>;
                if constexpr (requires { e.publicationId; }) {
                    auto& publication = s.publicationStates.at(e.publicationId);
                    auto& run = publication.run;
                    if constexpr (std::is_same_v<E, PublicationStarted>) {
                        run.runId = e.runId;
                        run.runAttempt = e.runAttempt;
                        run.started = *s.engineTime;
                        if (!run.finished) {
                            run.phase = WavePhase::Running;
                        }
                    } else if constexpr (std::is_same_v<E, PublicationFinished>) {
                        run.runFinished = true;
                        run.conclusion = e.conclusion;
                        finish(s, publication, options, "no publication result");
                    } else if constexpr (std::is_same_v<E, PublicationDispatchAccepted>) {
                        run.dispatchAccepted = true;
                    } else if constexpr (std::is_same_v<E, PublicationDispatchRejected>) {
                        const bool permanent = e.httpStatus == 401 || e.httpStatus == 403 || e.httpStatus == 404 || e.httpStatus == 422;
                        if (permanent || run.attempt >= options.maxDispatchAttempts) {
                            run.conclusion = "dispatch rejected";
                            finish(s, publication, options, "publication dispatch rejected");
                            if (permanent) {
                                for (const auto& item : publication.items) {
                                    for (const auto& entry : s.waveStates.at(item.waveId).plan.at(item.target)) {
                                        if (s.unitStates.at(entry.unit).state == State::Failed) {
                                            s.unitStates.at(entry.unit).retryAfter.reset();
                                        }
                                    }
                                }
                            }
                        } else {
                            run.retryAt = *s.engineTime + std::min<std::int64_t>(30LL << std::min(run.attempt - 1, 6U), 1800);
                        }
                    } else {
                        publication.results[e.buildId] = std::is_same_v<E, UnitPublishSucceeded>;
                        for (const auto& item : publication.items) {
                            for (const auto& entry : s.waveStates.at(item.waveId).plan.at(item.target)) {
                                if (entry.buildId == e.buildId) {
                                    auto& unit = s.unitStates.at(entry.unit);
                                    if constexpr (std::is_same_v<E, UnitPublishSucceeded>) {
                                        const bool newer = !unit.published || std::stoull(item.waveId.substr(1)) >
                                                                                  std::stoull(unit.published->waveId.substr(1));
                                        if (newer) {
                                            unit.published = Published{entry.release.version,
                                                                       entry.revision,
                                                                       entry.inputs,
                                                                       entry.repo,
                                                                       item.waveId,
                                                                       *s.engineTime,
                                                                       e.feedCommit,
                                                                       run.runId};
                                        }
                                        if (unit.waveId == item.waveId && unit.lastRevision.at(entry.release.version) == entry.revision) {
                                            unit.state = State::Published;
                                            unit.failure.reset();
                                            unit.retryAfter.reset();
                                            unit.publishEnd = *s.engineTime;
                                        }
                                    } else {
                                        if (unit.waveId == item.waveId) {
                                            s.fail(entry, e.reason);
                                            unit.publishEnd = *s.engineTime;
                                        }
                                        if (e.failure == "artifact-unavailable") {
                                            s.waveTargets.at({item.waveId, item.target}).artifactAvailable = false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            },
            event);
    }
    bool PublicationController::retry(WaveController& s, const Unit& key) {
        auto& unit = s.unitStates.at(key);
        const auto target = s.waveTargets.find({unit.waveId, key.target});
        const bool reusable = unit.failure && !unit.failure->buildFailed && target != s.waveTargets.end() && target->second.finished &&
                              target->second.artifactAvailable && !target->second.artifact.empty();
        if (reusable) {
            unit.state = State::Built;
            unit.retryAfter.reset();
            unit.failure.reset();
            target->second.publicationId.clear();
            target->second.status = State::Built;
        }
        return reusable;
    }
    void PublicationController::schedule(WaveController& s, const Options& options) {
        for (const auto& feed : {"feed", "feed-staging"}) {
            bool active = false;
            for (const auto& [id, publication] : s.publicationStates) {
                active = active || (publication.run.feedBranch == feed && publication.run.phase != WavePhase::Finished);
            }
            if (!active) {
                Publication publication;
                auto& run = publication.run;
                run.id = "p" + std::to_string(s.sequence) + (feed == std::string("feed") ? "" : "-staging");
                run.feedBranch = feed;
                run.workflow = options.workflow;
                run.dispatchedAt = *s.engineTime;
                run.deadline = *s.engineTime + options.publicationDeadline;
                run.startDeadline = *s.engineTime + options.waveStartTimeout;
                for (auto& [key, target] : s.waveTargets) {
                    const auto& wave = s.waveStates.at(key.first);
                    if (wave.feedBranch == run.feedBranch && wave.runId && target.finished && target.artifactAvailable &&
                        target.publicationId.empty() && !target.artifact.empty()) {
                        PublicationItem item{key.first, key.second, target.artifact, {}};
                        for (const auto& entry : wave.plan.at(key.second)) {
                            auto& unit = s.unitStates.at(entry.unit);
                            const bool unpublished =
                                !unit.published || std::stoull(unit.published->waveId.substr(1)) < std::stoull(wave.id.substr(1));
                            if (entry.build && target.builds.contains(entry.buildId) && target.builds.at(entry.buildId) && unpublished) {
                                item.buildIds.push_back(entry.buildId);
                                if (unit.waveId == wave.id) {
                                    unit.state = State::Publishing;
                                }
                            }
                        }
                        if (!item.buildIds.empty()) {
                            std::sort(item.buildIds.begin(), item.buildIds.end());
                            target.publicationId = run.id;
                            target.status = State::Publishing;
                            publication.items.push_back(std::move(item));
                        }
                    }
                }
                if (!publication.items.empty()) {
                    s.publicationStates.emplace(run.id, std::move(publication));
                }
            }
        }
    }
    void PublicationController::advance(WaveController& s, const Options& options) {
        for (auto& [id, publication] : s.publicationStates) {
            auto& run = publication.run;
            if (run.phase != WavePhase::Finished) {
                if (*s.engineTime >= run.deadline) {
                    run.expired = true;
                    run.conclusion = "timeout";
                    finish(s, publication, options, "publication timeout");
                } else if (run.phase == WavePhase::Dispatching &&
                           ((run.retryAt && *s.engineTime >= *run.retryAt) || (!run.retryAt && *s.engineTime >= run.startDeadline))) {
                    ++run.attempt;
                    run.dispatchAccepted = false;
                    run.retryAt.reset();
                    run.dispatchedAt = *s.engineTime;
                    run.startDeadline = *s.engineTime + options.waveStartTimeout;
                }
            }
        }
    }
} // namespace coreci
