#include "Check.h"
#include "adapters/github/Declarations.h"
#include "core/Status.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <tuple>
using namespace coreci;
using namespace coreci::github;
std::string package(const std::string& name, const std::string& dependency = "") {
    return "{\"package\":\"" + name + "\",\"depends_on\":[" + (dependency.empty() ? "" : "\"" + dependency + "\"") +
           "],\"matrix\":[{\"id\":\"t\"}]}";
}
std::string read(const std::string& file) {
    std::ifstream input(std::string(CORECI_TEST_DATA) + "/" + file);
    std::stringstream result;
    result << input.rdbuf();
    CHECK(input.good() || input.eof());
    return result.str();
}
int main() {
    const std::string registry =
        R"({"feeds":{"f":{"publisher":"o/recipes/.github/workflows/publish.yml"}},"default_feed":"f","targets":"ci/targets.json","build":"o/recipes/.github/workflows/build.yml","repositories":["O/A","o/b"],"ignored":42})";
    const auto location = registryFile("o/registry:ci/registry.json");
    REQUIRE(std::holds_alternative<DeclarationFile>(location));
    const auto load = [&](std::string reg, std::string a, std::string b, int status = 200, std::string branch = "master") {
        std::variant<LoadedDeclarations, Error> result = Error{"not completed"};
        bool completed = false;
        loadDeclarations(
            std::get<DeclarationFile>(location),
            "",
            [&](std::string path, bool raw, auto done) {
                int httpStatus = 200;
                std::string body = "{\"default_branch\":\"" + branch + "\",\"full_name\":\"" + repoKey(path.substr(7)) + "\"}";
                if (path.find("/commits/") != std::string::npos) {
                    body = "{\"sha\":\"" + std::string(40, 'a') + "\"}";
                } else if (path.find("/contents/") != std::string::npos) {
                    CHECK(raw);
                    CHECK(path.ends_with("?ref=" + escape(branch)));
                    CHECK(path.find("/contents/ci%2F") != std::string::npos);
                    if (path.find("targets.json") != std::string::npos) {
                        body = read("targets.json");
                    } else if (path.starts_with("/repos/o/registry/")) {
                        body = reg;
                    } else if (path.starts_with("/repos/o/a/")) {
                        body = a;
                        httpStatus = status;
                    } else if (path.starts_with("/repos/o/b/")) {
                        body = b;
                    } else {
                        CHECK(false);
                    }
                }
                done(httpStatus, std::move(body), {});
            },
            [&](auto value) {
                completed = true;
                result = std::move(value);
            });
        CHECK(completed);
        return result;
    };
    auto valid = load(registry, package("a"), package("b", "a"));
    REQUIRE(std::holds_alternative<LoadedDeclarations>(valid));
    const auto& pipeline = std::get<LoadedDeclarations>(valid).pipeline;
    CHECK_EQ(pipeline.packages().size(), 2U);
    CHECK_EQ(pipeline.packages().at("a").sourceRepo.branch, "master");
    CHECK(pipeline.packagesForRepo("O", "A") == std::vector<std::string>{"a"});
    CHECK(pipeline.prerequisites("b") == std::vector<std::string>{"a"});
    CHECK_EQ(pipeline.packages().at("a").revisionBase, 0U);
    const auto bad = [&](std::string reg, std::string a, int status = 200) {
        const auto result = load(reg, a, package("b", "a"), status);
        CHECK(std::holds_alternative<Error>(result));
        if (std::holds_alternative<Error>(result)) {
            CHECK(std::get<Error>(result).reason.find("o/") != std::string::npos);
            CHECK(std::get<Error>(result).reason.find(":ci/") != std::string::npos);
        }
    };
    auto withPresentation = registry;
    const auto publisherEnd = withPresentation.find("publish.yml\"") + std::string("publish.yml\"").size();
    withPresentation.insert(publisherEnd, ",\"presentation\":\"o/recipes/.github/workflows/status.yml\"");
    withPresentation.insert(1, "\"status_url\":\"https://controller.example/api/v1/status.json\",");
    const auto presented = load(withPresentation, package("a"), package("b", "a"));
    REQUIRE(std::holds_alternative<LoadedDeclarations>(presented));
    CHECK(std::get<LoadedDeclarations>(presented).presentations.at("f") == Workflow({"o", "recipes", "status.yml", "master"}));
    auto invalidStatus = withPresentation;
    invalidStatus.replace(invalidStatus.find("https://"), 8, "http://");
    bad(invalidStatus, package("a"));
    auto missingStatus = withPresentation;
    const auto firstComma = missingStatus.find(',');
    missingStatus.erase(1, firstComma);
    bad(missingStatus, package("a"));
    for (const auto& file : {"{}",
                             "[]",
                             "{",
                             R"({"package":1,"depends_on":[],"matrix":[]})",
                             R"({"package":"a","package":"a","depends_on":[],"matrix":[]})",
                             R"({"package":"a","depends_on":{},"matrix":[]})",
                             R"({"package":"a","depends_on":[],"matrix":{}})",
                             R"({"package":"a","depends_on":[],"matrix":[{}]})",
                             R"({"package":"a","depends_on":[],"matrix":[{"id":"t","feed":"missing"}]})",
                             R"({"package":"a","depends_on":[],"matrix":[{"id":"t"},{"id":"t"}]})",
                             R"({"package":"a","depends_on":[],"accept_prereleases":1,"matrix":[{"id":"t"}]})"}) {
        bad(registry, file);
    }
    for (const auto& base : {"-1", "1000001", "4294967296", "18446744073709551615", "1.0", "null", "true", "\"10\"", "[]", "{}"}) {
        auto file = package("a");
        file.insert(file.size() - 1, std::string(",\"revision_base\":") + base);
        bad(registry, file);
    }
    for (const unsigned base : {0U, 100U, 1000000U}) {
        auto file = package("a");
        file.insert(file.size() - 1, ",\"revision_base\":" + std::to_string(base));
        const auto loaded = load(registry, file, package("b", "a"));
        REQUIRE(std::holds_alternative<LoadedDeclarations>(loaded));
        CHECK_EQ(std::get<LoadedDeclarations>(loaded).pipeline.packages().at("a").revisionBase, base);
    }
    bad(registry, std::string(1024U * 1024U + 1, ' '));
    bad(registry, package("a"), 404);
    for (const auto& name : {std::string{}, std::string(65, 'a'), std::string("a/b"), std::string("a b"), std::string("é")}) {
        bad(registry, package(name));
    }
    for (const auto& workflow : {"o/recipes/build.yml",
                                 "o/recipes/.github/workflows/a/b.yml",
                                 "o/recipes/.github/workflows/build",
                                 "bad!/recipes/.github/workflows/build.yml"}) {
        auto changed = registry;
        const auto at = changed.find("o/recipes/.github/workflows/build.yml");
        changed.replace(at, std::string("o/recipes/.github/workflows/build.yml").size(), workflow);
        bad(changed, package("a"));
    }
    for (const auto& path : std::vector<std::string>{
             "o/r:", "o/r:/a", "o/r:a//b", "o/r:a/../b", "o/r:./a", "o/r:a?b", std::string("o/r:") + std::string(256, 'x')}) {
        CHECK(std::holds_alternative<Error>(registryFile(path)));
    }
    auto conflict = registry;
    conflict.replace(conflict.find("\"default_feed\""), 0, R"("extra":true,)");
    auto aFeed = package("a");
    aFeed.replace(aFeed.find("\"id\":\"t\""), 8, "\"id\":\"t\",\"feed\":\"g\"");
    conflict.replace(conflict.find("\"feeds\":{") + 9, 0, R"("g":{"publisher":"o/recipes/.github/workflows/publish.yml"},)");
    bad(conflict, aFeed);
    for (
        const auto& invalid :
        {"{}",
         "[]",
         "{",
         R"({"feeds":[]})",
         R"({"feeds":{"f":{"publisher":"o/recipes/.github/workflows/publish.yml"}},"default_feed":"f","targets":"ci/targets.json","build":"o/recipes/.github/workflows/build.yml"})"}) {
        bad(invalid, package("a"));
    }
    const auto replaced = [](std::string text, const std::string& oldValue, const std::string& newValue) {
        const auto at = text.find(oldValue);
        CHECK(at != std::string::npos);
        text.replace(at, oldValue.size(), newValue);
        return text;
    };
    for (const auto& [oldValue, newValue] : std::vector<std::pair<std::string, std::string>>{
             {R"("default_feed":"f")", R"("default_feed":false)"},
             {R"("default_feed":"f")", R"("default_feed":"missing")"},
             {R"("build":"o/recipes/.github/workflows/build.yml")", R"("build":1)"},
             {R"("repositories":["O/A","o/b"])", R"("repositories":{})"},
             {R"("repositories":["O/A","o/b"])", R"("repositories":[1])"},
             {R"("repositories":["O/A","o/b"])", R"("repositories":["o/a","O/A"])"},
             {R"("feeds":{)", R"("feeds":{"f":{"publisher":"o/recipes/.github/workflows/publish.yml"},)"},
             {R"("ignored":42)", R"("ignored":{"x":1,"x":2})"}}) {
        bad(replaced(registry, oldValue, newValue), package("a"));
    }
    for (const auto& invalid : {std::string{}, std::string(65, 'x'), std::string("a/b"), std::string("bad id")}) {
        bad(replaced(registry, R"("f":{"publisher")", "\"" + invalid + R"(":{"publisher")"), package("a"));
        bad(registry, replaced(package("a"), R"("id":"t")", "\"id\":\"" + invalid + "\""));
    }
    for (const auto& invalidRepo :
         {std::string("bad_owner/a"), std::string(40, 'a') + "/a", std::string("o/.."), "o/" + std::string(101, 'a')}) {
        bad(replaced(registry, "O/A", invalidRepo), package("a"));
    }
    auto unknown = package("a");
    unknown.insert(unknown.size() - 1, R"(,"ignored":{"any":[null,1,true]})");
    CHECK(std::holds_alternative<LoadedDeclarations>(load(registry, unknown, package("b", "a"))));
    auto boundary = package("a");
    boundary.resize(1024U * 1024U, ' ');
    CHECK(std::holds_alternative<LoadedDeclarations>(load(registry, boundary, package("b", "a"))));
    const auto slashBranch = load(registry, package("a"), package("b", "a"), 200, "feature/topic");
    REQUIRE(std::holds_alternative<LoadedDeclarations>(slashBranch));
    CHECK_EQ(std::get<LoadedDeclarations>(slashBranch).pipeline.packages().at("a").sourceRepo.branch, "feature/topic");
    for (const auto& branch : {std::string{},
                               std::string(256, 'a'),
                               std::string("@"),
                               std::string("HEAD"),
                               std::string("-x"),
                               std::string("a..b"),
                               std::string("a//b"),
                               std::string("a.lock/b"),
                               std::string("a b")}) {
        const auto invalid = load(registry, package("a"), package("b", "a"), 200, branch);
        REQUIRE(std::holds_alternative<Error>(invalid));
        CHECK(std::get<Error>(invalid).reason.find("o/registry:<repository>:") != std::string::npos);
    }
    // HTTP completions may arrive in any order. Independent package reads remain independent.
    std::vector<std::tuple<std::string, bool, Completion>> pending;
    std::size_t maximumPending = 0;
    int completions = 0;
    std::variant<LoadedDeclarations, Error> asynchronous = Error{"not completed"};
    loadDeclarations(
        std::get<DeclarationFile>(location),
        "",
        [&](std::string path, bool raw, auto callback) {
            pending.emplace_back(std::move(path), raw, std::move(callback));
            maximumPending = std::max(maximumPending, pending.size());
        },
        [&](auto value) {
            ++completions;
            asynchronous = std::move(value);
        });
    for (unsigned step = 0; !pending.empty() && step < 32; ++step) {
        auto item = std::move(pending.back());
        pending.pop_back();
        const auto& path = std::get<0>(item);
        std::string body = "{\"default_branch\":\"master\",\"full_name\":\"" + repoKey(path.substr(7)) + "\"}";
        if (path.find("/commits/") != std::string::npos) {
            body = "{\"sha\":\"" + std::string(40, 'a') + "\"}";
        } else if (path.find("targets.json") != std::string::npos) {
            body = read("targets.json");
        } else if (path.find("/contents/") != std::string::npos) {
            body = path.starts_with("/repos/o/registry/") ? registry : (path.starts_with("/repos/o/a/") ? package("a") : package("b", "a"));
        }
        std::get<2>(item)(200, std::move(body), {});
    }
    CHECK_EQ(completions, 1);
    CHECK(maximumPending >= 2);
    REQUIRE(std::holds_alternative<LoadedDeclarations>(asynchronous));
    CHECK(std::get<LoadedDeclarations>(asynchronous).pipeline == pipeline);
    // GitHub supplies spelling independently of the registry, for all repository roles.
    const std::string caseRegistry =
        R"({"feeds":{"f":{"publisher":"snodec/RECIPES/.github/workflows/publish.yml"}},"default_feed":"f","targets":"ci/targets.json","build":"SNODEC/recipes/.github/workflows/build.yml","repositories":["snodec/SNODE.C"]})";
    std::variant<LoadedDeclarations, Error> canonical = Error{"not completed"};
    std::vector<std::string> paths;
    loadDeclarations(
        std::get<DeclarationFile>(registryFile("snodec/REGISTRY:ci/registry.json")),
        "",
        [&](std::string path, bool, auto done) {
            paths.push_back(path);
            const auto identity = repoKey(path);
            std::string body;
            if (path.find("/commits/") != std::string::npos) {
                body = "{\"sha\":\"" + std::string(40, 'a') + "\"}";
            } else if (path.find("targets.json") != std::string::npos) {
                body = read("targets.json");
            } else if (path.find("/contents/") != std::string::npos) {
                CHECK(path.starts_with("/repos/SNodeC/"));
                CHECK(path.find("/Registry/") != std::string::npos || path.find("/snode.c/") != std::string::npos);
                body = identity.starts_with("/repos/snodec/registry/") ? caseRegistry : package("snodec");
            } else {
                const std::string name =
                    identity == "/repos/snodec/registry" ? "Registry" : (identity == "/repos/snodec/recipes" ? "Recipes" : "snode.c");
                body = "{\"default_branch\":\"master\",\"full_name\":\"SNodeC/" + name + "\"}";
            }
            done(200, std::move(body), {});
        },
        [&](auto value) {
            canonical = std::move(value);
        });
    REQUIRE(std::holds_alternative<LoadedDeclarations>(canonical));
    const auto& canon = std::get<LoadedDeclarations>(canonical).pipeline;
    CHECK_EQ(paths.size(), 9U); // Registry, shared workflow repository and source are each resolved once.
    CHECK(canon.repositories() == (std::set<std::pair<std::string, std::string>>{{"SNodeC", "snode.c"}}));
    CHECK_EQ(canon.packages().at("snodec").sourceRepo.owner, "SNodeC");
    CHECK_EQ(canon.packages().at("snodec").sourceRepo.repo, "snode.c");
    CHECK_EQ(canon.packages().at("snodec").buildWorkflow.repo, "Recipes");
    CHECK_EQ(canon.feeds().at("f").workflow.owner, "SNodeC");
    CHECK(canon.packagesForRepo("SNODEC", "SNODE.C") == std::vector<std::string>{"snodec"});
    Engine display({100, 50, 8, {}});
    display.handle(GraphChanged{1, canon});
    CHECK(statusText(display).find("SNodeC/snode.c") != std::string::npos);
    CHECK(statusDot(display).find("SNodeC/snode.c") != std::string::npos);
    const auto commands = display.handle(TagObserved{2, "snodec", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    REQUIRE(commands.size() == 1);
    CHECK_EQ(dispatchPath(commands.front()), "/repos/SNodeC/Recipes/actions/workflows/build.yml/dispatches");
    CHECK(dispatchBody(commands.front(), "https://callback").find("SNodeC/snode.c") != std::string::npos);
    // Real source-owned declarations are data only; no distribution interpretation occurs.
    auto real = load(registry, read("snodec-packages.json"), read("mqttsuite-packages.json"));
    REQUIRE(std::holds_alternative<LoadedDeclarations>(real));
    const auto& actual = std::get<LoadedDeclarations>(real).pipeline;
    CHECK_EQ(actual.packages().at("snodec").targets.size(), 76U);
    CHECK_EQ(actual.packages().at("mqttsuite").targets.size(), 76U);
    CHECK(actual.prerequisites("mqttsuite") == std::vector<std::string>{"snodec"});
    return checkResult();
}
