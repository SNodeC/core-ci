#include "fixtures.h"

#include <fstream>
#include <iostream>

namespace {

    void rejects(const std::function<void()>& operation, const std::string& message) {
        bool rejected = false;
        try {
            operation();
        } catch (const std::exception&) {
            rejected = true;
        }
        ci::require(rejected, message);
    }

    void publicationGuardFailure() {
        using namespace fixture;
        Json selected = fixture::graph();
        ci::selectRequest(selected, {{"Fixture/base", fixture::selected("base", Json::array())}}, "Fixture/base");
        const std::string id = selected.at("executions").at(0).at("id");
        finishBuild(selected, id);
        for (const std::string push : {"absent", "skipped", "failure", "success"}) {
            Json graph = selected;
            const Json original = work(graph, id);
            Json observed = observation(original, "publish", "completed", "failure");
            if (push != "absent") {
                observed["jobs"][0]["steps"] =
                    Json::array({{{"number", 1},
                                  {"name", "Require validated publication inputs"},
                                  {"status", "completed"},
                                  {"conclusion", push == "skipped" ? "failure" : "skipped"}},
                                 {{"number", 2}, {"name", "Publish checked feed update"}, {"status", "completed"}, {"conclusion", push}}});
            }
            ci::require(ci::applyEvidence(graph, original, "publish", observed, {}, "").empty(),
                        "Publication failures authorize no dependents");
            const auto& active = ci::current(work(graph, id));
            if (push == "skipped") {
                ci::require(active.at("state") == "failed" && active.at("failure_kind") == "definite",
                            "Input guard failure with skipped push is definite and retryable");
                ci::retry(graph, id);
                ci::require(work(graph, id).at("attempts").size() == 2, "Explicit retry preserves the failed publication attempt");
            } else {
                ci::require(active.at("state") == "publishing", "Absent, failed or successful push preserves publication uncertainty");
            }
        }
    }

