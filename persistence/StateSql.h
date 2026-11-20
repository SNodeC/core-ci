#pragma once
#include "StateStore.h"

namespace coreci::persistence {
    using Field = std::optional<std::string>;
    using Row = std::vector<Field>;
    using StateRows = std::map<std::string, std::vector<Row>>;
    struct Column {
        std::string name;
        bool numeric;
    };
    struct Table {
        std::string name;
        std::size_t keys;
        std::vector<Column> columns;
    };
    const std::vector<Table>& stateTables();
    StateRows encodeState(const StoredState& state);
    StoredState decodeState(const StateRows& rows);
    std::string sqlText(const std::string& value);
    std::vector<std::string> stateSql(const StoredState& before, const StoredState& after, const std::string& description);
    std::string selectState(const Table& table);
    Row decodeFields(const Table& table, const std::vector<Field>& fields);
} // namespace coreci::persistence
