#ifndef CORE_CI_PRIVATE_FILE_H
#define CORE_CI_PRIVATE_FILE_H

#include <cstddef>
#include <string>

namespace coreci {
    std::string readPrivateFile(const std::string& path, std::size_t limit);
}

#endif
