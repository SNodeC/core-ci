#include "Check.h"
#include "adapters/dashboard/Model.h"
#include "persistence/EventQueue.h"
#include "persistence/InMemoryEventStore.h"

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
    CHECK(!queue.engine()->units().at(unit).buildAssigned);
    apply(DispatchAccepted{103, id, Kind::Build, 1});
    CHECK(queue.engine()->units().at(unit).buildAssigned);
    apply(BuildSucceeded{106, id, 77, "artifact"});
    CHECK(!queue.engine()->units().at(unit).publishDispatchedAt);
    CHECK_EQ(dashboard::jobRequests(*queue.engine()).size(), 1U);
    apply(DispatchAccepted{104, id, Kind::Publish, 1});
    CHECK_EQ(dashboard::jobRequests(*queue.engine()).size(), 2U);
    apply(PublishSucceeded{102, id, 88, std::string(40, 'b')});
    CHECK_EQ(dashboard::jobRequests(*queue.engine()).size(), 2U);
    const auto before = queue.engine()->units();
    CHECK_EQ(*before.at(unit).publishDispatchedAt, 106);
    CHECK_EQ(*before.at(unit).publishedAt, 106);
    queue.recover("replay metadata");
    CHECK(before == queue.engine()->units());
    github::LoadedDeclarations loaded{pipeline, {}, {{"t", {"d", "s", "a", "deb"}}}, {{"p", {"Title", std::string(40, 'c')}}}};
    app::JobCache jobs;
    const auto model = nlohmann::json::parse(dashboard::model(*queue.engine(), loaded, jobs, 1, true));
    CHECK_EQ(model.at("packages")[0].at("commit").get<std::string>(), std::string(40, 'c'));
    const auto& view = model.at("units")[0];
    CHECK_EQ(view.at("order").get<unsigned>(), 3U);
    CHECK(view.at("assigned").get<bool>());
    CHECK_EQ(view.at("publishDispatchedAt").get<int>(), 106);
    CHECK_EQ(view.at("publishedAt").get<int>(), 106);
    queue.enqueue(DispatchAccepted{107, id, Kind::Publish, 1}, [](Result result) {
        CHECK(std::holds_alternative<Ignored>(result));
    });
    CHECK(before == queue.engine()->units());
    apply(RebuildRequested{108, "p", "t"});
    const auto next = queue.engine()->units().at(unit);
    CHECK_EQ(next.order, store.rows().back().seq);
    CHECK(!next.buildAssigned && !next.publishDispatchedAt && !next.publishedAt);
    CHECK_EQ(dashboard::jobRequests(*queue.engine()).size(), 1U);
    queue.recover("replay next revision");
    CHECK_EQ(queue.engine()->units().at(unit).order, next.order);
    const auto secondId = queue.engine()->units().at(unit).inflight->buildId;
    apply(BuildSucceeded{109, secondId, 90, "next artifact"});
    apply(DispatchRejected{110, secondId, Kind::Publish, 1, 401, "publisher denied"});
    CHECK(queue.engine()->units().at(unit).failure.has_value());
    CHECK_EQ(dashboard::jobRequests(*queue.engine()).size(), 1U);
    apply(RetryRequested{111, "p", "t"});
    const auto thirdId = queue.engine()->units().at(unit).inflight->buildId;
    apply(BuildSucceeded{112, thirdId, 91, "third artifact"});
    apply(DispatchAccepted{113, thirdId, Kind::Publish, 1});
    apply(PublishFailed{114, thirdId, 92, "validator failed"});
    CHECK(queue.engine()->units().at(unit).failure.has_value());
    CHECK_EQ(dashboard::jobRequests(*queue.engine()).size(), 2U);
    return checkResult();
}
