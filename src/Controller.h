#ifndef CORE_CI_CONTROLLER_H
#define CORE_CI_CONTROLLER_H

#include <algorithm>
#include <database/mariadb/MariaDBClient.h>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ci {

    using Json = nlohmann::json;
    using Done = std::function<void(const std::string&)>;
    using Value = std::function<void(const Json&, const std::string&)>;
    using HttpDone = std::function<void(int, const std::string&, const std::string&, const std::string&)>;
    struct Selection {
        Json sources = Json::object();
        std::size_t pending = 0;
        Value done;
    };
    Value checked(const Value& done, const std::function<void(const Json&)>& next);

    void require(bool condition, const std::string& message);
    std::string utc();
    std::string encode(const std::string& text);
    std::string protectedFile(const std::string& path);
    void https(const std::string& url, const std::string& method, const std::string& body, const std::string& token, const HttpDone& done);
    int compareVersions(const std::string& left, const std::string& right);
    bool validVersion(const std::string& tag);
    std::string sourceTag(const Json& tags, const std::string& requested);
    Json coordinates(const Json& target);
    Json& current(Json& execution);
    Json commands(Json& graph);
    std::vector<std::string> affectedRepositories(const Json& repositories, const std::string& repository);
    bool selectedTag(const Json& graph, const std::string& repository, const std::string& tag, const std::string& commit);
    Json selectRequest(Json& graph, const Json& selection, const std::string& repository, const std::string& commit = "");
    Json applyEvidence(
        Json& graph, const Json& selected, const std::string& stage, const Json& observed, const Json& evidence, std::string error);
    std::string
    selectWork(Json& graph, const Json& selection, const std::string& repository, const Json& target, std::vector<std::string> path = {});
    void retry(Json& graph, const std::string& execution);
    void retire(Json& graph, const std::string& execution, const std::string& acknowledgement);
    Json artifactManifest(const std::string& bytes, const Json& assignment);

    template <typename Records>
    auto findId(Records& records, const Json& id) {
        const auto found = std::find_if(records.begin(), records.end(), [&id](const auto& item) {
            return item.at("id") == id;
        });
        return found == records.end() ? nullptr : &*found;
    }

    Json projectWork(const Json& work, std::size_t index);
    Json projectRepositories(const Json& repositories);

    class Store {
    public:
        std::unique_ptr<database::mariadb::MariaDBClient> db;
        std::function<void(const Json&)> changed;
        std::function<void(const Json&)> dispatch;
        void initialize(const Done& done);
        database::mariadb::MariaDBCommandSequence& read(const Value& done);
        void change(const std::function<Json(Json&)>& operation, const Done& done);
    };

    class Controller {
    public:
        Controller(Store& store, Json deployment, std::string app, std::string installation, std::string key);
        void token(const std::function<void(const std::string&, const std::string&)>& done);
        void api(const std::string& token, const std::string& path, const Value& done);
        void pages(const std::string& token,
                   const std::string& path,
                   const std::string& field,
                   const Value& done,
                   int page = 1,
                   Json collected = Json::array());
        void contents(
            const std::string& token, const std::string& repository, const std::string& path, const std::string& ref, const Value& done);
        void enroll(const std::string& repository, const std::string& ref, const Done& done);
        void request(const std::string& repository, const std::string& tag, const Done& done, const std::string& commit = "");
        void resolve(const std::string& token,
                     const Json& enrollment,
                     const std::string& repository,
                     const std::string& tag,
                     const std::shared_ptr<Selection>& selection);
        void send(const Json& authorized);
        void observe(const std::string& execution, const Done& done, std::uint64_t candidate = 0, const std::string& attempt = "");
        void run(const std::string& token, const Json& work, const std::string& stage, const Value& done, std::uint64_t candidate = 0);
        void buildResult(const std::string& token, const Json& work, const Json& run, const Value& done);
        void publicationReceipt(const std::string& token, const Json& work, const Value& done);
        void webhook(const std::string& event, const Json& payload, const Done& done);
        void reconcile(const std::string& execution, const Done& done);
        void resume(const Done& done);
        const Json deployment;

    private:
        Store& store;
        std::string app;
        std::string installation;
        std::string key;
    };

} // namespace ci

#endif
