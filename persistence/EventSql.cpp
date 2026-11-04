#include "EventSql.h"

namespace coreci::persistence {
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
            result = "INSERT INTO coreci_events (recorded_at,event_type,package,target,tag,source_sha,origin,build_id,kind,"
                     "http_status,message,run_id,run_attempt,attempt,artifact,feed_commit,reason,event_at) VALUES (UTC_TIMESTAMP(3)," +
                     literal(r.eventType) + "," + literal(r.package) + "," + literal(r.target) + "," + literal(r.tag) + "," +
                     literal(r.sourceSha) + "," + literal(r.origin) + "," + literal(r.buildId) + "," + literal(r.kind) + "," +
                     literal(r.httpStatus) + "," + literal(r.message) + "," + literal(r.runId) + "," + literal(r.runAttempt) + "," +
                     literal(r.attempt) + "," + literal(r.artifact) + "," + literal(r.feedCommit) + "," + literal(r.reason) + "," +
                     literal(r.eventAt) + ")";
        }
        return result;
    }
    std::string selectSql() {
        return "SELECT seq,HEX(event_type) AS event_type,HEX(package) AS package,HEX(target) AS target,HEX(tag) AS tag,"
               "HEX(source_sha) AS source_sha,HEX(origin) AS origin,HEX(build_id) AS build_id,HEX(kind) AS kind,http_status,"
               "HEX(message) AS message,run_id,run_attempt,attempt,HEX(artifact) AS artifact,HEX(feed_commit) AS feed_commit,"
               "HEX(reason) AS reason,event_at FROM coreci_events ORDER BY seq";
    }
} // namespace coreci::persistence
