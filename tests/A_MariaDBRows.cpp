#include "Check.h"
#include "adapters/mariadb/MariaDBEventStore.h"

using namespace coreci;
using namespace coreci::persistence;
using namespace coreci::mariadb;
int main() {
    std::vector<std::optional<std::string>> fields(20);
    fields[0] = "73";
    fields[1] = "556e69745075626c6973684661696c6564"; // UnitPublishFailed
    fields[8] = "7731";                               // w1
    fields[7] = "6964";                               // id

    fields[16] = ""; // empty reason, SQL non-NULL
    fields[18] = "-9223372036854775808";
    auto decoded = decodeFields(fields);
    REQUIRE(std::holds_alternative<EventRow>(decoded));
    auto row = std::get<EventRow>(decoded);
    CHECK_EQ(row.seq, 73U);
    CHECK(row.reason && row.reason->empty());
    CHECK(!row.message);
    CHECK(std::holds_alternative<Event>(decode(row)));
    for (const auto& value : {"x", "a", "0G"}) {
        auto bad = fields;
        bad[7] = value;
        const auto result = decodeFields(bad);
        REQUIRE(std::holds_alternative<Invalid>(result));
        CHECK(std::get<Invalid>(result).reason.starts_with("seq 73:"));
    }
    for (const auto index : {0, 11, 18, 19}) {
        for (const auto& value : {"", "+1", "1x", " 1", "18446744073709551616"}) {
            auto bad = fields;
            bad[index] = value;
            CHECK(std::holds_alternative<Invalid>(decodeFields(bad)));
        }
    }
    for (const auto index : {9, 12, 13}) {
        auto bad = fields;
        bad[index] = "1x";
        CHECK(std::holds_alternative<Invalid>(decodeFields(bad)));
    }
    auto bad = fields;
    bad[11] = "-1";
    CHECK(std::holds_alternative<Invalid>(decodeFields(bad)));
    bad = fields;
    bad[1].reset();
    CHECK(std::holds_alternative<Invalid>(decodeFields(bad)));
    bad = fields;
    bad[7] = "c3a9f09f9880";
    const auto unicode = std::get<EventRow>(decodeFields(bad));
    CHECK_EQ(*unicode.buildId, "é😀");
    CHECK(std::holds_alternative<Event>(decode(unicode)));
    fields[16] = "610062"; // embedded NUL survives HEX transport and is rejected by EventRow
    decoded = decodeFields(fields);
    REQUIRE(std::holds_alternative<EventRow>(decoded));
    CHECK(std::holds_alternative<Invalid>(decode(std::get<EventRow>(decoded))));
    fields[16].reset(); // missing required reason
    CHECK(std::holds_alternative<Invalid>(decode(std::get<EventRow>(decodeFields(fields)))));
    fields.pop_back();
    CHECK(std::holds_alternative<Invalid>(decodeFields(fields)));
    MariaDBEventStore unopened({});
    bool unavailable = false, appendFailed = false;
    unopened.loadAll(
        [](auto, const auto&) {
            CHECK(false);
        },
        [] {
            CHECK(false);
        },
        [&](const auto& error) {
            unavailable = true;
            CHECK(error.kind == EventStore::StoreError::Kind::Unavailable);
        });
    unopened.append(
        Tick{1},
        [](std::uint64_t) {
            CHECK(false);
        },
        [&](const auto&) {
            appendFailed = true;
        });
    CHECK(unavailable);
    CHECK(appendFailed);
    return checkResult();
}
