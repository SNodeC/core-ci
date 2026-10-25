#ifndef CORE_CI_DATABASE_H
#define CORE_CI_DATABASE_H

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
    };

    class Database {
    public:
        using Callback = std::function<void(const DatabaseResult&)>;
        explicit Database(database::mariadb::MariaDBClient& client);
        bool mutationsAllowed() const;
        void query(const std::string& sql, const Callback& callback);
        void write(const std::vector<std::string>& statements, const Callback& callback);
        static std::string text(const std::string& value);

    private:
        struct Write {
            std::vector<std::string> statements;
            std::size_t index = 0;
            Callback callback;
        };

        void enqueue(const std::function<void()>& operation);
        void completed();
        void writeNext(const std::shared_ptr<Write>& write);
        void writeFailed(const std::shared_ptr<Write>& write, const std::string& error, unsigned int code);

        database::mariadb::MariaDBClient& client;
        bool mutationUncertain = false;
        std::deque<std::function<void()>> operations;
    };

} // namespace coreci

#endif
