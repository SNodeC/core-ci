#include "GraphStore.h"

#include <algorithm>
#include <exception>

namespace coreci {
    namespace {
        std::string document(const nlohmann::json& json) {
            return json.is_null() ? "NULL" : Database::text(json.dump());
        }

        void reserveNamespace(std::vector<DatabaseStatement>& statements, const SelectedDefinition& definition) {
            const auto id = std::to_string(definition.repositoryId);
            const auto package = Database::text(definition.document.at("package").get<std::string>());
            statements.push_back({"DELETE FROM namespace_claims WHERE repository_id=" + id + " AND namespace=" + package, std::nullopt});
            statements.push_back({"INSERT INTO namespace_claims(namespace,repository_id,definition_revision) SELECT " + package + "," + id +
                                      "," + Database::text(definition.revision) +
                                      " WHERE NOT EXISTS (SELECT 1 FROM namespace_claims WHERE repository_id<>" + id +
                                      " AND (namespace=" + package + " OR LEFT(namespace,CHAR_LENGTH(" + package + ")+1)=CONCAT(" +
                                      package + ",'-')" + " OR LEFT(" + package + ",CHAR_LENGTH(namespace)+1)=CONCAT(namespace,'-')))",
                                  1});
        }

        std::string attemptValues(const Attempt& attempt) {
            return Database::text(attempt.state) + "," + document(attempt.assignment) + "," + document(attempt.buildRun) + "," +
                   document(attempt.buildResult) + "," + document(attempt.publicationRun) + "," + document(attempt.receipt) + "," +
                   Database::text(attempt.failure);
        }
    } // namespace

    GraphStore::GraphStore(Database& database)
        : database(database) {
    }

    const Graph& GraphStore::graph() const {
        return state;
    }

    std::string GraphStore::guard(std::uint64_t nextRevision, std::uint64_t expectedGeneration) const {
        return "UPDATE graph_revision SET generation=generation+1,next_revision=" + std::to_string(nextRevision) +
               " WHERE id=1 AND generation=" + std::to_string(expectedGeneration) +
               " AND next_revision=" + std::to_string(state.nextRebuildRevision());
    }

    void GraphStore::restore(const Database::Callback& callback) {
        const auto restoring = std::make_shared<Restoring>();
        restoring->callback = callback;
        database.query("SELECT JSON_OBJECT('generation',generation,'next_revision',next_revision) FROM graph_revision WHERE id=1",
                       [this, restoring](const DatabaseResult& result) {
                           if (!result.error.empty() || result.rows.size() != 1) {
                               restoring->callback({{}, "graph schema/revision is unavailable", false});
                           } else {
                               try {
                                   restoring->generation = result.rows.front().at("generation").get<std::uint64_t>();
                                   restoring->nextRevision = result.rows.front().at("next_revision").get<std::uint64_t>();
                                   readNodes(restoring);
                               } catch (const std::exception& error) {
                                   restoring->callback({{}, error.what(), false});
                               }
                           }
                       });
    }

    void GraphStore::readNodes(const std::shared_ptr<Restoring>& restoring) {
        database.query("SELECT JSON_OBJECT('id',e.id,'source',JSON_EXTRACT(e.source,'$'),'target',JSON_EXTRACT(e.target,'$'),"
                       "'cause',JSON_EXTRACT(e.cause,'$'),'automatic',e.automatic,'superseded_by',e.superseded_by,"
                       "'definition',JSON_EXTRACT(d.document,'$'),'blob_sha',d.blob_sha) FROM executions e JOIN definitions d "
                       "ON d.repository_id=e.repository_id AND d.revision=e.definition_revision",
                       [this, restoring](const DatabaseResult& result) {
                           if (!result.error.empty()) {
                               restoring->callback(result);
                           } else {
                               try {
                                   for (const auto& row : result.rows) {
                                       Node node;
                                       node.id = row.at("id").get<std::string>();
                                       node.source = row.at("source");
                                       node.target = row.at("target");
                                       node.cause = row.at("cause");
                                       node.automatic = row.at("automatic").get<int>() != 0;
                                       node.supersededBy =
                                           row.at("superseded_by").is_null() ? "" : row.at("superseded_by").get<std::string>();
                                       Graph::verifyDefinition(node, row.at("definition"), row.at("blob_sha").get<std::string>());
                                       restoring->nodes.emplace(node.id, std::move(node));
                                   }
                                   readEdges(restoring);
                               } catch (const std::exception& error) {
                                   restoring->callback({{}, error.what(), false});
                               }
                           }
                       });
    }

    void GraphStore::readEdges(const std::shared_ptr<Restoring>& restoring) {
        database.query(
            "SELECT JSON_OBJECT('execution',execution_id,'prerequisite',prerequisite_id) FROM prerequisites ORDER BY execution_id,position",
            [this, restoring](const DatabaseResult& result) {
                if (!result.error.empty()) {
                    restoring->callback(result);
                } else {
                    try {
                        for (const auto& row : result.rows) {
                            restoring->nodes.at(row.at("execution").get<std::string>())
                                .prerequisites.push_back(row.at("prerequisite").get<std::string>());
                        }
                        readAttempts(restoring);
                    } catch (const std::exception& error) {
                        restoring->callback({{}, error.what(), false});
                    }
                }
            });
    }

