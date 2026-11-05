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
void prime(Engine& e) {
    setup(e);
    apply(e, TagObserved{100, "a", "v1.0.0", "sha", Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    build(e, "a", "t1", {publishCommand("a", "t1")});
    publish(e, "a", "t1", {buildCommand("b", "t1", input("a")), buildCommand("c", "t1", input("a"))});
}

int main() {
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
    CHECK(statusText(e).starts_with("PACKAGE\tTARGET\tRELEASE\tPUBLISHED\tSTATUS\n"));
    return checkResult();
}
