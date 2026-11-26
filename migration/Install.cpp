#include "Schema.h"
#include "legacy/MariaDBEventStore.h"
#include "persistence/StateSql.h"

namespace coreci::legacy::mariadb {
    void MariaDBEventStore::installState(const ::coreci::StoredState& state,
                                         const std::vector<std::string>& log,
                                         std::function<void()> done,
                                         std::function<void(const StoreError&)> error) {
        onFailure = std::move(error);
        auto ddl = std::make_shared<std::vector<std::string>>();
        const auto schema = ::coreci::mariadb::schema;
        std::size_t start = schema.find(';') + 1;
        while (start < schema.size()) {
            const auto end = schema.find(';', start);
            if (end == std::string_view::npos) {
                start = schema.size();
            } else {
                ddl->push_back(std::string(schema.substr(start, end - start)));
                start = end + 1;
            }
        }
        ddl->push_back("CREATE TABLE IF NOT EXISTS coreci_migration_schema (version INT NOT NULL PRIMARY KEY)");
        ddl->push_back("DELETE FROM coreci_migration_schema");
        ddl->push_back("INSERT INTO coreci_migration_schema (version) VALUES (8)");
        auto statements = std::make_shared<std::vector<std::string>>();
        for (const auto& table : ::coreci::persistence::stateTables()) {
            statements->push_back("DELETE FROM " + table.name);
        }
        statements->push_back("DELETE FROM coreci_log");
        ::coreci::StoredState empty;
        empty.sequence = state.sequence + 1;
        auto writes = ::coreci::persistence::stateSql(empty, state, "migration");
        writes.pop_back(); // Import actual display history below, preserving its sequence ids.
        statements->insert(statements->end(), writes.begin(), writes.end());
        statements->insert(statements->end(), log.begin(), log.end());
        insertGraph(ddl, 0, [this, statements, done] {
            client.startTransactions(
                [this, statements, done] {
                    insertGraph(statements, 0, [this, done] {
                        client.commit(
                            [this, done] {
                                client.endTransactions(
                                    [this, done] {
                                        execute(
                                            "RENAME TABLE coreci_schema TO legacy_coreci_schema, coreci_migration_schema TO coreci_schema, "
                                            "coreci_events TO legacy_coreci_events, coreci_graph_packages TO legacy_coreci_graph_packages, "
                                            "coreci_graph_targets TO legacy_coreci_graph_targets, coreci_graph_prerequisites TO "
                                            "legacy_coreci_graph_prerequisites, "
                                            "coreci_graph_releases TO legacy_coreci_graph_releases",
                                            [this, done] {
                                                onFailure = {};
                                                done();
                                            });
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
        });
    }
} // namespace coreci::legacy::mariadb
