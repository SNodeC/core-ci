#pragma once
#include "migration/legacy/Engine.h"

namespace coreci::legacy {
    std::string badge(const std::string& label, const std::string& message, const std::string& colour);
    std::string unitBadge(const Engine& engine, const Unit& unit);
} // namespace coreci::legacy
