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
Command buildCommand(
    const std::string& p, const std::string& t, Inputs inputs = {}, const std::string& v = "1.0.0", unsigned r = 1, unsigned attempt = 1) {
    return DispatchBuild{
        id(p, t, v, r), attempt, p, {"owner", p, "trunk"}, "v" + v, version(v), r, t, inputs, {"owner", "recipes", "build.yml", "trunk"}};
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
int main() {
    Engine e = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    setup(e);
    for (const auto& tag : {"V1.0.0",
                            "1.0",
                            "release-1.0.0",
                            "v01.0.0",
                            "01.0.0",
                            "1.01.0",
                            "1.0.01",
                            "1.0.0-01",
                            "1.0.0-a..b",
                            "1.0.0+",
                            "1.0.0-",
                            "vv1.0.0",
                            " 1.0.0",
                            "1.0.0\n"}) {
        CHECK(!Version::parse(tag));
        apply(e, TagObserved{999, "a", tag, "sha", Origin::Webhook});
    }
    const std::vector<std::string> chain{
        "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta", "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0"};
    for (std::size_t i = 0; i < chain.size(); ++i) {
        REQUIRE(Version::parse(chain[i]));
        if (i > 0) {
            CHECK(version(chain[i - 1]) < version(chain[i]));
        }
    }
    CHECK(version("v1.0.0") == version("1.0.0"));
    CHECK(version("1.0.0") == version("1.0.0+build"));
    CHECK(version("1.0.0+a") == version("1.0.0+b"));
    CHECK_EQ(version("v2.0.1").text(), "2.0.1");
    CHECK(Version::parse("1.0.0+001"));
    CHECK(Version::parse("v1.0.0+" + std::string(94, 'a')));
    CHECK(!Version::parse("1.0.0+" + std::string(95, 'a')));
    CHECK(version("999999999999999999999999.0.0") > version("99999999999999999999999.0.0"));
    apply(e, TagObserved{100, "a", "v1.0.0", "sha", Origin::Webhook}, {buildCommand("a", "t1"), buildCommand("a", "t2")});
    const auto state = e.units();
    for (const auto& tag : {"v0.9.0", "v1.0.0", "1.0.0", "1.0.0+a", "v1.1.0-rc.1", "bad"}) {
        CHECK(std::holds_alternative<Ignore>(e.classify(TagObserved{999, "a", tag, "sha", Origin::Reconcile})));
        apply(e, TagObserved{999, "a", tag, "sha", Origin::Reconcile});
        CHECK(e.units() == state);
        CHECK_EQ(*e.time(), 100);
    }
    Pipeline::Builder builder;
    builder.feed({"f", {"o", "feed", "pub.yml", "stable"}})
        .target({"t", "f"})
        .package({"p", {"o", "p", "trunk"}, {"o", "recipes", "build.yml", "trunk"}, {"t"}, {}, true});
    Engine pre = fixtureEngine(builder.build(), {100, 50, 8, {}});
    const DispatchBuild expected{"p~t~1.0.0-rc.1~r1",
                                 1,
                                 "p",
                                 {"o", "p", "trunk"},
                                 "v1.0.0-rc.1",
                                 version("1.0.0-rc.1"),
                                 1,
                                 "t",
                                 {},
                                 {"o", "recipes", "build.yml", "trunk"}};
    apply(pre, TagObserved{100, "p", "v1.0.0-rc.1", "sha", Origin::Reconcile}, {expected});
    return checkResult();
}
