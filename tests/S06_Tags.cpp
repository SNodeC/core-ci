#include "Check.h"
#include "Fixture.h"
#include "core/Status.h"

#include <algorithm>
#include <stdexcept>
using namespace coreci;

Version version(const std::string& v = "1.0.0") {
    return Version::parse(v).value();
}
int main() {
    Engine e;
    e.handle(GraphChanged{100, testPipeline()}, 1);
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
    CHECK(e.handle(TagObserved{100, "a", "v1.0.0", hash(), Origin::Webhook}, 2).empty());
    const auto state = e.units();
    for (const auto& tag : {"v0.9.0", "v1.0.0", "1.0.0", "1.0.0+a", "v1.1.0-rc.1", "bad"}) {
        CHECK(std::holds_alternative<Ignore>(e.classify(TagObserved{999, "a", tag, hash(), Origin::Reconcile})));
        CHECK(e.handle(TagObserved{999, "a", tag, hash(), Origin::Reconcile}, 3).empty());
        CHECK(e.units() == state);
        CHECK_EQ(*e.time(), 100);
    }
    return checkResult();
}
