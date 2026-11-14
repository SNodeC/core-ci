#include "Model.h"

#include <nlohmann/json.hpp>
namespace coreci::dashboard {
    namespace {
        nlohmann::json phase(const WorkflowPhase& value, const Workflow& workflow) {
            using Json = nlohmann::json;
            Json result{{"state", value.state},
                        {"start", value.start ? Json(*value.start) : Json()},
                        {"end", value.end ? Json(*value.end) : Json()},
                        {"duration", value.start && value.end ? Json(*value.end - *value.start) : Json()}};
            if (value.runId) {
                result["runId"] = *value.runId;
                result["html_url"] =
                    "https://github.com/" + workflow.owner + "/" + workflow.repo + "/actions/runs/" + std::to_string(*value.runId);
                if (value.runAttempt) {
                    result["runAttempt"] = *value.runAttempt;
                    result["html_url"] = result["html_url"].get<std::string>() + "/attempts/" + std::to_string(*value.runAttempt);
                }
            }
            return result;
        }
    } // namespace
    std::string model(const Engine& engine,
                      const github::LoadedDeclarations& declarations,
                      std::uint64_t generation,
                      bool ready,
                      const std::string& email,
                      const std::string& loadError) {
        using Json = nlohmann::json;
        auto value = Json::parse(app::statusJson(app::statusSnapshot(engine), generation));
        ready = ready && engine.pipeline() == declarations.pipeline;
        value["ready"] = ready;
        value["loadError"] = loadError;
        value["userEmail"] = email;
        for (auto& package : value["packages"]) {
            const auto name = package.at("name").get<std::string>();
            const auto& source = engine.pipeline().packages().at(name).sourceRepo;
            package["title"] =
                declarations.packages.contains(name) ? declarations.packages.at(name).title : source.owner + "/" + source.repo;
            package["commit"] = declarations.packages.contains(name) ? declarations.packages.at(name).commit : "";
        }
        if (!ready) {
            value["units"] = Json::array();
        }
        for (auto& unit : value["units"]) {
            const Unit key{unit.at("package"), unit.at("target")};
            const auto& state = engine.units().at(key);
            unit["order"] = state.order;
            unit["assigned"] = state.build.dispatchedAt.has_value() || state.build.runId.has_value();
            unit["publishDispatchedAt"] = state.publish.dispatchedAt ? Json(*state.publish.dispatchedAt) : Json();
            unit["publishedAt"] = state.publish.end ? Json(*state.publish.end) : Json();
            const auto& target = declarations.catalog.at(key.target);
            unit.update({{"distribution", target.distribution},
                         {"suite", target.suite},
                         {"arch", target.arch},
                         {"format", target.format},
                         {"prerequisites", declarations.packages.at(key.package).prerequisites}});
            unit["build"] = phase(state.build, engine.pipeline().packages().at(key.package).buildWorkflow);
            unit["publish"] = phase(state.publish, engine.pipeline().feeds().at(unit.at("feed").get<std::string>()).workflow);
            const auto& inflight = engine.units().at(key).inflight;
            if (inflight) {
                unit["tag"] = inflight->tag;
            }
        }
        return value.dump();
    }
    std::variant<Unit, github::Error> unit(const std::string& body) {
        std::variant<Unit, github::Error> result;
        try {
            const auto value = nlohmann::json::parse(body);
            result = Unit{value.at("package").get<std::string>(), value.at("target").get<std::string>()};
        } catch (const std::exception&) {
            result = github::Error{"invalid unit body"};
        }
        return result;
    }
    std::string result(const std::string& text, bool error) {
        return nlohmann::json{{error ? "error" : "result", text}}.dump();
    }
} // namespace coreci::dashboard
