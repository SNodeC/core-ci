#ifndef CORE_CI_PIPELINE_H
#define CORE_CI_PIPELINE_H

#include "Enrollment.h"
#include "FeedProof.h"
#include "PipelineConfig.h"

#include <set>

namespace coreci {
    struct PipelineResult {
        int status = 200;
        nlohmann::json body;
    };

    class Pipeline {
    public:
        using Callback = std::function<void(const PipelineResult&)>;
        Pipeline(const PipelineConfig& config,
                 const GitHubConfig& app,
                 GitHub& github,
                 SourceReader& sources,
                 Enrollment& enrollment,
                 GraphStore& store,
                 FeedProof& feeds);
        void request(const std::string& repository,
                     const std::string& tag,
                     bool automatic,
                     const Callback& callback,
                     const std::string& expectedTagObject = "");
        void event(const std::string& type, const std::string& signature, const std::string& body, const Callback& callback);
        void retry(const std::string& execution, const Callback& callback);
        void advance(const Callback& callback);
        void reconcile(const Callback& callback);
        void refreshStatus();
        void
        inspect(const std::string& execution, const std::string& attempt, bool publication, std::uint64_t runId, const Callback& callback);

    private:
        struct Selecting {
            std::string repository;
            std::string root;
            std::string tag;
            std::string expectedTagObject;
            bool automatic = false;
            std::uint64_t generation = 0;
            std::vector<nlohmann::json> repositories;
            std::size_t index = 0;
            std::map<std::string, nlohmann::json> sources;
            Callback callback;
        };

        void readSources(const std::shared_ptr<Selecting>& selection);
        void commitSelection(const std::shared_ptr<Selecting>& selection);
        void
        read(const std::string& repository, const std::string& suffix, const nlohmann::json& permissions, const GitHub::Callback& callback);
        void workflow(const std::string& repository, const nlohmann::json& definition, const SourceReader::Callback& callback);
        void proof(const std::string& repository,
                   const nlohmann::json& definition,
                   const std::string& commit,
                   const std::string& rootTree,
                   const SourceReader::Callback& callback);
        void observe(const std::string& execution,
                     const std::string& attempt,
                     bool publication,
                     const nlohmann::json& run,
                     const nlohmann::json& job,
                     const Callback& callback,
                     bool recovery = false);
        void record(const std::string& execution,
                    const std::string& attempt,
                    bool publication,
                    const nlohmann::json& identity,
                    const nlohmann::json& observation,
                    const Callback& callback,
                    bool recovery,
                    bool readJobs);
        void complete(
            const std::string& execution, const std::string& attempt, bool publication, const Callback& callback, bool readJobs = false);
        void dispatch(const Node& node, bool publication, const Callback& callback);
        void search(const std::string& execution,
                    const std::string& attempt,
                    bool publication,
                    unsigned int page,
                    const std::shared_ptr<std::vector<std::uint64_t>>& matches,
                    const Callback& callback);
        void buildArtifacts(const Node& node, const nlohmann::json& job, const Callback& callback);
        void publicationResult(const Node& node, const nlohmann::json& evidence, const Callback& callback);
        void change(const Node& node, const Callback& callback);
        static std::string title(const Attempt& attempt, bool publication, bool job = false);

        std::map<std::string, std::vector<GitHub::Callback>> reads;
        std::map<std::string, SourceResult> proofs;
        std::map<std::string, std::vector<SourceReader::Callback>> proofReads;
        std::set<std::string> completionReads;
        bool refreshing = false;
        const PipelineConfig& config;
        const GitHubConfig& app;
        GitHub& github;
        SourceReader& sources;
        Enrollment& enrollment;
        GraphStore& store;
        FeedProof& feeds;
    };
} // namespace coreci

#endif
