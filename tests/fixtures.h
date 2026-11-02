#ifndef CI_TEST_FIXTURES_H
#define CI_TEST_FIXTURES_H

#include "Auth.h"
#include "Controller.h"

namespace fixture {

    using ci::Json;

    inline Json deployment() {
        return {
            {"owner", "Fixture"},
            {"controller_app_bot_id", 41},
            {"repositories", {{"Fixture/worker", 42}}},
            {"worker", {{"repository", "Fixture/worker"}, {"repository_id", 42}, {"tooling_sha", std::string(40, 'a')}}},
            {"source_workflow", {{"path", ".github/workflows/build.yml"}, {"blob_sha", std::string(40, 'b')}}},
            {"publication_workflow", {{"path", ".github/workflows/publish.yml"}, {"blob_sha", std::string(40, 'c')}, {"ref", "reviewed"}}},
            {"feed",
             {{"repository", "Fixture/feed"},
              {"repository_id", 43},
              {"branch", "reviewed"},
              {"base_url", "https://fixture.invalid/feed/"}}},
            {"signing",
             {{"gpg_fingerprint", std::string(40, 'd')},
              {"usign_key_sha256", std::string(64, 'e')},
              {"apk_key_sha256", std::string(64, 'f')}}}};
    }

    inline Json target() {
        return {{"id", "linux-example"},
                {"family", "native"},
                {"distribution", "example"},
                {"suite", "stable"},
                {"arch", "amd64"},
                {"runner", "assigned-runner"},
                {"build", {{"format", "deb"}, {"platform", "linux/amd64"}, {"image", "example:stable"}}}};
    }

    inline Json selected(const std::string& name, const Json& dependencies, const std::string& tag = "v1.0.0") {
        const Json source = {{"repository", "Fixture/" + name},
                             {"repository_id", 100},
                             {"package", name},
                             {"title", name},
                             {"tag", tag},
                             {"commit", std::string(40, '1')},
                             {"definition_revision", std::string(40, '1')},
                             {"definition_blob_sha", std::string(40, '2')},
                             {"depends_on", dependencies},
                             {"build", {{"native", "ci/build.sh"}}},
                             {"deployment", deployment()}};
        return {{"source", source},
                {"definition", {{"schema", 1}, {"package", name}, {"depends_on", dependencies}, {"matrix", Json::array({target()})}}},
                {"workflows",
                 {{"build",
                   {{"repository", "Fixture/" + name},
                    {"repository_id", 100},
                    {"ref", "reviewed"},
                    {"path", ".github/workflows/build.yml"},
                    {"blob_sha", std::string(40, 'b')},
                    {"commit", std::string(40, '3')}}},
                  {"publish",
                   {{"repository", "Fixture/worker"},
                    {"repository_id", 42},
                    {"ref", "reviewed"},
                    {"path", ".github/workflows/publish.yml"},
                    {"blob_sha", std::string(40, 'c')},
                    {"commit", std::string(40, '4')}}}}}};
    }

    inline Json graph() {
        return {{"repositories", Json::object()}, {"executions", Json::array()}};
    }

    inline Json& work(Json& graph, const std::string& id) {
        Json* found = nullptr;
        for (auto& execution : graph.at("executions")) {
            if (execution.at("id") == id) {
                found = &execution;
            }
        }
        ci::require(found != nullptr, "Fixture execution missing");
        return *found;
    }

    inline Json observation(const Json& work,
                            const std::string& stage,
                            const std::string& status = "completed",
                            const std::string& conclusion = "success") {
        const auto& command = work.at("workflows").at(stage);
        const int run = stage == "build" ? 501 : 502;
        const int job = stage == "build" ? 701 : 702;
        const Json selected = {
            {"id", job},
            {"run_id", run},
            {"run_attempt", 1},
            {"name",
             (stage == "build" ? "Build " : "Publish ") + work.at("attempts").back().at("assignment").at("label").get<std::string>()},
            {"status", status},
            {"conclusion", conclusion},
            {"html_url", "https://github.com/Fixture/worker/actions/runs/502/job/702"},
            {"started_at", status == "queued" ? Json() : Json("2026-10-31T09:00:00Z")},
            {"completed_at", status == "completed" ? Json("2026-10-31T09:05:00Z") : Json()},
            {"steps", Json::array()}};
        return {{"identity",
                 {{"id", run},
                  {"run_attempt", 1},
                  {"repository_id", command.at("repository_id")},
                  {"actor_id", 41},
                  {"workflow_blob_sha", command.at("blob_sha")},
                  {"workflow_commit", command.at("commit")},
                  {"html_url", "https://github.com/Fixture/worker/actions/runs/502"}}},
                {"status", status},
                {"conclusion", conclusion},
                {"jobs", Json::array({selected})},
                {"job_id", job}};
    }

