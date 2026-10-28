#include "Enrollment.h"

namespace coreci {
    Enrollment::Enrollment(GitHub& github, GraphStore& store, SourceReader& sources, const PipelineConfig& config)
        : github(github)
        , store(store)
        , sources(sources)
        , config(config) {
    }
    void Enrollment::list(const Callback& callback) const {
        auto rows = Json::array();
        for (const auto& [name, row] : store.graph().at("repositories").items()) {
            rows.push_back(row);
        }
        callback({200, {{"repositories", rows}}});
    }
    void Enrollment::persist(const Json& row, std::uint64_t generation, const Callback& callback) {
        store.change(
            [row, generation](Json& state) {
                require(state.at("generation") == generation, "graph changed while editing enrollment");
                const auto name = row.at("name").get<std::string>();
                const bool fresh = !state.at("repositories").contains(name);
                require(fresh || state.at("repositories").at(name).at("id") == row.at("id"), "enrolled repository ID changed");
                state["repositories"][name] = row;
                graph::validateSources(state.at("repositories"), state.at("executions"));
                return Json{{"repository", row.at("repository")}, {"new_enrollment", fresh}};
            },
            [callback, row](const DatabaseResult& result, const Json& enrolled) {
                auto body = Json::object();
                if (result.error.empty()) {
                    body.update(row);
                    body.update(enrolled);
                } else {
                    body.update(Json{{"error", result.error}, {"uncertain", result.uncertain}});
                }
                callback({result.error.empty() ? 200 : (result.uncertain || result.code) ? 503 : 409, body});
            });
    }
    void Enrollment::refresh(const Json& input, const Callback& callback) {
        const auto repository = input.at("repository").get<std::string>();
        const auto branch = input.at("branch").get<std::string>();
        const auto generation = store.graph().at("generation").get<std::uint64_t>();
        require(!branch.empty(), "repository branch is required");
        github.request(repository, "GET", "", nullptr, {{"contents", "read"}}, [=, this](const GitHubResult& metadata) {
            if (metadata.status != 200 || !metadata.error.empty() ||
                metadata.body.value("id", std::uint64_t{0}) != github.repositoryId(repository)) {
                callback({503, {{"error", "enrolled repository evidence is unavailable"}}});
            } else {
                sources.definition(repository, branch, [=, this](const SourceResult& result) {
                    if (!result.error.empty()) {
                        callback({result.status, {{"error", result.error}, {"content", result.content}, {"blob_sha", result.blobSha}}});
                    } else {
                        const auto name = repository.substr(repository.find('/') + 1);
                        persist({{"id", metadata.body.at("id")},
                                 {"repository", repository},
                                 {"name", name},
                                 {"branch", branch},
                                 {"revision", result.source.at("commit")},
                                 {"blob_sha", result.blobSha},
                                 {"definition", result.source.at("definition")}},
                                generation,
                                callback);
                    }
                });
            }
        });
    }
    void Enrollment::save(const Json& input, const Callback& callback) {
        const auto repository = input.at("repository").get<std::string>();
        const auto branch = input.at("branch").get<std::string>();
        const auto name = repository.substr(repository.find('/') + 1);
        const auto content = input.at("definition").dump(2) + "\n";
        const auto generation = store.graph().at("generation");
        auto proposed = store.graph().at("repositories");
        const auto enrolled = proposed.value(name, Json::object());
        proposed[name] = {{"repository", repository}, {"definition", SourceReader::parseDefinition(content)}};
        graph::validateSources(proposed, store.graph().at("executions"));
        sources.file(repository, branch, "ci/packages.json", [=, this](const SourceResult& current) {
            if (store.graph().at("generation") != generation || (!current.error.empty() && current.status != 404) ||
                enrolled.value("revision", "") != input.value("revision", "") ||
                current.blobSha != input.value("blob_sha", enrolled.value("blob_sha", ""))) {
                callback({409, {{"error", "committed definition changed; read it before saving"}, {"github_committed", false}}});
            } else {
                Json update{{"message", "Update package definition"}, {"branch", branch}, {"content", base64(content)}};
                if (!current.blobSha.empty()) {
                    update["sha"] = current.blobSha;
                }
                github.request(repository,
                               "PUT",
                               "/contents/ci/packages.json",
                               update,
                               {{"contents", "write"}},
                               [=, this](const GitHubResult& result) {
                                   if ((result.status == 200 || result.status == 201) && result.error.empty()) {
                                       refresh(input, [callback](const EnrollmentResult& enrolled) {
                                           auto body = enrolled.body;
                                           body["github_committed"] = true;
                                           callback({enrolled.status, body});
                                       });
                                   } else {
                                       callback({503,
                                                 {{"error", result.error.empty() ? "GitHub definition commit failed" : result.error},
                                                  {"github_committed", result.uncertain() ? Json() : Json(false)},
                                                  {"uncertain", result.uncertain()}}});
                                   }
                               });
            }
        });
    }
    void Enrollment::workflow(const Json& input, const Callback& callback) {
        const auto repository = input.at("repository").get<std::string>();
        const auto name = repository.substr(repository.find('/') + 1);
        const auto row = store.graph().at("repositories").at(name);
        require(row.at("repository") == repository, "workflow repository is not enrolled");
        const auto proposed = config.sourceContent();
        require(SourceReader::blobSha(proposed) == config.sourceWorkflowBlob, "source workflow template differs from approval");
        sources.file(repository, row.at("branch"), config.sourceWorkflow, [=, this](const SourceResult& current) {
            Json body{{"repository", repository},
                      {"head", current.source.value("commit", "")},
                      {"blob_sha", current.blobSha},
                      {"proposed_blob_sha", config.sourceWorkflowBlob},
                      {"proposed", proposed},
                      {"content", current.content}};
            if (!current.error.empty() && current.status != 404) {
                callback({current.status, {{"error", current.error}}});
            } else if (!input.value("write", false)) {
                callback({200, body});
            } else if (input.value("head", "") != body.at("head").get<std::string>() || input.value("blob_sha", "") != current.blobSha) {
                callback({409, {{"error", "workflow changed after preview"}, {"github_committed", false}}});
            } else if (current.blobSha == config.sourceWorkflowBlob) {
                body["github_committed"] = false;
                callback({200, body});
            } else {
                Json update{{"message", "Install approved package workflow"}, {"branch", row.at("branch")}, {"content", base64(proposed)}};
                if (!current.blobSha.empty()) {
                    update["sha"] = current.blobSha;
                }
                github.request(repository,
                               "PUT",
                               "/contents/" + config.sourceWorkflow,
                               update,
                               {{"contents", "write"}, {"workflows", "write"}},
                               [callback, body](const GitHubResult& result) mutable {
                                   const bool success = (result.status == 200 || result.status == 201) && result.error.empty();
                                   body["github_committed"] = success ? Json(true) : result.uncertain() ? Json() : Json(false);
                                   body["blob_sha"] = success ? body.at("proposed_blob_sha") : body.at("blob_sha");
                                   body["uncertain"] = !success && result.uncertain();
                                   if (!success) {
                                       body["error"] = "GitHub workflow commit did not return confirmed success";
                                   }
                                   callback({success ? 200 : 503, body});
                               });
            }
        });
    }
} // namespace coreci
