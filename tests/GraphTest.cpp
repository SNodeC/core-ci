#include "Graph.h"

#include "GraphFixtures.h"
#include "Version.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using coreci::Json;
namespace graph = coreci::graph;

namespace {
    void check(bool condition, const std::string& error) {
        if (!condition) {
            throw std::runtime_error(error);
        }
    }

    void rejects(const std::function<void()>& action, const std::string& error) {
        bool rejected = false;
        try {
            action();
        } catch (const std::exception&) {
            rejected = true;
        }
        check(rejected, error);
    }

    Json run(unsigned int id) {
        return {{"id", id}, {"run_attempt", 1}};
    }

    void build(Json& node, unsigned int runId) {
        auto& attempt = node.at("attempts").back();
        const auto identity = run(runId);
        graph::observe(node, attempt.at("id"), false, identity, {{"status", "completed"}, {"conclusion", "success"}});
        graph::built(
            node,
            attempt.at("id"),
            {{"run", identity}, {"assignment_sha256", coreci::digest(attempt.at("assignment"))}, {"artifacts", {{{"id", runId + 1000}}}}});
    }

    void publish(Json& node, unsigned int runId) {
        auto& attempt = node.at("attempts").back();
        const auto identity = run(runId);
        graph::observe(node, attempt.at("id"), true, identity, {{"status", "completed"}, {"conclusion", "success"}});
        graph::published(node,
                         attempt.at("id"),
                         {{"run", identity},
                          {"execution", node.at("id")},
                          {"attempt", attempt.at("id")},
                          {"assignment_sha256", coreci::digest(attempt.at("assignment"))},
                          {"source_commit", node.at("source").at("commit")},
                          {"feed", node.at("source").at("deployment").at("feed")},
                          {"feed_revision", std::string(40, 'f')},
                          {"publication_verified_utc", "1900-01-01T00:00:00Z"},
                          {"packages", {{{"file", "package.deb"}}}},
                          {"environment", {{"image", "debian:trixie"}}}});
        const auto receipt = attempt.at("receipt");
        check(receipt.at("publication_verified_utc") != "1900-01-01T00:00:00Z", "publisher supplied the controller verification time");
        graph::published(node, attempt.at("id"), receipt);
        check(attempt.at("receipt") == receipt, "identical receipt replay refreshed the verification time");
        auto changed = receipt;
        changed["feed_revision"] = std::string(40, '0');
        rejects(
            [&] {
                graph::published(node, attempt.at("id"), changed);
            },
            "conflicting committed receipt was accepted");
    }
} // namespace

