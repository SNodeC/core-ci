#include "Check.h"
#include "adapters/github/Protocol.h"
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
    CHECK_EQ(std::get<Form>(decoded).at("build_id"), "a~t");
    for (const auto& s : {"x=%", "x=%xx", "x=1&x=2", "x", "=value"}) {
        CHECK(std::holds_alternative<Error>(form(s)));
    }
    CHECK_EQ(std::get<Form>(form("x=a+b%2Bc%26d%3D%C3%A9")).at("x"), "a b+c&d=é");
    return checkResult();
}
