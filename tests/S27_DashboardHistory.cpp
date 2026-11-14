#include "Check.h"
#include "adapters/dashboard/Model.h"
#include "persistence/EventQueue.h"
#include "tests/InMemoryEventStore.h"

#include <nlohmann/json.hpp>
using namespace coreci;
using namespace coreci::persistence;
int main() {
    const auto pipeline = Pipeline::Builder()
                              .feed({"f", {"o", "worker", "publish.yml", "main"}})
                              .target({"t", "f"})
                              .package({"p", {"o", "source", "main"}, {"o", "worker", "build.yml", "main"}, {"t"}, {}})
                              .build();
    InMemoryEventStore store;
    store.append(
        GraphChanged{100, pipeline},
        [](auto) {
        },
        [](const auto&) {
            CHECK(false);
        });
    EventQueue queue(
        store,
        [] {
            return Engine({100, 50, 8, {}});
        },
        [] {
            return Resumed{104};
        },
        [](const auto&) {
        },
        [](auto, const auto&) {
        });
    queue.start(
        [] {
        },
        [](const auto&) {
            CHECK(false);
        });
    const auto apply = [&](const Event& event) {
        queue.enqueue(event, [](Result result) {
            CHECK(std::holds_alternative<Accepted>(result));
        });
    };
    const Unit unit{"p", "t"};
    const std::string id = "p~t~1.0.0~r1";
    apply(TagObserved{105, "p", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    CHECK_EQ(queue.engine()->units().at(unit).order, store.rows().back().seq);
    CHECK(!queue.engine()->units().at(unit).build.dispatchedAt);
    apply(DispatchAccepted{103, id, Kind::Build, 1});
    CHECK(queue.engine()->units().at(unit).build.dispatchedAt);
    apply(BuildStarted{105, id, 77, 3});
    apply(BuildSucceeded{106, id, 77, "artifact"});
    CHECK(!queue.engine()->units().at(unit).publish.dispatchedAt);
    apply(DispatchAccepted{104, id, Kind::Publish, 1});
    apply(PublishStarted{110, id, 88, 2});
    apply(PublishSucceeded{120, id, 88, std::string(40, 'b')});
    const auto before = queue.engine()->units();
    CHECK_EQ(*before.at(unit).publish.dispatchedAt, 106);
    CHECK_EQ(*before.at(unit).publish.end, 120);
    queue.recover("replay metadata");
    CHECK(before == queue.engine()->units());
    github::LoadedDeclarations loaded{pipeline, {}, {{"t", {"d", "s", "a", "deb"}}}, {{"p", {"Title", std::string(40, 'c')}}}};
    const auto model = nlohmann::json::parse(dashboard::model(*queue.engine(), loaded, 1, true));
    CHECK_EQ(model.at("packages")[0].at("commit").get<std::string>(), std::string(40, 'c'));
    const auto& view = model.at("units")[0];
    CHECK_EQ(view.at("order").get<unsigned>(), 3U);
    CHECK(view.at("assigned").get<bool>());
    CHECK_EQ(view.at("publishDispatchedAt").get<int>(), 106);
    CHECK_EQ(view.at("publishedAt").get<int>(), 120);
    CHECK(view.at("build").at("start") == 105 && view.at("build").at("end") == 106);
    CHECK(view.at("build").at("duration") == 1 && view.at("build").at("state") == "succeeded");
    CHECK(view.at("build").at("html_url") == "https://github.com/o/worker/actions/runs/77/attempts/3");
    CHECK(view.at("publish").at("start") == 110 && view.at("publish").at("end") == 120);
    CHECK(view.at("publish").at("duration") == 10 && view.at("publish").at("state") == "succeeded");
    CHECK(view.at("publish").at("html_url") == "https://github.com/o/worker/actions/runs/88/attempts/2");
    queue.enqueue(DispatchAccepted{107, id, Kind::Publish, 1}, [](Result result) {
        CHECK(std::holds_alternative<Ignored>(result));
    });
    CHECK(before == queue.engine()->units());
    apply(RebuildRequested{121, "p", "t"});
    const auto next = queue.engine()->units().at(unit);
    CHECK_EQ(next.order, store.rows().back().seq);
    CHECK(!next.build.dispatchedAt && !next.publish.dispatchedAt && !next.publish.end);
    queue.recover("replay next revision");
    CHECK_EQ(queue.engine()->units().at(unit).order, next.order);
    const auto nextId = "p~t~1.0.0~r2";
    apply(BuildSucceeded{122, nextId, 90, "artifact"});
    CHECK(!queue.engine()->units().at(unit).build.start);
    apply(PublishStarted{123, nextId, 99, 4});
    CHECK(queue.engine()->units().at(unit).publish.start == 123);
    CHECK(queue.engine()->units().at(unit).publish.runId == 99);
    CHECK(queue.engine()->units().at(unit).publish.runAttempt == 4);
    const auto running = nlohmann::json::parse(dashboard::model(*queue.engine(), loaded, 1, true)).at("units")[0];
    CHECK(running.at("publish").at("state") == "running" && running.at("publish").at("end").is_null());
    for (const Event& event : std::vector<Event>{PublishStarted{124, nextId, 99, 5},
                                                 DispatchAccepted{124, nextId, Kind::Publish, 1},
                                                 PublishFailed{124, nextId, 100, "wrong run"}}) {
        queue.enqueue(event, [](Result result) {
            CHECK(std::holds_alternative<Ignored>(result));
        });
    }
    apply(PublishSucceeded{125, nextId, 99, std::string(40, 'b')});
    const auto finished = queue.engine()->units();
    queue.recover("phase callback before dispatch");
    CHECK(queue.engine()->units() == finished);
    return checkResult();
}
