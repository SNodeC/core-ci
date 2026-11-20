#include "StatusJson.h"

#include <algorithm>

namespace coreci::app {
    namespace {
        using Json = nlohmann::json;
        template <class T>
        Json nullable(const std::optional<T>& value) {
            return value ? Json(*value) : Json();
        }
        Json publicationPlan(const Publication& publication, const std::map<std::string, Wave>& waves) {
            Json items = Json::array();
            for (const auto& item : publication.items) {
                const auto& wave = waves.at(item.waveId);
                Json units = Json::array();
                for (const auto& entry : wave.plan.at(item.target)) {
                    if (std::find(item.buildIds.begin(), item.buildIds.end(), entry.buildId) != item.buildIds.end()) {
                        units.push_back({{"build_id", entry.buildId},
                                         {"package", entry.unit.package},
                                         {"version", entry.release.version.text()},
                                         {"revision", entry.revision}});
                    }
                }
                items.push_back({{"wave", item.waveId},
                                 {"wave_run_id", nullable(wave.runId)},
                                 {"target", item.target},
                                 {"artifact", item.artifact},
                                 {"units", units}});
            }
            return {{"publication", publication.run.id},
                    {"feed_branch", publication.run.feedBranch},
                    {"owner_run_id", nullable(publication.run.runId)},
                    {"items", items}};
        }
    } // namespace
    nlohmann::json statusSnapshot(const WaveController& engine) {
        Json value{{"schema", 2},
                   {"packages", Json::array()},
                   {"units", Json::array()},
                   {"wave", nullptr},
                   {"publication", nullptr},
                   {"targets", Json::array()}};
        for (const auto& [name, package] : engine.pipeline().packages()) {
            const auto& repo = package.sourceRepo;
            const auto& release = engine.releases().at(name);
            value["packages"].push_back(
                {{"name", name},
                 {"sourceRepo", {{"owner", repo.owner}, {"repo", repo.repo}, {"branch", repo.branch}}},
                 {"released",
                  release ? Json{{"tag", release->tag}, {"version", release->version.text()}, {"commit", release->sourceSha}} : Json()}});
            for (const auto& target : package.targets) {
                const Unit key{name, target};
                const auto& state = engine.units().at(key);
                Json published, failure, selection;
                std::string build;
                if (state.published) {
                    const auto& p = *state.published;
                    published = {{"version", p.version.text()},
                                 {"revision", p.revision},
                                 {"at", p.at},
                                 {"feedCommit", p.feedCommit},
                                 {"runId", nullable(p.runId)}};
                    selection = {{"version", p.version.text()}, {"revision", p.revision}};
                }
                if (state.failure) {
                    const auto& f = *state.failure;
                    failure = {{"version", f.version.text()}, {"revision", f.revision}, {"at", f.at}, {"reason", f.reason}};
                }
                if (const auto* entry = engine.selection(key)) {
                    selection = {{"version", entry->release.version.text()}, {"revision", entry->revision}};
                    build = entry->buildId;
                }
                value["units"].push_back({{"package", name},
                                          {"target", target},
                                          {"status", stateName(state.state)},
                                          {"buildId", build},
                                          {"published", published},
                                          {"failure", failure},
                                          {"selection", selection}});
            }
        }
        if (const auto* wave = engine.latestWave()) {
            Json counts = Json::object();
            for (const auto& [target, entries] : wave->plan) {
                for (const auto& entry : entries) {
                    const auto status = entry.build ? stateName(engine.units().at(entry.unit).state) : "Published";
                    counts[status] = counts.value(status, 0U) + 1;
                }
                const auto& result = engine.stored().waveTargets.at({wave->id, target});
                value["targets"].push_back({{"wave", wave->id},
                                            {"target", target},
                                            {"status", stateName(result.status)},
                                            {"artifact", result.artifact},
                                            {"publication", result.publicationId}});
            }
            value["wave"] = {{"id", wave->id},
                             {"phase", phaseName(wave->phase)},
                             {"runId", nullable(wave->runId)},
                             {"runAttempt", nullable(wave->runAttempt)},
                             {"started", nullable(wave->started)},
                             {"finished", nullable(wave->finished)},
                             {"conclusion", wave->conclusion},
                             {"counts", counts}};
        }
        const Publication* latest = nullptr;
        for (const auto& [id, publication] : engine.publications()) {
            if (!latest || std::stoull(id.substr(1)) > std::stoull(latest->run.id.substr(1))) {
                latest = &publication;
            }
        }
        if (latest) {
            value["publication"] = publicationPlan(*latest, engine.waves());
            value["publication"].update({{"phase", phaseName(latest->run.phase)}, {"runId", nullable(latest->run.runId)}});
        }
        return value;
    }
    std::string statusJson(nlohmann::json snapshot, std::uint64_t generation) {
        snapshot["generation"] = generation;
        return snapshot.dump();
    }
    std::string planJson(const Wave& wave) {
        Json targets = Json::array();
        for (const auto& [target, entries] : wave.plan) {
            Json units = Json::array();
            for (const auto& entry : entries) {
                units.push_back({{"package", entry.unit.package},
                                 {"repo", entry.repo.owner + "/" + entry.repo.repo},
                                 {"tag", entry.release.tag},
                                 {"commit", entry.release.sourceSha},
                                 {"version", entry.release.version.text()},
                                 {"revision", entry.revision},
                                 {"build_id", entry.buildId},
                                 {"action", entry.build ? "build" : "reuse"}});
            }
            targets.push_back({{"id", target}, {"units", units}});
        }
        return Json{{"wave", wave.id}, {"feed_branch", wave.feedBranch}, {"owner_run_id", nullable(wave.runId)}, {"targets", targets}}
            .dump();
    }
    std::string publicationPlanJson(const Publication& publication, const std::map<std::string, Wave>& waves) {
        return publicationPlan(publication, waves).dump();
    }
} // namespace coreci::app
