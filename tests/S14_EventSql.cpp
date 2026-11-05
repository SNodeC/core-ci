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
                                    DispatchAccepted{1, text, Kind::Build, 2147483647},
                                    DispatchRejected{2, text, Kind::Publish, 2, 999, text},
                                    BuildStarted{3, text, std::numeric_limits<std::uint64_t>::max(), 1},
                                    BuildSucceeded{4, text, 7, text},
                                    BuildFailed{5, text, 7, text},
                                    PublishSucceeded{6, text, 8, sha},
                                    PublishFailed{7, text, 8, ""},
                                    RetryRequested{8, text, text},
                                    Tick{9},
                                    Resumed{10}};
    const std::string prefix =
        "INSERT INTO coreci_events (recorded_at,event_type,package,target,tag,source_sha,origin,build_id,kind,"
        "http_status,message,run_id,run_attempt,attempt,artifact,feed_commit,reason,event_at) VALUES (UTC_TIMESTAMP(3),";
    const std::vector<std::string> expected{
        hex("TagObserved") + "," + hex(text) + ",NULL," + hex(text) + "," + hex(sha) + "," + hex("reconcile") +
            ",NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,-9)",
        hex("DispatchAccepted") + ",NULL,NULL,NULL,NULL,NULL," + hex(text) + "," + hex("build") +
            ",NULL,NULL,NULL,NULL,2147483647,NULL,NULL,NULL,1)",
        hex("DispatchRejected") + ",NULL,NULL,NULL,NULL,NULL," + hex(text) + "," + hex("publish") + ",999," + hex(text) +
            ",NULL,NULL,2,NULL,NULL,NULL,2)",
        hex("BuildStarted") + ",NULL,NULL,NULL,NULL,NULL," + hex(text) + ",NULL,NULL,NULL,18446744073709551615,1,NULL,NULL,NULL,NULL,3)",
        hex("BuildSucceeded") + ",NULL,NULL,NULL,NULL,NULL," + hex(text) + ",NULL,NULL,NULL,7,NULL,NULL," + hex(text) + ",NULL,NULL,4)",
        hex("BuildFailed") + ",NULL,NULL,NULL,NULL,NULL," + hex(text) + ",NULL,NULL,NULL,7,NULL,NULL,NULL,NULL," + hex(text) + ",5)",
        hex("PublishSucceeded") + ",NULL,NULL,NULL,NULL,NULL," + hex(text) + ",NULL,NULL,NULL,8,NULL,NULL,NULL," + hex(sha) + ",NULL,6)",
        hex("PublishFailed") + ",NULL,NULL,NULL,NULL,NULL," + hex(text) + ",NULL,NULL,NULL,8,NULL,NULL,NULL,NULL,_utf8mb4 X'',7)",
        hex("RetryRequested") + "," + hex(text) + "," + hex(text) + ",NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,8)",
        hex("Tick") + ",NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,9)",
        hex("Resumed") + ",NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,10)"};
    REQUIRE(events.size() == expected.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto sql = insertSql(events[i]);
        REQUIRE(std::holds_alternative<std::string>(sql));
        CHECK_EQ(std::get<std::string>(sql), prefix + expected[i]);
        CHECK(std::get<std::string>(sql).find(text) == std::string::npos);
    }
    CHECK(std::holds_alternative<Invalid>(insertSql(TagObserved{1, "", "tag", sha, Origin::Webhook})));
    CHECK_EQ(selectSql(),
             "SELECT seq,HEX(event_type) AS event_type,HEX(package) AS package,HEX(target) AS target,HEX(tag) AS tag,"
             "HEX(source_sha) AS source_sha,HEX(origin) AS origin,HEX(build_id) AS build_id,HEX(kind) AS kind,http_status,"
             "HEX(message) AS message,run_id,run_attempt,attempt,HEX(artifact) AS artifact,HEX(feed_commit) AS feed_commit,"
             "HEX(reason) AS reason,event_at FROM coreci_events ORDER BY seq");
    CHECK_EQ(hex("'\\\";--/* é😀"), "_utf8mb4 X'275c223b2d2d2f2a20c3a9f09f9880'");
    const auto graphEvent = GraphChanged{11, testPipeline()};
    const auto row = std::get<EventRow>(encode(graphEvent));
    const auto statements = graphInsertSql(*row.graph);
    CHECK_EQ(statements.size(), 21U);
    for (const auto& statement : statements) {
        CHECK(statement.find("VALUES (LAST_INSERT_ID(),_utf8mb4 X'") != std::string::npos);
        CHECK(statement.find("owner") == std::string::npos || statement.find("(event_seq,feed,owner") != std::string::npos ||
              statement.find("source_owner") != std::string::npos);
    }
    CHECK_EQ(graphSelectSql().size(), 4U);
    CHECK(std::get<std::string>(insertSql(graphEvent)).find(hex("GraphChanged")) != std::string::npos);
    return checkResult();
}
