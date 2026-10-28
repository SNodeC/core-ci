#include "Version.h"

#include "Error.h"

#include <algorithm>
#include <regex>
#include <sstream>

namespace coreci {
    namespace {
        bool numeric(const std::string& text) {
            return text.find_first_not_of("0123456789") == std::string::npos;
        }
    } // namespace

    Version::Version(const std::string& tag) {
        static const std::regex syntax(
            R"(^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$)");
        std::smatch match;
        require(tag.size() <= 256 && std::regex_match(tag, match, syntax), "invalid semantic version tag");
        parts = {match[1], match[2], match[3]};
        std::istringstream prerelease(match[4]);
        std::string part;
        while (std::getline(prerelease, part, '.')) {
            require(!numeric(part) || part.size() == 1 || part.front() != '0', "leading zero in prerelease number");
            parts.push_back(part);
        }
    }

    int Version::compare(const Version& other) const {
        int result = 0;
        for (std::size_t index = 0; index < std::min(parts.size(), other.parts.size()) && result == 0; ++index) {
            const auto& left = parts[index];
            const auto& right = other.parts[index];
            const bool leftNumeric = numeric(left);
            const bool rightNumeric = numeric(right);
            if (leftNumeric != rightNumeric) {
                result = leftNumeric ? -1 : 1;
            } else if (leftNumeric && left.size() != right.size()) {
                result = left.size() < right.size() ? -1 : 1;
            } else if (left != right) {
                result = left < right ? -1 : 1;
            }
        }
        if (result == 0 && parts.size() != other.parts.size()) {
            result = parts.size() == 3 ? 1 : other.parts.size() == 3 ? -1 : parts.size() < other.parts.size() ? -1 : 1;
        }
        return result;
    }
} // namespace coreci
