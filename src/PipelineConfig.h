#pragma once

#include "GitHub.h"
#include "SourceReader.h"

namespace coreci {
    class PipelineConfig : public utils::SubCommand {
    public:
        static constexpr std::string_view NAME{"pipeline"};
        static constexpr std::string_view DESCRIPTION{"Trusted worker and publication deployment"};
        explicit PipelineConfig(utils::SubCommand* parent);
        Json deployment(const GitHubConfig& app, const GitHub& github, const std::string& branch) const;
        std::string sourceContent() const;
        std::string workerRepository, toolingSha, sourceWorkflow = ".github/workflows/core-ci-build.yml", sourceWorkflowBlob;
        std::string publicationWorkflow = ".github/workflows/core-ci-publish.yml", publicationRef = "main", publicationWorkflowBlob;
        std::string statusWorkflow, statusWorkflowBlob, feedRepository, feedBranch = "main", feedBaseUrl;
        std::string gpgFingerprint, usignKeySha256, apkKeySha256;
        std::uint64_t feedId = 0;
    };
} // namespace coreci
