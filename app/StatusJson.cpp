#include "StatusJson.h"

#include "core/Status.h"

#include <charconv>
namespace coreci::app {
    namespace {
        std::string quote(const std::string& value) {
            const char* hex = "0123456789abcdef";
            std::string result = "\"";
            for (const unsigned char c : value) {
                if (c == '"' || c == '\\') {
                    result += '\\';
                    result += static_cast<char>(c);
                } else if (c < 32) {
                    result += "\\u00";
                    result += hex[c >> 4];
                    result += hex[c & 15];
                } else {
                    result += static_cast<char>(c);
                }
            }
            result += '"';
            return result;
        }
        std::string object(std::map<std::string, std::string> fields) {
            std::string result = "{";
            for (const auto& [key, value] : fields) {
                result += (result.size() == 1 ? "" : ",") + quote(key) + ":" + value;
            }
            return result + "}";
        }
        std::string array(const std::vector<std::string>& values) {
            std::string result = "[";
            for (const auto& value : values) {
                result += (result.size() == 1 ? "" : ",") + value;
            }
            return result + "]";
        }
        std::string run(const std::optional<std::uint64_t>& value) {
            return value ? std::to_string(*value) : "null";
        }
    } // namespace
    StatusSnapshot statusSnapshot(const Engine& engine) {
        StatusSnapshot result;
        for (const auto& [name, package] : engine.pipeline().packages()) {
            result.packages.emplace(name, PackageView{package.sourceRepo, engine.releases().at(name)});
        }
        for (const auto& [unit, status] : statuses(engine)) {
            const auto& state = engine.units().at(unit);
            UnitView view;
            view.status = status;
            view.published = state.published;
            view.failure = state.failure;
            if (state.published) {
                view.publishedRun = engine.waves().at(state.published->waveId).runId;
            }
            if (const auto* entry = engine.selection(unit)) {
                view.selection = VersionView{entry->release.version.text(), entry->revision};
                view.buildId = entry->buildId;
            } else if (state.published) {
                view.selection = VersionView{state.published->version.text(), state.published->revision};
            }
            result.units.emplace(unit, std::move(view));
        }
        if (engine.latestWave()) {
            result.wave = *engine.latestWave();
            for (const auto& [target, entries] : result.wave->plan) {
                for (const auto& entry : entries) {
                    ++result.waveCounts[entry.build ? stateName(engine.units().at(entry.unit).state) : "Published"];
                }
            }
        }
        return result;
    }
    std::string statusJson(const StatusSnapshot& snapshot, std::uint64_t generation) {
        const auto version = [](const std::string& text, std::uint64_t revision) {
            return object({{"version", quote(text)}, {"revision", std::to_string(revision)}});
        };
        const auto timestamp = [](std::optional<std::int64_t> value) {
            return value ? std::to_string(*value) : "null";
        };
        std::vector<std::string> packages, units;
        for (const auto& [name, package] : snapshot.packages) {
            const auto& r = package.released;
            packages.push_back(object(
                {{"name", quote(name)},
                 {"released",
                  r ? object({{"tag", quote(r->tag)}, {"version", quote(r->version.text())}, {"commit", quote(r->sourceSha)}}) : "null"},
                 {"sourceRepo",
                  object({{"branch", quote(package.sourceRepo.branch)},
                          {"owner", quote(package.sourceRepo.owner)},
                          {"repo", quote(package.sourceRepo.repo)}})}}));
        }
        for (const auto& [unit, state] : snapshot.units) {
            std::string publication = "null", failure = "null";
            if (state.published) {
                const auto& p = *state.published;
                publication = object({{"version", quote(p.version.text())},
                                      {"revision", std::to_string(p.revision)},
                                      {"at", std::to_string(p.at)},
                                      {"feedCommit", quote(p.feedCommit)},
                                      {"runId", run(state.publishedRun)}});
            }
            if (state.failure) {
                const auto& f = *state.failure;
                failure = object({{"version", quote(f.version.text())},
                                  {"revision", std::to_string(f.revision)},
                                  {"at", std::to_string(f.at)},
                                  {"reason", quote(f.reason)}});
            }
            units.push_back(
                object({{"package", quote(unit.package)},
                        {"target", quote(unit.target)},
                        {"status", quote(state.status)},
                        {"buildId", quote(state.buildId)},
                        {"published", publication},
                        {"failure", failure},
                        {"selection", state.selection ? version(state.selection->version, state.selection->revision) : "null"}}));
        }
        std::string wave = "null";
        if (snapshot.wave) {
            const auto& w = *snapshot.wave;
            std::map<std::string, std::string> encodedCounts;
            for (const auto& [state, count] : snapshot.waveCounts) {
                encodedCounts.emplace(state, std::to_string(count));
            }
            wave = object({{"id", quote(w.id)},
                           {"phase", quote(phaseName(w.phase))},
                           {"runId", run(w.runId)},
                           {"runAttempt", w.runAttempt ? std::to_string(*w.runAttempt) : "null"},
                           {"started", timestamp(w.started)},
                           {"finished", timestamp(w.finished)},
                           {"conclusion", quote(w.conclusion)},
                           {"counts", object(encodedCounts)}});
        }
        return object({{"schema", "2"},
                       {"generation", std::to_string(generation)},
                       {"packages", array(packages)},
                       {"units", array(units)},
                       {"wave", wave}});
    }
    std::string planJson(const Wave& wave) {
        std::vector<std::string> targets;
        for (const auto& [target, entries] : wave.plan) {
            std::vector<std::string> units;
            for (const auto& entry : entries) {
                units.push_back(object({{"package", quote(entry.unit.package)},
                                        {"repo", quote(entry.repo.owner + "/" + entry.repo.repo)},
                                        {"tag", quote(entry.release.tag)},
                                        {"commit", quote(entry.release.sourceSha)},
                                        {"version", quote(entry.release.version.text())},
                                        {"revision", std::to_string(entry.revision)},
                                        {"build_id", quote(entry.buildId)},
                                        {"action", quote(entry.build ? "build" : "reuse")}}));
            }
            targets.push_back(object({{"id", quote(target)}, {"units", array(units)}}));
        }
        return object({{"wave", quote(wave.id)},
                       {"feed_branch", quote(wave.feedBranch)},
                       {"owner_run_id", run(wave.runId)},
                       {"targets", array(targets)}});
    }
} // namespace coreci::app
