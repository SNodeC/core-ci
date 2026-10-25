#include "Graph.h"

#include "GraphFixtures.h"

#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
    using nlohmann::json;

    void check(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    void rejects(const std::function<void()>& action) {
        bool rejected = false;
        try {
            action();
        } catch (const std::exception&) {
            rejected = true;
        }
        check(rejected, "invalid graph transition/selection was accepted");
    }

    std::string find(const coreci::Graph& graph, const std::string& name, const std::string& target, const std::string& commit) {
        std::string result;
        for (const auto& [id, node] : graph.nodes()) {
            if (node.source.at("name") == name && node.target.at("id") == target && node.source.at("commit") == commit &&
                node.supersededBy.empty()) {
                result = id;
            }
        }
        check(!result.empty(), "expected graph node is absent");
        return result;
    }

    void apply(coreci::Graph& graph, const coreci::Node& node) {
        graph.apply({{node}, graph.nextRebuildRevision()});
    }

    json run(const coreci::Node& node, bool publication) {
        return {{"id", node.attempts.back().revision * 10 + (publication ? 1 : 0)},
                {"run_attempt", 1},
                {"repository_id", publication ? json(1000) : node.source.at("repository_id")},
                {"actor_id", 42},
                {"workflow_blob_sha", std::string(40, publication ? 'd' : 'c')}};
    }

    void publish(coreci::Graph& graph, const std::string& id) {
        const auto attempt = graph.node(id).attempts.back().id;
        apply(graph, graph.beginBuild(id));
        apply(graph, graph.bindRun(id, attempt, false, run(graph.node(id), false)));
        const auto& building = graph.node(id).attempts.back();
        apply(graph,
              graph.built(id,
                          attempt,
                          {{"run", building.buildRun},
                           {"assignment_sha256", coreci::Graph::digest(building.assignment)},
                           {"artifacts", {{{"id", 1}, {"sha256", std::string(64, 'e')}}}}}));
        apply(graph, graph.beginPublication(id));
        apply(graph, graph.bindRun(id, attempt, true, run(graph.node(id), true)));
        const auto& publishing = graph.node(id).attempts.back();
        const json receipt{{"execution", id},
                           {"attempt", attempt},
                           {"assignment_sha256", coreci::Graph::digest(publishing.assignment)},
                           {"run", publishing.publicationRun},
                           {"source_commit", graph.node(id).source.at("commit")},
                           {"feed_revision", std::string(40, 'f')},
                           {"packages", {{{"name", "fixture-package"}, {"sha256", std::string(64, 'e')}}}}};
        apply(graph, graph.published(id, attempt, receipt));
        apply(graph, graph.published(id, attempt, receipt));
        auto conflicting = receipt;
        conflicting["feed_revision"] = std::string(40, '0');
        rejects([&]() {
            graph.published(id, attempt, conflicting);
        });
    }
} // namespace

