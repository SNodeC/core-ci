#include "Model.h"

#include <nlohmann/json.hpp>
namespace coreci::dashboard {
    namespace {
        std::string runLink(const Wave& wave) {
            std::string result;
            if (wave.runId) {
                result =
                    "https://github.com/" + wave.workflow.owner + "/" + wave.workflow.repo + "/actions/runs/" + std::to_string(*wave.runId);
                if (wave.runAttempt) {
                    result += "/attempts/" + std::to_string(*wave.runAttempt);
                }
            }
            return result;
        }
        nlohmann::json
        phase(const std::string& state, std::optional<std::int64_t> start, std::optional<std::int64_t> end, const Wave* wave) {
            using Json = nlohmann::json;
            Json result{{"state", state},
                        {"start", start ? Json(*start) : Json()},
                        {"end", end ? Json(*end) : Json()},
                        {"duration", start && end ? Json(*end - *start) : Json()}};
            if (wave && wave->runId) {
                result["runId"] = *wave->runId;
                result["runAttempt"] = wave->runAttempt ? Json(*wave->runAttempt) : Json();
                result["html_url"] = runLink(*wave);
            }
            return result;
        }
    } // namespace
    std::string model(const WaveController& engine,
                      const github::LoadedDeclarations& declarations,
                      std::uint64_t generation,
                      bool ready,
                      const std::string& email,
                      const std::string& loadError) {
        using Json = nlohmann::json;
        auto value = app::statusSnapshot(engine, generation);
        ready = ready && engine.pipeline() == declarations.pipeline;
        value["ready"] = ready;
        value["loadError"] = loadError;
        value["userEmail"] = email;
        if (!ready) {
            value["units"] = Json::array();
        }
        if (const auto* wave = engine.latestWave()) {
            value["wave"]["html_url"] = runLink(*wave);
        }
        if (!value["publication"].is_null()) {
            const auto id = value["publication"].at("publication").get<std::string>();
            value["publication"]["html_url"] = runLink(engine.publications().at(id).run);
        }
        for (auto& unit : value["units"]) {
            const Unit key{unit.at("package"), unit.at("target")};
            const auto& state = engine.units().at(key);
            unit["order"] = state.order;
            const auto w = engine.waves().find(state.waveId);
            const auto* wave = w == engine.waves().end() ? nullptr : &w->second;
            unit["assigned"] = wave != nullptr;
            unit["publishedAt"] = state.published ? Json(state.published->at) : Json();
            const auto& target = declarations.catalog.at(key.target);
            unit.update({{"distribution", target.distribution},
                         {"suite", target.release},
                         {"arch", target.arch},
                         {"format", target.family},
                         {"prerequisites", Json::array()}});
            for (const auto& [name, range] : engine.pipeline().prerequisites(key.package)) {
                unit["prerequisites"].push_back(name);
            }
            const auto buildState =
                state.failure && state.failure->buildFailed
                    ? "failed"
                    : (state.state == State::Blocked ? "blocked"
                                                     : (state.buildEnd ? "succeeded" : (state.buildStart ? "running" : "pending")));
            unit["build"] = phase(buildState, state.buildStart, state.buildEnd, wave);
            const auto waveTarget = engine.stored().waveTargets.find({state.waveId, key.target});
            const Wave* publication = waveTarget != engine.stored().waveTargets.end() && !waveTarget->second.publicationId.empty()
                                          ? &engine.publications().at(waveTarget->second.publicationId).run
                                          : nullptr;
            if (!publication && state.published && state.published->waveId == state.waveId) {
                publication = wave;
            }
            unit["publish"] = phase(state.publishEnd ? (state.state == State::Published ? "succeeded" : "failed")
                                                     : (state.state == State::Publishing ? "running" : "pending"),
                                    publication ? publication->started : std::nullopt,
                                    state.publishEnd,
                                    publication);
            if (const auto* entry = engine.selection(key)) {
                unit["tag"] = entry->release.tag;
            }
        }
        return value.dump();
    }
    std::variant<Event, github::Error> operation(const std::string& name, const std::string& body, std::int64_t at) {
        std::variant<Event, github::Error> result;
        try {
            const auto value = nlohmann::json::parse(body);
            if (name == "dispatch-paused") {
                result = DispatchPaused{at, value.at("paused").get<bool>()};
            } else {
                const auto package = value.at("package").get<std::string>(), target = value.at("target").get<std::string>();
                result = name == "retry" ? Event{RetryRequested{at, package, target}} : Event{RebuildRequested{at, package, target}};
            }
        } catch (const std::exception&) {
            result = github::Error{"invalid operation body"};
        }
        return result;
    }
    std::string result(const std::string& text, bool error) {
        return nlohmann::json{{error ? "error" : "result", text}}.dump();
    }
} // namespace coreci::dashboard