    void GraphStore::readAttempts(const std::shared_ptr<Restoring>& restoring) {
        database.query("SELECT JSON_OBJECT('id',id,'execution',execution_id,'revision',revision,'state',state,"
                       "'assignment',JSON_EXTRACT(assignment,'$'),'build_run',JSON_EXTRACT(build_run,'$'),"
                       "'build_result',JSON_EXTRACT(build_result,'$'),'publication_run',JSON_EXTRACT(publication_run,'$'),"
                       "'receipt',JSON_EXTRACT(receipt,'$'),'failure',failure) FROM attempts ORDER BY revision",
                       [this, restoring](const DatabaseResult& result) {
                           if (!result.error.empty()) {
                               restoring->callback(result);
                           } else {
                               try {
                                   for (const auto& row : result.rows) {
                                       Attempt attempt;
                                       attempt.id = row.at("id").get<std::string>();
                                       attempt.revision = row.at("revision").get<std::uint64_t>();
                                       attempt.state = row.at("state").get<std::string>();
                                       attempt.assignment = row.at("assignment");
                                       attempt.buildRun = row.at("build_run");
                                       attempt.buildResult = row.at("build_result");
                                       attempt.publicationRun = row.at("publication_run");
                                       attempt.receipt = row.at("receipt");
                                       attempt.failure = row.at("failure").get<std::string>();
                                       restoring->nodes.at(row.at("execution").get<std::string>()).attempts.push_back(std::move(attempt));
                                   }
                                   std::vector<Node> nodes;
                                   for (const auto& [id, node] : restoring->nodes) {
                                       nodes.push_back(node);
                                   }
                                   state.restore(nodes, restoring->nextRevision);
                                   generation = restoring->generation;
                                   restoring->callback({});
                               } catch (const std::exception& error) {
                                   restoring->callback({{}, error.what(), false});
                               }
                           }
                       });
    }

    std::uint64_t GraphStore::getGeneration() const {
        return generation;
    }

    void GraphStore::enroll(const std::string& name,
                            const std::string& repository,
                            const std::string& branch,
                            const SelectedDefinition& definition,
                            std::uint64_t expectedGeneration,
                            const Database::Callback& callback) {
        const auto id = std::to_string(definition.repositoryId);
        const auto revision = Database::text(definition.revision);
        std::vector<DatabaseStatement> statements{
            {guard(state.nextRebuildRevision(), expectedGeneration), 1},
            {"INSERT INTO repositories(github_id,name,full_name,editing_ref,configuration_revision) VALUES (" + id + "," +
                 Database::text(name) + "," + Database::text(repository) + "," + Database::text(branch) + "," + revision +
                 ") ON DUPLICATE KEY UPDATE "
                 "github_id=IF(name=VALUES(name) AND full_name=VALUES(full_name) AND editing_ref=VALUES(editing_ref),github_id,0),"
                 "configuration_revision=VALUES(configuration_revision)",
             std::nullopt},
            {"INSERT INTO definitions(repository_id,revision,blob_sha,document) VALUES (" + id + "," + revision + "," +
                 Database::text(definition.blobSha) + "," + document(definition.document) +
                 ") ON DUPLICATE KEY UPDATE revision=VALUES(revision)",
             std::nullopt}};
        reserveNamespace(statements, definition);
        database.write(statements, [this, callback](const DatabaseResult& result) {
            if (result.error.empty()) {
                ++generation;
            }
            if (onChanged) {
                onChanged();
            }
            callback(result);
        });
    }

