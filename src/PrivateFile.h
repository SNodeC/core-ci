#pragma once

#include <string>

namespace coreci {
    std::string readPrivateFile(const std::string& path, std::size_t limit);
}
