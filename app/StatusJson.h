#pragma once
#include "core/Engine.h"
namespace coreci::app {
    struct VersionView {
        std::string version;
        std::uint64_t revision;
        bool operator==(const VersionView&) const = default;
    };
    struct BuildView {
        VersionView selection;
        std::string buildId;
        Phase phase;
        std::optional<std::uint64_t> runId;
        unsigned attempt;
        bool operator==(const BuildView&) const = default;
    };
    struct PublicationView {
        VersionView selection;
        std::optional<std::uint64_t> runId;
        std::int64_t at;
        std::string feedCommit;
        bool operator==(const PublicationView&) const = default;
    };
    struct FailureView {
        VersionView selection;
        std::optional<std::uint64_t> runId;
        std::int64_t at;
        std::string reason;
        bool operator==(const FailureView&) const = default;
    };
    struct UnitView {
        std::string feed, status;
        std::optional<BuildView> inflight;
        std::optional<PublicationView> published;
        std::optional<FailureView> failure;
        std::optional<VersionView> next;
        bool operator==(const UnitView&) const = default;
    };
    struct PackageView {
        Repo sourceRepo;
        std::optional<Released> released;
        bool operator==(const PackageView&) const = default;
    };
    struct StatusSnapshot {
        std::map<std::string, PackageView> packages;
        std::map<Unit, UnitView> units;
        bool operator==(const StatusSnapshot&) const = default;
    };
    StatusSnapshot statusSnapshot(const Engine& engine, const std::string& feed = {});
    std::string statusJson(const StatusSnapshot& snapshot, std::uint64_t generation);
} // namespace coreci::app
