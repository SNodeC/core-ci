#include "Check.h"
#include "app/StatusJson.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace coreci;
using Json = nlohmann::json;

Json readJson(const std::filesystem::path& path) {
    std::ifstream input(path);
    Json value;
    input >> value;
    return value;
}

template <typename T>
Json optional(const std::optional<T>& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json snapshot(const Engine& engine) {
    Json result;
    result["status"] = Json::parse(app::statusJson(app::statusSnapshot(engine), 0));
    for (const auto& [id, wave] : engine.waves()) {
        result["waves"][id] = {{"plan", Json::parse(app::planJson(wave))},
                               {"phase", phaseName(wave.phase)},
                               {"workflow", {wave.workflow.owner, wave.workflow.repo, wave.workflow.branch}},
                               {"attempt", wave.attempt},
                               {"retry_at", optional(wave.retryAt)},
                               {"dispatched_at", wave.dispatchedAt},
                               {"deadline", wave.deadline},
                               {"accepted", wave.dispatchAccepted},
                               {"started", optional(wave.started)},
                               {"finished", optional(wave.finished)},
                               {"conclusion", wave.conclusion},
                               {"run_id", optional(wave.runId)},
                               {"run_attempt", optional(wave.runAttempt)}};
    }
    for (const auto& [unit, state] : engine.units()) {
        auto& row = result["units"][unit.package + "/" + unit.target];
        row = {{"state", stateName(state.state)},
               {"wave", state.waveId},
               {"order", state.order},
               {"artifact", state.artifact},
               {"requested", state.requested},
               {"build_start", optional(state.buildStart)},
               {"build_end", optional(state.buildEnd)},
               {"publish_end", optional(state.publishEnd)},
               {"retry_after", optional(state.retryAfter)}};
        for (const auto& [version, revision] : state.lastRevision) {
            row["revisions"][version.text()] = revision;
        }
        row["auto_retried"] = state.autoRetried ? Json(state.autoRetried->first.text()) : Json(nullptr);
    }
    return result;
}

Engine history(const std::filesystem::path& directory, std::int64_t configuredDeadline) {
    const auto plan = readJson(directory / "legacy-w964-plan.json");
    const auto origins = readJson(directory / "production/origins.json");
    Pipeline::Builder builder;
    std::map<std::string, Json> packages;
    for (const auto& target : plan.at("targets")) {
        builder.target({target.at("id")});
        for (const auto& entry : target.at("units")) {
            packages.emplace(entry.at("package"), entry);
        }
    }
    std::vector<TagObserved> tags;
    constexpr std::int64_t start = 1794852832;
    for (const auto& [name, entry] : packages) {
        const std::string repo = entry.at("repo");
        const auto slash = repo.find('/');
        const auto declaration = readJson(directory / "production" / (repo.substr(slash + 1) + ".json"));
        builder.package({name,
                         {repo.substr(0, slash), repo.substr(slash + 1), origins.at(repo.substr(slash + 1)).at("branch")},
                         declaration.at("targets").get<std::set<std::string>>(),
                         declaration.at("depends_on").get<std::vector<std::string>>(),
                         entry.at("revision").get<unsigned>() - 1});
        tags.push_back({start - 6, name, entry.at("tag"), entry.at("commit"), Origin::Reconcile});
    }
    Engine engine({5, configuredDeadline, 8, 600});
    engine.handle(GraphChanged{start - 6, builder.build(), tags}, 963);
    engine.handle(Tick{start - 1}, 964);
    engine.handle(WaveStarted{start, "w964", 43926583894, 1}, 965);
    std::uint64_t seq = 966;
    const auto outcomes = [&](const Wave& wave, unsigned built, unsigned failed, unsigned building, std::int64_t at) {
        unsigned index = 0;
        for (const auto& [target, entries] : wave.plan) {
            for (const auto& entry : entries) {
                if (entry.build) {
                    if (index < built) {
                        engine.handle(UnitBuildSucceeded{at, wave.id, entry.buildId, "artifact"}, seq++);
                    } else if (index < built + failed) {
                        engine.handle(UnitBuildFailed{at, wave.id, entry.buildId, "compiler"}, seq++);
                    } else if (index < built + failed + building) {
                        engine.handle(UnitBuildStarted{at, wave.id, entry.buildId}, seq++);
                    }
                    ++index;
                }
            }
        }
        CHECK_EQ(index, 173U);
    };
    outcomes(*engine.latestWave(), 148, 24, 1, start + 23380);
    engine.handle(Tick{start + 23399}, 1300);
    engine.handle(Tick{start + 23999}, 1337);
    engine.handle(TagObserved{start + 24000, "core-ci", "v0.1.5", std::string(40, 'c'), Origin::Webhook}, 1338);
    engine.handle(Tick{start + 24005}, 1339);
    engine.handle(WaveStarted{start + 24006, "w1339", 43991561301, 1}, 1340);
    CHECK(engine.waves().contains("w1339"));
    seq = 1341;
    outcomes(*engine.latestWave(), 148, 10, 3, start + 47385);
    engine.handle(Tick{start + 47405}, 1700);
    engine.handle(Tick{start + 48005}, 1701);
    engine.handle(Tick{start + 48010}, 1702);
    engine.handle(Resumed{start + 48011}, 1703);
    return engine;
}

int main(int argc, char** argv) {
    CHECK_EQ(argc, 2);
    if (argc == 2) {
        const std::filesystem::path directory(argv[1]);
        const auto baseline = readJson(directory / "legacy-replay-baseline.json");
        for (const std::int64_t duration : {23400, 86400, 60}) {
            const auto engine = history(directory, duration);
            CHECK(snapshot(engine) == baseline);
            CHECK(engine.waves().at("w964").phase == WavePhase::Finished);
            CHECK(engine.waves().at("w1339").phase == WavePhase::Finished);
            for (const auto& [unit, state] : engine.units()) {
                for (const auto& [version, revision] : state.lastRevision) {
                    CHECK(revision >= baseline.at("units").at(unit.package + "/" + unit.target).at("revisions").at(version.text()));
                }
            }
        }
    }
    return checkResult();
}
