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
    std::size_t runReads = 0;
    std::size_t discoveries = 0;
    std::string diagnosticPhase;
    ci::Value heldRead;

    void observeRemote(const ci::Json& work) {
        const std::string stage =
            scenario == "resume_build" || scenario == "resume_build_running" || scenario.starts_with("build_") ? "build" : "publish";
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
            remoteRun["status"] = scenario == "resume_build" || scenario.starts_with("build_") ? "completed" : "in_progress";
            remoteRun["conclusion"] = scenario.starts_with("build_") ? ci::Json("success")
                                      : scenario == "resume_build"   ? ci::Json("failure")
                                                                     : ci::Json();
            remoteJobs.at(0)["status"] = remoteRun.at("status");
            remoteJobs.at(0)["conclusion"] = remoteRun.at("conclusion");
            if (scenario == "build_missing_job") {
                remoteJobs = ci::Json::array();
            }
            if (scenario == "build_skipped_job") {
                remoteJobs.at(0)["conclusion"] = "skipped";
            }
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
        done("isolated-token",
             scenario == "diagnostic_token" && diagnosticPhase == "F" ? "POST https://api.github.com/fixture connect: unavailable 1/3"
                                                                      : "");
    }

    void Controller::api(const std::string& token, const std::string& path, const Value& done) {
        require(token == "isolated-token", "Unexpected authentication");
        if (path.ends_with("/attempts/1") && scenario == "diagnostic_ack") {
            heldRead = done;
        } else if (path.ends_with("/attempts/1")) {
            ++runReads;
            done(remoteRun,
                 diagnosticPhase == "L"   ? "Receipt evidence unavailable"
                 : diagnosticPhase == "F" ? "GET https://api.github.com/fixture response: connection closed before HTTP response 3/3"
                                          : "");
        } else {
            require(path.find("/commits?") != std::string::npos, "Unexpected API request: " + path);
            ++reads;
            done(scenario == "missing_receipt" ? Json::array() : Json::array({{{"sha", std::string(40, '5')}}}),
                 diagnosticPhase == "E" ? "Receipt evidence unavailable" : "");
        }
    }

    void Controller::pages(const std::string&, const std::string& path, const std::string& field, const Value& done, int, Json) {
        if (field == "workflow_runs") {
            ++discoveries;
            done(scenario == "resume_unbound" ? Json::array() : Json::array({remoteRun}), "");
        } else if (field == "artifacts") {
            require(scenario.starts_with("build_"), "Unexpected artifact listing");
            done(Json::array(), "");
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
            done({{"bytes", scenario == "malformed_receipt" ? "{\"receipt\":\"private-response-sentinel" : receipt.dump()}}, "");
        }
    }

    void Controller::request(const std::string&, const std::string&, const Done&, const std::string&) {
        throw std::runtime_error("A publication observation must never select or replace work");
    }
} // namespace ci

