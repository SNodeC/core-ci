#pragma once
#include "core/Graph.h"
#include "core/Version.h"

#include <cstdint>
#include <map>

namespace coreci::legacy {
    struct Input {
        Version version;
        std::uint64_t revision;
        bool operator==(const Input&) const = default;
    };
    using Inputs = std::map<std::string, Input>;
    struct DispatchWave {
        std::string waveId;
        unsigned attempt;
        Repo workflow;
        std::string feedBranch;
        bool operator==(const DispatchWave&) const = default;
    };
    using Command = DispatchWave;
} // namespace coreci::legacy
