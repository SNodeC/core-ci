#include "Auth.h"
#include "Controller.h"

#include <algorithm>
#include <regex>
#include <set>

namespace ci {

    Controller::Controller(Store& store, Json deployment, std::string app, std::string installation, std::string key)
        : deployment(std::move(deployment))
        , store(store)
        , app(std::move(app))
        , installation(std::move(installation))
        , key(std::move(key)) {
    }

    void Controller::token(const std::function<void(const std::string&, const std::string&)>& done) {
        Json ids = Json::array();
        for (const auto& id : deployment.at("repositories")) {
            ids.push_back(id);
        }
        const Json scope = {{"repository_ids", ids}, {"permissions", {{"contents", "read"}, {"actions", "write"}}}};
        https("https://api.github.com/app/installations/" + installation + "/access_tokens",
              "POST",
              scope.dump(),
              appJwt(key, app),
              [done](int status, const std::string& body, const std::string&) {
                  std::string value;
                  std::string error;
                  try {
                      require(status == 201 && body.size() <= 2 * 1024 * 1024, "GitHub installation token unavailable");
                      value = Json::parse(body).at("token");
                      require(!value.empty() && value.size() < 10000, "Invalid installation token");
                  } catch (const std::exception& failure) {
                      error = failure.what();
                  }
                  done(value, error);
              });
    }

    void Controller::api(const std::string& token, const std::string& path, const Value& done) {
        require(path.starts_with("/") && !path.starts_with("//"), "Invalid GitHub path");
        https("https://api.github.com" + path, "GET", "", token, [done](int status, const std::string& body, const std::string&) {
            Json result;
            std::string error;
            try {
                require(status == 200 && body.size() <= 2 * 1024 * 1024, "GitHub read unavailable: HTTP " + std::to_string(status));
                result = Json::parse(body);
            } catch (const std::exception& failure) {
                error = failure.what();
            }
            done(result, error);
        });
    }

    void Controller::pages(
        const std::string& token, const std::string& path, const std::string& field, const Value& done, int page, Json collected) {
        api(token,
            path + (path.find('?') == std::string::npos ? "?" : "&") + "per_page=100&page=" + std::to_string(page),
            checked(done, [this, token, path, field, done, page, collected](const Json& result) mutable {
                const Json& items = field.empty() ? result : result.at(field);
                require(items.is_array(), "Expected GitHub result list");
                collected.insert(collected.end(), items.begin(), items.end());
                if (items.size() == 100) {
                    pages(token, path, field, done, page + 1, collected);
                } else {
                    done(collected, "");
                }
            }));
    }

    void Controller::contents(
        const std::string& token, const std::string& repository, const std::string& path, const std::string& ref, const Value& done) {
        api(token, "/repos/" + repository + "/contents/" + path + "?ref=" + encode(ref), checked(done, [done](const Json& file) {
                require(file.at("type") == "file" && file.at("encoding") == "base64", "Expected committed regular GitHub file");
                const std::string bytes = unbase64(file.at("content"));
                require(bytes.size() <= 2 * 1024 * 1024 &&
                            file.at("sha") == hash("blob " + std::to_string(bytes.size()) + std::string(1, '\0') + bytes, "SHA1"),
                        "Committed Git blob does not match bytes");
                done({{"bytes", bytes}, {"sha", file.at("sha")}}, "");
            }));
    }

