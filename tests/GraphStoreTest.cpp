#include "GraphStore.h"

#include "GraphFixtures.h"

#include <core/SNodeC.h>
#include <iostream>
#include <utils/Config.h>
#include <utils/SubCommand.h>

namespace {
    class Probe : public utils::SubCommand {
    public:
        static constexpr std::string_view NAME{"probe"};
        static constexpr std::string_view DESCRIPTION{"Graph persistence boundary"};
        explicit Probe(utils::SubCommand* parent)
            : SubCommand(parent, this, "Graph persistence") {
            required(addOptionVariable("--socket", socket, "Fixture socket", "path", CLI::TypeValidator<std::string>()));
            required(addOptionVariable("--case", mode, "Boundary", "string", CLI::TypeValidator<std::string>()));
        }
        std::string socket;
        std::string mode;
    };
} // namespace

int main(int argc, char* argv[]) {
    using coreci::Json;
    const auto* config = utils::Config::configRoot.newSubCommand<Probe>();
    core::SNodeC::init(argc, argv);
    std::unique_ptr<database::mariadb::MariaDBClient> client;
    std::unique_ptr<coreci::Database> database;
    std::unique_ptr<coreci::GraphStore> graph;
    bool passed = true;
    bool started = false;
    unsigned int notifications = 0;
    unsigned int callbacks = 0;
    const auto finish = [&] {
        database->query("SELECT document FROM graph_state WHERE id=1", [&](const auto& read) {
            passed = passed && read.error.empty() && read.rows.size() == 1 && read.rows.front() == graph->graph();
            const auto view = graph->view();
            const auto projection = graph->publicView();
            passed = passed && view.at("executions").size() == 20 && projection.at("projects").size() == 4;
            passed = passed && graph->graph().at("generation") == 13;
            for (const auto& row : projection.at("executions")) {
                passed = passed && row.contains("coordinates") && row.contains("build_job") && row.contains("publication_verified_utc");
            }
            passed = passed && notifications == (config->mode == "apply" ? 13u : 0u);
            graph.reset();
            database.reset();
            client.reset();
        });
    };
    client = std::make_unique<database::mariadb::MariaDBClient>(
        database::mariadb::MariaDBConnectionDetails{.connectionName = "graph-boundary",
                                                    .hostname = "",
                                                    .username = "root",
                                                    .password = "",
                                                    .database = "boundary",
                                                    .port = 0,
                                                    .socket = config->socket,
                                                    .flags = 0},
        [&](const auto& connection) {
            if (connection.error) {
                passed = false;
                std::cerr << connection.errorMessage << '\n';
                graph.reset();
                database.reset();
                client.reset();
            } else if (connection.connected && !started) {
                started = true;
                graph->restore([&](const auto& restored) {
                    if (config->mode == "missing") {
                        passed = passed && !restored.error.empty();
                        graph.reset();
                        database.reset();
                        client.reset();
                    } else if (!restored.error.empty()) {
                        passed = false;
                        std::cerr << restored.error << '\n';
                        graph.reset();
                        database.reset();
                        client.reset();
                    } else if (config->mode == "restore") {
                        graph->change(
                            [](Json& state) {
                                return coreci::graph::advance(state);
                            },
                            [&](const auto& result, const Json& ready) {
                                passed = passed && result.error.empty() && ready.empty();
                                finish();
                            });
                    } else {
                        graph->change(
                            [](Json& state) {
                                coreci::graph::select(
                                    state, "core", coreci::test::sources(), false, state.at("next_revision").get<std::uint64_t>());
                                return coreci::graph::advance(state);
                            },
                            [&](const auto& result, const Json& ready) {
                                passed = passed && result.error.empty() && ready.size() == 5;
                                ++callbacks;
                                for (unsigned int index = 0; index < 12; ++index) {
                                    graph->change(
                                        [index, ready](Json& state) {
                                            const auto id = ready.at(index % 5).at("execution").get<std::string>();
                                            auto& node = state.at("executions").at(id);
                                            const auto& attempt = node.at("attempts").back();
                                            const Json identity{{"id", 100 + index % 5},
                                                                {"run_attempt", 1},
                                                                {"html_url", "https://github.com/Owner/core/actions/runs/100"}};
                                            const auto status = index < 5 ? "queued" : index < 10 ? "in_progress" : "completed";
                                            coreci::graph::observe(node, attempt.at("id"), false, identity, {{"status", status}});
                                            return Json();
                                        },
                                        [&](const auto& changed, const auto&) {
                                            passed = passed && changed.error.empty();
                                            ++callbacks;
                                        });
                                }
                                graph->change(
                                    [](Json& state) {
                                        return coreci::graph::advance(state);
                                    },
                                    [&](const auto& unchanged, const Json& work) {
                                        passed = passed && unchanged.error.empty() && work.empty();
                                        graph->change(
                                            [](Json& state) -> Json {
                                                state["repositories"]["not-committed"] = true;
                                                throw std::invalid_argument("invalid graph event");
                                            },
                                            [&](const auto& rejected, const auto&) {
                                                passed = passed && !rejected.error.empty() &&
                                                         !graph->graph().at("repositories").contains("not-committed");
                                                finish();
                                            });
                                    });
                            });
                    }
                });
            }
        });
    database = std::make_unique<coreci::Database>(*client);
    graph = std::make_unique<coreci::GraphStore>(*database);
    graph->onChanged = [&](bool committed, bool) {
        ++notifications;
        passed = passed && committed && callbacks == notifications;
    };
    client->connect();
    const auto status = core::SNodeC::start();
    std::cout << (passed && started ? "PASS " : "FAIL ") << config->mode << '\n';
    return passed && started ? status : 1;
}
