#pragma once
#include "app/StatusJson.h"
inline std::string recordedStatus(const coreci::Engine& engine) {
    return coreci::app::statusJson(coreci::app::statusSnapshot(engine), 0);
}
