#include "Check.h"
#include "adapters/github/Declarations.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <tuple>

using namespace coreci;
using namespace coreci::github;
using Json = nlohmann::json;

std::string read(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::stringstream text;
    text << input.rdbuf();
    CHECK(input.good() || input.eof());
    return text.str();
}

void contract(const std::filesystem::path& directory, bool released) {
    const auto origins = Json::parse(read(directory / "origins.json"));
    std::variant<LoadedDeclarations, Error> result = Error{"unfinished"};
    std::vector<std::tuple<std::string, bool, Completion>> pending;
    std::set<std::string> declarations;
    unsigned completed = 0;
    loadDeclarations(
        {"SNodeC", "Packages", "ci/registry.json"},
        "ci/targets.json",
        {"api.example", ""},
        [&](std::string path, bool raw, Completion done) {
            pending.emplace_back(std::move(path), raw, std::move(done));
        },
        [&](auto value) {
            ++completed;
            result = std::move(value);
        });
    for (std::size_t index = 0; index < pending.size(); ++index) {
        auto [path, raw, done] = std::move(pending[index]);
        int status = 404;
        std::string body;
        for (const auto& [name, origin] : origins.items()) {
            const auto prefix = "/repos/SNodeC/" + name;
            const auto sha = origin.at("sha").get<std::string>();
            if (path == prefix) {
                body = Json{{"full_name", "SNodeC/" + name}, {"default_branch", origin.at("branch")}}.dump();
                status = 200;
            } else if (path == prefix + "/commits/" + origin.at("branch").get<std::string>()) {
                body = Json{{"sha", sha}}.dump();
                status = 200;
            } else if (path == prefix + "/tags?per_page=100") {
                body = released ? Json::array({{{"name", "v1.0.0"}, {"commit", {{"sha", sha}}}}}).dump() : "[]";
                status = 200;
            } else if (path.starts_with(prefix + "/contents/")) {
                CHECK(raw);
                CHECK(path.ends_with("?ref=" + sha));
                if (name == "Packages") {
                    const auto filename = path.find("registry.json") != std::string::npos ? "registry.json" : "targets.json";
                    body = read(directory / filename);
                } else if (path.find("/contents/?") != std::string::npos) {
                    body = Json::array({{{"name", "ci"}, {"type", "dir"}, {"sha", std::string(40, 'd')}}}).dump();
                } else {
                    CHECK(path.find("ci%2Fpackages.json") != std::string::npos);
                    declarations.insert(name);
                    auto declaration = Json::parse(read(directory / (name + ".json")));
                    declaration["schema"] = 2;
                    Json dependencies = Json::array();
                    for (const auto& prerequisite : declaration.at("depends_on")) {
                        dependencies.push_back({{"package", prerequisite}, {"version", ">=0.0.0"}});
                    }
                    declaration["depends_on"] = dependencies;
                    body = declaration.dump();
                }
                status = 200;
            }
        }
        CHECK_EQ(status, 200);
        done(status, std::move(body), {});
    }
    CHECK_EQ(completed, 1U);
    if (const auto* error = std::get_if<Error>(&result)) {
        std::cerr << error->reason << '\n';
        CHECK(false);
    } else {
        const auto& loaded = std::get<LoadedDeclarations>(result);
        CHECK_EQ(declarations.size(), 6U);
        CHECK_EQ(loaded.catalog.size(), 40U);
        CHECK_EQ(loaded.pipeline.targets().size(), 40U);
        CHECK_EQ(loaded.pipeline.packages().size(), 6U);
        CHECK_EQ(loaded.releases.size(), released ? 6U : 0U);
        std::size_t units = 0;
        for (const auto& [name, count] : std::map<std::string, std::size_t>{
                 {"snodec", 40}, {"mqttsuite", 40}, {"snodec-idp", 26}, {"core-ci", 25}, {"aisuite", 26}, {"codexui", 16}}) {
            const auto& package = loaded.pipeline.packages().at(name);
            CHECK_EQ(package.targets.size(), count);
            CHECK_EQ(package.sourceRepo.branch, origins.at(package.sourceRepo.repo).at("branch").get<std::string>());
            units += package.targets.size();
        }
        CHECK_EQ(units, 173U);
        std::cout << "Production contract: 6 packages, 40 targets, " << units << " units; "
                  << (released ? "HEADs selected as releases" : "default-branch bootstrap") << '\n';
    }
}

int main(int argc, char** argv) {
    const std::filesystem::path directory = argc == 2 ? argv[1] : CORECI_TEST_DATA;
    contract(directory, false);
    contract(directory, true);
    return checkResult();
}
