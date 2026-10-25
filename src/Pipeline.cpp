#include "Pipeline.h"

#include "Version.h"

#include <algorithm>
#include <regex>
#include <stdexcept>
#include <web/http/http_utils.h>

namespace coreci {
    namespace {
        void require(bool condition, const std::string& message) {
            if (!condition) {
                throw std::invalid_argument(message);
            }
        }
    } // namespace

    Pipeline::Pipeline(const PipelineConfig& config,
                       const GitHubConfig& app,
                       GitHub& github,
                       SourceReader& sources,
                       EnvironmentResolver& environments,
                       Enrollment& enrollment,
                       GraphStore& store)
        : config(config)
        , app(app)
        , github(github)
        , sources(sources)
        , environments(environments)
        , enrollment(enrollment)
        , store(store) {
    }

    void Pipeline::request(const std::string& repository,
                           const std::string& tag,
                           bool automatic,
                           const Callback& callback,
                           const std::string& expectedTagObject) {
        const auto selection = std::make_shared<Selecting>();
        selection->repository = repository;
        selection->tag = tag;
        selection->expectedTagObject = expectedTagObject;
        selection->automatic = automatic;
        selection->generation = store.getGeneration();
        selection->callback = callback;
        enrollment.list([this, selection](const EnrollmentResult& result) {
            if (result.status != 200) {
                selection->callback({result.status, result.body});
            } else {
                try {
                    selection->repositories = result.body.at("repositories").get<std::vector<nlohmann::json>>();
                    for (const auto& row : selection->repositories) {
                        if (row.at("repository") == selection->repository) {
                            selection->root = row.at("name").get<std::string>();
                        }
                        config.deployment(github, app, row);
                    }
                    require(!selection->root.empty(), "requested source repository is not enrolled");
                    readSources(selection);
                } catch (const std::exception& error) {
                    selection->callback({422, {{"error", error.what()}}});
                }
            }
        });
    }

    void Pipeline::readSources(const std::shared_ptr<Selecting>& selection) {
        if (selection->index == selection->repositories.size()) {
            try {
                if (!selection->sources.contains(selection->root) && selection->tag.empty()) {
                    selection->callback({200, {{"selected", 0}, {"reason", "repository has no semantic-version tag"}}});
                } else {
                    selection->order = Graph::traverseSources(selection->root, selection->sources).order;
                    selection->index = 0;
                    resolveEnvironments(selection);
                }
            } catch (const std::exception& error) {
                selection->callback({422, {{"error", error.what()}}});
            }
        } else {
            const auto row = selection->repositories.at(selection->index);
            const auto repository = row.at("repository").get<std::string>();
            sources.select(
                repository, repository == selection->repository ? selection->tag : "", [this, selection, row](const SourceResult& result) {
                    if (result.status == 404 && result.source.contains("repository_id")) {
                        ++selection->index;
                        readSources(selection);
                    } else if (result.status != 200) {
                        selection->callback({result.status, {{"error", result.error}, {"repository", row.at("repository")}}});
                    } else {
                        try {
                            require(result.source.at("repository_id") == row.at("id"), "selected source ID differs from enrollment");
                            require(row.at("repository") != selection->repository || selection->expectedTagObject.empty() ||
                                        result.source.at("commit") == selection->expectedTagObject ||
                                        result.source.at("tag_object_sha") == selection->expectedTagObject,
                                    "tag moved after this delivery; retain existing selections and process its current event");
                            auto source = result.source;
                            source["deployment"] = config.deployment(github, app, row);
                            selection->sources.emplace(row.at("name").get<std::string>(), std::move(source));
                            ++selection->index;
                            readSources(selection);
                        } catch (const std::exception& error) {
                            selection->callback({422, {{"error", error.what()}}});
                        }
                    }
                });
        }
    }

