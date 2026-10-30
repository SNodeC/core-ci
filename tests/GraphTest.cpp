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
    void retentionBoundaries() {
        auto sources = coreci::test::sources();
        sources.erase("side");
        sources.erase("join");
        for (auto& source : sources) {
            source["definition"]["matrix"] = {source.at("definition").at("matrix").front()};
        }
        auto state = graph::empty(), held = graph::empty();
        const auto complete = [&](Json& state, const std::string& tag, char commit) {
            sources["core"]["tag"] = tag;
            sources["core"]["commit"] = std::string(40, commit);
            const auto created = graph::select(state, "core", sources, true, state.at("next_revision"));
            const auto id = created.front().get<std::string>();
            graph::advance(state);
            auto& node = state["executions"][id];
            build(node, 100 + state.at("next_revision").get<unsigned int>());
            graph::advance(state);
            publish(node, 200 + state.at("next_revision").get<unsigned int>());
            auto& receipt = node["attempts"].back()["receipt"];
            receipt["packages"] = {{{"path", "debian/pool/trixie/core_" + node.at("attempts").back().at("revision").dump() + "_amd64.deb"},
                                    {"size", 100},
                                    {"sha256", std::string(64, 'a')}}};
            receipt["development_export"] = {
                {"path", "exports/" + id + "/1/development.tar.gz"}, {"size", 20}, {"sha256", std::string(64, 'b')}};
            return id;
        };
        std::vector<std::string> ids;
        std::string consumer;
        unsigned int version = 0;
        for (const auto* tag : {"v1.0.0", "v1.1.0-rc.1", "v1.1.0", "v2.0.0"}) {
            ids.push_back(complete(state, tag, 'a' + version));
            complete(held, tag, 'a' + version);
            if (version++ == 0) {
                consumer = graph::advance(held).front().at("execution");
            }
        }
        const auto deployment = sources.at("core").at("deployment");
        const auto oldest = state.at("executions").at(ids.front());
        check(graph::retain(held, deployment).empty(), "active exact inputs were withdrawn");
        auto retired = held;
        build(retired["executions"][consumer], 750);
        graph::advance(retired);
        graph::retire(retired, consumer, retired["executions"][consumer]["attempts"].back()["id"]);
        check(graph::retain(retired, deployment).empty(), "retired unknown publisher lost its prerequisite protection");
        auto retryFirst = held, expiryFirst = held;
        const auto attempt = held.at("executions").at(consumer).at("attempts").back().at("id");
        graph::failed(retryFirst["executions"][consumer], attempt, "verified build failure");
        graph::failed(expiryFirst["executions"][consumer], attempt, "verified build failure");
        graph::retry(retryFirst, consumer);
        graph::advance(retryFirst);
        check(graph::retain(retryFirst, deployment).empty(), "retry before withdrawal lost its frozen inputs");
        check(graph::retain(expiryFirst, deployment).size() == 1, "failed history protected inputs forever");
        rejects(
            [&] {
                graph::retry(expiryFirst, consumer);
            },
            "expired retry was rebound or accepted");
        const auto commands = graph::retain(state, deployment);
        check(commands.size() == 1, "retention did not derive a single-target cleanup");
        const auto& expired = state.at("executions").at(ids.front()).at("attempts").back();
        check(expired.contains("expired_by") && expired.at("state") == "published" &&
                  expired.at("receipt") == oldest.at("attempts").back().at("receipt"),
              "retention rewrote the verified publication outcome or receipt");
        for (std::size_t index = 1; index < ids.size(); ++index) {
            check(!state.at("executions").at(ids[index]).at("attempts").back().contains("expired_by"), "SemVer roots were not retained");
        }
        const auto& request = graph::work(state, commands.front().at("execution")).at("attempts").back().at("assignment");
        check(request.at("objects").size() == 3 && request.at("source").at("deployment") == deployment && !request.contains("dependencies"),
              "cleanup lost its independent authority or exact removal set");
        const auto before = state;
        check(graph::retain(state, deployment).empty() && state == before, "unchanged retention replayed cleanup");
        auto emptyOnly = before;
        const auto cleanupId = commands.front().at("execution").get<std::string>();
        graph::work(emptyOnly, cleanupId)["attempts"].back()["state"] = "published";
        emptyOnly["executions"][ids.front()]["attempts"].back().erase("expired_by");
        const auto revision = emptyOnly.at("next_revision");
        check(graph::retain(emptyOnly, deployment).empty() && emptyOnly.at("next_revision") == revision &&
                  emptyOnly.at("retention").size() == before.at("retention").size() &&
                  emptyOnly.at("executions").at(ids.front()).at("attempts").back().at("expired_by") == "retention policy",
              "graph-only expiry consumed a revision or created cleanup");
        const auto unchanged = emptyOnly;
        check(graph::retain(emptyOnly, deployment).empty() && emptyOnly == unchanged, "graph-only expiry repeated");
        check(graph::blockers(emptyOnly, emptyOnly.at("executions").at(ids.front())).front() == "inputs expired by retention",
              "graph-only expiry lost its blocker");
        auto recovery = before;
        auto& cleanupNode = graph::work(recovery, cleanupId);
        graph::failed(cleanupNode, cleanupNode.at("attempts").back().at("id"), "verified failure before push");
        const auto failedAttempt = cleanupNode.at("attempts").back();
        const auto retryCommands = graph::retry(recovery, cleanupId);
        const auto& renewed = cleanupNode.at("attempts").back();
        auto expectedAssignment = failedAttempt.at("assignment");
        expectedAssignment["attempt"] = renewed.at("id");
        expectedAssignment["revision"] = renewed.at("revision");
        expectedAssignment["label"] = "retention " + cleanupId + " | r" + renewed.at("revision").dump();
        check(retryCommands.size() == 1 && retryCommands.front().at("attempt") == renewed.at("id") &&
                  retryCommands.front().at("publication") == true && renewed.at("assignment") == expectedAssignment &&
                  renewed.at("state") == "publishing" && renewed.at("evidence").empty() && renewed.at("publication_run").is_null() &&
                  !renewed.contains("preflight") && !renewed.contains("dispatch") && cleanupNode.at("attempts").front() == failedAttempt,
              "explicit cleanup retry lost history, pins or exact authority");
        check(graph::retain(recovery, deployment).empty() &&
                  recovery.at("executions").at(ids.front()).at("attempts").back().at("expired_by") == cleanupId,
              "cleanup retry released claims or rewrote expiry");
        rejects(
            [&] {
                graph::retry(recovery, cleanupId);
            },
            "active cleanup retry accepted");
        auto oversized = before;
        auto& invalid = graph::work(oversized, cleanupId);
        graph::failed(invalid, invalid.at("attempts").back().at("id"), "verified failure before push");
        invalid["attempts"].back()["assignment"]["objects"][0]["path"] = std::string(60000, 'x');
        rejects(
            [&] {
                graph::retry(oversized, cleanupId);
            },
            "oversized cleanup retry accepted");
        auto& dependent = held["executions"][consumer];
        build(dependent, 800);
        graph::advance(held);
        publish(dependent, 900);
        auto& receipt = dependent["attempts"].back()["receipt"];
        receipt["packages"] = {{{"path", "debian/pool/trixie/app_1_amd64.deb"}, {"size", 100}, {"sha256", std::string(64, 'c')}}};
        receipt["development_export"] = nullptr;
        const auto cleanup = graph::retain(held, deployment);
        const auto objects = graph::work(held, cleanup.front().at("execution")).at("attempts").back().at("assignment").at("objects");
        check(objects.size() == 1 &&
                  objects.front().at("path") == oldest.at("attempts").back().at("receipt").at("development_export").at("path"),
              "runtime closure lost package bytes or retained audit-only exports");
        // Highest revision wins equal SemVer precedence; a failed replacement is never a successful root.
        auto revisions = before;
        graph::work(revisions, commands.front().at("execution"))["attempts"].back()["state"] = "published";
        for (const auto& work : graph::advance(revisions)) {
            graph::failed(graph::work(revisions, work.at("execution")), work.at("attempt"), "verified build failure");
        }
        const auto replacement = complete(revisions, "v2.0.0+build.2", 'e');
        check(graph::retain(revisions, deployment).size() == 1 &&
                  revisions.at("executions").at(ids.back()).at("attempts").back().contains("expired_by"),
              "highest successful rebuild did not replace the lower revision");
        sources["core"]["tag"] = "v9.0.0";
        const auto failed = graph::select(revisions, "core", sources, false, revisions.at("next_revision")).front().get<std::string>();
        graph::advance(revisions);
        graph::failed(revisions["executions"][failed], revisions["executions"][failed]["attempts"].back()["id"], "verified build failure");
        graph::retain(revisions, deployment);
        check(!revisions.at("executions").at(replacement).at("attempts").back().contains("expired_by"),
              "failed source displaced working publication");
        auto batch = before;
        batch.erase("retention");
        batch["executions"][ids.front()] = oldest;
        auto& components = batch["executions"][ids.front()]["attempts"].back()["receipt"]["packages"];
        auto component = components.front();
        for (unsigned int index = 1; index < 128; ++index) {
            component["path"] = "debian/pool/trixie/" + std::string(420, 'x') + std::to_string(index) + "_amd64.deb";
            components.push_back(component);
        }
        const auto first = graph::retain(batch, deployment);
        check(first.size() == 1 && graph::inputs(graph::work(batch, first.front().at("execution")), true).dump().size() <= 65535,
              "cleanup batch exceeded dispatch bounds");
        graph::work(batch, first.front().at("execution"))["attempts"].back()["state"] = "published";
        check(graph::retain(batch, deployment).size() == 1, "completion did not derive the remaining bounded batch");
    }

    void selectionBoundaries() {
        auto original = coreci::test::sources();
        for (auto& source : original) {
            source["tag"] = "v1.0.0";
        }
        original["join"]["definition"]["depends_on"] = Json::array({"app"});
        const auto changed = [](Json sources, const std::string& name, const std::string& tag, char commit) {
            sources[name]["tag"] = tag;
            for (const auto* key : {"commit", "definition_revision", "tag_object_sha"}) {
                sources[name][key] = std::string(40, commit);
            }
            return sources;
        };
        const auto higher = changed(original, "core", "v2.0.0", '2');
        for (const bool explicitRequest : {false, true}) {
            auto state = graph::empty();
            const auto newest = graph::select(state, "core", higher, false, 1);
            graph::select(state, "core", original, explicitRequest, state.at("next_revision"));
            for (const auto& id : newest) {
                check(state.at("executions").at(id.get<std::string>()).at("superseded_by") == "",
                      "older root or transitive input superseded a newer automatic selection");
            }
            const auto laterJoin = changed(original, "join", "v2.0.0", '3');
            graph::select(state, "join", laterJoin, false, state.at("next_revision"));
            for (const auto& id : newest) {
                check(state.at("executions").at(id.get<std::string>()).at("superseded_by") == "",
                      "later automatic dependent superseded a transitively newer input");
            }
        }
        auto progressing = graph::empty();
        const auto older = graph::select(progressing, "core", original, false, 1);
        graph::select(progressing, "core", higher, false, progressing.at("next_revision"));
        for (const auto& id : older) {
            check(progressing.at("executions").at(id.get<std::string>()).at("superseded_by") != "",
                  "newer transitive inputs did not supersede older waiting automatic work");
        }
        auto removal = graph::empty();
        const auto retained = graph::select(removal, "core", original, false, 1);
        auto independent = changed(original, "app", "v2.0.0", '2');
        independent["app"]["definition"]["depends_on"] = Json::array();
        graph::select(removal, "app", independent, false, removal.at("next_revision"));
        for (const auto& id : retained) {
            const auto& node = removal.at("executions").at(id.get<std::string>());
            if (node.at("source").at("name") == "app" || node.at("source").at("name") == "join") {
                check(node.at("superseded_by") != "", "legitimate dependency removal prevented supersession");
            }
        }
        auto moved = graph::empty();
        const auto prior = graph::select(moved, "core", original, false, 1);
        const auto sameVersion = changed(original, "core", "v1.0.0", '2');
        graph::select(moved, "core", sameVersion, false, moved.at("next_revision"));
        for (const auto& id : prior) {
            check(moved.at("executions").at(id.get<std::string>()).at("superseded_by") == "",
                  "incomparable same-version source identity superseded retained work");
        }
        auto competing = graph::empty();
        graph::select(competing, "core", original, false, 1);
        const auto since = competing.at("next_revision").get<std::uint64_t>();
        const auto newApp = changed(original, "app", "v2.0.0", '2');
        graph::select(competing, "app", newApp, false, since);
        const auto before = competing;
        rejects(
            [&] {
                graph::select(competing, "core", original, false, since);
            },
            "a newly committed higher non-root source did not reject stale selection");
        check(competing == before, "stale selection changed graph before rejection");
        graph::select(competing, "core", original, false, competing.at("next_revision"));
        auto explicitOlder = graph::empty();
        graph::select(explicitOlder, "core", higher, false, 1);
        check(graph::select(explicitOlder, "core", original, true, 1).size() == 20,
              "intentional explicit older root was rejected as stale");
        auto concurrentMove = graph::empty();
        graph::select(concurrentMove, "core", sameVersion, false, 1);
        rejects(
            [&] {
                graph::select(concurrentMove, "core", original, true, 1);
            },
            "explicit root bypassed same-version identity conflict after reads began");
        auto evidence = graph::empty();
        graph::select(evidence, "core", original, false, 1);
        const auto assigned = graph::advance(evidence);
        auto& node = evidence.at("executions").at(assigned.front().at("execution").get<std::string>());
        const auto began = evidence.at("next_revision").get<std::uint64_t>();
        graph::observe(
            node, node.at("attempts").back().at("id"), false, run(99), {{"job_status", "queued"}, {"started_at", "2026-10-27T09:59:58Z"}});
        auto& jobEvidence = node.at("attempts").back().at("evidence").at("build");
        check(!jobEvidence.contains("started_at"), "queued placeholder was recorded as actual job start");
        jobEvidence["started_at"] = "2026-10-27T09:59:58Z";
        graph::observe(node,
                       node.at("attempts").back().at("id"),
                       false,
                       run(99),
                       {{"job_status", "in_progress"}, {"started_at", "2026-10-27T10:00:00Z"}});
        check(jobEvidence.at("started_at") == "2026-10-27T10:00:00Z", "restored queued placeholder prevented actual job start");
        rejects(
            [&] {
                graph::observe(node,
                               node.at("attempts").back().at("id"),
                               false,
                               run(99),
                               {{"job_status", "in_progress"}, {"started_at", "2026-10-27T10:01:00Z"}});
            },
            "conflicting immutable job timestamp was silently replaced");
        auto restored = node;
        restored.at("attempts").back()["evidence"]["build"] = {{"job_status", "queued"}, {"started_at", "2026-10-27T09:59:58Z"}};
        graph::observe(restored,
                       restored.at("attempts").back().at("id"),
                       false,
                       run(99),
                       {{"job_status", "completed"}, {"started_at", "2026-10-27T10:00:00Z"}});
        check(restored.at("attempts").back().at("evidence").at("build").at("started_at") == "2026-10-27T10:00:00Z",
              "completed-only report did not recover restored queued evidence");
        const auto completedOnly = restored;
        graph::observe(restored,
                       restored.at("attempts").back().at("id"),
                       false,
                       run(99),
                       {{"job_status", "queued"}, {"started_at", "2026-10-27T09:59:58Z"}});
        check(restored == completedOnly, "delayed queued timestamp regressed completed evidence");
        graph::failed(node, node.at("attempts").back().at("id"), "verified test failure");
        graph::retry(evidence, node.at("id"));
        check(graph::select(evidence, "core", original, false, began).empty(), "evidence or retry invalidated source selection");
    }
} // namespace

