#include "GraphStore.h"

#include "Database.h"
#include "DatabaseConfig.h"
#include "GraphFixtures.h"

#include <core/SNodeC.h>
#include <iostream>
#include <memory>
#include <utils/Config.h>

namespace {
    void require(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }
} // namespace

int main(int argc, char* argv[]) {
    auto* config = utils::Config::configRoot.newSubCommand<coreci::DatabaseConfig>();
    core::SNodeC::init(argc, argv);

    bool success = false;
    std::unique_ptr<database::mariadb::MariaDBClient> client;
    std::unique_ptr<coreci::Database> database;
    std::unique_ptr<coreci::GraphStore> store;
    std::unique_ptr<coreci::GraphStore> restored;
    try {
        client = std::make_unique<
            database::mariadb::MariaDBClient>(config->connection(), [&](const database::mariadb::MariaDBState& state) {
            if (state.connected) {
                store->restore([&](const coreci::DatabaseResult& loaded) {
                    require(loaded.error.empty() && store->graph().nodes().empty(), "fresh graph store did not restore empty state");
                    const auto change = store->graph().select("example", {{"example", graph_fixture::source("example", 99, {})}}, false);
                    store->commit(change, [&](const coreci::DatabaseResult& selected) {
                        require(selected.error.empty() && store->graph().nodes().size() == 2, "atomic selection was not persisted");
                        const auto id = store->graph().nodes().begin()->first;
                        const auto building = store->graph().beginBuild(id);
                        store->commit({{building}, store->graph().nextRebuildRevision()}, [&, id](const coreci::DatabaseResult& assigned) {
                            require(assigned.error.empty() && store->graph().node(id).attempts.back().state == "building",
                                    "build authority did not commit");
                            restored->restore([&, id](const coreci::DatabaseResult& restoredResult) {
                                require(restoredResult.error.empty() && restored->graph().nodes().size() == 2,
                                        "graph could not restore persisted work: " + restoredResult.error);
                                const auto& attempt = restored->graph().node(id).attempts.back();
                                require(attempt.state == "building" && attempt.buildRun.is_null() && !attempt.assignment.is_null(),
                                        "restoration treated an unknown dispatch outcome as permission to replace it");
                                auto changedInputs = store->graph().node(id);
                                changedInputs.source["tag"] = "v0.1.0";
                                store->commit(
                                    {{changedInputs}, store->graph().nextRebuildRevision()},
                                    [&, id](const coreci::DatabaseResult& invalid) {
                                        require(!invalid.error.empty(), "store accepted rewritten execution inputs");
                                        const auto& selected = store->graph().node(id);
                                        const nlohmann::json run{{"id", 123},
                                                                 {"run_attempt", 1},
                                                                 {"repository_id", 99},
                                                                 {"actor_id", 42},
                                                                 {"workflow_sha", std::string(40, 'c')}};
                                        const auto bound = store->graph().bindRun(id, selected.attempts.back().id, false, run);
                                        store->commit(
                                            {{bound}, store->graph().nextRebuildRevision(), {}},
                                            [&, id, run](const coreci::DatabaseResult& configured) {
                                                require(configured.error.empty(), "could not advance graph revision");
                                                auto different = run;
                                                different["id"] = 999;
                                                const auto staleNode = restored->graph().bindRun(
                                                    id, restored->graph().node(id).attempts.back().id, false, different);
                                                restored->commit(
                                                    {{staleNode}, restored->graph().nextRebuildRevision(), {}},
                                                    [&, id](const coreci::DatabaseResult& stale) {
                                                        require(!stale.error.empty() && !stale.uncertain,
                                                                "stale graph mutation was accepted or replayed");
                                                        auto conflict = graph_fixture::source("other", 100, {}).at("definition");
                                                        conflict["package"] = "example";
                                                        store->enroll(
                                                            "other",
                                                            "Owner/other",
                                                            "main",
                                                            {100, std::string(40, 'a'), std::string(40, 'b'), conflict},
                                                            store->getGeneration(),
                                                            [&](const coreci::DatabaseResult& namespaceConflict) {
                                                                require(!namespaceConflict.error.empty() && !namespaceConflict.uncertain,
                                                                        "another repository claimed a historical package namespace");
                                                                database->query(
                                                                    "SELECT JSON_OBJECT('nodes',(SELECT COUNT(*) FROM "
                                                                    "executions),'attempts',(SELECT COUNT(*) FROM "
                                                                    "attempts),'repositories',(SELECT COUNT(*) FROM "
                                                                    "repositories),'run',(SELECT "
                                                                    "JSON_VALUE(build_run,'$.id') FROM attempts WHERE state='building'))",
                                                                    [&](const coreci::DatabaseResult& counted) {
                                                                        require(counted.error.empty() && counted.rows.size() == 1 &&
                                                                                    counted.rows.front().at("nodes") == 2 &&
                                                                                    counted.rows.front().at("attempts") == 2 &&
                                                                                    counted.rows.front().at("repositories") == 1 &&
                                                                                    counted.rows.front().at("run") == "123",
                                                                                "stale mutation changed persisted graph");
                                                                        success = true;
                                                                        std::cout << "PASS real MariaDB graph persistence, committed "
                                                                                     "assignments, "
                                                                                     "immutable inputs and stale-generation rollback\n";
                                                                        restored.reset();
                                                                        store.reset();
                                                                        database.reset();
                                                                        client.reset();
                                                                    });
                                                            });
                                                    });
                                            });
                                    });
                            });
                        });
                        require(store->graph().node(id).attempts.back().state == "waiting",
                                "in-memory graph advanced before COMMIT acknowledgement");
                    });
                    require(store->graph().nodes().empty(), "uncommitted selection entered the graph");
                });
            }
        });
        database = std::make_unique<coreci::Database>(*client);
        store = std::make_unique<coreci::GraphStore>(*database);
        restored = std::make_unique<coreci::GraphStore>(*database);
        client->connect();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    const int result = core::SNodeC::start();
    return success ? result : 1;
}
