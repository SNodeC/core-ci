#include "Auth.h"
#include "Controller.h"

#include <Log.h>
#include <algorithm>
#include <regex>

namespace ci {

    void Controller::send(const Json& authorized) {
        token([this, authorized](const std::string& access, const std::string& error) {
            for (const auto& action : authorized) {
                const auto record = [this, action](int status, const std::string& body, const std::string& context) {
                    const Json response = Json::parse(body, nullptr, false);
                    store.change(
                        [action, status, response, context](Json& graph) {
                            if (auto* work = findId(graph.at("executions"), action.at("execution"))) {
                                if (auto* attempt = findId(work->at("attempts"), action.at("attempt"))) {
                                    const std::string stage = action.at("stage");
                                    (*attempt)["facts"][stage + "_send"] = {
                                        {"http_status", status},
                                        {"context", context},
                                        {"at", utc()},
                                        {"outcome", status == 200 || status == 204 ? "accepted" : "unresolved"}};
                                    if (status == 200 && response.contains("workflow_run_id")) {
                                        require(!attempt->at("facts").contains(stage + "_run_id") ||
                                                    attempt->at("facts").at(stage + "_run_id") == response.at("workflow_run_id"),
                                                "Dispatch response conflicts with original run binding");
                                        (*attempt)["facts"][stage + "_run_id"] = response.at("workflow_run_id");
                                    }
                                    const bool awaiting = (stage == "build" && attempt->at("state") == "building") ||
                                                          (stage == "publish" && attempt->at("state") == "publishing");
                                    if (status >= 400 && status < 500 && status != 408 && awaiting &&
                                        !attempt->at("facts").contains(stage + "_run")) {
                                        (*attempt)["state"] = "failed";
                                        (*attempt)["failure_kind"] = "definite";
                                        (*attempt)["failure"] = "GitHub rejected workflow dispatch: HTTP " + std::to_string(status);
                                    }
                                }
                            }
                            return Json::array();
                        },
                        [](const std::string& error) {
                            if (!error.empty()) {
                                snode::log::application().error() << error;
                            }
                        });
                };
                if (!error.empty()) {
                    record(0, "", error);
                } else {
                    Json inputs = {{"execution", action.at("execution")},
                                   {"attempt", action.at("attempt")},
                                   {"assignment_sha256", hash(action.at("assignment").dump())},
                                   {"assignment", action.at("assignment").dump()}};
                    if (action.contains("build_result")) {
                        inputs["build_result"] = action.at("build_result").dump();
                    }
                    const auto& command = action.at("command");
                    const std::string file = command.at("path").get<std::string>();
                    https("https://api.github.com/repos/" + command.at("repository").get<std::string>() + "/actions/workflows/" +
                              encode(file.substr(file.find_last_of('/') + 1)) + "/dispatches",
                          "POST",
                          Json{{"ref", command.at("ref")}, {"inputs", inputs}}.dump(),
                          access,
                          [record](int status, const std::string& body, const std::string&, const std::string& context) {
                              record(status, body, context);
                          });
                }
            }
        });
    }

