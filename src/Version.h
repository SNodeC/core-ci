#pragma once

#include <string>
#include <vector>

namespace coreci {
    class Version {
    public:
        explicit Version(const std::string& tag);
        int compare(const Version& other) const;

    private:
        std::vector<std::string> parts;
    };
} // namespace coreci
