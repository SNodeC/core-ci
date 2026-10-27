#include "PipelineConfig.h"

#include "HttpsUrl.h"
#include "SourceReader.h"
#include "UiAssets.h"

#include <regex>
#include <stdexcept>

namespace coreci {
    PipelineConfig::PipelineConfig(utils::SubCommand* parent)
        : SubCommand(parent, this, "Package pipeline deployment") {
        setConfigurable(
            addOptionVariable("--publication-limit", publicationLimit, "Assigned publications per feed", "uint", 8u, CLI::Range(1u, 32u)),
            true);
        setConfigurable(addOptionVariable("--status-workflow",
                                          statusWorkflow,
                                          "Optional trusted public status workflow path",
                                          "path",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--status-workflow-blob",
                                          statusWorkflowBlob,
                                          "Approved public status workflow blob SHA",
                                          "sha",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--worker-repository",
                                          workerRepository,
                                          "Trusted worker owner/repository",
                                          "repository",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(
            addOptionVariable(
                "--tooling-sha", toolingSha, "Reviewed immutable worker tooling commit", "sha", CLI::TypeValidator<std::string>()),
            true);
        setConfigurable(addOptionVariable("--publication-workflow",
                                          publicationWorkflow,
                                          "Trusted publisher workflow path",
                                          "path",
                                          std::string(".github/workflows/core-ci-publish.yml"),
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--publication-ref",
                                          publicationRef,
                                          "Authorized publisher dispatch branch or tag",
                                          "ref",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--publication-workflow-blob",
                                          publicationWorkflowBlob,
                                          "Approved publisher workflow Git blob SHA",
                                          "sha",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--source-workflow",
                                          sourceWorkflow,
                                          "Approved source build workflow path",
                                          "path",
                                          std::string(".github/workflows/core-ci-build.yml"),
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--source-workflow-blob",
                                          sourceWorkflowBlob,
                                          "Approved source workflow Git blob SHA",
                                          "sha",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--feed-repository",
                                          feedRepository,
                                          "Feed owner/repository (separate feed-writer access)",
                                          "repository",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(
            addOptionVariable(
                "--feed-id", feedId, "Feed immutable repository ID", "uint64", std::uint64_t{0}, CLI::TypeValidator<std::uint64_t>()),
            true);
        setConfigurable(
            addOptionVariable("--feed-branch", feedBranch, "Explicit feed publication branch", "branch", CLI::TypeValidator<std::string>()),
            true);
        setConfigurable(addOptionVariable("--feed-base-url",
                                          feedBaseUrl,
                                          "Actual package-compatible public HTTPS base URL",
                                          "url",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--gpg-fingerprint",
                                          gpgFingerprint,
                                          "Trusted APT/RPM OpenPGP fingerprint",
                                          "fingerprint",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(
            addOptionVariable(
                "--usign-key-sha256", usignKeySha256, "Reviewed IPK public-key file SHA-256", "sha256", CLI::TypeValidator<std::string>()),
            true);
        setConfigurable(
            addOptionVariable(
                "--apk-key-sha256", apkKeySha256, "Reviewed APK public-key file SHA-256", "sha256", CLI::TypeValidator<std::string>()),
            true);
    }

    std::string PipelineConfig::sourceWorkflowContent() const {
        std::string content = ui::sourceWorkflow;
        for (const auto& [key, value] : std::map<std::string, std::string>{{"@WORKER@", workerRepository}, {"@TOOLING_SHA@", toolingSha}}) {
            const auto position = content.find(key);
            if (position == std::string::npos) {
                throw std::invalid_argument("workflow template substitution is missing");
            }
            content.replace(position, key.size(), value);
        }
        if (SourceReader::blobSha(content) != sourceWorkflowBlob) {
            throw std::invalid_argument("rendered source workflow differs from the configured approved blob SHA");
        }
        return content;
    }

    nlohmann::json PipelineConfig::deployment(const GitHub& github, const GitHubConfig& app, const nlohmann::json& repository) const {
        static const std::regex sha(R"(^[a-f0-9]{40}$)");
        static const std::regex hash(R"(^[a-f0-9]{64}$)");
        static const std::regex fingerprint(R"(^([A-F0-9]{40}|[A-F0-9]{64})$)");
        static const std::regex workflow(R"(^\.github/workflows/[A-Za-z0-9_-]+\.ya?ml$)");
        if (!GitHub::validRepository(workerRepository) || !GitHub::validRepository(feedRepository) || workerRepository == feedRepository ||
            workerRepository == repository.at("repository").get<std::string>() ||
            feedRepository == repository.at("repository").get<std::string>() || feedId == 0 || app.botId == 0) {
            throw std::invalid_argument(
                "configure distinct pipeline worker/feed repositories, feed ID and github.bot-id before selecting work");
        }
        const auto workerId = github.repositoryId(workerRepository);
        if (workerId == feedId || workerId == repository.at("id") || feedId == repository.at("id") ||
            github.repositoryId(repository.at("repository").get<std::string>()) != repository.at("id")) {
            throw std::invalid_argument("pipeline repository roles/IDs conflict with configured GitHub grants");
        }
        if (!std::regex_match(toolingSha, sha) || !std::regex_match(sourceWorkflowBlob, sha) ||
            !std::regex_match(publicationWorkflowBlob, sha) || !std::regex_match(sourceWorkflow, workflow) ||
            !std::regex_match(publicationWorkflow, workflow) || publicationRef.empty() || publicationRef.size() > 256 ||
            feedBranch.empty() || feedBranch.size() > 256) {
            throw std::invalid_argument("configure reviewed pipeline tooling/workflow identities and explicit publication/feed refs");
        }
        sourceWorkflowContent();
        const HttpsUrl feed(feedBaseUrl);
        if (feed.path.find('?') != std::string::npos || !std::regex_match(gpgFingerprint, fingerprint) ||
            !std::regex_match(usignKeySha256, hash) || !std::regex_match(apkKeySha256, hash)) {
            throw std::invalid_argument("configure the actual feed HTTPS base URL and all trusted public signing-key identities");
        }
        return {{"controller_app_bot_id", app.botId},
                {"worker", {{"repository", workerRepository}, {"repository_id", workerId}, {"tooling_sha", toolingSha}}},
                {"source_workflow", {{"path", sourceWorkflow}, {"ref", repository.at("branch")}, {"blob_sha", sourceWorkflowBlob}}},
                {"publication_workflow", {{"path", publicationWorkflow}, {"ref", publicationRef}, {"blob_sha", publicationWorkflowBlob}}},
                {"feed", {{"repository", feedRepository}, {"repository_id", feedId}, {"branch", feedBranch}, {"base_url", feedBaseUrl}}},
                {"signing", {{"gpg_fingerprint", gpgFingerprint}, {"usign_key_sha256", usignKeySha256}, {"apk_key_sha256", apkKeySha256}}}};
    }
} // namespace coreci
