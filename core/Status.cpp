#include "Status.h"

namespace coreci {
    std::map<Unit, std::string> statuses(const WaveController& engine) {
        std::map<Unit, std::string> result;
        for (const auto& [name, package] : engine.pipeline().packages()) {
            for (const auto& target : package.targets) {
                const Unit unit{name, target};
                result.emplace(unit, stateName(engine.units().at(unit).state));
            }
        }
        return result;
    }
} // namespace coreci
