#include "SourceReader.h"

#include <core/SNodeC.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utils/Config.h>

namespace {
    void require(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }
} // namespace

int main(int argc, char* argv[]) {
    auto* config = utils::Config::configRoot.newSubCommand<coreci::GitHubConfig>();
    coreci::GitHub github(*config);
    coreci::SourceReader sources(github);
    core::SNodeC::init(argc, argv);

    bool success = false;
    const auto* configuredCase = std::getenv("CORE_CI_SOURCE_CASE");
    const std::string testCase = configuredCase == nullptr ? "success" : configuredCase;
    try {
        github.loadCredentials();
        sources.select("Owner/example", testCase == "invalid-tag" ? "main" : "", [&](const coreci::SourceResult& result) {
            if (testCase == "success") {
                require(result.status == 200 && result.source.at("tag") == "v2.0.0-rc.2" &&
                            result.source.at("commit") == std::string(40, 'd'),
                        "highest numeric/prerelease tag was not selected across pages");
                const auto frozen = result.source;
                sources.select("Owner/example", "v0.9.0+build", [&, frozen](const coreci::SourceResult& older) {
                    require(older.status == 200 && older.source.at("tag") == "v0.9.0+build" &&
                                older.source.at("commit") == std::string(40, 'c') &&
                                older.source.at("definition_revision") == std::string(40, 'c') &&
                                older.source.at("tag_object_sha") == std::string(40, 'a') &&
                                older.source.at("definition").at("title") == "from-c",
                            "exact older annotated tag/definition was substituted");
                    sources.select("Owner/example", "", [&, frozen](const coreci::SourceResult& moved) {
                        require(moved.status == 200 && moved.source.at("commit") == std::string(40, 'e') &&
                                    moved.source.at("definition").at("title") == "from-e",
                                "moved tag did not create a new pinned source selection");
                        require(frozen.at("commit") == std::string(40, 'd') && frozen.at("definition").at("title") == "from-d",
                                "later tag move rewrote previously selected source/definition inputs");
                        success = true;
                    });
                });
            } else {
                const int expected = testCase == "ambiguous" ? 409 : testCase == "no-tags" ? 404 : 422;
                require(result.status == expected && !result.error.empty(), "invalid/ambiguous source was accepted");
                success = true;
            }
        });
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    const int result = core::SNodeC::start();
    if (success) {
        std::cout << "PASS local source selection " << testCase << '\n';
    }
    return success ? result : 1;
}