    void Controller::enroll(const std::string& repository, const std::string& ref, const Done& done) {
        const Value finish = [done](const Json&, const std::string& error) {
            done(error);
        };
        require(deployment.at("repositories").contains(repository) &&
                    repository.starts_with(deployment.at("owner").get<std::string>() + "/"),
                "Repository is outside authorized grants");
        require(!ref.empty(), "Enrollment requires an explicit definition/workflow branch");
        token([this, repository, ref, finish, done](const std::string& access, const std::string& error) {
            checked(finish, [this, repository, ref, access, finish, done](const Json&) {
                api(access, "/repos/" + repository, checked(finish, [this, repository, ref, access, finish, done](const Json& metadata) {
                        require(metadata.at("id") == deployment.at("repositories").at(repository) && metadata.at("full_name") == repository,
                                "Repository stable identity differs from grant");
                        api(access,
                            "/repos/" + repository + "/commits/" + encode(ref),
                            checked(finish, [this, repository, ref, access, finish, done](const Json& commit) {
                                contents(access,
                                         repository,
                                         "ci/packages.json",
                                         commit.at("sha"),
                                         checked(finish, [this, repository, ref, commit, done](const Json& file) {
                                             const Json definition = Json::parse(file.at("bytes").get<std::string>());
                                             require(definition.at("schema") == 1 && definition.at("matrix").is_array(),
                                                     "Invalid definition schema");
                                             store.change(
                                                 [repository, ref, commit, file, definition](Json& graph) {
                                                     for (const auto& [name, enrolled] : graph.at("repositories").items()) {
                                                         require(name == repository ||
                                                                     enrolled.at("definition").at("package") != definition.at("package"),
                                                                 "Package belongs to another enrolled repository");
                                                     }
                                                     graph["repositories"][repository] = {{"ref", ref},
                                                                                          {"commit", commit.at("sha")},
                                                                                          {"blob", file.at("sha")},
                                                                                          {"definition", definition},
                                                                                          {"refreshed", utc()}};
                                                     return Json::array();
                                                 },
                                                 done);
                                         }));
                            }));
                    }));
            })({}, error);
        });
    }

    void Controller::resolve(const std::string& token,
                             const Json& enrollment,
                             const std::string& repository,
                             const std::string& tag,
                             const std::shared_ptr<Selection>& selection) {
        if (!selection->sources.contains(repository)) {
            selection->sources[repository] = nullptr;
            ++selection->pending;
            const Value failed = [selection](const Json&, const std::string& error) {
                if (!error.empty() && selection->done) {
                    auto finish = std::exchange(selection->done, {});
                    finish({}, error);
                }
            };
            const auto selectedTag = [this, token, enrollment, repository, tag, selection, failed](const Json& tags) {
                const std::string chosen = sourceTag(tags, tag);
                api(token,
                    "/repos/" + repository + "/commits/" + encode("refs/tags/" + chosen),
                    checked(failed, [this, token, enrollment, repository, chosen, selection, failed](const Json& commit) {
                        contents(
                            token,
                            repository,
                            "ci/packages.json",
                            commit.at("sha"),
                            checked(failed, [this, token, enrollment, repository, chosen, commit, selection, failed](const Json& file) {
                                const Json definition = Json::parse(file.at("bytes").get<std::string>());
                                require(definition.at("schema") == 1 && definition.at("matrix").is_array() &&
                                            !definition.at("matrix").empty(),
                                        "Invalid source definition");
                                require(
                                    std::regex_match(definition.at("package").get<std::string>(), std::regex("[a-z0-9][a-z0-9+.-]{0,127}")),
                                    "Invalid package namespace");
                                std::set<std::string> unique;
                                for (const auto& dependency : definition.at("depends_on")) {
                                    require(dependency.is_string() && unique.insert(dependency.get<std::string>()).second,
                                            "Invalid duplicate dependency");
                                }
                                unique.clear();
                                for (const auto& target : definition.at("matrix")) {
                                    coordinates(target);
                                    require(unique.insert(target.at("id").get<std::string>()).second, "Duplicate target ID");
                                    require(definition.at("build").contains(target.at("family").get<std::string>()), "Missing build hook");
                                }
                                Json source = definition;
                                source.erase("matrix");
                                source.erase("schema");
                                Json pins;
                                for (const auto& key :
                                     {"controller_app_bot_id", "worker", "source_workflow", "publication_workflow", "feed", "signing"}) {
                                    pins[key] = deployment.at(key);
                                }
                                source.update({{"repository", repository},
                                               {"repository_id", deployment.at("repositories").at(repository)},
                                               {"tag", chosen},
                                               {"commit", commit.at("sha")},
                                               {"definition_revision", commit.at("sha")},
                                               {"definition_blob_sha", file.at("sha")},
                                               {"deployment", pins}});
                                const std::string ref = enrollment.at(repository).at("ref");
                                api(token,
                                    "/repos/" + repository + "/commits/" + encode(ref),
                                    checked(failed,
                                            [this, token, enrollment, repository, ref, source, definition, selection, failed](
                                                const Json& commit) {
                                                contents(
                                                    token,
                                                    repository,
                                                    deployment.at("source_workflow").at("path"),
                                                    commit.at("sha"),
                                                    checked(
                                                        failed,
                                                        [this, token, enrollment, repository, ref, source, definition, commit, selection](
                                                            const Json& file) {
                                                            require(file.at("sha") == deployment.at("source_workflow").at("blob_sha"),
                                                                    "Unapproved source workflow");
                                                            selection->sources[repository] = {
                                                                {"source", source},
                                                                {"definition", definition},
                                                                {"workflows",
                                                                 {{"build",
                                                                   {{"repository", repository},
                                                                    {"repository_id", source.at("repository_id")},
                                                                    {"ref", ref},
                                                                    {"commit", commit.at("sha")},
                                                                    {"path", deployment.at("source_workflow").at("path")},
                                                                    {"blob_sha", file.at("sha")}}}}}};
                                                            for (const auto& dependency : definition.at("depends_on")) {
                                                                std::string match;
                                                                for (const auto& [name, value] : enrollment.items()) {
                                                                    if (value.at("definition").at("package") == dependency) {
                                                                        require(match.empty(), "Ambiguous enrolled package");
                                                                        match = name;
                                                                    }
                                                                }
                                                                require(!match.empty(), "Dependency repository is not enrolled");
                                                                resolve(token, enrollment, match, "", selection);
                                                            }
                                                            --selection->pending;
                                                            if (selection->pending == 0 && selection->done) {
                                                                auto finish = std::exchange(selection->done, {});
                                                                finish(selection->sources, "");
                                                            }
                                                        }));
                                            }));
                            }));
                    }));
            };
            api(token, "/repos/" + repository, checked(failed, [this, token, repository, tag, failed, selectedTag](const Json& metadata) {
                    require(metadata.at("id") == deployment.at("repositories").at(repository) && metadata.at("full_name") == repository,
                            "Selected repository stable identity mismatch");
                    if (tag.empty()) {
                        pages(token, "/repos/" + repository + "/tags", "", checked(failed, selectedTag));
                    } else {
                        checked(failed, selectedTag)(Json::array(), "");
                    }
                }));
        }
    }

