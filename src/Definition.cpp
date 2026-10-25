#include "Definition.h"

#include "RepositoryName.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <regex>
#include <set>
#include <stdexcept>

namespace coreci {

    namespace {
        void fields(const nlohmann::json& object, const std::set<std::string>& allowed) {
            if (!object.is_object()) {
                throw std::invalid_argument("expected a JSON object");
            }
            for (const auto& [key, value] : object.items()) {
                if (!allowed.contains(key)) {
                    throw std::invalid_argument("unsupported field: " + key);
                }
            }
        }

        std::string text(const nlohmann::json& object, const std::string& key) {
            const auto value = object.at(key).get<std::string>();
            if (value.empty() || value.size() > 512) {
                throw std::invalid_argument("empty or oversized string: " + key);
            }
            return value;
        }

        bool safePath(const std::string& value) {
            bool valid = !value.empty() && value.front() != '/' && value.find('\\') == std::string::npos;
            std::size_t offset = 0;
            while (valid && offset < value.size()) {
                const auto separator = value.find('/', offset);
                const auto part = value.substr(offset, separator == std::string::npos ? separator : separator - offset);
                valid = !part.empty() && part != "." && part != ".." && part.find_first_of("\r\n\0", 0, 3) == std::string::npos;
                offset = separator == std::string::npos ? value.size() : separator + 1;
            }
            return valid && value.back() != '/';
        }

        void coordinate(const nlohmann::json& row, const std::string& key) {
            static const std::regex syntax(R"(^[A-Za-z0-9][A-Za-z0-9._-]*$)");
            if (!std::regex_match(text(row, key), syntax)) {
                throw std::invalid_argument("invalid target coordinate: " + key);
            }
        }
    } // namespace

    Definition Definition::parse(const nlohmann::json& input) {
        fields(input, {"schema", "title", "package", "depends_on", "build", "matrix"});
        if (!input.at("schema").is_number_integer() || input.at("schema") != 1) {
            throw std::invalid_argument("unsupported definition schema");
        }
        text(input, "title");
        static const std::regex packageSyntax(R"(^[a-z0-9][a-z0-9+.-]*$)");
        if (text(input, "package").size() > 128 || !std::regex_match(text(input, "package"), packageSyntax)) {
            throw std::invalid_argument("invalid package namespace");
        }

        if (!input.at("depends_on").is_array() || input.at("depends_on").size() > 128) {
            throw std::invalid_argument("depends_on must be a bounded array");
        }
        std::set<std::string> dependencies;
        for (const auto& dependency : input.at("depends_on")) {
            const std::string name = dependency.get<std::string>();
            if (!repositoryName(name) || !dependencies.insert(name).second) {
                throw std::invalid_argument("invalid or duplicate dependency name");
            }
        }

        fields(input.at("build"), {"native", "openwrt"});
        for (const auto& [family, hook] : input.at("build").items()) {
            if (!safePath(hook.get<std::string>())) {
                throw std::invalid_argument("hook must remain inside its source checkout");
            }
        }
        if (!input.at("matrix").is_array() || input.at("matrix").empty() || input.at("matrix").size() > 256) {
            throw std::invalid_argument("matrix must have 1 to 256 rows");
        }

        std::set<std::string> ids;
        std::set<std::string> identities;
        for (const auto& row : input.at("matrix")) {
            fields(row, {"id", "family", "distribution", "suite", "arch", "runner", "build"});
            for (const auto* key : {"id", "family", "distribution", "suite", "arch"}) {
                coordinate(row, key);
            }
            if (!ids.insert(text(row, "id")).second) {
                throw std::invalid_argument("duplicate target row ID");
            }
            const auto& runner = row.at("runner");
            if (runner.is_string()) {
                text(row, "runner");
            } else if (runner.is_array() && !runner.empty() && runner.size() <= 32) {
                for (const auto& label : runner) {
                    if (!label.is_string() || label.get<std::string>().empty() || label.get<std::string>().size() > 256) {
                        throw std::invalid_argument("runner labels must be nonempty bounded strings");
                    }
                }
            } else {
                throw std::invalid_argument("unsupported runner selector; use a string or label array");
            }

            const auto family = text(row, "family");
            const auto& build = row.at("build");
            if (!input.at("build").contains(family)) {
                throw std::invalid_argument("missing source hook for target family");
            }
            if (family == "native") {
                fields(build, {"image", "platform", "format"});
                text(build, "image");
                text(build, "platform");
                if (text(build, "format") != "deb" && text(build, "format") != "rpm") {
                    throw std::invalid_argument("unsupported native package format");
                }
            } else if (family == "openwrt") {
                fields(build, {"target", "format"});
                if (!safePath(text(build, "target"))) {
                    throw std::invalid_argument("unsafe OpenWrt target path");
                }
                if (text(build, "format") != "ipk" && text(build, "format") != "apk") {
                    throw std::invalid_argument("unsupported OpenWrt package format");
                }

            } else {
                throw std::invalid_argument("unsupported target family");
            }
            if (!identities.insert(coordinates(row).dump()).second) {
                throw std::invalid_argument("duplicate compatible target coordinates");
            }
        }
        return {input};
    }

    nlohmann::json Definition::coordinates(const nlohmann::json& row) {
        nlohmann::json result;
        for (const auto* key : {"family", "distribution", "suite", "arch"}) {
            result[key] = row.at(key);
        }
        result["format"] = row.at("build").at("format");
        if (row.at("family") == "native") {
            result["platform"] = row.at("build").at("platform");
        } else {
            result["target"] = row.at("build").at("target");
        }
        return result;
    }

    void Definition::validateGraph(const std::map<std::string, Definition>& repositories) {
        if (repositories.size() > 512) {
            throw std::invalid_argument("selected graph exceeds the supported repository bound");
        }
        std::set<std::string> namespaces;
        for (const auto& [name, definition] : repositories) {
            const auto package = definition.json.at("package").get<std::string>();
            for (const auto& claimed : namespaces) {
                if (claimed == package || claimed.starts_with(package + "-") || package.starts_with(claimed + "-")) {
                    throw std::invalid_argument("overlapping package namespace at " + name);
                }
            }
            namespaces.insert(package);
        }

        std::map<std::string, unsigned int> color;
        std::function<void(const std::string&)> visit = [&](const std::string& name) {
            if (color[name] == 1) {
                throw std::invalid_argument("dependency cycle at " + name);
            }
            if (color[name] == 0) {
                color[name] = 1;
                const auto& definition = repositories.at(name).json;
                for (const auto& dependency : definition.at("depends_on")) {
                    const auto other = repositories.find(dependency.get<std::string>());
                    if (other == repositories.end()) {
                        throw std::invalid_argument("missing enrolled dependency: " + dependency.get<std::string>());
                    }
                    for (const auto& row : definition.at("matrix")) {
                        const auto expected = coordinates(row);
                        const auto matches = std::count_if(
                            other->second.json.at("matrix").begin(), other->second.json.at("matrix").end(), [&](const auto& candidate) {
                                return coordinates(candidate) == expected;
                            });
                        if (matches != 1) {
                            throw std::invalid_argument("missing or ambiguous prerequisite target for " + name + "/" + text(row, "id"));
                        }
                    }
                    visit(other->first);
                }
                color[name] = 2;
            }
        };
        for (const auto& [name, definition] : repositories) {
            visit(name);
        }
    }

} // namespace coreci
