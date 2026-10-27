#ifndef CORE_CI_DATABASE_H
#define CORE_CI_DATABASE_H

#include <database/mariadb/MariaDBClient.h>
#include <deque>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace coreci {

    struct DatabaseResult {
        std::vector<nlohmann::json> rows;
        std::string error;
        bool uncertain = false;
        bool preparationFailed = false;
    };

    struct DatabaseStatement {
        std::string sql;
        std::optional<std::uint64_t> affected;
    };

    class Database {
    public:
        using Callback = std::function<void(const DatabaseResult&)>;
        explicit Database(database::mariadb::MariaDBClient& client);
        bool mutationsAllowed() const;
        void query(const std::string& sql, const Callback& callback);
        void write(const std::function<std::vector<DatabaseStatement>()>& prepare, const Callback& callback);
        static std::string text(const std::string& value);

    private:
        struct Write {
            std::vector<DatabaseStatement> statements;
            std::size_t index = 0;
            Callback callback;
        };

        void read(const std::string& sql, const Callback& callback);
        void enqueue(const std::function<void()>& operation);
        void completed();
        void drain();
        void finish(const std::shared_ptr<Write>& write, const DatabaseResult& result);
        void writeNext(const std::shared_ptr<Write>& write);
        void writeFailed(const std::shared_ptr<Write>& write, const std::string& error, unsigned int code);

        database::mariadb::MariaDBClient& client;
        bool mutationUncertain = false;
        bool draining = false;
        std::deque<std::shared_ptr<std::function<void()>>> operations;
    };

} // namespace coreci

#endif
