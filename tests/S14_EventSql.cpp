#include "Check.h"
#include "Fixture.h"
#include "persistence/EventSql.h"

#include <limits>
#include <vector>
using namespace coreci;
using namespace coreci::persistence;

std::string hex(const std::string& s) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (const unsigned char c : s) {
        result += digits[c >> 4];
        result += digits[c & 15];
    }
    return "_utf8mb4 X'" + result + "'";
}
int main() {
    const std::string text = "'\\\";--/* é😀";
    const std::string sha(40, 'a');
    const std::vector<Event> events{TagObserved{-9, text, text, sha, Origin::Reconcile},
                                    WaveDispatchAccepted{1, text, 2147483647},
                                    WaveDispatchRejected{2, text, 2, 999, text},
                                    WaveStarted{3, text, std::numeric_limits<std::uint64_t>::max(), 1},
                                    UnitBuildStarted{4, text, text},
                                    UnitBuildSucceeded{4, text, text, text},
                                    UnitBuildFailed{5, text, text, text},
                                    UnitPublishSucceeded{6, text, text, sha},
                                    UnitPublishFailed{7, text, text, ""},
                                    WaveFinished{8, text, 7, text},
                                    RetryRequested{8, text, text},
                                    RebuildRequested{8, text, text},
                                    Tick{9},
                                    Resumed{10}};
    for (const auto& event : events) {
        const auto sql = insertSql(event);
        REQUIRE(std::holds_alternative<std::string>(sql));
        const auto& statement = std::get<std::string>(sql);
        CHECK(statement.starts_with("INSERT INTO coreci_events (recorded_at,event_type,"));
        CHECK(statement.find("VALUES (UTC_TIMESTAMP(3),_utf8mb4 X'") != std::string::npos);
        CHECK(statement.find(text) == std::string::npos);
        CHECK(statement.find("kind") == std::string::npos);
    }
    const auto dispatch = std::get<std::string>(insertSql(WaveDispatchAccepted{1, "w1", 2}));
    CHECK(dispatch.ends_with(hex("WaveDispatchAccepted") + ",NULL,NULL,NULL,NULL,NULL,NULL," + hex("w1") +
                             ",NULL,NULL,NULL,NULL,2,NULL,NULL,NULL,NULL,1,NULL)"));
    Event expiry = WaveExpired{40, "w1"};
    expiry.dispatchDeadline = 140;
    const auto expirySql = std::get<std::string>(insertSql(expiry));
    CHECK(expirySql.find(hex("WaveExpired")) != std::string::npos);
    CHECK(expirySql.ends_with(",40,140)"));
    CHECK(std::holds_alternative<Invalid>(insertSql(TagObserved{1, "", "tag", sha, Origin::Webhook})));
    CHECK_EQ(selectSql(),
             "SELECT seq,HEX(event_type) AS event_type,HEX(package) AS package,HEX(target) AS target,HEX(tag) AS tag,"
             "HEX(source_sha) AS source_sha,HEX(origin) AS origin,HEX(build_id) AS build_id,HEX(wave_id) AS wave_id,http_status,"
             "HEX(message) AS message,run_id,run_attempt,attempt,HEX(artifact) AS artifact,HEX(feed_commit) AS feed_commit,"
             "HEX(reason) AS reason,HEX(conclusion) AS conclusion,event_at,dispatch_deadline FROM coreci_events ORDER BY seq");
    CHECK_EQ(hex("'\\\";--/* é😀"), "_utf8mb4 X'275c223b2d2d2f2a20c3a9f09f9880'");
    const auto graphEvent = GraphChanged{11, testPipeline()};
    const auto row = std::get<EventRow>(encode(graphEvent));
    const auto statements = graphInsertSql(*row.graph);
    CHECK_EQ(statements.size(), 22U);
    for (const auto& statement : statements) {
        CHECK(statement.find("VALUES (LAST_INSERT_ID(),_utf8mb4 X'") != std::string::npos);
        CHECK(statement.find("owner") == std::string::npos || statement.find("(event_seq,feed,owner") != std::string::npos ||
              statement.find("source_owner") != std::string::npos);
    }
    CHECK_EQ(graphSelectSql().size(), 4U);
    CHECK(std::get<std::string>(insertSql(graphEvent)).find(hex("GraphChanged")) != std::string::npos);
    return checkResult();
}