    void Pipeline::resolveEnvironments(const std::shared_ptr<Selecting>& selection) {
        if (selection->index == selection->order.size()) {
            try {
                require(selection->generation == store.getGeneration(), "graph changed during selection; request it again");
                const auto change = store.graph().select(selection->root, selection->sources, !selection->automatic);
                store.commit(change, [this, selection, count = change.nodes.size()](const DatabaseResult& result) {
                    if (!result.error.empty()) {
                        selection->callback({503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                    } else {
                        advance([selection, count](const PipelineResult& dispatched) {
                            auto response = dispatched.body;
                            response["selected"] = count;
                            selection->callback({dispatched.status, response});
                        });
                    }
                });
            } catch (const std::exception& error) {
                selection->callback({409, {{"error", error.what()}}});
            }
        } else {
            const auto name = selection->order.at(selection->index);
            environments.resolve(selection->sources.at(name), [this, selection, name](const EnvironmentResult& result) {
                if (!result.error.empty()) {
                    selection->callback({422, {{"error", result.error}, {"repository", name}}});
                } else {
                    selection->sources.at(name) = result.source;
                    ++selection->index;
                    resolveEnvironments(selection);
                }
            });
        }
    }

    void Pipeline::event(const std::string& type, const std::string& signature, const std::string& body, const Callback& callback) {
        if (!github.verifyWebhook(body, signature)) {
            callback({401, {{"error", "invalid GitHub raw-body webhook signature"}}});
        } else {
            try {
                require(body.size() <= 2 * 1024 * 1024, "GitHub webhook exceeds the configured body bound");
                const auto delivery = nlohmann::json::parse(body);
                if (type == "ping") {
                    callback({200, {{"event", "ping verified"}}});
                } else {
                    const auto repository = delivery.at("repository").at("full_name").get<std::string>();
                    require(delivery.at("installation").at("id") == app.installationId &&
                                delivery.at("repository").at("id") == github.repositoryId(repository),
                            "webhook installation/repository differs from configured grants");
                    if (type == "push") {
                        const auto ref = delivery.at("ref").get<std::string>();
                        bool versionTag = ref.starts_with("refs/tags/") && !delivery.value("deleted", false);
                        if (versionTag) {
                            try {
                                const Version version(ref.substr(10));
                            } catch (const std::invalid_argument&) {
                                versionTag = false;
                            }
                        }
                        if (versionTag) {
                            request(repository, ref.substr(10), true, callback, delivery.at("after").get<std::string>());
                        } else {
                            callback({202, {{"event", "no version selection required"}}});
                        }
                    } else if (type == "workflow_run" || type == "workflow_job") {
                        const auto& report = delivery.at(type);
                        const auto runId = report.at(type == "workflow_job" ? "run_id" : "id").get<std::uint64_t>();
                        std::string selected;
                        std::string attemptId;
                        bool publication = false;
                        for (const auto& [id, node] : store.graph().nodes()) {
                            for (const auto& attempt : node.attempts) {
                                if (!attempt.assignment.is_null()) {
                                    for (const bool publishing : {false, true}) {
                                        const auto expectedRepository = publishing
                                                                            ? node.source.at("deployment").at("worker").at("repository")
                                                                            : node.source.at("repository");
                                        const auto& assigned = publishing ? attempt.publicationRun : attempt.buildRun;
                                        const auto expectedJob =
                                            std::string("core-ci ") + (publishing ? "publish " : "package ") + attempt.id;
                                        const auto jobName = type == "workflow_job" ? report.value("name", std::string{}) : "";
                                        const bool matches =
                                            (!assigned.is_null() && assigned.at("id") == runId) ||
                                            (type == "workflow_run" &&
                                             report.value("display_title", std::string{}) == title(attempt, publishing)) ||
                                            (type == "workflow_job" && (jobName == expectedJob || jobName.ends_with(" / " + expectedJob)));
                                        if (expectedRepository == repository && matches) {
                                            require(selected.empty(), "webhook matches multiple assignments");
                                            selected = id;
                                            attemptId = attempt.id;
                                            publication = publishing;
                                        }
                                    }
                                }
                            }
                        }
                        if (selected.empty()) {
                            callback({202, {{"event", "unassigned execution ignored"}}});
                        } else {
                            inspect(selected, attemptId, publication, runId, callback);
                        }
                    } else {
                        callback({422, {{"error", "unsupported GitHub webhook event"}}});
                    }
                }
            } catch (const std::exception& error) {
                callback({422, {{"error", error.what()}}});
            }
        }
    }

    void Pipeline::retry(const std::string& execution, const Callback& callback) {
        try {
            store.commit(store.graph().retry(execution), [this, callback](const DatabaseResult& result) {
                if (!result.error.empty()) {
                    callback({503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                } else {
                    advance(callback);
                }
            });
        } catch (const std::exception& error) {
            callback({409, {{"error", error.what()}}});
        }
    }

    std::string Pipeline::title(const Attempt& attempt, bool publication) {
        return std::string("core-ci ") + (publication ? "publish " : "build ") + attempt.id + " " + Graph::digest(attempt.assignment);
    }

    void Pipeline::advance(const Callback& callback) {
        try {
            GraphChange change;
            change.nextRevision = store.graph().nextRebuildRevision();
            for (const auto& [id, node] : store.graph().nodes()) {
                if (store.graph().blockers(id).empty()) {
                    const auto assigned = store.graph().beginBuild(id);
                    require(assigned.attempts.back().assignment.dump().size() <= 60000,
                            "immutable assignment exceeds the supported GitHub dispatch input bound");
                    change.nodes.push_back(assigned);
                }
            }
            if (change.nodes.empty()) {
                callback({200, {{"assigned", 0}}});
            } else {
                store.commit(change, [this, nodes = change.nodes, callback](const DatabaseResult& result) {
                    if (!result.error.empty()) {
                        callback({503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                    } else {
                        auto responses = std::make_shared<nlohmann::json>(nlohmann::json::array());
                        for (const auto& node : nodes) {
                            dispatch(node, false, [responses, count = nodes.size(), callback](const PipelineResult& dispatched) {
                                responses->push_back(dispatched.body);
                                if (responses->size() == count) {
                                    callback({202, {{"assigned", count}, {"dispatches", *responses}}});
                                }
                            });
                        }
                    }
                });
            }
        } catch (const std::exception& error) {
            callback({422, {{"error", error.what()}}});
        }
    }

    void Pipeline::workflow(const std::string& repository, const nlohmann::json& definition, const SourceReader::Callback& callback) {
        github.request(repository,
                       "GET",
                       "/commits/" + httputils::url_encode(definition.at("ref").get<std::string>()),
                       nullptr,
                       {{"contents", "read"}},
                       [this, repository, definition, callback](const GitHubResult& result) {
                           try {
                               require(result.status == 200 && result.error.empty(),
                                       "could not resolve the assigned workflow dispatch ref");
                               sources.file(repository,
                                            result.body.at("sha").get<std::string>(),
                                            definition.at("path").get<std::string>(),
                                            [definition, callback](const SourceResult& file) {
                                                if (file.status != 200 || file.blobSha != definition.at("blob_sha").get<std::string>()) {
                                                    callback({422, nullptr, "", "", "workflow file does not match deployment approval"});
                                                } else {
                                                    callback(file);
                                                }
                                            });
                           } catch (const std::exception& error) {
                               callback({422, nullptr, "", "", error.what()});
                           }
                       });
    }

    void Pipeline::dispatch(const Node& node, bool publication, const Callback& callback) {
        const auto attempt = node.attempts.back();
        const auto& deployment = node.source.at("deployment");
        const auto definition = deployment.at(publication ? "publication_workflow" : "source_workflow");
        const auto repository =
            publication ? deployment.at("worker").at("repository").get<std::string>() : node.source.at("repository").get<std::string>();
        workflow(
            repository, definition, [this, node, attempt, publication, repository, definition, callback](const SourceResult& approved) {
                if (approved.status != 200) {
                    try {
                        change(store.graph().rejectedDispatch(node.id, attempt.id, publication, 0, false, approved.error), callback);
                    } catch (const std::exception& error) {
                        callback({409, {{"error", error.what()}}});
                    }
                } else {
                    nlohmann::json inputs{{"execution", node.id},
                                          {"attempt", attempt.id},
                                          {"assignment_sha256", Graph::digest(attempt.assignment)},
                                          {"assignment", attempt.assignment.dump()}};
                    if (publication) {
                        inputs["build_result"] = attempt.buildResult.dump();
                    }
                    const auto filename = definition.at("path").get<std::string>().substr(std::string(".github/workflows/").size());
                    github.request(
                        repository,
                        "POST",
                        "/actions/workflows/" + httputils::url_encode(filename) + "/dispatches",
                        {{"ref", definition.at("ref")}, {"inputs", inputs}},
                        {{"actions", "write"}},
                        [this, node, attempt, publication, callback](const GitHubResult& result) {
                            if (!result.submitted || (result.status >= 400 && result.status < 500 && result.status != 408)) {
                                try {
                                    const auto reason = result.error.empty()
                                                            ? "GitHub rejected dispatch (HTTP " + std::to_string(result.status) + ")"
                                                            : result.error;
                                    change(store.graph().rejectedDispatch(
                                               node.id, attempt.id, publication, result.status, result.submitted, reason),
                                           callback);
                                } catch (const std::exception& error) {
                                    callback({409, {{"error", error.what()}}});
                                }
                            } else if (result.status == 200 && result.error.empty() && result.body.contains("workflow_run_id") &&
                                       result.body.at("workflow_run_id").is_number_unsigned()) {
                                inspect(node.id, attempt.id, publication, result.body.at("workflow_run_id").get<std::uint64_t>(), callback);
                            } else {
                                callback({202,
                                          {{"execution", node.id},
                                           {"attempt", attempt.id},
                                           {"dispatch", "assigned; reconcile GitHub run identity"}}});
                            }
                        });
                }
            });
    }

    void Pipeline::inspect(
        const std::string& execution, const std::string& attemptId, bool publication, std::uint64_t runId, const Callback& callback) {
        try {
            const auto node = store.graph().node(execution);
            const auto found = std::find_if(node.attempts.begin(), node.attempts.end(), [&](const auto& attempt) {
                return attempt.id == attemptId;
            });
            require(found != node.attempts.end() && !found->assignment.is_null() && runId > 0, "run does not match assigned work");
            const auto attempt = *found;
            const auto& deployment = node.source.at("deployment");
            const auto repository =
                publication ? deployment.at("worker").at("repository").get<std::string>() : node.source.at("repository").get<std::string>();
            const auto expectedRepository = publication ? deployment.at("worker").at("repository_id") : node.source.at("repository_id");
            const auto workflow = deployment.at(publication ? "publication_workflow" : "source_workflow");
            github.request(
                repository,
                "GET",
                "/actions/runs/" + std::to_string(runId),
                nullptr,
                {{"actions", "read"}},
                [this, execution, attemptId, publication, runId, node, attempt, repository, expectedRepository, workflow, callback](
                    const GitHubResult& result) {
                    try {
                        require(result.status == 200 && result.error.empty(),
                                "assigned GitHub result is inaccessible; preserve its assignment");
                        const auto run = result.body;
                        require(run.at("id") == runId && run.at("run_attempt") == 1 && run.at("event") == "workflow_dispatch" &&
                                    run.at("repository").at("id") == expectedRepository &&
                                    run.at("head_repository").at("id") == expectedRepository &&
                                    run.at("actor").at("id") == node.source.at("deployment").at("controller_app_bot_id") &&
                                    run.at("display_title") == title(attempt, publication) && run.at("path") == workflow.at("path"),
                                "GitHub run identity/event/actor/workflow does not match assignment");
                        sources.file(
                            repository,
                            run.at("head_sha").get<std::string>(),
                            workflow.at("path").get<std::string>(),
                            [this, execution, attemptId, publication, node, run, workflow, callback](const SourceResult& file) {
                                try {
                                    require(file.status == 200 && file.blobSha == workflow.at("blob_sha").get<std::string>(),
                                            "GitHub executed a workflow outside the approved blob identity");
                                    const auto& current = store.graph().node(execution);
                                    if (current.attempts.back().id != attemptId || current.attempts.back().state == "published" ||
                                        current.attempts.back().state == "failed") {
                                        callback({200, {{"execution", execution}, {"report", "late or terminal; history retained"}}});
                                    } else {
                                        const nlohmann::json identity{{"id", run.at("id")},
                                                                      {"run_attempt", run.at("run_attempt")},
                                                                      {"repository_id", run.at("repository").at("id")},
                                                                      {"actor_id", run.at("actor").at("id")},
                                                                      {"workflow_blob_sha", file.blobSha},
                                                                      {"workflow_commit", run.at("head_sha")},
                                                                      {"html_url", run.at("html_url")}};
                                        const auto bound = store.graph().bindRun(execution, attemptId, publication, identity);
                                        change(bound,
                                               [this, execution, attemptId, publication, run, callback](const PipelineResult& stored) {
                                                   if (stored.status != 200) {
                                                       callback(stored);
                                                   } else if (run.at("status") != "completed") {
                                                       callback(stored);
                                                   } else if (run.at("conclusion") != "success") {
                                                       try {
                                                           change(store.graph().failed(execution,
                                                                                       attemptId,
                                                                                       "GitHub execution completed: " +
                                                                                           run.at("conclusion").get<std::string>()),
                                                                  callback);
                                                       } catch (const std::exception& error) {
                                                           callback({409, {{"error", error.what()}}});
                                                       }
                                                   } else if (!publication) {
                                                       buildArtifacts(store.graph().node(execution), callback);
                                                   } else {
                                                       callback({202,
                                                                 {{"execution", execution},
                                                                  {"state", "publishing"},
                                                                  {"report", "awaiting verified feed publication receipt"}}});
                                                   }
                                               });
                                    }
                                } catch (const std::exception& error) {
                                    callback({409, {{"error", error.what()}}});
                                }
                            });
                    } catch (const std::exception& error) {
                        callback({503, {{"error", error.what()}, {"execution", execution}, {"assignment_preserved", true}}});
                    }
                });
        } catch (const std::exception& error) {
            callback({409, {{"error", error.what()}}});
        }
    }

    void Pipeline::buildArtifacts(const Node& node, const Callback& callback) {
        const auto attempt = node.attempts.back();
        const auto repository = node.source.at("repository").get<std::string>();
        const auto runId = attempt.buildRun.at("id").get<std::uint64_t>();
        github.request(
            repository,
            "GET",
            "/actions/runs/" + std::to_string(runId) + "/jobs?per_page=100",
            nullptr,
            {{"actions", "read"}},
            [this, node, attempt, repository, runId, callback](const GitHubResult& jobs) {
                try {
                    require(jobs.status == 200 && jobs.error.empty() && jobs.body.at("total_count").get<unsigned int>() <= 100,
                            "assigned GitHub jobs are unavailable or exceed supported bounds");
                    nlohmann::json job;
                    const auto expected = "core-ci package " + attempt.id;
                    for (const auto& candidate : jobs.body.at("jobs")) {
                        const auto name = candidate.at("name").get<std::string>();
                        if (name == expected || name.ends_with(" / " + expected)) {
                            require(job.is_null(), "assigned package job is ambiguous");
                            job = candidate;
                        }
                    }
                    require(!job.is_null() && job.at("run_id") == runId && job.at("run_attempt") == 1 && job.at("status") == "completed" &&
                                job.at("conclusion") == "success",
                            "assigned package job did not complete successfully");
                    github.request(
                        repository,
                        "GET",
                        "/actions/runs/" + std::to_string(runId) + "/artifacts?per_page=100",
                        nullptr,
                        {{"actions", "read"}},
                        [this, node, attempt, job, runId, callback](const GitHubResult& artifacts) {
                            try {
                                require(artifacts.status == 200 && artifacts.error.empty() &&
                                            artifacts.body.at("total_count").get<unsigned int>() <= 100,
                                        "assigned GitHub artifacts are unavailable or exceed supported bounds");
                                nlohmann::json artifact;
                                for (const auto& candidate : artifacts.body.at("artifacts")) {
                                    if (candidate.at("name") == "core-ci-build-" + attempt.id) {
                                        require(artifact.is_null(), "assigned build artifact is ambiguous");
                                        artifact = candidate;
                                    }
                                }
                                static const std::regex digest(R"(^sha256:[a-f0-9]{64}$)");
                                require(!artifact.is_null() && artifact.at("id").get<std::uint64_t>() > 0 &&
                                            !artifact.at("expired").get<bool>() && artifact.at("workflow_run").at("id") == runId &&
                                            artifact.at("size_in_bytes").get<std::uint64_t>() <= 1024ULL * 1024 * 1024 &&
                                            std::regex_match(artifact.at("digest").get<std::string>(), digest),
                                        "assigned artifact lacks a bounded immutable GitHub identity");
                                const nlohmann::json selected{{"id", artifact.at("id")},
                                                              {"name", artifact.at("name")},
                                                              {"digest", artifact.at("digest")},
                                                              {"size_bytes", artifact.at("size_in_bytes")},
                                                              {"run_id", runId},
                                                              {"job_id", job.at("id")},
                                                              {"run_attempt", 1}};
                                const auto result = nlohmann::json{{"run", attempt.buildRun},
                                                                   {"assignment_sha256", Graph::digest(attempt.assignment)},
                                                                   {"artifacts", {selected}}};
                                change(store.graph().built(node.id, attempt.id, result), callback);
                            } catch (const std::exception& error) {
                                callback({503, {{"error", error.what()}, {"execution", node.id}, {"assignment_preserved", true}}});
                            }
                        });
                } catch (const std::exception& error) {
                    callback({503, {{"error", error.what()}, {"execution", node.id}, {"assignment_preserved", true}}});
                }
            });
    }

    void Pipeline::search(const std::string& execution,
                          const std::string& attemptId,
                          bool publication,
                          unsigned int page,
                          const std::shared_ptr<std::vector<std::uint64_t>>& matches,
                          const Callback& callback) {
        const auto node = store.graph().node(execution);
        const auto& attempt = node.attempts.back();
        const auto& deployment = node.source.at("deployment");
        const auto repository =
            publication ? deployment.at("worker").at("repository").get<std::string>() : node.source.at("repository").get<std::string>();
        const auto workflow = deployment.at(publication ? "publication_workflow" : "source_workflow").at("path").get<std::string>();
        const auto filename = workflow.substr(std::string(".github/workflows/").size());
        github.request(repository,
                       "GET",
                       "/actions/workflows/" + httputils::url_encode(filename) +
                           "/runs?event=workflow_dispatch&per_page=100&page=" + std::to_string(page),
                       nullptr,
                       {{"actions", "read"}},
                       [this, execution, attemptId, publication, page, attempt, matches, callback](const GitHubResult& result) {
                           try {
                               require(result.status == 200 && result.error.empty(),
                                       "assigned GitHub executions are inaccessible; no replacement authorized");
                               const auto& runs = result.body.at("workflow_runs");
                               require(runs.is_array() && runs.size() <= 100, "invalid GitHub execution page");
                               for (const auto& run : runs) {
                                   if (run.at("display_title") == title(attempt, publication)) {
                                       matches->push_back(run.at("id").get<std::uint64_t>());
                                   }
                               }
                               require(matches->size() <= 1, "multiple GitHub runs claim one assignment; preserve uncertainty");
                               if (runs.size() == 100 && page < 32) {
                                   search(execution, attemptId, publication, page + 1, matches, callback);
                               } else if (runs.size() == 100 || matches->empty()) {
                                   callback({202,
                                             {{"execution", execution},
                                              {"assignment_preserved", true},
                                              {"report", "assigned run not proven; no replacement dispatched"}}});
                               } else {
                                   inspect(execution, attemptId, publication, matches->front(), callback);
                               }
                           } catch (const std::exception& error) {
                               callback({503, {{"error", error.what()}, {"execution", execution}, {"assignment_preserved", true}}});
                           }
                       });
    }

    void Pipeline::reconcile(const Callback& callback) {
        std::vector<Node> assigned;
        for (const auto& [id, node] : store.graph().nodes()) {
            const auto& attempt = node.attempts.back();
            if (attempt.state == "building" || attempt.state == "publishing") {
                assigned.push_back(node);
            }
        }
        if (assigned.empty()) {
            advance(callback);
        } else {
            auto responses = std::make_shared<nlohmann::json>(nlohmann::json::array());
            for (const auto& node : assigned) {
                const auto& attempt = node.attempts.back();
                const bool publication = attempt.state == "publishing";
                const auto& run = publication ? attempt.publicationRun : attempt.buildRun;
                const auto completed = [this, responses, count = assigned.size(), callback](const PipelineResult& result) {
                    responses->push_back(result.body);
                    if (responses->size() == count) {
                        advance([responses, callback](const PipelineResult& advanced) {
                            auto body = advanced.body;
                            body["reconciled"] = *responses;
                            callback({advanced.status, body});
                        });
                    }
                };
                if (run.is_null()) {
                    search(node.id, attempt.id, publication, 1, std::make_shared<std::vector<std::uint64_t>>(), completed);
                } else {
                    inspect(node.id, attempt.id, publication, run.at("id").get<std::uint64_t>(), completed);
                }
            }
        }
    }

    void Pipeline::change(const Node& node, const Callback& callback) {
        store.commit({{node}, store.graph().nextRebuildRevision(), {}}, [node, callback](const DatabaseResult& result) {
            if (result.error.empty()) {
                callback({200, {{"execution", node.id}, {"attempt", node.attempts.back().id}, {"state", node.attempts.back().state}}});
            } else {
                callback({503, {{"error", result.error}, {"uncertain", result.uncertain}}});
            }
        });
    }
} // namespace coreci