    void Controller::run(const std::string& token, const Json& work, const std::string& stage, const Value& done, std::uint64_t candidate) {
        const auto& attempt = work.at("attempts").back();
        const Json command = attempt.at(stage + "_assignment");
        const Json bot = work.at("source").at("deployment").at("controller_app_bot_id");
        const std::string prefix = "/repos/" + command.at("repository").get<std::string>();
        const std::string title = (stage == "build" ? "Build " : "Publish ") + attempt.at("assignment").at("label").get<std::string>();
        const auto inspect = [this, token, work, stage, done, command, prefix, title, bot](const Json& selected) {
            require(!selected.is_null(), "Assigned run remains unbound");
            const auto& active = work.at("attempts").back();
            if (active.at("facts").contains(stage + "_run_id")) {
                require(selected.at("id") == active.at("facts").at(stage + "_run_id"), "Run conflicts with original assignment");
            }
            api(token,
                prefix + "/actions/runs/" + selected.at("id").dump() + "/attempts/1",
                checked(done, [this, token, done, command, prefix, title, bot](const Json& observed) {
                    require(observed.at("repository").at("id") == command.at("repository_id") &&
                                observed.at("head_repository").at("id") == command.at("repository_id") &&
                                observed.at("actor").at("id") == bot && observed.at("event") == "workflow_dispatch" &&
                                observed.at("run_attempt") == 1 && observed.at("head_sha") == command.at("commit") &&
                                observed.at("path") == command.at("path") && observed.at("display_title") == title,
                            "Unrelated or untrusted GitHub run");
                    contents(token,
                             command.at("repository"),
                             command.at("path"),
                             observed.at("head_sha"),
                             checked(done, [this, token, done, command, prefix, observed, title](const Json& file) {
                                 require(file.at("sha") == command.at("blob_sha"), "Run workflow differs from approved immutable identity");
                                 pages(token,
                                       prefix + "/actions/runs/" + observed.at("id").dump() + "/attempts/1/jobs",
                                       "jobs",
                                       checked(done, [done, command, observed, title](const Json& jobs) {
                                           Json facts = {{"identity",
                                                          {{"id", observed.at("id")},
                                                           {"run_attempt", 1},
                                                           {"repository_id", command.at("repository_id")},
                                                           {"actor_id", observed.at("actor").at("id")},
                                                           {"workflow_blob_sha", command.at("blob_sha")},
                                                           {"workflow_commit", observed.at("head_sha")},
                                                           {"html_url", observed.at("html_url")}}},
                                                         {"status", observed.at("status")},
                                                         {"conclusion", observed.at("conclusion")},
                                                         {"jobs", Json::array()},
                                                         {"job_id", nullptr}};
                                           for (const auto& job : jobs) {
                                               require(job.at("run_id") == observed.at("id") && job.at("run_attempt") == 1,
                                                       "Unrelated job evidence");
                                               Json selected;
                                               for (const auto& field : {"id",
                                                                         "run_id",
                                                                         "run_attempt",
                                                                         "name",
                                                                         "status",
                                                                         "conclusion",
                                                                         "html_url",
                                                                         "started_at",
                                                                         "completed_at",
                                                                         "steps"}) {
                                                   selected[field] = job.at(field);
                                               }
                                               facts["jobs"].push_back(selected);
                                               const std::string name = selected.at("name");
                                               if (name == title || name.ends_with(" / " + title)) {
                                                   require(facts.at("job_id").is_null(), "Multiple jobs match the assignment");
                                                   facts["job_id"] = selected.at("id");
                                               }
                                           }
                                           done(facts, "");
                                       }));
                             }));
                }));
        };
        if (candidate == 0 && attempt.at("facts").contains(stage + "_run_id")) {
            candidate = attempt.at("facts").at(stage + "_run_id");
        }
        if (candidate != 0) {
            inspect({{"id", candidate}});
        } else {
            const std::string file = command.at("path");
            pages(token,
                  prefix + "/actions/workflows/" + encode(file.substr(file.find_last_of('/') + 1)) + "/runs?event=workflow_dispatch",
                  "workflow_runs",
                  checked(done, [title, inspect, bot](const Json& runs) {
                      Json selected;
                      for (const auto& run : runs) {
                          if (run.at("display_title") == title && run.at("actor").at("id") == bot && run.at("run_attempt") == 1) {
                              require(selected.is_null(), "Multiple runs match the immutable assignment");
                              selected = run;
                          }
                      }
                      inspect(selected);
                  }));
        }
    }

