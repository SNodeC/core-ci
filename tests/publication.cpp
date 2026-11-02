#include "fixtures.h"

#include <core/EventReceiver.h>
#include <core/SNodeC.h>
#include <iostream>
#include <utils/Config.h>

namespace {
    ci::Json remoteRun;
    ci::Json remoteJobs;
    ci::Json receipt;
    std::string scenario;
    std::size_t reads = 0;
    std::size_t discoveries = 0;

    void observeRemote(const ci::Json& work) {
        const std::string stage = scenario == "resume_build" || scenario == "resume_build_running" ? "build" : "publish";
        const auto& command = work.at("attempts").back().at(stage + "_assignment");
        const auto observation = fixture::observation(work, stage);
        remoteRun = {
            {"id", observation.at("identity").at("id")},
            {"repository", {{"id", command.at("repository_id")}}},
            {"head_repository", {{"id", command.at("repository_id")}}},
            {"actor", {{"id", 41}}},
            {"event", "workflow_dispatch"},
            {"run_attempt", 1},
            {"head_sha", command.at("commit")},
            {"path", command.at("path")},
            {"display_title",
             (stage == "build" ? "Build " : "Publish ") + work.at("attempts").back().at("assignment").at("label").get<std::string>()},
            {"status", "in_progress"},
            {"conclusion", nullptr},
            {"html_url", "https://github.com/Fixture/worker/actions/runs/502"}};
        remoteJobs = observation.at("jobs");
        if (stage == "publish") {
            ci::Json confirm = remoteJobs.at(0);
            confirm["id"] = 703;
            confirm["name"] = "Confirm public receipt";
            if (scenario == "queued" || scenario == "running" || scenario == "resume_running") {
                confirm["status"] = scenario == "queued" ? "queued" : "in_progress";
                confirm["conclusion"] = nullptr;
                confirm["completed_at"] = nullptr;
            } else if (scenario == "skipped" || scenario == "failure") {
                confirm["conclusion"] = scenario;
                remoteRun["status"] = "completed";
                remoteRun["conclusion"] = scenario == "failure" ? "failure" : "success";
            }
            if (scenario != "missing") {
                remoteJobs.push_back(confirm);
            }
            if (scenario == "duplicate") {
                confirm["id"] = 704;
                remoteJobs.push_back(confirm);
            } else if (scenario == "wrong_job_run") {
                remoteJobs.back()["run_id"] = 999;
            } else if (scenario == "wrong_job_attempt") {
                remoteJobs.back()["run_attempt"] = 2;
            } else if (scenario == "wrong_actor") {
                remoteRun["actor"]["id"] = 999;
            } else if (scenario == "wrong_workflow") {
                remoteRun["path"] = ".github/workflows/untrusted.yml";
            } else if (scenario == "wrong_run_attempt") {
                remoteRun["run_attempt"] = 2;
            } else if (scenario == "publish_failed") {
                remoteJobs.at(0)["conclusion"] = "failure";
            }
            receipt = fixture::receipt(work);
            if (scenario == "wrong_receipt") {
                receipt["attempt"] = "unrelated:1";
            }
        } else {
            remoteRun["status"] = scenario == "resume_build" ? "completed" : "in_progress";
            remoteRun["conclusion"] = scenario == "resume_build" ? ci::Json("failure") : ci::Json();
            remoteJobs.at(0)["status"] = remoteRun.at("status");
            remoteJobs.at(0)["conclusion"] = remoteRun.at("conclusion");
            if (scenario == "resume_build_running") {
                remoteJobs.at(0)["completed_at"] = nullptr;
            }
        }
    }
} // namespace

// Test-only doubles of the existing public GitHub API boundary. Execution.cpp,
// its run/job validation, Store and the graph state machine remain the real code.
namespace ci {
    Controller::Controller(Store& store, Json deployment, std::string app, std::string installation, std::string key)
        : deployment(std::move(deployment))
        , store(store)
        , app(std::move(app))
        , installation(std::move(installation))
        , key(std::move(key)) {
    }

