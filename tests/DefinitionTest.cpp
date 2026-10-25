#include "Definition.h"

#include "Version.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void check(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    void rejects(const std::function<void()>& operation) {
        bool rejected = false;
        try {
            operation();
        } catch (const std::exception&) {
            rejected = true;
        }
        check(rejected, "invalid public input was accepted");
    }

    nlohmann::json definition(const std::string& package, const std::vector<std::string>& dependencies) {
        return {{"schema", 1},
                {"title", package},
                {"package", package},
                {"depends_on", dependencies},
                {"build", {{"native", "ci/native.sh"}}},
                {"matrix",
                 {{{"id", "local-label"},
                   {"family", "native"},
                   {"distribution", "example"},
                   {"suite", "future"},
                   {"arch", "custom-arch"},
                   {"runner", "owner-selected-runner"},
                   {"build", {{"image", "example:future"}, {"platform", "linux/custom"}, {"format", "deb"}}}}}}};
    }
} // namespace

int main() {
    int status = 0;
    try {
        const std::vector<std::string> ordered{"1.0.0-alpha",
                                               "1.0.0-alpha.1",
                                               "1.0.0-alpha.beta",
                                               "1.0.0-beta",
                                               "1.0.0-beta.2",
                                               "1.0.0-beta.11",
                                               "1.0.0-rc.1",
                                               "1.0.0",
                                               "1.2.0",
                                               "1.10.0",
                                               "2.0.0-rc.1",
                                               "2.0.0",
                                               "999999999999999999999999.0.0"};
        for (std::size_t index = 1; index < ordered.size(); ++index) {
            check(coreci::Version(ordered[index - 1]).compare(coreci::Version(ordered[index])) < 0, "SemVer ordering");
            check(coreci::Version(ordered[index]).compare(coreci::Version(ordered[index - 1])) > 0, "reverse SemVer ordering");
        }
        check(coreci::Version("v1.0.0+first").compare(coreci::Version("1.0.0+second")) == 0, "metadata changes precedence");
        for (const auto* invalid : {"main", "1.2", "01.2.3", "1.2.3-01", "1.2.3-", "1.2.3+"}) {
            rejects([&]() {
                coreci::Version version(invalid);
            });
        }

        auto core = definition("demo-core", {});
        auto app = definition("demo-app", {"demo-core"});
        app["matrix"][0]["id"] = "different-repository-local-label";
        coreci::Definition::validateGraph({{"demo-core", coreci::Definition::parse(core)}, {"demo-app", coreci::Definition::parse(app)}});
        check(coreci::Definition::parse(app).json == app, "worker data was rewritten");
        auto named = app;
        named["depends_on"] = {"Mixed_Name"};
        coreci::Definition::validateGraph({{"Mixed_Name", coreci::Definition::parse(core)}, {"app", coreci::Definition::parse(named)}});

        auto overlap = app;
        overlap["package"] = "demo-core-extra";
        rejects([&]() {
            coreci::Definition::validateGraph(
                {{"demo-core", coreci::Definition::parse(core)}, {"demo-app", coreci::Definition::parse(overlap)}});
        });

        auto wrong = app;
        wrong["matrix"].push_back(wrong["matrix"][0]);
        rejects([&]() {
            coreci::Definition::parse(wrong);
        });
        wrong = app;
        wrong["build"]["native"] = "ci/../../escape";
        rejects([&]() {
            coreci::Definition::parse(wrong);
        });
        wrong = app;
        wrong["matrix"][0]["build"]["arch"] = "duplicated";
        rejects([&]() {
            coreci::Definition::parse(wrong);
        });
        wrong = app;
        wrong["matrix"][0]["arch"] = "other-arch";
        rejects([&]() {
            coreci::Definition::validateGraph(
                {{"demo-core", coreci::Definition::parse(core)}, {"demo-app", coreci::Definition::parse(wrong)}});
        });
        rejects([&]() {
            coreci::Definition::validateGraph({{"demo-app", coreci::Definition::parse(app)}});
        });
        core["depends_on"] = {"demo-app"};
        rejects([&]() {
            coreci::Definition::validateGraph(
                {{"demo-core", coreci::Definition::parse(core)}, {"demo-app", coreci::Definition::parse(app)}});
        });

        auto sdk = definition("demo-sdk", {});
        sdk["build"] = {{"openwrt", "ci/openwrt.sh"}};
        auto& row = sdk["matrix"][0];
        row["family"] = "openwrt";
        row["distribution"] = "openwrt";
        row["build"] = {{"target", "custom/target"}, {"format", "apk"}};
        coreci::Definition::parse(sdk);
        row["build"]["sdk"]["sha256"] = "not source-owned";
        rejects([&]() {
            coreci::Definition::parse(sdk);
        });
        std::cout << "PASS authoritative definitions, target graph validation and numeric SemVer\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    return status;
}
