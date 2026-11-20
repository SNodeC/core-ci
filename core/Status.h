#pragma once
#include "WaveController.h"

namespace coreci {
    std::map<Unit, std::string> statuses(const WaveController& engine);
} // namespace coreci
