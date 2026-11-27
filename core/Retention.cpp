#include "Scheduler.h"
#include "Version.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace coreci {
    namespace {
        using Text = std::string;

        Text capability(const Text& relation) {
            Text result = relation.substr(0, relation.find_first_of(" \t<>=~"));
            if (result.starts_with('!')) {
                result.clear();
            }
            return result;
        }

        Text exactVersion(const Text& relation) {
            Text result;
            const auto sign = relation.find_first_of("<>=~");
            if (sign != Text::npos && relation[sign] == '=') {
                const auto start = relation.find_first_not_of(" =\t", sign);
                if (start != Text::npos) {
                    result = relation.substr(start, relation.find_first_of(" \t)", start) - start);
                }
            }
            return result;
        }

        bool supplies(const Json& output, const Text& name, const Text& requiredVersion) {
            bool result = false;
            for (const auto& bin : output.at("inventory")) {
                result = result || (bin.at("name") == name && (requiredVersion.empty() || bin.value("version", "") == requiredVersion));
                for (const auto& property : bin.value("provides", Json::array())) {
                    const auto relation = property.get<Text>();
                    result =
                        result || (capability(relation) == name && (requiredVersion.empty() || exactVersion(relation) == requiredVersion));
                }
            }
            return result;
        }

        bool needs(const Json& consumer, const Json& provider) {
            bool result = false;
            if (consumer.at("target").at("id") == provider.at("target").at("id")) {
                for (const auto& bin : consumer.at("inventory")) {
                    for (const auto& relation : bin.at("runtime_dependencies")) {
                        const auto raw = relation.get<Text>();
                        std::size_t position = 0;
                        while (!result && position < raw.size()) {
                            const auto start = raw.find_first_not_of(" \t", position);
                            const auto end = raw.find('|', position);
                            if (start != Text::npos && (end == Text::npos || start < end)) {
                                const auto alternative = raw.substr(start, end - start);
                                const auto name = capability(alternative);
                                if (!name.empty()) {
                                    result = supplies(provider, name, exactVersion(alternative));
                                }
                            }
                            position = end == Text::npos ? raw.size() : end + 1;
                        }
                    }
                }
            }
            return result;
        }
    } // namespace

    Json retirements(const Json& entries, Time now, const Json& planned) {
        using Group = std::pair<Text, Text>;
        std::map<Group, std::vector<std::size_t>> groups;
        std::set<std::size_t> keep;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& output = entries[i];
            groups[{output.at("package").get<Text>(), output.at("target").at("id").get<Text>()}].push_back(i);
            if (output.at("published_at").get<Time>() >= now - 7 * 86400 || std::ranges::any_of(planned, [&](const auto& consumer) {
                    return needs(consumer, output);
                })) {
                keep.insert(i);
            }
        }
        for (auto& [_, group] : groups) {
            std::stable_sort(group.begin(), group.end(), [&](std::size_t lhs, std::size_t rhs) {
                const auto left = Version::parse(entries[lhs].at("version").get<Text>()).value();
                const auto right = Version::parse(entries[rhs].at("version").get<Text>()).value();
                return left != right ? left > right : entries[lhs].at("revision") > entries[rhs].at("revision");
            });
            for (std::size_t i = 0; i < std::min(group.size(), std::size_t{2}); ++i) {
                keep.insert(group[i]);
            }
        }
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (std::size_t i = 0; i < entries.size(); ++i) {
                if (!keep.contains(i) && std::ranges::any_of(keep, [&](std::size_t j) {
                        return needs(entries[j], entries[i]);
                    })) {
                    keep.insert(i);
                    expanded = true;
                }
            }
        }
        Json result = Json::array();
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (!keep.contains(i)) {
                const auto& out = entries[i];
                result.push_back({out.at("package"), out.at("version"), out.at("revision"), out.at("target").at("id")});
            }
        }
        return result;
    }
} // namespace coreci
