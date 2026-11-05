#include "Check.h"
#include "Fixture.h"
#include "persistence/InMemoryEventStore.h"

#include <limits>
#include <tuple>
using namespace coreci;
using namespace coreci::persistence;

std::string repeated(const std::string& character, std::size_t n) {
    std::string value;
    for (std::size_t i = 0; i < n; ++i) {
        value += character;
    }
    return value;
}
void fatal(const EventRow& row) {
    const auto decoded = decode(row);
    CHECK(std::holds_alternative<Invalid>(decoded));
    bool failed = false, done = false;
    InMemoryEventStore store({row});
    store.loadAll(
        [](auto, const auto&) {
            CHECK(false);
        },
        [&] {
            done = true;
        },
        [&](const auto& reason) {
            failed = true;
            CHECK(reason.kind == EventStore::StoreError::Kind::Corrupt);
            CHECK(reason.message.find("seq " + std::to_string(row.seq) + ":") != std::string::npos);
        });
    CHECK(failed);
    CHECK(!done);
}
void identifiers(Event event,
                 const std::function<void(Event&, const std::string&)>& set,
                 std::size_t maximum,
                 std::optional<std::string> EventRow::* member) {
    for (const auto& value : {std::string("x"), std::string("'\\\""), repeated("\xf0\x9f\x98\x80", maximum)}) {
        set(event, value);
        CHECK(std::holds_alternative<Event>(normalize(event)));
        const auto row = encode(event, 73);
        CHECK(std::holds_alternative<EventRow>(row));
        if (std::holds_alternative<EventRow>(row)) {
            CHECK(std::get<Event>(decode(std::get<EventRow>(row))) == event);
        }
    }
    const auto valid = std::get<EventRow>(encode(event, 73));
    for (const auto& value : {std::string{},
                              repeated("\xc3\xa9", maximum + 1),
                              std::string("x\0y", 3),
                              std::string("\xff"),
                              std::string("\xc0\xaf"),
                              std::string("\xed\xa0\x80"),
                              std::string("\xf4\x90\x80\x80"),
                              std::string("\xe2\x82")}) {
        set(event, value);
        CHECK(std::holds_alternative<Invalid>(normalize(event)));
        auto bad = valid;
        bad.*member = value;
        fatal(bad);
    }
}
int main() {
    const std::string hash(40, 'a');
    const std::string text = "'\\\" UTF-8 \xc3\xa9 \xf0\x9f\x98\x80";
    const std::vector<Event> events{TagObserved{std::numeric_limits<std::int64_t>::min(), "p", "v1.0.0", hash, Origin::Webhook},
                                    DispatchAccepted{1, "id", Kind::Build, 1},
                                    DispatchRejected{2, "id", Kind::Publish, 2, 999, text},
                                    BuildStarted{3, "id", std::numeric_limits<std::uint64_t>::max(), 1},
                                    BuildSucceeded{4, "id", 7, "artifact"},
                                    BuildFailed{5, "id", 7, text},
                                    PublishSucceeded{6, "id", 8, hash},
                                    PublishFailed{7, "id", 8, text},
                                    RetryRequested{8, "p", "t"},
                                    Tick{std::numeric_limits<std::int64_t>::max()},
                                    Resumed{10},
                                    GraphChanged{11, testPipeline()},
                                    GraphChanged{12, Pipeline{}}};
    const auto fields = std::tuple{&EventRow::package,
                                   &EventRow::target,
                                   &EventRow::tag,
                                   &EventRow::sourceSha,
                                   &EventRow::origin,
                                   &EventRow::buildId,
                                   &EventRow::kind,
                                   &EventRow::message,
                                   &EventRow::artifact,
                                   &EventRow::feedCommit,
                                   &EventRow::reason,
                                   &EventRow::httpStatus,
                                   &EventRow::runAttempt,
                                   &EventRow::attempt,
                                   &EventRow::eventAt,
                                   &EventRow::runId};
    for (const auto& event : events) {
        const auto encoded = encode(event, 73);
        REQUIRE(std::holds_alternative<EventRow>(encoded));
        const auto row = std::get<EventRow>(encoded);
        const auto decoded = decode(row);
        REQUIRE(std::holds_alternative<Event>(decoded));
        CHECK(std::get<Event>(decoded) == event);
        CHECK(std::get<EventRow>(encode(std::get<Event>(decoded), 73)) == row);
        const auto column = [&](auto member) {
            auto malformed = row;
            if ((malformed.*member).has_value()) {
                (malformed.*member).reset();
            } else {
                using Value = typename std::decay_t<decltype(malformed.*member)>::value_type;
                if constexpr (std::is_same_v<Value, std::string>) {
                    malformed.*member = "unused";
                } else {
                    malformed.*member = 1;
                }
            }
            fatal(malformed);
        };
        std::apply(
            [&](auto... member) {
                (column(member), ...);
            },
            fields);
    }
    auto bad = std::get<EventRow>(encode(events[0], 73));
    bad.eventType = "Unknown";
    fatal(bad);
    bad = std::get<EventRow>(encode(events[0], 73));
    bad.origin = "invalid";
    fatal(bad);
    bad = std::get<EventRow>(encode(events[1], 73));
    bad.kind = "invalid";
    fatal(bad);
    for (const auto n : {std::int64_t{-1}, std::int64_t{0}, std::int64_t{2147483648LL}}) {
        bad = std::get<EventRow>(encode(events[1], 73));
        bad.attempt = n;
        fatal(bad);
        bad = std::get<EventRow>(encode(events[3], 73));
        bad.runAttempt = n;
        fatal(bad);
    }
    for (const auto n : {-1, 1000}) {
        bad = std::get<EventRow>(encode(events[2], 73));
        bad.httpStatus = n;
        fatal(bad);
    }
    identifiers(
        events[0],
        [](Event& e, const auto& s) {
            std::get<TagObserved>(e).package = s;
        },
        128,
        &EventRow::package);
    identifiers(
        events[0],
        [](Event& e, const auto& s) {
            std::get<TagObserved>(e).tag = s;
        },
        255,
        &EventRow::tag);
    identifiers(
        events[8],
        [](Event& e, const auto& s) {
            std::get<RetryRequested>(e).target = s;
        },
        128,
        &EventRow::target);
    identifiers(
        events[1],
        [](Event& e, const auto& s) {
            std::get<DispatchAccepted>(e).buildId = s;
        },
        255,
        &EventRow::buildId);
    identifiers(
        events[4],
        [](Event& e, const auto& s) {
            std::get<BuildSucceeded>(e).artifact = s;
        },
        255,
        &EventRow::artifact);
    for (const auto& s : {std::string(39, 'a'), std::string(41, 'a'), std::string(40, 'A'), std::string(40, 'g'), std::string(40, '\0')}) {
        CHECK(std::holds_alternative<Invalid>(normalize(TagObserved{1, "p", "tag", s, Origin::Webhook})));
        CHECK(std::holds_alternative<Invalid>(normalize(PublishSucceeded{1, "id", 1, s})));
        bad = std::get<EventRow>(encode(events[0], 73));
        bad.sourceSha = s;
        fatal(bad);
        bad = std::get<EventRow>(encode(events[6], 73));
        bad.feedCommit = s;
        fatal(bad);
    }
    CHECK(std::holds_alternative<Event>(
        normalize(TagObserved{1, "p", "tag", "0123456789abcdef0123456789abcdef01234567", Origin::Reconcile})));
    CHECK(std::holds_alternative<Invalid>(normalize(TagObserved{1, "p", "tag", hash, static_cast<Origin>(99)})));
    CHECK(std::holds_alternative<Invalid>(normalize(DispatchAccepted{1, "id", static_cast<Kind>(99), 1})));
    for (const auto n : {0U, std::numeric_limits<unsigned>::max()}) {
        CHECK(std::holds_alternative<Invalid>(normalize(DispatchAccepted{1, "id", Kind::Build, n})));
        CHECK(std::holds_alternative<Invalid>(normalize(BuildStarted{1, "id", 1, n})));
    }
    CHECK(std::holds_alternative<Event>(normalize(BuildStarted{1, "id", 0, 2147483647U})));
    CHECK(std::holds_alternative<Event>(normalize(DispatchAccepted{1, "id", Kind::Build, 2147483647U})));
    for (const auto n : {0, 999}) {
        CHECK(std::holds_alternative<Event>(normalize(DispatchRejected{1, "id", Kind::Build, 1, n, ""})));
    }
    for (const auto n : {-1, 1000}) {
        CHECK(std::holds_alternative<Invalid>(normalize(DispatchRejected{1, "id", Kind::Build, 1, n, ""})));
    }
    // Every free-text field is normalized, and decoding requires already-normalized values.
    const std::vector<std::pair<std::string, std::string>> texts{{"", ""},
                                                                 {std::string("A\0B\xff", 4), "AB\xef\xbf\xbd"},
                                                                 {"\xe2\x82", "\xef\xbf\xbd"},
                                                                 {std::string(4095, 'x') + "\xc3\xa9", std::string(4095, 'x')},
                                                                 {std::string(4094, 'x') + "\xc3\xa9", std::string(4094, 'x') + "\xc3\xa9"},
                                                                 {std::string(4093, 'x') + "\xff", std::string(4093, 'x') + "\xef\xbf\xbd"},
                                                                 {std::string(10000, 'x'), std::string(4096, 'x')}};
    for (const auto& [raw, expected] : texts) {
        for (auto event : std::vector<Event>{
                 DispatchRejected{1, "id", Kind::Build, 1, 500, raw}, BuildFailed{1, "id", 1, raw}, PublishFailed{1, "id", 1, raw}}) {
            const auto normalized = normalize(event);
            REQUIRE(std::holds_alternative<Event>(normalized));
            CHECK(normalize(std::get<Event>(normalized)) == normalized);
            const auto row = std::get<EventRow>(encode(event, 73));
            CHECK_EQ(row.message.value_or(row.reason.value_or("")), expected);
            CHECK(std::holds_alternative<Event>(decode(row)));
            if (raw != expected) {
                bad = row;
                if (bad.message) {
                    bad.message = raw;
                } else {
                    bad.reason = raw;
                }
                fatal(bad);
            }
        }
    }
    return checkResult();
}
