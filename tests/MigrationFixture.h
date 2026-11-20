#pragma once
#include "Check.h"
#include "app/StatusJson.h"
#include "migration/legacy/StatusJson.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
using Json = nlohmann::json;
using coreci::legacy::Engine;
using coreci::legacy::Wave;
namespace migration_app = coreci::legacy::app;
inline Json readJson(const std::filesystem::path& path) {
    std::ifstream input(path);
    Json value;
    input >> value;
    return value;
}

template <typename T>
Json optional(const std::optional<T>& value) {
    return value ? Json(*value) : Json(nullptr);
}

inline Json statusView(const Engine& engine) {
    return Json::parse(migration_app::statusJson(migration_app::statusSnapshot(engine), 0));
}
Json statusView(const coreci::WaveController& engine) {
    auto value = Json::parse(coreci::app::statusJson(coreci::app::statusSnapshot(engine), 0));
    value.erase("publication");
    value.erase("targets");
    return value;
}
inline std::string planView(const Wave& wave) {
    return migration_app::planJson(wave);
}
std::string planView(const coreci::Wave& wave) {
    return coreci::app::planJson(wave);
}
template <typename Controller>
Json snapshot(const Controller& engine) {
    Json result;
    result["status"] = statusView(engine);
    for (const auto& [id, wave] : engine.waves()) {
        result["waves"][id] = {{"plan", Json::parse(planView(wave))},
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
