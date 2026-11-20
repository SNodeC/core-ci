#pragma once
#include "core/WaveController.h"

#include <nlohmann/json.hpp>

namespace coreci::app {
    nlohmann::json statusSnapshot(const WaveController& engine);
    std::string statusJson(nlohmann::json snapshot, std::uint64_t generation);
    std::string planJson(const Wave& wave);
    std::string publicationPlanJson(const Publication& publication, const std::map<std::string, Wave>& waves);
} // namespace coreci::app
