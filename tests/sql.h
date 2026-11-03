#ifndef CI_TEST_SQL_H
#define CI_TEST_SQL_H

#include "fixtures.h"

namespace fixture {

    inline Json sqlRows(MYSQL* reader) {
        Json tables = Json::object();
        const Json queries = Json::array({{"repository", "name,body,refs,NULL"},
                                          {"execution", "id,body,refs,JSON_OBJECT('ordinal',ordinal)"},
                                          {"attempt", "id,body,refs,JSON_OBJECT('execution',execution,'ordinal',ordinal)"},
                                          {"content", "hash,body,refs,NULL"},
                                          {"graph_root", "id,body,'{}',NULL"}});
        for (const auto& query : queries) {
            const std::string table = query.at(0);
            const std::string statement = "SELECT " + query.at(1).get<std::string>() + " FROM " + table;
            ci::require(mysql_query(reader, statement.c_str()) == 0, "Independent SQL rows: " + std::string(mysql_error(reader)));
            std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> rows(mysql_store_result(reader), mysql_free_result);
            tables[table] = Json::object();
            MYSQL_ROW row = nullptr;
            while ((row = mysql_fetch_row(rows.get())) != nullptr) {
                const auto* lengths = mysql_fetch_lengths(rows.get());
                Json value = row[3] == nullptr ? Json::object() : Json::parse(row[3]);
                value["body"] = Json::parse(std::string(row[1], lengths[1]));
                value["refs"] = Json::parse(std::string(row[2], lengths[2]));
                tables[table][std::string(row[0], lengths[0])] = value;
            }
        }
        return tables;
    }

    inline Json sqlGraph(const Json& tables) {
        const std::function<Json(const Json&)> expand = [&](const Json& row) {
            Json node = row.at("body");
            for (const auto& [key, reference] : row.at("refs").items()) {
                const Json child = expand(tables.at("content").at(reference.get<std::string>()));
                ci::require(Json(ci::hash(child.dump())) == reference, "Independent expanded content digest");
                if (node.is_array()) {
                    node.at(std::stoull(key)) = child;
                } else {
                    node.at(key) = child;
                }
            }
            return node;
        };
        Json graph = tables.at("graph_root").at("1").at("body");
        graph["repositories"] = Json::object();
        graph["executions"] = Json::array();
        for (const auto& [name, row] : tables.at("repository").items()) {
            graph["repositories"][name] = expand(row);
        }
        for (std::size_t ordinal = 0; ordinal < tables.at("execution").size(); ++ordinal) {
            for (const auto& [id, row] : tables.at("execution").items()) {
                if (row.at("ordinal") == ordinal) {
                    Json work = expand(row);
                    work["id"] = id;
                    work["attempts"] = Json::array();
                    for (const auto& [attemptId, active] : tables.at("attempt").items()) {
                        if (active.at("execution") == id) {
                            Json attempt = expand(active);
                            attempt["id"] = attemptId;
                            work["attempts"][active.at("ordinal").get<std::size_t>()] = attempt;
                        }
                    }
                    graph["executions"].push_back(work);
                }
            }
        }
        return graph;
    }

    inline void audit(MYSQL* reader) {
        ci::require(mysql_query(reader, "CREATE TABLE IF NOT EXISTS test_audit (table_name TEXT,operation TEXT)") == 0, "Audit table");
        for (const std::string table : {"repository", "execution", "attempt", "content", "graph_root"}) {
            for (const std::string operation : {"INSERT", "UPDATE"}) {
                const std::string statement = "CREATE TRIGGER IF NOT EXISTS audit_" + table + operation + " AFTER " + operation + " ON " +
                                              table + " FOR EACH ROW INSERT INTO test_audit VALUES ('" + table + "','" + operation + "')";
                ci::require(mysql_query(reader, statement.c_str()) == 0, "Install independent write audit");
            }
        }
    }

    inline std::size_t writes(MYSQL* reader) {
        ci::require(mysql_query(reader, "SELECT COUNT(*) FROM test_audit") == 0, "Independent write count");
        std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> rows(mysql_store_result(reader), mysql_free_result);
        const std::size_t count = std::stoull(mysql_fetch_row(rows.get())[0]);
        return count;
    }

} // namespace fixture

#endif
