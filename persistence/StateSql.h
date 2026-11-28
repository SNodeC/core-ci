#pragma once
#include "StateStore.h"

#include <vector>

namespace coreci::persistence {
    std::string sqlText(const std::string& bytes);
    void readRecord(Json& state, const std::string& section, const std::string& key, const std::string& data);
    std::vector<std::string> stateSql(const Json& before, const Json& after, const std::string& description);
} // namespace coreci::persistence
