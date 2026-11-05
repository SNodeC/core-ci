#include "Check.h"
#include "Fixture.h"
#include "core/Badge.h"
#include "core/Status.h"

using namespace coreci;
void expect(const Engine& engine, const Unit& unit, const std::string& status, const std::string& message, const std::string& colour) {
    CHECK(statuses(engine).at(unit).starts_with(status));
    CHECK_EQ(unitBadge(engine, unit), badge(unit.package, message, colour));
}
int main() {
    const std::string golden =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"110\" height=\"20\" role=\"img\"><title>a: building 1.0.0</title>"
        "<rect width=\"16\" height=\"20\" fill=\"#555\"/><rect x=\"16\" width=\"94\" height=\"20\" fill=\"#0969da\"/>"
        "<g fill=\"white\" font-family=\"Verdana,DejaVu Sans,sans-serif\" font-size=\"11\" text-anchor=\"middle\">"
        "<text x=\"8\" y=\"14\">a</text><text x=\"63\" y=\"14\">building 1.0.0</text></g></svg>";
    CHECK_EQ(badge("a", "building 1.0.0", "#0969da"), golden);
    const auto escaped = badge("&<>\"'", "&<>\"'", "#555");
    CHECK(escaped.find("<title>&amp;&lt;&gt;&quot;&apos;: &amp;&lt;&gt;&quot;&apos;</title>") != std::string::npos);
    CHECK(escaped.find("width=\"80\"") != std::string::npos); // 2 * (6 * 5 + 10), before escaping.
    CHECK(escaped.find("<text x=\"20\" y=\"14\">&amp;&lt;&gt;&quot;&apos;</text>") != std::string::npos);
    CHECK(escaped.find("<text x=\"60\" y=\"14\">&amp;&lt;&gt;&quot;&apos;</text>") != std::string::npos);
    CHECK(badge("é😀", "", "#555").find("width=\"32\"") != std::string::npos); // Unicode characters, not UTF-8 bytes.
    CHECK(badge("", "", "#555").find("width=\"20\"") != std::string::npos);
    auto engine = fixtureEngine(testPipeline(), {100, 50, 8, 1});
    expect(engine, {"a", "t1"}, "NoRelease", "no release", "#6e7781");
    CHECK_EQ(unitBadge(engine, {"missing", "t1"}), badge("missing", "unknown", "#6e7781"));
    CHECK_EQ(unitBadge(engine, {"a", "missing"}), badge("a", "unknown", "#6e7781"));
    engine.handle(TagObserved{101, "b", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    expect(engine, {"b", "t1"}, "Waiting", "building 1.0.0", "#0969da");
    engine.handle(TagObserved{102, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    expect(engine, {"a", "t1"}, "Dispatching", "building 1.0.0", "#0969da");
    expect(engine, {"a", "t2"}, "Ready", "building 1.0.0", "#0969da");
    engine.handle(BuildStarted{103, "a~t1~1.0.0~r1", 1, 1});
    expect(engine, {"a", "t1"}, "Building", "building 1.0.0", "#0969da");
    engine.handle(BuildFailed{104, "a~t1~1.0.0~r1", 1, "failure"});
    expect(engine, {"a", "t1"}, "Failed", "failed 1.0.0", "#cf222e");
    expect(engine, {"b", "t1"}, "Blocked", "blocked", "#bf8700");
    auto publishing = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    publishing.handle(TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    publishing.handle(BuildSucceeded{102, "a~t1~1.0.0~r1", 1, "artifact"});
    expect(publishing, {"a", "t1"}, "Publishing", "building 1.0.0", "#0969da");
    publishing.handle(DispatchAccepted{103, "a~t1~1.0.0~r1", Kind::Publish, 1});
    expect(publishing, {"a", "t1"}, "Publishing", "building 1.0.0", "#0969da");
    publishing.handle(PublishSucceeded{104, "a~t1~1.0.0~r1", 2, std::string(40, 'b')});
    expect(publishing, {"a", "t1"}, "Settled", "1.0.0-1", "#2ea44f");
    publishing.handle(TagObserved{105, "b", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    publishing.handle(TagObserved{105, "c", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    publishing.handle(BuildSucceeded{106, "b~t1~1.0.0~r1", 1, "artifact"});
    publishing.handle(BuildSucceeded{106, "c~t1~1.0.0~r1", 1, "artifact"});
    expect(publishing, {"c", "t1"}, "AwaitingPublish", "building 1.0.0", "#0969da");
    publishing.handle(GraphChanged{107, Pipeline{}}); // Deferred while publishing.
    CHECK(publishing.pipeline() != Pipeline{});
    auto dormant = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    dormant.handle(TagObserved{101, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    dormant.handle(GraphChanged{102, Pipeline{}});
    CHECK_EQ(unitBadge(dormant, {"a", "t1"}), badge("a", "unknown", "#6e7781"));
    CHECK(badge("a", "unavailable", "#6e7781").find("<title>a: unavailable</title>") != std::string::npos);
    return checkResult();
}
