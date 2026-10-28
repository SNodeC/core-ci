#pragma once

#include "Graph.h"

namespace coreci::test {
    inline Json sources() {
        auto result = Json::object();
        unsigned int repository = 1;
        for (const auto* name : {"core", "app", "side", "join"}) {
            auto dependencies = Json::array();
            if (std::string(name) != "core") {
                dependencies.push_back("core");
            }
            if (std::string(name) == "join") {
                dependencies.push_back("app");
                dependencies.push_back("side");
            }
            auto matrix = Json::array();
            for (const auto* arch : {"amd64", "arm64"}) {
                matrix.push_back({{"id", std::string("debian-") + arch},
                                  {"family", "native"},
                                  {"distribution", "debian"},
                                  {"suite", "trixie"},
                                  {"arch", arch},
                                  {"runner", "ubuntu-latest"},
                                  {"build", {{"format", "deb"}, {"platform", std::string("linux/") + arch}, {"image", "debian:trixie"}}}});
            }
            matrix.push_back({{"id", "fedora"},
                              {"family", "native"},
                              {"distribution", "fedora"},
                              {"suite", "43"},
                              {"arch", "x86_64"},
                              {"runner", "ubuntu-latest"},
                              {"build", {{"format", "rpm"}, {"platform", "linux/amd64"}, {"image", "fedora:43"}}}});
            for (const auto& [release, format] : {std::pair{"24.10", "ipk"}, std::pair{"25.12", "apk"}}) {
                matrix.push_back({{"id", std::string("openwrt-") + format},
                                  {"family", "openwrt"},
                                  {"distribution", "openwrt"},
                                  {"suite", release},
                                  {"arch", "x86_64"},
                                  {"runner", "ubuntu-latest"},
                                  {"build", {{"format", format}, {"target", "x86/64"}}}});
            }
            result[name] = {
                {"name", name},
                {"repository", std::string("Owner/") + name},
                {"repository_id", repository++},
                {"tag", "v2.1.3"},
                {"commit", std::string(40, 'a')},
                {"tag_object_sha", std::string(40, 'a')},
                {"definition_revision", std::string(40, 'a')},
                {"definition_blob_sha", std::string(40, 'b')},
                {"deployment", {{"worker", {{"tooling_sha", std::string(40, 'c')}}}, {"feed", {{"repository", "Owner/packages"}}}}},
                {"definition",
                 {{"schema", 1},
                  {"title", name},
                  {"package", name},
                  {"depends_on", dependencies},
                  {"build", {{"native", "ci/build.sh"}, {"openwrt", "ci/openwrt.sh"}}},
                  {"matrix", matrix}}}};
        }
        return result;
    }

} // namespace coreci::test
