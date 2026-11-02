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

    void observeRemote(const ci::Json& work) {
        const auto& command = work.at("attempts").back().at("publish_assignment");
        remoteRun = {{"id", 502},
                     {"repository", {{"id", command.at("repository_id")}}},
                     {"head_repository", {{"id", command.at("repository_id")}}},
                     {"actor", {{"id", 41}}},
                     {"event", "workflow_dispatch"},
                     {"run_attempt", 1},
                     {"head_sha", command.at("commit")},
                     {"path", command.at("path")},
                     {"display_title", "Publish " + work.at("attempts").back().at("assignment").at("label").get<std::string>()},
                     {"status", "in_progress"},
                     {"conclusion", nullptr},
                     {"html_url", "https://github.com/Fixture/worker/actions/runs/502"}};
        remoteJobs = fixture::observation(work, "publish").at("jobs");
        ci::Json confirm = remoteJobs.at(0);
        confirm["id"] = 703;
        confirm["name"] = "Confirm public receipt";
        if (scenario == "queued" || scenario == "running") {
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
        require(field == "jobs" && path.ends_with("/attempts/1/jobs"), "Unexpected pagination");
        done(remoteJobs, "");
    }

    void Controller::contents(const std::string&, const std::string&, const std::string& path, const std::string&, const Value& done) {
        if (path.starts_with(".github/workflows/")) {
            done({{"sha", std::string(40, scenario == "wrong_blob" ? 'd' : 'c')}}, "");
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
    ci::Controller controller(store, fixture::deployment(), "fixture", "fixture", "unused");
    int result = 0;
    bool started = false;
    std::size_t effects = 0;
    std::size_t notifications = 0;
    scenario = std::getenv("CI_PUBLICATION_CASE");
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
                            graph.at("executions").at(0).at("attempts").back().at("state") == "published" &&
                            input.at("attempt") == graph.at("executions").at(0).at("attempts").back().at("id") &&
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
                if (scenario == "restart") {
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
                                    fixture::finishBuild(graph, graph.at("executions").at(0).at("id"));
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
