#include "Check.h"
#include "Fixture.h"
#include "core/Status.h"
#include "persistence/EventRow.h"

using namespace coreci;
Pipeline graph(unsigned base) {
    Pipeline::Builder b;
    b.feed({"f", {"owner", "feed", "publish.yml", "trunk"}}).target({"t", "f"});
    b.package({"a", {"owner", "a", "trunk"}, {"owner", "recipes", "build.yml", "trunk"}, {"t"}, {}, false, base});
    return b.build();
}
Command build(unsigned revision, const std::string& version = "1.0.0") {
    return DispatchBuild{"a~t~" + version + "~r" + std::to_string(revision),
                         1,
                         "a",
                         {"owner", "a", "trunk"},
                         "v" + version,
                         *Version::parse(version),
                         revision,
                         "t",
                         {},
                         {"owner", "recipes", "build.yml", "trunk"}};
}
int main() {
    auto engine = fixtureEngine(graph(100), {100, 50, 8, {}});
    std::vector<Event> history{GraphChanged{100, graph(100)}};
    const auto apply = [&](const Event& event) {
        const auto commands = engine.handle(event);
        history.push_back(event);
        Engine replay({100, 50, 8, {}});
        std::uint64_t seq = 0;
        for (const auto& stored : history) {
            const auto row = persistence::encode(stored, ++seq);
            CHECK(std::holds_alternative<persistence::EventRow>(row));
            if (std::holds_alternative<persistence::EventRow>(row)) {
                const auto decoded = persistence::decode(std::get<persistence::EventRow>(row));
                CHECK(std::holds_alternative<Event>(decoded));
                if (std::holds_alternative<Event>(decoded)) {
                    replay.handle(std::get<Event>(decoded));
                }
            }
        }
        CHECK_EQ(statusText(replay), statusText(engine));
        CHECK(replay.units() == engine.units());
        CHECK(replay.pipeline() == engine.pipeline());
        return commands;
    };
    CHECK(apply(TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}) == std::vector<Command>{build(101)});
    const auto inflight = engine.units().at({"a", "t"}).inflight;
    CHECK(apply(GraphChanged{102, graph(50)}).empty());
    CHECK(engine.units().at({"a", "t"}).inflight == inflight);
    CHECK(apply(BuildFailed{103, "a~t~1.0.0~r101", 1, "failed"}).empty());
    CHECK(apply(RetryRequested{104, "a", "t"}) == std::vector<Command>{build(102)});
    const Command publish = DispatchPublish{"a~t~1.0.0~r102",
                                            1,
                                            "a",
                                            *Version::parse("1.0.0"),
                                            102,
                                            "t",
                                            "f",
                                            {"owner", "recipes"},
                                            1,
                                            "artifact",
                                            {"owner", "feed", "publish.yml", "trunk"}};
    CHECK(apply(BuildSucceeded{105, "a~t~1.0.0~r102", 1, "artifact"}) == std::vector<Command>{publish});
    const auto publishing = engine.units().at({"a", "t"}).inflight;
    CHECK(apply(GraphChanged{105, graph(75)}).empty());
    CHECK(engine.units().at({"a", "t"}).inflight == publishing);
    CHECK(apply(PublishSucceeded{106, "a~t~1.0.0~r102", 2, std::string(40, 'b')}).empty());
    CHECK(engine.derive().at({"a", "t"}).settled);
    CHECK(std::holds_alternative<Accept>(engine.classify(GraphChanged{107, graph(200)})));
    CHECK(apply(GraphChanged{107, graph(200)}).empty());
    CHECK(engine.derive().at({"a", "t"}).settled);
    CHECK_EQ(engine.units().at({"a", "t"}).published->revision, 102U);
    CHECK(apply(TagObserved{108, "a", "v1.1.0", std::string(40, 'a'), Origin::Webhook}) == std::vector<Command>{build(201, "1.1.0")});
    CHECK(apply(BuildFailed{109, "a~t~1.1.0~r201", 1, "failed"}).empty());
    CHECK(apply(GraphChanged{110, graph(1000000)}).empty());
    CHECK(apply(RetryRequested{111, "a", "t"}) == std::vector<Command>{build(1000001, "1.1.0")});
    CHECK(apply(BuildFailed{112, "a~t~1.1.0~r1000001", 1, "failed"}).empty());
    CHECK(apply(GraphChanged{113, graph(0)}).empty());
    CHECK(apply(RetryRequested{114, "a", "t"}) == std::vector<Command>{build(1000002, "1.1.0")});
    auto malformed = std::get<persistence::EventRow>(persistence::encode(GraphChanged{115, graph(0)}, 73));
    malformed.graph->packages.front().revisionBase = 1000001;
    const auto invalid = persistence::decode(malformed);
    CHECK(std::holds_alternative<persistence::Invalid>(invalid));
    if (std::holds_alternative<persistence::Invalid>(invalid)) {
        CHECK(std::get<persistence::Invalid>(invalid).reason.starts_with("seq 73:"));
    }
    bool rejected = false;
    try {
        graph(1000001);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CHECK(rejected);
    return checkResult();
}
