#pragma once
#include "persistence/StateSql.h"

#include <database/mariadb/MariaDBClient.h>
#include <memory>

namespace coreci::mariadb {
    class MariaDBStateStore : public persistence::StateStore {
    public:
        using StoreError = persistence::StoreError;
        explicit MariaDBStateStore(const database::mariadb::MariaDBConnectionDetails& details);
        ~MariaDBStateStore() override;
        void setConnectionLost(std::function<void(const std::string&)> callback);
        void open(std::function<void()> ready, std::function<void(const StoreError&)> error) override;
        void load(std::function<void(StoredState)> done, std::function<void(const StoreError&)> error) override;
        void commit(const StoredState& before,
                    const StoredState& after,
                    const std::string& description,
                    std::function<void()> done,
                    std::function<void(const StoreError&)> error) override;

    private:
        void fail(const StoreError& error);
        void execute(const std::string& sql, std::function<void()> done);
        void createSchema(std::size_t index, std::function<void()> done);
        void lockWriter(std::function<void()> done);
        void checkSchema(std::function<void()> done);
        void read(std::shared_ptr<persistence::StateRows> rows, std::size_t table, std::function<void()> done);
        void write(std::shared_ptr<std::vector<std::string>> statements, std::size_t index, std::function<void()> done);
        database::mariadb::MariaDBClient client;
        std::function<void(const StoreError&)> onFailure;
        std::function<void(const std::string&)> onLost;
        bool ready = false;
    };
} // namespace coreci::mariadb