    void Controller::buildResult(const std::string& token, const Json& work, const Json& run, const Value& done) {
        const Json assignment = work.at("attempts").back().at("assignment");
        const std::string prefix = "/repos/" + work.at("source").at("repository").get<std::string>();
        pages(
            token,
            prefix + "/actions/runs/" + run.at("identity").at("id").dump() + "/artifacts",
            "artifacts",
            checked(done, [token, run, assignment, prefix, done](const Json& artifacts) {
                Json selected;
                for (const auto& job : run.at("jobs")) {
                    if (job.at("id") == run.at("job_id")) {
                        selected = job;
                    }
                }
                require(!selected.is_null() && selected.at("status") == "completed" && selected.at("conclusion") == "success",
                        "Invalid assigned build job");

                Json artifact;
                for (const auto& item : artifacts) {
                    if (item.at("name") == assignment.at("artifact_name")) {
                        require(artifact.is_null(), "Duplicate assigned build artifacts");
                        artifact = item;
                    }
                }
                require(!artifact.is_null() && !artifact.at("expired").get<bool>() &&
                            artifact.at("workflow_run").at("id") == run.at("identity").at("id") &&
                            artifact.at("size_in_bytes").get<std::uint64_t>() > 0 &&
                            artifact.at("size_in_bytes").get<std::uint64_t>() <= 512 * 1024 * 1024 &&
                            std::regex_match(artifact.at("digest").get<std::string>(), std::regex("sha256:[a-f0-9]{64}")),
                        "Invalid assigned artifact identity");
                https(
                    "https://api.github.com" + prefix + "/actions/artifacts/" + artifact.at("id").dump() + "/zip",
                    "GET",
                    "",
                    token,
                    [artifact, assignment, selected, run, done](
                        int status, const std::string&, const std::string& location, const std::string& context) {
                        checked(done, [artifact, assignment, selected, run, done, status, location](const Json&) {
                            require(
                                status == 302 &&
                                    std::regex_match(
                                        location,
                                        std::regex(
                                            R"(https://[A-Za-z0-9.-]+\.(blob.core.windows.net|actions.githubusercontent.com)(:443)?/[^#\r\n]*)")),
                                "Unsafe artifact download origin");
                            https(location,
                                  "GET",
                                  "",
                                  "",
                                  [artifact, assignment, selected, run, done](
                                      int status, const std::string& bytes, const std::string&, const std::string& context) {
                                      checked(done, [artifact, assignment, selected, run, done, status, bytes](const Json&) {
                                          require(status == 200 && bytes.size() == artifact.at("size_in_bytes") &&
                                                      artifact.at("digest") == "sha256:" + hash(bytes),
                                                  "Artifact archive bytes differ from authorization");
                                          const Json manifest = artifactManifest(bytes, assignment);
                                          const Json result = {{"assignment_sha256", hash(assignment.dump())},
                                                               {"run", run.at("identity")},
                                                               {"artifacts",
                                                                Json::array({{{"id", artifact.at("id")},
                                                                              {"name", artifact.at("name")},
                                                                              {"job_id", selected.at("id")},
                                                                              {"digest", artifact.at("digest")},
                                                                              {"size_bytes", artifact.at("size_in_bytes")}}})}};
                                          done({{"result", result}, {"manifest", manifest}}, "");
                                      })({}, status == 200 ? "" : context);
                                  });
                        })({}, status == 302 ? "" : context);
                    });
            }));
    }

    void Controller::publicationReceipt(const std::string& token, const Json& work, const Value& complete) {
        const Value done = [complete, started = utc()](Json evidence, const std::string& error) {
            evidence["verification_started_at"] = started;
            complete(evidence, error);
        };
        const auto& active = work.at("attempts").back();
        const Json feed = work.at("source").at("deployment").at("feed");
        const std::string path = "receipts/" + work.at("id").get<std::string>() + "/" + active.at("revision").dump() + ".json";
        api(token,
            "/repos/" + feed.at("repository").get<std::string>() + "/commits?sha=" + encode(feed.at("branch")) + "&path=" + encode(path) +
                "&per_page=1",
            checked(done, [this, token, feed, path, done](const Json& commits) {
                require(!commits.empty(), "Publisher receipt unavailable");
                const Json revision = commits.at(0).at("sha");
                contents(token, feed.at("repository"), path, revision, checked(done, [revision, done](const Json& file) {
                             done({{"receipt", Json::parse(file.at("bytes").get<std::string>())},
                                   {"feed_revision", revision},
                                   {"verified_at", utc()}},
                                  "");
                         }));
            }));
    }

