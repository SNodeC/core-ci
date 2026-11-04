#include "Check.h"
#include "core/Graph.h"

#include <stdexcept>
using namespace coreci;

Pipeline::Builder base() {
    Pipeline::Builder b;
    b.feed({"f", {"o", "feed", "publish.yml", "stable"}}).target({"t", Family::Debian, "f"});
    return b;
}
Package package(const std::string& name, std::vector<std::string> prerequisites = {}) {
    return {name, {"o", name}, {"o", "recipes", "build.yml", "trunk"}, {"t"}, prerequisites};
}
void rejects(const Pipeline::Builder& builder, const std::string& message) {
    bool rejected = false;
    try {
        builder.build();
    } catch (const std::invalid_argument& error) {
        rejected = true;
        CHECK_EQ(std::string(error.what()), message);
    }
    CHECK(rejected);
}
int main() {
    auto cycle = base();
    cycle.package(package("a", {"b"})).package(package("b", {"c"})).package(package("c", {"a"}));
    rejects(cycle, "cycle: a -> b -> c -> a");

    auto self = base();
    self.package(package("a", {"a"}));
    rejects(self, "cycle: a -> a");

    auto unknown = base();
    unknown.package(package("a", {"missing"}));
    rejects(unknown, "unknown package: missing required by a");

    auto missing = base();
    auto q = package("q");
    q.targets.clear();
    missing.package(package("p", {"q"})).package(q);
    rejects(missing, "prerequisite q missing target t required by p");

    auto target = base();
    auto p = package("p");
    p.targets = {"unknown"};
    target.package(p);
    rejects(target, "unknown target: unknown for package p");

    auto feed = base();
    feed.target({"t2", Family::Fedora, "missing"});
    rejects(feed, "unknown feed: missing for target t2");

    auto duplicatePackage = base();
    duplicatePackage.package(package("p")).package(package("p"));
    rejects(duplicatePackage, "duplicate package: p");

    auto duplicateTarget = base();
    duplicateTarget.target({"t", Family::Fedora, "f"});
    rejects(duplicateTarget, "duplicate target: t");

    auto duplicateFeed = base();
    duplicateFeed.feed({"f", {"o", "feed", "publish.yml", "stable"}});
    rejects(duplicateFeed, "duplicate feed: f");

    auto overlap = base();
    p = package("p");
    q = package("q");
    q.sourceRepo = p.sourceRepo;
    overlap.package(p).package(q);
    rejects(overlap, "sourceRepo overlap: p and q on t");

    // Insertion order does not determine tie order; forward references resolve.
    auto valid = base();
    valid.target({"t2", Family::Fedora, "f"});
    valid.package(package("d", {"b", "c"})).package(package("c", {"a"})).package(package("b", {"a"})).package(package("a"));
    q.targets = {"t2"};
    q.sourceRepo = {"o", "a"};
    valid.package(q);
    const auto pipeline = valid.build();
    CHECK((pipeline.topologicalOrder("t") == std::vector<std::string>{"a", "b", "c", "d"}));
    CHECK((pipeline.topologicalOrder("t2") == std::vector<std::string>{"q"}));
    CHECK((pipeline.prerequisites("d") == std::vector<std::string>{"b", "c"}));
    CHECK((pipeline.dependents("a") == std::vector<std::string>{"b", "c"}));
    CHECK((pipeline.packagesForRepo("o", "a") == std::vector<std::string>{"a", "q"}));
    CHECK(pipeline.packagesForRepo("unknown", "repo").empty());
    // A built graph owns its definitions and cannot be changed by its builder.
    valid.package(package("z"));
    CHECK(!pipeline.packages().contains("z"));
    return checkResult();
}