    void Controller::request(const std::string& repository, const std::string& tag, const Done& done, const std::string& expected) {
        const Value finish = [done](const Json&, const std::string& error) {
            done(error);
        };
        store.read(checked(finish, [this, repository, tag, expected, done, finish](const Json& snapshot) {
            const Json enrollment = snapshot.at("graph").at("repositories");
            require(enrollment.contains(repository), "Repository is not enrolled");
            if (!expected.empty() && selectedTag(snapshot.at("graph"), repository, tag, expected)) {
                done("");
            } else {
                token([this, repository, tag, expected, enrollment, done, finish](const std::string& access, const std::string& error) {
                    checked(finish, [this, repository, tag, expected, enrollment, access, done, finish](const Json&) {
                        const Json publisher = deployment.at("publication_workflow");
                        const Json worker = deployment.at("worker");
                        api(access,
                            "/repos/" + worker.at("repository").get<std::string>() + "/commits/" + encode(publisher.at("ref")),
                            checked(
                                finish,
                                [this, repository, tag, expected, enrollment, access, publisher, worker, done, finish](const Json& commit) {
                                    contents(
                                        access,
                                        worker.at("repository"),
                                        publisher.at("path"),
                                        commit.at("sha"),
                                        checked(
                                            finish,
                                            [this, repository, tag, expected, enrollment, access, publisher, worker, commit, done, finish](
                                                const Json& file) {
                                                require(file.at("sha") == publisher.at("blob_sha"), "Unapproved publication workflow");
                                                auto selection = std::make_shared<Selection>();
                                                selection->done = checked(
                                                    finish,
                                                    [this, repository, expected, publisher, worker, commit, done](const Json& result) {
                                                        Json selection = result, workflow = publisher;
                                                        workflow.update({{"repository", worker.at("repository")},
                                                                         {"repository_id", worker.at("repository_id")},
                                                                         {"commit", commit.at("sha")}});
                                                        for (auto& selected : selection) {
                                                            selected["workflows"]["publish"] = workflow;
                                                        }
                                                        store.change(
                                                            [selection, repository, expected](Json& graph) {
                                                                return selectRequest(graph, selection, repository, expected);
                                                            },
                                                            done);
                                                    });
                                                for (const auto& name : affectedRepositories(enrollment, repository)) {
                                                    resolve(access, enrollment, name, name == repository ? tag : "", selection);
                                                }
                                            }));
                                }));
                    })({}, error);
                });
            }
        }));
    }

} // namespace ci
