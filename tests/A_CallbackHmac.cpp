#include "Check.h"
#include "adapters/github/Protocol.h"
#include "persistence/EventRow.h"
using namespace coreci;
using namespace coreci::github;
int main() {
    const std::string body = "phase=build&status=started&run_id=1&run_attempt=1";
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
    const auto event = report("a.t.1.0.0%2Bmeta.r1", std::get<Form>(decoded), 1000);
    REQUIRE(std::holds_alternative<Event>(event));
    CHECK_EQ(std::get<BuildStarted>(std::get<Event>(event)).buildId, "a.t.1.0.0+meta.r1");
    CHECK(std::holds_alternative<Event>(persistence::normalize(std::get<Event>(event))));
    for (const auto& s : {"x=%", "x=%xx", "x=1&x=2", "x", "=value"}) {
        CHECK(std::holds_alternative<Error>(form(s)));
    }
    CHECK_EQ(std::get<Form>(form("x=a+b%2Bc%26d%3D%C3%A9")).at("x"), "a b+c&d=é");
    CHECK(std::holds_alternative<Error>(report("id", {{"phase", "build"}, {"status", "failed"}, {"run_id", "1"}}, 1)));
    CHECK(std::holds_alternative<std::monostate>(report("id", {{"phase", "publish"}, {"status", "started"}}, 1)));
    for (const auto& data :
         std::vector<Form>{{{"phase", "build"}, {"status", "succeeded"}, {"run_id", "1"}, {"artifact", "artifact"}},
                           {{"phase", "build"}, {"status", "failed"}, {"run_id", "1"}, {"reason", "cancelled"}},
                           {{"phase", "publish"}, {"status", "succeeded"}, {"run_id", "1"}, {"feed_commit", std::string(40, 'a')}},
                           {{"phase", "publish"}, {"status", "failed"}, {"run_id", "1"}, {"reason", "failed"}}}) {
        CHECK(std::holds_alternative<Event>(report("id", data, 1)));
    }
    return checkResult();
}
