#include "Version.h"

#include <algorithm>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace coreci {

    namespace {
        bool numeric(const std::string& value) {
            return !value.empty() && std::all_of(value.begin(), value.end(), [](char character) {
                return character >= '0' && character <= '9';
            });
        }

        int compareNumber(const std::string& left, const std::string& right) {
            int result = 0;
            if (left.size() != right.size()) {
                result = left.size() < right.size() ? -1 : 1;
            } else if (left != right) {
                result = left < right ? -1 : 1;
            }
            return result;
        }
    } // namespace

    Version::Version(const std::string& selectedTag)
        : tag(selectedTag) {
        static const std::regex syntax(
            R"(^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$)");
        std::smatch match;
        if (tag.size() > 256 || !std::regex_match(tag, match, syntax)) {
            throw std::invalid_argument("tag is not a supported semantic version");
        }
        release = {match[1], match[2], match[3]};

        std::istringstream parts(match[4]);
        std::string part;
        while (std::getline(parts, part, '.')) {
            if (numeric(part) && part.size() > 1 && part.front() == '0') {
                throw std::invalid_argument("numeric prerelease identifiers cannot have leading zeroes");
            }
            prerelease.push_back(part);
        }
    }

    int Version::compare(const Version& other) const {
        int result = 0;
        for (std::size_t index = 0; index < release.size() && result == 0; ++index) {
            result = compareNumber(release[index], other.release[index]);
        }

        if (result == 0 && prerelease.empty() != other.prerelease.empty()) {
            result = prerelease.empty() ? 1 : -1;
        } else if (result == 0) {
            const std::size_t count = std::min(prerelease.size(), other.prerelease.size());
            for (std::size_t index = 0; index < count && result == 0; ++index) {
                const auto& left = prerelease[index];
                const auto& right = other.prerelease[index];
                if (numeric(left) && numeric(right)) {
                    result = compareNumber(left, right);
                } else if (numeric(left) != numeric(right)) {
                    result = numeric(left) ? -1 : 1;
                } else if (left != right) {
                    result = left < right ? -1 : 1;
                }
            }
            if (result == 0 && prerelease.size() != other.prerelease.size()) {
                result = prerelease.size() < other.prerelease.size() ? -1 : 1;
            }
        }
        return result;
    }

} // namespace coreci
