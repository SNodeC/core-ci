#include "Fixture.h"
#include "core/Badge.h"

#include <random>
using namespace coreci;

void debounce() {
    History h;
    h.load();
    CHECK(!h.engine.activeWave());
    CHECK(h.apply(Tick{104}).empty());
    h.apply(TagObserved{104, "a", "v1.1.0", hash('b'), Origin::Webhook});
    h.apply(TagObserved{106, "b", "v1.1.0", hash('c'), Origin::Webhook});
    CHECK(h.apply(Tick{110}).empty());
    CHECK_EQ(h.apply(Tick{111}).size(), 1U);
    const auto original = h.wave();
    CHECK_EQ(original.plan.at("t1").front().release.tag, "v1.1.0");
    CHECK_EQ(h.engine.waves().size(), 1U);
    h.apply(WaveDispatchAccepted{112, original.id, 1});
    h.apply(TagObserved{113, "a", "v2.0.0", hash('d'), Origin::Webhook});
    CHECK(h.apply(Tick{118}).empty());
    CHECK_EQ(h.engine.waves().size(), 1U);
    CHECK_EQ(h.wave().plan.at("t1").front().release.tag, "v1.1.0");
    h.complete(120);
    CHECK_EQ(h.engine.waves().size(), 2U);
    CHECK(h.engine.activeWave());
    CHECK_EQ(h.wave().plan.at("t1").front().release.tag, "v2.0.0");
}

void planning() {
    History h;
    h.load(10);
    CHECK_EQ(h.apply(Tick{105}).size(), 1U);
    for (const auto& [target, entries] : h.wave().plan) {
        for (const auto& entry : entries) {
            CHECK(entry.build);
            CHECK_EQ(entry.revision, 11U);
            for (const auto& [name, input] : entry.inputs) {
                CHECK_EQ(input.revision, 11U);
            }
        }
    }
    h.complete();
    CHECK(!h.engine.activeWave());
    for (const auto& [unit, state] : h.engine.units()) {
        CHECK(state.state == State::Published);
        CHECK_EQ(state.published->feedCommit, hash('b'));
    }
    const auto before = h.engine.waves().size();
    h.apply(GraphChanged{120, testPipeline(10)});
    h.apply(Resumed{125});
    CHECK_EQ(h.engine.waves().size(), before);
    h.apply(TagObserved{130, "b", "v1.1.0", hash('c'), Origin::Webhook});
    CHECK_EQ(h.apply(Tick{135}).size(), 1U);
    const auto planned = h.wave();
    CHECK_EQ(planned.plan.at("t1").size(), 6U);
    CHECK_EQ(planned.plan.at("t2").size(), 5U);
    for (const auto& [target, entries] : planned.plan) {
        for (const auto& entry : entries) {
            const bool build = entry.unit.package == "b" || entry.unit.package == "d" || entry.unit.package == "e";
            CHECK(entry.build == build);
            CHECK_EQ(entry.revision, build && entry.unit.package != "b" ? 12U : 11U);
            CHECK(entry.build == !entry.buildId.empty());
        }
    }
    h.complete(140);
    h.apply(RebuildRequested{150, "a", "t1"});
    h.apply(Tick{155});
    CHECK_EQ(h.wave().plan.size(), 1U);
    const auto diamond = h.wave().plan.at("t1");
    for (const auto& entry : diamond) {
        CHECK(entry.build == (entry.unit.package != "z"));
    }
    CHECK_EQ(diamond[3].inputs.at("b").revision, 12U);
    CHECK_EQ(diamond[3].inputs.at("c").revision, 12U);
    CHECK(unitBadge(h.engine, {"a", "t1"}).find("building") != std::string::npos);
}

void outcomes() {
    History h;
    h.load();
    h.apply(Tick{105});
    const auto w = h.wave();
    h.apply(UnitBuildFailed{106, w.id, w.plan.at("t1")[0].buildId, "compiler"});
    for (const auto& [target, entries] : w.plan) {
        for (const auto& entry : entries) {
            if (target == "t2" || entry.unit.package == "z") {
                h.apply(UnitBuildSucceeded{107, w.id, entry.buildId, "artifact"});
                h.apply(UnitPublishSucceeded{108, w.id, entry.buildId, hash('c')});
            }
        }
    }
    h.apply(WaveFinished{110, w.id, 42, "failure"});
    CHECK(h.engine.units().at({"a", "t1"}).state == State::Failed);
    for (const auto& name : {"b", "c", "d", "e"}) {
        CHECK(h.engine.units().at({name, "t1"}).state == State::Blocked);
        CHECK(!h.engine.units().at({name, "t1"}).failure);
    }
    CHECK(h.engine.units().at({"z", "t1"}).state == State::Published);
    CHECK(h.engine.units().at({"a", "t2"}).state == State::Published);
    CHECK(unitBadge(h.engine, {"b", "t1"}).find("blocked") != std::string::npos);
    h.apply(RetryRequested{111, "a", "t1"});
    h.apply(Tick{116});
    CHECK_EQ(h.wave().plan.size(), 1U);
    for (const auto& entry : h.wave().plan.at("t1")) {
        CHECK(entry.build == (entry.unit.package != "z"));
        CHECK_EQ(entry.revision, entry.build ? 2U : 1U);
    }
    History missing;
    missing.load();
    missing.apply(Tick{105});
    const auto m = missing.wave();
    missing.apply(UnitBuildStarted{106, m.id, m.plan.at("t1")[0].buildId});
    missing.apply(UnitBuildSucceeded{107, m.id, m.plan.at("t1")[1].buildId, "artifact"});
    missing.apply(WaveFinished{108, m.id, 42, "failure"});
    for (const auto& [unit, state] : missing.engine.units()) {
        CHECK(state.state == State::Failed);
        CHECK_EQ(state.failure->reason, "no result");
    }
}

