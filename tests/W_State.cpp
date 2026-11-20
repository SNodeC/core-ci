#include "Fixture.h"

#include <random>

using namespace coreci;

namespace {
    History started(unsigned targets = 2, Options options = {0, 100, 3, 0, "feed", {"SNodeC", "Packages", "main"}, 900, 100}) {
        History h(options);
        Pipeline::Builder b;
        std::set<std::string> names;
        for (unsigned i = 1; i <= targets; ++i) {
            names.insert("t" + std::to_string(i));
            b.target({"t" + std::to_string(i)});
        }
        b.package({"a", {"o", "a", "main"}, names, {}});
        b.package({"b", {"o", "b", "main"}, names, {"a"}});
        h.apply(GraphChanged{
            100, b.build(), {{100, "a", "v1.0.0", hash(), Origin::Reconcile}, {100, "b", "v1.0.0", hash(), Origin::Reconcile}}});
        h.apply(WaveStarted{101, h.wave().id, 42, 1});
        return h;
    }
    void built(History& h, const std::string& target, std::int64_t at = 102) {
        const auto wave = h.wave();
        for (const auto& unit : wave.plan.at(target)) {
            h.apply(UnitBuildSucceeded{at, wave.id, unit.buildId});
        }
        h.apply(TargetFinished{at, wave.id, target, "coreci-" + wave.id + "-" + target});
    }
    void early() {
        auto h = started(3);
        CHECK(h.engine.publications().empty());
        built(h, "t1");
        const auto first = h.publication();
        CHECK_EQ(first.items.size(), 1U);
        CHECK_EQ(first.items[0].target, "t1");
        CHECK_EQ(h.engine.commands().size(), 1U);
        CHECK(h.engine.commands()[0].publication);
        CHECK(h.engine.activeWave());
        h.apply(PublicationStarted{103, first.run.id, 55, 1});
        h.apply(PublicationStarted{104, first.run.id, 999, 2});
        CHECK_EQ(*h.publication().run.runId, 55U);
        built(h, "t2", 104);
        built(h, "t3", 105);
        h.options.feedBranch = "feed-staging";
        h.engine = WaveController(h.options, h.engine.stored());
        CHECK_EQ(h.engine.publications().size(), 1U);
        h.apply(TagObserved{106, "a", "v2.0.0", hash('c'), Origin::Webhook});
        const auto waveId = h.wave().id;
        h.apply(WaveFinished{107, waveId, 42, "success"});
        CHECK_EQ(h.wave().id, waveId);
        CHECK(h.engine.activeWave());
        for (const auto& id : first.items[0].buildIds) {
            h.apply(UnitPublishSucceeded{108, first.run.id, id, hash('b')});
            const auto state = h.engine.stored();
            h.apply(UnitPublishSucceeded{109, first.run.id, id, hash('b')});
            CHECK(h.engine.stored() == state);
        }
        h.apply(PublicationFinished{110, first.run.id, 55, "success"});
        const auto second = h.publication();
        CHECK(second.run.id != first.run.id);
        CHECK_EQ(second.items.size(), 2U);
        CHECK_EQ(second.run.feedBranch, "feed");
        CHECK_EQ(second.items[0].target, "t2");
        CHECK_EQ(second.items[1].target, "t3");
        h.publish(111);
        CHECK(h.wave().id != waveId);
        CHECK_EQ(h.wave().feedBranch, "feed-staging");
        CHECK_EQ(h.engine.waves().at(waveId).phase, WavePhase::Finished);
        CHECK_EQ(h.wave().plan.at("t1")[0].revision, 1U);
        CHECK_EQ(h.wave().plan.at("t1")[1].revision, 2U);
    }
    void failures() {
        for (const std::string failure : {"artifact-unavailable", "immutable-conflict", "feed-push", "signing", "other", "unknown", ""}) {
            auto h = started(1);
            built(h, "t1");
            const auto publication = h.publication();
            const auto wave = h.wave();
            h.apply(PublicationStarted{103, publication.run.id, 55, 1});
            for (const auto& id : publication.items[0].buildIds) {
                h.apply(UnitPublishFailed{104, publication.run.id, id, "artifact unavailable: misleading display text", failure});
                const auto terminal = h.engine.stored();
                h.apply(UnitPublishSucceeded{104, publication.run.id, id, hash('b')});
                CHECK(h.engine.stored() == terminal);
            }
            h.apply(PublicationFinished{105, publication.run.id, 55, "failure"});
            h.apply(WaveFinished{106, wave.id, 42, "success"});
            h.apply(RetryRequested{107, "a", "t1"});
            if (failure == "artifact-unavailable") {
                CHECK(h.wave().id != wave.id);
                CHECK_EQ(h.wave().plan.at("t1")[0].revision, 2U);
            } else {
                CHECK_EQ(h.wave().id, wave.id);
                CHECK(h.publication().run.id != publication.run.id);
                CHECK_EQ(h.publication().items[0].artifact, publication.items[0].artifact);
                CHECK_EQ(h.engine.nextRevision({"a", "t1"}), 2U);
                CHECK_EQ(h.publication().items[0].buildIds, publication.items[0].buildIds);
            }
        }
    }
    void outcomes() {
        auto empty = started(1);
        const auto emptyWave = empty.wave();
        empty.apply(TargetFinished{102, emptyWave.id, "t1", ""});
        empty.apply(UnitBuildSucceeded{103, emptyWave.id, emptyWave.plan.at("t1")[0].buildId});
        empty.apply(WaveFinished{104, emptyWave.id, 42, "failure"});
        CHECK(!empty.engine.activeWave());
        CHECK_EQ(empty.engine.units().at({"a", "t1"}).state, State::Failed);
        auto partial = started(1);
        built(partial, "t1");
        const auto publication = partial.publication();
        partial.apply(PublicationStarted{103, publication.run.id, 55, 1});
        partial.apply(UnitPublishSucceeded{104, publication.run.id, publication.items[0].buildIds[0], hash('b')});
        partial.apply(UnitPublishFailed{105, publication.run.id, publication.items[0].buildIds[1], "signing", "signing"});
        partial.apply(PublicationFinished{106, publication.run.id, 55, "failure"});
        partial.apply(WaveFinished{107, partial.wave().id, 42, "success"});
        CHECK_EQ(partial.engine.stored().waveTargets.begin()->second.status, State::Failed);
        CHECK(!partial.engine.activeWave());
        auto h = started();
        const auto wave = h.wave();
        h.apply(UnitBuildFailed{102, wave.id, wave.plan.at("t1")[0].buildId, "compiler"});
        h.apply(TargetFinished{103, wave.id, "t1", ""});
        CHECK_EQ(h.engine.units().at({"b", "t1"}).state, State::Blocked);
        built(h, "t2", 104);
        h.apply(WaveFinished{105, wave.id, 42, "failure"});
        CHECK(h.engine.activeWave());
        h.publish(106);
        CHECK(!h.engine.activeWave());
        h.apply(RetryRequested{109, "a", "t1"});
        CHECK(h.wave().id != wave.id);
        CHECK_EQ(h.wave().plan.size(), 1U);
        CHECK_EQ(h.wave().plan.at("t1").size(), 2U);
        CHECK_EQ(h.wave().plan.at("t1")[0].revision, 2U);
        CHECK_EQ(h.wave().plan.at("t1")[1].revision, 2U);
    }
    void deadlines() {
        auto h = started(1);
        const auto wave = h.wave();
        h.apply(Tick{200});
        CHECK(!h.engine.activeWave());
        CHECK(h.engine.waves().at(wave.id).expired);
        built(h, "t1", 201);
        CHECK(!h.engine.publications().empty());
        const auto p = h.publication();
        h.apply(PublicationStarted{202, p.run.id, 55, 1});
        h.apply(Tick{302});
        CHECK_EQ(h.publication().run.phase, WavePhase::Finished);
        for (const auto& id : p.items[0].buildIds) {
            h.apply(UnitPublishSucceeded{303, p.run.id, id, hash('b')});
        }
        CHECK_EQ(h.engine.units().at({"a", "t1"}).state, State::Published);
        h.apply(RebuildRequested{304, "a", "t1"});
        CHECK(h.wave().id != wave.id);
        CHECK_EQ(h.wave().plan.at("t1")[0].revision, 2U);
        const auto newer = h.wave();
        h.complete(305);
        const auto fact = h.engine.units().at({"a", "t1"}).published;
        h.apply(UnitPublishSucceeded{315, p.run.id, p.items[0].buildIds[0], hash('c')});
        CHECK(h.engine.units().at({"a", "t1"}).published == fact);
        const auto committed = h.engine.stored();
        h.apply(UnitPublishSucceeded{316, p.run.id, p.items[0].buildIds[0], hash('c')});
        CHECK(h.engine.stored() == committed);
        CHECK_EQ(h.engine.waves().at(newer.id).phase, WavePhase::Finished);
    }
    void dispatch() {
        History h({0, 100, 3, 0, "feed", {"SNodeC", "Packages", "main"}, 10, 80});
        h.load();
        const auto original = h.wave();
        CHECK_EQ(h.engine.commands().size(), 1U);
        h.apply(WaveDispatchAccepted{101, original.id, 1});
        CHECK(h.engine.commands().empty());
        h.apply(Tick{110});
        CHECK_EQ(h.wave().attempt, 2U);
        CHECK_EQ(h.wave().deadline, original.deadline);
        h.apply(WaveDispatchRejected{111, original.id, 2, 500, "temporary"});
        CHECK(h.engine.commands().empty());
        CHECK_EQ(*h.wave().retryAt, 171);
        Options changed = h.options;
        changed.waveDeadline = 86400;
        changed.waveStartTimeout = 86400;
        WaveController restarted(changed, h.engine.stored());
        CHECK_EQ(*restarted.nextDeadline(), 171);
        h.apply(Tick{171});
        CHECK_EQ(h.wave().attempt, 3U);
        h.apply(WaveDispatchRejected{172, original.id, 3, 403, "permanent"});
        CHECK(!h.engine.activeWave());
        for (const auto& [unit, state] : h.engine.units()) {
            CHECK(!state.retryAfter);
        }
    }
    void randomized() {
        std::mt19937 random(8130648);
        for (unsigned run = 0; run < 10000; ++run) {
            auto h = started(1);
            const auto wave = h.wave();
            const bool failure = random() % 4 == 0;
            if (failure) {
                h.apply(UnitBuildFailed{102, wave.id, wave.plan.at("t1")[0].buildId, "compiler"});
                h.apply(TargetFinished{103, wave.id, "t1", ""});
            } else {
                built(h, "t1");
                h.publish(103);
            }
            h.apply(WaveFinished{106, wave.id, 42, failure ? "failure" : "success"});
            CHECK(!h.engine.activeWave());
            CHECK_EQ(h.engine.units().at({"a", "t1"}).state, failure ? State::Failed : State::Published);
            const auto committed = h.engine.stored();
            h.apply(WaveFinished{107, wave.id, 999, "failure"});
            CHECK(committed == h.engine.stored());
        }
    }
} // namespace
int main(int argc, char** argv) {
    const std::string scenario = argc == 2 ? argv[1] : "all";
    if (scenario == "all" || scenario == "early") {
        early();
    }
    if (scenario == "all" || scenario == "failures") {
        failures();
    }
    if (scenario == "all" || scenario == "outcomes") {
        outcomes();
    }
    if (scenario == "all" || scenario == "deadlines") {
        deadlines();
    }
    if (scenario == "all" || scenario == "dispatch") {
        dispatch();
    }
    if (scenario == "all" || scenario == "randomized") {
        randomized();
    }
    return checkResult();
}
