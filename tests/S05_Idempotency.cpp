#include "Check.h"
#include "Fixture.h"
#include "core/Status.h"

#include <algorithm>
#include <stdexcept>
using namespace coreci;

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
void apply(Engine& e, Event event, std::vector<Command> expected = {}) {
    CHECK(e.handle(event) == expected);
}
void setup(Engine& e) {
    for (const auto& p : {"b", "c", "d", "e"}) {
        apply(e, TagObserved{100, p, "v1.0.0", "sha", Origin::Webhook});
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
    apply(e, PublishSucceeded{100, id(p, t, v, r), 201, "commit"}, expected);
}
int main() {
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    setup(e);
    apply(e, TagObserved{100, "a", "v1.0.0", "sha", Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    apply(e, BuildStarted{101, id("a", "t1"), 101, 1});
    const auto units = e.units();
    const auto feeds = e.feeds();
    const auto releases = e.releases();
    const auto time = e.time();
    const auto text = statusText(e);
    for (const Event& event : std::vector<Event>{BuildStarted{999, id("a", "t1"), 101, 2},
                                                 BuildSucceeded{999, id("a", "t1"), 102, "wrong"},
                                                 BuildFailed{999, "unknown", 101, "wrong"},
                                                 PublishSucceeded{999, id("a", "t1"), 201, "wrong"},
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
    apply(e, PublishSucceeded{999, id("a", "t1"), 201, "commit"});
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
