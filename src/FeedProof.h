#ifndef CORE_CI_FEED_PROOF_H
#define CORE_CI_FEED_PROOF_H

#include "Graph.h"
#include "PipelineConfig.h"
#include "SourceReader.h"

namespace coreci {
    struct FeedResult {
        nlohmann::json receipt;
        std::string error;
    };

    class FeedProof {
    public:
        using Callback = std::function<void(const FeedResult&)>;
        FeedProof(const PipelineConfig& config, GitHub& github, SourceReader& sources);
        void configure();
        void verify(const Node& node, const Callback& callback);

    private:
        void read(const Node& node, const std::string& revision, const std::string& path, const Callback& callback);
        const PipelineConfig& config;
        GitHub& github;
        SourceReader& sources;
        HttpClient http;
    };
} // namespace coreci

#endif
