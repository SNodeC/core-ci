#include "Check.h"
#include "core/Status.h"
#include "persistence/EventQueue.h"
#include "persistence/InMemoryEventStore.h"
using namespace coreci;
using namespace coreci::persistence;

Pipeline pipeline() {
    Pipeline::Builder b;
    b.feed({"f", {"o", "publisher", "publish.yml", "stable"}})
        .target({"t", "f"})
        .package({"a", {"o", "a", "trunk"}, {"o", "recipes", "build.yml", "trunk"}, {"t"}, {}})
        .package({"b", {"o", "b", "trunk"}, {"o", "recipes", "build.yml", "trunk"}, {"t"}, {"a"}});
    return b.build();
}
Engine engine() {
    return Engine({100, 50, 8, {}});
}
Event tag(const std::string& package, std::int64_t at = 100) {
    return TagObserved{at, package, "v1.0.0", std::string(40, 'a'), Origin::Webhook};
}
Command buildCommand() {
    return DispatchBuild{"a~t~1.0.0~r1",
                         1,
                         "a",
                         {"o", "a", "trunk"},
                         "v1.0.0",
                         *Version::parse("1.0.0"),
                         1,
                         "t",
                         {},
                         {"o", "recipes", "build.yml", "trunk"}};
}
int recovery(InMemoryEventStore::Failure failure) {
    InMemoryEventStore store({std::get<EventRow>(encode(GraphChanged{0, pipeline()}, 1))});
    std::vector<Result> replies;
    std::vector<Command> emitted;
    int ready = 0;
    std::int64_t now = 10;
    EventQueue queue(
        store,
        engine,
        [&] {
            return Resumed{now};
        },
        [&](const auto& commands) {
            emitted.insert(emitted.end(), commands.begin(), commands.end());
        },
        [](auto, const auto&) {
            CHECK(false);
        });
    queue.start(
        [&] {
            ++ready;
        },
        [](const auto&) {
            CHECK(false);
        });
    REQUIRE(queue.engine());
    CHECK_EQ(ready, 1);
    CHECK(emitted.empty());
    store.defer(true);
    store.failNextAppend(failure);
    const auto reply = [&](Result result) {
        replies.push_back(std::move(result));
    };
    queue.enqueue(tag("a"), reply);
    queue.enqueue(tag("b", 101), reply);
    CHECK_EQ(store.pending(), 1U);
    CHECK(replies.empty());
    CHECK(!queue.engine()->releases().at("a"));
    now = 110;
    REQUIRE(store.completeOne());
    CHECK_EQ(replies.size(), 2U);
    CHECK(std::holds_alternative<Unavailable>(replies[0]));
    CHECK(std::holds_alternative<Unavailable>(replies[1]));
    CHECK(!queue.engine());
    CHECK(emitted.empty());
    queue.enqueue(TagObserved{111, "", "", "", Origin::Webhook}, reply);
    CHECK(std::holds_alternative<Unavailable>(replies.back()));
    REQUIRE(store.completeOne()); // reconnect/open
    REQUIRE(store.completeOne()); // load/replay; Resumed append pending
    CHECK(!queue.engine());
    CHECK(emitted.empty());
    REQUIRE(store.completeOne()); // Resumed commits, then commands and availability
    REQUIRE(queue.engine());
    CHECK_EQ(ready, 2);
    const bool stored = failure == InMemoryEventStore::Failure::AfterStore;
    CHECK(emitted == (stored ? std::vector<Command>{buildCommand()} : std::vector<Command>{}));
    queue.enqueue(tag("a", 111), reply);
    if (stored) {
        CHECK(std::holds_alternative<Ignored>(replies.back()));
        CHECK_EQ(store.pending(), 0U);
    } else {
        REQUIRE(store.completeOne());
        CHECK(std::holds_alternative<Accepted>(replies.back()));
        CHECK_EQ(store.rows()[2].seq, 4U); // graph=1, Resumed=2, failed append consumed seq 3
    }
    CHECK((emitted == std::vector<Command>{buildCommand()}));
    store.defer(false);
    Engine fresh = engine();
    store.loadAll(
        [&](auto, const Event& event) {
            fresh.handle(event);
        },
        [] {
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK_EQ(statusText(*queue.engine()), statusText(fresh));
    CHECK(queue.engine()->units() == fresh.units());
    CHECK(queue.engine()->feeds() == fresh.feeds());
    CHECK(queue.engine()->releases() == fresh.releases());
    CHECK(queue.engine()->time() == fresh.time());
    return checkResult();
}
class UnavailableStore : public InMemoryEventStore {
public:
    explicit UnavailableStore(bool failOpen)
        : failOpen(failOpen) {
    }
    void open(std::function<void()> ready, std::function<void(const StoreError&)> error) override {
        if (failOpen) {
            error({StoreError::Kind::Unavailable, "open unavailable"});
        } else {
            ready();
        }
    }
    void loadAll(std::function<void(std::uint64_t, const Event&)>,
                 std::function<void()>,
                 std::function<void(const StoreError&)> error) override {
        error({StoreError::Kind::Unavailable, "load unavailable"});
    }

private:
    bool failOpen;
};
void unavailableErrors() {
    for (const bool failOpen : {false, true}) {
        UnavailableStore store(failOpen);
        bool reported = false;
        EventQueue queue(
            store,
            engine,
            [] {
                return Resumed{100};
            },
            [](const auto&) {
                CHECK(false);
            },
            [](auto, const auto&) {
                CHECK(false);
            });
        queue.start(
            [] {
                CHECK(false);
            },
            [&](const auto& error) {
                reported = true;
                CHECK(error.kind == EventStore::StoreError::Kind::Unavailable);
                CHECK_EQ(error.message, failOpen ? "open unavailable" : "load unavailable");
            });
        CHECK(reported);
        CHECK(!queue.engine());
        queue.enqueue(tag("a"), [](Result result) {
            CHECK(std::holds_alternative<Unavailable>(result));
        });
        CHECK(store.rows().empty());
    }
}
int externalRecovery() {
    InMemoryEventStore store({std::get<EventRow>(encode(GraphChanged{0, pipeline()}, 1))});
    std::vector<Command> emitted;
    std::vector<Result> replies;
    int ready = 0;
    EventQueue queue(
        store,
        engine,
        [] {
            return Resumed{110};
        },
        [&](const auto& commands) {
            emitted.insert(emitted.end(), commands.begin(), commands.end());
        },
        [](auto, const auto&) {
            CHECK(false);
        });
    queue.start(
        [&] {
            ++ready;
        },
        [](const auto&) {
            CHECK(false);
        });
    store.defer(true);
    queue.enqueue(tag("a"), [&](Result r) {
        replies.push_back(std::move(r));
    });
    queue.enqueue(tag("b"), [&](Result r) {
        replies.push_back(std::move(r));
    });
    queue.recover("connection lost");
    CHECK(!queue.engine());
    CHECK_EQ(replies.size(), 2U);
    CHECK(std::holds_alternative<Unavailable>(replies[0]));
    CHECK(std::holds_alternative<Unavailable>(replies[1]));
    REQUIRE(store.completeOne()); // old append commits after recovery started
    CHECK(emitted.empty());
    CHECK_EQ(replies.size(), 2U);
    queue.recover("connection lost again"); // earlier open callback is now stale too
    REQUIRE(store.completeOne());
    CHECK_EQ(store.pending(), 1U);
    REQUIRE(store.completeOne()); // current open
    REQUIRE(store.completeOne()); // current load
    REQUIRE(store.completeOne()); // Resumed commit
    REQUIRE(queue.engine());
    CHECK_EQ(ready, 2);
    CHECK((emitted == std::vector<Command>{buildCommand()}));
    CHECK(queue.engine()->releases().at("a").has_value());
    CHECK(!queue.engine()->releases().at("b"));
    return checkResult();
}
int main() {
    InMemoryEventStore store({std::get<EventRow>(encode(GraphChanged{0, pipeline()}, 1))});
    std::vector<Result> replies;
    std::vector<Command> emitted;
    bool ready = false;
    bool reenter = false;
    EventQueue* active = nullptr;
    const auto reply = [&](Result result) {
        replies.push_back(std::move(result));
    };
    EventQueue queue(
        store,
        engine,
        [] {
            return Resumed{10};
        },
        [&](const auto& commands) {
            emitted.insert(emitted.end(), commands.begin(), commands.end());
            if (reenter) {
                reenter = false;
                active->enqueue(DispatchAccepted{101, "a~t~1.0.0~r1", Kind::Build, 1}, reply);
                CHECK(replies.empty()); // command callback precedes original request reply
            }
        },
        [](auto, const auto&) {
            CHECK(false);
        });
    active = &queue;
    queue.enqueue(tag("a"), reply);
    CHECK(std::holds_alternative<Unavailable>(replies.back()));
    replies.clear();
    queue.start(
        [&] {
            ready = true;
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK(ready);
    REQUIRE(queue.engine());
    CHECK(!queue.engine()->time().has_value() || *queue.engine()->time() == 10);
    const auto rows = store.rows().size();
    // Invalid events are rejected before classification, even for unknown packages.
    queue.enqueue(TagObserved{999, "unknown", "v1.0.0", "bad", Origin::Webhook}, reply);
    CHECK(std::holds_alternative<Invalid>(replies.back()));
    CHECK_EQ(store.rows().size(), rows);
    CHECK_EQ(*queue.engine()->time(), 10);
    replies.clear();
    store.defer(true);
    reenter = true;
    queue.enqueue(tag("a"), reply);
    CHECK_EQ(store.pending(), 1U);
    CHECK(emitted.empty());
    CHECK(replies.empty());
    CHECK(!queue.engine()->releases().at("a"));
    REQUIRE(store.completeOne());
    CHECK((emitted == std::vector<Command>{buildCommand()}));
    CHECK_EQ(replies.size(), 1U);
    CHECK(std::holds_alternative<Accepted>(replies.back()));
    CHECK_EQ(store.pending(), 1U);
    CHECK(queue.engine()->units().at({"a", "t"}).inflight->phase == Phase::Dispatching);
    queue.enqueue(tag("a", 102), reply); // duplicate waits behind pending dispatch result
    CHECK_EQ(replies.size(), 1U);
    REQUIRE(store.completeOne());
    CHECK_EQ(replies.size(), 3U);
    CHECK(std::holds_alternative<Accepted>(replies[1]));
    CHECK(std::holds_alternative<Ignored>(replies[2]));
    CHECK_EQ(store.rows().size(), rows + 2);
    CHECK_EQ(store.pending(), 0U);
    CHECK(queue.engine()->units().at({"a", "t"}).inflight->phase == Phase::Building);

    // The committed row and engine both receive the normalized free text.
    store.defer(false);
    queue.enqueue(BuildFailed{103, "a~t~1.0.0~r1", 101, std::string("A\0B\xff", 4)}, reply);
    CHECK(std::holds_alternative<Accepted>(replies.back()));
    CHECK_EQ(*store.rows().back().reason, "AB\xef\xbf\xbd");
    CHECK_EQ(queue.engine()->units().at({"a", "t"}).failure->reason, "AB\xef\xbf\xbd");

    externalRecovery();
    unavailableErrors();
    recovery(InMemoryEventStore::Failure::BeforeStore);
    recovery(InMemoryEventStore::Failure::AfterStore);

    // Replay warnings expose actual seq, even with gaps and out-of-order seed rows.
    auto removed = std::get<EventRow>(encode(tag("removed"), 9));
    auto known = std::get<EventRow>(encode(tag("b"), 27));
    InMemoryEventStore changed({known, removed, std::get<EventRow>(encode(GraphChanged{0, pipeline()}, 1))});
    std::vector<std::uint64_t> warnings;
    bool changedReady = false;
    EventQueue changedQueue(
        changed,
        engine,
        [] {
            return Resumed{110};
        },
        [](const auto& c) {
            CHECK(c.empty());
        },
        [&](auto seq, const auto&) {
            warnings.push_back(seq);
        });
    changedQueue.start(
        [&] {
            changedReady = true;
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK(changedReady);
    CHECK((warnings == std::vector<std::uint64_t>{9}));
    CHECK_EQ(changed.rows().back().seq, 28U);
    REQUIRE(changedQueue.engine());
    CHECK(changedQueue.engine()->releases().at("b").has_value());

    auto bad = known;
    bad.seq = 321;
    bad.package.reset();
    InMemoryEventStore corrupt({bad});
    bool fatal = false, started = false;
    EventQueue refused(
        corrupt,
        engine,
        [] {
            return Resumed{110};
        },
        [](const auto&) {
            CHECK(false);
        },
        [](auto, const auto&) {
        });
    refused.start(
        [&] {
            started = true;
        },
        [&](const auto& error) {
            fatal = true;
            CHECK(error.kind == EventStore::StoreError::Kind::Corrupt);
            CHECK(error.message.find("seq 321") != std::string::npos);
        });
    CHECK(fatal);
    CHECK(!started);
    CHECK(!refused.engine());
    CHECK_EQ(corrupt.rows().size(), 1U);
    return checkResult();
}
