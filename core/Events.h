#pragma once
#include "Graph.h"

#include <cstdint>
#include <string>
#include <variant>

namespace coreci {
    enum class Kind { Build, Publish };
    enum class Origin { Webhook, Reconcile };
    struct TagObserved {
        std::int64_t at;
        std::string package, tag, sourceSha;
        Origin origin;
        bool operator==(const TagObserved&) const = default;
    };
    struct DispatchAccepted {
        std::int64_t at;
        std::string buildId;
        Kind kind;
        unsigned attempt;
        bool operator==(const DispatchAccepted&) const = default;
    };
    struct DispatchRejected {
        std::int64_t at;
        std::string buildId;
        Kind kind;
        unsigned attempt;
        int httpStatus;
        std::string message;
        bool operator==(const DispatchRejected&) const = default;
    };
    struct BuildStarted {
        std::int64_t at;
        std::string buildId;
        std::uint64_t runId;
        unsigned runAttempt;
        bool operator==(const BuildStarted&) const = default;
    };
    struct BuildSucceeded {
        std::int64_t at;
        std::string buildId;
        std::uint64_t runId;
        std::string artifact;
        bool operator==(const BuildSucceeded&) const = default;
    };
    struct BuildFailed {
        std::int64_t at;
        std::string buildId;
        std::uint64_t runId;
        std::string reason;
        bool operator==(const BuildFailed&) const = default;
    };
    struct PublishSucceeded {
        std::int64_t at;
        std::string buildId;
        std::uint64_t runId;
        std::string feedCommit;
        bool operator==(const PublishSucceeded&) const = default;
    };
    struct PublishFailed {
        std::int64_t at;
        std::string buildId;
        std::uint64_t runId;
        std::string reason;
        bool operator==(const PublishFailed&) const = default;
    };
    struct RetryRequested {
        std::int64_t at;
        std::string package, target;
        bool operator==(const RetryRequested&) const = default;
    };
    struct RebuildRequested {
        std::int64_t at;
        std::string package, target;
        bool operator==(const RebuildRequested&) const = default;
    };
    struct Tick {
        std::int64_t at;
        bool operator==(const Tick&) const = default;
    };
    struct Resumed {
        std::int64_t at;
        bool operator==(const Resumed&) const = default;
    };
    struct GraphChanged {
        std::int64_t at;
        Pipeline pipeline;
        bool operator==(const GraphChanged&) const = default;
    };
    using Event = std::variant<TagObserved,
                               DispatchAccepted,
                               DispatchRejected,
                               BuildStarted,
                               BuildSucceeded,
                               BuildFailed,
                               PublishSucceeded,
                               PublishFailed,
                               RetryRequested,
                               RebuildRequested,
                               Tick,
                               Resumed,
                               GraphChanged>;
} // namespace coreci
