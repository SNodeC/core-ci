#include "Pipeline.h"

#include <Log.h>
#include <regex>
#include <web/http/http_utils.h>

namespace coreci {
    void Pipeline::log(const PipelineResult& result) {
        if (result.status >= 400) {
            snode::log::application().error() << "Pipeline: " << result.body.dump();
        }
    }
    void Pipeline::change(const std::function<Json(Json&)>& derive, const Callback& callback) {
        store.change(
            [derive](Json& state) {
                const auto value = derive(state);
                return Json{{"value", value}, {"dispatches", graph::advance(state)}};
            },
            [this, callback](const DatabaseResult& result, const Json& outcome) {
                if (result.error.empty()) {
                    dispatch(outcome.at("dispatches"));
                    callback({200, outcome.at("value")});
                } else {
                    callback({(result.uncertain || result.code) ? 503 : 409,
                              {{"error", result.error}, {"uncertain", result.uncertain}, {"assignment_preserved", true}}});
                }
            });
    }
    void Pipeline::fail(const std::string& id, const std::string& attempt, const std::string& reason) {
        change(
            [=](Json& state) {
                graph::failed(state.at("executions").at(id), attempt, reason);
                return Json{{"execution", id}};
            },
            log);
    }
    void Pipeline::request(
        const std::string& repository, const std::string& tag, bool automatic, const Callback& callback, const std::string& expectedTag) {
        const Version version(tag.empty() ? "0.0.0" : tag);
        const auto enrolled = store.graph().at("repositories");
        const auto root = repository.substr(repository.find('/') + 1);
        require(enrolled.contains(root) && enrolled.at(root).at("repository") == repository, "requested repository is not enrolled");
        struct Selection {
            Json sources = Json::object();
            std::map<std::string, std::string> errors;
            std::size_t pending;
        };
        const auto selection = std::make_shared<Selection>();
        selection->pending = enrolled.size();
        const auto since = store.graph().at("next_revision").get<std::uint64_t>();
        for (const auto& [name, row] : enrolled.items()) {
            sources.select(
                row.at("repository"),
                name == root && !automatic ? tag : "",
                [=, this](const SourceResult& result) {
                    try {
                        require(result.error.empty(), result.error);
                        auto selected = result.source;
                        require(selected.at("repository_id") == row.at("id"), "enrolled repository ID changed");
                        require(name != root || expectedTag.empty() || selected.at("tag") != tag ||
                                    selected.at("tag_object_sha") == expectedTag,
                                "tag changed after notification");
                        selected["deployment"] = config.deployment(app, github, row.at("branch"));
                        selection->sources[name] = selected;
                    } catch (const std::exception& error) {
                        selection->errors[name] = error.what();
                    }
                    if (--selection->pending == 0) {
                        if (!selection->errors.empty()) {
                            callback({503, {{"error", "selection blocked by unreadable repository"}, {"repositories", selection->errors}}});
                        } else {
                            change(
                                [=](Json& state) {
                                    require(state.at("repositories") == enrolled, "enrollment changed during source selection");
                                    return Json{{"created", graph::select(state, root, selection->sources, !automatic, since)}};
                                },
                                callback);
                        }
                    }
                },
                name == root && automatic ? tag : "");
        }
    }
    void Pipeline::retry(const std::string& execution, const Callback& callback) {
        change(
            [execution](Json& state) {
                graph::retry(state, execution);
                return Json{{"execution", execution}};
            },
            callback);
    }
    void Pipeline::dispatch(const Json& executions) {
        for (const auto& work : executions) {
            const auto id = work.at("execution").get<std::string>();
            const auto attemptId = work.at("attempt").get<std::string>();
            const bool publication = work.at("publication");
            const auto node = store.graph().at("executions").at(id);
            const auto assigned = node.at("attempts").back().at("assignment");
            const auto deployment = assigned.at("source").at("deployment");
            const auto workflow = deployment.at(publication ? "publication_workflow" : "source_workflow");
            const auto repository = publication ? deployment.at("worker").at("repository") : assigned.at("source").at("repository");
            sources.file(
                repository,
                workflow.at("ref"),
                workflow.at("path"),
                [=, this](const SourceResult& proof) {
                    if (!proof.error.empty() || proof.blobSha != workflow.at("blob_sha").get<std::string>()) {
                        fail(id, attemptId, "approved dispatch workflow is unavailable or changed");
                    } else {
                        store.change(
                            [=](Json& state) {
                                auto& attempt = state.at("executions").at(id).at("attempts").back();
                                require(attempt.at("id") == attemptId && attempt.at("state") == (publication ? "publishing" : "building"),
                                        "dispatch attempt changed before preflight commit");
                                attempt["preflight"][publication ? "publication" : "build"] = {{"commit", proof.source.at("commit")},
                                                                                               {"blob_sha", proof.blobSha}};
                                return graph::inputs(state.at("executions").at(id), publication);
                            },
                            [=, this](const DatabaseResult& committed, const Json& inputs) {
                                if (!committed.error.empty()) {
                                    log({503, {{"error", committed.error}, {"uncertain", committed.uncertain}}});
                                } else {
                                    const auto path = workflow.at("path").get<std::string>();
                                    github.request(
                                        repository,
                                        "POST",
                                        "/actions/workflows/" + httputils::url_encode(path.substr(path.rfind('/') + 1)) + "/dispatches",
                                        {{"ref", workflow.at("ref")}, {"inputs", inputs}},
                                        {{"actions", "write"}},
                                        [=, this](const GitHubResult& response) {
                                            if (response.status == 200 && response.error.empty() &&
                                                response.body.contains("workflow_run_id")) {
                                                store.change(
                                                    [=](Json& state) {
                                                        auto& attempt = state.at("executions").at(id).at("attempts").back();
                                                        require(attempt.at("id") == attemptId,
                                                                "dispatch response belongs to an older attempt");
                                                        auto& identity = attempt["dispatch"][publication ? "publication" : "build"];
                                                        require(identity.is_null() || identity == response.body.at("workflow_run_id"),
                                                                "conflicting dispatch run ID");
                                                        identity = response.body.at("workflow_run_id");
                                                        return Json();
                                                    },
                                                    [](const DatabaseResult& result, const Json&) {
                                                        if (!result.error.empty()) {
                                                            log({503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                                                        }
                                                    });
                                            } else if (!response.submitted || (response.status >= 400 && !response.uncertain())) {
                                                fail(id, attemptId, "GitHub definitively rejected dispatch");
                                            } else {
                                                log({503,
                                                     {{"error", "dispatch outcome is uncertain; assignment retained"}, {"execution", id}}});
                                            }
                                        });
                                }
                            });
                    }
                },
                false,
                "",
                false);
        }
    }
    Json Pipeline::facts(const Json& report, bool job) {
        Json result{{job ? "job_status" : "status", report.at("status")}};
        const auto conclusion = report.value("conclusion", Json());
        require(report.at("status") != "completed" || (conclusion.is_string() && !conclusion.get_ref<const std::string&>().empty()),
                "completed report has no conclusion");
        result[job ? "job_conclusion" : "conclusion"] = conclusion;
        if (job) {
            result["job_id"] = report.at("id");
            for (const auto* key : {"started_at", "completed_at", "html_url"}) {
                result[key] = report.value(key, Json());
            }
            for (const auto& step : report.value("steps", Json::array())) {
                if (step.at("name") == "Publish checked feed update" && !step.value("conclusion", Json()).is_null()) {
                    result["push_conclusion"] = step.at("conclusion");
                }
            }
        }
        return result;
    }
    void Pipeline::record(const std::string& id, const std::string& attemptId, bool publication, const Json& run, const Json& evidence) {
        change(
            [=](Json& state) {
                auto& node = state.at("executions").at(id);
                const auto before = node;
                graph::observe(node, attemptId, publication, run, evidence);
                return Json{{"changed", before != node}};
            },
            [=, this](const PipelineResult& result) {
                log(result);
                if (result.status == 200 && result.body.at("changed") &&
                    store.graph().at("executions").at(id).at("attempts").back().at("id") == attemptId) {
                    complete(id, publication);
                }
            });
    }
    void Pipeline::accept(const std::string& repository, const Json& report, bool job, const Json& packageJob) {
        try {
            const auto runId = report.at(job ? "run_id" : "id");
            require(report.at("run_attempt") == 1 && runId.is_number_unsigned() && runId.get<std::uint64_t>() > 0 &&
                        std::regex_match(report.at("head_sha").get<std::string>(), std::regex("^[a-f0-9]{40}$")),
                    "report does not identify an immutable attempt-one run");
            for (const auto& [id, node] : store.graph().at("executions").items()) {
                for (const auto& attempt : node.at("attempts")) {
                    if (!attempt.at("assignment").is_null()) {
                        const auto deployment = node.at("source").at("deployment");
                        for (const bool publication : {false, true}) {
                            const auto stage = publication ? "publication" : "build";
                            const auto expectedRepository =
                                publication ? deployment.at("worker").at("repository") : node.at("source").at("repository");
                            const auto& known = attempt.at(publication ? "publication_run" : "build_run");
                            const auto assignedId = attempt.value("dispatch", Json::object()).value(stage, Json());
                            const auto expected =
                                std::string(publication ? "Publish " : "Build ") + attempt.at("assignment").at("label").get<std::string>();
                            const auto jobMatches = [&](const Json& candidate) {
                                const auto name = candidate.at("name").get<std::string>();
                                return name == expected || name.ends_with(" / " + expected) ||
                                       (publication && candidate.at("status") == "completed" && candidate.at("conclusion") == "skipped" &&
                                        name == "Publish ${{ fromJSON(inputs.assignment).label }}");
                            };
                            const bool matches = (job ? jobMatches(report) : report.at("display_title") == expected) &&
                                                 (packageJob.is_null() || jobMatches(packageJob));
                            if (repository == expectedRepository.get<std::string>() && matches &&
                                (assignedId.is_null() || assignedId == runId) && (known.is_null() || known.at("id") == runId)) {
                                if (job && known.is_null()) {
                                    github.request(repository,
                                                   "GET",
                                                   "/actions/runs/" + runId.dump() + "/attempts/1",
                                                   nullptr,
                                                   {{"actions", "read"}},
                                                   [=, this](const GitHubResult& response) {
                                                       if (response.status == 200 && response.error.empty()) {
                                                           accept(repository, response.body, false, report);
                                                       }
                                                   });
                                } else if (job) {
                                    require(report.at("head_sha") == known.at("workflow_commit"),
                                            "job workflow commit differs from bound run");
                                    record(id, attempt.at("id"), publication, known, facts(report, true));
                                } else {
                                    const auto workflow = deployment.at(publication ? "publication_workflow" : "source_workflow");
                                    const auto repositoryId =
                                        publication ? deployment.at("worker").at("repository_id") : node.at("source").at("repository_id");
                                    require(report.at("repository").at("id") == repositoryId &&
                                                report.at("head_repository").at("id") == repositoryId &&
                                                report.at("actor").at("id") == deployment.at("controller_app_bot_id") &&
                                                report.at("event") == "workflow_dispatch" && report.at("path") == workflow.at("path") &&
                                                report.at("head_branch") == workflow.at("ref"),
                                            "run identity differs from assignment");
                                    const auto commit = report.at("head_sha").get<std::string>();
                                    auto evidence = facts(report, false);
                                    if (!packageJob.is_null()) {
                                        require(packageJob.at("run_id") == runId && packageJob.at("head_sha") == commit &&
                                                    packageJob.at("run_attempt") == 1,
                                                "job differs from authenticated run binding");
                                        evidence.update(facts(packageJob, true));
                                    }
                                    const Json identity{{"repository_id", repositoryId},
                                                        {"actor_id", deployment.at("controller_app_bot_id")},
                                                        {"id", runId},
                                                        {"run_attempt", 1},
                                                        {"workflow_commit", commit},
                                                        {"workflow_blob_sha", workflow.at("blob_sha")},
                                                        {"html_url", "https://github.com/" + repository + "/actions/runs/" + runId.dump()}};
                                    const auto proof = attempt.value("preflight", Json::object()).value(stage, Json());
                                    if (!known.is_null() || (!proof.is_null() && proof.at("commit") == commit &&
                                                             proof.at("blob_sha") == workflow.at("blob_sha"))) {
                                        record(id, attempt.at("id"), publication, identity, evidence);
                                    } else {
                                        sources.file(
                                            repository,
                                            commit,
                                            workflow.at("path"),
                                            [=, this](const SourceResult& result) {
                                                if (result.error.empty() && result.blobSha == workflow.at("blob_sha").get<std::string>()) {
                                                    record(id, attempt.at("id"), publication, identity, evidence);
                                                } else {
                                                    log({409, {{"error", "actual run workflow differs from approval"}, {"execution", id}}});
                                                }
                                            },
                                            false,
                                            "",
                                            false);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& error) {
            log({409, {{"error", error.what()}}});
        }
    }
    void Pipeline::complete(const std::string& id, bool publication) {
        const auto node = store.graph().at("executions").at(id);
        const auto attempt = node.at("attempts").back();
        const auto stage = publication ? "publication" : "build";
        const auto evidence = attempt.at("evidence").value(stage, Json::object());
        const auto key = attempt.at("id").get<std::string>() + ":" + stage;
        const bool active = attempt.at("state") == (publication ? "publishing" : "building");
        if (active && !completionReads.contains(key)) {
            const bool terminalRun = evidence.value("status", "") == "completed";
            const bool failedRun =
                terminalRun && !evidence.value("conclusion", "").empty() && evidence.value("conclusion", "") != "success";
            const bool skippedPush =
                evidence.value("push_conclusion", "") == "skipped" || evidence.value("job_conclusion", "") == "skipped";
            const bool successfulBuild = terminalRun && evidence.value("conclusion", "") == "success" &&
                                         evidence.value("job_status", "") == "completed" &&
                                         evidence.value("job_conclusion", "") == "success";
            if ((!publication && failedRun) || (publication && skippedPush)) {
                fail(id, attempt.at("id"), publication ? "verified publication failure before feed update" : "verified build failure");
            } else if ((publication && terminalRun && evidence.value("job_status", "") == "completed") ||
                       (!publication && successfulBuild && attempt.at("build_result").is_null())) {
                completionReads.insert(key);
                if (publication) {
                    feeds.verify(node, [=, this](const SourceResult& result) {
                        if (result.error.empty()) {
                            change(
                                [=](Json& state) {
                                    graph::published(state.at("executions").at(id), attempt.at("id"), result.source);
                                    return Json{{"execution", id}};
                                },
                                [=, this](const PipelineResult& outcome) {
                                    completionReads.erase(key);
                                    log(outcome);
                                });
                        } else {
                            completionReads.erase(key);
                            log({503, {{"error", result.error}, {"execution", id}, {"uncertain", true}}});
                            const auto& current = store.graph().at("executions").at(id).at("attempts").back();
                            if (current.at("id") == attempt.at("id") && current.at("evidence").value(stage, Json::object()) != evidence) {
                                complete(id, publication);
                            }
                        }
                    });
                } else {
                    const auto repository = node.at("source").at("repository").get<std::string>();
                    github.request(
                        repository,
                        "GET",
                        "/actions/runs/" + attempt.at("build_run").at("id").dump() + "/artifacts?per_page=100",
                        nullptr,
                        {{"actions", "read"}},
                        [=, this](const GitHubResult& response) {
                            try {
                                require(response.status == 200 && response.error.empty(), "build artifact evidence is inaccessible");
                                const auto& artifacts = response.body.at("artifacts");
                                require(artifacts.is_array() && artifacts.size() <= 100 &&
                                            response.body.at("total_count") == artifacts.size(),
                                        "artifact listing is incomplete or exceeds supported bound");
                                auto matching = Json::array();
                                for (const auto& artifact : artifacts) {
                                    if (artifact.at("name") == attempt.at("assignment").at("artifact_name")) {
                                        matching.push_back(artifact);
                                    }
                                }
                                const bool missing = matching.empty() || (matching.size() == 1 && matching.at(0).at("expired") == true);
                                Json result;
                                if (!missing) {
                                    require(matching.size() == 1, "assigned artifact identity is ambiguous");
                                    const auto& artifact = matching.at(0);
                                    static const std::regex sha256("^sha256:[a-f0-9]{64}$");
                                    require(artifact.at("expired") == false && artifact.at("id").get<std::uint64_t>() > 0 &&
                                                artifact.at("workflow_run").at("id") == attempt.at("build_run").at("id") &&
                                                artifact.at("size_in_bytes").get<std::uint64_t>() > 0 &&
                                                artifact.at("size_in_bytes").get<std::uint64_t>() <= 512 * 1024 * 1024 &&
                                                std::regex_match(artifact.at("digest").get<std::string>(), sha256) &&
                                                artifact.at("created_at") >= evidence.at("started_at") &&
                                                artifact.at("created_at") <= evidence.at("completed_at"),
                                            "artifact does not bind to the assigned attempt-one job");
                                    result = {{"run", attempt.at("build_run")},
                                              {"assignment_sha256", digest(attempt.at("assignment"))},
                                              {"build_job",
                                               {{"started_at", evidence.at("started_at")}, {"completed_at", evidence.at("completed_at")}}},
                                              {"artifacts",
                                               {{{"id", artifact.at("id")},
                                                 {"name", artifact.at("name")},
                                                 {"digest", artifact.at("digest")},
                                                 {"size_bytes", artifact.at("size_in_bytes")},
                                                 {"job_id", evidence.at("job_id")}}}}};
                                }
                                change(
                                    [=](Json& state) {
                                        auto& current = state.at("executions").at(id);
                                        if (missing) {
                                            graph::failed(current, attempt.at("id"), "required artifacts missing/expired");
                                        } else {
                                            graph::built(current, attempt.at("id"), result);
                                        }
                                        return Json{{"execution", id}};
                                    },
                                    [=, this](const PipelineResult& outcome) {
                                        completionReads.erase(key);
                                        log(outcome);
                                    });
                            } catch (const std::exception& error) {
                                completionReads.erase(key);
                                log({503, {{"error", error.what()}, {"execution", id}, {"assignment_preserved", true}}});
                            }
                        });
                }
            }
        }
    }
    void Pipeline::event(const std::string& type, const std::string& signature, const std::string& raw, const Callback& callback) {
        Json body;
        std::string repository, error;
        try {
            require(github.verifyWebhook(raw, signature), "invalid raw-body webhook HMAC");
            body = Json::parse(raw);
            require(type == "workflow_run" || type == "workflow_job" || type == "push" || type == "ping", "unsupported webhook event");
            if (type != "ping") {
                require(body.at("installation").at("id") == app.installationId, "webhook installation differs from configured App");
                repository = body.at("repository").at("full_name").get<std::string>();
                require(body.at("repository").at("id") == github.repositoryId(repository), "webhook repository ID differs from grant");
                if (type != "push") {
                    const auto action = body.at("action").get<std::string>();
                    require(action == "requested" || action == "queued" || action == "in_progress" || action == "completed" ||
                                action == "waiting",
                            "unsupported workflow event action");
                    require(body.at(type).at("run_attempt").is_number_unsigned() && body.at(type).at("status").is_string(),
                            "invalid workflow report");
                } else {
                    require(body.at("ref").is_string() && body.at("after").is_string() && body.at("deleted").is_boolean(),
                            "invalid tag notification");
                }
            }
        } catch (const std::exception& failure) {
            error = failure.what();
        }
        callback({error.empty() ? 202 : 400, error.empty() ? Json{{"event", "accepted"}} : Json{{"error", error}}});
        if (error.empty()) {
            try {
                require(store.mutationsAllowed(), "database mutation authority is unavailable");
                if (type == "workflow_run" || type == "workflow_job") {
                    accept(repository, body.at(type), type == "workflow_job");
                } else if (type == "push" && body.at("ref").get<std::string>().starts_with("refs/tags/") &&
                           !body.at("deleted").get<bool>()) {
                    request(repository, body.at("ref").get<std::string>().substr(10), true, log, body.at("after"));
                }
            } catch (const std::exception& failure) {
                log({503, {{"error", failure.what()}}});
            }
        }
    }
    void Pipeline::inspect(const std::string& id, const Callback& callback) {
        const auto node = store.graph().at("executions").at(id);
        const auto attempt = node.at("attempts").back();
        const bool publication = attempt.at("state") == "publishing";
        const auto stage = publication ? "publication" : "build";
        const auto deployment = node.at("source").at("deployment");
        const auto repository = publication ? deployment.at("worker").at("repository") : node.at("source").at("repository");
        const auto known = attempt.at(publication ? "publication_run" : "build_run");
        const auto runId = known.is_null() ? attempt.value("dispatch", Json::object()).value(stage, Json()) : known.at("id");
        const auto read = [=, this](const Json& identity) {
            const auto prefix = "/actions/runs/" + identity.dump() + "/attempts/1";
            github.request(repository, "GET", prefix, nullptr, {{"actions", "read"}}, [=, this](const GitHubResult& response) {
                if (response.status == 200 && response.error.empty()) {
                    accept(repository, response.body, false);
                    github.request(repository,
                                   "GET",
                                   prefix + "/jobs?per_page=100",
                                   nullptr,
                                   {{"actions", "read"}},
                                   [=, this](const GitHubResult& jobs) {
                                       if (jobs.status == 200 && jobs.error.empty() && jobs.body.contains("jobs") &&
                                           jobs.body.at("jobs").is_array() && jobs.body.at("jobs").size() <= 100 &&
                                           jobs.body.at("total_count") == jobs.body.at("jobs").size()) {
                                           for (const auto& job : jobs.body.at("jobs")) {
                                               accept(repository, response.body, false, job);
                                           }
                                           complete(id, publication);
                                       }
                                       callback({202, {{"execution", id}, {"assignment_preserved", true}}});
                                   });
                } else {
                    callback(
                        {503,
                         {{"error", "assigned attempt-one evidence is inaccessible"}, {"execution", id}, {"assignment_preserved", true}}});
                }
            });
        };
        if (runId.is_null()) {
            const auto path = deployment.at(publication ? "publication_workflow" : "source_workflow").at("path").get<std::string>();
            github.request(repository,
                           "GET",
                           "/actions/workflows/" + httputils::url_encode(path.substr(path.rfind('/') + 1)) +
                               "/runs?event=workflow_dispatch&per_page=100",
                           nullptr,
                           {{"actions", "read"}},
                           [=](const GitHubResult& response) {
                               auto matches = Json::array();
                               if (response.status == 200 && response.error.empty() && response.body.is_object() &&
                                   response.body.value("workflow_runs", Json()).is_array()) {
                                   for (const auto& run : response.body.at("workflow_runs")) {
                                       if (run.is_object() && run.value("actor", Json()).is_object() &&
                                           run.at("actor").value("id", Json()) == deployment.at("controller_app_bot_id") &&
                                           run.value("display_title", Json()) ==
                                               std::string(publication ? "Publish " : "Build ") +
                                                   attempt.at("assignment").at("label").get<std::string>()) {
                                           matches.push_back(run);
                                       }
                                   }
                               }
                               if (matches.size() == 1 && matches.at(0).value("id", Json()).is_number_unsigned()) {
                                   read(matches.at(0).at("id"));
                               } else {
                                   callback({202, {{"execution", id}, {"assignment_preserved", true}}});
                               }
                           });
        } else {
            read(runId);
        }
    }
    void Pipeline::reconcile(bool restored, const Callback& callback) {
        change(
            [restored](Json& state) {
                auto ids = Json::array();
                for (auto& [id, node] : state.at("executions").items()) {
                    const auto& attempt = node.at("attempts").back();
                    const bool publication = attempt.at("state") == "publishing";
                    if (publication || (attempt.at("state") == "building" && attempt.at("build_result").is_null())) {
                        const auto stage = publication ? "publication" : "build";
                        if (restored && !attempt.value("preflight", Json::object()).contains(stage) &&
                            attempt.value("dispatch", Json::object()).value(stage, Json()).is_null() &&
                            attempt.at(publication ? "publication_run" : "build_run").is_null()) {
                            graph::failed(node, attempt.at("id"), "workflow not submitted; dispatch preflight absent");
                        } else {
                            ids.push_back(id);
                        }
                    }
                }
                return ids;
            },
            [this, callback](const PipelineResult& result) {
                const auto pending = std::make_shared<std::size_t>(result.body.size());
                if (result.status != 200 || result.body.empty()) {
                    callback(result.status == 200 ? PipelineResult{200, {{"reconciled", 0}}} : result);
                } else {
                    for (const auto& id : result.body) {
                        inspect(id, [pending, callback, count = result.body.size()](const PipelineResult& observed) {
                            log(observed);
                            if (--*pending == 0) {
                                callback({200, {{"reconciled", count}}});
                            }
                        });
                    }
                }
            });
    }
    void Pipeline::refreshStatus() {
        const auto generation = store.graph().at("generation").get<std::uint64_t>();
        if (!config.statusWorkflow.empty() && !refreshing && generation != statusSent) {
            refreshing = generation;
            sources.file(
                config.workerRepository,
                config.publicationRef,
                config.statusWorkflow,
                [=, this](const SourceResult& proof) {
                    if (!proof.error.empty() || proof.blobSha != config.statusWorkflowBlob) {
                        refreshing.reset();
                        log({503, {{"error", "approved public status workflow is unavailable"}}});
                    } else {
                        const auto path = config.statusWorkflow.substr(config.statusWorkflow.rfind('/') + 1);
                        github.request(config.workerRepository,
                                       "POST",
                                       "/actions/workflows/" + httputils::url_encode(path) + "/dispatches",
                                       {{"ref", config.publicationRef}, {"inputs", {{"generation", std::to_string(generation)}}}},
                                       {{"actions", "write"}},
                                       [=, this](const GitHubResult& response) {
                                           statusSent = generation;
                                           refreshing.reset();
                                           if (!response.error.empty() || (response.status != 200 && response.status != 204)) {
                                               log({503, {{"error", "public status dispatch is unconfirmed; not replayed"}}});
                                           }
                                           if (store.graph().at("generation") != generation) {
                                               refreshStatus();
                                           }
                                       });
                    }
                },
                false,
                "",
                false);
        }
    }
} // namespace coreci