void retry() {
    for (const bool automatic : {false, true}) {
        History h({5, 100, 3, automatic ? 10 : 0});
        h.load();
        h.apply(Tick{105});
        const auto w = h.wave();
        h.apply(UnitBuildFailed{106, w.id, w.plan.at("t1")[0].buildId, "compiler"});
        for (const auto& [target, entries] : w.plan) {
            for (const auto& entry : entries) {
                if (target == "t2" || entry.unit.package == "z") {
                    h.apply(UnitBuildSucceeded{107, w.id, entry.buildId, "artifact"});
                    h.apply(UnitPublishSucceeded{108, w.id, entry.buildId, hash('b')});
                }
            }
        }
        h.apply(WaveFinished{110, w.id, 42, "failure"});
        if (automatic) {
            CHECK(h.apply(Tick{115}).empty());
            CHECK(h.apply(Tick{116}).empty());
        } else {
            h.apply(RetryRequested{116, "a", "t1"});
        }
        CHECK_EQ(h.apply(Tick{121}).size(), 1U);
        const auto second = h.wave();
        CHECK_EQ(second.plan.size(), 1U);
        CHECK_EQ(second.plan.at("t1")[0].revision, 2U);
        h.apply(UnitBuildFailed{122, second.id, second.plan.at("t1")[0].buildId, "again"});
        h.apply(WaveFinished{123, second.id, 43, "failure"});
        if (automatic) {
            CHECK(!h.engine.units().at({"a", "t1"}).retryAfter);
        }
    }
}

void dispatch() {
    History h;
    h.load();
    h.apply(Tick{105});
    const auto w = h.wave();
    h.apply(WaveDispatchRejected{106, w.id, 1, 503, "busy"});
    CHECK_EQ(*h.engine.nextDeadline(), 136);
    CHECK(h.apply(Tick{135}).empty());
    const auto commands = h.apply(Tick{136});
    CHECK_EQ(commands.size(), 1U);
    CHECK_EQ(commands.front().waveId, w.id);
    CHECK_EQ(commands.front().attempt, 2U);
    CHECK(h.apply(WaveDispatchAccepted{137, w.id, 1}).empty());
    h.apply(WaveDispatchRejected{137, w.id, 2, 503, "busy"});
    CHECK_EQ(*h.engine.nextDeadline(), 197);
    h.apply(Tick{197});
    h.apply(WaveDispatchRejected{198, w.id, 3, 503, "busy"});
    CHECK(!h.engine.activeWave());
    CHECK(h.engine.units().at({"a", "t1"}).state == State::Failed);
    for (const auto status : {401, 403, 404, 422}) {
        History permanent({5, 100, 3, 10});
        permanent.load();
        permanent.apply(Tick{105});
        permanent.apply(WaveDispatchRejected{106, permanent.wave().id, 1, status, "denied"});
        CHECK(!permanent.engine.activeWave());
        for (const auto& [unit, state] : permanent.engine.units()) {
            CHECK(!state.retryAfter);
        }
    }
    History deadline;
    deadline.load();
    deadline.apply(Tick{105});
    deadline.apply(WaveDispatchAccepted{106, deadline.wave().id, 1});
    CHECK(deadline.apply(Tick{204}).empty());
    deadline.apply(Tick{205});
    CHECK_EQ(deadline.wave().conclusion, "timeout");
    CHECK(!deadline.engine.activeWave());
}

