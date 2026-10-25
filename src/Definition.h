#ifndef CORE_CI_DEFINITION_H
#define CORE_CI_DEFINITION_H

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace coreci {

    struct Definition {
        static Definition parse(const nlohmann::json& input);
        static nlohmann::json coordinates(const nlohmann::json& row);
        static void validateGraph(const std::map<std::string, Definition>& repositories);

        nlohmann::json json;
    };

} // namespace coreci

#endif
