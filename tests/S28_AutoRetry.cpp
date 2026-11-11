#include "Check.h"
#include "app/StatusJson.h"
#include "core/Status.h"
#include "persistence/EventRow.h"

#include <algorithm>

using namespace coreci;

Pipeline graph(bool dependent = false) {
    Pipeline::Builder builder;
    builder.feed({"feed", {"owner", "feed", "publish.yml", "main"}}).target({"target", "feed"});
    builder.package({"a", {"owner", "a", "main"}, {"owner", "recipes", "build.yml", "main"}, {"target"}, {}});
    builder.package({"b",
                     {"owner", "b", "main"},
                     {"owner", "recipes", "build.yml", "main"},
                     {"target"},
                     dependent ? std::vector<std::string>{"a"} : std::vector<std::string>{}});
    return builder.build();
}

class Scenario {
public:
    explicit Scenario(Options options = {100, 50, 1, {}, 10}, bool dependent = false)
        : options(options)
        , engine(options) {
        apply(GraphChanged{100, graph(dependent)});
    }
    std::vector<Command> apply(Event event) {
        const bool accepted = std::holds_alternative<Accept>(engine.classify(event));
        const auto commands = engine.handle(event, events.size() + 1);
        if (accepted) {
            events.push_back(event);
        } else {
            CHECK(commands.empty());
        }
        Engine replay(options);
        std::uint64_t seq = 0;
        std::vector<Command> replayCommands;
        for (const auto& stored : events) {
            const auto row = std::get<persistence::EventRow>(persistence::encode(stored, ++seq));
            replayCommands = replay.handle(std::get<Event>(persistence::decode(row)), seq);
        }
        if (accepted) {
            CHECK(commands == replayCommands);
        }
        CHECK(engine.units() == replay.units());
        CHECK(engine.feeds() == replay.feeds());
        CHECK(engine.time() == replay.time());
        CHECK(engine.nextDeadline() == replay.nextDeadline());
        CHECK(app::statusJson(app::statusSnapshot(engine), seq) == app::statusJson(app::statusSnapshot(replay), seq));
        return commands;
    }
    std::string start(const std::string& package = "a", std::int64_t at = 100, const std::string& tag = "v1.0.0") {
        const auto commands = apply(TagObserved{at, package, tag, std::string(40, 'a'), Origin::Webhook});
        CHECK(commands.size() == 1);
        return engine.units().at({package, "target"}).inflight->buildId;
    }
    void publish(const std::string& id, std::int64_t at) {
        apply(BuildSucceeded{at, id, 101, "artifact"});
        apply(PublishSucceeded{at, id, 201, std::string(40, 'b')});
    }
    Options options;
    Engine engine;
    std::vector<Event> events;
};

