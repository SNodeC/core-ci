#include "Check.h"
#include "Fixture.h"
#include "core/Status.h"
#include "persistence/InMemoryEventStore.h"
using namespace coreci;
using namespace coreci::persistence;
Pipeline graph(bool edge = false,
               bool remove = false,
               bool extra = false,
               std::string feed = "f",
               std::string source = "a",
               std::string workflow = "build.yml",
               std::string publisher = "publish.yml") {
    Pipeline::Builder b;
    b.feed({"f", {"o", "publisher", publisher, "default"}}).feed({"g", {"o", "publisher", "publish.yml", "default"}}).target({"t", feed});
    if (extra) {
        b.target({"u", feed});
    }
    if (!remove) {
        b.package({"a",
                   {"o", source, "trunk"},
                   {"o", "recipes", workflow, "trunk"},
                   extra ? std::set<std::string>{"t", "u"} : std::set<std::string>{"t"},
                   {}});
    }
    b.package({"b",
               {"o", "b", "trunk"},
               {"o", "recipes", "build.yml", "trunk"},
               {"t"},
               edge ? std::vector<std::string>{"a"} : std::vector<std::string>{}});
    return b.build();
}
Command
build(const std::string& name, unsigned revision = 1, Inputs inputs = {}, std::string target = "t", std::string workflow = "build.yml") {
    return DispatchBuild{name + "~" + target + "~1.0.0~r" + std::to_string(revision),
                         1,
                         name,
                         {"o", name, "trunk"},
                         "v1.0.0",
                         *Version::parse("1.0.0"),
                         revision,
                         target,
                         inputs,
                         {"o", "recipes", workflow, "trunk"}};
}
Command publish(const std::string& name, unsigned revision = 1) {
    return DispatchPublish{name + "~t~1.0.0~r" + std::to_string(revision),
                           1,
                           name,
                           *Version::parse("1.0.0"),
                           revision,
                           "t",
                           "f",
                           {"o", "recipes"},
                           1,
                           "artifact",
                           {"o", "publisher", "publish.yml", "default"}};
}
int main() {
    Engine e({100, 50, 8, {}});
    std::vector<EventRow> history;
    const auto apply = [&](Event event, std::vector<Command> expected = {}) {
        if (std::holds_alternative<Accept>(e.classify(event))) {
            const auto encoded = encode(event, history.size() + 1);
            CHECK(std::holds_alternative<EventRow>(encoded));
            history.push_back(std::get<EventRow>(encoded));
        }
        CHECK(e.handle(event) == expected);
        Engine replay({100, 50, 8, {}});
        for (const auto& row : history) {
            const auto decoded = decode(row);
            CHECK(std::holds_alternative<Event>(decoded));
            replay.handle(std::get<Event>(decoded));
        }
        CHECK_EQ(statusText(replay), statusText(e));
        CHECK(replay.pipeline() == e.pipeline());
        CHECK(replay.units() == e.units());
        CHECK(replay.time() == e.time());
    };
    apply(GraphChanged{1, graph()});
    apply(GraphChanged{999, graph()});
    CHECK_EQ(*e.time(), 1);
    for (const auto& name : {"a", "b"}) {
        apply(TagObserved{2, name, "v1.0.0", std::string(40, 'a'), Origin::Webhook}, {build(name)});
        apply(BuildSucceeded{3, std::string(name) + "~t~1.0.0~r1", 1, "artifact"}, {publish(name)});
        apply(PublishSucceeded{4, std::string(name) + "~t~1.0.0~r1", 2, std::string(40, 'b')});
    }
    const Inputs a{{"a", {*Version::parse("1.0.0"), 1}}};
    apply(GraphChanged{5, graph(true)}, {build("b", 2, a)});
    apply(GraphChanged{6, graph(true, false, false, "f", "a", "next.yml")}); // b keeps its workflow
    apply(BuildSucceeded{7, "b~t~1.0.0~r2", 1, "artifact"}, {publish("b", 2)});
    const auto before = statusText(e);
    for (const auto& changed : {graph(true, false, false, "g"), graph(true, false, false, "f", "a", "build.yml", "next.yml")}) {
        apply(GraphChanged{8, changed});
        CHECK_EQ(statusText(e), before);
    }
    apply(PublishSucceeded{9, "b~t~1.0.0~r2", 2, std::string(40, 'b')});
    apply(GraphChanged{10, graph(false, false, true, "f", "a", "next.yml")}, {build("b", 3), build("a", 1, {}, "u", "next.yml")});
    apply(GraphChanged{11, graph(false, true)});
    CHECK(!statuses(e).contains({"a", "u"}));
    CHECK(!e.units().at({"a", "u"}).inflight);
    apply(BuildSucceeded{12, "a~u~1.0.0~r1", 1, "artifact"});
    apply(GraphChanged{13, graph(false, false, true)}, {build("a", 2, {}, "u")});
    apply(GraphChanged{14, graph(false, false, true, "f", "a", "next.yml")}, {build("a", 3, {}, "u", "next.yml")});
    apply(GraphChanged{15, graph(false, false, true, "g", "a", "next.yml")}, {build("a", 2, {}, "t", "next.yml")});
    CHECK(!e.units().at({"a", "t"}).published);
    apply(GraphChanged{16, graph(false, false, true, "g", "other", "next.yml")});
    CHECK(!e.releases().at("a"));
    CHECK(!e.units().at({"a", "u"}).inflight);
    // Publication phases protect removal and source/build identity; a queued unit protects its feed too.
    for (const auto phase : {Phase::AwaitingPublish, Phase::PublishDispatching, Phase::Publishing}) {
        Engine p = fixtureEngine(graph(), {100, 50, 8, {}});
        p.handle(TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
        p.handle(TagObserved{101, "b", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
        if (phase == Phase::AwaitingPublish) {
            p.handle(BuildSucceeded{102, "b~t~1.0.0~r1", 1, "artifact"});
        }
        p.handle(BuildSucceeded{102, "a~t~1.0.0~r1", 1, "artifact"});
        if (phase == Phase::Publishing) {
            p.handle(DispatchAccepted{103, "a~t~1.0.0~r1", Kind::Publish, 1});
        }
        for (const auto& changed : {graph(false, true),
                                    graph(false, false, false, "g"),
                                    graph(false, false, false, "f", "other"),
                                    graph(false, false, false, "f", "a", "next.yml")}) {
            CHECK(std::holds_alternative<Ignore>(p.classify(GraphChanged{104, changed})));
            CHECK(p.handle(GraphChanged{104, changed}).empty());
        }
    }
    Engine identity = fixtureEngine(graph(), {100, 50, 8, {}});
    CHECK(identity.handle(TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}) == std::vector<Command>{build("a")});
    identity.handle(DispatchAccepted{102, "a~t~1.0.0~r1", Kind::Build, 1});
    CHECK(identity.handle(GraphChanged{103, graph(false, false, false, "f", "a", "next.yml")}) ==
          std::vector<Command>{build("a", 2, {}, "t", "next.yml")});
    CHECK(identity.handle(BuildSucceeded{104, "a~t~1.0.0~r1", 1, "stale"}).empty());
    identity.handle(BuildFailed{105, "a~t~1.0.0~r2", 1, "failure"});
    CHECK(identity.units().at({"a", "t"}).failure.has_value());
    CHECK(identity.handle(GraphChanged{106, graph(false, false, false, "g", "a", "next.yml")}) ==
          std::vector<Command>{build("a", 3, {}, "t", "next.yml")});
    CHECK(!identity.units().at({"a", "t"}).failure);
    // Dormant metadata survives replay and is the comparison baseline on reappearance.
    for (const auto mode : {0, 1, 2}) {
        std::vector<Event> events{GraphChanged{100, graph()},
                                  TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook},
                                  BuildSucceeded{102, "a~t~1.0.0~r1", 1, "artifact"},
                                  PublishSucceeded{103, "a~t~1.0.0~r1", 2, std::string(40, 'b')},
                                  GraphChanged{104, graph(false, true)}};
        Engine dormant({100, 50, 8, {}});
        for (const auto& event : events) {
            dormant.handle(event);
        }
        CHECK(!statuses(dormant).contains({"a", "t"}));
        const auto next = mode == 1 ? graph(false, false, false, "g") : (mode == 2 ? graph(false, false, false, "f", "other") : graph());
        const auto commands = dormant.handle(GraphChanged{105, next});
        CHECK(commands == (mode == 1 ? std::vector<Command>{build("a", 2)} : std::vector<Command>{}));
        CHECK(dormant.units().at({"a", "t"}).published.has_value() == (mode != 1));
        CHECK(dormant.releases().at("a").has_value() == (mode != 2));
        Engine replay({100, 50, 8, {}});
        for (const auto& event : events) {
            const auto row = std::get<EventRow>(encode(event, 1));
            replay.handle(std::get<Event>(decode(row)));
        }
        CHECK(replay.handle(GraphChanged{105, next}) == commands);
        CHECK(replay.units() == dormant.units());
        CHECK_EQ(statusText(replay), statusText(dormant));
    }
    Engine abandoned = fixtureEngine(graph(), {100, 50, 8, {}});
    abandoned.handle(TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    abandoned.handle(GraphChanged{102, graph(false, true)});
    CHECK(abandoned.handle(GraphChanged{103, graph(false, false, false, "f", "a", "next.yml")}) ==
          std::vector<Command>{build("a", 2, {}, "t", "next.yml")});
    Pipeline::Builder collision;
    collision.feed({"f", {"o", "publisher", "publish.yml", "master"}})
        .target({"c", "f"})
        .target({"b.c", "f"})
        .package({"a.b", {"o", "ab", "master"}, {"o", "recipes", "build.yml", "master"}, {"c"}, {}})
        .package({"a", {"o", "a", "master"}, {"o", "recipes", "build.yml", "master"}, {"b.c"}, {}});
    Engine distinct = fixtureEngine(collision.build(), {100, 50, 8, {}});
    auto one = distinct.handle(TagObserved{101, "a.b", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    auto two = distinct.handle(TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    REQUIRE(one.size() == 1 && two.size() == 1);
    CHECK_EQ(std::get<DispatchBuild>(one[0]).buildId, "a.b~c~1.0.0~r1");
    CHECK_EQ(std::get<DispatchBuild>(two[0]).buildId, "a~b.c~1.0.0~r1");
    CHECK_EQ(distinct.handle(BuildSucceeded{102, "a~b.c~1.0.0~r1", 1, "artifact"}).size(), 1U);
    CHECK(distinct.units().at({"a.b", "c"}).inflight->phase == Phase::Dispatching);
    Engine limited = fixtureEngine(graph(), {100, 50, 8, 1});
    limited.handle(TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    limited.handle(TagObserved{101, "b", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    CHECK(limited.handle(GraphChanged{102, graph(false, true)}) == std::vector<Command>{build("b")});
    return checkResult();
}