int main(int argc, char* argv[]) {
    int result = 0;
    try {
        selectionBoundaries();
        retentionBoundaries();
        const auto selected = coreci::test::sources();
        auto state = graph::empty();
        const auto start = std::chrono::steady_clock::now();
        const auto created = graph::select(state, "core", selected, false, state.at("next_revision").get<std::uint64_t>());
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
        check(graph::select(state, "core", selected, false, state.at("next_revision").get<std::uint64_t>()).empty(),
              "unchanged selection created work");
        auto indexHeavy = state;
        auto indexes = Json::array();
        for (unsigned int index = 0; index < 100; ++index) {
            indexes.push_back(
                {{"path", "rpm/fedora/43/x86_64/repodata/" + std::string(120, 'a') + std::to_string(index) + "-primary.xml.gz"},
                 {"sha256", std::string(64, 'b')},
                 {"size_bytes", 1024}});
        }
        for (auto& node : indexHeavy.at("executions")) {
            node.at("attempts").back().at("receipt")["indexes"] = indexes;
        }
        const auto retained = indexHeavy.at("executions");
        auto nextJoin = selected;
        nextJoin["join"]["tag"] = "v2.1.4";
        nextJoin["join"]["commit"] = std::string(40, '9');
        graph::select(indexHeavy, "join", nextJoin, false, indexHeavy.at("next_revision"));
        const auto joins = graph::advance(indexHeavy);
        check(joins.size() == 5, "unused repository metadata blocked ready joins");
        for (const auto& work : joins) {
            const auto& node = indexHeavy.at("executions").at(work.at("execution").get<std::string>());
            const auto& assigned = node.at("attempts").back().at("assignment");
            const auto payload = graph::inputs(node, false);
            check(assigned.dump().size() < 60000 && payload.dump().size() <= 65535 &&
                      payload.at("assignment_sha256") == coreci::digest(assigned),
                  "compact dispatch lost its bound or digest");
            for (const auto& dependency : assigned.at("dependencies")) {
                const auto& original = retained.at(dependency.at("execution").get<std::string>()).at("attempts").back();
                auto consumed = original.at("receipt");
                consumed.erase("indexes");
                check(dependency.at("receipt") == consumed && dependency.at("attempt") == original.at("id"),
                      "compact prerequisite changed exact consumption facts");
                check(indexHeavy.at("executions").at(dependency.at("execution").get<std::string>()).at("attempts").back().at("receipt") ==
                          original.at("receipt"),
                      "assignment projection mutated the retained receipt");
            }
        }
        check(graph::advance(indexHeavy).empty(), "compact join assignment was dispatched twice");
        auto& assignedJoin = indexHeavy.at("executions").at(joins.front().at("execution").get<std::string>());
        assignedJoin.at("attempts").back().at("assignment")["dependencies"][0]["receipt"]["indexes"] = Json::array({indexes.front()});
        const auto committedAssignment = assignedJoin.at("attempts").back().at("assignment");
        check(graph::advance(indexHeavy).empty() && assignedJoin.at("attempts").back().at("assignment") == committedAssignment,
              "existing full assignment was rewritten");
        graph::failed(assignedJoin, assignedJoin.at("attempts").back().at("id"), "definite test failure");
        graph::retry(indexHeavy, assignedJoin.at("id"));
        check(graph::advance(indexHeavy).size() == 1 &&
                  assignedJoin.at("attempts").back().at("assignment").at("dependencies") == committedAssignment.at("dependencies"),
              "retry changed its original prerequisite inputs");
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
                graph::select(broken, "app", selected, true, broken.at("next_revision").get<std::uint64_t>());
            },
            "missing prerequisite must fail explicitly, not trigger an autobuild");
        auto cyclic = selected;
        cyclic["core"]["definition"]["depends_on"] = {"join"};
        rejects(
            [&] {
                graph::select(broken, "core", cyclic, false, broken.at("next_revision").get<std::uint64_t>());
            },
            "dependency cycle accepted");

        auto oversized = selected;
        oversized["core"]["definition"]["title"] = std::string(70000, 'x');
        auto bounded = graph::empty();
        graph::select(bounded, "core", oversized, false, bounded.at("next_revision").get<std::uint64_t>());
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
        graph::select(retries, "core", selected, true, retries.at("next_revision").get<std::uint64_t>());
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
        graph::select(retries, "core", upgraded, true, retries.at("next_revision").get<std::uint64_t>());
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
        graph::select(supersession, "core", selected, false, supersession.at("next_revision").get<std::uint64_t>());
        const auto firstRoots = graph::advance(supersession);
        const auto frozen = supersession.at("executions").at(firstRoots.front().at("execution").get<std::string>());
        graph::select(supersession, "core", upgraded, false, supersession.at("next_revision").get<std::uint64_t>());
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
        graph::select(state, "core", upgraded, false, state.at("next_revision").get<std::uint64_t>());
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
        graph::select(mixed, "core", quoted, false, mixed.at("next_revision").get<std::uint64_t>());
        graph::select(mixed, "small", quoted, false, mixed.at("next_revision").get<std::uint64_t>());
        check(graph::advance(mixed).size() == 5, "oversized full dispatch payload blocked independent roots");
        for (const auto& [id, node] : mixed.at("executions").items()) {
            if (node.at("source").at("name") == "core") {
                check(node.at("attempts").back().at("state") == "waiting" && !graph::blockers(mixed, node).empty(),
                      "oversized node was assigned or hidden");
            }
        }
        auto publicationBound = graph::empty();
        graph::select(publicationBound, "core", selected, false, publicationBound.at("next_revision").get<std::uint64_t>());
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
        auto retiring = graph::empty();
        graph::select(retiring, "core", selected, false, 1);
        const auto retiringReady = graph::advance(retiring);
        const auto retiringId = retiringReady.at(0).at("execution").get<std::string>();
        auto& retiringNode = retiring.at("executions").at(retiringId);
        check(!graph::retirable(retiringNode), "building execution offered retirement");
        rejects(
            [&] {
                graph::retire(retiring, retiringId, retiringNode.at("attempts").back().at("id"));
            },
            "building execution retired");
        build(retiringNode, 800);
        graph::advance(retiring);
        const auto unconfirmed = retiringNode;
        const auto attemptId = retiringNode.at("attempts").back().at("id").get<std::string>();
        for (const auto& field : {"dispatch", "evidence", "publication_run"}) {
            retiringNode = unconfirmed;
            auto& attempt = retiringNode.at("attempts").back();
            if (std::string(field) == "publication_run") {
                attempt[field] = run(900);
            } else {
                attempt[field]["publication"] = std::string(field) == "dispatch" ? Json(900) : Json{{"status", "queued"}};
            }
            rejects(
                [&] {
                    graph::retire(retiring, retiringId, attemptId);
                },
                "retirement accepted recorded publication facts");
            check(!graph::retirable(retiringNode), "unsafe retirement capability exposed");
        }
        retiringNode = unconfirmed;
        graph::retire(retiring, retiringId, attemptId);
        check(retiringNode.at("attempts").back().at("state") == "failed" && !graph::retirable(retiringNode), "retirement failed");
        std::cout << "PASS graph traversal and state boundaries\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        result = 1;
    }
    return result;
}