    void Controller::observe(const std::string& execution, const Done& done, std::uint64_t candidate, const std::string& identity) {
        const Value finish = [done](const Json&, const std::string& error) {
            done(error);
        };
        store.read(checked(finish, [this, execution, identity, candidate, done, finish](const Json& snapshot) {
            const auto* work = findId(snapshot.at("graph").at("executions"), execution);
            require(work != nullptr, "Unknown execution");
            Json selected = *work;
            if (!identity.empty()) {
                if (const auto* attempt = findId(work->at("attempts"), identity)) {
                    selected["attempts"] = Json::array({*attempt});
                }
            }
            const auto& active = selected.at("attempts").back();
            require(active.contains("assignment"), "Work has no assigned remote execution");
            const std::string stage = active.contains("publish_assignment") ? "publish" : "build";
            const auto record = [this, selected, stage, done](const Json& observed, const Json& evidence, const std::string& error) {
                store.change(
                    [selected, stage, observed, evidence, error](Json& graph) {
                        return applyEvidence(graph, selected, stage, observed, evidence, error);
                    },
                    [done, error, execution = selected.at("attempts").back().at("id").get<std::string>()](const std::string& failure) {
                        done(failure.empty() && error.empty() ? ""
                                                              : "Observation " + execution + ": " + (failure.empty() ? error : failure));
                    });
            };
            const Value unread = [record](const Json&, const std::string& error) {
                record(nullptr, {}, error);
            };
            token([this, selected, stage, candidate, record, unread](const std::string& access, const std::string& error) {
                checked(unread, [this, selected, stage, candidate, access, record, unread](const Json&) {
                    run(access,
                        selected,
                        stage,
                        checked(unread,
                                [this, selected, stage, access, record](const Json& observed) {
                                    const Value apply = [record, observed](const Json& evidence, const std::string& error) {
                                        record(observed, evidence, error);
                                    };
                                    const auto& active = selected.at("attempts").back();
                                    if (stage == "build" && active.at("state") == "building" && observed.at("status") == "completed" &&
                                        observed.at("conclusion") == "success") {
                                        buildResult(access, selected, observed, apply);
                                    } else if (stage == "publish" && active.at("state") == "publishing" &&
                                               std::count_if(observed.at("jobs").begin(),
                                                             observed.at("jobs").end(),
                                                             [&](const auto& job) {
                                                                 return job.at("name") == "Confirm public receipt";
                                                             }) == 1 &&
                                               std::count_if(observed.at("jobs").begin(), observed.at("jobs").end(), [&](const auto& job) {
                                                   return (job.at("id") == observed.at("job_id") ||
                                                           job.at("name") == "Confirm public receipt") &&
                                                          job.at("status") == "completed" && job.at("conclusion") == "success";
                                               }) == 2) {
                                        publicationReceipt(access, selected, apply);
                                    } else {
                                        apply({}, "");
                                    }
                                }),
                        candidate);
                })({}, error);
            });
        }));
    }

    void Controller::resume(const Done& done) {
        store.read(checked(
            [done](const Json&, const std::string& error) {
                done(error);
            },
            [this, done](const Json& snapshot) {
                for (const auto& work : snapshot.at("graph").at("executions")) {
                    const auto& active = work.at("attempts").back();
                    if (active.contains("assignment") && (active.at("state") == "building" || active.at("state") == "publishing")) {
                        observe(work.at("id"), done, 0, active.at("id"));
                    }
                }
            }));
    }

    void Controller::reconcile(const std::string& execution, const Done& done) {
        observe(execution, done);
    }

    void Controller::webhook(const std::string& event, const Json& payload, const Done& done) {
        require(payload.at("installation").at("id").dump() == installation, "Wrong GitHub installation");
        const std::string repository = payload.at("repository").at("full_name");
        require(deployment.at("repositories").contains(repository) &&
                    deployment.at("repositories").at(repository) == payload.at("repository").at("id"),
                "Unauthorized webhook repository");
        if (event == "push" && !payload.value("deleted", false) && payload.at("ref").get<std::string>().starts_with("refs/tags/")) {
            request(repository, payload.at("ref").get<std::string>().substr(10), done, payload.at("after"));
        } else if (event == "workflow_run" || event == "workflow_job") {
            store.read(checked(
                [done](const Json&, const std::string& error) {
                    done(error);
                },
                [this, repository, event, payload, done](const Json& snapshot) {
                    std::size_t matches = 0;
                    const Json notification = payload.at(event);
                    const auto id = notification.at(event == "workflow_run" ? "id" : "run_id").get<std::uint64_t>();
                    for (const auto& work : snapshot.at("graph").at("executions")) {
                        for (const auto& attempt : work.at("attempts")) {
                            const std::string stage = attempt.contains("publish_assignment") ? "publish" : "build";
                            if (attempt.contains(stage + "_assignment") &&
                                attempt.at(stage + "_assignment").at("repository") == repository) {
                                const std::string title =
                                    (stage == "build" ? "Build " : "Publish ") + attempt.at("assignment").at("label").get<std::string>();
                                const std::string label = notification.value(event == "workflow_run" ? "display_title" : "name", "");
                                if (label == title || label.ends_with(" / " + title) ||
                                    attempt.at("facts").value(stage + "_run_id", Json()) == id) {
                                    ++matches;
                                    observe(work.at("id"), done, id, attempt.at("id"));
                                }
                            }
                        }
                    }
                    if (matches == 0) {
                        done("No authorized assignment matches this notification");
                    }
                }));
        } else {
            done("");
        }
    }

} // namespace ci
