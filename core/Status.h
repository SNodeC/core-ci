#pragma once
#include "Engine.h"

namespace coreci {
    std::map<Unit, std::string> statuses(const Engine& engine);
    std::string statusText(const Engine& engine);
    std::string statusDot(const Engine& engine);
} // namespace coreci
