#include "Controller.h"

namespace ci {

    namespace {

        Json fields(const Json& value, std::initializer_list<const char*> paths) {
            Json result;
            for (const auto* const path : paths) {
                const auto pointer = Json::json_pointer(path);
                if (value.contains(pointer)) {
                    result[pointer] = value.at(pointer);
                }
            }
            return result;
        }

        std::string literal(const std::string& text) {
            constexpr char digits[] = "0123456789abcdef";
            std::string result = "CONVERT(0x";
            for (const unsigned char byte : text) {
                result += digits[byte >> 4];
                result += digits[byte & 15];
            }
            return result + " USING utf8mb4)";
        }

    } // namespace

    Json projectRepositories(const Json& repositories) {
        Json result = Json::object();
        for (const auto& [name, entry] : repositories.items()) {
            result[name] = fields(entry, {"/ref", "/commit", "/definition"});
            result[name]["title"] = entry.at("definition").at("title");
        }
        return result;
    }

    Json projectWork(const Json& work, std::size_t index) {
        const auto& active = work.at("attempts").back();
        Json result = fields(work, {"/id", "/selection", "/prerequisites"});
        result["index"] = index;
        result["created"] = work.at("attempts").front().at("created");
        result["source"] = fields(work.at("source"), {"/repository", "/package", "/tag"});
        result["target"] = fields(work.at("target"), {"/id", "/distribution", "/suite", "/arch", "/build/format"});
        result["previous"] = Json::array();
        for (const auto& item : work.at("attempts")) {
            Json summary = fields(item, {"/id", "/revision", "/state", "/failure", "/failure_kind"});
            summary["uncertainty"] = fields(item.value("uncertainty", Json()), {"/at", "/stage", "/kind", "/reason"});
            summary["retirement"] = fields(item.value("retirement", Json()), {"/acknowledgement", "/at", "/remote_outcome"});
            result["previous"].push_back(summary);
        }
        result["active"] = result["previous"].back();
        result["previous"].erase(result["previous"].end() - 1);
        const auto verification = fields(active, {"/verification_started_at", "/proof/verified_at"});
        for (const auto& [key, value] : verification.items()) {
            result["active"][key] = value;
        }
        result["active"]["assigned"] = active.contains("assignment");
        result["active"]["publish_assigned"] = active.contains("publish_assignment");
        result["active"]["publish_run_bound"] = active.at("facts").contains("publish_run_id");
        for (const std::string stage : {"build", "publish"}) {
            const Json observed = active.at("facts").value(stage + "_run", Json::object());
            Json run = fields(observed, {"/status", "/conclusion", "/identity/html_url"});
            run["send"] = active.at("facts").value(stage + "_send", Json::object()).value("outcome", Json());
            for (const auto& job : observed.value("jobs", Json::array())) {
                if (job.at("id") == observed.value("job_id", Json())) {
                    run["job"] = fields(job, {"/status", "/conclusion", "/started_at", "/completed_at", "/html_url"});
                }
                for (const auto& step : job.value("steps", Json::array())) {
                    if (!run.contains("step") && step.at("status") == "in_progress") {
                        run["step"] = step.at("name");
                    }
                }
            }
            result[stage] = run;
        }
        if (active.at("state") == "published") {
            const auto& receipt = active.at("receipt");
            const std::string path = receipt.at("packages").at(0).at("path");
            std::string directory;
            for (const char c : path.substr(0, path.rfind('/') == std::string::npos ? 0 : path.rfind('/'))) {
                directory += std::string("/!'()*").find(c) != std::string::npos ? std::string(1, c) : encode(std::string(1, c));
            }
            result["packages"] = "https://github.com/" + receipt.at("feed").at("repository").get<std::string>() + "/tree/" +
                                 active.at("proof").at("feed_revision").get<std::string>() + "/" + directory;
        }
        return result;
    }

