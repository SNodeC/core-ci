#pragma once
#include "core/Events.h"

namespace coreci::persistence {
    struct Invalid {
        std::string reason;
        bool operator==(const Invalid&) const = default;
    };
    using Normalized = std::variant<Event, Invalid>;
    Normalized normalize(Event input);
    std::string describe(const Event& input);
} // namespace coreci::persistence
