#ifndef CORE_CI_VERSION_H
#define CORE_CI_VERSION_H

#include <array>
#include <string>
#include <vector>

namespace coreci {

    struct Version {
        explicit Version(const std::string& tag);
        int compare(const Version& other) const;

        std::string tag;
        std::array<std::string, 3> release;
        std::vector<std::string> prerelease;
    };

} // namespace coreci

#endif