    void Store::initialize(const Done& done) {
        const auto onError = [done](const std::string& error, unsigned int) {
            done(error);
        };
        db->endTransactions(
            [this, done, onError]() {
                db->exec(
                    "CREATE TABLE IF NOT EXISTS ci_graph (id INT PRIMARY KEY, revision BIGINT UNSIGNED NOT NULL, "
                    "body LONGTEXT NOT NULL CHECK(JSON_VALID(body))) ENGINE=InnoDB",
                    [this, done, onError]() {
                        const Json empty = {{"repositories", Json::object()}, {"executions", Json::array()}};
                        db->exec(
                            "INSERT IGNORE INTO ci_graph VALUES (1,0," + literal(empty.dump()) + ")",
                            [done]() {
                                done("");
                            },
                            onError);
                    },
                    onError);
            },
            onError);
    }

    database::mariadb::MariaDBCommandSequence& Store::read(const Value& done) {
        auto snapshot = std::make_shared<Json>();
        return db->query(
            "SELECT body,revision FROM ci_graph WHERE id=1",
            [snapshot, done](MYSQL_ROW row) {
                if (row != nullptr) {
                    try {
                        *snapshot = {{"graph", Json::parse(row[0])}, {"revision", std::stoull(row[1])}};
                    } catch (const std::exception& error) {
                        *snapshot = {{"error", error.what()}};
                    }
                } else if (snapshot->contains("graph")) {
                    done(*snapshot, "");
                } else {
                    done({}, snapshot->value("error", "Graph row missing"));
                }
            },
            [done](const std::string& error, unsigned int) {
                done({}, error);
            });
    }

    void Store::change(const std::function<Json(Json&)>& operation, const Done& done) {
        auto sequence = std::make_shared<database::mariadb::MariaDBCommandSequence*>();
        const Value failure = [done](const Json&, const std::string& error) {
            done(error);
        };
        *sequence = &read(checked(failure, [this, sequence, operation, done](const Json& snapshot) {
            Json graph = snapshot.at("graph");
            const Json authorized = operation(graph);
            if (graph == snapshot.at("graph")) {
                require(authorized.empty(), "Unchanged graph cannot authorize a command");
                done("");
            } else {
                Json updates = Json::array();
                try {
                    for (const std::string section : {"executions", "repositories"}) {
                        const auto& before = snapshot.at("graph").at(section);
                        const auto& after = graph.at(section);
                        for (const auto& [key, value] : before.items()) {
                            require(after.contains(Json::json_pointer("") / key), "UI record removed");
                        }
                        for (const auto& [key, value] : after.items()) {
                            const auto item = Json::json_pointer("") / key;
                            const bool exists = before.contains(item);
                            if (!exists || before.at(item) != value) {
                                require(!exists || section != "executions" || before.at(item).at("id") == value.at("id"),
                                        "UI record moved");
                                const auto project = [&](const Json& record) {
                                    return section == "executions" ? projectWork(record, std::stoull(key))
                                                                   : projectRepositories({{key, record}}).at(key);
                                };
                                const std::string id = section == "executions" ? value.at("id").get<std::string>() : key;
                                const Json previous = exists ? Json{{id, project(before.at(item))}} : Json::object();
                                const Json patch = Json::diff(previous, {{id, project(value)}}, "/" + section);
                                updates.insert(updates.end(), patch.begin(), patch.end());
                            }
                        }
                    }
                } catch (...) {
                    updates = nullptr;
                }
                (*sequence)->exec(
                    "UPDATE ci_graph SET revision=revision+1,body=" + literal(graph.dump()) + " WHERE id=1",
                    [this, updates, authorized, done]() {
                        if (dispatch && !authorized.empty()) {
                            dispatch(authorized);
                        }
                        if (changed && (updates.is_null() || !updates.empty())) {
                            changed(updates);
                        }
                        done("");
                    },
                    [done](const std::string& message, unsigned int) {
                        done("Database write outcome unresolved; no dispatch: " + message);
                    });
            }
        }));
    }

} // namespace ci
