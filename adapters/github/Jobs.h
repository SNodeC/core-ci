#pragma once
#include "Declarations.h"
namespace coreci::github {
    struct Job {
        std::string htmlUrl, startedAt, completedAt, status, conclusion;
        bool operator==(const Job&) const = default;
    };
    struct RunJob {
        std::uint64_t runId = 0;
        std::string htmlUrl, status, conclusion;
        std::optional<Job> job;
        bool operator==(const RunJob&) const = default;
    };
    struct JobRequest {
        Workflow workflow;
        std::string buildId, mainJob;
        std::optional<std::uint64_t> runId;
        std::string key() const;
    };
    void loadJob(const JobRequest& request, const Url& base, Fetch fetch, std::function<void(std::optional<RunJob>)> done);
} // namespace coreci::github
