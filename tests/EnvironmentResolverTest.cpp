#include "EnvironmentResolver.h"

#include <core/SNodeC.h>
#include <cstdlib>
#include <iostream>
#include <utils/Config.h>

int main(int argc, char* argv[]) {
    auto* config = utils::Config::configRoot.newSubCommand<coreci::RegistryConfig>();
    coreci::EnvironmentResolver resolver(*config);
    core::SNodeC::init(argc, argv);

    const std::string testCase = std::getenv("CORE_CI_ENV_CASE");
    const std::string sdkUrl = std::getenv("CORE_CI_ENV_SDK");
    nlohmann::json row{{"id", "native"},
                       {"family", "native"},
                       {"distribution", "source-defined"},
                       {"suite", "series"},
                       {"arch", "source-defined"},
                       {"runner", {"self-hosted", "source-defined"}},
                       {"build", {{"image", "sample:series"}, {"platform", "linux/amd64"}, {"format", "deb"}}}};
    nlohmann::json sdkRow{{"id", "sdk"},
                          {"family", "openwrt"},
                          {"distribution", "openwrt"},
                          {"suite", "series"},
                          {"arch", "source-defined"},
                          {"runner", "source-defined"},
                          {"build",
                           {{"release", "exact-patch"},
                            {"target", "selected/subtarget"},
                            {"format", "apk"},
                            {"sdk", {{"url", sdkUrl}, {"sha256", std::string(64, 'a')}}}}}};
    nlohmann::json source{{"definition",
                           {{"schema", 1},
                            {"title", "Example"},
                            {"package", "example"},
                            {"depends_on", nlohmann::json::array()},
                            {"build", {{"native", "ci/native.sh"}, {"openwrt", "ci/openwrt.sh"}}},
                            {"matrix", {row, sdkRow}}}}};
    if (testCase == "public-image") {
        source["definition"]["matrix"] = {row};
        source["definition"]["matrix"][0]["build"]["image"] = "debian:trixie";
    }
    int calls = 0;
    bool success = false;
    try {
        resolver.configure();
        resolver.resolve(source, [&](const coreci::EnvironmentResult& result) {
            ++calls;
            if (testCase == "public-image") {
                success =
                    result.error.empty() && result.source.at("environments").at("native").at("digest").get<std::string>().size() == 71;
                if (success) {
                    std::cout << result.source.at("environments").dump() << '\n';
                } else {
                    std::cerr << result.error << '\n';
                }
            } else if (testCase == "success" || testCase == "single" || testCase == "redirect") {
                if (!result.error.empty()) {
                    std::cerr << result.error << '\n';
                } else {
                    const auto& environment = result.source.at("environments").at("native");
                    success =
                        result.source.at("definition") == source.at("definition") && environment.at("image") == "sample:series" &&
                        environment.at("platform") == "linux/amd64" && environment.at("digest") == std::getenv("CORE_CI_ENV_MANIFEST") &&
                        environment.at("resolved_image").get<std::string>().ends_with("@" + environment.at("digest").get<std::string>()) &&
                        result.source.at("environments").at("sdk").at("sdk") == sdkRow.at("build").at("sdk");
                }
            } else {
                success = result.source.is_null() && !result.error.empty();
            }
        });
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    const int result = core::SNodeC::start();
    if (success && calls == 1) {
        std::cout << "PASS immutable environment " << testCase << '\n';
    }
    return success && calls == 1 ? result : 1;
}
