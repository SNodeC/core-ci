#include "Version.h"

#include <algorithm>
#include <regex>

namespace coreci {
    namespace {
        std::vector<std::string> split(const std::string& s) {
            std::vector<std::string> parts;
            std::size_t start = 0;
            for (std::size_t i = 0; i <= s.size(); ++i) {
                if (i == s.size() || s[i] == '.') {
                    parts.push_back(s.substr(start, i - start));
                    start = i + 1;
                }
            }
            return parts;
        }
        bool numeric(const std::string& s) {
            return std::all_of(s.begin(), s.end(), [](char c) {
                return c >= '0' && c <= '9';
            });
        }
        std::strong_ordering numberOrder(const std::string& a, const std::string& b) {
            return a.size() == b.size() ? a <=> b : a.size() <=> b.size();
        }
    } // namespace
    std::optional<Version> Version::parse(const std::string& tag) {
        static const std::regex pattern(R"(v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*))"
                                        R"((-([0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*))?)"
                                        R"((\+([0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*))?)");
        std::smatch m;
        std::optional<Version> result;
        if (tag.size() - (tag.starts_with("v") ? 1U : 0U) <= 100 && std::regex_match(tag, m, pattern)) {
            Version version;
            version.value = tag.substr(tag.starts_with('v') ? 1 : 0);
            version.numbers = {m[1], m[2], m[3]};
            if (m[5].matched) {
                version.pre = split(m[5]);
            }
            bool valid = true;
            for (const auto& id : version.pre) {
                valid = valid && !(numeric(id) && id.size() > 1 && id[0] == '0');
            }
            if (valid) {
                result = std::move(version);
            }
        }
        return result;
    }
    const std::string& Version::text() const {
        return value;
    }
    bool Version::prerelease() const {
        return !pre.empty();
    }
    std::strong_ordering Version::operator<=>(const Version& other) const {
        auto order = std::strong_ordering::equal;
        for (std::size_t i = 0; i < numbers.size() && order == 0; ++i) {
            order = numberOrder(numbers[i], other.numbers[i]);
        }
        if (order == 0 && pre.empty() != other.pre.empty()) {
            order = pre.empty() ? std::strong_ordering::greater : std::strong_ordering::less;
        } else if (order == 0) {
            for (std::size_t i = 0; i < std::min(pre.size(), other.pre.size()) && order == 0; ++i) {
                const bool a = numeric(pre[i]);
                const bool b = numeric(other.pre[i]);
                order = a != b ? (a ? std::strong_ordering::less : std::strong_ordering::greater)
                               : (a ? numberOrder(pre[i], other.pre[i]) : pre[i] <=> other.pre[i]);
            }
            if (order == 0) {
                order = pre.size() <=> other.pre.size();
            }
        }
        return order;
    }
    bool Version::operator==(const Version& other) const {
        return (*this <=> other) == 0;
    }
} // namespace coreci