    void GraphStore::commit(const GraphChange& change, const Database::Callback& callback) {
        std::string invalid;
        try {
            state.validate(change);
        } catch (const std::exception& error) {
            invalid = error.what();
        }
        if (!invalid.empty()) {
            callback({{}, invalid, false});
        } else if (change.nextRevision == state.nextRebuildRevision() && change.definitions.empty() &&
                   std::all_of(change.nodes.begin(), change.nodes.end(), [&](const auto& node) {
                       return state.nodes().contains(node.id) && state.node(node.id) == node;
                   })) {
            callback({});
        } else {
            std::vector<DatabaseStatement> statements{{guard(change.nextRevision, generation), 1}};
            for (const auto& selected : change.definitions) {
                const auto id = std::to_string(selected.repositoryId);
                statements.push_back({"INSERT INTO definitions(repository_id,revision,blob_sha,document) SELECT " + id + "," +
                                          Database::text(selected.revision) + "," + Database::text(selected.blobSha) + "," +
                                          document(selected.document) +
                                          " WHERE NOT EXISTS (SELECT 1 FROM definitions WHERE repository_id=" + id +
                                          " AND revision=" + Database::text(selected.revision) + ")",
                                      std::nullopt});
                reserveNamespace(statements, selected);
            }
            for (const auto& node : change.nodes) {
                if (!state.nodes().contains(node.id)) {
                    statements.push_back(
                        {"INSERT INTO executions(id,repository_id,definition_revision,source,target,cause,automatic) VALUES (" +
                             Database::text(node.id) + "," + std::to_string(node.source.at("repository_id").get<std::uint64_t>()) + "," +
                             Database::text(node.source.at("definition_revision").get<std::string>()) + "," + document(node.source) + "," +
                             document(node.target) + "," + document(node.cause) + "," + (node.automatic ? "1" : "0") + ")",
                         1});
                }
            }
            for (const auto& node : change.nodes) {
                const auto previous = state.nodes().find(node.id);
                if (previous == state.nodes().end()) {
                    for (std::size_t index = 0; index < node.prerequisites.size(); ++index) {
                        statements.push_back({"INSERT INTO prerequisites(execution_id,prerequisite_id,position) VALUES (" +
                                                  Database::text(node.id) + "," + Database::text(node.prerequisites[index]) + "," +
                                                  std::to_string(index) + ")",
                                              1});
                    }
                }
                if (previous != state.nodes().end() && previous->second.automatic != node.automatic) {
                    statements.push_back({"UPDATE executions SET automatic=0 WHERE id=" + Database::text(node.id) + " AND automatic=1", 1});
                }
                if (!node.supersededBy.empty() && (previous == state.nodes().end() || previous->second.supersededBy != node.supersededBy)) {
                    statements.push_back({"UPDATE executions SET superseded_by=" + Database::text(node.supersededBy) +
                                              " WHERE id=" + Database::text(node.id) + " AND superseded_by IS NULL",
                                          1});
                }
                for (const auto& attempt : node.attempts) {
                    const Attempt* old = nullptr;
                    if (previous != state.nodes().end()) {
                        for (const auto& candidate : previous->second.attempts) {
                            if (candidate.id == attempt.id) {
                                old = &candidate;
                            }
                        }
                    }
                    if (old == nullptr) {
                        statements.push_back({"INSERT INTO "
                                              "attempts(id,execution_id,revision,state,assignment,build_run,build_result,publication_run,"
                                              "receipt,failure) VALUES (" +
                                                  Database::text(attempt.id) + "," + Database::text(node.id) + "," +
                                                  std::to_string(attempt.revision) + "," + attemptValues(attempt) + ")",
                                              1});
                    } else if (*old != attempt) {
                        statements.push_back(
                            {"UPDATE attempts SET state=" + Database::text(attempt.state) + ",assignment=" + document(attempt.assignment) +
                                 ",build_run=" + document(attempt.buildRun) + ",build_result=" + document(attempt.buildResult) +
                                 ",publication_run=" + document(attempt.publicationRun) + ",receipt=" + document(attempt.receipt) +
                                 ",failure=" + Database::text(attempt.failure) + " WHERE id=" + Database::text(attempt.id) +
                                 " AND state=" + Database::text(old->state),
                             1});
                    }
                }
            }
            database.write(statements, [this, change, callback](const DatabaseResult& result) {
                if (result.error.empty()) {
                    state.apply(change);
                    ++generation;
                }
                if (onChanged) {
                    onChanged();
                }
                callback(result);
            });
        }
    }

    nlohmann::json GraphStore::view() const {
        nlohmann::json nodes = nlohmann::json::array();
        for (const auto& [id, node] : state.nodes()) {
            const auto& attempt = node.attempts.back();
            const std::string activity =
                !node.supersededBy.empty()                                      ? "superseded"
                : attempt.state == "building" && !attempt.buildResult.is_null() ? "build complete; waiting for publication"
                : attempt.state == "building" && attempt.buildRun.is_null()     ? "build assigned; awaiting GitHub run identity"
                                                                                : attempt.state;
            const auto buildJob =
                attempt.buildResult.is_object() ? attempt.buildResult.value("build_job", nlohmann::json()) : nlohmann::json();
            nodes.push_back({{"id", id},
                             {"label", Graph::label(node)},
                             {"repository", node.source.at("repository")},
                             {"tag", node.source.at("tag")},
                             {"source_commit", node.source.at("commit")},
                             {"definition_revision", node.source.at("definition_revision")},
                             {"target", node.target.at("id")},
                             {"target_row", node.target},
                             {"state", attempt.state},
                             {"activity", activity},
                             {"revision", attempt.revision},
                             {"attempt", attempt.id},
                             {"prerequisites", node.prerequisites},
                             {"blockers", state.blockers(id)},
                             {"failure", attempt.failure},
                             {"build_run", attempt.buildRun},
                             {"build_job", buildJob},
                             {"publication_run", attempt.publicationRun},
                             {"receipt", attempt.receipt},
                             {"superseded_by", node.supersededBy},
                             {"automatic", node.automatic}});
        }
        return {{"generation", generation}, {"mutations_allowed", database.mutationsAllowed()}, {"executions", nodes}};
    }
} // namespace coreci
