#include "PipelineConfig.h"

#include "UiAssets.h"

#include <regex>

namespace coreci {
    PipelineConfig::PipelineConfig(utils::SubCommand* parent)
        : SubCommand(parent, this, "Package pipeline") {
        const std::pair<const char*, std::string&> options[]{{"worker-repository", workerRepository},
                                                             {"tooling-sha", toolingSha},
                                                             {"source-workflow", sourceWorkflow},
                                                             {"source-workflow-blob", sourceWorkflowBlob},
                                                             {"publication-workflow", publicationWorkflow},
                                                             {"publication-ref", publicationRef},
                                                             {"publication-workflow-blob", publicationWorkflowBlob},
                                                             {"status-workflow", statusWorkflow},
                                                             {"status-workflow-blob", statusWorkflowBlob},
                                                             {"feed-repository", feedRepository},
                                                             {"feed-branch", feedBranch},
                                                             {"feed-base-url", feedBaseUrl},
                                                             {"gpg-fingerprint", gpgFingerprint},
                                                             {"usign-key-sha256", usignKeySha256},
                                                             {"apk-key-sha256", apkKeySha256}};
        for (const auto& [name, value] : options) {
            setConfigurable(addOptionVariable(std::string("--") + name, value, name, "string", value, CLI::TypeValidator<std::string>()),
                            true);
        }
        setConfigurable(
            addOptionVariable("--feed-id", feedId, "Feed repository ID", "id", std::uint64_t{0}, CLI::TypeValidator<std::uint64_t>()),
            true);
    }
    std::string PipelineConfig::sourceContent() const {
        auto result = std::string(ui::sourceWorkflow);
        for (const auto& [marker, value] :
             std::map<std::string, std::string>{{"@WORKER@", workerRepository}, {"@TOOLING_SHA@", toolingSha}}) {
            std::size_t position = 0;
            while ((position = result.find(marker, position)) != std::string::npos) {
                result.replace(position, marker.size(), value);
                position += value.size();
            }
        }
        return result;
    }
    Json PipelineConfig::deployment(const GitHubConfig& app, const GitHub& github, const std::string& branch) const {
        static const std::regex sha("^[a-f0-9]{40}$"), fingerprint("^[A-F0-9]{40}$"), sha256("^[a-f0-9]{64}$");
        require(GitHub::validRepository(workerRepository) && GitHub::validRepository(feedRepository) && feedId && app.botId &&
                    std::regex_match(toolingSha, sha) && std::regex_match(sourceWorkflowBlob, sha) &&
                    std::regex_match(publicationWorkflowBlob, sha) && std::regex_match(gpgFingerprint, fingerprint) &&
                    std::regex_match(usignKeySha256, sha256) && std::regex_match(apkKeySha256, sha256) && !branch.empty() &&
                    !publicationRef.empty() && !feedBranch.empty() && feedBaseUrl.starts_with("https://"),
                "configure immutable worker, workflow, feed and signing identities");
        require(SourceReader::blobSha(sourceContent()) == sourceWorkflowBlob, "rendered source workflow differs from approval");
        return {{"controller_app_bot_id", app.botId},
                {"worker",
                 {{"repository", workerRepository}, {"repository_id", github.repositoryId(workerRepository)}, {"tooling_sha", toolingSha}}},
                {"source_workflow", {{"path", sourceWorkflow}, {"ref", branch}, {"blob_sha", sourceWorkflowBlob}}},
                {"publication_workflow", {{"path", publicationWorkflow}, {"ref", publicationRef}, {"blob_sha", publicationWorkflowBlob}}},
                {"feed", {{"repository", feedRepository}, {"repository_id", feedId}, {"branch", feedBranch}, {"base_url", feedBaseUrl}}},
                {"signing", {{"gpg_fingerprint", gpgFingerprint}, {"usign_key_sha256", usignKeySha256}, {"apk_key_sha256", apkKeySha256}}}};
    }
} // namespace coreci
