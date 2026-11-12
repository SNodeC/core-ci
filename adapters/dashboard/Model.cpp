#include "Model.h"

#include <nlohmann/json.hpp>
namespace coreci::dashboard {
    namespace {
        using Json = nlohmann::json;
        std::optional<app::VersionView> selection(const app::UnitView& unit) {
            std::optional<app::VersionView> value;
            if (unit.inflight) {
                value = unit.inflight->selection;
            } else if (unit.status.starts_with("Failed") && unit.failure) {
                value = unit.failure->selection;
            } else if (unit.next) {
                value = unit.next;
            } else if (unit.published) {
                value = unit.published->selection;
            }
            return value;
        }
        Json run(const github::JobRequest& request, const app::JobCache& jobs) {
            Json result = Json::object();
            if (request.runId) {
                result["runId"] = *request.runId;
                result["html_url"] = "https://github.com/" + request.workflow.owner + "/" + request.workflow.repo + "/actions/runs/" +
                                     std::to_string(*request.runId);
            }
            const auto value = jobs.get(request);
            if (value) {
                result.update(
                    {{"runId", value->runId}, {"html_url", value->htmlUrl}, {"status", value->status}, {"conclusion", value->conclusion}});
                if (value->job) {
                    const auto& job = *value->job;
                    result["job"] = {{"html_url", job.htmlUrl},
                                     {"started_at", job.startedAt},
                                     {"completed_at", job.completedAt},
                                     {"status", job.status},
                                     {"conclusion", job.conclusion}};
                }
            }
            return result;
        }
    } // namespace
    std::vector<github::JobRequest> jobRequests(const Engine& engine) {
        std::vector<github::JobRequest> result;
        for (const auto& [unit, view] : app::statusSnapshot(engine).units) {
            const auto current = selection(view);
            if (current && (view.inflight || view.status.starts_with("Failed") || view.status == "Settled")) {
                const auto id = unit.package + "~" + unit.target + "~" + current->version + "~r" + std::to_string(current->revision);
                const auto runId = view.inflight                       ? view.inflight->runId
                                   : view.status.starts_with("Failed") ? view.failure->runId
                                                                       : view.published->runId;
                result.push_back({engine.pipeline().packages().at(unit.package).buildWorkflow, id, "build", runId});
                if (engine.units().at(unit).publishDispatchedAt) {
                    result.push_back({engine.pipeline().feeds().at(view.feed).workflow, id, "publish", {}});
                }
            }
        }
        return result;
    }
    std::string model(const Engine& engine,
                      const github::LoadedDeclarations& declarations,
                      const app::JobCache& jobs,
                      std::uint64_t generation,
                      bool ready,
                      const std::string& email) {
        auto value = Json::parse(app::statusJson(app::statusSnapshot(engine), generation));
        ready = ready && engine.pipeline() == declarations.pipeline;
        value["ready"] = ready;
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
        const auto requests = jobRequests(engine);
        for (auto& unit : value["units"]) {
            const Unit key{unit.at("package"), unit.at("target")};
            const auto& state = engine.units().at(key);
            unit["order"] = state.order;
            unit["assigned"] = state.buildAssigned;
            unit["publishDispatchedAt"] = state.publishDispatchedAt ? Json(*state.publishDispatchedAt) : Json();
            unit["publishedAt"] = state.publishedAt ? Json(*state.publishedAt) : Json();
            const auto& target = declarations.catalog.at(key.target);
            unit.update({{"distribution", target.distribution},
                         {"suite", target.suite},
                         {"arch", target.arch},
                         {"format", target.format},
                         {"prerequisites", declarations.packages.at(key.package).prerequisites}});
            for (const auto& request : requests) {
                if (request.buildId.starts_with(key.package + "~" + key.target + "~")) {
                    unit[request.mainJob] = run(request, jobs);
                }
            }
            const auto& inflight = engine.units().at(key).inflight;
            if (inflight) {
                unit["tag"] = inflight->tag;
            }
        }
        return value.dump();
    }
    std::variant<Unit, github::Error> retry(const std::string& body) {
        std::variant<Unit, github::Error> result;
        try {
            const auto value = Json::parse(body);
            result = Unit{value.at("package").get<std::string>(), value.at("target").get<std::string>()};
        } catch (const std::exception&) {
            result = github::Error{"invalid retry body"};
        }
        return result;
    }
    std::string result(const std::string& text, bool error) {
        return Json{{error ? "error" : "result", text}}.dump();
    }
} // namespace coreci::dashboard
