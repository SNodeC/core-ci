#pragma once

#include <stdexcept>
#include <string>

namespace coreci {
    inline void require(bool condition, const std::string& error) {
        if (!condition) {
            throw std::invalid_argument(error);
        }
    }
} // namespace coreci
