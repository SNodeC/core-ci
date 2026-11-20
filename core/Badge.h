#pragma once
#include "WaveController.h"

namespace coreci {
    std::string badge(const std::string& label, const std::string& message, const std::string& colour);
    std::string unitBadge(const WaveController& engine, const Unit& unit);
} // namespace coreci
