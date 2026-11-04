#pragma once
#include "Graph.h"
#include "Version.h"

#include <cstdint>
#include <map>
#include <variant>

namespace coreci {
    struct Input {
        Version version;
        std::uint64_t revision;
        bool operator==(const Input&) const = default;
    };
    using Inputs = std::map<std::string, Input>;
    struct DispatchBuild {
        std::string buildId;
        unsigned attempt;
        std::string package;
        Repo sourceRepo;
        std::string tag;
        Version version;
        std::uint64_t revision;
        std::string target;
        Inputs inputs;
        Workflow workflow;
        bool operator==(const DispatchBuild&) const = default;
    };
    struct DispatchPublish {
        std::string buildId;
        unsigned attempt;
        std::string package;
        Version version;
        std::uint64_t revision;
        std::string target, feed;
        Repo buildRepo;
        std::uint64_t runId;
        std::string artifact;
        Workflow workflow;
        bool operator==(const DispatchPublish&) const = default;
    };
    using Command = std::variant<DispatchBuild, DispatchPublish>;
} // namespace coreci
