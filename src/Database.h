#pragma once

#include <database/mariadb/MariaDBClient.h>
#include <deque>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace coreci {
    struct DatabaseResult {
        std::vector<nlohmann::json> rows;
        std::string error;
        bool uncertain = false;
        unsigned int code = 0;
    };

    class Database {
    public:
        using Callback = std::function<void(const DatabaseResult&)>;
        explicit Database(database::mariadb::MariaDBClient& client)
            : client(client) {
        }
        void query(const std::string& sql, const Callback& callback);
        void write(const std::function<std::string()>& prepare, const Callback& callback);
        bool mutationsAllowed() const {
            return !uncertain;
        }
        static std::string text(const std::string& value);

    private:
        void enqueue(const std::function<void()>& operation);
        void release();
        void pump();
        database::mariadb::MariaDBClient& client;
        std::deque<std::shared_ptr<std::function<void()>>> operations;
        bool pumping = false;
        bool uncertain = false;
    };
} // namespace coreci