namespace {
    void diagnosticSequence(ci::Store& store,
                            ci::Controller& controller,
                            const ci::Json& selected,
                            std::size_t& notifications,
                            const ci::Done& finish,
                            const std::function<ci::Json()>& persisted) {
        const bool token = scenario == "diagnostic_token";
        const bool read = scenario == "diagnostic_read" || token;
        const bool legacy = scenario == "diagnostic_legacy";
        const bool blocked = scenario == "diagnostic_reject" || scenario == "diagnostic_unknown";
        const std::vector<std::string> stages = legacy    ? std::vector<std::string>{"L", "E", "F", "F", "S", "C"}
                                                : blocked ? std::vector<std::string>{"F"}
                                                : read    ? std::vector<std::string>{"F", "F", "S", "C"}
                                                          : std::vector<std::string>{"E", "F", "F", "S", "C"};
        const auto advance = [&, selected, stages, read, legacy, blocked, token, finish, persisted](
                                 const auto& self, std::size_t index, ci::Json previous) -> void {
            diagnosticPhase = stages.at(index);
            remoteJobs.back()["status"] = diagnosticPhase == "S" ? "queued" : "completed";
            remoteJobs.back()["conclusion"] = diagnosticPhase == "S" ? ci::Json() : ci::Json("success");
            controller.observe(
                selected.at("id"),
                [&, self, index, previous, selected, stages, read, legacy, blocked, token, finish, persisted](const std::string& error) {
                    try {
                        ci::require((!error.empty()) == (stages.at(index) == "L" || stages.at(index) == "E" || stages.at(index) == "F"),
                                    "Correlated observation result");
                        if (!error.empty()) {
                            ci::require(
                                error.starts_with("Observation " + selected.at("attempts").back().at("id").get<std::string>() + ": "),
                                "Attempt correlation");
                        }
                        ci::require(error.find("private-response-sentinel") == std::string::npos, "No untrusted JSON in diagnostics");
                        if (blocked) {
                            const auto saved = persisted();
                            const auto& active = saved.at("executions").at(0).at("attempts").back();
                            ci::require(notifications == 0 && active.at("state") == "publishing" &&
                                            active.contains("uncertainty") == (scenario == "diagnostic_unknown"),
                                        "Failed/uncertain diagnostic commit sends no SSE, command or retry");
                            finish("");
                        } else {
                            store.read(ci::checked(
                                [finish](const ci::Json&, const std::string& failure) {
                                    finish(failure);
                                },
                                [&, self, index, previous, selected, stages, read, legacy, blocked, token, finish, persisted](
                                    const ci::Json& snapshot) {
                                    const auto& active = snapshot.at("graph").at("executions").at(0).at("attempts").back();
                                    const auto phase = stages.at(index);
                                    if (phase == "L") {
                                        ci::require(notifications == 0 && snapshot.at("revision") == 2 &&
                                                        active.at("uncertainty") == ci::Json{{"at", "2026-01-01T00:00:00Z"},
                                                                                             {"reason", "Receipt evidence unavailable"}},
                                                    "Identical normalized legacy failure performs no UPDATE, revision change or SSE");
                                    }
                                    if (phase == "C") {
                                        ci::require(active.at("state") == "published" && !active.contains("uncertainty") &&
                                                        snapshot.at("graph").at("executions").at(1).at("attempts").back().at("state") ==
                                                            "building",
                                                    "Validated receipt clears uncertainty and authorizes exact dependents atomically");
                                    } else if (phase == "S" && read) {
                                        ci::require(!active.contains("uncertainty") && active.at("state") == "publishing",
                                                    "Read success clears read uncertainty without authorizing work");
                                    } else {
                                        const auto& uncertainty = active.at("uncertainty");
                                        ci::require(
                                            (legacy && index <= 1 && !uncertainty.contains("kind") && !uncertainty.contains("stage")) ||
                                                (uncertainty.at("stage") == "publish" &&
                                                 uncertainty.at("kind") == (read ? "read" : "evidence")),
                                            "Read errors preserve required evidence clearance");
                                        ci::require(uncertainty.at("reason") ==
                                                        (phase == "L" || phase == "E" ? "Receipt evidence unavailable"
                                                         : token ? "POST https://api.github.com/fixture connect: unavailable 1/3"
                                                                 : "GET https://api.github.com/fixture response: connection closed before "
                                                                   "HTTP response 3/3"),
                                                    "Final fixed failure context is durable");
                                        if (index == 0 && read) {
                                            ci::require(!active.at("facts").contains("publish_run"), "Early error fabricates no run facts");
                                        }
                                        if ((phase == "F" && index > 0 && stages.at(index - 1) == "F") || phase == "S") {
                                            ci::require(snapshot == previous,
                                                        "Repeated F or evidence-level S cannot write, revise or broadcast");
                                        }
                                    }
                                    if (index + 1 == stages.size()) {
                                        finish("");
                                    } else {
                                        self(self, index + 1, snapshot);
                                    }
                                }));
                        }
                    } catch (const std::exception& failure) {
                        finish(failure.what());
                    }
                },
                502);
        };
        const auto start = [&, advance, finish]() {
            notifications = 0;
            advance(advance, 0, {});
        };
        if (legacy) {
            store.change(
                [selected](ci::Json& graph) {
                    graph["executions"][0]["attempts"].back()["uncertainty"] = {{"at", "2026-01-01T00:00:00Z"},
                                                                                {"reason", "Receipt evidence unavailable"}};
                    return ci::Json::array();
                },
                [start, finish](const std::string& error) {
                    if (error.empty()) {
                        start();
                    } else {
                        finish(error);
                    }
                });
        } else {
            start();
        }
    }
} // namespace

