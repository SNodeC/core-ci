#pragma once
#include "Graph.h"

#include <cstdint>
#include <variant>

namespace coreci {
    enum class Origin { Webhook, Reconcile };
    struct TagObserved {
        std::int64_t at;
        std::string package, tag, sourceSha;
        Origin origin;
        bool operator==(const TagObserved&) const = default;
    };
    struct GraphChanged {
        std::int64_t at;
        Pipeline pipeline;
        std::vector<TagObserved> tags = {};
        bool operator==(const GraphChanged&) const = default;
    };
    struct WaveDispatchAccepted {
        std::int64_t at;
        std::string waveId;
        unsigned attempt;
        bool operator==(const WaveDispatchAccepted&) const = default;
    };
    struct WaveDispatchRejected {
        std::int64_t at;
        std::string waveId;
        unsigned attempt;
        int httpStatus;
        std::string message;
        bool operator==(const WaveDispatchRejected&) const = default;
    };
    struct WaveStarted {
        std::int64_t at;
        std::string waveId;
        std::uint64_t runId;
        unsigned runAttempt;
        bool operator==(const WaveStarted&) const = default;
    };
    struct WaveFinished {
        std::int64_t at;
        std::string waveId;
        std::uint64_t runId;
        std::string conclusion;
        bool operator==(const WaveFinished&) const = default;
    };
    struct UnitBuildStarted {
        std::int64_t at;
        std::string waveId, buildId;
        bool operator==(const UnitBuildStarted&) const = default;
    };
    struct UnitBuildSucceeded {
        std::int64_t at;
        std::string waveId, buildId, artifact;
        bool operator==(const UnitBuildSucceeded&) const = default;
    };
    struct UnitBuildFailed {
        std::int64_t at;
        std::string waveId, buildId, reason;
        bool operator==(const UnitBuildFailed&) const = default;
    };
    struct UnitPublishSucceeded {
        std::int64_t at;
        std::string waveId, buildId, feedCommit;
        bool operator==(const UnitPublishSucceeded&) const = default;
    };
    struct UnitPublishFailed {
        std::int64_t at;
        std::string waveId, buildId, reason;
        bool operator==(const UnitPublishFailed&) const = default;
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
    using Event = std::variant<TagObserved,
                               GraphChanged,
                               WaveDispatchAccepted,
                               WaveDispatchRejected,
                               WaveStarted,
                               WaveFinished,
                               UnitBuildStarted,
                               UnitBuildSucceeded,
                               UnitBuildFailed,
                               UnitPublishSucceeded,
                               UnitPublishFailed,
                               RetryRequested,
                               RebuildRequested,
                               Tick,
                               Resumed>;
} // namespace coreci