    void observationUncertainty() {
        using namespace fixture;
        Json graph = fixture::graph();
        const Json selection = {{"Fixture/base", selected("base", Json::array())},
                                {"Fixture/client", selected("client", Json::array({"base"}))}};
        ci::selectRequest(graph, selection, "Fixture/base");
        const auto id = graph.at("executions").at(0).at("id").get<std::string>();
        const Json original = work(graph, id);
        auto& active = ci::current(work(graph, id));
        active["facts"]["build_send"] = {{"outcome", "accepted"}};
        const auto assignment = active.at("assignment");
        ci::require(ci::applyEvidence(graph, original, "build", nullptr, {}, "read F").empty(), "Early errors authorize nothing");
        ci::require(active.at("uncertainty").at("kind") == "read" && active.at("uncertainty").at("stage") == "build" &&
                        active.at("state") == "building" && active.at("assignment") == assignment &&
                        !active.at("facts").contains("build_run") && active.at("facts").at("build_send").at("outcome") == "accepted",
                    "Attempt-id guard accepts dispatch acknowledgement without fabricating run facts");
        const auto duplicate = graph.dump();
        ci::applyEvidence(graph, original, "build", nullptr, {}, "read F");
        ci::require(graph.dump() == duplicate, "Identical read failures preserve the entire object and first-recorded at");
        const auto running = observation(original, "build", "in_progress", "");
        ci::applyEvidence(graph, original, "build", running, {}, "");
        ci::require(!active.contains("uncertainty"), "A successful read clears read-level uncertainty");
        active["uncertainty"] = {{"at", "2026-01-01T00:00:00Z"}, {"stage", "build"}, {"kind", "read"}, {"reason", "same reason"}};
        ci::applyEvidence(graph, original, "build", running, {}, "same reason");
        ci::require(active.at("uncertainty").at("kind") == "evidence" && active.at("uncertainty").at("at") == "2026-01-01T00:00:00Z",
                    "Strengthening the clearance level retains the first-recorded time for an unchanged reason");
        ci::applyEvidence(graph, original, "build", running, {}, "evidence E");
        ci::require(active.at("uncertainty").at("kind") == "evidence", "Validated-run evidence error needs evidence clearance");
        ci::applyEvidence(graph, original, "build", nullptr, {}, "read F");
        const auto evidence = active.at("uncertainty");
        ci::require(evidence.at("kind") == "evidence" && evidence.at("reason") == "read F", "Read failure cannot downgrade evidence");
        ci::applyEvidence(graph, original, "build", running, {}, "");
        ci::require(active.at("uncertainty") == evidence, "E -> F -> S retains evidence-level uncertainty carrying F's reason");
        active["uncertainty"] = {{"at", "2026-01-01T00:00:00Z"}, {"reason", "legacy E"}};
        const auto legacy = active.at("uncertainty").dump();
        ci::applyEvidence(graph, original, "build", nullptr, {}, "legacy E");
        ci::require(active.at("uncertainty").dump() == legacy, "Identical normalized legacy context stays byte-identical");
        ci::applyEvidence(graph, original, "build", running, {}, "");
        ci::require(active.at("uncertainty").dump() == legacy, "A status read cannot clear kindless/stageless legacy evidence");
        ci::applyEvidence(graph, original, "build", nullptr, {}, "new F");
        ci::require(active.at("uncertainty").at("kind") == "evidence" && active.at("uncertainty").at("stage") == "build",
                    "A changed legacy reason writes explicit clearance level and stage");
        const auto changed = graph.dump();
        ci::applyEvidence(graph, original, "build", nullptr, {}, "new F");
        ci::applyEvidence(graph, original, "build", running, {}, "");
        ci::require(graph.dump() == changed, "E -> F -> F -> S preserves evidence uncertainty without touching at");
        finishBuild(graph, id);
        ci::require(!active.contains("uncertainty") && active.at("state") == "publishing", "Validated build completion clears evidence");
        auto before = graph.dump();
        ci::applyEvidence(graph, original, "build", nullptr, {}, "late build");
        ci::require(graph.dump() == before, "A late build read failure during Publishing is ignored");
        const auto publishing = work(graph, id);
        ci::applyEvidence(graph, publishing, "publish", nullptr, {}, "publication read");
        ci::require(active.at("uncertainty").at("kind") == "read" && active.at("uncertainty").at("stage") == "publish",
                    "Publication reads during Publishing are recorded for that stage");
        active["uncertainty"]["stage"] = "build";
        active["uncertainty"]["kind"] = "evidence";
        ci::applyEvidence(graph, publishing, "publish", nullptr, {}, "new-stage read");
        ci::require(active.at("uncertainty").at("kind") == "read", "An old-stage entry does not strengthen a new-stage read error");
        for (const std::string state : {"built", "failed", "published"}) {
            Json terminal = graph;
            ci::current(work(terminal, id))["state"] = state;
            const auto saved = terminal.dump();
            ci::applyEvidence(terminal, publishing, "publish", nullptr, {}, "late read");
            ci::require(terminal.dump() == saved, "Nonawaiting/terminal work cannot gain or clear diagnostics");
        }
        Json retired = graph;
        ci::retire(retired, id, "Operator acknowledges the unknown publication outcome");
        const auto history = ci::current(work(retired, id));
        ci::applyEvidence(retired, publishing, "publish", nullptr, {}, "late retired read");
        ci::require(ci::current(work(retired, id)) == history && history.contains("uncertainty"), "Retirement preserves uncertainty");
        rejects(
            [&]() {
                ci::retry(retired, id);
            },
            "Retired work is not eligible for retry");
        rejects(
            [&]() {
                ci::retry(graph, id);
            },
            "Retry is only from definite Failed");
        active["state"] = "failed";
        active["failure_kind"] = "definite";
        const auto failed = active;
        ci::retry(graph, id);
        const auto retried = graph.dump();
        ci::applyEvidence(graph, publishing, "publish", nullptr, {}, "old attempt read");
        ci::applyEvidence(graph, publishing, "publish", observation(publishing, "publish"), {}, "old evidence");
        ci::require(graph.dump() == retried && work(graph, id).at("attempts").at(0) == failed,
                    "Retry creates a new attempt; old failures and their uncertainty remain unchanged");
    }