int main(int argc, char* argv[]) {
    ci::Store store;
    auto grants = fixture::deployment();
    grants["repositories"]["Fixture/base"] = 100;
    ci::Controller controller(store, grants, "fixture", "17", "unused");
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
    store.changed = [&](const ci::Json& changes) {
        ci::require(changes.dump().find("private-response-sentinel") == std::string::npos, "No remote JSON excerpts in SSE");
        ++notifications;
    };
    const auto exercise = [&]() {
        store.read(ci::checked(
            [&](const ci::Json&, const std::string& error) {
                finish(error);
            },
            [&](const ci::Json& snapshot) {
                ci::Json graph = snapshot.at("graph");
                if (scenario.starts_with("late_build_")) {
                    const auto selected = graph.at("executions").at(0);
                    const bool job = scenario.find("_job_") != std::string::npos;
                    const std::string event = job ? "workflow_job" : "workflow_run";
                    const ci::Json notification = {
                        {job ? "run_id" : "id", 501},
                        {job ? "name" : "display_title",
                         "Build " + selected.at("attempts").back().at("assignment").at("label").get<std::string>()}};
                    controller.webhook(
                        event,
                        {{"installation", {{"id", 17}}},
                         {"repository", {{"full_name", "Fixture/base"}, {"id", 100}}},
                         {event, notification}},
                        [&, graph](const std::string& error) {
                            ci::require(
                                error == "No authorized assignment matches this notification" && persisted() == graph && effects == 0 &&
                                    notifications == 0 && runReads == 0,
                                "Late build run/job notification cannot be relabelled as a publish observation or change uncertainty");
                            finish("");
                        });
                } else if (scenario.starts_with("build_")) {
                    const auto selected = graph.at("executions").at(0);
                    observeRemote(selected);
                    controller.observe(
                        selected.at("id"),
                        [&](const std::string& error) {
                            const auto saved = persisted();
                            const auto& active = saved.at("executions").at(0).at("attempts").back();
                            ci::require(
                                error.ends_with("Invalid assigned build job") && active.at("state") == "building" &&
                                    active.at("uncertainty").at("kind") == "evidence" && active.at("uncertainty").at("stage") == "build" &&
                                    active.at("facts").at("build_run").at("status") == "completed" && notifications == 1 && effects == 0,
                                "Post-run job validation preserves valid run facts and requires evidence clearance");
                            finish("");
                        },
                        501);
                } else if (scenario == "diagnostic_ack") {
                    const auto selected = graph.at("executions").at(0);
                    observeRemote(selected);
                    controller.observe(
                        selected.at("id"),
                        [&, selected](const std::string& error) {
                            const auto saved = persisted();
                            const auto& active = saved.at("executions").at(0).at("attempts").back();
                            ci::require(!error.empty() && notifications == 2 && active.at("uncertainty").at("kind") == "read" &&
                                            active.at("facts").at("publish_send").at("outcome") == "accepted" &&
                                            !active.at("facts").contains("publish_run") &&
                                            active.at("assignment") == selected.at("attempts").back().at("assignment"),
                                        "A dispatch-ack committed after read start cannot suppress the attempt-bound failure");
                            finish("");
                        },
                        502);
                    store.change(
                        [](ci::Json& saved) {
                            saved["executions"][0]["attempts"].back()["facts"]["publish_send"] = {{"outcome", "accepted"}};
                            return ci::Json::array();
                        },
                        [&](const std::string& error) {
                            ci::require(error.empty() && bool(heldRead), "Read is held until acknowledgement commits");
                            std::exchange(heldRead, {})(
                                {}, "GET https://api.github.com/fixture response: connection closed before HTTP response 3/3");
                        });
                } else if (scenario.starts_with("diagnostic_order_")) {
                    const auto selected = graph.at("executions").at(0);
                    const bool failureLast = scenario.ends_with("SF");
                    for (unsigned int index = 0; index < 2; ++index) {
                        const bool failure = (index == 1) == failureLast;
                        store.change(
                            [selected, failure](ci::Json& saved) {
                                return ci::applyEvidence(saved,
                                                         selected,
                                                         "publish",
                                                         failure ? ci::Json()
                                                                 : fixture::observation(selected, "publish", "in_progress", ""),
                                                         {},
                                                         failure ? "Committed-order read failure" : "");
                            },
                            [&, index, failureLast, selected](const std::string& error) {
                                ci::require(error.empty(), "Store order commit");
                                if (index == 1) {
                                    const auto saved = persisted();
                                    const auto& active = saved.at("executions").at(0).at("attempts").back();
                                    ci::require(notifications == 2 && active.contains("uncertainty") == failureLast &&
                                                    active.at("state") == "publishing" &&
                                                    active.at("assignment") == selected.at("attempts").back().at("assignment"),
                                                "Diagnostic writes and clearances follow Store commit order with no authorization");
                                    finish("");
                                }
                            });
                    }
                } else if (scenario.starts_with("diagnostic_")) {
                    observeRemote(graph.at("executions").at(0));
                    diagnosticSequence(store, controller, graph.at("executions").at(0), notifications, finish, persisted);
                } else if (scenario.starts_with("resume_")) {
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
                                    for (const std::size_t i : {3U, 4U}) {
                                        ci::require(saved.at("executions").at(i) == original.at("executions").at(i),
                                                    "Unbound and terminal history must remain unchanged");
                                    }
                                }
                                if (scenario == "resume_mixed" || scenario == "resume_again" || scenario == "resume_unbound") {
                                    const auto index = scenario == "resume_mixed" ? 0U : scenario == "resume_again" ? 1U : 0U;
                                    const auto& awaiting = saved.at("executions").at(index).at("attempts").back();
                                    ci::require(
                                        notifications == (scenario == "resume_mixed" ? 2U : 1U) &&
                                            awaiting.at("uncertainty").at("kind") == "read" &&
                                            awaiting.at("uncertainty").at("reason") == "Assigned run remains unbound" &&
                                            !awaiting.at("facts").contains(scenario == "resume_again" ? "build_run" : "publish_run"),
                                        "Unbound startup observation commits only attempt-bound read uncertainty");
                                } else if (rejected) {
                                    ci::require(notifications == 0, "Unresolved DB commit cannot notify SSE");
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
                                ci::require(error.empty() != ((invalid && scenario != "wrong_receipt") || blocked ||
                                                              scenario == "missing_receipt" || scenario == "malformed_receipt"),
                                            "Unexpected observation outcome: " + error);
                                ci::require(reads == (complete || blocked || scenario == "missing_receipt" || scenario == "wrong_receipt" ||
                                                              scenario == "malformed_receipt"
                                                          ? 1U
                                                          : 0U),
                                            "Receipt fetched before required confirmation");
                                ci::require(effects == (complete ? 1U : 0U), "Unexpected dispatch count");
                                ci::require((active.at("state") == "published") == (complete || scenario == "unknown"),
                                            "Publication state must reflect confirmation and actual commit");
                                if (blocked) {
                                    ci::require(notifications == 0, "Rejected/uncertain DB change must not emit committed SSE");
                                } else if (invalid || scenario == "missing_receipt" || scenario == "malformed_receipt") {
                                    ci::require(notifications == 1 && active.contains("uncertainty") &&
                                                    active.at("uncertainty").at("stage") == "publish" &&
                                                    active.at("uncertainty").at("kind") ==
                                                        (scenario == "wrong_receipt" || scenario == "missing_receipt" ||
                                                                 scenario == "malformed_receipt"
                                                             ? "evidence"
                                                             : "read"),
                                                "Untrusted observations preserve facts and expose only guarded uncertainty");
                                    if (scenario != "wrong_receipt" && scenario != "missing_receipt" && scenario != "malformed_receipt") {
                                        ci::require(!active.at("facts").contains("publish_run"), "Early errors fabricate no run facts");
                                    }
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
                                    if (scenario != "resume_build" && scenario != "resume_build_running" &&
                                        !scenario.starts_with("build_")) {
                                        fixture::finishBuild(graph, graph.at("executions").at(0).at("id"));
                                    }
                                    if (scenario.starts_with("late_build_")) {
                                        auto& active = graph["executions"][0]["attempts"].back();
                                        active["uncertainty"] = {{"at", "2026-01-01T00:00:00Z"},
                                                                 {"stage", "publish"},
                                                                 {"kind", "evidence"},
                                                                 {"reason", "Real receipt evidence unavailable"}};
                                        if (scenario.ends_with("_bound")) {
                                            active["facts"]["publish_run_id"] = 502;
                                        }
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
