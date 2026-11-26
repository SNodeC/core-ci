#include "Fixture.h"
#include "core/Badge.h"

using namespace coreci;

namespace {
    Pipeline graph(const std::string& commit, const std::string& tree, const std::string& range = ">=0.0.0") {
        Pipeline::Builder builder;
        builder.target({"target"});
        builder.package({"root", {"owner", "root", "main"}, {"target"}, {}, 0, commit, tree});
        builder.package({"app", {"owner", "app", "main"}, {"target"}, {{"root", range}}, 0, commit, tree});
        return builder.build();
    }
    void load(History& history,
              const std::string& commit = hash('b'),
              const std::string& tree = hash('c'),
              const std::string& range = ">=0.0.0",
              std::int64_t at = 100) {
        history.apply(GraphChanged{at,
                                   graph(commit, tree, range),
                                   {{at, "root", "v3.0.0", hash(), Origin::Reconcile}, {at, "app", "v1.0.0", hash(), Origin::Reconcile}}});
    }
    void identity() {
        History history({0, 100, 3, 0});
        load(history);
        const auto first = history.wave();
        const auto id = first.plan.at("target")[0].buildId;
        history.apply(WaveStarted{101, first.id, 42, 1});
        history.apply(UnitBuildFailed{102, first.id, id, "broken hook"});
        history.apply(TargetFinished{103, first.id, "target", ""});
        history.apply(WaveFinished{104, first.id, 42, "failure"});
        load(history, hash('d'), hash('e'), ">=0.0.0", 105);
        history.apply(RetryRequested{106, "root", "target"});
        const auto corrected = history.wave();
        CHECK(corrected.id != first.id);
        CHECK_EQ(corrected.plan.at("target")[0].release.sourceSha, hash());
        CHECK_EQ(corrected.plan.at("target")[0].release.tag, "v3.0.0");
        CHECK_EQ(corrected.plan.at("target")[0].ciCommit, hash('d'));
        CHECK_EQ(corrected.plan.at("target")[0].ciTree, hash('e'));
        CHECK_EQ(corrected.plan.at("target")[0].revision, 2U);
        CHECK(history.engine.waves().at(first.id).plan == first.plan);
        history.complete(110);
        const auto count = history.engine.waves().size();
        load(history, hash('f'), hash('e'), ">=0.0.0", 120);
        history.apply(Tick{130});
        CHECK_EQ(history.engine.waves().size(), count);
        CHECK(history.engine.commands().empty());
    }
    void ranges() {
        History history({0, 100, 3, 0});
        load(history, hash('b'), hash('c'), ">=2.0,<3");
        CHECK_EQ(history.engine.units().at({"app", "target"}).state, State::Blocked);
        CHECK_EQ(history.engine.units().at({"app", "target"}).failure->reason, "dependency root 3.0.0 outside >=2.0,<3");
        CHECK_EQ(history.wave().plan.at("target").size(), 1U);
        const auto plan = app::statusSnapshot(history.engine);
        CHECK_EQ(plan["totals"]["Blocked"], 1);
        CHECK_EQ(plan["units"][0]["selection"]["ci_commit"], hash('b'));
        CHECK_EQ(history.engine.nextRevision({"app", "target"}), 1U);
        const auto v = *Version::parse("3.2.1");
        for (const auto& range : {">=3.0,<4", ">3,<=3.2.1", "==3.2.1", ">=0"}) {
            CHECK(v.satisfies(range));
        }
        for (const auto& range : {"<3.2.1", ">3.2.1", "==3.2", "1.0", ">=03", ">=3,", ">=3 || <4"}) {
            CHECK(!v.satisfies(range));
        }
    }
    void pausedBackoff() {
        History history({0, 100, 3, 0});
        load(history);
        history.apply(WaveDispatchRejected{101, history.wave().id, 1, 503, "retry"});
        history.apply(DispatchPaused{102, true});
        CHECK_EQ(*history.engine.nextDeadline(), 200);
        CHECK(std::holds_alternative<Ignore>(history.engine.classify(Tick{132})));
    }
    void pause() {
        History history({0, 100, 3, 0});
        history.apply(DispatchPaused{99, true});
        load(history);
        const auto pending = history.wave();
        CHECK(history.engine.commands().empty());
        CHECK_EQ(pending.deadline, 0);
        history.apply(Tick{10000});
        CHECK(history.wave() == pending);
        history.engine = WaveController(history.options, persistence::decodeState(persistence::encodeState(history.engine.stored())));
        CHECK(history.engine.stored().dispatchPaused);
        CHECK(history.engine.commands().empty());
        CHECK(history.wave() == pending);
        history.apply(DispatchPaused{10001, false});
        CHECK_EQ(history.engine.commands().size(), 1U);
        CHECK_EQ(history.wave().plan, pending.plan);
        CHECK_EQ(history.wave().deadline, 10101);
        history.apply(WaveStarted{10002, pending.id, 42, 1});
        history.apply(DispatchPaused{10003, true});
        for (const auto& entry : pending.plan.at("target")) {
            history.apply(UnitBuildSucceeded{10004, pending.id, entry.buildId});
        }
        history.apply(TargetFinished{10005, pending.id, "target", "artifact"});
        CHECK(!history.engine.publications().empty());
        CHECK(history.engine.commands().empty());
        CHECK_EQ(history.publication().run.deadline, 0);
        history.engine = WaveController(history.options, history.engine.stored());
        CHECK(history.engine.commands().empty());
        history.apply(DispatchPaused{10006, false});
        CHECK_EQ(history.engine.commands().size(), 1U);
        CHECK(history.engine.commands()[0].publication);
        history.apply(PublicationDispatchRejected{10007, history.publication().run.id, 1, 503, "retry"});
        history.apply(DispatchPaused{10008, true});
        CHECK(std::holds_alternative<Ignore>(history.engine.classify(Tick{10038})));
        CHECK_EQ(history.publication().run.deadline, 33406);
        CHECK(badge("a&\"'", "<>", "#fff").find("&amp;&quot;&apos;: &lt;&gt;") != std::string::npos);
    }
} // namespace
int main() {
    identity();
    ranges();
    pause();
    pausedBackoff();
    return checkResult();
}
