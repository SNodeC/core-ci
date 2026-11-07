#include "MariaDBEventStore.h"

#include "Schema.h"
#include "persistence/EventSql.h"

#include <charconv>
#include <memory>
#include <tuple>
#include <utility>

namespace coreci::mariadb {
    namespace {
        int nibble(char c) {
            int result = -1;
            if (c >= '0' && c <= '9') {
                result = c - '0';
            } else if (c >= 'A' && c <= 'F') {
                result = c - 'A' + 10;
            } else if (c >= 'a' && c <= 'f') {
                result = c - 'a' + 10;
            }
            return result;
        }
        bool unhex(const std::string& value, std::string& result) {
            bool valid = value.size() % 2 == 0;
            for (std::size_t i = 0; valid && i < value.size(); i += 2) {
                const int high = nibble(value[i]), low = nibble(value[i + 1]);
                valid = high >= 0 && low >= 0;
                if (valid) {
                    result += static_cast<char>(high * 16 + low);
                }
            }
            return valid;
        }
        template <typename T>
        bool number(const std::string& value, T& result) {
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
            return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
        }
    } // namespace
    std::variant<persistence::EventRow, persistence::Invalid> decodeFields(const std::vector<std::optional<std::string>>& fields) {
        persistence::EventRow row;
        bool valid = fields.size() == 18;
        const std::string seq = !fields.empty() && fields[0] ? *fields[0] : "NULL";
        if (valid) {
            valid = fields[0] && number(*fields[0], row.seq);
        }
        const auto members = std::tuple{&persistence::EventRow::package,
                                        &persistence::EventRow::target,
                                        &persistence::EventRow::tag,
                                        &persistence::EventRow::sourceSha,
                                        &persistence::EventRow::origin,
                                        &persistence::EventRow::buildId,
                                        &persistence::EventRow::kind,
                                        &persistence::EventRow::httpStatus,
                                        &persistence::EventRow::message,
                                        &persistence::EventRow::runId,
                                        &persistence::EventRow::runAttempt,
                                        &persistence::EventRow::attempt,
                                        &persistence::EventRow::artifact,
                                        &persistence::EventRow::feedCommit,
                                        &persistence::EventRow::reason,
                                        &persistence::EventRow::eventAt};
        if (valid) {
            valid = fields[1] && unhex(*fields[1], row.eventType);
            std::size_t index = 2;
            const auto field = [&](auto member) {
                if (valid && fields[index]) {
                    using T = typename std::decay_t<decltype(row.*member)>::value_type;
                    T value{};
                    if constexpr (std::is_same_v<T, std::string>) {
                        valid = unhex(*fields[index], value);
                    } else {
                        valid = number(*fields[index], value);
                    }
                    row.*member = std::move(value);
                }
                ++index;
            };
            std::apply(
                [&](auto... member) {
                    (field(member), ...);
                },
                members);
        }
        std::variant<persistence::EventRow, persistence::Invalid> result = row;
        if (!valid) {
            result = persistence::Invalid{"seq " + seq + ": invalid hex, decimal number or field count"};
        }
        return result;
    }
    MariaDBEventStore::MariaDBEventStore(const database::mariadb::MariaDBConnectionDetails& details)
        : client(details, [this](const database::mariadb::MariaDBState& state) {
            if (!state.connected) {
                fail({StoreError::Kind::Unavailable, state.errorMessage.empty() ? "MariaDB connection lost" : state.errorMessage});
            }
        }) {
    }
    MariaDBEventStore::~MariaDBEventStore() {
        client.abort();
    }
    void MariaDBEventStore::fail(const StoreError& error) {
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
    void MariaDBEventStore::execute(const std::string& sql, std::function<void()> done) {
        client.exec(sql, std::move(done), [this](const std::string& message, unsigned int) {
            fail({StoreError::Kind::Unavailable, message});
        });
    }
    void MariaDBEventStore::open(std::function<void()> onReady, std::function<void(const StoreError&)> onError) {
        ready = false;
        onFailure = std::move(onError);
        client.connect();
        execute("SET NAMES utf8mb4", [this, onReady] {
            execute("SET autocommit = 1", [this, onReady] {
                lockWriter([this, onReady] {
                    createSchema(0, [this, onReady] {
                        checkSchema([this, onReady] {
                            ready = true;
                            onFailure = {};
                            onReady();
                        });
                    });
                });
            });
        });
    }
    void MariaDBEventStore::setConnectionLost(std::function<void(const std::string&)> callback) {
        onLost = std::move(callback);
    }
    void MariaDBEventStore::lockWriter(std::function<void()> done) {
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
    void MariaDBEventStore::createSchema(std::size_t index, std::function<void()> done) {
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
    void MariaDBEventStore::checkSchema(std::function<void()> done) {
        const auto bootstrap = std::make_shared<bool>(false);
        client.query(
            "SELECT (SELECT COUNT(*) FROM coreci_schema),(SELECT MIN(version) FROM coreci_schema),"
            "(EXISTS(SELECT 1 FROM coreci_events LIMIT 1) OR EXISTS(SELECT 1 FROM coreci_graph_feeds LIMIT 1) OR "
            "EXISTS(SELECT 1 FROM coreci_graph_packages LIMIT 1) OR EXISTS(SELECT 1 FROM coreci_graph_targets LIMIT 1) OR "
            "EXISTS(SELECT 1 FROM coreci_graph_prerequisites LIMIT 1))",
            [this, done, bootstrap](const MYSQL_ROW row) {
                if (row != nullptr) {
                    if (row[0] != nullptr && std::string(row[0]) == "1" && row[1] != nullptr && std::string(row[1]) == "3") {
                        // Wait for query's final nullptr before starting the next operation.
                    } else if (row[0] != nullptr && std::string(row[0]) == "0" && row[2] != nullptr && std::string(row[2]) == "0") {
                        *bootstrap = true;
                    } else {
                        fail({StoreError::Kind::Corrupt, "coreci_schema: expected one version 3 row, or empty schema and events"});
                    }
                } else if (*bootstrap) {
                    execute("INSERT INTO coreci_schema (version) VALUES (3)", done);
                } else {
                    done();
                }
            },
            [this](const std::string& message, unsigned int) {
                fail({StoreError::Kind::Unavailable, message});
            });
    }
    void MariaDBEventStore::append(const Event& event,
                                   std::function<void(std::uint64_t)> onCommitted,
                                   std::function<void(const std::string&)> onError) {
        const auto sql = persistence::insertSql(event);
        if (!ready) {
            onError("MariaDB session requires open before append");
        } else if (std::holds_alternative<persistence::Invalid>(sql)) {
            onError(std::get<persistence::Invalid>(sql).reason);
        } else {
            onFailure = [onError](const StoreError& error) {
                onError(error.message);
            };
            const auto seq = std::make_shared<std::uint64_t>(0);
            const auto committed = [this, onCommitted, seq] {
                onFailure = {};
                onCommitted(*seq);
            };
            const auto sequence = [this, seq](std::function<void()> done) {
                client.query(
                    "SELECT LAST_INSERT_ID()",
                    [this, seq, done](const MYSQL_ROW row) {
                        if (row != nullptr) {
                            if (row[0] == nullptr || !number(std::string(row[0]), *seq) || *seq == 0) {
                                fail({StoreError::Kind::Unavailable, "invalid committed event seq"});
                            }
                        } else if (ready) {
                            done();
                        }
                    },
                    [this](const std::string& error, unsigned) {
                        fail({StoreError::Kind::Unavailable, error});
                    });
            };
            if (std::holds_alternative<GraphChanged>(event)) {
                auto statements = std::make_shared<std::vector<std::string>>();
                const auto row = std::get<persistence::EventRow>(persistence::encode(event));
                const auto graphSql = persistence::graphInsertSql(*row.graph);
                statements->insert(statements->end(), graphSql.begin(), graphSql.end());
                client.startTransactions(
                    [this, statements, committed, sequence, eventSql = std::get<std::string>(sql)] {
                        execute(eventSql, [this, statements, committed, sequence] {
                            sequence([this, statements, committed] {
                                insertGraph(statements, 0, [this, committed] {
                                    client.commit(
                                        [this, committed] {
                                            client.endTransactions(committed, [this](const auto& error, auto) {
                                                fail({StoreError::Kind::Unavailable, error});
                                            });
                                        },
                                        [this](const auto& error, auto) {
                                            fail({StoreError::Kind::Unavailable, error});
                                        });
                                });
                            });
                        });
                    },
                    [this](const auto& error, auto) {
                        fail({StoreError::Kind::Unavailable, error});
                    });
            } else {
                execute(std::get<std::string>(sql), [sequence, committed] {
                    sequence(committed);
                });
            }
        }
    }
    void
    MariaDBEventStore::insertGraph(std::shared_ptr<std::vector<std::string>> statements, std::size_t index, std::function<void()> done) {
        if (index == statements->size()) {
            done();
        } else {
            execute(statements->at(index), [this, statements, index, done] {
                insertGraph(statements, index + 1, done);
            });
        }
    }
    void MariaDBEventStore::loadGraphs(std::shared_ptr<Rows> rows, std::size_t table, std::function<void()> done) {
        const auto selects = persistence::graphSelectSql();
        if (table == selects.size()) {
            done();
        } else {
            client.query(
                selects[table],
                [this, rows, table, done](const MYSQL_ROW fields) {
                    if (fields == nullptr) {
                        loadGraphs(rows, table + 1, done);
                    } else {
                        std::uint64_t seq = 0;
                        std::string error;
                        if (fields[0] == nullptr || !number(std::string(fields[0]), seq)) {
                            error = "invalid graph event_seq";
                        } else if (!rows->contains(seq) || rows->at(seq).eventType != "GraphChanged") {
                            error = "orphan graph row";
                        }
                        std::vector<std::string> values;
                        const std::size_t count = table == 0 ? 5 : (table == 1 ? 8 : (table == 2 ? 3 : 2));
                        for (std::size_t i = 1; i <= count && error.empty(); ++i) {
                            std::string value;
                            if (fields[i] == nullptr || !unhex(fields[i], value)) {
                                error = "invalid graph hex or NULL";
                            }
                            values.push_back(value);
                        }
                        if (error.empty()) {
                            auto& graph = *rows->at(seq).graph;
                            if (table == 0) {
                                graph.feeds.push_back({values[0], {values[1], values[2], values[3], values[4]}});
                            } else if (table == 1) {
                                unsigned revisionBase = 0;
                                if (fields[10] == nullptr || !number(fields[10], revisionBase) || revisionBase > 1000000) {
                                    error = "invalid revision_base";
                                } else if (fields[9] == nullptr || (std::string(fields[9]) != "0" && std::string(fields[9]) != "1")) {
                                    error = "invalid accept_prereleases";
                                } else {
                                    graph.packages.push_back({values[0],
                                                              {values[1], values[2], values[3]},
                                                              {values[4], values[5], values[6], values[7]},
                                                              {},
                                                              {},
                                                              std::string(fields[9]) == "1",
                                                              revisionBase});
                                }
                            } else if (table == 2) {
                                graph.targets.push_back({values[0], values[1], values[2]});
                            } else {
                                graph.prerequisites.push_back({values[0], values[1]});
                            }
                        }
                        if (!error.empty()) {
                            fail({StoreError::Kind::Corrupt, "seq " + std::to_string(seq) + ": " + error});
                        }
                    }
                },
                [this](const auto& error, auto) {
                    fail({StoreError::Kind::Unavailable, error});
                });
        }
    }
    void MariaDBEventStore::loadAll(std::function<void(std::uint64_t, const Event&)> onEvent,
                                    std::function<void()> onDone,
                                    std::function<void(const StoreError&)> onError) {
        if (!ready) {
            onError({StoreError::Kind::Unavailable, "MariaDB session requires open before loadAll"});
        } else {
            onFailure = std::move(onError);
            auto rows = std::make_shared<Rows>();
            client.query(
                persistence::selectSql(),
                [this, rows, onEvent, onDone](const MYSQL_ROW fields) {
                    if (fields == nullptr) {
                        loadGraphs(rows, 0, [this, rows, onEvent, onDone] {
                            bool valid = true;
                            for (auto i = rows->begin(); valid && i != rows->end(); ++i) {
                                const auto event = persistence::decode(i->second);
                                valid = std::holds_alternative<Event>(event);
                                if (valid) {
                                    onEvent(i->first, std::get<Event>(event));
                                } else {
                                    fail({StoreError::Kind::Corrupt, std::get<persistence::Invalid>(event).reason});
                                }
                            }
                            if (valid) {
                                onFailure = {};
                                onDone();
                            }
                        });
                    } else {
                        std::vector<std::optional<std::string>> values;
                        for (std::size_t i = 0; i < 18; ++i) {
                            values.push_back(fields[i] == nullptr ? std::nullopt : std::optional<std::string>(fields[i]));
                        }
                        const auto decoded = decodeFields(values);
                        if (std::holds_alternative<persistence::Invalid>(decoded)) {
                            fail({StoreError::Kind::Corrupt, std::get<persistence::Invalid>(decoded).reason});
                        } else {
                            auto row = std::get<persistence::EventRow>(decoded);
                            if (row.eventType == "GraphChanged") {
                                row.graph = persistence::GraphRows{};
                            }
                            rows->emplace(row.seq, std::move(row));
                        }
                    }
                },
                [this](const auto& message, auto) {
                    fail({StoreError::Kind::Unavailable, message});
                });
        }
    }
} // namespace coreci::mariadb
