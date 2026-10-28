#pragma once

#include "Enrollment.h"
#include "FeedProof.h"

#include <optional>
#include <set>

namespace coreci {
    using PipelineResult = EnrollmentResult;
    class Pipeline {
    public:
        using Callback = std::function<void(const PipelineResult&)>;
        Pipeline(const PipelineConfig& config,
                 const GitHubConfig& app,
                 GitHub& github,
                 SourceReader& sources,
                 GraphStore& store,
                 FeedProof& feeds)
            : config(config)
            , app(app)
            , github(github)
            , sources(sources)
            , store(store)
            , feeds(feeds) {
        }
        void event(const std::string& type, const std::string& signature, const std::string& body, const Callback& callback);
        void request(const std::string& repository,
                     const std::string& tag,
                     bool automatic,
                     const Callback& callback,
                     const std::string& expectedTag = "");
        void retire(const std::string& execution, const std::string& attempt, const Callback& callback);
        void retry(const std::string& execution, const Callback& callback);
        void reconcile(bool restored, const Callback& callback);
        void refreshStatus();

    private:
        void change(const std::function<Json(Json&)>& derive, const Callback& callback);
        void dispatch(const Json& executions);
        void fail(const std::string& id, const std::string& attempt, const std::string& reason);
        void accept(const std::string& repository, const Json& report, bool job, const Json& packageJob = nullptr);
        void record(const std::string& id, const std::string& attempt, bool publication, const Json& run, const Json& facts);
        void inspect(const std::string& id, const Callback& callback);
        void complete(const std::string& id, bool publication);
        static Json facts(const Json& report, bool job);
        static void log(const PipelineResult& result);
        const PipelineConfig& config;
        const GitHubConfig& app;
        GitHub& github;
        SourceReader& sources;
        GraphStore& store;
        FeedProof& feeds;
        std::set<std::string> completionReads;
        std::optional<std::uint64_t> refreshing;
        std::uint64_t statusSent = UINT64_MAX;
    };
} // namespace coreci
