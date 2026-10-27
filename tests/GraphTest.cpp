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

    void completeBuild(coreci::Graph& graph, const std::string& id) {
        const auto attempt = graph.node(id).attempts.back().id;
        apply(graph, graph.beginBuild(id));
        check(graph.node(id).attempts.back().assignment.at("artifact_name") ==
                  "core-ci-build-" + id + "-r" + std::to_string(graph.node(id).attempts.back().revision),
              "GitHub artifact name must be assigned once without forbidden colon characters");
        check(graph.node(id).attempts.back().assignment.at("label") == coreci::Graph::label(graph.node(id)),
              "assigned human label differs from the graph display label");
        apply(graph, graph.bindRun(id, attempt, false, run(graph.node(id), false)));
        const auto& building = graph.node(id).attempts.back();
        apply(graph,
              graph.built(id,
                          attempt,
                          {{"run", building.buildRun},
                           {"assignment_sha256", coreci::Graph::digest(building.assignment)},
                           {"artifacts", {{{"id", 1}, {"sha256", std::string(64, 'e')}}}}}));
    }

    void publish(coreci::Graph& graph, const std::string& id) {
        completeBuild(graph, id);
        const auto attempt = graph.node(id).attempts.back().id;
        apply(graph, graph.beginPublication(id));
        apply(graph, graph.bindRun(id, attempt, true, run(graph.node(id), true)));
        const auto& publishing = graph.node(id).attempts.back();
        const json receipt{{"execution", id},
                           {"attempt", attempt},
                           {"assignment_sha256", coreci::Graph::digest(publishing.assignment)},
                           {"run", publishing.publicationRun},
                           {"source_commit", graph.node(id).source.at("commit")},
                           {"feed_revision", std::string(40, 'f')},
                           {"publication_verified_utc", "1900-01-01T00:00:00Z"},
                           {"environment", {{"worker_evidence", "immutable identity"}}},
                           {"packages", {{{"name", "fixture-package"}, {"sha256", std::string(64, 'e')}}}}};
        apply(graph, graph.published(id, attempt, receipt));
        const auto verified = graph.node(id).attempts.back().receipt;
        const auto timestamp = verified.at("publication_verified_utc").get<std::string>();
        check(timestamp.size() == 20 && timestamp.back() == 'Z' && timestamp != receipt.at("publication_verified_utc").get<std::string>(),
              "feed supplied the controller's publication verification time");
        apply(graph, graph.published(id, attempt, receipt));
        check(graph.node(id).attempts.back().receipt == verified, "duplicate publication refreshed its verification time");
        std::vector<coreci::Node> stored;
        for (const auto& [key, node] : graph.nodes()) {
            stored.push_back(node);
            if (key == id) {
                stored.back().attempts.back().receipt.erase("publication_verified_utc");
            }
        }
        coreci::Graph historical;
        historical.restore(stored, graph.nextRebuildRevision());
        apply(historical, historical.published(id, attempt, receipt));
        check(!historical.node(id).attempts.back().receipt.contains("publication_verified_utc"),
              "replay invented a verification time for an existing unrecorded publication");
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
        auto largeSources = sources;
        largeSources["r"]["deployment"]["fixture_padding"] = std::string(18000, '"');
        largeSources["small"] = graph_fixture::source("small", 10, {});
        coreci::Graph bounded;
        bounded.apply(bounded.select("r", largeSources, false));
        bounded.apply(bounded.select("small", largeSources, false));
        const auto tooLarge = find(bounded, "r", "target-first", sha);
        const auto small = find(bounded, "small", "target-first", sha);
        const auto inputs = bounded.dispatchInputs(bounded.node(tooLarge), false);
        check(inputs.at("assignment").get<std::string>().size() < 60000 && inputs.dump().size() > 65535,
              "fixture must exceed full input payload only");
        check(!bounded.blockers(tooLarge).empty() && bounded.blockers(small).empty(),
              "oversized input blocked unrelated work or remained invisible");
        rejects([&]() {
            bounded.beginBuild(tooLarge);
        });
        apply(bounded, bounded.beginBuild(small));
        check(bounded.node(tooLarge).attempts.back().state == "waiting", "oversized work mutated state");

        auto publicationBound = graph;
        apply(publicationBound, publicationBound.beginBuild(root));
        const auto boundedAttempt = publicationBound.node(root).attempts.back();
        const auto boundedRun = run(publicationBound.node(root), false);
        apply(publicationBound, publicationBound.bindRun(root, boundedAttempt.id, false, boundedRun));
        apply(publicationBound,
              publicationBound.built(root,
                                     boundedAttempt.id,
                                     {{"run", boundedRun},
                                      {"assignment_sha256", coreci::Graph::digest(boundedAttempt.assignment)},
                                      {"artifacts", {{{"id", 1}, {"fixture_padding", std::string(65536, 'p')}}}}}));
        check(!publicationBound.publicationReady(root) && !publicationBound.dispatchBlocker(publicationBound.node(root), true).empty(),
              "oversized publication input acquired a writer slot");
        completeBuild(publicationBound, find(publicationBound, "r", "target-second", sha));
        check(publicationBound.publicationReady(find(publicationBound, "r", "target-second", sha)),
              "oversized publication blocked an independent writer");
        coreci::Graph serialized(1);
        std::vector<coreci::Node> snapshot;
        for (const auto& [id, node] : graph.nodes()) {
            snapshot.push_back(node);
        }
        serialized.restore(snapshot, graph.nextRebuildRevision());
        const auto secondRoot = find(serialized, "r", "target-second", sha);
        completeBuild(serialized, root);
        completeBuild(serialized, secondRoot);
        const coreci::GraphChange conflicting{{serialized.beginPublication(root), serialized.beginPublication(secondRoot)},
                                              serialized.nextRebuildRevision()};
        rejects([&]() {
            serialized.apply(conflicting);
        });
        apply(serialized, serialized.beginPublication(root));
        check(!serialized.publicationReady(secondRoot), "uncertain feed writer did not block another publication");
        rejects([&]() {
            serialized.beginPublication(secondRoot);
        });
        coreci::Graph generations;
        auto oldSource = graph_fixture::source("retained", 11, {});
        generations.apply(generations.select("retained", {{"retained", oldSource}}, true));
        const auto historical = find(generations, "retained", "target-first", sha);
        completeBuild(generations, historical);
        apply(generations, generations.failed(historical, generations.node(historical).attempts.back().id, "explicit test failure"));
        auto newSource = oldSource;
        newSource["commit"] = std::string(40, 'e');
        newSource["definition_revision"] = std::string(40, 'e');
        newSource["deployment"]["worker"]["tooling_sha"] = std::string(40, 'f');
        newSource["deployment"]["source_workflow"]["path"] = ".github/workflows/build-parallel.yml";
        newSource["deployment"]["publication_workflow"]["path"] = ".github/workflows/publish-parallel.yml";
        generations.apply(generations.select("retained", {{"retained", newSource}}, true));
        generations.apply(generations.retry(historical));
        const auto retrying = generations.beginBuild(historical);
        check(retrying.attempts.back().assignment.at("source").at("deployment") == oldSource.at("deployment"),
              "historical retry adopted new tooling or workflow paths");
        check(generations.node(find(generations, "retained", "target-first", std::string(40, 'e'))).source.at("deployment") ==
                  newSource.at("deployment"),
              "new selection did not retain its own generation");
        coreci::Graph capacity;
        std::map<std::string, json> independent;
        for (unsigned index = 1; index <= 5; ++index) {
            const auto name = "root" + std::to_string(index);
            independent[name] = graph_fixture::source(name, index, {});
            capacity.apply(capacity.select(name, independent, false));
        }
        std::vector<std::string> ready;
        for (const auto& [id, node] : capacity.nodes()) {
            ready.push_back(id);
        }
        for (const auto& id : ready) {
            completeBuild(capacity, id);
        }
        coreci::GraphChange admitted{{}, capacity.nextRebuildRevision()};
        for (std::size_t index = 0; index < 8; ++index) {
            admitted.nodes.push_back(capacity.beginPublication(ready[index], index));
        }
        capacity.apply(admitted);
        check(!capacity.publicationReady(ready[8]), "ninth publisher exceeded default feed capacity");
        check(capacity.publicationBlocker(ready[8]).find("capacity") != std::string::npos, "capacity blocker was hidden");
        rejects([&]() {
            capacity.beginPublication(ready[8]);
        });
        auto parallel = graph;
        completeBuild(parallel, root);
        completeBuild(parallel, secondRoot);
        parallel.apply({{parallel.beginPublication(root), parallel.beginPublication(secondRoot)}, parallel.nextRebuildRevision()});
        std::vector<coreci::Node> assigned;
        for (const auto& [id, node] : parallel.nodes()) {
            assigned.push_back(node);
        }
        coreci::Graph reduced(1);
        reduced.restore(assigned, parallel.nextRebuildRevision());
        check(reduced.node(root).attempts.back().state == "publishing" && reduced.node(secondRoot).attempts.back().state == "publishing",
              "lowering capacity rejected assigned independent publications");
        check(!parallel.blockers(a).empty(), "unverified parallel publication satisfied a dependent");
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
        const auto priorBuild = graph.node(b).attempts.front();
        const auto retryBuild = graph.beginBuild(b).attempts.back().assignment;
        check(retryBuild.at("environment_from").size() == 1 && retryBuild.at("environment_from").front().at("run") == priorBuild.buildRun &&
                  retryBuild.at("environment_from").front().at("assignment_sha256") == coreci::Graph::digest(priorBuild.assignment),
              "retry lost its assigned environment evidence reference");
        check(retryBuild.at("label") != priorBuild.assignment.at("label") &&
                  retryBuild.at("environment_from").front().at("label") == priorBuild.assignment.at("label"),
              "retry must distinguish its rebuild while retaining earlier immutable display evidence");
        check(retryBuild.at("dependencies") == priorBuild.assignment.at("dependencies"), "retry changed prerequisite artifact identities");
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
        std::map<std::string, json> sdkSources{{"sdkroot", graph_fixture::source("sdkroot", 21, {})},
                                               {"sdkapp", graph_fixture::source("sdkapp", 22, {"sdkroot"})}};
        for (auto& [name, source] : sdkSources) {
            auto row = source["definition"]["matrix"][0];
            row["family"] = "openwrt";
            row["distribution"] = "openwrt";
            row["suite"] = "99.1";
            row["build"] = {{"target", "custom/target"}, {"format", "apk"}};
            source["definition"]["build"] = {{"openwrt", "ci/openwrt.sh"}};
            source["definition"]["matrix"] = {row};
        }
        coreci::Graph sdkGraph;
        const auto sdkWork = sdkGraph.select("sdkroot", sdkSources, false);
        check(sdkWork.nodes.size() == 2, "OpenWrt graph selection must not require controller SDK resolution");
        sdkGraph.apply(sdkWork);
        for (const auto& [id, node] : sdkGraph.nodes()) {
            check(!node.source.contains("environment"), "worker environment leaked into graph source selection");
            if (node.prerequisites.empty()) {
                const auto assigned = sdkGraph.beginBuild(id);
                check(!assigned.attempts.back().assignment.contains("environment"), "controller assigned a resolved SDK");
            }
        }

        coreci::Graph observations;
        observations.apply(observations.select("r", {{"r", sources.at("r")}}, false));
        const auto observedId = observations.nodes().begin()->first;
        apply(observations, observations.beginBuild(observedId));
        const auto observedAttempt = observations.node(observedId).attempts.back().id;
        const auto observedRun = run(observations.node(observedId), false);
        json progress{{"run_id", observedRun.at("id")},
                      {"status", "in_progress"},
                      {"job_id", 123},
                      {"job_status", "in_progress"},
                      {"started_at", "2026-10-26T10:00:00Z"}};
        apply(observations, observations.bindRun(observedId, observedAttempt, false, observedRun, progress));
        const auto originalAssignment = observations.node(observedId).attempts.back().assignment;
        apply(observations, observations.failed(observedId, observedAttempt, "verified failure"));
        progress["status"] = "completed";
        progress["job_status"] = "completed";
        progress["completed_at"] = "2026-10-26T10:01:00Z";
        progress["conclusion"] = "failure";
        apply(observations, observations.bindRun(observedId, observedAttempt, false, observedRun, progress));
        const auto complete = observations.node(observedId);
        const json stale{{"run_id", observedRun.at("id")}, {"status", "queued"}, {"job_status", "queued"}, {"completed_at", nullptr}};
        apply(observations, observations.bindRun(observedId, observedAttempt, false, observedRun, stale));
        check(observations.node(observedId) == complete, "late progress regressed completion evidence or terminal state");
        auto rerunIdentity = observedRun;
        rerunIdentity["run_attempt"] = 2;
        rejects([&]() {
            observations.bindRun(observedId, observedAttempt, false, rerunIdentity, progress);
        });
        auto conflict = progress;
        conflict["started_at"] = "2026-10-26T09:00:00Z";
        rejects([&]() {
            observations.bindRun(observedId, observedAttempt, false, observedRun, conflict);
        });
        conflict = progress;
        conflict["job_id"] = 999;
        rejects([&]() {
            observations.bindRun(observedId, observedAttempt, false, observedRun, conflict);
        });
        auto erased = complete;
        erased.attempts.back().evidence.clear();
        rejects([&]() {
            apply(observations, erased);
        });
        check(complete.attempts.back().buildRun == observedRun && complete.attempts.back().assignment == originalAssignment,
              "progress metadata changed immutable identities or inputs");

        std::cout << "PASS local graph/state boundaries: diamond, independent targets, pinned inputs, failures/retry, delayed reports and "
                     "supersession\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    return status;
}
