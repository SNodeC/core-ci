#include "Check.h"
#include "core/Engine.h"
#include "core/Status.h"
#include "persistence/EventRow.h"
#include "persistence/EventSql.h"

using namespace coreci;
using namespace coreci::persistence;

Pipeline graph(bool parent = true) {
    Pipeline::Builder builder;
    builder.feed({"feed", {"owner", "feed", "publish.yml", "main"}}).target({"target", "feed"});
    if (parent) {
        builder.package({"a", {"owner", "a", "main"}, {"owner", "recipes", "build.yml", "main"}, {"target"}, {}});
    }
    builder.package({"b",
                     {"owner", "b", "main"},
                     {"owner", "recipes", "build.yml", "main"},
                     {"target"},
                     parent ? std::vector<std::string>{"a"} : std::vector<std::string>{}});
    return builder.build();
}

int main() {
    Engine engine({1000, 1000, 8, {}});
    std::vector<Event> events;
    std::uint64_t seq = 0;
    const auto apply = [&](Event event) {
        events.push_back(event);
        const auto encoded = encode(event, ++seq);
        CHECK(std::holds_alternative<EventRow>(encoded));
        const auto decoded = decode(std::get<EventRow>(encoded));
        CHECK(std::get<Event>(decoded) == event);
        const auto commands = engine.handle(std::get<Event>(decoded));
        Engine replay({1000, 1000, 8, {}});
        for (const auto& prior : events) {
            replay.handle(std::get<Event>(decode(std::get<EventRow>(encode(prior)))));
        }
        CHECK(replay.units() == engine.units());
        CHECK(replay.feeds() == engine.feeds());
        return commands;
    };
    const auto ignored = [&](Event event) {
        const auto before = engine.units();
        CHECK(std::holds_alternative<Ignore>(engine.classify(event)));
        CHECK(engine.handle(event).empty());
        CHECK(engine.units() == before);
    };
    const auto id = [](const std::string& package, unsigned revision) {
        return package + "~target~1.0.0~r" + std::to_string(revision);
    };
    const auto finish = [&](const std::string& package, unsigned revision) {
        const auto buildId = id(package, revision);
        apply(BuildStarted{100, buildId, 101, 1});
        const auto commands = apply(BuildSucceeded{100, buildId, 101, "artifact"});
        CHECK(commands.size() == 1 && std::holds_alternative<DispatchPublish>(commands.front()));
        return apply(PublishSucceeded{100, buildId, 201, std::string(40, 'b')});
    };
    const Unit a{"a", "target"};
    const Unit b{"b", "target"};
    apply(GraphChanged{100, graph()});
    ignored(RebuildRequested{100, "a", "target"});
    ignored(RebuildRequested{100, "unknown", "target"});
    apply(TagObserved{100, "b", "v1.0.0", std::string(40, 'a'), Origin::Reconcile});
    apply(TagObserved{100, "a", "v1.0.0", std::string(40, 'a'), Origin::Reconcile});
    ignored(RebuildRequested{100, "a", "target"});
    CHECK(finish("a", 1).size() == 1);
    CHECK(finish("b", 1).empty());
    CHECK(engine.derive().at(a).settled && engine.derive().at(b).settled);
    const auto rebuilt = apply(RebuildRequested{100, "a", "target"});
    CHECK(rebuilt.size() == 1 && std::holds_alternative<DispatchBuild>(rebuilt.front()));
    const auto& build = std::get<DispatchBuild>(rebuilt.front());
    CHECK(build.buildId == id("a", 2));
    CHECK(build.version == *Version::parse("1.0.0") && build.revision == 2);
    CHECK(engine.units().at(a).rebuild && !engine.derive().at(a).satisfied);
    ignored(RebuildRequested{100, "a", "target"});
    apply(BuildFailed{100, id("a", 2), 101, "compiler failed"});
    CHECK(engine.units().at(a).rebuild && engine.units().at(a).failure);
    ignored(RebuildRequested{100, "a", "target"});
    const auto retry = apply(RetryRequested{100, "a", "target"});
    CHECK(retry.size() == 1 && std::get<DispatchBuild>(retry.front()).revision == 3);
    const auto dependents = finish("a", 3);
    CHECK(!engine.units().at(a).rebuild);
    CHECK(dependents.size() == 1 && std::get<DispatchBuild>(dependents.front()).package == "b");
    CHECK(std::get<DispatchBuild>(dependents.front()).revision == 2);
    CHECK(engine.units().at(b).inflight->inputs.at("a").revision == 3);
    CHECK(apply(Resumed{100}).size() == 1); // Re-emission preserves the already allocated dependent attempt.
    CHECK(engine.units().at(b).lastRevision.at(*Version::parse("1.0.0")) == 2);
    CHECK(finish("b", 2).empty());
    CHECK(engine.derive().at(a).settled && engine.derive().at(b).settled);
    Engine replay({1000, 1000, 8, {}});
    for (const auto& event : events) {
        replay.handle(event);
    }
    CHECK(replay.units() == engine.units());
    CHECK(replay.feeds() == engine.feeds());
    CHECK(statuses(replay) == statuses(engine));
    CHECK(std::get<std::string>(insertSql(RebuildRequested{100, "a", "target"})).find("52656275696c64526571756573746564") !=
          std::string::npos);
    apply(GraphChanged{100, graph(false)});
    ignored(RebuildRequested{100, "a", "target"});
    return checkResult();
}
