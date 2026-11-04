#include "Enrollment.h"

namespace coreci {
    Enrollment::Enrollment(GitHub& github, GraphStore& store, SourceReader& sources)
        : github(github)
        , store(store)
        , sources(sources) {
    }
    Json Enrollment::list() const {
        auto rows = Json::array();
        for (const auto& [name, row] : store.graph().at("repositories").items()) {
            rows.push_back(row);
        }
        return {{"repositories", rows}};
    }
    void Enrollment::persist(const Json& row, const Json& expected, const Callback& callback) {
        store.change(
            [row, expected](Json& state) {
                const auto name = row.at("name").get<std::string>();
                require(state.at("repositories").value(name, Json::object()) == expected,
                        "repository enrollment changed during GitHub read");
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
        const auto name = repository.substr(repository.find('/') + 1);
        const auto previous = store.graph().at("repositories").value(name, Json::object());
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
                        persist({{"id", metadata.body.at("id")},
                                 {"repository", repository},
                                 {"name", name},
                                 {"branch", branch},
                                 {"revision", result.source.at("commit")},
                                 {"blob_sha", result.blobSha},
                                 {"definition", result.source.at("definition")}},
                                previous,
                                callback);
                    }
                });
            }
        });
    }
} // namespace coreci
