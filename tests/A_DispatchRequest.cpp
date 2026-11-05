#include "Check.h"
#include "adapters/github/Protocol.h"
using namespace coreci;
using namespace coreci::github;
int main() {
    const auto version = *Version::parse("1.2.3+meta");
    const DispatchBuild b{"a.t.1.2.3+meta.r2",
                          1,
                          "a",
                          {"owner", "source"},
                          "v1.2.3+meta",
                          version,
                          2,
                          "t",
                          {{"b", {version, 3}}},
                          {"owner", "recipes", "build.yml", "master"}};
    const auto r = dispatch(b, "https://callback/", "token");
    CHECK_EQ(r.method, "POST");
    CHECK_EQ(r.path, "/repos/owner/recipes/actions/workflows/build.yml/dispatches");
    CHECK_EQ(r.headers.at("Authorization"), "Bearer token");
    CHECK_EQ(r.headers.at("Accept"), "application/vnd.github+json");
    CHECK_EQ(r.headers.at("X-GitHub-Api-Version"), "2022-11-28");
    CHECK_EQ(
        r.body,
        R"({"inputs":{"build_id":"a.t.1.2.3+meta.r2","callback_url":"https://callback/api/v1/builds/a.t.1.2.3%2Bmeta.r2/report","package":"a","prerequisites":"b=1.2.3+meta-3","revision":"2","source_repo":"owner/source","tag":"v1.2.3+meta","target":"t","version":"1.2.3+meta"},"ref":"master"})");
    const DispatchPublish p{
        b.buildId, 1, "a", version, 2, "t", "f", {"owner", "recipes"}, 42, "artifact", {"owner", "publisher", "publish.yml", "trunk"}};
    const auto published = dispatch(p, "https://callback", "token");
    CHECK(published.body.find("\"build_run_id\":\"42\"") != std::string::npos);
    CHECK(published.body.find("\"build_repo\":\"owner/recipes\"") != std::string::npos);
    CHECK(published.body.find("\"ref\":\"trunk\"") != std::string::npos);
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
