#include "LegacyFixture.h"
#include "migration/legacy/Badge.h"

#include <random>
using namespace coreci::legacy;
using coreci::Pipeline;
using coreci::Repo;
using coreci::Unit;
using coreci::Version;

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
    h.apply(WaveStarted{105, w.id, 42, 1});
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
    missing.apply(WaveStarted{105, m.id, 42, 1});
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
        h.apply(WaveStarted{105, w.id, 42, 1});
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
        h.apply(WaveStarted{121, second.id, 43, 1});
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
    deadline.apply(WaveExpired{205, deadline.wave().id});
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
        CHECK_EQ(h.apply(Resumed{110}).size(), accepted ? 0U : 1U);
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
        h.apply(WaveExpired{205, w.id});
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

void late() {
    History h;
    h.load();
    h.apply(Tick{105});
    const auto w = h.wave();
    h.apply(WaveStarted{106, w.id, 42, 1});
    const auto first = w.plan.at("t1").front().buildId;
    h.apply(UnitBuildSucceeded{107, w.id, first, "artifact"});
    const auto before = h.engine.units();
    h.apply(Resumed{205});
    CHECK(h.engine.activeWave());
    h.apply(WaveExpired{205, w.id});
    CHECK(!h.engine.activeWave());
    CHECK_EQ(h.wave().conclusion, "timeout");
    CHECK(h.engine.units() == before);
    CHECK(h.engine.commands().empty());
    h.apply(UnitPublishSucceeded{206, w.id, first, hash('b')});
    for (const auto& [target, entries] : w.plan) {
        for (const auto& entry : entries) {
            if (entry.buildId != first) {
                h.apply(UnitBuildStarted{207, w.id, entry.buildId});
                h.apply(UnitBuildSucceeded{208, w.id, entry.buildId, "artifact"});
                h.apply(UnitPublishSucceeded{209, w.id, entry.buildId, hash('b')});
            }
        }
    }
    CHECK(!h.engine.activeWave());
    CHECK(h.engine.commands().empty());
    for (const auto& [unit, state] : h.engine.units()) {
        CHECK(state.state == State::Published);
        CHECK(!state.failure);
        CHECK(!state.retryAfter);
        CHECK(state.published.has_value());
        if (state.published) {
            CHECK_EQ(state.published->waveId, w.id);
            CHECK_EQ(state.published->revision, 1U);
        }
    }
    h.apply(WaveFinished{210, w.id, 42, "success"});
    CHECK_EQ(h.wave().conclusion, "success");
    h.apply(RebuildRequested{211, "z", "t1"});
    h.apply(Tick{216});
    CHECK(h.engine.activeWave());
    CHECK_EQ(h.wave().plan.size(), 1U);
    for (const auto& entry : h.wave().plan.at("t1")) {
        CHECK(entry.build == (entry.unit.package == "z"));
        CHECK_EQ(entry.revision, entry.build ? 2U : 1U);
    }

    History overlapping;
    overlapping.load();
    overlapping.apply(Tick{105});
    const auto expired = overlapping.wave();
    overlapping.apply(WaveStarted{106, expired.id, 42, 1});
    overlapping.apply(WaveExpired{205, expired.id});
    overlapping.apply(TagObserved{206, "z", "v2.0.0", hash('c'), Origin::Webhook});
    overlapping.apply(Tick{211});
    const auto newer = overlapping.wave();
    CHECK(newer.id != expired.id);
    const auto assigned = overlapping.engine.units();
    overlapping.apply(WaveFinished{212, expired.id, 42, "failure"});
    CHECK(overlapping.engine.units() == assigned);
    CHECK_EQ(overlapping.engine.activeWave()->id, newer.id);

    History failures({5, 100, 3, 10});
    failures.load();
    failures.apply(Tick{105});
    const auto failed = failures.wave();
    failures.apply(WaveStarted{106, failed.id, 42, 1});
    failures.apply(WaveExpired{205, failed.id});
    CHECK(!failures.engine.nextDeadline());
    failures.apply(UnitBuildFailed{206, failed.id, failed.plan.at("t1")[0].buildId, "late compiler failure"});
    failures.apply(UnitBuildSucceeded{207, failed.id, failed.plan.at("t2")[0].buildId, "artifact"});
    failures.apply(UnitPublishFailed{208, failed.id, failed.plan.at("t2")[0].buildId, "late feed failure"});
    CHECK_EQ(failures.engine.units().at({"a", "t1"}).failure->reason, "late compiler failure");
    CHECK_EQ(failures.engine.units().at({"a", "t2"}).failure->reason, "late feed failure");
    CHECK_EQ(*failures.engine.units().at({"a", "t1"}).retryAfter, 216);
    failures.apply(WaveFinished{209, failed.id, 42, "failure"});
    CHECK(failures.engine.units().at({"b", "t1"}).state == State::Blocked);
}

