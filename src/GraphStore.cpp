#include "GraphStore.h"

#include "Version.h"

#include <algorithm>

namespace coreci {
    void GraphStore::restore(const Database::Callback& callback) {
        database.query("SELECT document FROM graph_state WHERE id=1", [this, callback](const DatabaseResult& read) {
            auto result = read;
            if (result.error.empty()) {
                try {
                    require(result.rows.size() == 1, "missing graph state; provision the database before starting");
                    const auto& restored = result.rows.front();
                    require(restored.at("executions").is_object() && restored.at("repositories").is_object() &&
                                restored.at("next_revision").get<std::uint64_t>() > 0 && restored.at("generation").is_number_unsigned(),
                            "invalid stored graph document");
                    state = restored;
                } catch (const std::exception& error) {
                    result.error = error.what();
                }
            }
            callback(result);
        });
    }

    void GraphStore::change(const std::function<Json(Json&)>& apply, const Callback& callback) {
        struct Change {
            Json next;
            Json result;
        };
        const auto change = std::make_shared<Change>();
        database.write(
            [this, apply, change] {
                change->next = state;
                change->result = apply(change->next);
                std::string sql;
                if (change->next != state) {
                    change->next["generation"] = state.at("generation").get<std::uint64_t>() + 1;
                    sql = "UPDATE graph_state SET document=" + Database::text(change->next.dump()) +
                          " WHERE id=1 AND JSON_VALUE(document,'$.generation')=" + state.at("generation").dump();
                }
                return sql;
            },
            [this, change, callback](const DatabaseResult& result) {
                const bool committed = result.error.empty() && change->next != state;
                const bool repositories = committed && change->next.at("repositories") != state.at("repositories");
                bool publicChange = false;
                if (committed) {
                    auto before = publicView(state);
                    auto after = publicView(change->next);
                    before["generation"] = after.at("generation");
                    publicChange = before != after;
                    state = std::move(change->next);
                }
                callback(result, change->result);
                if (onChanged && (committed || result.uncertain)) {
                    onChanged(publicChange, repositories);
                }
            });
    }

    Json GraphStore::view() const {
        return view(state);
    }
    Json GraphStore::view(const Json& state) const {
        auto executions = Json::array();
        for (const auto& [id, node] : state.at("executions").items()) {
            const auto& attempt = node.at("attempts").back();
            const auto& source = node.at("source");
            Json row = attempt;
            row["id"] = id;
            row["attempt"] = attempt.at("id");
            row["target"] = node.at("target_row").at("id");
            for (const auto* key : {"target_row", "prerequisites", "automatic", "superseded_by"}) {
                row[key] = node.at(key);
            }
            for (const auto* key : {"repository", "tag", "definition_revision", "package", "title"}) {
                row[key] = source.at(key);
            }
            row["source_commit"] = source.at("commit");
            row["label"] = attempt.at("assignment").is_null() ? source.at("title") : attempt.at("assignment").at("label");
            row["retirable"] = graph::retirable(node);
            row["blockers"] = graph::blockers(state, node);
            row["build_job"] = attempt.at("evidence").value("build", Json::object());
            row["publication_job"] = attempt.at("evidence").value("publication", Json::object());
            for (const auto* key : {"build_job", "publication_job"}) {
                if (row.at(key).contains("job_status")) {
                    row[key]["status"] = row.at(key).at("job_status");
                }
            }
            row["activity"] = attempt.at("state");
            const auto stage = attempt.at("state").get<std::string>();
            if (stage == "building" || stage == "publishing") {
                const auto& facts = row.at(stage == "building" ? "build_job" : "publication_job");
                row["activity"] =
                    stage + ": " + facts.value("status", stage == "publishing" ? "publication run unconfirmed" : "build run unconfirmed");
                if (stage == "building" && !attempt.at("build_result").is_null()) {
                    row["activity"] = "build complete; waiting for publication";
                }
            }
            for (const auto* key : {"assignment", "build_result", "preflight", "dispatch"}) {
                row.erase(key);
            }
            executions.push_back(std::move(row));
        }
        return {{"generation", state.at("generation")}, {"mutations_allowed", database.mutationsAllowed()}, {"executions", executions}};
    }

    Json GraphStore::publicView() const {
        return publicView(state);
    }
    Json GraphStore::publicView(const Json& state) const {
        auto projects = Json::object();
        auto executions = Json::array();
        const auto snapshot = view(state);
        for (const auto& row : snapshot.at("executions")) {
            Json item;
            for (const auto* key : {"id",
                                    "repository",
                                    "tag",
                                    "source_commit",
                                    "definition_revision",
                                    "target",
                                    "state",
                                    "activity",
                                    "revision",
                                    "attempt",
                                    "prerequisites",
                                    "blockers",
                                    "failure",
                                    "superseded_by",
                                    "package",
                                    "build_job",
                                    "publication_job"}) {
                item[key] = row.at(key);
            }
            item["coordinates"] = Json::object();
            for (const auto* key : {"distribution", "suite", "arch"}) {
                item["coordinates"][key] = row.at("target_row").at(key);
            }
            item["coordinates"]["format"] = row.at("target_row").at("build").at("format");
            item["build_url"] = row.at("build_run").is_null() ? Json() : row.at("build_run").at("html_url");
            item["build_job_url"] = nullptr;
            if (!item.at("build_url").is_null() && row.at("build_job").contains("job_id")) {
                item["build_job_url"] = item.at("build_url").get<std::string>() + "/job/" + row.at("build_job").at("job_id").dump();
            }
            item["publication_verified_utc"] = row.at("receipt").is_null() ? Json() : row.at("receipt").at("publication_verified_utc");
            const auto repository = item.at("repository").get<std::string>();
            if (!projects.contains(repository)) {
                projects[repository] = {{"repository", repository}, {"title", row.at("title")}, {"versions", Json::array()}};
            }
            auto& versions = projects.at(repository).at("versions");
            if (std::find(versions.begin(), versions.end(), row.at("tag")) == versions.end()) {
                versions.push_back(row.at("tag"));
            }
            executions.push_back(std::move(item));
        }
        auto ordered = Json::array();
        for (auto& [repository, project] : projects.items()) {
            auto& versions = project.at("versions");
            std::sort(versions.begin(), versions.end(), [](const Json& left, const Json& right) {
                return Version(left.get<std::string>()).compare(Version(right.get<std::string>())) > 0;
            });
            ordered.push_back(project);
        }
        return {{"schema", 1}, {"generation", state.at("generation")}, {"projects", ordered}, {"executions", executions}};
    }
} // namespace coreci
