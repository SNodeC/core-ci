#pragma once
#include "Engine.h"

namespace coreci {
    std::map<Unit, std::string> statuses(const Engine& engine);
} // namespace coreci
