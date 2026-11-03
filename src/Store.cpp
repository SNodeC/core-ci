#include "Auth.h"
#include "Controller.h"

#include <map>

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

        using Sequence = database::mariadb::MariaDBCommandSequence;

        Json unpack(const Json& row, const Json& content, Json& expanded) {
            Json result = row.at("body");
            for (const auto& [key, reference] : row.at("refs").items()) {
                const std::string id = reference;
                if (!expanded.contains(id)) {
                    expanded[id] = unpack(content.at(id), content, expanded);
                }
                result.at(Json::json_pointer("") / key) = expanded.at(id);
            }
            return result;
        }

        std::string identity(const std::string& table) {
            return table == "repository" ? "name" : (table == "content" ? "hash" : "id");
        }

        Json pack(const Json& value, const std::string& context, Json& content) {
            static const std::map<std::string, std::vector<std::string>> policies = {
                {"execution", {"source", "target", "definition", "workflows"}},
                {"attempt", {"assignment", "build_assignment", "publish_assignment", "build_manifest", "build_result", "receipt"}},
                {"source", {"deployment"}},
                {"deployment", {"worker", "source_workflow", "publication_workflow", "feed", "signing"}},
                {"workflows", {"build", "publish"}},
                {"definition", {"matrix", "build", "depends_on"}},
                {"matrix", {"*"}},
                {"assignment", {"source", "target", "dependencies"}},
                {"inputs", {"*"}},
                {"input", {"receipt"}},
                {"document", {"source", "target", "environment", "packages", "development_export", "summary"}},
                {"build_manifest", {"dependencies", "package_version"}},
                {"packages", {"*"}},
                {"development_export", {"compatibility"}},
                {"build_result", {"run", "artifacts"}},
                {"artifacts", {"*"}},
                {"receipt", {"build_job", "run", "build_result", "feed", "compatibility", "indexes", "dependencies"}},
                {"indexes", {"*"}},
                {"receiptdeps", {"*"}},
                {"receiptdep", {"source", "compatibility", "packages", "development_export", "feed"}}};
            Json result = {{"body", value}, {"refs", Json::object()}};
            for (const auto& [key, child] : value.items()) {
                const std::string slot = value.is_array() ? "*" : key;
                const std::string rule =
                    (context == "receipt" || context == "build_manifest") &&
                            std::find(policies.at("document").begin(), policies.at("document").end(), slot) != policies.at("document").end()
                        ? "document"
                        : context;
                if (child.is_structured() && policies.contains(rule) &&
                    std::find(policies.at(rule).begin(), policies.at(rule).end(), slot) != policies.at(rule).end()) {
                    std::string next = key;
                    if (value.is_array()) {
                        next = context == "inputs" ? "input" : (context == "receiptdeps" ? "receiptdep" : "");
                    } else if (key == "dependencies") {
                        next = context == "assignment" ? "inputs" : (context == "receipt" ? "receiptdeps" : "");
                    }
                    const std::string id = hash(child.dump());
                    if (!content.contains(id)) {
                        content[id] = pack(child, next, content);
                        content[id]["hash"] = id;
                    }
                    result["body"].at(Json::json_pointer("") / key) = nullptr;
                    result["refs"][key] = id;
                }
            }
            return result;
        }

        Json materialize(const Json& rows) {
            Json expanded = Json::object();
            Json graph = rows.at("graph_root").at("1").at("body");
            graph["repositories"] = Json::object();
            graph["executions"] = std::vector<Json>(rows.at("execution").size());
            for (const auto& [name, row] : rows.at("repository").items()) {
                graph["repositories"][name] = unpack(row, rows.at("content"), expanded);
            }
            for (const auto& [id, row] : rows.at("execution").items()) {
                const std::size_t index = row.at("ordinal");
                require(index < graph.at("executions").size() && graph.at("executions").at(index).is_null(), "Invalid execution ordinal");
                Json work = unpack(row, rows.at("content"), expanded);
                work["id"] = id;
                work["attempts"] = Json::array();
                graph["executions"][index] = std::move(work);
            }
            for (const auto& [id, row] : rows.at("attempt").items()) {
                const std::size_t index = rows.at("execution").at(row.at("execution").get<std::string>()).at("ordinal");
                const std::size_t ordinal = row.at("ordinal");
                require(ordinal < rows.at("attempt").size(), "Invalid attempt ordinal");
                Json active = unpack(row, rows.at("content"), expanded);
                active["id"] = id;
                graph["executions"][index]["attempts"][ordinal] = std::move(active);
            }
            for (const auto& work : graph.at("executions")) {
                require(!work.at("attempts").empty(), "Execution has no attempts");
                for (const auto& active : work.at("attempts")) {
                    require(active.is_object(), "Missing attempt ordinal");
                }
            }
            return graph;
        }

        std::vector<std::string> rowStatements(const Json& rows) {
            std::vector<std::string> statements{"START TRANSACTION"};

            for (const std::string table : {"content", "repository", "execution", "attempt", "graph_root"}) {
                if (rows.contains(table)) {
                    for (const auto& [id, row] : rows.at(table).items()) {
                        std::string columns;
                        std::string values;
                        for (const auto& [column, value] : row.items()) {
                            if (column != "refs" || table != "graph_root") {
                                columns += (columns.empty() ? "" : ",") + column;
                                values += (values.empty() ? "" : ",") + literal(column == "body" || column == "refs" || !value.is_string()
                                                                                    ? value.dump()
                                                                                    : value.get<std::string>());
                            }
                        }
                        const std::string update = table == "content" ? " ON DUPLICATE KEY UPDATE hash=hash"
                                                   : table == "execution"
                                                       ? ""
                                                       : " ON DUPLICATE KEY UPDATE body=VALUES(body)" +
                                                             std::string(table == "graph_root" ? "" : ",refs=VALUES(refs)");
                        statements.push_back("INSERT INTO " + table + " (" + columns + ") VALUES (" + values + ")" + update);
                    }
                }
            }
            return statements;
        }

        void persist(Store& store, Sequence* sequence, std::vector<std::string> sql, const Done& done) {
            const auto complete = std::make_shared<Done>(done);
            auto statements = std::make_shared<std::vector<std::string>>(std::move(sql));
            const auto failure = [&store, sequence, complete](const std::string& message, unsigned int) {
                const auto finish = [complete, message]() {
                    (*complete)("Database write outcome unresolved; no dispatch: " + message);
                };
                sequence->rollback(finish, [&store, finish](const std::string&, unsigned int) {
                    store.db->abort();
                    store.db.reset();
                    finish();
                });
            };
            const auto next = [sequence, statements, failure, complete](const auto& self, std::size_t index) -> void {
                if (index == statements->size()) {
                    sequence->commit(
                        [complete]() {
                            (*complete)("");
                        },
                        failure);
                } else {
                    sequence->exec(
                        statements->at(index),
                        [self, index]() {
                            self(self, index + 1);
                        },
                        failure);
                }
            };
            next(next, 0);
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
        auto names = std::make_shared<Json>(Json::array());
        auto sequence = std::make_shared<Sequence*>();
        const auto error = [done](const std::string& message, unsigned int) {
            done(message);
        };
        const Value finish = [done](const Json&, const std::string& message) {
            done(message);
        };
        if (db) {
            *sequence = &db->query(
                "SELECT TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND "
                "TABLE_NAME IN ('ci_graph','repository','execution','attempt','content','graph_root')",
                [this, names, sequence, done, finish, error](MYSQL_ROW row) {
                    if (row != nullptr) {
                        names->push_back(row[0]);
                    } else {
                        checked(finish, [this, names, sequence, done, finish, error](const Json&) {
                            require(names->empty() ||
                                        (names->size() == 5 && std::find(names->begin(), names->end(), "ci_graph") == names->end()),
                                    "Legacy or partial controller schema requires explicit offline migration");
                            if (!names->empty()) {
                                read(checked(finish, [done](const Json&) {
                                    done("");
                                }));
                            } else {
                                const Json definitions = Json::array(
                                    {{"repository", "name LONGBLOB NOT NULL,UNIQUE KEY repository_name(name)"},
                                     {"execution",
                                      "id CHAR(64) CHARACTER SET ascii COLLATE ascii_bin PRIMARY KEY,ordinal BIGINT UNSIGNED NOT NULL "
                                      "UNIQUE"},
                                     {"attempt",
                                      "id VARBINARY(85) PRIMARY KEY,execution CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,"
                                      "ordinal BIGINT UNSIGNED NOT NULL,UNIQUE(execution,ordinal),FOREIGN KEY(execution) REFERENCES "
                                      "execution(id)"},
                                     {"content", "hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin PRIMARY KEY"},
                                     {"graph_root", "id TINYINT UNSIGNED PRIMARY KEY CHECK(id=1)"}});

                                std::vector<std::string> statements;
                                for (const auto& definition : definitions) {
                                    const std::string table = definition.at(0);
                                    std::string columns = definition.at(1);
                                    for (const std::string name : {"body", "refs"}) {
                                        if (name == "body" || table != "graph_root") {
                                            columns += "," + name + " LONGTEXT NOT NULL CHECK(JSON_VALID(" + name + "))";
                                        }
                                    }
                                    statements.push_back("CREATE TABLE " + table + " (" + columns +
                                                         ") ENGINE=InnoDB "
                                                         "DEFAULT CHARACTER SET=utf8mb4 COLLATE=utf8mb4_bin");
                                }
                                const auto seed = rowStatements({{"graph_root", {{"1", {{"id", "1"}, {"body", Json::object()}}}}}});
                                statements.insert(statements.end(), seed.begin(), seed.end());
                                persist(*this, *sequence, std::move(statements), done);
                            }
                        })({}, "");
                    }
                },
                error);
        } else {
            done("Database unavailable");
        }
    }

    database::mariadb::MariaDBCommandSequence* Store::read(const Value& done) {
        auto sequence = std::make_shared<Sequence*>();
        auto snapshot = std::make_shared<Json>(Json{{"rows", Json::object()}});
        if (db) {
            const Json queries = Json::array(
                {{"repository", "JSON_QUOTE(CONVERT(name USING utf8mb4)),body,refs,NULL"},
                 {"execution", "JSON_QUOTE(id),body,refs,JSON_OBJECT('ordinal',ordinal)"},
                 {"attempt", "JSON_QUOTE(CONVERT(id USING utf8mb4)),body,refs,JSON_OBJECT('execution',execution,'ordinal',ordinal)"},
                 {"content", "JSON_QUOTE(hash),body,refs,NULL"},
                 {"graph_root", "JSON_QUOTE(CAST(id AS CHAR)),body,'{}',NULL"}});
            const auto select = [this, sequence, snapshot, queries, done](const auto& self, std::size_t index) -> void {
                if (index == queries.size()) {
                    checked(done, [snapshot, done](const Json&) {
                        (*snapshot)["graph"] = materialize(snapshot->at("rows"));
                        done(*snapshot, "");
                    })({}, "");
                } else {
                    const std::string table = queries.at(index).at(0);
                    (*snapshot)["rows"][table] = Json::object();
                    auto& api = *sequence == nullptr ? static_cast<database::mariadb::MariaDBClientASyncAPI&>(*db) : **sequence;
                    *sequence = &api.query(
                        "SELECT " + queries.at(index).at(1).get<std::string>() + " FROM " + table,
                        [self, index, snapshot, table, done](MYSQL_ROW row) {
                            if (row != nullptr && !snapshot->contains("error")) {
                                try {
                                    Json item = row[3] == nullptr ? Json::object() : Json::parse(row[3]);
                                    const std::string id = Json::parse(row[0]);
                                    item[identity(table)] = id;
                                    item["body"] = Json::parse(row[1]);
                                    item["refs"] = Json::parse(row[2]);
                                    (*snapshot)["rows"][table][id] = std::move(item);
                                } catch (const Json::exception&) {
                                    (*snapshot)["error"] = "Invalid stored JSON";
                                }
                            } else if (row == nullptr) {
                                if (snapshot->contains("error")) {
                                    done({}, snapshot->at("error"));
                                } else {
                                    self(self, index + 1);
                                }
                            }
                        },
                        [done](const std::string& message, unsigned int) {
                            done({}, message);
                        });
                }
            };
            select(select, 0);
        } else {
            done({}, "Database unavailable");
        }
        return *sequence;
    }

    void Store::change(const std::function<Json(Json&)>& operation, const Done& done) {
        auto sequence = std::make_shared<Sequence*>();
        const Value failure = [done](const Json&, const std::string& error) {
            done(error);
        };
        *sequence = read(checked(failure, [this, sequence, operation, done](const Json& snapshot) {
            Json graph = snapshot.at("graph");
            const Json authorized = operation(graph);
            if (graph == snapshot.at("graph")) {
                require(authorized.empty(), "Unchanged graph cannot authorize a command");
                done("");
            } else {
                Json content = snapshot.at("rows").at("content");
                Json rows = Json::object();
                Json expanded = Json::object();

                const auto plan = [&](const std::string& table, const std::string& id, Json value, const Json& metadata) {
                    if (table == "execution" || table == "attempt") {
                        value.erase("id");
                    }
                    if (table == "execution") {
                        value.erase("attempts");
                    }
                    const auto& old = snapshot.at("rows").at(table);
                    require(!old.contains(id) || (metadata.empty() ? fields(old.at(id), {"/ordinal", "/execution"}).empty()
                                                                   : fields(old.at(id), {"/ordinal", "/execution"}) == metadata),
                            "Moved persistent identity");
                    require(table != "execution" || !old.contains(id) || unpack(old.at(id), content, expanded) == value,
                            "Immutable execution changed");
                    if (table != "execution" || !old.contains(id)) {
                        Json row = pack(value, table, content);
                        require(unpack(row, content, expanded) == value, "Persistence roundtrip mismatch");
                        row.update(metadata);
                        row[identity(table)] = id;
                        rows[table][id] = std::move(row);
                    }
                };
                Json root = Json::object();
                for (const auto& [key, value] : graph.items()) {
                    if (key != "executions" && key != "repositories") {
                        root[key] = value;
                    }
                }
                if (root != snapshot.at("rows").at("graph_root").at("1").at("body")) {
                    plan("graph_root", "1", root, Json::object());
                }

                Json updates = Json::array();
                for (const std::string section : {"repositories", "executions"}) {
                    const auto& before = snapshot.at("graph").at(section);
                    const auto& after = graph.at(section);
                    for (const auto& [key, value] : before.items()) {
                        require(after.contains(Json::json_pointer("") / key), "Persistent record removed");
                    }
                    for (const auto& [key, value] : after.items()) {
                        const auto pointer = Json::json_pointer("") / key;
                        const bool exists = before.contains(pointer);
                        if (!exists || before.at(pointer) != value) {
                            const std::string id = section == "repositories" ? key : value.at("id").get<std::string>();
                            if (section == "repositories") {
                                plan("repository", id, value, Json::object());
                            } else {
                                require(!exists || before.at(pointer).at("id") == id, "Execution moved");
                                plan("execution", id, value, {{"ordinal", std::stoull(key)}});
                                const Json old = exists ? before.at(pointer).at("attempts") : Json::array();
                                require(value.at("attempts").size() >= old.size(), "Attempt removed");
                                for (std::size_t j = 0; j < value.at("attempts").size(); ++j) {
                                    const auto& active = value.at("attempts").at(j);
                                    require(j >= old.size() || active.at("id") == old.at(j).at("id"), "Attempt moved");
                                    if (j >= old.size() || active != old.at(j)) {
                                        plan("attempt", active.at("id"), active, {{"execution", id}, {"ordinal", j}});
                                    }
                                }
                            }
                            if (!updates.is_null()) {
                                try {
                                    const auto project = [&](const Json& record) {
                                        return section == "executions" ? projectWork(record, std::stoull(key))
                                                                       : projectRepositories({{key, record}}).at(key);
                                    };
                                    const Json previous = exists ? Json{{id, project(before.at(pointer))}} : Json::object();
                                    const Json patch = Json::diff(previous, {{id, project(value)}}, "/" + section);
                                    updates.insert(updates.end(), patch.begin(), patch.end());
                                } catch (...) {
                                    updates = nullptr;
                                }
                            }
                        }
                    }
                }
                for (const auto& [id, row] : content.items()) {
                    if (!snapshot.at("rows").at("content").contains(id)) {
                        rows["content"][id] = row;
                    }
                }
                for (const auto& action : authorized) {
                    require(Json(hash(action.at("assignment").dump())) ==
                                rows.at("attempt").at(action.at("attempt").get<std::string>()).at("refs").at("assignment"),
                            "Authorized assignment differs from frozen root");
                }

                persist(*this, *sequence, rowStatements(rows), [this, updates, authorized, done](const std::string& message) {
                    if (message.empty()) {
                        if (dispatch && !authorized.empty()) {
                            dispatch(authorized);
                        }
                        if (changed && (updates.is_null() || !updates.empty())) {
                            changed(updates);
                        }
                    }
                    done(message);
                });
            }
        }));
    }

} // namespace ci
