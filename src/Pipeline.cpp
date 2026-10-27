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
        selection->remaining = selection->repositories.size();
        selection->results.resize(selection->remaining);
        for (std::size_t index = 0; index < selection->repositories.size(); ++index) {
            const auto repository = selection->repositories.at(index).at("repository").get<std::string>();
            sources.select(
                repository,
                repository == selection->repository ? selection->tag : "",
                [this, selection, index](const SourceResult& result) {
                    selection->results.at(index) = result;
                    --selection->remaining;
                    if (selection->remaining == 0) {
                        PipelineResult failure;
                        for (std::size_t position = 0; position < selection->repositories.size(); ++position) {
                            const auto& row = selection->repositories.at(position);
                            const auto& selected = selection->results.at(position);
                            if (failure.status == 200) {
                                if (selected.status == 404 && selected.source.contains("repository_id")) {
                                    // A known repository without a semantic-version tag has no selected definition.
                                } else if (selected.status != 200) {
                                    failure = {selected.status, {{"error", selected.error}, {"repository", row.at("repository")}}};
                                } else {
                                    try {
                                        require(selected.source.at("repository_id") == row.at("id"),
                                                "selected source ID differs from enrollment");
                                        require(row.at("repository") != selection->repository || selection->expectedTagObject.empty() ||
                                                    selected.source.at("commit") == selection->expectedTagObject ||
                                                    selected.source.at("tag_object_sha") == selection->expectedTagObject,
                                                "tag moved after this delivery; retain existing selections and process its current event");
                                        auto source = selected.source;
                                        source["deployment"] = config.deployment(github, app, row);
                                        selection->sources.emplace(row.at("name").get<std::string>(), std::move(source));
                                    } catch (const std::exception& error) {
                                        failure = {422, {{"error", error.what()}, {"repository", row.at("repository")}}};
                                    }
                                }
                            }
                        }
                        if (failure.status != 200) {
                            selection->callback(failure);
                        } else if (!selection->sources.contains(selection->root) && selection->tag.empty()) {
                            selection->callback({200, {{"selected", 0}, {"reason", "repository has no semantic-version tag"}}});
                        } else {
                            commitSelection(selection);
                        }
                    }
                });
        }
    }

    void Pipeline::commitSelection(const std::shared_ptr<Selecting>& selection) {
        store.commit(
            [this, selection](const Graph& graph) {
                require(selection->generation == store.getGeneration(), "graph changed during selection; request it again");
                return graph.select(selection->root, selection->sources, !selection->automatic);
            },
            [this, selection](const DatabaseResult& result, const GraphChange* change) {
                if (!result.error.empty()) {
                    selection->callback({result.preparationFailed ? 409 : 503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                } else {
                    advance([selection, count = change == nullptr ? 0 : change->nodes.size()](const PipelineResult& dispatched) {
                        auto response = dispatched.body;
                        response["selected"] = count;
                        selection->callback({dispatched.status, response});
                    });
                }
            });
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
                        const auto action = delivery.at("action").get<std::string>();
                        require(action == "requested" || report.at("status") == action, "report action/status mismatch");
                        require(report.at("run_attempt") == 1 &&
                                    (type == "workflow_run"
                                         ? action == "requested" || action == "in_progress" || action == "completed"
                                         : action == "queued" || action == "waiting" || action == "in_progress" || action == "completed"),
                                "unsupported GitHub report action/attempt");
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
                                        const bool matches =
                                            type == "workflow_run"
                                                ? ((!assigned.is_null() && assigned.at("id") == runId) ||
                                                   report.value("display_title", std::string{}) == title(attempt, publishing))
                                                : matchesJob(attempt, publishing, report);
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
                            observe(selected,
                                    attemptId,
                                    publication,
                                    type == "workflow_run" ? report : nlohmann::json(),
                                    type == "workflow_job" ? report : nlohmann::json(),
                                    completed);
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
        store.commit(
            [execution](const Graph& graph) {
                return graph.retry(execution);
            },
            [this, callback](const DatabaseResult& result, const GraphChange*) {
                if (!result.error.empty()) {
                    callback({result.preparationFailed ? 409 : 503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                } else {
                    advance(callback);
                }
            });
    }

    bool Pipeline::matchesJob(const Attempt& attempt, bool publication, const nlohmann::json& job) {
        const auto name = job.value("name", std::string{}), expected = title(attempt, publication, true);
        const bool skipped = publication && job.value("status", "") == "completed" && job.value("conclusion", "") == "skipped";
        const auto& run = publication ? attempt.publicationRun : attempt.buildRun;
        return (run.is_null() || run.at("id") == job.at("run_id")) &&
               (name == expected || name.ends_with(" / " + expected) ||
                (skipped && !run.is_null() && name == "Publish ${{ fromJSON(inputs.assignment).label }}"));
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
        const auto blocked = std::make_shared<nlohmann::json>(nlohmann::json::object());
        store.commit(
            [blocked](const Graph& graph) {
                GraphChange change;
                change.nextRevision = graph.nextRebuildRevision();
                std::map<std::string, std::size_t> reserved;
                for (const auto& [id, node] : graph.nodes()) {
                    if (node.attempts.back().state == "waiting") {
                        const auto reasons = graph.blockers(id);
                        if (reasons.empty()) {
                            change.nodes.push_back(graph.beginBuild(id));
                        } else {
                            (*blocked)[id] = reasons;
                        }
                    } else if (node.attempts.back().state == "building" && !node.attempts.back().buildResult.is_null()) {
                        auto& count = reserved[Graph::publicationFeed(node)];
                        const auto reason = graph.publicationBlocker(id, count);
                        if (reason.empty()) {
                            change.nodes.push_back(graph.beginPublication(id, count));
                            ++count;
                        } else {
                            (*blocked)[id] = nlohmann::json::array({reason});
                        }
                    }
                }
                return change;
            },
            [this, blocked, callback](const DatabaseResult& result, const GraphChange* change) {
                if (!result.error.empty()) {
                    callback({result.preparationFailed ? 422 : 503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                } else if (change == nullptr) {
                    callback({200, {{"assigned", 0}, {"blocked", *blocked}}});
                } else {
                    const auto responses = std::make_shared<nlohmann::json>(nlohmann::json::array());
                    for (const auto& node : change->nodes) {
                        dispatch(node,
                                 node.attempts.back().state == "publishing",
                                 [responses, count = change->nodes.size(), blocked, callback](const PipelineResult& dispatched) {
                                     responses->push_back(dispatched.body);
                                     if (responses->size() == count) {
                                         callback({202, {{"assigned", count}, {"dispatches", *responses}, {"blocked", *blocked}}});
                                     }
                                 });
                    }
                }
            });
    }

    void Pipeline::read(const std::string& repository,
                        const std::string& suffix,
                        const nlohmann::json& permissions,
                        const GitHub::Callback& callback) {
        const auto key = nlohmann::json{repository, suffix, permissions}.dump();
        auto& waiting = reads[key];
        waiting.push_back(callback);
        if (waiting.size() == 1) {
            github.request(repository, "GET", suffix, nullptr, permissions, [this, key](const GitHubResult& result) {
                const auto callbacks = std::move(reads.at(key));
                reads.erase(key);
                for (const auto& ready : callbacks) {
                    ready(result);
                }
            });
        }
    }

    void Pipeline::proof(const std::string& repository,
                         const nlohmann::json& definition,
                         const std::string& commit,
                         const std::string& rootTree,
                         const SourceReader::Callback& callback) {
        const auto path = definition.at("path").get<std::string>();
        const auto slot = repository + ":" + path;
        const auto key = nlohmann::json{repository, path, commit, definition.at("blob_sha")}.dump();
        if (proofs.contains(slot) && proofs.at(slot).source.at("commit") == commit &&
            proofs.at(slot).blobSha == definition.at("blob_sha").get<std::string>()) {
            callback(proofs.at(slot));
        } else {
            auto& waiting = proofReads[key];
            waiting.push_back(callback);
            if (waiting.size() == 1) {
                sources.file(
                    repository,
                    commit,
                    path,
                    [this, slot, key, definition](const SourceResult& file) {
                        const auto result = file.status == 200 && file.blobSha == definition.at("blob_sha").get<std::string>()
                                                ? file
                                                : SourceResult{422, nullptr, "", "", "workflow file does not match deployment approval"};
                        if (result.status == 200) {
                            proofs[slot] = result;
                        }
                        const auto callbacks = std::move(proofReads.at(key));
                        proofReads.erase(key);
                        for (const auto& ready : callbacks) {
                            ready(result);
                        }
                    },
                    false,
                    rootTree,
                    false);
            }
        }
    }

    void Pipeline::workflow(const std::string& repository, const nlohmann::json& definition, const SourceReader::Callback& callback) {
        read(repository,
             "/commits/" + httputils::url_encode(definition.at("ref").get<std::string>()),
             {{"contents", "read"}},
             [this, repository, definition, callback](const GitHubResult& result) {
                 try {
                     require(result.status == 200 && result.error.empty(), "could not resolve the assigned workflow dispatch ref");
                     proof(repository,
                           definition,
                           result.body.at("sha").get<std::string>(),
                           result.body.at("commit").at("tree").at("sha").get<std::string>(),
                           callback);
                 } catch (const std::exception& error) {
                     callback({422, nullptr, "", "", error.what()});
                 }
             });
    }

    void Pipeline::refreshStatus() {
        if (!refreshing && !config.statusWorkflow.empty() && store.graph().nodes().size() > 0) {
            static const std::regex path(R"(^\.github/workflows/[A-Za-z0-9_-]+\.ya?ml$)");
            static const std::regex sha(R"(^[a-f0-9]{40}$)");
            if (!std::regex_match(config.statusWorkflow, path) || !std::regex_match(config.statusWorkflowBlob, sha)) {
                snode::log::application().error() << "Public status workflow configuration is invalid";
            } else {
                refreshing = true;
                const auto generation = store.getGeneration();
                workflow(config.workerRepository,
                         {{"path", config.statusWorkflow}, {"ref", config.publicationRef}, {"blob_sha", config.statusWorkflowBlob}},
                         [this, generation](const SourceResult& approved) {
                             if (approved.status != 200) {
                                 refreshing = false;
                                 snode::log::application().error() << "Public status workflow approval could not be verified";
                                 if (generation != store.getGeneration()) {
                                     refreshStatus();
                                 }
                             } else {
                                 const auto name = config.statusWorkflow.substr(std::string(".github/workflows/").size());
                                 github.request(config.workerRepository,
                                                "POST",
                                                "/actions/workflows/" + name + "/dispatches",
                                                {{"ref", config.publicationRef}, {"inputs", {{"generation", std::to_string(generation)}}}},
                                                {{"actions", "write"}},
                                                [this, generation](const GitHubResult& result) {
                                                    refreshing = false;
                                                    if (generation != store.getGeneration()) {
                                                        refreshStatus();
                                                    }
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
                        change(
                            [=](const Graph& graph) {
                                return graph.rejectedDispatch(node.id, attempt.id, publication, 0, false, approved.error);
                            },
                            callback);
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
                                    change(
                                        [=](const Graph& graph) {
                                            return graph.rejectedDispatch(
                                                node.id, attempt.id, publication, result.status, result.submitted, reason);
                                        },
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
            const auto& node = store.graph().node(execution);
            const auto repository = publication ? node.source.at("deployment").at("worker").at("repository") : node.source.at("repository");
            read(repository,
                 "/actions/runs/" + std::to_string(runId) + "/attempts/1",
                 {{"actions", "read"}},
                 [this, execution, attemptId, publication, runId, callback](const GitHubResult& result) {
                     if (result.status != 200 || !result.error.empty() || !result.body.is_object() || !result.body.contains("id") ||
                         result.body.at("id") != runId) {
                         callback({503, {{"error", "assigned GitHub result is inaccessible; preserve its assignment"}}});
                     } else {
                         observe(execution, attemptId, publication, result.body, nullptr, callback, true);
                     }
                 });
        } catch (const std::exception& error) {
            callback({409, {{"error", error.what()}}});
        }
    }

    void Pipeline::observe(const std::string& execution,
                           const std::string& attemptId,
                           bool publication,
                           const nlohmann::json& run,
                           const nlohmann::json& job,
                           const Callback& callback,
                           bool recovery) {
        try {
            const auto& node = store.graph().node(execution);
            const auto found = std::find_if(node.attempts.begin(), node.attempts.end(), [&](const auto& attempt) {
                return attempt.id == attemptId;
            });
            require(found != node.attempts.end() && !found->assignment.is_null(), "report does not match assigned work");
            const auto attempt = *found;
            const auto known = publication ? attempt.publicationRun : attempt.buildRun;
            require(!publication || !known.is_null() || attempt.state == "publishing", "publication has no committed dispatch authority");
            const auto& deployment = node.source.at("deployment");
            const auto repository = publication ? deployment.at("worker").at("repository") : node.source.at("repository");
            const auto expectedRepository = publication ? deployment.at("worker").at("repository_id") : node.source.at("repository_id");
            const auto definition = deployment.at(publication ? "publication_workflow" : "source_workflow");
            if (run.is_null() && known.is_null()) {
                read(repository,
                     "/actions/runs/" + std::to_string(job.at("run_id").get<std::uint64_t>()) + "/attempts/1",
                     {{"actions", "read"}},
                     [this, execution, attemptId, publication, job, callback](const GitHubResult& result) {
                         if (result.status == 200 && result.error.empty() && result.body.is_object() && result.body.contains("id") &&
                             result.body.at("id") == job.at("run_id")) {
                             observe(execution, attemptId, publication, result.body, job, callback);
                         } else {
                             callback({503, {{"error", "job run proof is inaccessible; assignment preserved"}}});
                         }
                     });
            } else {
                auto identity = known;
                nlohmann::json observation;
                if (!run.is_null()) {
                    require(run.at("id").get<std::uint64_t>() > 0 && run.at("run_attempt") == 1 && run.at("event") == "workflow_dispatch" &&
                                run.at("repository").at("id") == expectedRepository &&
                                run.at("head_repository").at("id") == expectedRepository &&
                                run.at("actor").at("id") == deployment.at("controller_app_bot_id") &&
                                run.at("display_title") == title(attempt, publication) && run.at("path") == definition.at("path"),
                            "GitHub run identity/event/actor/workflow does not match assignment");
                    identity = {{"id", run.at("id")},
                                {"run_attempt", 1},
                                {"repository_id", expectedRepository},
                                {"actor_id", run.at("actor").at("id")},
                                {"workflow_blob_sha", definition.at("blob_sha")},
                                {"workflow_commit", run.at("head_sha")},
                                {"html_url", run.at("html_url")}};
                    observation = {{"run_id", run.at("id")}, {"status", run.at("status")}};
                    if (run.at("status") == "completed") {
                        require(run.at("conclusion").is_string(), "completed run has no conclusion");
                        observation["run_conclusion"] = run.at("conclusion");
                    }
                } else {
                    observation = {{"run_id", known.at("id")}};
                }
                require(known.is_null() || known == identity, "conflicting GitHub run identity");
                auto apply =
                    [this, execution, attemptId, publication, identity, observation, callback, recovery, readJobs = !run.is_null()](
                        const nlohmann::json& selected) mutable {
                        try {
                            if (!selected.is_null()) {
                                const auto& assigned = store.graph().node(execution);
                                const auto attempt =
                                    std::find_if(assigned.attempts.begin(), assigned.attempts.end(), [&](const auto& value) {
                                        return value.id == attemptId;
                                    });
                                require(matchesJob(*attempt, publication, selected) && selected.at("run_id") == identity.at("id") &&
                                            selected.at("head_sha") == identity.at("workflow_commit") && selected.at("run_attempt") == 1 &&
                                            selected.at("id").get<std::uint64_t>() > 0,
                                        "ambiguous or mismatched execution job");
                                observation["job_id"] = selected.at("id");
                                observation["job_status"] = selected.at("status");
                                if (selected.at("status") == "in_progress" || selected.at("status") == "completed") {
                                    observation["started_at"] = selected.value("started_at", nlohmann::json());
                                }
                                if (selected.at("status") == "completed") {
                                    require(selected.at("conclusion").is_string(), "completed job has no conclusion");
                                    observation["completed_at"] = selected.value("completed_at", nlohmann::json());
                                    observation["conclusion"] = selected.at("conclusion");
                                    if (publication) {
                                        for (const auto& step : selected.value("steps", nlohmann::json::array())) {
                                            if (step.at("name") == "Publish checked feed update") {
                                                require(!observation.contains("push_step_conclusion"), "ambiguous publisher push step");
                                                observation["push_step_conclusion"] = step.at("conclusion");
                                            }
                                        }
                                    }
                                }
                            }
                            record(execution, attemptId, publication, identity, observation, callback, recovery, readJobs);
                        } catch (const std::exception& error) {
                            callback({409, {{"error", error.what()}, {"assignment_preserved", true}}});
                        }
                    };
                auto ready = [apply, job, callback](const SourceResult& approved) mutable {
                    if (approved.status != 200) {
                        callback({409, {{"error", approved.error}}});
                    } else {
                        apply(job);
                    }
                };
                if (!known.is_null()) {
                    ready({200, nullptr, "", definition.at("blob_sha"), ""});
                } else {
                    std::string tree;
                    if (run.contains("head_commit") && !run.at("head_commit").is_null()) {
                        require(run.at("head_commit").at("id") == run.at("head_sha"), "run commit tree belongs to another commit");
                        tree = run.at("head_commit").at("tree_id").get<std::string>();
                    }
                    proof(repository, definition, run.at("head_sha").get<std::string>(), tree, ready);
                }
            }
        } catch (const std::exception& error) {
            callback({409, {{"error", error.what()}, {"assignment_preserved", true}}});
        }
    }

    void Pipeline::record(const std::string& execution,
                          const std::string& attemptId,
                          bool publication,
                          const nlohmann::json& identity,
                          const nlohmann::json& observation,
                          const Callback& callback,
                          bool recovery,
                          bool readJobs) {
        store.commit(
            [=](const Graph& graph) {
                return GraphChange{
                    {graph.bindRun(execution, attemptId, publication, identity, observation)}, graph.nextRebuildRevision(), {}};
            },
            [this, execution, attemptId, publication, callback, recovery, readJobs](const DatabaseResult& result,
                                                                                    const GraphChange* change) {
                if (!result.error.empty()) {
                    callback({result.preparationFailed ? 409 : 503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                } else if (change == nullptr && !recovery) {
                    callback({200, {{"execution", execution}, {"report", "unchanged"}}});
                } else {
                    complete(execution, attemptId, publication, callback, readJobs);
                }
            });
    }

    void Pipeline::complete(
        const std::string& execution, const std::string& attemptId, bool publication, const Callback& callback, bool readJobs) {
        const auto node = store.graph().node(execution);
        const auto attempt = node.attempts.back();
        const auto evidence = attempt.evidence.value(publication ? "publication" : "build", nlohmann::json::object());
        const auto key = attemptId + (publication ? ":publication" : ":build");
        if (attempt.id != attemptId || attempt.state != (publication ? "publishing" : "building") ||
            evidence.value("status", "") != "completed" || completionReads.contains(key) ||
            (!publication && !attempt.buildResult.is_null())) {
            callback({200, {{"execution", execution}, {"report", "progress/history retained"}}});
        } else if (!publication && evidence.contains("run_conclusion") && evidence.at("run_conclusion") != "success") {
            change(
                [=](const Graph& graph) {
                    return graph.failed(
                        execution, attemptId, "GitHub execution completed: " + evidence.at("run_conclusion").get<std::string>());
                },
                callback);
        } else if (!evidence.contains("run_conclusion") || (!readJobs && evidence.value("job_status", "") != "completed")) {
            callback({202, {{"assignment_preserved", true}, {"error", "run conclusion incomplete"}}});
        } else {
            completionReads.insert(key);
            const auto finished = [this, key, callback](const PipelineResult& result) {
                completionReads.erase(key);
                callback(result);
            };
            if (evidence.value("job_status", "") != "completed") {
                const auto repository =
                    publication ? node.source.at("deployment").at("worker").at("repository") : node.source.at("repository");
                github.request(
                    repository,
                    "GET",
                    "/actions/runs/" + std::to_string(evidence.at("run_id").get<std::uint64_t>()) + "/attempts/1/jobs?per_page=100",
                    nullptr,
                    {{"actions", "read"}},
                    [this, execution, attemptId, publication, attempt, key, finished, callback](const GitHubResult& jobs) {
                        completionReads.erase(key);
                        try {
                            require(jobs.status == 200 && jobs.error.empty() && jobs.body.at("jobs").is_array() &&
                                        jobs.body.value("total_count", 101) <= 100 &&
                                        jobs.body.at("jobs").size() == jobs.body.at("total_count"),
                                    "assigned jobs are inaccessible or unbounded");
                            nlohmann::json selected;
                            for (const auto& candidate : jobs.body.at("jobs")) {
                                if (matchesJob(attempt, publication, candidate)) {
                                    require(selected.is_null(), "ambiguous execution job");
                                    selected = candidate;
                                }
                            }
                            require(!selected.is_null(), "assigned job evidence incomplete");
                            observe(execution, attemptId, publication, nullptr, selected, callback, true);
                        } catch (const std::exception& error) {
                            const auto facts = store.graph().node(execution).attempts.back().evidence;
                            if (facts.value(publication ? "publication" : "build", nlohmann::json::object()).value("job_status", "") ==
                                "completed") {
                                complete(execution, attemptId, publication, callback);
                            } else {
                                finished({202, {{"error", error.what()}, {"assignment_preserved", true}}});
                            }
                        }
                    });
            } else if (publication) {
                publicationResult(node, evidence, finished);
            } else {
                buildArtifacts(node, evidence, finished);
            }
        }
    }

    void Pipeline::buildArtifacts(const Node& node, const nlohmann::json& job, const Callback& callback) {
        const auto attempt = node.attempts.back();
        const auto repository = node.source.at("repository").get<std::string>();
        const auto runId = attempt.buildRun.at("id").get<std::uint64_t>();
        try {
            require(!job.is_null() && job.at("run_id") == runId && job.at("job_status") == "completed" && job.at("conclusion") == "success",
                    "assigned package job did not complete successfully");
            github.request(
                repository,
                "GET",
                "/actions/runs/" + std::to_string(runId) + "/artifacts?per_page=100",
                nullptr,
                {{"actions", "read"}},
                [this, node, attempt, job, runId, callback](const GitHubResult& artifacts) {
                    try {
                        const auto& current = store.graph().node(node.id).attempts.back();
                        require(current.id == attempt.id && current.state == "building" && current.buildResult.is_null(),
                                "late artifact result; current attempt preserved");
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
                            change(
                                [=](const Graph& graph) {
                                    return graph.failed(node.id, attempt.id, "required artifacts missing/expired");
                                },
                                callback);
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
                                                          {"job_id", job.at("job_id")},
                                                          {"run_attempt", 1}};
                            const auto result = nlohmann::json{{"run", attempt.buildRun},
                                                               {"assignment_sha256", Graph::digest(attempt.assignment)},
                                                               {"artifacts", {selected}},
                                                               {"build_job",
                                                                {{"started_at", job.value("started_at", nlohmann::json())},
                                                                 {"completed_at", job.value("completed_at", nlohmann::json())}}}};
                            change(
                                [=](const Graph& graph) {
                                    return graph.built(node.id, attempt.id, result);
                                },
                                [this, callback](const PipelineResult& stored) {
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

    void Pipeline::publicationResult(const Node& node, const nlohmann::json& evidence, const Callback& callback) {
        const auto attemptId = node.attempts.back().id;
        const bool beforePush = evidence.at("run_conclusion") != "success" &&
                                (evidence.value("conclusion", "") == "skipped" || evidence.value("push_step_conclusion", "") == "skipped");
        if (beforePush) {
            change(
                [=](const Graph& graph) {
                    return graph.failed(
                        node.id, attemptId, "Verified GitHub publication failure before the feed-update step; explicit retry is available");
                },
                callback);
        } else {
            feeds.verify(node, [this, node, attemptId, evidence, callback](const FeedResult& result) {
                try {
                    const auto& current = store.graph().node(node.id).attempts.back();
                    require(current.id == attemptId && current.state == "publishing", "late feed result; current attempt preserved");
                    if (result.error.empty()) {
                        change(
                            [=](const Graph& graph) {
                                return graph.published(node.id, attemptId, result.receipt);
                            },
                            callback);
                    } else {
                        callback({202,
                                  {{"execution", node.id},
                                   {"state", "publishing"},
                                   {"assignment_preserved", true},
                                   {"github_conclusion", evidence.at("run_conclusion")},
                                   {"error", result.error}}});
                    }
                } catch (const std::exception& error) {
                    callback({409, {{"error", error.what()}, {"assignment_preserved", true}}});
                }
            });
        }
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

    void Pipeline::change(const std::function<Node(const Graph&)>& derive, const Callback& callback) {
        store.commit(
            [derive](const Graph& graph) {
                return GraphChange{{derive(graph)}, graph.nextRebuildRevision(), {}};
            },
            [this, callback](const DatabaseResult& result, const GraphChange* change) {
                if (!result.error.empty()) {
                    callback({result.preparationFailed ? 409 : 503, {{"error", result.error}, {"uncertain", result.uncertain}}});
                } else if (change == nullptr) {
                    callback({200, {{"report", "unchanged"}}});
                } else {
                    const auto& node = change->nodes.front();
                    if (node.attempts.back().state == "published" || node.attempts.back().state == "failed") {
                        advance(callback);
                    } else {
                        callback(
                            {200, {{"execution", node.id}, {"attempt", node.attempts.back().id}, {"state", node.attempts.back().state}}});
                    }
                }
            });
    }
} // namespace coreci
