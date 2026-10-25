#ifndef CORE_CI_REPOSITORY_NAME_H
#define CORE_CI_REPOSITORY_NAME_H

#include <algorithm>
#include <string_view>

namespace coreci {
    inline bool repositoryName(std::string_view name) {
        const bool characters = std::all_of(name.begin(), name.end(), [](unsigned char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.';
        });
        return !name.empty() && name.size() <= 128 && name != "." && name != ".." && characters;
    }
} // namespace coreci

#endif
