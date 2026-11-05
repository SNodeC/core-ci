#include "Check.h"
#include "adapters/github/Protocol.h"
using namespace coreci::github;
int main() {
    CHECK_EQ(hmac("key", "The quick brown fox jumps over the lazy dog"),
             "sha256=f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
    const std::string body =
        R"({"ref":"refs/tags/v1.0.0","created":true,"deleted":false,"after":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","repository":{"full_name":"owner/a"}})";
    CHECK(verify("secret", body, hmac("secret", body)));
    CHECK(!verify("secret", body, hmac("wrong", body)));
    CHECK(!verify("secret", body, ""));
    auto event = webhook(body, "push");
    REQUIRE(std::holds_alternative<Push>(event));
    CHECK_EQ(std::get<Push>(event).tag, "v1.0.0");
    CHECK_EQ(std::get<Push>(event).repo, "owner/a");
    for (const auto& payload :
         {R"({"ref":"refs/tags/v1.0.0","created":false})", R"({"ref":"refs/tags/v1.0.0","created":true,"deleted":true})"}) {
        CHECK(std::holds_alternative<std::monostate>(webhook(payload, "push")));
    }
    CHECK(std::holds_alternative<Error>(webhook(R"({"ref":"refs/heads/master"})", "push")));
    const auto head =
        webhook(R"({"ref":"refs/heads/master","deleted":false,"repository":{"full_name":"owner/a","default_branch":"master"}})", "push");
    CHECK(std::holds_alternative<Push>(head));
    CHECK_EQ(std::get<Push>(head).branch, "master");
    CHECK(std::holds_alternative<Error>(webhook("{", "push")));
    CHECK(std::holds_alternative<Error>(webhook("[]", "push")));
    CHECK(std::holds_alternative<std::monostate>(webhook("{}", "issues")));
    CHECK(std::holds_alternative<std::monostate>(webhook("{}", "ping")));
    return checkResult();
}