void graph() {
    History h;
    h.load();
    h.apply(Tick{105});
    const auto w = h.wave();
    h.apply(WaveStarted{105, w.id, 42, 1});
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

void deadlines() {
    History h({5, 100, 3, 0});
    h.load();
    h.apply(Tick{105});
    const auto first = h.wave();
    CHECK_EQ(first.deadline, 205);
    h.apply(WaveStarted{106, first.id, 42, 1});
    h.apply(Tick{205});
    CHECK(h.engine.activeWave()); // Only the persisted expiry ends the hold.
    h.apply(WaveExpired{205, first.id});
    const auto expired = h.wave();
    const auto count = h.events.size();
    h.apply(WaveExpired{999, first.id});
    CHECK_EQ(h.events.size(), count);
    CHECK(h.wave() == expired);
    Options changed = h.options;
    changed.waveDeadline = 86400;
    Engine restarted(changed);
    for (std::size_t n = 0; n < h.events.size(); ++n) {
        restarted.handle(h.events[n], n + 1);
    }
    CHECK(restarted.waves() == h.engine.waves());
    CHECK(restarted.units() == h.engine.units());
    CHECK(!restarted.activeWave());
    restarted.handle(restarted.record(TagObserved{206, "z", "v2.0.0", hash('c'), Origin::Webhook}), count + 1);
    restarted.handle(restarted.record(Tick{211}), count + 2);
    CHECK_EQ(restarted.activeWave()->deadline, 86611);
    CHECK_EQ(restarted.waves().at(first.id).deadline, 205);
    const auto newer = *restarted.activeWave();
    const auto unit = first.plan.at("t1").front();
    const auto allocation = restarted.units().at(unit.unit);
    restarted.handle(restarted.record(UnitPublishSucceeded{212, first.id, unit.buildId, hash('b')}), count + 3);
    auto expected = allocation;
    expected.published = restarted.units().at(unit.unit).published;
    CHECK(restarted.units().at(unit.unit) == expected);
    CHECK(expected.published && expected.published->waveId == first.id);
    CHECK_EQ(restarted.activeWave()->id, newer.id);
    restarted.handle(restarted.record(WaveStarted{213, newer.id, 43, 1}), count + 4);
    const auto nextUnit = newer.plan.at("t1").front();
    restarted.handle(restarted.record(UnitBuildSucceeded{214, newer.id, nextUnit.buildId, "new-artifact"}), count + 5);
    restarted.handle(restarted.record(UnitPublishSucceeded{215, newer.id, nextUnit.buildId, hash('d')}), count + 6);
    const auto published = restarted.units().at(unit.unit);
    CHECK(std::holds_alternative<Ignore>(restarted.classify(UnitPublishSucceeded{999, first.id, unit.buildId, hash('b')})));
    CHECK(restarted.units().at(unit.unit) == published);

    History backoff;
    backoff.load();
    backoff.apply(Tick{105});
    backoff.apply(WaveDispatchRejected{106, backoff.wave().id, 1, 503, "busy"});
    backoff.apply(Tick{136});
    CHECK_EQ(backoff.wave().deadline, 236);
    Engine replay(changed);
    for (std::size_t n = 0; n < backoff.events.size(); ++n) {
        replay.handle(backoff.events[n], n + 1);
    }
    CHECK(replay.waves() == backoff.engine.waves());

    History terminal;
    terminal.load();
    terminal.apply(Tick{105});
    terminal.apply(WaveStarted{106, terminal.wave().id, 42, 1});
    terminal.apply(WaveFinished{107, terminal.wave().id, 42, "timeout"});
    const auto finished = terminal.wave();
    CHECK(std::holds_alternative<Ignore>(terminal.engine.classify(WaveFinished{999, finished.id, 42, "success"})));
    CHECK(terminal.wave() == finished);

    Engine legacy({5, 86400, 3, 600});
    const std::vector<Event> oldRows{
        h.events.front(), Tick{105}, WaveStarted{106, "w2", 42, 1}, UnitBuildSucceeded{107, "w2", "w2~a~t1", "artifact"}, Tick{23505}};
    for (std::size_t n = 0; n < oldRows.size(); ++n) {
        legacy.handle(oldRows[n], n + 1);
    }
    CHECK(legacy.units().at({"a", "t1"}).state == State::Failed);
    CHECK_EQ(legacy.units().at({"a", "t1"}).failure->reason, "no result");
    legacy.handle(legacy.record(UnitPublishSucceeded{23506, "w2", "w2~a~t1", hash('b')}), 6);
    CHECK(legacy.units().at({"a", "t1"}).state == State::Published);
    CHECK(!legacy.units().at({"a", "t1"}).failure);
    legacy.handle(legacy.record(UnitBuildStarted{23507, "w2", "w2~a~t2"}), 7);
    CHECK(!legacy.units().at({"a", "t2"}).failure);
    CHECK(!legacy.units().at({"a", "t2"}).retryAfter);
    CHECK(!legacy.units().at({"a", "t2"}).buildEnd);
    legacy.handle(legacy.record(UnitBuildFailed{23508, "w2", "w2~a~t2", "late compiler failure"}), 8);
    CHECK_EQ(*legacy.units().at({"a", "t2"}).buildEnd, 23508);
    CHECK_EQ(*legacy.units().at({"a", "t2"}).retryAfter, 24108);
    CHECK_EQ(legacy.units().at({"a", "t2"}).failure->reason, "late compiler failure");
}

void randomized() {
    std::mt19937 random(0x57a9);
    for (unsigned iteration = 0; iteration < 10000; ++iteration) {
        History h;
        h.load();
        h.apply(Tick{105});
        const auto w = h.wave();
        h.apply(WaveStarted{105, w.id, 42, 1});
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
                                                      {"late", late},
                                                      {"deadlines", deadlines},
                                                      {"graph", graph},
                                                      {"randomized", randomized}};
    CHECK(argc == 2);
    if (argc == 2) {
        scenarios.at(argv[1])();
    }
    return checkResult();
}
