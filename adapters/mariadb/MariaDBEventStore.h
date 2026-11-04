#pragma once
#include "persistence/EventRow.h"
#include "persistence/EventStore.h"

#include <database/mariadb/MariaDBClient.h>
#include <vector>

namespace coreci::mariadb {
    // Pure transport decoding; fields are in selectSql() order, with SQL NULL preserved.
    std::variant<persistence::EventRow, persistence::Invalid> decodeFields(const std::vector<std::optional<std::string>>& fields);

    class MariaDBEventStore : public persistence::EventStore {
    public:
        explicit MariaDBEventStore(const database::mariadb::MariaDBConnectionDetails& details);
        ~MariaDBEventStore() override;
        // The event queue serializes operations. Keep this store alive through its callbacks.
        void open(std::function<void()> onReady, std::function<void(const StoreError&)> onError) override;
        void append(const Event&, std::function<void()> onCommitted, std::function<void(const std::string&)> onError) override;
        void loadAll(std::function<void(std::uint64_t, const Event&)> onEvent,
                     std::function<void()> onDone,
                     std::function<void(const StoreError&)> onError) override;

    private:
        void fail(const StoreError& error);
        void execute(const std::string& sql, std::function<void()> done);
        void createSchema(std::size_t index, std::function<void()> done);
        void checkSchema(std::function<void()> done);
        database::mariadb::MariaDBClient client;
        std::function<void(const StoreError&)> onFailure;
        // A lost session must pass through open(), never lazy-reconnect straight into an INSERT.
        bool ready = false;
    };
} // namespace coreci::mariadb
