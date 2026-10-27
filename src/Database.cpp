#include "Database.h"

#include <Log.h>
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
        operations.push_back(std::make_shared<std::function<void()>>(operation));
        if (operations.size() == 1) {
            drain();
        }
    }

    void Database::completed() {
        operations.pop_front();
        drain();
    }

    void Database::drain() {
        if (!draining) {
            draining = true;
            bool pending = false;
            while (!operations.empty() && !pending) {
                const auto operation = operations.front();
                (*operation)();
                pending = !operations.empty() && operations.front() == operation;
            }
            draining = false;
        }
    }

    void Database::finish(const std::shared_ptr<Write>& write, const DatabaseResult& result) {
        try {
            write->callback(result);
        } catch (const std::exception& error) {
            mutationUncertain = true;
            snode::log::application().error() << "Database completion failed; restart and reconcile: " << error.what();
        }
        completed();
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

    void Database::write(const std::function<std::vector<DatabaseStatement>()>& prepare, const Callback& callback) {
        const auto write = std::make_shared<Write>();
        write->callback = callback;
        enqueue([this, write, prepare]() {
            DatabaseResult rejected;
            if (mutationUncertain) {
                rejected = {{}, "database mutation outcome needs reconciliation before further writes", true};
            } else {
                try {
                    write->statements = prepare();
                } catch (const std::exception& error) {
                    rejected = {{}, error.what(), false, true};
                }
            }
            if (!rejected.error.empty() || write->statements.empty()) {
                finish(write, rejected);
            } else {
                client.exec(
                    "START TRANSACTION",
                    [this, write]() {
                        writeNext(write);
                    },
                    [this, write](const std::string& error, unsigned int) {
                        finish(write, {{}, error, false});
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
                    finish(write, {});
                },
                [this, write](const std::string& error, unsigned int) {
                    mutationUncertain = true;
                    finish(write, {{}, error, true});
                });
        }
    }

    void Database::writeFailed(const std::shared_ptr<Write>& write, const std::string& error, unsigned int code) {
        if (code == CR_SERVER_GONE_ERROR || code == CR_SERVER_LOST || code == CR_SERVER_LOST_EXTENDED) {
            finish(write, {{}, error, false});
        } else {
            client.rollback(
                [this, write, error]() {
                    finish(write, {{}, error, false});
                },
                [this, write, error](const std::string&, unsigned int) {
                    mutationUncertain = true;
                    finish(write, {{}, error, true});
                });
        }
    }

} // namespace coreci