    void Controller::token(const std::function<void(const std::string&, const std::string&)>& done) {
        done("isolated-token", "");
    }

    void Controller::api(const std::string& token, const std::string& path, const Value& done) {
        require(token == "isolated-token", "Unexpected authentication");
        if (path.ends_with("/attempts/1")) {
            done(remoteRun, "");
        } else {
            require(path.find("/commits?") != std::string::npos, "Unexpected API request: " + path);
            ++reads;
            done(scenario == "missing_receipt" ? Json::array() : Json::array({{{"sha", std::string(40, '5')}}}), "");
        }
    }

    void Controller::pages(const std::string&, const std::string& path, const std::string& field, const Value& done, int, Json) {
        if (field == "workflow_runs") {
            ++discoveries;
            done(scenario == "resume_unbound" ? Json::array() : Json::array({remoteRun}), "");
        } else {
            require(field == "jobs" && path.ends_with("/attempts/1/jobs"), "Unexpected pagination");
            done(remoteJobs, "");
        }
    }

    void Controller::contents(const std::string&, const std::string&, const std::string& path, const std::string&, const Value& done) {
        if (path.starts_with(".github/workflows/")) {
            done({{"sha", std::string(40, scenario == "wrong_blob" ? 'd' : path.ends_with("build.yml") ? 'b' : 'c')}}, "");
        } else {
            require(path.starts_with("receipts/"), "Unexpected contents request");
            done({{"bytes", receipt.dump()}}, "");
        }
    }

    void Controller::request(const std::string&, const std::string&, const Done&, const std::string&) {
        throw std::runtime_error("A publication observation must never select or replace work");
    }
} // namespace ci

