#include "Check.h"
#include "adapters/github/Declarations.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
using namespace coreci;
using namespace coreci::github;
using Json = nlohmann::json;

Json source(const std::string& name, std::vector<std::string> prerequisites = {}) {
    return Json{{"schema", 1},
                {"package", name},
                {"title", name + " title"},
                {"depends_on", prerequisites},
                {"build", {{"deb", "ci/deb.sh"}, {"rpm", "ci/rpm.sh"}}},
                {"build_dependencies", {{"deb", Json::array()}, {"rpm", Json::array()}, {"openwrt", Json::array()}}},
                {"targets", {"t"}}};
}
std::string read(const std::string& path) {
    std::ifstream input(std::string(CORECI_TEST_DATA) + "/" + path);
    std::stringstream text;
    text << input.rdbuf();
    CHECK(input.good() || input.eof());
    return text.str();
}
struct LoadResult {
    std::variant<LoadedDeclarations, Error> value = Error{"unfinished"};
    std::vector<std::string> refs;
    unsigned completed = 0, sourceHeads = 0;
    std::size_t maximumPending = 0;
};
LoadResult load(const std::string& registry,
                const std::string& a,
                const std::string& b,
                const std::string& catalog,
                const std::string& mode = "normal",
                int sourceStatus = 200,
                const std::string& branch = "master") {
    LoadResult result;
    std::vector<std::tuple<std::string, bool, Completion>> pending;
    loadDeclarations(
        {"o", "registry", "ci/registry.json"},
        "ci/targets.json",
        {"api.example", ""},
        [&](std::string path, bool raw, Completion done) {
            pending.emplace_back(path, raw, std::move(done));
            result.maximumPending = std::max(result.maximumPending, pending.size());
        },
        [&](auto value) {
            ++result.completed;
            result.value = std::move(value);
        });
    for (unsigned step = 0; !pending.empty() && step < 100; ++step) {
        auto [path, raw, done] = std::move(pending.back());
        pending.pop_back();
        std::string body, link;
        int status = 200;
        if (path.find("/commits/") != std::string::npos) {
            result.sourceHeads += path.find("/registry/") == std::string::npos ? 1 : 0;
            body = Json{{"sha", std::string(40, 'a')}}.dump();
        } else if (path.find("/tags?") != std::string::npos) {
            if (mode == "empty") {
                body = "[]";
            } else if (path.ends_with("page=2")) {
                body = mode == "malformed" ? "{" : Json::array({{{"name", "v2.0.0"}, {"commit", {{"sha", std::string(40, 'c')}}}}}).dump();
                if (mode == "loop") {
                    link = "<https://api.example/repos/o/a/tags?page=2>; rel=\"next\"";
                }
            } else {
                body = Json::array({{{"name", "v1.0.0"}, {"commit", {{"sha", std::string(40, 'b')}}}},
                                    {{"name", "v3.0.0-rc.1"}, {"commit", {{"sha", std::string(40, 'd')}}}}})
                           .dump();
                if (mode != "normal") {
                    link = "<https://" + std::string(mode == "off-origin" ? "else.example" : "api.example") +
                           "/repos/o/a/tags?page=2>; rel=\"next\"";
                }
            }
        } else if (path.find("/contents/") != std::string::npos) {
            CHECK(raw);
            CHECK(path.find("/contents/ci%2F") != std::string::npos);
            if (path.find("registry.json") != std::string::npos) {
                CHECK(path.ends_with("?ref=" + std::string(40, 'a')));
                body = registry;
            } else if (path.find("targets.json") != std::string::npos) {
                CHECK(path.ends_with("?ref=" + std::string(40, 'a')));
                body = catalog;
            } else {
                result.refs.push_back(path.substr(path.find("?ref=") + 5));
                body = path.starts_with("/repos/o/a/") ? a : b;
                status = sourceStatus;
            }
        } else {
            body = Json{{"full_name", repoKey(path.substr(7))}, {"default_branch", branch}}.dump();
        }
        done(status, body, link);
    }
    CHECK_EQ(result.completed, 1U);
    return result;
}
int main() {
    const Json registry{{"schema", 1}, {"repositories", {"O/A", "o/b"}}};
    const auto catalog = read("targets.json");
    const auto valid = load(registry.dump(), source("a").dump(), source("b", {"a"}).dump(), catalog);
    REQUIRE(std::holds_alternative<LoadedDeclarations>(valid.value));
    const auto& loaded = std::get<LoadedDeclarations>(valid.value);
    CHECK_EQ(loaded.pipeline.packages().size(), 2U);
    CHECK_EQ(loaded.pipeline.packages().at("a").sourceRepo.branch, "master");
    CHECK_EQ(loaded.pipeline.packages().at("a").sourceRepo.repo, "a");
    CHECK(loaded.pipeline.prerequisites("b") == std::vector<std::string>{"a"});
    CHECK_EQ(loaded.catalog.at("t").family, "deb");
    CHECK_EQ(loaded.packages.at("a").title, "a title");
    CHECK(valid.maximumPending >= 2);
    CHECK(valid.sourceHeads == 0);
    for (const auto& ref : valid.refs) {
        CHECK_EQ(ref, std::string(40, 'b'));
    }
    const auto bad = [&](std::string reg, std::string a, std::string cat = "") {
        const auto result = load(reg, a, source("b", {"a"}).dump(), cat.empty() ? catalog : cat);
        CHECK(std::holds_alternative<Error>(result.value));
    };
    for (const auto& value : {"{}", "[]", "{", "{\"schema\":1,\"schema\":1}", "{\"schema\":false}"}) {
        bad(value, source("a").dump());
        bad(registry.dump(), value);
    }
    for (const auto& field : {"schema", "package", "title", "depends_on", "build", "build_dependencies", "targets"}) {
        auto missing = source("a");
        missing.erase(field);
        bad(registry.dump(), missing.dump());
        auto wrong = source("a");
        wrong[field] = nullptr;
        bad(registry.dump(), wrong.dump());
    }
    for (const auto& field : {"depends_on", "targets"}) {
        auto wrong = source("a");
        wrong[field] = Json::array({"missing"});
        bad(registry.dump(), wrong.dump());
        wrong[field] = Json::array({"t", "t"});
        bad(registry.dump(), wrong.dump());
    }
    for (const auto& base : std::vector<Json>{-1, 1000001, 1.5, true, nullptr, "1"}) {
        auto wrong = source("a");
        wrong["revision_base"] = base;
        bad(registry.dump(), wrong.dump());
    }
    for (const auto base : {0U, 100U, 1000000U}) {
        auto changed = source("a");
        changed["revision_base"] = base;
        const auto result = load(registry.dump(), changed.dump(), source("b", {"a"}).dump(), catalog);
        REQUIRE(std::holds_alternative<LoadedDeclarations>(result.value));
        CHECK_EQ(std::get<LoadedDeclarations>(result.value).pipeline.packages().at("a").revisionBase, base);
    }
    for (const auto& path : {"../a", "/a", "a//b", "a?b", ""}) {
        auto changed = source("a");
        changed["build"]["deb"] = path;
        bad(registry.dump(), changed.dump());
        CHECK(std::holds_alternative<Error>(registryFile("o/r:" + std::string(path))));
    }
    auto duplicate = registry;
    duplicate["repositories"] = {"o/a", "O/A"};
    bad(duplicate.dump(), source("a").dump());
    auto cat = Json::parse(catalog);
    cat["targets"].push_back(cat["targets"][0]);
    bad(registry.dump(), source("a").dump(), cat.dump());
    for (const auto& field : {"id", "family", "distribution", "release", "codename", "arch", "image", "runner", "platform", "emulated"}) {
        cat = Json::parse(catalog);
        cat["targets"][0].erase(field);
        bad(registry.dump(), source("a").dump(), cat.dump());
    }
    bad(registry.dump(), std::string(1024U * 1024U + 1, ' '));
    CHECK(std::holds_alternative<Error>(load(registry.dump(), source("a").dump(), source("b").dump(), catalog, "normal", 404).value));
    for (const auto& branch : {"", "a..b", "a//b", "a.lock/b", "HEAD"}) {
        CHECK(std::holds_alternative<Error>(
            load(registry.dump(), source("a").dump(), source("b").dump(), catalog, "normal", 200, branch).value));
    }
    for (const auto& mode : {"pages", "off-origin", "loop", "malformed", "empty"}) {
        const auto result = load(registry.dump(), source("a").dump(), source("b").dump(), catalog, mode);
        const bool accepted = std::string(mode) == "pages" || std::string(mode) == "empty";
        CHECK(std::holds_alternative<LoadedDeclarations>(result.value) == accepted);
        if (accepted) {
            const auto& declarations = std::get<LoadedDeclarations>(result.value);
            CHECK(declarations.releases.empty() == (std::string(mode) == "empty"));
            for (const auto& ref : result.refs) {
                CHECK_EQ(ref, std::string(40, std::string(mode) == "empty" ? 'a' : 'c'));
            }
        }
    }
    const auto real = load(registry.dump(), read("snodec-packages.json"), read("mqttsuite-packages.json"), catalog);
    REQUIRE(std::holds_alternative<LoadedDeclarations>(real.value));
    CHECK_EQ(std::get<LoadedDeclarations>(real.value).pipeline.packages().at("snodec").targets.size(), 76U);
    return checkResult();
}
