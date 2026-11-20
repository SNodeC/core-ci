#include "MariaDBStateStore.h"

#include "Schema.h"

#include <utility>

namespace coreci::mariadb {
    MariaDBStateStore::MariaDBStateStore(const database::mariadb::MariaDBConnectionDetails& details)
        : client(details, [this](const database::mariadb::MariaDBState& state) {
            if (!state.connected) {
                fail({StoreError::Kind::Unavailable, state.errorMessage.empty() ? "MariaDB connection lost" : state.errorMessage});
            }
        }) {
    }
    MariaDBStateStore::~MariaDBStateStore() {
        client.abort();
    }
    void MariaDBStateStore::fail(const StoreError& error) {
        const bool wasReady = ready;
        ready = false;
        // Cancel remaining callbacks before a recovery callback may issue new commands.
        client.abort();
        auto failed = std::exchange(onFailure, {});
        if (failed) {
            failed(error);
        } else if (wasReady && onLost) {
            onLost(error.message);
        }
    }
    void MariaDBStateStore::execute(const std::string& sql, std::function<void()> done) {
        client.exec(sql, std::move(done), [this](const std::string& message, unsigned int) {
            fail({StoreError::Kind::Unavailable, message});
        });
    }
    void MariaDBStateStore::open(std::function<void()> onReady, std::function<void(const StoreError&)> onError) {
        ready = false;
        onFailure = std::move(onError);
        client.connect();
        execute("SET NAMES utf8mb4", [this, onReady] {
            execute("SET autocommit = 1", [this, onReady] {
                lockWriter([this, onReady] {
                    checkSchema([this, onReady] {
                        ready = true;
                        onFailure = {};
                        onReady();
                    });
                });
            });
        });
    }
    void MariaDBStateStore::setConnectionLost(std::function<void(const std::string&)> callback) {
        onLost = std::move(callback);
    }
    void MariaDBStateStore::lockWriter(std::function<void()> done) {
        client.query(
            "SELECT GET_LOCK(CONCAT('coreci:', DATABASE()), 0)",
            [this, done](const MYSQL_ROW row) {
                if (row != nullptr) {
                    if (row[0] == nullptr || std::string(row[0]) != "1") {
                        fail({StoreError::Kind::Unavailable, "MariaDB writer lock unavailable"});
                    }
                } else {
                    done();
                }
            },
            [this](const std::string& error, unsigned int) {
                fail({StoreError::Kind::Unavailable, error});
            });
    }
    void MariaDBStateStore::createSchema(std::size_t index, std::function<void()> done) {
        // Schema.sql is embedded by CMake; its CREATE statements contain no string literals.
        const auto end = schema.find(';', index);
        if (end == std::string_view::npos) {
            done();
        } else {
            execute(std::string(schema.substr(index, end - index)), [this, end, done] {
                createSchema(end + 1, done);
            });
        }
    }
    void MariaDBStateStore::checkSchema(std::function<void()> done) {
        const auto exists = std::make_shared<bool>(false);
        client.query(
            "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema=DATABASE() AND table_name='coreci_schema'",
            [this, exists, done](const MYSQL_ROW row) {
                if (row) {
                    *exists = row[0] && std::string(row[0]) == "1";
                } else if (*exists) {
                    const auto valid = std::make_shared<bool>(false);
                    client.query(
                        "SELECT COUNT(*),MIN(version) FROM coreci_schema",
                        [this, valid, done](const MYSQL_ROW version) {
                            if (version) {
                                *valid = version[0] && version[1] && std::string(version[0]) == "1" && std::string(version[1]) == "7";
                            } else if (*valid) {
                                done();
                            } else {
                                fail({StoreError::Kind::Corrupt, "state schema 7 required; run coreci-migrate on an event-store database"});
                            }
                        },
                        [this](const auto& message, auto) {
                            fail({StoreError::Kind::Unavailable, message});
                        });
                } else {
                    createSchema(0, [this, done] {
                        execute("INSERT INTO coreci_schema (version) VALUES (7)", [this, done] {
                            execute("INSERT INTO coreci_state (singleton,sequence,clock,quiet_until,latest) VALUES (1,0,NULL,NULL," +
                                        persistence::sqlText("") + ")",
                                    done);
                        });
                    });
                }
            },
            [this](const auto& message, auto) {
                fail({StoreError::Kind::Unavailable, message});
            });
    }
    void MariaDBStateStore::read(std::shared_ptr<persistence::StateRows> rows, std::size_t index, std::function<void()> done) {
        const auto& tables = persistence::stateTables();
        if (index == tables.size()) {
            done();
        } else {
            const auto table = tables[index];
            (*rows)[table.name] = {};
            client.query(
                persistence::selectState(table),
                [this, rows, index, table, done](const MYSQL_ROW fields) {
                    if (!fields) {
                        read(rows, index + 1, done);
                    } else {
                        std::vector<persistence::Field> values;
                        for (std::size_t field = 0; field < table.columns.size(); ++field) {
                            values.push_back(fields[field] ? persistence::Field{fields[field]} : std::nullopt);
                        }
                        try {
                            rows->at(table.name).push_back(persistence::decodeFields(table, values));
                        } catch (const std::exception& error) {
                            fail({StoreError::Kind::Corrupt, error.what()});
                        }
                    }
                },
                [this](const auto& message, auto) {
                    fail({StoreError::Kind::Unavailable, message});
                });
        }
    }
    void MariaDBStateStore::load(std::function<void(StoredState)> done, std::function<void(const StoreError&)> error) {
        if (!ready) {
            error({StoreError::Kind::Unavailable, "open required before state load"});
        } else {
            onFailure = std::move(error);
            auto rows = std::make_shared<persistence::StateRows>();
            read(rows, 0, [this, rows, done] {
                try {
                    auto state = persistence::decodeState(*rows);
                    onFailure = {};
                    done(std::move(state));
                } catch (const std::exception& error) {
                    fail({StoreError::Kind::Corrupt, error.what()});
                }
            });
        }
    }
    void MariaDBStateStore::write(std::shared_ptr<std::vector<std::string>> statements, std::size_t index, std::function<void()> done) {
        if (index == statements->size()) {
            done();
        } else {
            execute(statements->at(index), [this, statements, index, done] {
                write(statements, index + 1, done);
            });
        }
    }
    void MariaDBStateStore::commit(const StoredState& before,
                                   const StoredState& after,
                                   const std::string& description,
                                   std::function<void()> done,
                                   std::function<void(const StoreError&)> error) {
        if (!ready) {
            error({StoreError::Kind::Unavailable, "open required before state transaction"});
        } else {
            onFailure = std::move(error);
            try {
                auto statements = std::make_shared<std::vector<std::string>>(persistence::stateSql(before, after, description));
                client.startTransactions(
                    [this, statements, done] {
                        write(statements, 0, [this, done] {
                            client.commit(
                                [this, done] {
                                    client.endTransactions(
                                        [this, done] {
                                            onFailure = {};
                                            done();
                                        },
                                        [this](const auto& message, auto) {
                                            fail({StoreError::Kind::Unavailable, message});
                                        });
                                },
                                [this](const auto& message, auto) {
                                    fail({StoreError::Kind::Unavailable, message});
                                });
                        });
                    },
                    [this](const auto& message, auto) {
                        fail({StoreError::Kind::Unavailable, message});
                    });
            } catch (const std::exception& failure) {
                fail({StoreError::Kind::Corrupt, failure.what()});
            }
        }
    }
} // namespace coreci::mariadb
