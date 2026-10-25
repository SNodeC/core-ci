#include "Database.h"

#include <errmsg.h>
#include <exception>
#include <utility>

namespace coreci {

    Database::Database(database::mariadb::MariaDBClient& client)
        : client(client) {
    }

    bool Database::mutationsAllowed() const {
        return !mutationUncertain;
    }

    std::string Database::text(const std::string& value) {
        constexpr char digits[] = "0123456789abcdef";
        std::string result = "CONVERT(X'";
        for (const unsigned char byte : value) {
            result.push_back(digits[byte >> 4]);
            result.push_back(digits[byte & 15]);
        }
        result += "' USING utf8mb4)";
        return result;
    }

    void Database::enqueue(const std::function<void()>& operation) {
        operations.push_back(operation);
        if (operations.size() == 1) {
            const auto start = operations.front();
            start();
        }
    }

    void Database::completed() {
        operations.pop_front();
        if (!operations.empty()) {
            const auto start = operations.front();
            start();
        }
    }

    void Database::query(const std::string& sql, const Callback& callback) {
        enqueue([this, sql, callback]() {
            read(sql, [this, callback](const DatabaseResult& result) {
                completed();
                callback(result);
            });
        });
    }

    void Database::read(const std::string& sql, const Callback& callback) {
        struct Query {
            DatabaseResult result;
            bool finished = false;
        };
        const auto query = std::make_shared<Query>();
        client.query(
            sql,
            [query, callback](const MYSQL_ROW row) {
                if (!query->finished) {
                    if (row != nullptr) {
                        try {
                            query->result.rows.push_back(row[0] == nullptr ? nlohmann::json{} : nlohmann::json::parse(row[0]));
                        } catch (const std::exception&) {
                            query->result.error = "database returned invalid JSON";
                        }
                    } else {
                        query->finished = true;
                        callback(query->result);
                    }
                }
            },
            [query, callback](const std::string& error, unsigned int) {
                if (!query->finished) {
                    query->finished = true;
                    query->result.error = error;
                    callback(query->result);
                }
            });
    }

    void Database::write(const std::vector<DatabaseStatement>& statements, const Callback& callback) {
        const auto write = std::make_shared<Write>();
        write->statements = statements;
        write->callback = callback;
        enqueue([this, write]() {
            if (mutationUncertain) {
                completed();
                write->callback({{}, "database mutation outcome needs reconciliation before further writes", true});
            } else {
                client.exec(
                    "START TRANSACTION",
                    [this, write]() {
                        writeNext(write);
                    },
                    [this, write](const std::string& error, unsigned int) {
                        completed();
                        write->callback({{}, error, false});
                    });
            }
        });
    }

    void Database::writeNext(const std::shared_ptr<Write>& write) {
        if (write->index < write->statements.size()) {
            client.exec(
                write->statements[write->index].sql,
                [this, write]() {
                    const auto expected = write->statements[write->index].affected;
                    if (expected) {
                        read("SELECT JSON_OBJECT('affected', ROW_COUNT())", [this, write, expected](const DatabaseResult& result) {
                            if (result.error.empty() && result.rows.size() == 1 && result.rows.front().at("affected") == *expected) {
                                ++write->index;
                                writeNext(write);
                            } else {
                                writeFailed(write, "conditional mutation did not match the expected graph revision/state", 0);
                            }
                        });
                    } else {
                        ++write->index;
                        writeNext(write);
                    }
                },
                [this, write](const std::string& error, unsigned int code) {
                    writeFailed(write, error, code);
                });
        } else {
            client.commit(
                [this, write]() {
                    completed();
                    write->callback({});
                },
                [this, write](const std::string& error, unsigned int) {
                    mutationUncertain = true;
                    completed();
                    write->callback({{}, error, true});
                });
        }
    }

    void Database::writeFailed(const std::shared_ptr<Write>& write, const std::string& error, unsigned int code) {
        if (code == CR_SERVER_GONE_ERROR || code == CR_SERVER_LOST || code == CR_SERVER_LOST_EXTENDED) {
            completed();
            write->callback({{}, error, false});
        } else {
            client.rollback(
                [this, write, error]() {
                    completed();
                    write->callback({{}, error, false});
                },
                [this, write, error](const std::string&, unsigned int) {
                    mutationUncertain = true;
                    completed();
                    write->callback({{}, error, true});
                });
        }
    }

} // namespace coreci
