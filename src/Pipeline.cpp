#include "Pipeline.h"

#include "Version.h"

#include <Log.h>
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
                       Enrollment& enrollment,
                       GraphStore& store,
                       FeedProof& feeds)
        : config(config)
        , app(app)
        , github(github)
        , sources(sources)
        , enrollment(enrollment)
        , store(store)
        , feeds(feeds) {
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
                    commitSelection(selection);
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

    void Pipeline::commitSelection(const std::shared_ptr<Selecting>& selection) {
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
    }

    void Pipeline::event(const std::string& type, const std::string& signature, const std::string& body, const Callback& callback) {
        const auto completed = [](const PipelineResult& result) {
            snode::log::application().info() << "GitHub webhook completed: "
                                             << nlohmann::json{{"status", result.status}, {"result", result.body}}.dump();
        };
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
                            const auto tagObject = delivery.at("after").get<std::string>();
                            callback({202, {{"event", "version notification accepted"}}});
                            request(repository, ref.substr(10), true, completed, tagObject);
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
                                        const auto expectedJob = title(attempt, publishing, true);
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
                            callback({202, {{"event", "assigned execution notification accepted"}}});
                            inspect(selected, attemptId, publication, runId, completed);
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

    std::string Pipeline::title(const Attempt& attempt, bool publication, bool job) {
        std::string result;
        if (attempt.assignment.contains("label")) {
            result = std::string(publication ? "Publish " : "Build ") + attempt.assignment.at("label").get<std::string>();
        } else {
            result = std::string("core-ci ") + (publication ? "publish " : job ? "package " : "build ") + attempt.id;
            if (!job) {
                result += " " + Graph::digest(attempt.assignment);
            }
        }
        return result;
    }

    void Pipeline::advance(const Callback& callback) {
        try {
            GraphChange change;
            change.nextRevision = store.graph().nextRebuildRevision();
            bool publicationSelected = false;
            nlohmann::json blocked = nlohmann::json::object();
            for (const auto& [id, node] : store.graph().nodes()) {
                if (!publicationSelected && store.graph().publicationReady(id)) {
                    change.nodes.push_back(store.graph().beginPublication(id));
                    publicationSelected = true;
                }
                if (node.attempts.back().state == "waiting") {
                    const auto reasons = store.graph().blockers(id);
                    if (reasons.empty()) {
                        change.nodes.push_back(store.graph().beginBuild(id));
                    } else {
                        blocked[id] = reasons;
                    }
                } else if (node.attempts.back().state == "building" && !node.attempts.back().buildResult.is_null()) {
                    const auto reason = store.graph().dispatchBlocker(node, true);
                    if (!reason.empty()) {
                        blocked[id] = nlohmann::json::array({reason});
                    }
                }
            }
            if (change.nodes.empty()) {
                callback({200, {{"assigned", 0}, {"blocked", blocked}}});
            } else {
                store.commit(change, [this, nodes = change.nodes, blocked, callback](const DatabaseResult& result) {
                    if (!result.error.empty()) {
                        callback({503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                    } else {
                        auto responses = std::make_shared<nlohmann::json>(nlohmann::json::array());
                        for (const auto& node : nodes) {
                            dispatch(node,
                                     node.attempts.back().state == "publishing",
                                     [responses, count = nodes.size(), blocked, callback](const PipelineResult& dispatched) {
                                         responses->push_back(dispatched.body);
                                         if (responses->size() == count) {
                                             callback({202, {{"assigned", count}, {"dispatches", *responses}, {"blocked", blocked}}});
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

    void Pipeline::refreshStatus() {
        if (!config.statusWorkflow.empty() && store.graph().nodes().size() > 0) {
            static const std::regex path(R"(^\.github/workflows/[A-Za-z0-9_-]+\.ya?ml$)");
            static const std::regex sha(R"(^[a-f0-9]{40}$)");
            if (!std::regex_match(config.statusWorkflow, path) || !std::regex_match(config.statusWorkflowBlob, sha)) {
                snode::log::application().error() << "Public status workflow configuration is invalid";
            } else {
                const auto generation = store.getGeneration();
                workflow(config.workerRepository,
                         {{"path", config.statusWorkflow}, {"ref", config.publicationRef}, {"blob_sha", config.statusWorkflowBlob}},
                         [this, generation](const SourceResult& approved) {
                             if (approved.status != 200) {
                                 snode::log::application().error() << "Public status workflow approval could not be verified";
                             } else {
                                 const auto name = config.statusWorkflow.substr(std::string(".github/workflows/").size());
                                 github.request(config.workerRepository,
                                                "POST",
                                                "/actions/workflows/" + name + "/dispatches",
                                                {{"ref", config.publicationRef}, {"inputs", {{"generation", std::to_string(generation)}}}},
                                                {{"actions", "write"}},
                                                [](const GitHubResult& result) {
                                                    if (!result.error.empty() || (result.status != 200 && result.status != 204)) {
                                                        snode::log::application().error()
                                                            << "Public status dispatch not confirmed; no retry (HTTP " << result.status
                                                            << ")";
                                                    }
                                                });
                             }
                         });
            }
        }
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
                    const auto inputs = store.graph().dispatchInputs(node, publication);
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
                "/actions/runs/" + std::to_string(runId) + "/attempts/1",
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
                            [this, execution, attemptId, publication, run, runId, repository, attempt, workflow, callback](
                                const SourceResult& file) {
                                try {
                                    require(file.status == 200 && file.blobSha == workflow.at("blob_sha").get<std::string>(),
                                            "GitHub executed a workflow outside the approved blob identity");
                                    github.request(
                                        repository,
                                        "GET",
                                        "/actions/runs/" + std::to_string(runId) + "/attempts/1/jobs?per_page=100",
                                        nullptr,
                                        {{"actions", "read"}},
                                        [this,
                                         execution,
                                         attemptId,
                                         publication,
                                         run,
                                         workflow,
                                         workflowBlob = file.blobSha,
                                         attempt,
                                         callback](const GitHubResult& jobs) {
                                            try {
                                                nlohmann::json selectedJob;
                                                nlohmann::json observation{{"run_id", run.at("id")}, {"status", run.at("status")}};
                                                if (jobs.status == 200 && jobs.error.empty() && jobs.body.is_object() &&
                                                    jobs.body.value("total_count", 101) <= 100 && jobs.body.at("jobs").is_array() &&
                                                    jobs.body.at("jobs").size() == jobs.body.at("total_count")) {
                                                    for (const auto& job : jobs.body.at("jobs")) {
                                                        const auto name = job.at("name").get<std::string>(),
                                                                   expected = title(attempt, publication, true);
                                                        if (name == expected || name.ends_with(" / " + expected)) {
                                                            require(!observation.contains("job_id") && job.at("run_id") == run.at("id") &&
                                                                        job.at("run_attempt") == 1 && job.at("id").get<std::uint64_t>() > 0,
                                                                    "ambiguous or mismatched execution job");
                                                            selectedJob = job;
                                                            observation["job_id"] = job.at("id");
                                                            observation["job_status"] = job.at("status");
                                                            if (job.at("status") == "in_progress" || job.at("status") == "completed") {
                                                                observation["started_at"] = job.value("started_at", nlohmann::json());
                                                            }
                                                            if (job.at("status") == "completed") {
                                                                observation["completed_at"] = job.value("completed_at", nlohmann::json());
                                                                observation["conclusion"] = job.value("conclusion", nlohmann::json());
                                                            }
                                                        }
                                                    }
                                                }
                                                const auto& current = store.graph().node(execution);
                                                const bool late = current.attempts.back().id != attemptId ||
                                                                  current.attempts.back().state == "published" ||
                                                                  current.attempts.back().state == "failed" ||
                                                                  (!publication && current.attempts.back().state == "publishing");
                                                const nlohmann::json identity{{"id", run.at("id")},
                                                                              {"run_attempt", run.at("run_attempt")},
                                                                              {"repository_id", run.at("repository").at("id")},
                                                                              {"actor_id", run.at("actor").at("id")},
                                                                              {"workflow_blob_sha", workflowBlob},
                                                                              {"workflow_commit", run.at("head_sha")},
                                                                              {"html_url", run.at("html_url")}};
                                                const bool buildFailed = !late && !publication && run.at("status") == "completed" &&
                                                                         run.at("conclusion") != "success";
                                                const auto bound =
                                                    buildFailed
                                                        ? store.graph().failed(execution,
                                                                               attemptId,
                                                                               "GitHub execution completed: " +
                                                                                   run.at("conclusion").get<std::string>(),
                                                                               identity,
                                                                               observation)
                                                        : store.graph().bindRun(execution, attemptId, publication, identity, observation);
                                                change(bound,
                                                       [this, execution, attemptId, publication, run, selectedJob, late, callback](
                                                           const PipelineResult& stored) {
                                                           if (stored.status != 200) {
                                                               callback(stored);
                                                           } else if (late) {
                                                               callback({200,
                                                                         {{"execution", execution},
                                                                          {"report", "late or terminal; history retained"}}});
                                                           } else if (run.at("status") != "completed") {
                                                               callback(stored);
                                                           } else if (publication) {
                                                               publicationResult(store.graph().node(execution), run, selectedJob, callback);
                                                           } else if (run.at("conclusion") != "success") {
                                                               callback(stored);
                                                           } else {
                                                               buildArtifacts(store.graph().node(execution), selectedJob, callback);
                                                           }
                                                       });
                                            } catch (const std::exception& error) {
                                                callback({409, {{"error", error.what()}, {"assignment_preserved", true}}});
                                            }
                                        });
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

    void Pipeline::buildArtifacts(const Node& node, const nlohmann::json& job, const Callback& callback) {
        const auto attempt = node.attempts.back();
        const auto repository = node.source.at("repository").get<std::string>();
        const auto runId = attempt.buildRun.at("id").get<std::uint64_t>();
        try {
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
                                    artifacts.body.at("total_count").get<unsigned int>() <= 100 &&
                                    artifacts.body.at("artifacts").is_array() &&
                                    artifacts.body.at("artifacts").size() == artifacts.body.at("total_count"),
                                "assigned GitHub artifacts are unavailable or exceed supported bounds");
                        nlohmann::json artifact;
                        for (const auto& candidate : artifacts.body.at("artifacts")) {
                            if (candidate.at("name") == attempt.assignment.at("artifact_name")) {
                                require(artifact.is_null(), "assigned build artifact is ambiguous");
                                artifact = candidate;
                            }
                        }
                        if (artifact.is_null() || artifact.at("expired").get<bool>()) {
                            change(store.graph().failed(node.id, attempt.id, "required artifacts missing/expired"), callback);
                        } else {
                            static const std::regex digest(R"(^sha256:[a-f0-9]{64}$)");
                            require(artifact.at("id").get<std::uint64_t>() > 0 && artifact.at("workflow_run").at("id") == runId &&
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
                                                               {"artifacts", {selected}},
                                                               {"build_job",
                                                                {{"started_at", job.value("started_at", nlohmann::json())},
                                                                 {"completed_at", job.value("completed_at", nlohmann::json())}}}};
                            change(store.graph().built(node.id, attempt.id, result), [this, callback](const PipelineResult& stored) {
                                if (stored.status == 200) {
                                    advance(callback);
                                } else {
                                    callback(stored);
                                }
                            });
                        }
                    } catch (const std::exception& error) {
                        callback({503, {{"error", error.what()}, {"execution", node.id}, {"assignment_preserved", true}}});
                    }
                });
        } catch (const std::exception& error) {
            callback({503, {{"error", error.what()}, {"execution", node.id}, {"assignment_preserved", true}}});
        }
    }

    void Pipeline::publicationResult(const Node& node, const nlohmann::json& run, const nlohmann::json& job, const Callback& callback) {
        feeds.verify(node, [this, node, run, job, callback](const FeedResult& result) {
            if (result.error.empty()) {
                try {
                    change(store.graph().published(node.id, node.attempts.back().id, result.receipt), callback);
                } catch (const std::exception& error) {
                    callback({409, {{"error", error.what()}}});
                }
            } else {
                bool beforePush = false;
                try {
                    if (!job.is_null() && job.value("status", "") == "completed" && run.value("conclusion", "") != "success") {
                        beforePush = job.value("conclusion", "") == "skipped";
                        for (const auto& step : job.value("steps", nlohmann::json::array())) {
                            if (step.value("name", "") == "Publish checked feed update") {
                                beforePush = step.value("conclusion", "") == "skipped";
                            }
                        }
                    }
                } catch (const std::exception&) {
                    beforePush = false;
                }
                if (beforePush) {
                    change(store.graph().failed(
                               node.id,
                               node.attempts.back().id,
                               "Verified GitHub publication failure before the feed-update step; explicit retry is available"),
                           callback);
                } else {
                    callback({202,
                              {{"execution", node.id},
                               {"state", "publishing"},
                               {"assignment_preserved", true},
                               {"github_conclusion", run.at("conclusion")},
                               {"error", result.error}}});
                }
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
                                   if (run.at("display_title") == title(attempt, publication) &&
                                       run.at("actor").at("id") ==
                                           attempt.assignment.at("source").at("deployment").at("controller_app_bot_id")) {
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
            if ((attempt.state == "building" && attempt.buildResult.is_null()) || attempt.state == "publishing") {
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
        const auto generation = store.getGeneration();
        const auto completed = [this, node, callback](const DatabaseResult& result) {
            if (result.error.empty()) {
                if (node.attempts.back().state == "published" || node.attempts.back().state == "failed") {
                    advance(callback);
                } else {
                    callback({200, {{"execution", node.id}, {"attempt", node.attempts.back().id}, {"state", node.attempts.back().state}}});
                }
            } else {
                callback({503, {{"error", result.error}, {"uncertain", result.uncertain}}});
            }
        };
        store.commit({{node}, store.graph().nextRebuildRevision(), {}}, [this, node, generation, completed](const DatabaseResult& result) {
            if (!result.uncertain && result.error == "conditional mutation did not match the expected graph revision/state" &&
                store.getGeneration() != generation) {
                store.commit({{node}, store.graph().nextRebuildRevision(), {}}, completed);
            } else {
                completed(result);
            }
        });
    }
} // namespace coreci
