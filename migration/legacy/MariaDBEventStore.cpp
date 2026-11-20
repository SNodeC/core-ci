#include "MariaDBEventStore.h"

#include "migration/legacy/EventSql.h"

#include <charconv>
#include <memory>
#include <tuple>
#include <utility>

namespace coreci::legacy::mariadb {
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
        bool valid = fields.size() == 20;
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
                                        &persistence::EventRow::waveId,
                                        &persistence::EventRow::httpStatus,
                                        &persistence::EventRow::message,
                                        &persistence::EventRow::runId,
                                        &persistence::EventRow::runAttempt,
                                        &persistence::EventRow::attempt,
                                        &persistence::EventRow::artifact,
                                        &persistence::EventRow::feedCommit,
                                        &persistence::EventRow::reason,
                                        &persistence::EventRow::conclusion,
                                        &persistence::EventRow::eventAt,
                                        &persistence::EventRow::dispatchDeadline};
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
        ready = false;
        // Cancel remaining callbacks before a recovery callback may issue new commands.
        client.abort();
        auto failed = std::exchange(onFailure, {});
        if (failed) {
            failed(error);
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
                    checkSchema([this, onReady] {
                        ready = true;
                        onFailure = {};
                        onReady();
                    });
                });
            });
        });
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
    void MariaDBEventStore::checkSchema(std::function<void()> done) {
        const auto version = std::make_shared<std::string>();
        client.query(
            "SELECT (SELECT COUNT(*) FROM coreci_schema),(SELECT MIN(version) FROM coreci_schema),"
            "(EXISTS(SELECT 1 FROM coreci_events LIMIT 1) OR "
            "EXISTS(SELECT 1 FROM coreci_graph_packages LIMIT 1) OR EXISTS(SELECT 1 FROM coreci_graph_targets LIMIT 1) OR "
            "EXISTS(SELECT 1 FROM coreci_graph_prerequisites LIMIT 1) OR EXISTS(SELECT 1 FROM coreci_graph_releases LIMIT 1))",
            [this, done, version](const MYSQL_ROW row) {
                if (row != nullptr) {
                    if (row[0] != nullptr && std::string(row[0]) == "1" && row[1] != nullptr &&
                        (std::string(row[1]) == "5" || std::string(row[1]) == "6")) {
                        *version = row[1];
                    } else {
                        fail({StoreError::Kind::Corrupt, "coreci_schema: expected one version 5/6 row, or empty schema and events"});
                    }
                } else if (*version == "5") {
                    execute("ALTER TABLE coreci_events ADD COLUMN IF NOT EXISTS dispatch_deadline BIGINT NULL", [this, done] {
                        execute("UPDATE coreci_schema SET version=6 WHERE version=5", done);
                    });
                } else {
                    done();
                }
            },
            [this](const std::string& message, unsigned int) {
                fail({StoreError::Kind::Unavailable, message});
            });
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
                        const std::size_t count = table == 0 ? 4 : (table == 1 || table == 2 ? 2 : 4);
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
                                unsigned revisionBase = 0;
                                if (fields[5] == nullptr || !number(fields[5], revisionBase) || revisionBase > 1000000) {
                                    error = "invalid revision_base";
                                } else {
                                    graph.packages.push_back({values[0], {values[1], values[2], values[3]}, {}, {}, revisionBase});
                                }
                            } else if (table == 1) {
                                graph.targets.push_back({values[0], values[1]});
                            } else if (table == 2) {
                                graph.prerequisites.push_back({values[0], values[1]});
                            } else if (values[3] != "webhook" && values[3] != "reconcile") {
                                error = "invalid graph release origin";
                            } else {
                                graph.releases.push_back({*rows->at(seq).eventAt,
                                                          values[0],
                                                          values[1],
                                                          values[2],
                                                          values[3] == "webhook" ? Origin::Webhook : Origin::Reconcile});
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
                        for (std::size_t i = 0; i < 20; ++i) {
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
} // namespace coreci::legacy::mariadb
