#pragma once
#include "core/Engine.h"

namespace coreci::app {
    struct VersionView {
        std::string version;
        std::uint64_t revision;
        bool operator==(const VersionView&) const = default;
    };
    struct UnitView {
        std::string status, buildId;
        std::optional<VersionView> selection;
        std::optional<Published> published;
        std::optional<std::uint64_t> publishedRun;
        std::optional<Failure> failure;
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
        std::optional<Wave> wave;
        std::map<std::string, unsigned> waveCounts;
        bool operator==(const StatusSnapshot&) const = default;
    };
    StatusSnapshot statusSnapshot(const Engine& engine);
    std::string statusJson(const StatusSnapshot& snapshot, std::uint64_t generation);
    std::string planJson(const Wave& wave);
} // namespace coreci::app
