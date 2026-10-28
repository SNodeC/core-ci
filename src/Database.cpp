#include "Database.h"

#include <Log.h>
#include <errmsg.h>
#include <exception>
#include <utility>

namespace coreci {
    std::string Database::text(const std::string& value) {
        std::string result = "CONVERT(X'";
        for (const unsigned char byte : value) {
            result += "0123456789abcdef"[byte >> 4];
            result += "0123456789abcdef"[byte & 15];
        }
        return result + "' USING utf8mb4)";
    }

    void Database::enqueue(const std::function<void()>& operation) {
        operations.push_back(std::make_shared<std::function<void()>>(operation));
        if (operations.size() == 1) {
            pump();
        }
    }

    void Database::release() {
        operations.pop_front();
        pump();
    }

    void Database::pump() {
        if (!pumping) {
            pumping = true;
            bool pending = false;
            while (!operations.empty() && !pending) {
                const auto operation = operations.front();
                (*operation)();
                pending = !operations.empty() && operation == operations.front();
            }
            pumping = false;
        }
    }

    void Database::query(const std::string& sql, const Callback& callback) {
        enqueue([this, sql, callback] {
            const auto rows = std::make_shared<DatabaseResult>();
            const auto completion = std::make_shared<Callback>(callback);
            const auto finish = [this, rows, completion] {
                if (const auto done = std::exchange(*completion, nullptr)) {
                    release();
                    done(*rows);
                }
            };
            client.query(
                sql,
                [rows, completion, finish](const MYSQL_ROW row) {
                    if (*completion) {
                        if (row) {
                            try {
                                rows->rows.push_back(row[0] ? nlohmann::json::parse(row[0]) : nlohmann::json());
                            } catch (const std::exception& error) {
                                rows->error = error.what();
                            }
                        } else {
                            finish();
                        }
                    }
                },
                [rows, finish](const std::string& error, unsigned int code) {
                    *rows = {{}, error, false, code};
                    finish();
                });
        });
    }

    void Database::write(const std::function<std::string()>& prepare, const Callback& callback) {
        enqueue([this, prepare, callback] {
            const auto completion = std::make_shared<Callback>(callback);
            const auto finish = [this, completion](const DatabaseResult& result) {
                if (const auto done = std::exchange(*completion, nullptr)) {
                    try {
                        done(result);
                    } catch (const std::exception& error) {
                        uncertain = true;
                        snode::log::application().error() << "Graph write completion failed: " << error.what();
                    }
                    release();
                }
            };
            std::string sql;
            DatabaseResult result;
            try {
                if (uncertain) {
                    result = {{}, "database write outcome requires reconciliation", true};
                } else {
                    sql = prepare();
                }
            } catch (const std::exception& error) {
                result.error = error.what();
            }
            if (!result.error.empty() || sql.empty()) {
                finish(result);
            } else {
                // A graph change is one conditional, autocommitted statement.
                client.exec(
                    sql,
                    [this, finish] {
                        client.affectedRows(
                            [finish](my_ulonglong affected) {
                                finish(affected == 1 ? DatabaseResult{} : DatabaseResult{{}, "stored graph revision changed", false});
                            },
                            [this, finish](const std::string& error, unsigned int) {
                                uncertain = true;
                                finish({{}, error, true});
                            });
                    },
                    [this, finish](const std::string& error, unsigned int code) {
                        uncertain = code >= CR_MIN_ERROR;
                        finish({{}, error, uncertain, code});
                    });
            }
        });
    }
} // namespace coreci
