#include "Check.h"
#include "Fixture.h"
#include "app/Presentation.h"
#include "app/StatusJson.h"
using coreci::app::Presentation;
coreci::app::StatusSnapshot view(const std::string& value) {
    coreci::app::StatusSnapshot result;
    result.units[{"a", "t"}].status = value;
    return result;
}
int main() {
    Presentation dispatch;
    dispatch.observe({{"f1", view("a")}, {"f2", view("b")}});
    CHECK(dispatch.due(100, 60) == std::vector<std::string>({"f1", "f2"}));
    CHECK(dispatch.due(160, 60).empty());
    dispatch.complete("f1", true);
    dispatch.complete("f2", true);
    CHECK(dispatch.due(160, 60).empty());
    dispatch.observe({{"f1", view("a2")}, {"f2", view("b")}});
    CHECK(dispatch.due(160, 60) == std::vector<std::string>{"f1"});
    dispatch.complete("f1", false);
    CHECK(dispatch.due(219, 60).empty());
    CHECK(dispatch.due(220, 60) == std::vector<std::string>{"f1"});
    dispatch.observe({{"f1", view("a3")}, {"f2", view("b")}});
    dispatch.complete("f1", true);
    CHECK(dispatch.due(280, 60) == std::vector<std::string>{"f1"});
    dispatch.complete("f1", true);
    dispatch.observe({{"f1", view("a3")}, {"f2", view("b")}});
    CHECK(dispatch.due(340, 60).empty());
    Presentation restarted;
    restarted.observe({{"f1", view("a3")}, {"f2", view("b")}});
    CHECK(restarted.due(340, 60) == std::vector<std::string>({"f1", "f2"}));
    dispatch.observe({});
    dispatch.complete("f1", true);
    CHECK(dispatch.due(1000, 60).empty());
    auto engine = coreci::fixtureEngine(coreci::testPipeline(), {100, 50, 8, {}});
    engine.handle(coreci::TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), coreci::Origin::Webhook});
    Presentation actual;
    const auto f1 = coreci::app::statusSnapshot(engine, "f1");
    const auto f2 = coreci::app::statusSnapshot(engine, "f2");
    actual.observe({{"f1", f1}, {"f2", f2}});
    CHECK(actual.due(100, 60) == std::vector<std::string>({"f1", "f2"}));
    actual.complete("f1", true);
    actual.complete("f2", true);
    engine.handle(coreci::BuildStarted{102, "a~t1~1.0.0~r1", 1, 1});
    CHECK(coreci::app::statusSnapshot(engine, "f2") == f2);
    actual.observe({{"f1", coreci::app::statusSnapshot(engine, "f1")}, {"f2", coreci::app::statusSnapshot(engine, "f2")}});
    CHECK(actual.due(160, 60) == std::vector<std::string>{"f1"});
    Presentation publishing;
    for (const auto& state : {"PublishDispatching", "Publishing"}) {
        publishing.observe({{"f", view(state)}});
        CHECK(publishing.due(1000, 60).empty());
    }
    publishing.observe({{"f", view("Settled")}});
    CHECK(publishing.due(1001, 60) == std::vector<std::string>{"f"});
    return checkResult();
}
