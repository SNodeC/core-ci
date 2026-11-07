#include "Check.h"
#include "Fixture.h"
#include "app/StatusJson.h"
#include "core/Status.h"
#include "persistence/EventQueue.h"
#include "persistence/EventRow.h"
#include "persistence/InMemoryEventStore.h"

#include <set>
using namespace coreci;
int main() {
    Engine engine({100, 50, 8, 1});
    std::vector<Event> history;
    std::set<std::string> seen;
    CHECK_EQ(app::statusJson(app::statusSnapshot(engine), 0), "{\"generation\":0,\"packages\":[],\"schema\":1,\"units\":[]}");
    const auto apply = [&](Event event) {
        REQUIRE(std::holds_alternative<Accept>(engine.classify(event)));
        history.push_back(event);
        engine.handle(event);
        for (const auto& [unit, status] : statuses(engine)) {
            seen.insert(status.substr(0, status.find('(')));
        }
        Engine replay({100, 50, 8, 1});
        std::uint64_t seq = 0;
        for (const auto& stored : history) {
            const auto row = std::get<persistence::EventRow>(persistence::encode(stored, ++seq));
            replay.handle(std::get<Event>(persistence::decode(row)));
        }
        CHECK_EQ(app::statusJson(app::statusSnapshot(replay), seq), app::statusJson(app::statusSnapshot(engine), seq));
        return 0;
    };
    CHECK_EQ(apply(GraphChanged{100, testPipeline()}), 0);
    for (const auto* p : {"b", "c", "d", "e", "a"}) {
        CHECK_EQ(apply(TagObserved{101, p, "v1.0.0", std::string(40, 'a'), Origin::Webhook}), 0);
    }
    CHECK_EQ(apply(BuildStarted{102, "a~t1~1.0.0~r1", 10, 1}), 0);
    CHECK(app::statusJson(app::statusSnapshot(engine), 7).find("\"runId\":10") != std::string::npos);
    CHECK_EQ(apply(BuildSucceeded{103, "a~t1~1.0.0~r1", 10, "artifact"}), 0);
    CHECK_EQ(apply(PublishSucceeded{104, "a~t1~1.0.0~r1", 11, std::string(40, 'b')}), 0);
    CHECK(app::statusJson(app::statusSnapshot(engine), 9)
              .find("\"at\":104,\"feedCommit\":\"" + std::string(40, 'b') + "\",\"revision\":1,\"runId\":10") != std::string::npos);
    CHECK_EQ(apply(BuildFailed{105, "a~t2~1.0.0~r1", 12, "bad\n\"\\"}), 0);
    CHECK(app::statusJson(app::statusSnapshot(engine), 10).find("bad\\u000a\\\"\\\\") != std::string::npos);
    CHECK(app::statusJson(app::statusSnapshot(engine), 10).find("\"at\":105,\"reason\":") != std::string::npos);
    CHECK_EQ(apply(BuildSucceeded{106, "b~t1~1.0.0~r1", 13, "artifact"}), 0);
    CHECK_EQ(apply(BuildSucceeded{107, "c~t1~1.0.0~r1", 14, "artifact"}), 0);
    CHECK(app::statusJson(app::statusSnapshot(engine), 12).find("\"position\":1") != std::string::npos);
    CHECK_EQ(apply(PublishFailed{108, "b~t1~1.0.0~r1", 99, "publication failed"}), 0);
    CHECK(engine.units().at({"b", "t1"}).failure->runId == 13);
    CHECK(
        app::statusJson(app::statusSnapshot(engine), 13).find("\"at\":108,\"reason\":\"publication failed\",\"revision\":1,\"runId\":13") !=
        std::string::npos);
    for (const auto* kind :
         {"NoRelease", "Settled", "Waiting", "Ready", "Dispatching", "Building", "AwaitingPublish", "Publishing", "Failed", "Blocked"}) {
        CHECK(seen.contains(kind));
    }
    Pipeline::Builder empty;
    Engine dormant = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    dormant.handle(TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    dormant.handle(GraphChanged{102, empty.build()});
    CHECK_EQ(app::statusJson(app::statusSnapshot(dormant), 99), "{\"generation\":99,\"packages\":[],\"schema\":1,\"units\":[]}");
    persistence::InMemoryEventStore store({std::get<persistence::EventRow>(persistence::encode(GraphChanged{100, testPipeline()}, 40))});
    std::uint64_t generation = 0;
    persistence::EventQueue queue(
        store,
        [] {
            return Engine({100, 50, 8, {}});
        },
        [] {
            return Resumed{101};
        },
        [](const auto&) {
        },
        [](auto, const auto&) {
        },
        [&](auto seq, const auto&, const auto&) {
            generation = seq;
        });
    queue.start(
        [] {
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK_EQ(generation, 41U);
    store.failNextAppend(persistence::InMemoryEventStore::Failure::BeforeStore);
    queue.enqueue(TagObserved{102, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook}, [](auto result) {
        CHECK(std::holds_alternative<persistence::Unavailable>(result));
    });
    CHECK_EQ(generation, 43U);
    return checkResult();
}