int main() {
    const Unit a{"a", "target"}, b{"b", "target"};
    for (const auto mode : {"build", "publish", "build-timeout", "publish-timeout", "dispatch"}) {
        Scenario s;
        const auto id = s.start();
        if (std::string(mode).starts_with("publish")) {
            s.apply(BuildSucceeded{101, id, 101, "artifact"});
        }
        std::int64_t failedAt = 110;
        if (std::string(mode).ends_with("timeout")) {
            const bool build = std::string(mode) == "build-timeout";
            s.apply(DispatchAccepted{110, id, build ? Kind::Build : Kind::Publish, 1});
            failedAt += build ? 100 : 50;
            s.apply(Tick{failedAt});
        } else if (std::string(mode) == "dispatch") {
            s.apply(DispatchRejected{failedAt, id, Kind::Build, 1, 503, "busy"});
        } else if (std::string(mode) == "publish") {
            s.apply(PublishFailed{failedAt, id, 201, "signing"});
        } else {
            s.apply(BuildFailed{failedAt, id, 101, "compiler"});
        }
        CHECK(s.engine.units().at(a).retryAfter == failedAt + 10);
        CHECK(s.apply(Tick{failedAt + 9}).empty());
        CHECK(statuses(s.engine).at(a).starts_with("Failed("));
        CHECK(app::statusJson(app::statusSnapshot(s.engine), 0).find("\"status\":\"Failed\"") != std::string::npos);
        CHECK(s.apply(Resumed{failedAt + 9}).empty());
        const auto retry = s.apply(Tick{failedAt + 10});
        CHECK(retry.size() == 1 && std::get<DispatchBuild>(retry.front()).revision == 2);
        const auto next = s.engine.units().at(a).inflight->buildId;
        s.apply(BuildFailed{failedAt + 11, next, 102, "again"});
        CHECK(!s.engine.units().at(a).retryAfter);
        CHECK(s.apply(Tick{failedAt + 100}).empty());
        CHECK(statuses(s.engine).at(a).starts_with("Failed("));
        const auto newId = s.start("a", failedAt + 101, "v2.0.0");
        s.apply(BuildFailed{failedAt + 102, newId, 103, "new version"});
        CHECK(s.apply(Tick{failedAt + 112}).size() == 1);
    }
    for (const auto code : {401, 403, 404, 422}) {
        Scenario s;
        s.apply(DispatchRejected{101, s.start(), Kind::Build, 1, code, "permanent"});
        CHECK(!s.engine.nextDeadline());
        CHECK(s.apply(Tick{1000}).empty());
        CHECK(statuses(s.engine).at(a).starts_with("Failed("));
    }
    Scenario disabled({100, 50, 1, {}, 0});
    disabled.apply(BuildFailed{101, disabled.start(), 101, "disabled"});
    CHECK(!disabled.engine.nextDeadline());
    CHECK(disabled.apply(Tick{1000}).empty());

    Scenario manual;
    manual.apply(BuildFailed{101, manual.start(), 101, "compiler"});
    const auto requested = manual.apply(RetryRequested{105, "a", "target"});
    CHECK(requested.size() == 1 && std::get<DispatchBuild>(requested.front()).revision == 2);
    CHECK(!manual.engine.units().at(a).retryAfter && !manual.engine.units().at(a).autoRetried);
    CHECK(manual.apply(Tick{111}).empty());

    Scenario limited({100, 50, 1, 1, 10});
    limited.apply(BuildFailed{101, limited.start(), 101, "compiler"});
    const auto busy = limited.start("b", 102);
    CHECK(limited.apply(Tick{111}).empty());
    CHECK_EQ(statuses(limited.engine).at(a), "Ready");
    CHECK(!limited.engine.units().at(a).failure && limited.engine.units().at(a).autoRetried);
    CHECK(app::statusSnapshot(limited.engine).units.at(a).status == "Ready");
    const auto freed = limited.apply(BuildSucceeded{112, busy, 102, "artifact"});
    CHECK(std::count_if(freed.begin(), freed.end(), [](const Command& command) {
              return std::holds_alternative<DispatchBuild>(command) && std::get<DispatchBuild>(command).revision == 2;
          }) == 1);

    Scenario inputs({100, 50, 1, {}, 10}, true);
    inputs.publish(inputs.start(), 101);
    inputs.apply(BuildFailed{103, inputs.start("b", 102), 101, "compiler"});
    inputs.apply(RebuildRequested{104, "a", "target"});
    inputs.publish(inputs.engine.units().at(a).inflight->buildId, 105);
    CHECK(!inputs.engine.units().at(b).retryAfter);
    CHECK(inputs.engine.units().at(b).inflight->inputs.at("a").revision == 2);
    inputs.apply(BuildFailed{106, inputs.engine.units().at(b).inflight->buildId, 102, "new inputs"});
    CHECK(inputs.apply(Tick{116}).size() == 1);

    Scenario dormant;
    dormant.apply(BuildFailed{101, dormant.start(), 101, "compiler"});
    dormant.apply(GraphChanged{102, Pipeline::Builder{}.build()});
    CHECK(!dormant.engine.nextDeadline());
    CHECK(dormant.apply(Tick{111}).empty());
    return checkResult();
}
