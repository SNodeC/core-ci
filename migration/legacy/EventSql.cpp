#include "EventSql.h"

namespace coreci::legacy::persistence {
    namespace {
        std::string literal(const std::string& value) {
            constexpr char digits[] = "0123456789abcdef";
            std::string result = "_utf8mb4 X'";
            for (const unsigned char byte : value) {
                result += digits[byte >> 4];
                result += digits[byte & 15];
            }
            result += "'";
            return result;
        }
        template <typename T>
        std::string literal(const std::optional<T>& value) {
            std::string result = "NULL";
            if (value) {
                if constexpr (std::is_same_v<T, std::string>) {
                    result = literal(*value);
                } else {
                    result = std::to_string(*value);
                }
            }
            return result;
        }
    } // namespace
    std::variant<std::string, Invalid> insertSql(const Event& event) {
        const auto encoded = encode(event);
        std::variant<std::string, Invalid> result;
        if (std::holds_alternative<Invalid>(encoded)) {
            result = std::get<Invalid>(encoded);
        } else {
            const auto& r = std::get<EventRow>(encoded);
            result =
                "INSERT INTO coreci_events (recorded_at,event_type,package,target,tag,source_sha,origin,build_id,wave_id,"
                "http_status,message,run_id,run_attempt,attempt,artifact,feed_commit,reason,conclusion,event_at,dispatch_deadline) VALUES "
                "(UTC_TIMESTAMP(3)," +
                literal(r.eventType) + "," + literal(r.package) + "," + literal(r.target) + "," + literal(r.tag) + "," +
                literal(r.sourceSha) + "," + literal(r.origin) + "," + literal(r.buildId) + "," + literal(r.waveId) + "," +
                literal(r.httpStatus) + "," + literal(r.message) + "," + literal(r.runId) + "," + literal(r.runAttempt) + "," +
                literal(r.attempt) + "," + literal(r.artifact) + "," + literal(r.feedCommit) + "," + literal(r.reason) + "," +
                literal(r.conclusion) + "," + literal(r.eventAt) + "," + literal(r.dispatchDeadline) + ")";
        }
        return result;
    }
    std::vector<std::string> graphInsertSql(const GraphRows& rows) {
        std::vector<std::string> result;
        for (const auto& tag : rows.releases) {
            result.push_back("INSERT INTO coreci_graph_releases (event_seq,package,tag,source_sha,origin) VALUES (LAST_INSERT_ID()," +
                             literal(tag.package) + "," + literal(tag.tag) + "," + literal(tag.sourceSha) + "," +
                             literal(tag.origin == Origin::Webhook ? std::string("webhook") : std::string("reconcile")) + ")");
        }
        for (const auto& pkg : rows.packages) {
            result.push_back("INSERT INTO coreci_graph_packages (event_seq,package,source_owner,source_repo,source_branch,revision_base) "
                             "VALUES (LAST_INSERT_ID()," +
                             literal(pkg.name) + "," + literal(pkg.sourceRepo.owner) + "," + literal(pkg.sourceRepo.repo) + "," +
                             literal(pkg.sourceRepo.branch) + "," + std::to_string(pkg.revisionBase) + ")");
        }
        for (const auto& t : rows.targets) {
            result.push_back("INSERT INTO coreci_graph_targets (event_seq,package,target) VALUES (LAST_INSERT_ID()," + literal(t.package) +
                             "," + literal(t.target) + ")");
        }
        for (const auto& q : rows.prerequisites) {
            result.push_back("INSERT INTO coreci_graph_prerequisites (event_seq,package,prerequisite) VALUES (LAST_INSERT_ID()," +
                             literal(q.package) + "," + literal(q.prerequisite) + ")");
        }
        return result;
    }
    std::vector<std::string> graphSelectSql() {
        return {"SELECT event_seq,HEX(package),HEX(source_owner),HEX(source_repo),HEX(source_branch),revision_base FROM "
                "coreci_graph_packages ORDER BY event_seq,package",
                "SELECT event_seq,HEX(package),HEX(target) FROM coreci_graph_targets ORDER BY event_seq,package,target",
                "SELECT event_seq,HEX(package),HEX(prerequisite) FROM coreci_graph_prerequisites ORDER BY event_seq,package,prerequisite",
                "SELECT event_seq,HEX(package),HEX(tag),HEX(source_sha),HEX(origin) FROM coreci_graph_releases ORDER BY event_seq,package"};
    }
    std::string selectSql() {
        return "SELECT seq,HEX(event_type) AS event_type,HEX(package) AS package,HEX(target) AS target,HEX(tag) AS tag,"
               "HEX(source_sha) AS source_sha,HEX(origin) AS origin,HEX(build_id) AS build_id,HEX(wave_id) AS wave_id,http_status,"
               "HEX(message) AS message,run_id,run_attempt,attempt,HEX(artifact) AS artifact,HEX(feed_commit) AS feed_commit,"
               "HEX(reason) AS reason,HEX(conclusion) AS conclusion,event_at,dispatch_deadline FROM coreci_events ORDER BY seq";
    }
} // namespace coreci::legacy::persistence