    void compactProjection() {
        using ci::Json;
        Json selected = {{"Fixture/base", fixture::selected("base", Json::array())},
                         {"Fixture/client", fixture::selected("client", Json::array({"base"}))},
                         {"Fixture/tool", fixture::selected("tool", Json::array({"base"}))},
                         {"Fixture/bundle", fixture::selected("bundle", Json::array({"base", "client", "tool"}))}};
        for (auto& entry : selected) {
            for (int i = 1; i < 5; ++i) {
                auto target = fixture::target();
                target["id"] = "target-" + std::to_string(i);
                target["suite"] = "suite-" + std::to_string(i);
                entry["definition"]["matrix"].push_back(target);
            }
        }
        Json graph = fixture::graph();
        ci::selectRequest(graph, selected, "Fixture/base");
        Json projected = Json::object();
        std::size_t largest = 0;
        for (std::size_t i = 0; i < graph.at("executions").size(); ++i) {
            const auto& work = graph.at("executions").at(i);
            const auto row = ci::projectWork(work, i);
            ci::require(row.at("created") == work.at("attempts").at(0).at("created") &&
                            row.at("prerequisites") == work.at("prerequisites") && row.at("index") == i,
                        "Projection preserves ordering, identity and exact prerequisites");
            ci::require(!row.contains("definition") && !row.at("source").contains("deployment") && !row.at("source").contains("commit") &&
                            !row.at("active").contains("assignment"),
                        "Definitions, internal pins and assignments never enter execution records");
            ci::require(row.at("active").at("uncertainty").is_null() && !row.at("active").contains("proof"),
                        "Absent optional facts remain absent/null");
            projected[work.at("id").get<std::string>()] = row;
            largest = std::max(largest, row.dump().size());
        }
        const auto patch = Json::diff(Json::object(), projected, "/executions");
        ci::require(patch.size() == 20 && patch.dump().size() + 28 <= 49152,
                    "Twenty-record chain selection has framing headroom below 48 KiB");
        Json work = graph.at("executions").at(0);
        auto& active = work["attempts"].back();
        active["uncertainty"] = {{"at", "2026-11-02T12:00:00Z"},
                                 {"stage", "publish"},
                                 {"kind", "evidence"},
                                 {"reason", "Receipt awaited"},
                                 {"extra", "EXCLUDED_SENTINEL"}};
        active["retirement"] = {{"at", "2026-11-02T12:00:00Z"},
                                {"acknowledgement", "Owner acknowledges unknown"},
                                {"remote_outcome", "unknown"},
                                {"extra", "EXCLUDED_SENTINEL"}};
        active["failure_kind"] = "retirement";
        active["facts"]["unshown"] = "EXCLUDED_SENTINEL";
        active["facts"]["build_run"] = {
            {"job_id", 1},
            {"jobs",
             Json::array({{{"id", 2}, {"steps", Json::array({{{"status", "in_progress"}, {"name", "Other job progress"}}})}},
                          {{"id", 1}, {"status", "queued"}, {"started_at", nullptr}, {"extra", "EXCLUDED_SENTINEL"}}})}};
        work["attempts"].push_back(active);
        const auto row = ci::projectWork(work, 0);
        ci::require(row.at("build").at("step") == "Other job progress" && row.at("build").at("job").at("status") == "queued",
                    "Current step searches all jobs but timestamps remain bound to the matched job");
        ci::require(row.dump().find("EXCLUDED_SENTINEL") == std::string::npos && row.at("active").at("uncertainty").size() == 4 &&
                        row.at("active").at("retirement").size() == 3 && row.at("previous").at(0).at("retirement").size() == 3,
                    "Current and historical uncertainty/retirement exclude extra data");
        auto& published = work["attempts"].back();
        published["state"] = "published";
        published["receipt"] = {{"feed", {{"repository", "Fixture/feed"}}},
                                {"packages", Json::array({{{"path", "dir ü/!'()*#+%/package.deb"}}})}};
        published["proof"] = {{"feed_revision", "exact-revision"}, {"verified_at", "2026-11-02T12:00:01Z"}};
        ci::require(ci::projectWork(work, 0).at("packages") ==
                        "https://github.com/Fixture/feed/tree/exact-revision/dir%20%C3%BC/!'()*%23%2B%25",
                    "Packages links encode individual directory segments like encodeURIComponent");
        const Json definition = {{"title", "Definition"}, {"matrix", Json::array({fixture::target()})}, {"extra", true}};
        const Json repos = {{"Fixture/~example",
                             {{"definition", definition}, {"ref", "main"}, {"commit", "displayed"}, {"deployment", "EXCLUDED_SENTINEL"}}}};
        const auto metadata = ci::projectRepositories(repos);
        ci::require(metadata.at("Fixture/~example").size() == 4 && metadata.at("Fixture/~example").at("definition") == definition &&
                        metadata.dump().find("EXCLUDED_SENTINEL") == std::string::npos,
                    "Repository metadata preserves the complete parsed definition exactly once with four whitelisted fields");
        const auto escaped = Json::diff(Json::object(), metadata, "/repositories");
        ci::require(escaped.at(0).at("path") == "/repositories/Fixture~1~0example", "JSON library escapes pointer tokens");
        std::cout << "Compact projection: twenty-record patch " << patch.dump().size() + 8 << " SSE bytes; largest row " << largest << '\n';
    }

} // namespace

