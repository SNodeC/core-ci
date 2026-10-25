#ifndef CORE_CI_GRAPH_FIXTURES_H
#define CORE_CI_GRAPH_FIXTURES_H

#include "Graph.h"

namespace graph_fixture {
    inline nlohmann::json source(const std::string& name, std::uint64_t id, const std::vector<std::string>& dependencies) {
        nlohmann::json matrix = nlohmann::json::array();
        for (const auto* arch : {"first", "second"}) {
            const std::string row = std::string("target-") + arch;
            matrix.push_back({{"id", row},
                              {"family", "native"},
                              {"distribution", "configured"},
                              {"suite", "series"},
                              {"arch", arch},
                              {"runner", "configured-runner"},
                              {"build", {{"image", "configured:tag"}, {"platform", std::string("linux/") + arch}, {"format", "deb"}}}});
        }
        return {
            {"name", name},
            {"repository", "Owner/" + name},
            {"repository_id", id},
            {"tag", "v1.0.0"},
            {"commit", std::string(40, 'a')},
            {"definition_revision", std::string(40, 'a')},
            {"definition_blob_sha", std::string(40, 'b')},
            {"definition",
             {{"schema", 1},
              {"title", name},
              {"package", name + "pkg"},
              {"depends_on", dependencies},
              {"build", {{"native", "ci/native.sh"}}},
              {"matrix", matrix}}},
            {"deployment",
             {{"worker", {{"repository_id", 1000}, {"repository", "Owner/worker"}, {"tooling_sha", std::string(40, 'd')}}},
              {"source_workflow", {{"path", ".github/workflows/core-ci-build.yml"}, {"ref", "main"}, {"blob_sha", std::string(40, 'c')}}},
              {"publication_workflow",
               {{"path", ".github/workflows/core-ci-publish.yml"}, {"ref", "main"}, {"blob_sha", std::string(40, 'd')}}},
              {"controller_app_bot_id", 42},
              {"feed", {{"repository", "Owner/feed"}, {"repository_id", 2000}, {"branch", "main"}}}}}};
    }

} // namespace graph_fixture

#endif
