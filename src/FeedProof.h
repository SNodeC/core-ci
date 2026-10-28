#pragma once

#include "PipelineConfig.h"

namespace coreci {
    class FeedProof {
    public:
        using Callback = std::function<void(const SourceResult&)>;
        FeedProof(const PipelineConfig& config, GitHub& github, SourceReader& sources);
        void configure();
        void verify(const Json& node, const Callback& callback);

    private:
        const PipelineConfig& config;
        GitHub& github;
        SourceReader& sources;
        HttpClient http{"feed-http"};
    };
} // namespace coreci
