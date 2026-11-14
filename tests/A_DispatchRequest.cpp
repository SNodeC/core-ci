#include "Check.h"
#include "adapters/github/Protocol.h"
using namespace coreci;
using namespace coreci::github;
int main() {
    const DispatchWave wave{"w42", 2, {"owner", "Packages", "main"}, "feed-staging"};
    CHECK_EQ(dispatchPath(wave), "/repos/owner/Packages/actions/workflows/wave.yml/dispatches");
    const auto body = dispatchBody(wave, "https://callback/");
    CHECK_EQ(
        body,
        R"({"inputs":{"callback_url":"https://callback/api/v1/waves/w42/report","feed_branch":"feed-staging","plan_url":"https://callback/api/v1/waves/w42/plan.json","wave_id":"w42"},"ref":"main"})");
    CHECK(httpsUrl("https://localhost:123/api"));
    CHECK(httpsUrl("https://localhost:123/api?page=2"));
    CHECK(!httpsUrl("http://localhost"));
    CHECK(!httpsUrl("https://user@localhost"));
    CHECK(!httpsUrl("https://localhost:99999"));
    CHECK(std::holds_alternative<std::vector<Tag>>(
        tags(R"([{"name":"v1.0.0","commit":{"sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}])")));
    CHECK(std::holds_alternative<Error>(tags("{}")));
    CHECK(std::holds_alternative<Error>(tags("[{}]")));
    return checkResult();
}
