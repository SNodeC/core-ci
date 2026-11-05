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
    apply(e, BuildFailed{100, id("b", "t1"), 101, "compiler error"});
    CHECK_EQ(statuses(e).at({"b", "t1"}), "Failed(compiler error)");
    CHECK_EQ(statuses(e).at({"d", "t1"}), "Blocked(by b@t1)");
    CHECK_EQ(statuses(e).at({"e", "t1"}), "Blocked(by b@t1)");
    build(e, "c", "t1", {publishCommand("c", "t1")});
    publish(e, "c", "t1");
    apply(e, RetryRequested{100, "b", "t1"}, {buildCommand("b", "t1", input("a"), "1.0.0", 2)});
    build(e, "b", "t1", {publishCommand("b", "t1", "1.0.0", 2)}, "1.0.0", 2);
    const Inputs diamond{{"b", {version(), 2}}, {"c", {version(), 1}}};
    publish(e, "b", "t1", {buildCommand("d", "t1", diamond)}, "1.0.0", 2);
    CHECK(!e.units().at({"b", "t1"}).failure);
    build(e, "d", "t1", {publishCommand("d", "t1")});
    publish(e, "d", "t1", {buildCommand("e", "t1", input("d"))});
    // Failed publication frees its feed and only explicit retry reopens it.
    build(e, "e", "t1", {publishCommand("e", "t1")});
    apply(e, PublishFailed{100, id("e", "t1"), 201, "signing failed"});
    CHECK(!e.feeds().at("f1").active);
    apply(e, RetryRequested{100, "e", "t1"}, {buildCommand("e", "t1", input("d"), "1.0.0", 2)});
    return checkResult();
}
