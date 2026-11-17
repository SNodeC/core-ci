#pragma once
#include "core/Events.h"

#include <optional>

namespace coreci::persistence {
    struct Invalid {
        std::string reason;
        bool operator==(const Invalid&) const = default;
    };
    using Normalized = std::variant<Event, Invalid>;
    struct GraphTargetRow {
        std::string package, target;
        bool operator==(const GraphTargetRow&) const = default;
    };
    struct GraphPrerequisiteRow {
        std::string package, prerequisite;
        bool operator==(const GraphPrerequisiteRow&) const = default;
    };
    struct GraphRows {
        std::vector<TagObserved> releases;
        std::vector<Package> packages;
        std::vector<GraphTargetRow> targets;
        std::vector<GraphPrerequisiteRow> prerequisites;
        bool operator==(const GraphRows&) const = default;
    };
    struct EventRow {
        std::uint64_t seq = 0;
        std::string eventType;
        std::optional<std::string> package, target, tag, sourceSha, origin, buildId, waveId, message, artifact, feedCommit, reason,
            conclusion;
        std::optional<std::int64_t> httpStatus, runAttempt, attempt, eventAt, dispatchDeadline;
        std::optional<std::uint64_t> runId;
        std::optional<GraphRows> graph;
        bool operator==(const EventRow&) const = default;
    };
    Normalized normalize(Event event);
    std::variant<EventRow, Invalid> encode(const Event& event, std::uint64_t seq = 0);
    Normalized decode(const EventRow& row);
} // namespace coreci::persistence
