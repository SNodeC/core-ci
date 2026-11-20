#pragma once
#include "core/WaveController.h"
#include "migration/legacy/EventRow.h"
#include "migration/legacy/EventStore.h"

#include <database/mariadb/MariaDBClient.h>
#include <memory>
#include <vector>

namespace coreci::legacy::mariadb {
    // Pure transport decoding; fields are in selectSql() order, with SQL NULL preserved.
    std::variant<persistence::EventRow, persistence::Invalid> decodeFields(const std::vector<std::optional<std::string>>& fields);

    class MariaDBEventStore {
    public:
        using StoreError = persistence::EventStore::StoreError;
        explicit MariaDBEventStore(const database::mariadb::MariaDBConnectionDetails& details);
        ~MariaDBEventStore();
        void open(std::function<void()> onReady, std::function<void(const StoreError&)> onError);
        void loadAll(std::function<void(std::uint64_t, const Event&)> onEvent,
                     std::function<void()> onDone,
                     std::function<void(const StoreError&)> onError);
        void installState(const ::coreci::StoredState& state,
                          const std::vector<std::string>& log,
                          std::function<void()> done,
                          std::function<void(const StoreError&)> error);

    private:
        using Rows = std::map<std::uint64_t, persistence::EventRow>;
        void loadGraphs(std::shared_ptr<Rows> rows, std::size_t table, std::function<void()> done);
        void insertGraph(std::shared_ptr<std::vector<std::string>> statements, std::size_t index, std::function<void()> done);
        void fail(const StoreError& error);
        void execute(const std::string& sql, std::function<void()> done);
        void lockWriter(std::function<void()> done);
        void checkSchema(std::function<void()> done);
        database::mariadb::MariaDBClient client;
        std::function<void(const StoreError&)> onFailure;
        bool ready = false;
    };
} // namespace coreci::legacy::mariadb
