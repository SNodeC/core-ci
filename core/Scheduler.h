#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace coreci {
    using Json = nlohmann::json;
    using Time = std::int64_t;

    struct Options {
        std::string branch = "feed", workflowRepository, fault;
        unsigned slots = 20;
        Time batch = 120, queue = 7200, execution = 21600, grace = 900, poll = 300;
        std::optional<bool> paused;
    };

    struct Result {
        int code = 200;
        std::string text;
    };

    Json initialState();
    std::string sha256(const std::string& bytes);
    std::string attemptKey(const std::string& id, unsigned attempt);
    bool stableTag(const std::string& tag);
    bool reserved(const Json& attempt);
    Json view(const Json& state, const Options& options);
    Json retirements(const Json& entries, Time now, const Json& planned = Json::array());

    class Scheduler {
    public:
        explicit Scheduler(Options options);
        const Options& options() const;
        Result admit(Json& state, const std::string& repo, const std::string& tag, const std::string& commit) const;
        Result declarations(Json& state, const Json& catalog, const Json& declarations, const Json& releases) const;
        Result action(Json& state, const std::string& action, const Json& body) const;
        Result turn(Json& state, Time now) const;
        Result claim(Json& state, bool publication, const std::string& id, const Json& owner, Time now) const;
        Result report(Json& state, bool publication, const std::string& id, const Json& record, Time now) const;
        Result observed(Json& state, const std::string& key, const Json& run, Time now) const;
        Result feed(Json& state, const std::string& head, const Json& snapshots, const Json& ledger, Time now) const;

    private:
        Options config;
    };
} // namespace coreci