    inline Json buildEvidence(const Json& work) {
        const auto& assignment = work.at("attempts").back().at("assignment");
        std::string tag = work.at("source").at("tag");
        if (tag.front() == 'v') {
            tag.erase(0, 1);
        }
        const std::string release = assignment.at("revision").dump();
        const auto& target = work.at("target");
        Json manifest = assignment;
        manifest.erase("label");
        manifest["assignment_sha256"] = ci::hash(assignment.dump());
        manifest["package_version"] = {{"version", tag}, {"release", release}, {"full", tag + "-" + release}};
        manifest["environment"] = {{"image", target.at("build").at("image")},
                                   {"platform", "linux/amd64"},
                                   {"digest", "sha256:" + std::string(64, 'a')},
                                   {"config_digest", "sha256:" + std::string(64, 'b')},
                                   {"resolved_image", "example@sha256:" + std::string(64, 'a')}};
        manifest["packages"] = Json::array({{{"file", "example.deb"},
                                             {"component", work.at("source").at("package")},
                                             {"name", work.at("source").at("package")},
                                             {"version", tag + "-" + release},
                                             {"arch", target.at("arch")},
                                             {"dependencies", Json::array({"runtime-lib (>= 1.0), runtime-lib (<< 3.0)"})},
                                             {"size", 7},
                                             {"sha256", ci::hash("package")}}});
        manifest["development_export"] = nullptr;
        return {{"result",
                 {{"assignment_sha256", ci::hash(assignment.dump())},
                  {"run", observation(work, "build").at("identity")},
                  {"artifacts",
                   Json::array({{{"id", 601},
                                 {"name", assignment.at("artifact_name")},
                                 {"job_id", 701},
                                 {"digest", "sha256:" + std::string(64, '1')},
                                 {"size_bytes", 100}}})}}},
                {"manifest", manifest}};
    }

    inline Json receipt(const Json& work) {
        const auto& active = work.at("attempts").back();
        const auto& assignment = active.at("assignment");
        Json source;
        for (const auto& key : {"repository", "title", "package", "tag", "definition_revision", "definition_blob_sha"}) {
            source[key] = work.at("source").at(key);
        }
        Json dependencies = Json::array();
        for (const auto& dependency : assignment.at("dependencies")) {
            Json selected;
            for (const auto& key : {"execution",
                                    "attempt",
                                    "assignment_sha256",
                                    "source_commit",
                                    "source",
                                    "revision",
                                    "compatibility",
                                    "packages",
                                    "development_export",
                                    "feed"}) {
                selected[key] = dependency.at("receipt").value(key, Json());
            }
            dependencies.push_back(selected);
        }
        Json packages = active.at("build_manifest").at("packages");
        for (auto& package : packages) {
            package["file"] = package.at("name").get<std::string>() + "_" + package.at("version").get<std::string>() + "_" +
                              package.at("arch").get<std::string>() + ".deb";
            package["path"] = "example/pool/stable/" + package.at("file").get<std::string>();
            package["unsigned_sha256"] = package.at("sha256");
        }
        Json buildJob;
        for (const auto& key : {"id", "run_id", "run_attempt", "html_url", "started_at", "completed_at"}) {
            buildJob[key] = active.at("facts").at("build_run").at("jobs").at(0).at(key);
        }
        return {{"schema", 1},
                {"execution", work.at("id")},
                {"attempt", active.at("id")},
                {"assignment_sha256", ci::hash(assignment.dump())},
                {"source_commit", work.at("source").at("commit")},
                {"source", source},
                {"revision", active.at("revision")},
                {"target", work.at("target")},
                {"dependencies", dependencies},
                {"run", observation(work, "publish").at("identity")},
                {"build_result", active.at("build_result")},
                {"feed", work.at("source").at("deployment").at("feed")},
                {"environment", active.at("build_manifest").at("environment")},
                {"compatibility", ci::coordinates(work.at("target"))},
                {"packages", packages},
                {"development_export", nullptr},
                {"summary", nullptr},
                {"build_job", buildJob},
                {"indexes", Json::array({{{"path", "shared/index"}, {"size", 1}, {"sha256", std::string(64, '9')}}})}};
    }

    inline Json finishBuild(Json& graph, const std::string& id) {
        const Json selected = work(graph, id);
        return ci::applyEvidence(graph, selected, "build", observation(selected, "build"), buildEvidence(selected), "");
    }

    inline Json publish(Json& graph, const std::string& id, const Json& indexes = nullptr) {
        const Json selected = work(graph, id);
        Json receipt = fixture::receipt(selected);
        if (!indexes.is_null()) {
            receipt["indexes"] = indexes;
        }
        return ci::applyEvidence(graph,
                                 selected,
                                 "publish",
                                 observation(selected, "publish"),
                                 {{"receipt", receipt},
                                  {"feed_revision", std::string(40, '5')},
                                  {"receipt_sha256", ci::hash(receipt.dump())},
                                  {"verified_at", "2026-10-31T10:00:00Z"}},
                                 "");
    }

} // namespace fixture

#endif
