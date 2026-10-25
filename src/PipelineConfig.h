#ifndef CORE_CI_PIPELINE_CONFIG_H
#define CORE_CI_PIPELINE_CONFIG_H

#include "GitHub.h"

namespace coreci {
    class PipelineConfig : public utils::SubCommand {
    public:
        constexpr static std::string_view NAME{"pipeline"};
        constexpr static std::string_view DESCRIPTION{"Trusted worker, workflows, feed and public signing identities"};
        explicit PipelineConfig(utils::SubCommand* parent);
        nlohmann::json deployment(const GitHub& github, const GitHubConfig& app, const nlohmann::json& repository) const;

        std::string workerRepository;
        std::string toolingSha;
        std::string publicationWorkflow;
        std::string publicationRef;
        std::string publicationWorkflowBlob;
        std::string sourceWorkflow;
        std::string sourceWorkflowBlob;
        std::string feedRepository;
        std::uint64_t feedId = 0;
        std::string feedBranch;
        std::string feedBaseUrl;
        std::string gpgFingerprint;
        std::string usignKeySha256;
        std::string apkKeySha256;
    };
} // namespace coreci

#endif
