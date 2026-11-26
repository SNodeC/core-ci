#pragma once
#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace coreci {
    class Version {
    public:
        static std::optional<Version> parse(const std::string& tag);
        static bool validRange(const std::string& range);
        bool satisfies(const std::string& range) const;
        const std::string& text() const;
        bool prerelease() const;
        std::strong_ordering operator<=>(const Version& other) const;
        bool operator==(const Version& other) const;

    private:
        std::string value;
        std::vector<std::string> numbers;
        std::vector<std::string> pre;
    };
} // namespace coreci
