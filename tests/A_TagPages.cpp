#include "Check.h"
#include "adapters/github/Protocol.h"
using namespace coreci::github;
int main() {
    const Url origin{"api.example", "", 443};
    const auto next = nextLink(
        "<https://api.example/tags?page=2>; title=\"a,b\"; rel=\"prev next\", <https://api.example/tags?page=8>; rel=last", origin);
    REQUIRE(std::holds_alternative<std::optional<std::string>>(next));
    CHECK_EQ(*std::get<std::optional<std::string>>(next), "https://api.example/tags?page=2");
    for (const auto& header : {"", "<https://api.example/tags>; rel=last"}) {
        const auto parsed = nextLink(header, origin);
        REQUIRE(std::holds_alternative<std::optional<std::string>>(parsed));
        CHECK(!std::get<std::optional<std::string>>(parsed));
    }
    for (const auto& header : {"garbage",
                               "<https://api.example/tags>; rel=\"next",
                               "<https://api.example/tags>; rel=next,",
                               "<https://api.example/1>; rel=next, <https://api.example/2>; rel=next",
                               "<http://api.example/tags>; rel=next",
                               "<https://evil.example/tags>; rel=next",
                               "<https://api.example:444/tags>; rel=last"}) {
        CHECK(std::holds_alternative<Error>(nextLink(header, origin)));
    }
    const auto parsed = tags("[{\"name\":\"v1.0.0\",\"commit\":{\"sha\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}}]");
    REQUIRE(std::holds_alternative<std::vector<Tag>>(parsed));
    CHECK_EQ(std::get<std::vector<Tag>>(parsed).size(), 1U);
    CHECK_EQ(std::get<std::vector<Tag>>(parsed)[0].tag, "v1.0.0");
    for (const auto& body : {"{}", "[{}]", "[{\"name\":1}]", "invalid"}) {
        CHECK(std::holds_alternative<Error>(tags(body)));
    }
    CHECK_EQ(*decodePath("snode.c%2Btest"), "snode.c+test");
    CHECK(!decodePath("%xx"));
    return checkResult();
}
