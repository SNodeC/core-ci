#include "Check.h"
#include "core/Status.h"

#include <algorithm>
#include <stdexcept>
using namespace coreci;
Pipeline testPipeline() {
    Pipeline::Builder b;
    b.feed({"f1", {"owner", "feeds", "publish.yml", "stable"}})
        .feed({"f2", {"owner", "feeds", "publish.yml", "stable"}})
        .target({"t1", Family::Debian, "f1"})
        .target({"t2", Family::Fedora, "f2"});
    b.package({"a", {"owner", "a"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {}})
        .package({"b", {"owner", "b"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"a"}})
        .package({"c", {"owner", "c"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"a"}})
        .package({"d", {"owner", "d"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"b", "c"}})
        .package({"e", {"owner", "e"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1"}, {"d"}});
    return b.build();
}
Version version(const std::string& v = "1.0.0") {
    return Version::parse(v).value();
}
std::string id(const std::string& p, const std::string& t, const std::string& v = "1.0.0", unsigned r = 1) {
    return p + "." + t + "." + v + ".r" + std::to_string(r);
}
Inputs input(const std::string& p, const std::string& v = "1.0.0", unsigned r = 1) {
    return {{p, {version(v), r}}};
}
Command buildCommand(
    const std::string& p, const std::string& t, Inputs inputs = {}, const std::string& v = "1.0.0", unsigned r = 1, unsigned attempt = 1) {
    return DispatchBuild{
        id(p, t, v, r), attempt, p, {"owner", p}, "v" + v, version(v), r, t, inputs, {"owner", "recipes", "build.yml", "trunk"}};
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
    Engine e(testPipeline(), {100, 50, 8, {}});
    setup(e);
    apply(e, TagObserved{100, "a", "v1.0.0", "sha", Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    apply(e, DispatchAccepted{110, id("a", "t1"), Kind::Build, 1});
    CHECK_EQ(*e.nextDeadline(), 210);
    apply(e, Tick{209});
    CHECK_EQ(*e.time(), 110);
    apply(e, Tick{210});
    CHECK_EQ(statuses(e).at({"a", "t1"}), "Failed(build timeout)");
    std::int64_t now = 220;
    const std::vector<std::int64_t> delays{30, 60, 120, 240, 480, 960, 1800};
    for (unsigned attempt = 1; attempt <= 8; ++attempt) {
        apply(e, DispatchRejected{now, id("a", "t2"), Kind::Build, attempt, 503, "busy"});
        if (attempt < 8) {
            const auto due = now + delays.at(attempt - 1);
            CHECK_EQ(*e.nextDeadline(), due);
            apply(e, Resumed{now + 1});
            apply(e, Tick{due - 1});
            apply(e, Tick{due}, {buildCommand("a", "t2", {}, "1.0.0", 1, attempt + 1)});
            now = due + 1;
        }
    }
    CHECK(e.units().at({"a", "t2"}).failure.has_value());
    CHECK(!e.nextDeadline());
    apply(e, RetryRequested{now, "a", "t2"}, {buildCommand("a", "t2", {}, "1.0.0", 2)});
    apply(e, DispatchRejected{now, id("a", "t2", "1.0.0", 2), Kind::Build, 1, 404, "missing"});
    CHECK(!e.units().at({"a", "t2"}).inflight);
    // Regressing event timestamps use monotonic engine time.
    apply(e, RetryRequested{now, "a", "t1"}, {buildCommand("a", "t1", {}, "1.0.0", 2)});
    apply(e, DispatchAccepted{1, id("a", "t1", "1.0.0", 2), Kind::Build, 1});
    CHECK_EQ(*e.units().at({"a", "t1"}).inflight->deadline, now + 100);
    // Resume only emits dispatches without backoff; callbacks can precede REST results.
    Engine q(testPipeline(), {100, 50, 8, 1});
    setup(q);
    apply(q, TagObserved{100, "a", "v1.0.0", "sha", Origin::Webhook}, {buildCommand("a", "t1")});
    CHECK_EQ(statuses(q).at({"a", "t2"}), "Ready");
    apply(q, Resumed{101}, {buildCommand("a", "t1")});
    apply(q, DispatchRejected{102, id("a", "t1"), Kind::Build, 1, 0, "transport"});
    apply(q, Resumed{103});
    apply(q, BuildStarted{104, id("a", "t1"), 101, 1});
    CHECK(!q.units().at({"a", "t1"}).inflight->retryAt);
    apply(q, BuildSucceeded{105, id("a", "t1"), 101, "artifact"}, {buildCommand("a", "t2"), publishCommand("a", "t1")});
    apply(q, DispatchAccepted{106, id("a", "t1"), Kind::Publish, 1});
    apply(q, Tick{155});
    apply(q, Tick{156});
    CHECK_EQ(statuses(q).at({"a", "t1"}), "Failed(publish timeout)");
    // A retry/resume keeps its assignment when an upstream release changes.
    Engine r(testPipeline(), {100, 50, 8, {}});
    prime(r);
    build(r, "c", "t1", {publishCommand("c", "t1")});
    apply(r, DispatchRejected{101, id("b", "t1"), Kind::Build, 1, 503, "busy"});
    apply(r, TagObserved{102, "a", "v1.1.0", "sha2", Origin::Webhook}, {buildCommand("a", "t1", {}, "1.1.0")});
    apply(r, Tick{131}, {buildCommand("b", "t1", input("a"), "1.0.0", 1, 2)});
    apply(r,
          Resumed{132},
          {buildCommand("a", "t1", {}, "1.1.0"),
           buildCommand("b", "t1", input("a"), "1.0.0", 1, 2),
           publishCommand("c", "t1"),
           buildCommand("a", "t2")});
    build(r, "a", "t1", {}, "1.1.0");
    apply(r, BuildSucceeded{133, id("b", "t1"), 101, "artifact"});
    CHECK(r.units().at({"b", "t1"}).inflight->phase == Phase::AwaitingPublish);
    publish(r, "c", "t1", {publishCommand("a", "t1", "1.1.0")});
    publish(r,
            "a",
            "t1",
            {buildCommand("b", "t1", input("a", "1.1.0"), "1.0.0", 2), buildCommand("c", "t1", input("a", "1.1.0"), "1.0.0", 2)},
            "1.1.0");
    CHECK(!r.units().at({"b", "t1"}).published);
    return checkResult();
}
