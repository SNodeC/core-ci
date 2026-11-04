#pragma once
#include "EventRow.h"

namespace coreci::persistence {
    // Invalid events never produce SQL. seq is allocated by the database.
    std::variant<std::string, Invalid> insertSql(const Event& event);
    std::string selectSql();
} // namespace coreci::persistence
