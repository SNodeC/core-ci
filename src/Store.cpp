#include "Controller.h"

namespace ci {

    namespace {

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

    void Store::initialize(const Done& done) {
        db->endTransactions(
            [this, done]() {
                db->exec(
                    "CREATE TABLE IF NOT EXISTS ci_graph (id INT PRIMARY KEY, revision BIGINT UNSIGNED NOT NULL, "
                    "body LONGTEXT NOT NULL CHECK(JSON_VALID(body))) ENGINE=InnoDB",
                    [this, done]() {
                        const Json empty = {{"repositories", Json::object()}, {"executions", Json::array()}};
                        db->exec(
                            "INSERT IGNORE INTO ci_graph VALUES (1,0," + literal(empty.dump()) + ")",
                            [done]() {
                                done("");
                            },
                            [done](const std::string& error, unsigned int) {
                                done(error);
                            });
                    },
                    [done](const std::string& error, unsigned int) {
                        done(error);
                    });
            },
            [done](const std::string& error, unsigned int) {
                done(error);
            });
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
        *sequence = &read([this, sequence, operation, done](const Json& snapshot, const std::string& error) {
            if (!error.empty()) {
                done(error);
            } else {
                try {
                    Json graph = snapshot.at("graph");
                    const Json authorized = operation(graph);
                    const Json updates = Json::diff(snapshot.at("graph"), graph);
                    if (updates.empty()) {
                        require(authorized.empty(), "Unchanged graph cannot authorize a command");
                        done("");
                    } else {
                        (*sequence)->exec(
                            "UPDATE ci_graph SET revision=revision+1,body=" + literal(graph.dump()) + " WHERE id=1",
                            [this, updates, authorized, done]() {
                                if (dispatch && !authorized.empty()) {
                                    dispatch(authorized);
                                }
                                if (changed) {
                                    changed(updates);
                                }
                                done("");
                            },
                            [done](const std::string& message, unsigned int) {
                                done("Database write outcome unresolved; no dispatch: " + message);
                            });
                    }
                } catch (const std::exception& failure) {
                    done(failure.what());
                }
            }
        });
    }

} // namespace ci