int main() {
    int status = 0;
    try {
        std::map<std::string, json> sources{{"r", graph_fixture::source("r", 1, {})},
                                            {"a", graph_fixture::source("a", 2, {"r"})},
                                            {"b", graph_fixture::source("b", 3, {"r"})},
                                            {"j", graph_fixture::source("j", 4, {"a", "b"})}};
        coreci::Graph graph;
        auto selection = graph.select("r", sources, false);
        check(selection.nodes.size() == 8, "diamond must be created atomically for both targets");
        graph.apply(selection);
        check(graph.select("r", sources, false).nodes.empty(), "duplicate selection created executions");
        auto unrelated = sources;
        unrelated["unrelated"] = graph_fixture::source("unrelated", 9, {"tagless"});
        unrelated["unrelated"].erase("environments");
        check(graph.select("r", unrelated, false).nodes.empty(), "unrelated tagless work blocked an independent selection");
        auto claimed = graph;
        claimed.apply(claimed.select("r", sources, true));
        check(claimed.nodes().size() == 8 && claimed.nextRebuildRevision() == graph.nextRebuildRevision(),
              "an explicit claim of existing work created duplicate executions/revisions");
        auto differentDependent = sources;
        differentDependent["a"]["tag"] = "v9.0.0";
        differentDependent["a"]["commit"] = std::string(40, 'e');
        check(graph.select("r", differentDependent, false).nodes.empty(),
              "duplicate root delivery selected unrelated new dependent inputs");

        const auto sha = std::string(40, 'a');
        const auto root = find(graph, "r", "target-first", sha);
        const auto a = find(graph, "a", "target-first", sha);
        const auto b = find(graph, "b", "target-first", sha);
        const auto join = find(graph, "j", "target-first", sha);
        const auto otherJoin = find(graph, "j", "target-second", sha);
        check(graph.node(join).prerequisites.size() == 2 && !graph.blockers(join).empty(), "join did not bind both prerequisites");
        publish(graph, root);
        check(graph.blockers(a).empty() && graph.blockers(b).empty(), "published root did not unblock both branches");
        publish(graph, a);
        check(!graph.blockers(join).empty(), "one published diamond branch made join eligible");
        apply(graph, graph.beginBuild(b));
        const auto failedAttempt = graph.node(b).attempts.back().id;
        rejects([&]() {
            graph.failed(b, failedAttempt, "absent result");
        });
        auto wrongRun = run(graph.node(b), false);
        wrongRun["repository_id"] = 1000;
        rejects([&]() {
            graph.bindRun(b, failedAttempt, false, wrongRun);
        });
        apply(graph, graph.bindRun(b, failedAttempt, false, run(graph.node(b), false)));
        apply(graph, graph.failed(b, failedAttempt, "verified GitHub build failure"));
        check(!graph.blockers(join).empty(), "failed branch did not block join");
        const auto oldRevision = graph.node(b).attempts.back().revision;
        graph.apply(graph.retry(b));
        check(graph.node(b).attempts.size() == 2 && graph.node(b).attempts.back().revision > oldRevision,
              "retry did not retain history/fresh revision");
        rejects([&]() {
            graph.failed(b, failedAttempt, "late report");
        });
        publish(graph, b);
        check(graph.blockers(join).empty(), "retried prerequisite did not unblock the same logical join");
        check(!graph.blockers(otherJoin).empty(), "one target unblocked an independent target");
        publish(graph, join);
        rejects([&]() {
            graph.retry(join);
        });
        rejects([&]() {
            graph.beginBuild(otherJoin);
        });
        const auto frozen = graph.node(join).attempts.back().assignment;

        auto newer = sources;
        newer["r"]["tag"] = "v1.1.0-rc.1";
        newer["r"]["commit"] = std::string(40, 'b');
        newer["r"]["definition_revision"] = std::string(40, 'b');
        graph.apply(graph.select("r", newer, false));
        check(!graph.node(otherJoin).supersededBy.empty(), "older waiting automatic work was not superseded");
        check(graph.node(join).supersededBy.empty() && graph.node(join).attempts.back().assignment == frozen,
              "new inputs changed published work");
        graph.apply(graph.select("r", sources, true));
        const auto explicitOlder = find(graph, "r", "target-second", sha);
        check(!graph.node(explicitOlder).automatic && graph.node(explicitOlder).source.at("tag") == "v1.0.0",
              "explicit older source was substituted");
        auto missing = newer;
        missing["r"]["tag"] = "v2.0.0";
        missing["r"]["commit"] = std::string(40, 'e');
        missing["r"]["definition_revision"] = std::string(40, 'e');
        const auto count = graph.nodes().size();
        rejects([&]() {
            graph.select("a", missing, true);
        });
        check(graph.nodes().size() == count, "missing prerequisite created automatic upward work");

        coreci::Graph rejectedDispatch;
        auto independent = graph_fixture::source("solo", 8, {});
        rejectedDispatch.apply(rejectedDispatch.select("solo", {{"solo", independent}}, true));
        const auto soloId = rejectedDispatch.nodes().begin()->first;
        apply(rejectedDispatch, rejectedDispatch.beginBuild(soloId));
        const auto soloAttempt = rejectedDispatch.node(soloId).attempts.back().id;
        rejects([&]() {
            rejectedDispatch.rejectedDispatch(soloId, soloAttempt, false, 500, true, "uncertain server response");
        });
        rejects([&]() {
            rejectedDispatch.rejectedDispatch(soloId, soloAttempt, false, 408, true, "uncertain timeout");
        });
        apply(rejectedDispatch, rejectedDispatch.rejectedDispatch(soloId, soloAttempt, false, 403, true, "definitive dispatch rejection"));
        const auto rejectedRevision = rejectedDispatch.node(soloId).attempts.back().revision;
        rejectedDispatch.apply(rejectedDispatch.retry(soloId));
        check(rejectedDispatch.node(soloId).attempts.back().revision > rejectedRevision,
              "dispatch rejection retry reused package identity");

        std::vector<coreci::Node> persisted;
        for (const auto& [id, node] : graph.nodes()) {
            persisted.push_back(node);
        }
        coreci::Graph restored;
        restored.restore(persisted, graph.nextRebuildRevision());
        check(restored.nodes().size() == graph.nodes().size() && restored.node(join).attempts.back().assignment == frozen,
              "restart changed frozen selection/attempt inputs");
        check(restored.node(explicitOlder).supersededBy.empty(), "explicit older selection was superseded");
        std::cout << "PASS local graph/state boundaries: diamond, independent targets, pinned inputs, failures/retry, delayed reports and "
                     "supersession\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    return status;
}