void restart() {
    for (const bool accepted : {false, true}) {
        History h;
        h.load();
        h.apply(Tick{105});
        const auto w = h.wave();
        if (accepted) {
            h.apply(WaveDispatchAccepted{106, w.id, 1});
        }
        CHECK(h.apply(Resumed{110}).empty());
        CHECK(h.engine.activeWave());
        h.apply(WaveStarted{111, w.id, 42, 2});
        const auto before = h.engine.units();
        const auto at = h.engine.time();
        for (const Event& e : std::vector<Event>{WaveStarted{999, w.id, 43, 3},
                                                 WaveFinished{999, w.id, 43, "success"},
                                                 UnitBuildStarted{999, "wrong", w.plan.at("t1")[0].buildId},
                                                 UnitPublishSucceeded{999, w.id, w.plan.at("t1")[0].buildId, hash('c')}}) {
            CHECK(h.apply(e).empty());
            CHECK(h.engine.units() == before);
            CHECK(h.engine.time() == at);
        }
        CHECK(h.apply(Resumed{120}).empty());
        h.apply(Tick{205});
        CHECK_EQ(h.wave().conclusion, "timeout");
    }
    History h;
    h.load();
    h.apply(Tick{105});
    const auto w = h.wave();
    h.apply(UnitBuildSucceeded{106, w.id, w.plan.at("t1")[0].buildId, "artifact"});
    CHECK(h.apply(WaveDispatchAccepted{107, w.id, 1}).empty());
    h.apply(UnitPublishFailed{108, w.id, w.plan.at("t1")[0].buildId, "signing"});
    CHECK(h.apply(UnitPublishSucceeded{109, w.id, w.plan.at("t1")[0].buildId, hash('b')}).empty());
}

void graph() {
    History h;
    h.load();
    h.apply(Tick{105});
    const auto w = h.wave();
    Pipeline::Builder builder;
    builder.target({"t1"}).package({"a", {"owner", "a", "trunk"}, {"t1"}, {}});
    h.apply(GraphChanged{106, builder.build()});
    h.apply(UnitBuildSucceeded{107, w.id, w.plan.at("t1")[1].buildId, "artifact"});
    h.apply(UnitPublishSucceeded{108, w.id, w.plan.at("t1")[1].buildId, hash('b')});
    CHECK(h.engine.units().at({"b", "t1"}).state == State::Published);
    CHECK_EQ(h.wave().plan.at("t1").size(), 6U);
    h.apply(WaveFinished{109, w.id, 42, "failure"});
    h.apply(GraphChanged{110, testPipeline()});
    h.apply(RetryRequested{111, "a", "t1"});
    h.apply(Tick{116});
    CHECK(h.engine.activeWave());
    CHECK_EQ(h.wave().plan.size(), 1U);
    CHECK(h.engine.units().at({"a", "t2"}).state == State::Failed);

    History addition;
    addition.load();
    addition.apply(Tick{105});
    addition.complete();
    Pipeline::Builder more;
    for (const auto& [id, target] : addition.engine.pipeline().targets()) {
        more.target(target);
    }
    for (const auto& [name, package] : addition.engine.pipeline().packages()) {
        more.package(package);
    }
    more.target({"t3"}).package({"q", {"owner", "q", "main"}, {"t3"}, {}});
    addition.apply(GraphChanged{120, more.build(), {{120, "q", "v1.0.0", hash(), Origin::Reconcile}}});
    CHECK_EQ(addition.apply(Tick{125}).size(), 1U);
    CHECK_EQ(addition.wave().plan.size(), 1U);
    CHECK_EQ(addition.wave().plan.at("t3").front().unit.package, "q");
    CHECK(addition.engine.units().at({"a", "t1"}).state == State::Published);
}

void randomized() {
    std::mt19937 random(0x57a9);
    for (unsigned iteration = 0; iteration < 10000; ++iteration) {
        History h;
        h.load();
        h.apply(Tick{105});
        const auto w = h.wave();
        for (const auto& [target, entries] : w.plan) {
            std::set<std::string> failed;
            for (const auto& entry : entries) {
                const bool blocked = std::any_of(entry.inputs.begin(), entry.inputs.end(), [&](const auto& input) {
                    return failed.contains(input.first);
                });
                if (blocked) {
                    failed.insert(entry.unit.package);
                } else if (random() % 5 == 0) {
                    h.apply(UnitBuildFailed{106, w.id, entry.buildId, "compiler"});
                    failed.insert(entry.unit.package);
                } else {
                    h.apply(UnitBuildSucceeded{107, w.id, entry.buildId, "artifact"});
                    h.apply(UnitPublishSucceeded{108, w.id, entry.buildId, hash('b')});
                }
            }
        }
        h.apply(WaveFinished{110, w.id, 42, "completed"});
        CHECK(!h.engine.activeWave());
        for (const auto& [unit, state] : h.engine.units()) {
            CHECK(state.state == State::Published || state.state == State::Failed || state.state == State::Blocked);
            if (state.published) {
                CHECK_EQ(state.published->revision, 1U);
                CHECK_EQ(state.published->inputs.size(), h.engine.pipeline().prerequisites(unit.package).size());
            }
        }
    }
}

int main(int argc, char** argv) {
    const std::map<std::string, void (*)()> scenarios{{"debounce", debounce},
                                                      {"planning", planning},
                                                      {"outcomes", outcomes},
                                                      {"retry", retry},
                                                      {"dispatch", dispatch},
                                                      {"restart", restart},
                                                      {"graph", graph},
                                                      {"randomized", randomized}};
    CHECK(argc == 2);
    if (argc == 2) {
        scenarios.at(argv[1])();
    }
    return checkResult();
}
