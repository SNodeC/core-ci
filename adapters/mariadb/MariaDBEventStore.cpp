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
                createSchema(0, [this, onReady] {
                    checkSchema([this, onReady] {
                        ready = true;
                        onFailure = {};
                        onReady();
                    });
                });
            });
        });
    }
    void MariaDBEventStore::createSchema(std::size_t index, std::function<void()> done) {
        // Schema.sql is embedded by CMake; its two CREATE statements contain no string literals.
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
            "EXISTS(SELECT 1 FROM coreci_events LIMIT 1)",
            [this, done, bootstrap](const MYSQL_ROW row) {
                if (row != nullptr) {
                    if (row[0] != nullptr && std::string(row[0]) == "1" && row[1] != nullptr && std::string(row[1]) == "1") {
                        // Wait for query's final nullptr before starting the next operation.
                    } else if (row[0] != nullptr && std::string(row[0]) == "0" && row[2] != nullptr && std::string(row[2]) == "0") {
                        *bootstrap = true;
                    } else {
                        fail({StoreError::Kind::Corrupt, "coreci_schema: expected one version 1 row, or empty schema and events"});
                    }
                } else if (*bootstrap) {
                    execute("INSERT INTO coreci_schema (version) VALUES (1)", done);
                } else {
                    done();
                }
            },
            [this](const std::string& message, unsigned int) {
                fail({StoreError::Kind::Unavailable, message});
            });
    }
    void MariaDBEventStore::append(const Event& event, std::function<void()> onCommitted, std::function<void(const std::string&)> onError) {
        const auto sql = persistence::insertSql(event);
        if (!ready) {
            onError("MariaDB session requires open before append");
        } else if (std::holds_alternative<persistence::Invalid>(sql)) {
            onError(std::get<persistence::Invalid>(sql).reason);
        } else {
            onFailure = [onError](const StoreError& error) {
                onError(error.message);
            };
            execute(std::get<std::string>(sql), [this, onCommitted] {
                onFailure = {};
                onCommitted();
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
            client.query(
                persistence::selectSql(),
                [this, onEvent, onDone](const MYSQL_ROW fields) {
                    if (fields == nullptr) {
                        onFailure = {};
                        onDone();
                    } else {
                        std::vector<std::optional<std::string>> values;
                        for (std::size_t i = 0; i < 18; ++i) {
                            values.push_back(fields[i] == nullptr ? std::nullopt : std::optional<std::string>(fields[i]));
                        }
                        const auto decoded = decodeFields(values);
                        if (std::holds_alternative<persistence::Invalid>(decoded)) {
                            fail({StoreError::Kind::Corrupt, std::get<persistence::Invalid>(decoded).reason});
                        } else {
                            const auto& row = std::get<persistence::EventRow>(decoded);
                            const auto event = persistence::decode(row);
                            if (std::holds_alternative<persistence::Invalid>(event)) {
                                fail({StoreError::Kind::Corrupt, std::get<persistence::Invalid>(event).reason});
                            } else {
                                onEvent(row.seq, std::get<Event>(event));
                            }
                        }
                    }
                },
                [this](const std::string& message, unsigned int) {
                    fail({StoreError::Kind::Unavailable, message});
                });
        }
    }
} // namespace coreci::mariadb
