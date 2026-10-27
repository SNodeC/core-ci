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
    void overlap(coreci::GraphStore& store, coreci::Database& database, const std::function<void()>& done) {
        struct Counts {
            unsigned int callbacks = 0;
            unsigned int notifications = 0;
            unsigned int noops = 0;
            unsigned int rejected = 0;
        };
        const auto counts = std::make_shared<Counts>();
        auto first = store.graph().nodes().begin();
        const auto id = first->first;
        const auto second = (++first)->first;
        const auto attempt = store.graph().node(id).attempts.back();
        const auto run = attempt.buildRun;
        const nlohmann::json result{
            {"run", run}, {"assignment_sha256", coreci::Graph::digest(attempt.assignment)}, {"artifacts", {{{"id", 1}}}}};
        store.onChanged = [counts](bool committed, bool repositories) {
            require(committed && !repositories && counts->notifications < counts->callbacks, "notification preceded continuation");
            ++counts->notifications;
        };
        store.commit(
            [=](const coreci::Graph& graph) {
                return coreci::GraphChange{
                    {graph.bindRun(id, attempt.id, false, run, {{"run_id", 123}, {"job_id", 321}, {"job_status", "completed"}})},
                    graph.nextRebuildRevision()};
            },
            [counts](const coreci::DatabaseResult& outcome, const coreci::GraphChange* change) {
                require(outcome.error.empty() && change != nullptr, "overlapping progress failed");
                ++counts->callbacks;
            });
        store.commit(
            [second](const coreci::Graph& graph) {
                return coreci::GraphChange{{graph.beginBuild(second)}, graph.nextRebuildRevision()};
            },
            [counts](const coreci::DatabaseResult& outcome, const coreci::GraphChange* change) {
                require(outcome.error.empty() && change != nullptr, "independent advance lost to progress commit");
                ++counts->callbacks;
            });
        store.commit(
            [=](const coreci::Graph& graph) {
                return coreci::GraphChange{{graph.built(id, attempt.id, result)}, graph.nextRebuildRevision()};
            },
            [&, counts, id](const coreci::DatabaseResult& outcome, const coreci::GraphChange* change) {
                require(outcome.error.empty() && change != nullptr, "overlapping artifact acceptance failed");
                require(store.graph().node(id).attempts.back().evidence.at("build").at("job_id") == 321,
                        "artifact acceptance lost newer evidence");
                ++counts->callbacks;
            });
        for (unsigned int index = 0; index < 4096; ++index) {
            store.commit(
                [](const coreci::Graph& graph) {
                    return coreci::GraphChange{{}, graph.nextRebuildRevision()};
                },
                [counts](const coreci::DatabaseResult& outcome, const coreci::GraphChange* change) {
                    require(outcome.error.empty() && change == nullptr, "no-op unexpectedly mutated state");
                    ++counts->noops;
                });
            store.commit(
                [](const coreci::Graph& graph) {
                    return graph.retry("absent-execution");
                },
                [counts](const coreci::DatabaseResult& outcome, const coreci::GraphChange*) {
                    require(outcome.preparationFailed && !outcome.uncertain, "invalid derivation became SQL failure");
                    ++counts->rejected;
                });
        }
        database.query("SELECT JSON_OBJECT('building',(SELECT COUNT(*) FROM attempts WHERE state='building'))",
                       [&, counts, id, done](const coreci::DatabaseResult& outcome) {
                           require(outcome.error.empty() && outcome.rows.at(0).at("building") == 2 && counts->callbacks == 3 &&
                                       counts->notifications == 3 && counts->noops == 4096 && counts->rejected == 4096 &&
                                       !store.graph().node(id).attempts.back().buildResult.is_null(),
                                   "overlap/drain lost work or repeated callbacks");
                           std::cout << "PASS overlapping commits preserve evidence and advance; bounded synchronous drain\n";
                           done();
                       });
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
                    store->commit(
                        [change](const coreci::Graph&) {
                            return change;
                        },
                        [&](const coreci::DatabaseResult& selected, const coreci::GraphChange*) {
                            require(selected.error.empty() && store->graph().nodes().size() == 2, "atomic selection was not persisted");
                            const auto publicFacts = store->publicView();
                            require(publicFacts.at("executions").size() == 2 && publicFacts.at("projects").size() == 1,
                                    "public projection lost selected waiting targets");
                            require(!publicFacts.at("executions").at(0).contains("assignment") &&
                                        !publicFacts.at("executions").at(0).contains("target_row"),
                                    "public projection exposed internal inputs");
                            const auto previousSource = graph_fixture::source("example", 99, {});
                            store->enroll(
                                "example",
                                "Owner/example",
                                "main",
                                {99, std::string(40, 'a'), std::string(40, 'b'), previousSource.at("definition")},
                                store->getGeneration() - 1,
                                [&](const coreci::DatabaseResult& staleEnrollment) {
                                    require(!staleEnrollment.error.empty(), "enrollment ignored its originally captured graph generation");
                                    const auto id = store->graph().nodes().begin()->first;
                                    const auto building = store->graph().beginBuild(id);
                                    store->commit(
                                        [building](const coreci::Graph& graph) {
                                            return coreci::GraphChange{{building}, graph.nextRebuildRevision()};
                                        },
                                        [&, id](const coreci::DatabaseResult& assigned, const coreci::GraphChange*) {
                                            require(assigned.error.empty() && store->graph().node(id).attempts.back().state == "building",
                                                    "build authority did not commit");
                                            restored->restore([&, id](const coreci::DatabaseResult& restoredResult) {
                                                require(restoredResult.error.empty() && restored->graph().nodes().size() == 2,
                                                        "graph could not restore persisted work: " + restoredResult.error);
                                                const auto& attempt = restored->graph().node(id).attempts.back();
                                                require(attempt.state == "building" && attempt.buildRun.is_null() &&
                                                            !attempt.assignment.is_null(),
                                                        "restoration treated an unknown dispatch outcome as permission to replace it");
                                                auto changedInputs = store->graph().node(id);
                                                changedInputs.source["tag"] = "v0.1.0";
                                                store->commit(
                                                    [changedInputs](const coreci::Graph& graph) {
                                                        return coreci::GraphChange{{changedInputs}, graph.nextRebuildRevision()};
                                                    },
                                                    [&, id](const coreci::DatabaseResult& invalid, const coreci::GraphChange*) {
                                                        require(!invalid.error.empty(), "store accepted rewritten execution inputs");
                                                        const auto& selected = store->graph().node(id);
                                                        const nlohmann::json run{{"id", 123},
                                                                                 {"run_attempt", 1},
                                                                                 {"repository_id", 99},
                                                                                 {"actor_id", 42},
                                                                                 {"workflow_blob_sha", std::string(40, 'c')}};
                                                        const auto bound =
                                                            store->graph().bindRun(id, selected.attempts.back().id, false, run);
                                                        store->commit(
                                                            [bound](const coreci::Graph& graph) {
                                                                return coreci::GraphChange{{bound}, graph.nextRebuildRevision()};
                                                            },
                                                            [&, id, run](const coreci::DatabaseResult& configured,
                                                                         const coreci::GraphChange*) {
                                                                require(configured.error.empty(), "could not advance graph revision");
                                                                auto different = run;
                                                                different["id"] = 999;
                                                                const auto staleNode = restored->graph().bindRun(
                                                                    id, restored->graph().node(id).attempts.back().id, false, different);
                                                                restored->commit(
                                                                    [staleNode](const coreci::Graph& graph) {
                                                                        return coreci::GraphChange{{staleNode},
                                                                                                   graph.nextRebuildRevision()};
                                                                    },
                                                                    [&, id](const coreci::DatabaseResult& stale,
                                                                            const coreci::GraphChange*) {
                                                                        require(!stale.error.empty() && !stale.uncertain,
                                                                                "stale graph mutation was accepted or replayed");
                                                                        auto conflict =
                                                                            graph_fixture::source("other", 100, {}).at("definition");
                                                                        conflict["package"] = "example";
                                                                        store->enroll(
                                                                            "other",
                                                                            "Owner/other",
                                                                            "main",
                                                                            {100, std::string(40, 'a'), std::string(40, 'b'), conflict},
                                                                            store->getGeneration(),
                                                                            [&](const coreci::DatabaseResult& namespaceConflict) {
                                                                                require(!namespaceConflict.error.empty() &&
                                                                                            !namespaceConflict.uncertain,
                                                                                        "another repository claimed a historical package "
                                                                                        "namespace");
                                                                                database->query(
                                                                                    "SELECT JSON_OBJECT('nodes',(SELECT COUNT(*) FROM "
                                                                                    "executions),'attempts',(SELECT COUNT(*) FROM "
                                                                                    "attempts),'repositories',(SELECT COUNT(*) FROM "
                                                                                    "repositories),'run',(SELECT "
                                                                                    "JSON_VALUE(build_run,'$.id') FROM attempts WHERE "
                                                                                    "state='building'))",
                                                                                    [&](const coreci::DatabaseResult& counted) {
                                                                                        require(
                                                                                            counted.error.empty() &&
                                                                                                counted.rows.size() == 1 &&
                                                                                                counted.rows.front().at("nodes") == 2 &&
                                                                                                counted.rows.front().at("attempts") == 2 &&
                                                                                                counted.rows.front().at("repositories") ==
                                                                                                    1 &&
                                                                                                counted.rows.front().at("run") == "123",
                                                                                            "stale mutation changed persisted graph");
                                                                                        overlap(*store, *database, [&]() {
                                                                                            success = true;
                                                                                            std::cout
                                                                                                << "PASS real MariaDB graph persistence, "
                                                                                                   "committed "
                                                                                                   "assignments, "
                                                                                                   "immutable inputs and stale-generation "
                                                                                                   "rollback\n";
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
                                        });
                                    require(store->graph().node(id).attempts.back().state == "waiting",
                                            "in-memory graph advanced before COMMIT acknowledgement");
                                });
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
