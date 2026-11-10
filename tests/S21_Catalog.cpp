#include "Check.h"
#include "adapters/dashboard/Model.h"
using namespace coreci;
using namespace coreci::github;
int main() {
    const auto load = [](const std::string& matrix, const std::string& catalog) {
        std::variant<LoadedDeclarations, Error> result = Error{"not completed"};
        loadDeclarations(
            {"o", "registry", "ci/registry.json"},
            "",
            [&](std::string path, bool, Completion done) {
                std::string body;
                if (path.find("/commits/") != std::string::npos) {
                    body = "{\"sha\":\"" + std::string(40, 'a') + "\"}";
                } else if (path.find("registry.json") != std::string::npos) {
                    body =
                        R"({"feeds":{"f":{"publisher":"o/recipes/.github/workflows/publish.yml"}},"default_feed":"f","targets":"ci/targets.json","build":"o/recipes/.github/workflows/build.yml","repositories":["o/source"]})";
                } else if (path.find("targets.json") != std::string::npos) {
                    body = "{\"targets\":" + catalog + "}";
                } else if (path.find("packages.json") != std::string::npos) {
                    body = "{\"package\":\"p\",\"title\":\"Package title\",\"depends_on\":[],\"matrix\":" + matrix + "}";
                } else {
                    body = "{\"default_branch\":\"main\",\"full_name\":\"" + path.substr(7) + "\"}";
                }
                done(200, body, {});
            },
            [&](auto value) {
                result = std::move(value);
            });
        return result;
    };
    const std::string target = R"({"id":"t","distribution":"d","suite":"s","arch":"a","build":{"format":"deb"}})";
    const auto valid = load(R"([{"id":"t"}])", "[" + target + "]");
    REQUIRE(std::holds_alternative<LoadedDeclarations>(valid));
    const auto& declarations = std::get<LoadedDeclarations>(valid);
    CHECK_EQ(declarations.packages.at("p").title, "Package title");
    CHECK_EQ(declarations.packages.at("p").commit, std::string(40, 'a'));
    Engine engine({100, 50, 8, {}});
    engine.handle(GraphChanged{1, declarations.pipeline});
    app::JobCache cache;
    const auto model = dashboard::model(engine, declarations, cache, 1, true);
    CHECK(model.find("\"distribution\":\"d\"") != std::string::npos);
    CHECK(model.find("\"title\":\"Package title\"") != std::string::npos);
    CHECK(dashboard::model(engine, {}, cache, 1, false).find("\"units\":[]") != std::string::npos);
    for (const auto& matrix : {R"([{"id":"unknown"}])", R"([{"id":"t","arch":"a"}])", R"([{"id":"t"},{"id":"t"}])"}) {
        CHECK(std::holds_alternative<Error>(load(matrix, "[" + target + "]")));
    }
    CHECK(std::holds_alternative<Error>(load(R"([{"id":"t"}])", "[" + target + "," + target + "]")));
    CHECK(std::holds_alternative<Error>(
        load(R"([{"id":"t"}])", "[" + target + R"(,{"id":"bad#id","distribution":"d","suite":"s","arch":"a","build":{"format":"deb"}}])")));
    return checkResult();
}
