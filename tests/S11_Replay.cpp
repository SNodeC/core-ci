#include "Check.h"
#include "Fixture.h"
#include "core/Status.h"
#include "persistence/InMemoryEventStore.h"

#include <algorithm>
#include <stdexcept>
using namespace coreci;
using namespace coreci::persistence;
std::map<const Engine*, InMemoryEventStore> histories;

Version version(const std::string& v = "1.0.0") {
    return Version::parse(v).value();
}
std::string id(const std::string& p, const std::string& t, const std::string& v = "1.0.0", unsigned r = 1) {
    return p + "~" + t + "~" + v + "~r" + std::to_string(r);
}
Inputs input(const std::string& p, const std::string& v = "1.0.0", unsigned r = 1) {
    return {{p, {version(v), r}}};
}
Command buildCommand(
    const std::string& p, const std::string& t, Inputs inputs = {}, const std::string& v = "1.0.0", unsigned r = 1, unsigned attempt = 1) {
    return DispatchBuild{
        id(p, t, v, r), attempt, p, {"owner", p, "trunk"}, "v" + v, version(v), r, t, inputs, {"owner", "recipes", "build.yml", "trunk"}};
}
Command publishCommand(const std::string& p, const std::string& t, const std::string& v = "1.0.0", unsigned r = 1, unsigned attempt = 1) {
    return DispatchPublish{id(p, t, v, r),
                           attempt,
                           p,
                           version(v),
                           r,
                           t,
                           t == "t1" ? "f1" : "f2",
                           {"owner", "recipes"},
                           101,
                           "artifact",
                           {"owner", "feeds", "publish.yml", "stable"}};
}
int apply(Engine& e, Event event, std::vector<Command> expected = {}) {
    auto& store = histories[&e];
    if (store.rows().empty()) {
        store.append(
            GraphChanged{100, e.pipeline()},
            [](std::uint64_t) {
            },
            [](const auto&) {
                CHECK(false);
            });
    }
    const auto normalized = normalize(event);
    REQUIRE(std::holds_alternative<Event>(normalized));
    const auto canonical = std::get<Event>(normalized);
    const auto count = store.rows().size();
    if (std::holds_alternative<Accept>(e.classify(canonical))) {
        store.append(
            canonical,
            [&](std::uint64_t) {
                CHECK(e.handle(canonical) == expected);
            },
            [](const auto&) {
                CHECK(false);
            });
        CHECK_EQ(store.rows().size(), count + 1);
    } else {
        CHECK(e.handle(canonical) == expected);
        CHECK_EQ(store.rows().size(), count);
    }
    Engine replay({100, 50, 8, {}});
    bool loaded = false;
    store.loadAll(
        [&](std::uint64_t seq, const Event& saved) {
            const auto row = encode(saved, seq);
            REQUIRE(std::holds_alternative<EventRow>(row));
            const auto decoded = decode(std::get<EventRow>(row));
            REQUIRE(std::holds_alternative<Event>(decoded));
            replay.handle(std::get<Event>(decoded));
            return checkResult();
        },
        [&] {
            loaded = true;
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK(loaded);
    CHECK_EQ(statusText(replay), statusText(e));
    CHECK(replay.units() == e.units());
    CHECK(replay.feeds() == e.feeds());
    CHECK(replay.releases() == e.releases());
    CHECK(replay.time() == e.time());
    CHECK(replay.nextDeadline() == e.nextDeadline());
    return checkResult();
}
void setup(Engine& e) {
    for (const auto& p : {"b", "c", "d", "e"}) {
        apply(e, TagObserved{100, p, "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    }
}
void build(
    Engine& e, const std::string& p, const std::string& t, std::vector<Command> expected, const std::string& v = "1.0.0", unsigned r = 1) {
    const auto buildId = id(p, t, v, r);
    apply(e, DispatchAccepted{100, buildId, Kind::Build, 1});
    apply(e, BuildStarted{100, buildId, 101, 1});
    CHECK_EQ(statuses(e).at({p, t}), "Building(run 101)");
    apply(e, BuildSucceeded{100, buildId, 101, "artifact"}, expected);
}
void publish(Engine& e,
             const std::string& p,
             const std::string& t,
             std::vector<Command> expected = {},
             const std::string& v = "1.0.0",
             unsigned r = 1) {
    apply(e, DispatchAccepted{100, id(p, t, v, r), Kind::Publish, 1});
    apply(e, PublishSucceeded{100, id(p, t, v, r), 201, std::string(40, 'c')}, expected);
}
void prime(Engine& e) {
    setup(e);
    apply(e, TagObserved{100, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    build(e, "a", "t1", {publishCommand("a", "t1")});
    publish(e, "a", "t1", {buildCommand("b", "t1", input("a")), buildCommand("c", "t1", input("a"))});
}

int scenario1() {
    histories.clear();
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    prime(e);
    build(e, "b", "t1", {publishCommand("b", "t1")});
    publish(e, "b", "t1");
    CHECK(!e.units().at({"d", "t1"}).inflight);
    build(e, "c", "t1", {publishCommand("c", "t1")});
    const Inputs diamond{{"b", {version(), 1}}, {"c", {version(), 1}}};
    publish(e, "c", "t1", {buildCommand("d", "t1", diamond)});
    CHECK_EQ(statuses(e).at({"d", "t2"}), "Waiting(on b@t2)");
    build(e, "a", "t2", {publishCommand("a", "t2")});
    publish(e, "a", "t2", {buildCommand("b", "t2", input("a")), buildCommand("c", "t2", input("a"))});
    CHECK(e.units().at({"d", "t1"}).inflight.has_value());
    const auto dot = statusDot(e);
    CHECK(dot.find("\"b@t1\" -> \"d@t1\"") != std::string::npos);
    CHECK(dot.find("fillcolor=yellow") != std::string::npos);
    CHECK(statusText(e).starts_with("PACKAGE\tTARGET\tRELEASE\tPUBLISHED\tSTATUS\tSOURCE\n"));
    return checkResult();
}

int scenario2() {
    histories.clear();
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    prime(e);
    build(e, "b", "t1", {publishCommand("b", "t1")});
    build(e, "c", "t1", {});
    CHECK_EQ(statuses(e).at({"c", "t1"}), "AwaitingPublish(position 1 on f1)");
    CHECK((e.feeds().at("f1").active == Unit{"b", "t1"}));
    build(e, "a", "t2", {publishCommand("a", "t2")});
    CHECK((e.feeds().at("f2").active == Unit{"a", "t2"}));
    publish(e, "b", "t1", {publishCommand("c", "t1")});
    CHECK(e.feeds().at("f1").queue.empty());
    CHECK((e.feeds().at("f1").active == Unit{"c", "t1"}));
    return checkResult();
}

int scenario3() {
    histories.clear();
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    setup(e);
    apply(e, TagObserved{100, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    apply(e, DispatchAccepted{100, id("a", "t1"), Kind::Build, 1});
    apply(e, TagObserved{101, "a", "v1.1.0", std::string(40, 'b'), Origin::Webhook});
    apply(e, BuildSucceeded{102, id("a", "t1"), 101, "old"}, {buildCommand("a", "t1", {}, "1.1.0")});
    CHECK(!e.feeds().at("f1").active);
    CHECK(!e.units().at({"a", "t1"}).published);
    Engine q = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    prime(q);
    build(q, "c", "t1", {publishCommand("c", "t1")});
    build(q, "b", "t1", {});
    CHECK_EQ(statuses(q).at({"b", "t1"}), "AwaitingPublish(position 1 on f1)");
    apply(q, TagObserved{101, "b", "v1.1.0", std::string(40, 'b'), Origin::Webhook}, {buildCommand("b", "t1", input("a"), "1.1.0")});
    CHECK(q.feeds().at("f1").queue.empty());
    publish(q, "c", "t1");
    CHECK(!q.units().at({"b", "t1"}).published);
    // Already publishing work records the feed truth even after a newer tag.
    Engine p = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    setup(p);
    apply(p, TagObserved{100, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    build(p, "a", "t1", {publishCommand("a", "t1")});
    apply(p, TagObserved{101, "a", "v1.1.0", std::string(40, 'b'), Origin::Webhook});
    publish(p, "a", "t1", {buildCommand("a", "t1", {}, "1.1.0")});
    CHECK_EQ(p.units().at({"a", "t1"}).published->version.text(), "1.0.0");
    return checkResult();
}

int scenario4() {
    histories.clear();
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    prime(e);
    apply(e, BuildFailed{100, id("b", "t1"), 101, "compiler error"});
    CHECK_EQ(statuses(e).at({"b", "t1"}), "Failed(compiler error)");
    CHECK_EQ(statuses(e).at({"d", "t1"}), "Blocked(by b@t1)");
    CHECK_EQ(statuses(e).at({"e", "t1"}), "Blocked(by b@t1)");
    build(e, "c", "t1", {publishCommand("c", "t1")});
    publish(e, "c", "t1");
    apply(e, RetryRequested{100, "b", "t1"}, {buildCommand("b", "t1", input("a"), "1.0.0", 2)});
    build(e, "b", "t1", {publishCommand("b", "t1", "1.0.0", 2)}, "1.0.0", 2);
    const Inputs diamond{{"b", {version(), 2}}, {"c", {version(), 1}}};
    publish(e, "b", "t1", {buildCommand("d", "t1", diamond)}, "1.0.0", 2);
    CHECK(!e.units().at({"b", "t1"}).failure);
    build(e, "d", "t1", {publishCommand("d", "t1")});
    publish(e, "d", "t1", {buildCommand("e", "t1", input("d"))});
    // Failed publication frees its feed and only explicit retry reopens it.
    build(e, "e", "t1", {publishCommand("e", "t1")});
    apply(e, PublishFailed{100, id("e", "t1"), 201, "signing failed"});
    CHECK(!e.feeds().at("f1").active);
    apply(e, RetryRequested{100, "e", "t1"}, {buildCommand("e", "t1", input("d"), "1.0.0", 2)});
    return checkResult();
}

int scenario5() {
    histories.clear();
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    setup(e);
    apply(e, TagObserved{100, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    apply(e, BuildStarted{101, id("a", "t1"), 101, 1});
    const auto units = e.units();
    const auto feeds = e.feeds();
    const auto releases = e.releases();
    const auto time = e.time();
    const auto text = statusText(e);
    for (const Event& event : std::vector<Event>{BuildStarted{999, id("a", "t1"), 101, 2},
                                                 BuildSucceeded{999, id("a", "t1"), 102, "wrong"},
                                                 BuildFailed{999, "unknown", 101, "wrong"},
                                                 PublishSucceeded{999, id("a", "t1"), 201, std::string(40, 'c')},
                                                 DispatchAccepted{999, id("a", "t1"), Kind::Build, 1}}) {
        CHECK(std::holds_alternative<Ignore>(e.classify(event)));
        apply(e, event);
        CHECK(e.units() == units);
        CHECK(e.feeds() == feeds);
        CHECK(e.releases() == releases);
        CHECK(e.time() == time);
        CHECK_EQ(statusText(e), text);
    }
    apply(e, BuildSucceeded{102, id("a", "t1"), 101, "artifact"}, {publishCommand("a", "t1")});
    apply(e, BuildSucceeded{999, id("a", "t1"), 101, "artifact"});
    publish(e, "a", "t1", {buildCommand("b", "t1", input("a")), buildCommand("c", "t1", input("a"))});
    const auto done = e.units();
    apply(e, PublishSucceeded{999, id("a", "t1"), 201, std::string(40, 'c')});
    CHECK(e.units() == done);
    apply(e, DispatchRejected{103, id("a", "t2"), Kind::Build, 1, 503, "busy"});
    const auto backoff = e.units();
    apply(e, DispatchRejected{999, id("a", "t2"), Kind::Build, 1, 503, "duplicate"});
    CHECK(e.units() == backoff);
    apply(e, Tick{133}, {buildCommand("a", "t2", {}, "1.0.0", 1, 2)});
    apply(e, DispatchRejected{999, id("a", "t2"), Kind::Build, 1, 404, "stale attempt"});
    CHECK_EQ(e.units().at({"a", "t2"}).inflight->attempt, 2U);
    CHECK_EQ(*e.time(), 133);
    return checkResult();
}

int scenario6() {
    histories.clear();
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    setup(e);
    for (const auto& tag : {"V1.0.0",
                            "1.0",
                            "release-1.0.0",
                            "v01.0.0",
                            "01.0.0",
                            "1.01.0",
                            "1.0.01",
                            "1.0.0-01",
                            "1.0.0-a..b",
                            "1.0.0+",
                            "1.0.0-",
                            "vv1.0.0",
                            " 1.0.0",
                            "1.0.0\n"}) {
        CHECK(!Version::parse(tag));
        apply(e, TagObserved{999, "a", tag, std::string(40, 'a'), Origin::Webhook});
    }
    const std::vector<std::string> chain{
        "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta", "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0"};
    for (std::size_t i = 0; i < chain.size(); ++i) {
        REQUIRE(Version::parse(chain[i]));
        if (i > 0) {
            CHECK(version(chain[i - 1]) < version(chain[i]));
        }
    }
    CHECK(version("v1.0.0") == version("1.0.0"));
    CHECK(version("1.0.0") == version("1.0.0+build"));
    CHECK(version("1.0.0+a") == version("1.0.0+b"));
    CHECK_EQ(version("v2.0.1").text(), "2.0.1");
    CHECK(Version::parse("1.0.0+001"));
    CHECK(version("999999999999999999999999.0.0") > version("99999999999999999999999.0.0"));
    apply(e, TagObserved{100, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    const auto state = e.units();
    for (const auto& tag : {"v0.9.0", "v1.0.0", "1.0.0", "1.0.0+a", "v1.1.0-rc.1", "bad"}) {
        CHECK(std::holds_alternative<Ignore>(e.classify(TagObserved{999, "a", tag, std::string(40, 'a'), Origin::Reconcile})));
        apply(e, TagObserved{999, "a", tag, std::string(40, 'a'), Origin::Reconcile});
        CHECK(e.units() == state);
        CHECK_EQ(*e.time(), 100);
    }
    Pipeline::Builder builder;
    builder.feed({"f", {"o", "feed", "pub.yml", "stable"}})
        .target({"t", "f"})
        .package({"p", {"o", "p", "trunk"}, {"o", "recipes", "build.yml", "trunk"}, {"t"}, {}, true});
    Engine pre = fixtureEngine(builder.build(), {100, 50, 8, {}});
    const DispatchBuild expected{"p~t~1.0.0-rc.1~r1",
                                 1,
                                 "p",
                                 {"o", "p", "trunk"},
                                 "v1.0.0-rc.1",
                                 version("1.0.0-rc.1"),
                                 1,
                                 "t",
                                 {},
                                 {"o", "recipes", "build.yml", "trunk"}};
    apply(pre, TagObserved{100, "p", "v1.0.0-rc.1", std::string(40, 'a'), Origin::Reconcile}, {expected});
    return checkResult();
}

int scenario7() {
    histories.clear();
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    prime(e);
    const Inputs diamond{{"b", {version(), 1}}, {"c", {version(), 1}}};
    build(e, "b", "t1", {publishCommand("b", "t1")});
    publish(e, "b", "t1");
    build(e, "c", "t1", {publishCommand("c", "t1")});
    publish(e, "c", "t1", {buildCommand("d", "t1", diamond)});
    build(e, "d", "t1", {publishCommand("d", "t1")});
    publish(e, "d", "t1", {buildCommand("e", "t1", input("d"))});
    build(e, "e", "t1", {publishCommand("e", "t1")});
    publish(e, "e", "t1");
    build(e, "a", "t2", {publishCommand("a", "t2")});
    publish(e, "a", "t2", {buildCommand("b", "t2", input("a")), buildCommand("c", "t2", input("a"))});
    build(e, "b", "t2", {publishCommand("b", "t2")});
    publish(e, "b", "t2");
    build(e, "c", "t2", {publishCommand("c", "t2")});
    publish(e, "c", "t2", {buildCommand("d", "t2", diamond)});
    build(e, "d", "t2", {publishCommand("d", "t2")});
    publish(e, "d", "t2");
    for (const auto& [u, d] : e.derive()) {
        CHECK(d.settled);
    }
    apply(e,
          TagObserved{101, "a", "v1.0.1", std::string(40, 'b'), Origin::Webhook},
          {buildCommand("a", "t1", {}, "1.0.1"), buildCommand("a", "t2", {}, "1.0.1")});
    build(e, "a", "t1", {publishCommand("a", "t1", "1.0.1")}, "1.0.1");
    publish(e,
            "a",
            "t1",
            {buildCommand("b", "t1", input("a", "1.0.1"), "1.0.0", 2), buildCommand("c", "t1", input("a", "1.0.1"), "1.0.0", 2)},
            "1.0.1");
    CHECK_EQ(e.units().at({"b", "t1"}).inflight->revision, 2U);
    CHECK_EQ(e.units().at({"b", "t1"}).inflight->version.text(), "1.0.0");
    CHECK(!e.units().at({"d", "t1"}).inflight);
    return checkResult();
}

int scenario8() {
    histories.clear();
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    prime(e);
    apply(e, TagObserved{101, "d", "v2.0.0", std::string(40, 'b'), Origin::Webhook});
    build(e, "b", "t1", {publishCommand("b", "t1")});
    publish(e, "b", "t1");
    CHECK(!e.units().at({"d", "t1"}).inflight);
    build(e, "c", "t1", {publishCommand("c", "t1")});
    const Inputs diamond{{"b", {version(), 1}}, {"c", {version(), 1}}};
    publish(e, "c", "t1", {buildCommand("d", "t1", diamond, "2.0.0")});
    apply(e, PublishSucceeded{999, id("c", "t1"), 201, std::string(40, 'c')});
    CHECK_EQ(e.units().at({"d", "t1"}).lastRevision.at(version("2.0.0")), 1U);
    return checkResult();
}

int main() {
    scenario1();
    scenario2();
    scenario3();
    scenario4();
    scenario5();
    scenario6();
    scenario7();
    scenario8();
    histories.clear();
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    prime(e);
    apply(e, DispatchRejected{101, id("b", "t1"), Kind::Build, 1, 503, "busy"});
    apply(e, Resumed{102}, {buildCommand("c", "t1", input("a")), buildCommand("a", "t2")});
    build(e, "c", "t1", {publishCommand("c", "t1")});
    apply(e, Resumed{103}, {publishCommand("c", "t1"), buildCommand("a", "t2")});
    // Gaps and ordering use actual row sequence numbers, never callback counts.
    InMemoryEventStore gaps;
    gaps.failNextAppend(InMemoryEventStore::Failure::BeforeStore);
    bool failed = false;
    gaps.append(
        Resumed{1},
        [](std::uint64_t) {
            CHECK(false);
        },
        [&](const auto&) {
            failed = true;
        });
    CHECK(failed);
    gaps.append(
        Resumed{2},
        [](std::uint64_t) {
        },
        [](const auto&) {
            CHECK(false);
        });
    std::vector<std::uint64_t> sequences;
    gaps.loadAll(
        [&](std::uint64_t seq, const Event&) {
            sequences.push_back(seq);
        },
        [] {
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK((sequences == std::vector<std::uint64_t>{2}));
    auto low = std::get<EventRow>(encode(Resumed{7}, 7));
    auto high = std::get<EventRow>(encode(Resumed{50}, 50));
    InMemoryEventStore unordered({high, low});
    sequences.clear();
    unordered.loadAll(
        [&](std::uint64_t seq, const Event&) {
            sequences.push_back(seq);
        },
        [] {
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK((sequences == std::vector<std::uint64_t>{7, 50}));
    return checkResult();
}