int main(int argc, char* argv[]) {
    ci::Store store;
    ci::Controller controller(store, fixture::deployment(), "fixture", "17", "unused");
    int result = 0;
    bool started = false;
    std::size_t effects = 0;
    std::size_t notifications = 0;
    std::size_t recovered = 0;
    std::size_t errors = 0;
    scenario = std::getenv("CI_PUBLICATION_CASE");
    const std::size_t parentIndex = scenario == "resume_mixed" ? 2 : 0;
    const bool prepared = std::getenv("CI_PUBLICATION_PREPARED") != nullptr;
    const std::string database = std::getenv("CI_TEST_DATABASE");
    const auto persisted = [&]() {
        std::unique_ptr<MYSQL, decltype(&mysql_close)> connection(mysql_init(nullptr), mysql_close);
        ci::require(mysql_real_connect(connection.get(), "localhost", "root", "", database.c_str(), 0, std::getenv("CI_TEST_BACKEND"), 0) !=
                        nullptr,
                    "Independent DB connection");
        ci::require(mysql_query(connection.get(), "SELECT body FROM ci_graph WHERE id=1") == 0, "Independent committed read");
        std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> rows(mysql_store_result(connection.get()), mysql_free_result);
        return ci::Json::parse(mysql_fetch_row(rows.get())[0]);
    };
    const ci::Done finish = [&](const std::string& error) {
        if (!error.empty()) {
            std::cerr << scenario << ": " << error << '\n';
            result = 1;
        }
        core::SNodeC::stop();
    };
    store.dispatch = [&](const ci::Json& actions) {
        const auto graph = persisted();
        for (const auto& action : actions) {
            const auto& execution = graph.at("executions").at(1);
            const auto& input = execution.at("attempts").back().at("assignment").at("dependencies").at(0);
            ci::Json expected = receipt;
            expected.erase("indexes");
            ci::require(action.at("execution") == execution.at("id") &&
                            graph.at("executions").at(parentIndex).at("attempts").back().at("state") == "published" &&
                            input.at("attempt") == graph.at("executions").at(parentIndex).at("attempts").back().at("id") &&
                            input.at("receipt") == expected,
                        "Exact proof and dispatch authority must be committed together");
            ++effects;
        }
    };
    store.changed = [&](const ci::Json&) {
        ++notifications;
    };
    const auto exercise = [&]() {
        store.read(ci::checked(
            [&](const ci::Json&, const std::string& error) {
                finish(error);
            },
            [&](const ci::Json& snapshot) {
                ci::Json graph = snapshot.at("graph");
                if (scenario.starts_with("resume_")) {
                    const auto original = graph;
                    observeRemote(graph.at("executions").at(parentIndex));
                    const ci::Done recoveredOne = [&, original](const std::string& error) {
                        ++recovered;
                        errors += !error.empty();
                        const bool concurrent = scenario == "resume_mixed" || scenario == "resume_race";
                        if (recovered == (concurrent ? 2U : 1U)) {
                            try {
                                const auto saved = persisted();
                                const auto& active = saved.at("executions").at(parentIndex).at("attempts").back();
                                const bool complete = scenario == "resume_complete" || concurrent;
                                const bool published = complete || scenario == "resume_unknown" || scenario == "resume_again";
                                const bool rejected = scenario == "resume_reject" || scenario == "resume_unknown";
                                ci::require(effects == (complete ? 1U : 0U), "Recovery cannot replay an old assignment");
                                ci::require((active.at("state") == "published") == published, "Missed publication recovery");
                                ci::require(errors == (rejected || scenario == "resume_unbound" || scenario == "resume_again" ||
                                                               scenario == "resume_mixed"
                                                           ? 1U
                                                           : 0U),
                                            "Independent recovery errors: " + error);
                                ci::require(active.at("assignment") ==
                                                original.at("executions").at(parentIndex).at("attempts").back().at("assignment"),
                                            "Recovery preserves the exact original assignment");
                                if (scenario == "resume_build" || scenario == "resume_build_running") {
                                    ci::require(active.at("state") == (scenario == "resume_build" ? "failed" : "building") && reads == 0,
                                                "Missed build completion or running build must use the original attempt");
                                }
                                if (scenario == "resume_mixed") {
                                    ci::require(discoveries == 2, "Only unfinished assigned executions are observed");
                                    for (const std::size_t i : {0U, 3U, 4U}) {
                                        ci::require(saved.at("executions").at(i) == original.at("executions").at(i),
                                                    "Unbound and terminal history must remain unchanged");
                                    }
                                }
                                if (rejected || scenario == "resume_again" || scenario == "resume_unbound") {
                                    ci::require(notifications == 0, "Unresolved commit or unchanged recovery cannot notify SSE");
                                }
                                finish("");
                            } catch (const std::exception& failure) {
                                finish(failure.what());
                            }
                        }
                    };
                    controller.resume(recoveredOne);
                    if (scenario == "resume_race") {
                        controller.webhook("workflow_run",
                                           {{"installation", {{"id", 17}}},
                                            {"repository", {{"full_name", "Fixture/worker"}, {"id", 42}}},
                                            {"workflow_run", {{"id", 502}, {"display_title", remoteRun.at("display_title")}}}},
                                           recoveredOne);
                    }
                } else if (scenario == "restart") {
                    ci::require(ci::commands(graph).empty() && effects == 0 && notifications == 0, "Restart cannot replay assignments");
                    finish("");
                } else {
                    const auto selected = graph.at("executions").at(0);
                    observeRemote(selected);
                    controller.observe(
                        selected.at("id"),
                        [&, selected](const std::string& error) {
                            try {
                                const auto graph = persisted();
                                const auto& active = graph.at("executions").at(0).at("attempts").back();
                                const bool complete = scenario == "complete" || scenario == "duplicate_event" || scenario == "out_of_order";
                                const bool blocked = scenario == "reject" || scenario == "unknown";
                                const bool invalid = scenario.starts_with("wrong_");
                                ci::require(error.empty() != (invalid || blocked), "Unexpected observation outcome: " + error);
                                ci::require(
                                    reads ==
                                        (complete || blocked || scenario == "missing_receipt" || scenario == "wrong_receipt" ? 1U : 0U),
                                    "Receipt fetched before required confirmation");
                                ci::require(effects == (complete ? 1U : 0U), "Unexpected dispatch count");
                                ci::require((active.at("state") == "published") == (complete || scenario == "unknown"),
                                            "Publication state must reflect confirmation and actual commit");
                                if (invalid || blocked) {
                                    ci::require(notifications == 0, "Rejected/uncertain change must not emit committed SSE");
                                }
                                if (scenario == "duplicate_event" || scenario == "out_of_order") {
                                    if (scenario == "out_of_order") {
                                        remoteJobs.back()["status"] = "queued";
                                        remoteJobs.back()["conclusion"] = nullptr;
                                        remoteJobs.back()["completed_at"] = nullptr;
                                    }
                                    const auto before = notifications;
                                    controller.observe(
                                        selected.at("id"),
                                        [&, before](const std::string& error) {
                                            ci::require(error.empty() && effects == 1 && notifications == before,
                                                        "Duplicate evidence must not redispatch or notify unchanged facts");
                                            finish("");
                                        },
                                        502);
                                } else {
                                    finish("");
                                }
                            } catch (const std::exception& failure) {
                                finish(failure.what());
                            }
                        },
                        502);
                }
            }));
    };
    core::SNodeC::init(argc, argv);
    core::EventReceiver::atNextTick([&]() {
        const database::mariadb::MariaDBConnectionDetails details = {.connectionName = "publication",
                                                                     .hostname = "localhost",
                                                                     .username = "root",
                                                                     .password = "",
                                                                     .database = database,
                                                                     .port = 0,
                                                                     .socket = std::getenv("CI_TEST_SOCKET"),
                                                                     .flags = 0};
        store.db = std::make_unique<database::mariadb::MariaDBClient>(
            details,
            [&](const database::mariadb::MariaDBState& state) {
                if (state.connected && !started) {
                    started = true;
                    store.initialize([&](const std::string& error) {
                        if (!error.empty()) {
                            finish(error);
                        } else if (prepared) {
                            exercise();
                        } else {
                            store.change(
                                [](ci::Json& graph) {
                                    graph = fixture::graph();
                                    ci::Json selection = {{"Fixture/base", fixture::selected("base", ci::Json::array())},
                                                          {"Fixture/client", fixture::selected("client", ci::Json::array({"base"}))}};
                                    ci::selectRequest(graph, selection, "Fixture/base");
                                    if (scenario != "resume_build" && scenario != "resume_build_running") {
                                        fixture::finishBuild(graph, graph.at("executions").at(0).at("id"));
                                    }
                                    if (scenario == "resume_mixed") {
                                        for (const std::string state : {"publishing", "published", "failed"}) {
                                            auto other = graph.at("executions").at(0);
                                            other["id"] = ci::hash(state);
                                            auto& active = other["attempts"].back();
                                            active["id"] = state + ":1";
                                            active["state"] = state;
                                            active["assignment"]["label"] = "Unbound " + state;
                                            graph["executions"].push_back(other);
                                        }
                                        std::swap(graph["executions"][0], graph["executions"][2]);
                                    }
                                    return ci::Json::array();
                                },
                                [&](const std::string& error) {
                                    notifications = 0;
                                    if (!error.empty()) {
                                        finish(error);
                                    } else {
                                        exercise();
                                    }
                                });
                        }
                    });
                } else if (state.error != 0 && !started) {
                    finish(state.errorMessage);
                }
            },
            "database");
        store.db->connect();
    });
    core::SNodeC::start();
    std::cout << scenario << ": receipt reads=" << reads << ", committed dispatches=" << effects << '\n';
    return result;
}
