#include "Check.h"
#include "adapters/github/Protocol.h"
#include "persistence/Input.h"
using namespace coreci;
using namespace coreci::github;
int main() {
    const std::string body = "scope=unit&build_id=a%7Et&phase=build&status=started&run_id=42";
    const auto signature = hmac("key", "1000." + body);
    CHECK(verifyCallback("key", body, "1000", signature, 1300));
    CHECK(verifyCallback("key", body, "1000", signature, 700));
    CHECK(!verifyCallback("key", body, "1000", signature, 1301));
    CHECK(!verifyCallback("key", body, "1000", signature, 699));
    CHECK(!verifyCallback("key", body, "1000", "", 1000));
    CHECK(!verifyCallback("key", body + "&x=1", "1000", signature, 1000));
    CHECK(!verifyCallback("key", body, "bad", signature, 1000));
    CHECK(!verifyCallback("key", body, "-9223372036854775808", signature, 1000));
    const auto decoded = form(body);
    REQUIRE(std::holds_alternative<Form>(decoded));
    const auto event = report("w1", std::get<Form>(decoded), 1000, 42);
    REQUIRE(std::holds_alternative<Event>(event));
    CHECK_EQ(std::get<UnitBuildStarted>(std::get<Event>(event)).buildId, "a~t");
    CHECK(std::holds_alternative<Event>(persistence::normalize(std::get<Event>(event))));
    CHECK(std::holds_alternative<std::monostate>(report("w1", std::get<Form>(decoded), 1000, 43)));
    CHECK(std::holds_alternative<std::monostate>(report("w1", std::get<Form>(decoded), 1000)));
    for (const auto& value : {"", "0", "-1", "bad"}) {
        auto fields = std::get<Form>(decoded);
        fields["run_id"] = value;
        CHECK(std::holds_alternative<Error>(report("w1", fields, 1000, 42)));
    }
    for (const auto& s : {"x=%", "x=%xx", "x=1&x=2", "x", "=value"}) {
        CHECK(std::holds_alternative<Error>(form(s)));
    }
    CHECK_EQ(std::get<Form>(form("x=a+b%2Bc%26d%3D%C3%A9")).at("x"), "a b+c&d=é");
    CHECK(std::holds_alternative<Error>(report("id", {{"phase", "build"}, {"status", "failed"}, {"run_id", "1"}}, 1)));
    CHECK(std::holds_alternative<Error>(report("id", {{"phase", "publish"}, {"status", "started"}}, 1)));
    for (const auto& data : std::vector<Form>{
             {{"scope", "wave"}, {"status", "started"}, {"run_id", "2"}, {"run_attempt", "3"}},
             {{"scope", "unit"},
              {"build_id", "id"},
              {"phase", "build"},
              {"status", "succeeded"},
              {"run_id", "1"},
              {"artifact", "artifact"}},
             {{"scope", "unit"}, {"build_id", "id"}, {"phase", "build"}, {"status", "failed"}, {"run_id", "1"}, {"reason", "cancelled"}},
             {{"scope", "unit"},
              {"build_id", "id"},
              {"phase", "publish"},
              {"status", "succeeded"},
              {"run_id", "1"},
              {"feed_commit", std::string(40, 'a')}},
             {{"scope", "unit"}, {"build_id", "id"}, {"phase", "publish"}, {"status", "failed"}, {"run_id", "1"}, {"reason", "failed"}}}) {
        CHECK(std::holds_alternative<Event>(report("id", data, 1, 1, data.contains("phase") && data.at("phase") == "publish")));
    }
    CHECK(std::holds_alternative<Error>(
        report("w1", {{"scope", "unit"}, {"build_id", "id"}, {"phase", "publish"}, {"status", "started"}, {"run_id", "1"}}, 1, 1)));
    CHECK(std::holds_alternative<Event>(
        report("w1", {{"scope", "wave"}, {"status", "finished"}, {"run_id", "2"}, {"conclusion", "success"}}, 1)));
    for (const std::string failure : {"artifact-unavailable", "immutable-conflict", "feed-push", "signing", "other", "unknown", ""}) {
        Form fields{{"scope", "unit"},
                    {"build_id", "build"},
                    {"phase", "publish"},
                    {"status", "failed"},
                    {"run_id", "7"},
                    {"reason", "artifact unavailable"},
                    {"failure", failure}};
        if (failure.empty()) {
            fields.erase("failure");
        }
        const auto parsed = report("p1", fields, 100, 7, true);
        CHECK(std::holds_alternative<Event>(parsed));
        if (std::holds_alternative<Event>(parsed)) {
            const auto& result = std::get<UnitPublishFailed>(std::get<Event>(parsed));
            CHECK_EQ(result.failure, failure.empty() || failure == "unknown" ? "other" : failure);
            CHECK_EQ(result.reason, "artifact unavailable");
        }
        CHECK(std::holds_alternative<std::monostate>(report("p1", fields, 100, 8, true)));
        CHECK(std::holds_alternative<Error>(report("w1", fields, 100, 7, false)));
    }
    CHECK(std::holds_alternative<Event>(
        report("w1", {{"scope", "target"}, {"status", "finished"}, {"run_id", "7"}, {"target", "t1"}, {"artifact", ""}}, 100, 7)));
    return checkResult();
}