int main(int argc, char* argv[]) {
    int result = 0;
    try {
        publicationGuardFailure();
        using namespace fixture;
        observationUncertainty();
        compactProjection();
        const Json tags = Json::array({{{"name", "v1.0.0"}}, {{"name", "v1.5.0"}}, {{"name", "v2.0.0"}}, {{"name", "not-a-version"}}});
        ci::require(ci::sourceTag(tags, "v1.2.0") == "v1.2.0" && ci::sourceTag(tags, "") == "v2.0.0",
                    "Explicit older source and highest prerequisite");
        ci::require(ci::compareVersions("v1.10.0", "v1.9.0") > 0 && ci::compareVersions("v2.0.0", "v2.0.0-rc.1") > 0 &&
                        ci::compareVersions("v1.0.0-alpha.10", "v1.0.0-alpha.2") > 0,
                    "SemVer precedence must be numeric");
        ci::require(!ci::validVersion("v01.2.3") && !ci::validVersion("v1.2.3-01"), "Invalid tags rejected");
        Json selection = {{"Fixture/foundation", selected("foundation", Json::array(), ci::sourceTag(tags, ""))},
                          {"Fixture/client", selected("client", Json::array({"foundation"}), "v1.2.0")},
                          {"Fixture/tool", selected("tool", Json::array({"foundation"}))},
                          {"Fixture/bundle", selected("bundle", Json::array({"foundation", "client", "tool"}))},
                          {"Fixture/independent", selected("independent", Json::array())}};
        Json propagated = fixture::graph();
        const Json rootCommands = ci::selectRequest(propagated, selection, "Fixture/foundation", std::string(40, '1'));
        ci::require(
            propagated.at("executions").size() == 4 && rootCommands.size() == 1 &&
                work(propagated, rootCommands.at(0).at("execution")).at("source").at("package") == "foundation",
            "A root source trigger selects the transitive dependent diamond, excludes unrelated work, and dispatches only the root");
        const Json selectedChain = propagated;
        ci::require(ci::selectRequest(propagated, selection, "Fixture/foundation", std::string(40, '1')).empty() &&
                        propagated == selectedChain,
                    "Duplicate root notification cannot create work or dispatch effects");
        const std::string propagatedRoot = rootCommands.at(0).at("execution");
        finishBuild(propagated, propagatedRoot);
        const Json branches = publish(propagated, propagatedRoot);
        ci::require(branches.size() == 2, "Root publication independently releases both branches without a new request");
        for (const auto& branch : branches) {
            finishBuild(propagated, branch.at("execution"));
        }
        ci::require(publish(propagated, branches.at(0).at("execution")).empty(), "Join cannot release before its final exact prerequisite");
        const Json joinCommand = publish(propagated, branches.at(1).at("execution"));
        ci::require(joinCommand.size() == 1 && work(propagated, joinCommand.at(0).at("execution")).at("source").at("package") == "bundle",
                    "Final branch proof releases the automatically selected join");
        finishBuild(propagated, joinCommand.at(0).at("execution"));
        publish(propagated, joinCommand.at(0).at("execution"));
        const Json originalChain = propagated.at("executions");
        Json appChanged = selection;
        appChanged["Fixture/client"]["source"]["tag"] = "v1.3.0";
        appChanged["Fixture/client"]["source"]["commit"] = std::string(40, '7');
        const Json appCommands = ci::selectRequest(propagated, appChanged, "Fixture/client", std::string(40, '7'));
        ci::require(appCommands.size() == 1 && propagated.at("executions").size() == 6 &&
                        work(propagated, appCommands.at(0).at("execution")).at("source").at("package") == "client",
                    "App tag selects only new app and join work, reusing published root and sibling");
        for (std::size_t index = 0; index < originalChain.size(); ++index) {
            ci::require(propagated.at("executions").at(index) == originalChain.at(index),
                        "Forward selection cannot rewrite frozen history");
        }
        finishBuild(propagated, appCommands.at(0).at("execution"));
        const Json appJoin = publish(propagated, appCommands.at(0).at("execution"));
        ci::require(appJoin.size() == 1 && work(propagated, appJoin.at(0).at("execution")).at("source").at("package") == "bundle",
                    "App proof alone releases its newly selected join without rebuilding the sibling");
        finishBuild(propagated, appJoin.at(0).at("execution"));
        publish(propagated, appJoin.at(0).at("execution"));
        appChanged["Fixture/bundle"]["source"]["tag"] = "v1.1.0";
        appChanged["Fixture/bundle"]["source"]["commit"] = std::string(40, '8');
        const Json joinOnly = ci::selectRequest(propagated, appChanged, "Fixture/bundle", std::string(40, '8'));
        ci::require(joinOnly.size() == 1 && propagated.at("executions").size() == 7 &&
                        work(propagated, joinOnly.at(0).at("execution")).at("source").at("package") == "bundle",
                    "Join tag reuses published prerequisites and selects only the join");
        Json multi = selection, twoTargets = fixture::graph();
        Json secondTarget = target();
        secondTarget["id"] = "second-target";
        secondTarget["suite"] = "other";
        for (auto& selected : multi) {
            selected["definition"]["matrix"].push_back(secondTarget);
        }
        ci::require(ci::selectRequest(twoTargets, multi, "Fixture/foundation", std::string(40, '1')).size() == 2 &&
                        twoTargets.at("executions").size() == 8,
                    "Forward selection covers every target and dispatches every independent ready root");
        Json graph = fixture::graph();
        const std::string bundle = ci::selectWork(graph, selection, "Fixture/bundle", target());
        const std::string independent = ci::selectWork(graph, selection, "Fixture/independent", target());
        const std::string foundation = graph.at("executions").at(0).at("id");
        const std::string client = graph.at("executions").at(1).at("id");
        const std::string tool = graph.at("executions").at(2).at("id");
        const Json ready = ci::commands(graph);
        ci::require(ready.size() == 2 && ready.at(0).at("execution") == foundation && ready.at(1).at("execution") == independent,
                    "All independent roots dispatch in creation order");
        Json restarted = Json::parse(graph.dump());
        ci::require(ci::commands(graph).empty() && ci::commands(restarted).empty(), "Assignments never replay");
        Json timed = graph;
        const Json timedWork = work(timed, independent);
        ci::applyEvidence(timed, timedWork, "build", observation(timedWork, "build", "completed", "timed_out"), {}, "");
        ci::require(ci::current(work(timed, independent)).at("state") == "building" && ci::commands(timed).empty(),
                    "A timeout preserves the assignment and cannot authorize replacement");
        rejects(
            [&]() {
                ci::retry(timed, independent);
            },
            "Timed-out work is not a definite-failure retry");
        Json progress = graph;
        Json delayed = observation(work(progress, foundation), "build");
        delayed["jobs"][0]["status"] = "in_progress";
        delayed["jobs"][0]["completed_at"] = nullptr;
        delayed["jobs"][0]["steps"] = Json::array({{{"number", 1}, {"status", "completed"}, {"conclusion", "success"}}});
        ci::applyEvidence(progress, work(progress, foundation), "build", delayed, {}, "Artifact not yet visible");
        Json queued = observation(work(progress, foundation), "build", "queued", "");
        queued["jobs"][0]["started_at"] = "2026-10-31T08:59:55Z";
        ci::applyEvidence(progress, work(progress, foundation), "build", queued, {}, "");
        ci::require(ci::current(work(progress, foundation)).at("facts").at("build_run").at("jobs").at(0).at("started_at") ==
                        "2026-10-31T09:00:00Z",
                    "Late queued evidence cannot replace the actual runner start with queue creation time");
        finishBuild(progress, foundation);
        const Json accumulated = ci::current(work(progress, foundation)).at("facts").at("build_run");
        ci::require(accumulated.at("jobs").at(0).at("completed_at") == "2026-10-31T09:05:00Z" &&
                        accumulated.at("jobs").at(0).at("steps").at(0).at("status") == "completed",
                    "Completed run accepts later job timestamps and preserves earlier completed steps");
        ci::applyEvidence(progress, work(progress, foundation), "build", queued, {}, "");
        ci::require(ci::current(work(progress, foundation)).at("facts").at("build_run") == accumulated,
                    "Late queued facts cannot regress completed job timestamps, status or steps");
        publish(progress, foundation);
        finishBuild(graph, foundation);
        const Json building = work(graph, foundation);
        ci::applyEvidence(graph, building, "build", observation(building, "build", "completed", "failure"), {}, "");
        ci::require(ci::current(work(graph, foundation)).at("state") == "publishing", "Late build facts cannot fail publication");
        const Json frozen = work(graph, client).at("prerequisites");
        ci::require(ci::current(work(graph, client)).at("state") == "waiting", "Build success alone cannot release prerequisites");
        Json uncertainty = work(graph, foundation);
        ci::applyEvidence(graph,
                          uncertainty,
                          "publish",
                          observation(uncertainty, "publish"),
                          {{"verification_started_at", "2026-10-31T09:06:00Z"}},
                          "Receipt unavailable");
        ci::applyEvidence(graph,
                          uncertainty,
                          "publish",
                          observation(uncertainty, "publish"),
                          {{"verification_started_at", "2026-10-31T09:07:00Z"}},
                          "Receipt unavailable");
        ci::require(ci::current(work(graph, foundation)).at("state") == "publishing", "Unavailable proof is not failure");
        const Json released = publish(graph, foundation);
        ci::require(ci::current(work(graph, foundation)).at("verification_started_at") == "2026-10-31T09:06:00Z" &&
                        ci::current(work(graph, foundation)).at("proof").contains("verified_at"),
                    "Repeated unresolved checks preserve the first real verification start through successful proof");
        ci::require(released.size() == 2 && released.at(0).at("execution") == client && released.at(1).at("execution") == tool,
                    "Verified publication releases all exact dependents immediately");
        ci::require(work(graph, client).at("prerequisites") == frozen &&
                        ci::current(work(graph, client)).at("assignment").at("dependencies").at(0).at("attempt") ==
                            ci::current(work(graph, foundation)).at("id"),
                    "Frozen execution consumes exact published attempt");
        Json retried = graph;
        const Json original = ci::current(work(retried, client));
        ci::applyEvidence(
            retried, work(retried, client), "build", observation(work(retried, client), "build", "completed", "failure"), {}, "");
        ci::retry(retried, client);
        ci::commands(retried);
        const Json replacement = ci::current(work(retried, client));
        ci::require(work(retried, client).at("attempts").at(0).at("assignment") == original.at("assignment") &&
                        replacement.at("assignment").at("source") == original.at("assignment").at("source") &&
                        replacement.at("assignment").at("dependencies") == original.at("assignment").at("dependencies") &&
                        replacement.at("id") != original.at("id"),
                    "Dependent retry preserves original authority, source and exact terminal prerequisite inputs");
        Json later = selection;
        later["Fixture/foundation"]["source"]["tag"] = "v3.0.0";
        later["Fixture/foundation"]["source"]["commit"] = std::string(40, '7');
        Json notifications = graph;
        ci::require(ci::selectedTag(notifications, "Fixture/client", "v1.2.0", std::string(40, '1')) &&
                        ci::selectRequest(notifications, later, "Fixture/client", std::string(40, '1')).empty() && notifications == graph,
                    "Duplicate source notification cannot discover/rebind newer prerequisites");
        ci::require(ci::selectRequest(notifications, later, "Fixture/client").size() == 1 &&
                        notifications.at("executions").size() == graph.at("executions").size() + 4,
                    "Explicit app selection freezes new prerequisite and downstream join inputs without altering earlier work");
        const auto count = graph.at("executions").size();
        selection["Fixture/foundation"]["workflows"]["build"]["commit"] = std::string(40, '6');
        ci::selectWork(graph, selection, "Fixture/client", target());
        ci::require(graph.at("executions").size() == count && ci::commands(graph).empty(),
                    "Approved exact work is reused without rebuilding a library");
        finishBuild(graph, client);
        publish(graph, client);
        ci::require(ci::current(work(graph, bundle)).at("state") == "waiting", "Join waits for every exact prerequisite");
        finishBuild(graph, tool);
        ci::require(publish(graph, tool).size() == 1 && ci::current(work(graph, bundle)).at("state") == "building",
                    "Join releases on its final prerequisite");
        const Json terminal = ci::current(work(graph, foundation)).at("receipt");
        const Json published = work(graph, foundation);
        ci::require(ci::applyEvidence(graph, published, "publish", observation(published, "publish", "queued", ""), {}, "").empty() &&
                        ci::current(work(graph, foundation)).at("state") == "published" &&
                        ci::current(work(graph, foundation)).at("receipt") == terminal,
                    "Stale evidence cannot regress publication");
        Json receiptMismatch = graph;
        finishBuild(receiptMismatch, bundle);
        Json wrongReceipt = fixture::receipt(work(receiptMismatch, bundle));
        wrongReceipt["attempt"] = "unrelated";
        const auto rejectedCommands = ci::applyEvidence(
            receiptMismatch, work(graph, bundle), "publish", observation(work(graph, bundle), "publish"), {{"receipt", wrongReceipt}}, "");
        ci::require(rejectedCommands.empty() && ci::current(work(receiptMismatch, bundle)).at("state") == "publishing" &&
                        ci::current(work(receiptMismatch, bundle)).at("uncertainty").at("kind") == "evidence",
                    "A receipt for a different attempt retains evidence uncertainty and cannot release dependents");
        Json failed = work(graph, independent);
        ci::applyEvidence(graph, failed, "build", observation(failed, "build", "completed", "failure"), {}, "");
        const Json old = ci::current(work(graph, independent));
        ci::retry(graph, independent);
        ci::require(ci::commands(graph).size() == 1 && work(graph, independent).at("attempts").size() == 2 &&
                        ci::current(work(graph, independent)).at("id") != old.at("id") &&
                        ci::current(work(graph, independent)).at("assignment").at("source") == old.at("assignment").at("source"),
                    "Definite retry preserves input identity and history");
        finishBuild(graph, independent);
        const Json assigned = ci::current(work(graph, independent)).at("publish_assignment");
        ci::retire(graph, independent, "Operator acknowledges the unknown remote outcome");
        ci::require(ci::current(work(graph, independent)).at("failure_kind") == "retirement" &&
                        ci::current(work(graph, independent)).at("publish_assignment") == assigned,
                    "Retirement preserves original authority");
        rejects(
            [&]() {
                ci::retry(graph, independent);
            },
            "Retirement cannot be retried");
        ci::require(publish(graph, independent).empty() && ci::current(work(graph, independent)).at("state") == "failed",
                    "Late retirement evidence cannot release work");
        if (argc == 2) {
            std::ofstream output(argv[1]);
            output << graph.dump();
        }
        std::cout << "Graph and receipt invariants passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    return result;
}
