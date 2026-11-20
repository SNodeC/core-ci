#pragma once
#include "Engine.h"

namespace coreci::legacy {
    std::map<Unit, std::string> statuses(const Engine& engine);
} // namespace coreci::legacy
