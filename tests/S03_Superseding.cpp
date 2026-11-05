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
    setup(e);
    apply(e, TagObserved{100, "a", "v1.0.0", "sha", Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    apply(e, DispatchAccepted{100, id("a", "t1"), Kind::Build, 1});
    apply(e, TagObserved{101, "a", "v1.1.0", "sha2", Origin::Webhook});
    apply(e, BuildSucceeded{102, id("a", "t1"), 101, "old"}, {buildCommand("a", "t1", {}, "1.1.0")});
    CHECK(!e.feeds().at("f1").active);
    CHECK(!e.units().at({"a", "t1"}).published);
    Engine q = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    prime(q);
    build(q, "c", "t1", {publishCommand("c", "t1")});
    build(q, "b", "t1", {});
    CHECK_EQ(statuses(q).at({"b", "t1"}), "AwaitingPublish(position 1 on f1)");
    apply(q, TagObserved{101, "b", "v1.1.0", "sha2", Origin::Webhook}, {buildCommand("b", "t1", input("a"), "1.1.0")});
    CHECK(q.feeds().at("f1").queue.empty());
    publish(q, "c", "t1");
    CHECK(!q.units().at({"b", "t1"}).published);
    // Already publishing work records the feed truth even after a newer tag.
    Engine p = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    setup(p);
    apply(p, TagObserved{100, "a", "v1.0.0", "sha", Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    build(p, "a", "t1", {publishCommand("a", "t1")});
    apply(p, TagObserved{101, "a", "v1.1.0", "sha2", Origin::Webhook});
    publish(p, "a", "t1", {buildCommand("a", "t1", {}, "1.1.0")});
    CHECK_EQ(p.units().at({"a", "t1"}).published->version.text(), "1.0.0");
    return checkResult();
}