int main(int argc, char* argv[]) {
    int result = 0;
    try {
        const auto selected = coreci::test::sources();
        auto state = graph::empty();
        const auto start = std::chrono::steady_clock::now();
        const auto created = graph::select(state, "core", selected, false);
        check(created.size() == 20, "full selection must contain all twenty targets");
        auto ready = graph::advance(state);
        check(ready.size() == 5, "all five independent roots must be assigned together");
        const auto firstId = ready.front().at("execution").get<std::string>();
        if (argc == 2) {
            std::ofstream(argv[1]) << state.at("executions").at(firstId).at("attempts").back().at("assignment").dump();
        }
        auto& nodes = state.at("executions");
        unsigned int runId = 1;
        for (const auto& dispatch : ready) {
            build(nodes.at(dispatch.at("execution").get<std::string>()), runId++);
        }
        ready = graph::advance(state);
        check(ready.size() == 5, "all five publishers must be admitted; GitHub owns the feed-write lock");
        check(graph::advance(state).empty(), "assigned mutations must not be replayed");
        for (const auto& dispatch : ready) {
            publish(nodes.at(dispatch.at("execution").get<std::string>()), runId++);
        }
        ready = graph::advance(state);
        check(ready.size() == 10, "all ten app and side targets must become eligible together");
        for (const auto& dispatch : ready) {
            auto& node = nodes.at(dispatch.at("execution").get<std::string>());
            check(node.at("source").at("name") != "join", "join ran before its prerequisites published");
            build(node, runId++);
        }
        ready = graph::advance(state);
        check(ready.size() == 10, "independent publishers were artificially limited");
        for (const auto& dispatch : ready) {
            auto& node = nodes.at(dispatch.at("execution").get<std::string>());
            if (node.at("source").at("name") == "app") {
                publish(node, runId++);
            }
        }
        check(graph::advance(state).empty(), "join must wait for side as well as app and core");
        state = Json::parse(state.dump());
        for (auto& [id, node] : state.at("executions").items()) {
            if (node.at("source").at("name") == "side") {
                publish(node, runId++);
            }
        }
        ready = graph::advance(state);
        check(ready.size() == 5, "restored graph failed to release every matching join target");
        for (const auto& dispatch : ready) {
            auto& node = state.at("executions").at(dispatch.at("execution").get<std::string>());
            check(node.at("attempts").back().at("assignment").at("dependencies").size() == 3, "join inputs were not exact");
            build(node, runId++);
        }
        ready = graph::advance(state);
        check(ready.size() == 5, "join publications missing");
        for (const auto& dispatch : ready) {
            publish(state.at("executions").at(dispatch.at("execution").get<std::string>()), runId++);
        }
        check(graph::advance(state).empty(), "completed graph dispatched replacement work");
        check(graph::select(state, "core", selected, false).empty(), "unchanged selection created work");
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Synthetic in-memory twenty-target graph: " << elapsed << " ms (no HTTP or database time)\n";

        auto& terminal = state.at("executions").at(firstId);
        const auto before = terminal;
        const auto& completed = terminal.at("attempts").back();
        graph::observe(terminal, completed.at("id"), false, completed.at("build_run"), {{"status", "queued"}});
        check(terminal == before, "delayed queued evidence regressed a terminal result");
        rejects(
            [&] {
                graph::observe(terminal, completed.at("id"), false, run(999), {{"status", "completed"}});
            },
            "conflicting run identity accepted");
        auto rerun = completed.at("build_run");
        rerun["run_attempt"] = 2;
        rejects(
            [&] {
                graph::observe(terminal, completed.at("id"), false, rerun, {{"status", "completed"}});
            },
            "rerun evidence accepted");
        rejects(
            [&] {
                graph::retry(state, firstId);
            },
            "published work was retried");

        auto broken = graph::empty();
        rejects(
            [&] {
                graph::select(broken, "app", selected, true);
            },
            "missing prerequisite must fail explicitly, not trigger an autobuild");
        auto cyclic = selected;
        cyclic["core"]["definition"]["depends_on"] = {"join"};
        rejects(
            [&] {
                graph::select(broken, "core", cyclic, false);
            },
            "dependency cycle accepted");

        auto oversized = selected;
        oversized["core"]["definition"]["title"] = std::string(70000, 'x');
        auto bounded = graph::empty();
        graph::select(bounded, "core", oversized, false);
        check(graph::advance(bounded).empty(), "oversized assignments must remain unassigned");
        for (const auto& [id, node] : bounded.at("executions").items()) {
            if (node.at("source").at("name") == "core") {
                check(!graph::blockers(bounded, node).empty(), "oversized work was silently dropped");
            }
        }
        const std::vector<std::string> versions{"1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-beta", "1.0.0", "2.0.0", "10.0.0"};
        for (std::size_t index = 1; index < versions.size(); ++index) {
            check(coreci::Version(versions[index - 1]).compare(coreci::Version(versions[index])) < 0, "numeric SemVer order changed");
        }
        check(coreci::Version("v2.1.3+first").compare(coreci::Version("2.1.3+other")) == 0, "build metadata changed precedence");
        rejects(
            [] {
                const coreci::Version invalid("1.0.0-01");
            },
            "invalid prerelease accepted");
        auto retries = graph::empty();
        graph::select(retries, "core", selected, true);
        auto roots = graph::advance(retries);
        for (const auto& work : roots) {
            build(retries.at("executions").at(work.at("execution").get<std::string>()), runId++);
        }
        for (const auto& work : graph::advance(retries)) {
            publish(retries.at("executions").at(work.at("execution").get<std::string>()), runId++);
        }
        const auto dependents = graph::advance(retries);
        const auto retryId = dependents.front().at("execution").get<std::string>();
        auto& retryNode = retries.at("executions").at(retryId);
        const auto oldAttempt = retryNode.at("attempts").back().at("id").get<std::string>();
        const auto oldAssignment = retryNode.at("attempts").back().at("assignment");
        const auto oldRun = run(runId++);
        graph::observe(retryNode, oldAttempt, false, oldRun, {{"status", "completed"}, {"conclusion", "failure"}});
        graph::failed(retryNode, oldAttempt, "verified build failure");
        auto upgraded = selected;
        upgraded["core"]["commit"] = std::string(40, 'e');
        upgraded["core"]["tag"] = "v9.0.0";
        upgraded["core"]["definition_revision"] = std::string(40, 'e');
        for (auto& [name, source] : upgraded.items()) {
            source["deployment"]["worker"]["tooling_sha"] = std::string(40, 'f');
        }
        graph::select(retries, "core", upgraded, true);
        graph::retry(retries, retryId);
        graph::advance(retries);
        const auto& retried = retries.at("executions").at(retryId).at("attempts").back();
        check(retried.at("assignment").at("dependencies") == oldAssignment.at("dependencies"),
              "retry changed consumed artifact identities");
        check(retried.at("assignment").at("source").at("deployment") == oldAssignment.at("source").at("deployment"),
              "historical retry adopted new tooling");
        check(retried.at("assignment").at("environment_from").at(0).at("run") == oldRun &&
                  retried.at("assignment").at("environment_from").at(0).at("assignment_sha256") == coreci::digest(oldAssignment),
              "retry lost pinned environment provenance");
        check(retried.at("revision") > retryNode.at("attempts").front().at("revision") && retryNode.at("attempts").size() == 2,
              "retry reused its revision or discarded history");
        rejects(
            [&] {
                graph::failed(retryNode, oldAttempt, "late failure");
            },
            "late failure changed a replacement attempt");
        const auto currentBefore = retryNode.at("attempts").back();
        graph::observe(retryNode, oldAttempt, false, oldRun, {{"completed_at", "2026-10-27T10:00:00Z"}});
        check(retryNode.at("attempts").back() == currentBefore, "historical evidence changed the current retry");

        auto supersession = graph::empty();
        graph::select(supersession, "core", selected, false);
        const auto firstRoots = graph::advance(supersession);
        const auto frozen = supersession.at("executions").at(firstRoots.front().at("execution").get<std::string>());
        graph::select(supersession, "core", upgraded, false);
        check(supersession.at("executions").at(frozen.at("id").get<std::string>()) == frozen, "new selection rewrote assigned work");
        unsigned int superseded = 0;
        for (const auto& [id, node] : supersession.at("executions").items()) {
            if (node.at("superseded_by") != "") {
                ++superseded;
                check(node.at("automatic") && node.at("attempts").back().at("state") == "waiting", "supersession affected nonwaiting work");
            }
        }
        check(superseded == 15, "waiting automatic dependents were not superseded");
        const auto publishedBefore = state;
        auto conflictingNamespace = selected;
        conflictingNamespace["core"]["definition"]["package"] = "renamed-core";
        conflictingNamespace["app"]["definition"]["package"] = "core";
        graph::validateSources(conflictingNamespace);
        rejects(
            [&] {
                graph::validateSources(conflictingNamespace, state.at("executions"));
            },
            "selected history released its package namespace");
        graph::select(state, "core", upgraded, false);
        for (const auto& [id, node] : publishedBefore.at("executions").items()) {
            check(state.at("executions").at(id) == node, "new source selection changed retained publications");
        }

        auto quoted = selected;
        quoted["core"]["deployment"]["padding"] = std::string(18000, '"');
        auto smallSource = selected.at("core");
        smallSource["name"] = "small";
        smallSource["repository"] = "Owner/small";
        smallSource["repository_id"] = 99;
        smallSource["definition"]["package"] = "small";
        quoted["small"] = smallSource;
        auto mixed = graph::empty();
        graph::select(mixed, "core", quoted, false);
        graph::select(mixed, "small", quoted, false);
        check(graph::advance(mixed).size() == 5, "oversized full dispatch payload blocked independent roots");
        for (const auto& [id, node] : mixed.at("executions").items()) {
            if (node.at("source").at("name") == "core") {
                check(node.at("attempts").back().at("state") == "waiting" && !graph::blockers(mixed, node).empty(),
                      "oversized node was assigned or hidden");
            }
        }
        auto publicationBound = graph::empty();
        graph::select(publicationBound, "core", selected, false);
        roots = graph::advance(publicationBound);
        auto& large = publicationBound.at("executions").at(roots.at(0).at("execution").get<std::string>());
        auto& largeAttempt = large.at("attempts").back();
        const auto largeRun = run(runId++);
        graph::observe(large, largeAttempt.at("id"), false, largeRun, {{"status", "completed"}, {"conclusion", "success"}});
        graph::built(large,
                     largeAttempt.at("id"),
                     {{"run", largeRun},
                      {"assignment_sha256", coreci::digest(largeAttempt.at("assignment"))},
                      {"artifacts", {{{"id", 100}, {"padding", std::string(65536, 'x')}}}}});
        build(publicationBound.at("executions").at(roots.at(1).at("execution").get<std::string>()), runId++);
        check(graph::advance(publicationBound).size() == 1 && largeAttempt.at("state") == "building" &&
                  !graph::blockers(publicationBound, large).empty(),
              "oversized publication prevented another publisher or acquired authority");
        std::cout << "PASS graph traversal and state boundaries\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        result = 1;
    }
    return result;
}
