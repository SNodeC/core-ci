#include "Check.h"
#include "core/Status.h"
#include "persistence/EventQueue.h"
#include "tests/InMemoryEventStore.h"
#include "tests/Status.h"
using namespace coreci;
using namespace coreci::persistence;

Pipeline pipeline() {
    Pipeline::Builder b;
    b.target({"t"}).package({"a", {"o", "a", "trunk"}, {"t"}, {}}).package({"b", {"o", "b", "trunk"}, {"t"}, {"a"}});
    return b.build();
}
Engine engine() {
    return Engine({0, 100, 8, 0});
}
Event tag(const std::string& package, std::int64_t at = 100) {
    return TagObserved{at, package, "v1.0.0", std::string(40, 'a'), Origin::Webhook};
}
std::vector<Command> actions(const EventQueue& queue) {
    return queue.engine() ? queue.engine()->commands() : std::vector<Command>{};
}
int recovery(InMemoryEventStore::Failure failure) {
    InMemoryEventStore store({std::get<EventRow>(encode(GraphChanged{0, pipeline()}, 1))});
    std::vector<Result> replies;
    int ready = 0;
    std::int64_t now = 10;
    EventQueue queue(
        store,
        engine,
        [&] {
            return Resumed{now};
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
    CHECK(actions(queue).empty());
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
    CHECK(actions(queue).empty());
    queue.enqueue(TagObserved{111, "", "", "", Origin::Webhook}, reply);
    CHECK(std::holds_alternative<Unavailable>(replies.back()));
    REQUIRE(store.completeOne()); // reconnect/open
    REQUIRE(store.completeOne()); // load/replay; Resumed append pending
    CHECK(!queue.engine());
    CHECK(actions(queue).empty());
    REQUIRE(store.completeOne()); // Resumed commits, then commands and availability
    REQUIRE(queue.engine());
    CHECK_EQ(ready, 2);
    const bool stored = failure == InMemoryEventStore::Failure::AfterStore;
    CHECK_EQ(actions(queue).size(), stored ? 1U : 0U);
    queue.enqueue(tag("a", 111), reply);
    if (stored) {
        CHECK(std::holds_alternative<Ignored>(replies.back()));
        CHECK_EQ(store.pending(), 0U);
    } else {
        REQUIRE(store.completeOne());
        CHECK(std::holds_alternative<Accepted>(replies.back()));
        CHECK_EQ(store.rows()[2].seq, 4U); // graph=1, Resumed=2, failed append consumed seq 3
    }
    CHECK_EQ(actions(queue).size(), 1U);
    store.defer(false);
    Engine fresh = engine();
    store.loadAll(
        [&](auto seq, const Event& event) {
            fresh.handle(event, seq);
        },
        [] {
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK_EQ(recordedStatus(*queue.engine()), recordedStatus(fresh));
    CHECK(queue.engine()->units() == fresh.units());
    CHECK(queue.engine()->waves() == fresh.waves());
    CHECK(queue.engine()->releases() == fresh.releases());
    CHECK(queue.engine()->time() == fresh.time());
    return checkResult();
}
void stopping() {
    for (unsigned phase = 0; phase != 4; ++phase) {
        InMemoryEventStore store({std::get<EventRow>(encode(GraphChanged{0, pipeline()}, 1))});
        store.defer(true);
        bool live = true;
        unsigned ready = 0, committed = 0;
        EventQueue queue(
            store,
            engine,
            [] {
                return Resumed{100};
            },
            [](auto, const auto&) {
                CHECK(false);
            },
            [&](auto, const auto&, const auto&) {
                ++committed;
            },
            [&] {
                return live;
            });
        const auto start = [&] {
            queue.start(
                [&] {
                    ++ready;
                },
                [](const auto&) {
                    CHECK(false);
                });
        };
        start();
        for (unsigned completed = 0; completed < phase; ++completed) {
            CHECK(store.completeOne());
        }
        if (phase == 3) {
            store.failNextAppend(InMemoryEventStore::Failure::AfterStore);
            queue.enqueue(tag("a"), [](const auto&) {
                CHECK(false); // entered callback is fenced after termination
            });
        }
        const auto rows = store.rows().size();
        const auto callbacks = committed;
        live = false;
        CHECK(!queue.engine());
        queue.enqueue(tag("b"), [](const auto& result) {
            CHECK(std::holds_alternative<Unavailable>(result));
        });
        while (store.completeOne()) {
        }
        queue.recover("late connection loss during stop");
        start();
        CHECK_EQ(store.pending(), 0U);
        CHECK_EQ(store.rows().size(), rows + (phase >= 2 ? 1U : 0U));
        CHECK_EQ(committed, callbacks);
        CHECK_EQ(ready, phase == 3 ? 1U : 0U);
        CHECK(!queue.engine());
    }
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
    std::vector<Result> replies;
    int ready = 0;
    EventQueue queue(
        store,
        engine,
        [] {
            return Resumed{110};
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
    CHECK(actions(queue).empty());
    CHECK_EQ(replies.size(), 2U);
    queue.recover("connection lost again"); // earlier open callback is now stale too
    REQUIRE(store.completeOne());
    CHECK_EQ(store.pending(), 1U);
    REQUIRE(store.completeOne()); // current open
    REQUIRE(store.completeOne()); // current load
    REQUIRE(store.completeOne()); // Resumed commit
    REQUIRE(queue.engine());
    CHECK_EQ(ready, 2);
    CHECK_EQ(actions(queue).size(), 1U);
    CHECK(queue.engine()->releases().at("a").has_value());
    CHECK(!queue.engine()->releases().at("b"));
    return checkResult();
}
int restartDispatching() {
    for (const auto resumeAt : {110, 115}) {
        InMemoryEventStore store({std::get<EventRow>(encode(GraphChanged{0, pipeline()}, 1))});
        std::int64_t clock = 10;
        Plan frozen;
        const auto make = [] {
            return Engine({0, 100, 8, 0, "feed", {"o", "Packages", "main"}, 15});
        };
        const auto resumed = [&] {
            return Resumed{clock};
        };
        const auto warning = [](auto, const auto&) {
            CHECK(false);
        };
        const auto ready = [] {
        };
        const auto error = [](const auto&) {
            CHECK(false);
        };
        const auto reply = [](const auto&) {
        };
        {
            EventQueue first(store, make, resumed, warning);
            first.start(ready, error);
            first.enqueue(tag("a"), reply);
            first.enqueue(WaveDispatchAccepted{101, "w3", 1}, reply);
            first.enqueue(WaveDispatchAccepted{999, "w3", 1}, reply);
            CHECK(actions(first).empty());
            if (first.engine() && first.engine()->activeWave()) {
                CHECK(first.engine()->activeWave()->phase == WavePhase::Dispatching);
                CHECK_EQ(*first.engine()->time(), 101);
                frozen = first.engine()->activeWave()->plan;
            } else {
                CHECK(false);
            }
        }
        clock = resumeAt;
        EventQueue restarted(store, make, resumed, warning);
        restarted.start(ready, error);
        CHECK_EQ(actions(restarted).size(), resumeAt == 115 ? 1U : 0U);
        if (resumeAt < 115) {
            CHECK_EQ(*restarted.engine()->nextDeadline(), 115);
            restarted.enqueue(Tick{114}, reply);
            CHECK(actions(restarted).empty());
            restarted.enqueue(Tick{115}, reply);
        }
        CHECK_EQ(actions(restarted).size(), 1U);
        if (!actions(restarted).empty() && restarted.engine() && restarted.engine()->activeWave()) {
            CHECK_EQ(actions(restarted).front().waveId, "w3");
            CHECK_EQ(actions(restarted).front().attempt, 2U);
            CHECK(restarted.engine()->activeWave()->plan == frozen);
            CHECK_EQ(restarted.engine()->activeWave()->deadline, 200);
            restarted.enqueue(WaveFinished{115, "w3", 42, "success"}, reply);
            CHECK(!restarted.engine()->activeWave()->runId);
            restarted.enqueue(WaveDispatchAccepted{116, "w3", 2}, reply);
            restarted.enqueue(WaveDispatchRejected{999, "w3", 2, 503, "late duplicate"}, reply);
            CHECK_EQ(*restarted.engine()->time(), 116);
            CHECK(restarted.engine()->activeWave()->phase == WavePhase::Dispatching);
            restarted.enqueue(WaveStarted{117, "w3", 42, 1}, reply);
            restarted.enqueue(Resumed{132}, reply);
            CHECK(actions(restarted).empty());
            CHECK_EQ(*restarted.engine()->activeWave()->runId, 42U);
            restarted.enqueue(Resumed{200}, reply);
            CHECK(restarted.engine()->activeWave());
            restarted.enqueue(WaveExpired{200, "w3"}, reply);
            CHECK(!restarted.engine()->activeWave());
            CHECK_EQ(restarted.engine()->latestWave()->conclusion, "timeout");
        }
    }
    return checkResult();
}
int lostResume() {
    for (const auto failure : {InMemoryEventStore::Failure::BeforeStore, InMemoryEventStore::Failure::AfterStore}) {
        InMemoryEventStore store({std::get<EventRow>(encode(GraphChanged{0, pipeline()}, 1)),
                                  std::get<EventRow>(encode(tag("a"), 2)),
                                  std::get<EventRow>(encode(Tick{105}, 3))});
        store.failNextAppend(failure);
        unsigned ready = 0;
        EventQueue queue(
            store,
            [] {
                return Engine({5, 2000, 8, 0, "feed", {"o", "Packages", "main"}, 900});
            },
            [] {
                return Resumed{1005};
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
        CHECK_EQ(ready, 1U);
        REQUIRE(queue.engine());
        const std::vector<Command> expected{{"w3", 2, {"o", "Packages", "main"}, "feed"}};
        CHECK(actions(queue) == expected);
        CHECK(actions(queue) == expected); // Reading work never consumes its obligation.
        CHECK_EQ(queue.engine()->activeWave()->attempt, 2U);
        CHECK_EQ(store.rows().size(), failure == InMemoryEventStore::Failure::AfterStore ? 5U : 4U);
        const auto frozen = queue.engine()->activeWave()->plan;
        store.failNextAppend(failure);
        queue.enqueue(WaveDispatchAccepted{1006, "w3", 2}, [](const auto& result) {
            CHECK(std::holds_alternative<Unavailable>(result));
        });
        REQUIRE(queue.engine());
        CHECK(queue.engine()->activeWave()->plan == frozen);
        CHECK_EQ(actions(queue).size(), failure == InMemoryEventStore::Failure::AfterStore ? 0U : 1U);
        CHECK_EQ(queue.engine()->activeWave()->attempt, 2U);
    }
    return checkResult();
}
void shutdownReconnectAndResume() {
    for (const bool historicalShutdownResume : {false, true}) {
        const std::int64_t stoppedAt = 1794823672; // production stop: 2026-11-16 10:07:52 UTC
        InMemoryEventStore store({std::get<EventRow>(encode(GraphChanged{stoppedAt - 906, pipeline()}, 938)),
                                  std::get<EventRow>(encode(tag("a", stoppedAt - 905), 939)),
                                  std::get<EventRow>(encode(Tick{stoppedAt - 900}, 940))});
        const auto make = [] {
            return Engine({5, 2000, 8, 0, "feed", {"o", "Packages", "main"}, 900});
        };
        std::int64_t clock = stoppedAt - 899;
        bool live = true;
        const auto resumed = [&] {
            return Resumed{clock};
        };
        const auto warning = [](auto, const auto&) {
            CHECK(false);
        };
        const auto ready = [] {
        };
        const auto error = [](const auto&) {
            CHECK(false);
        };
        Plan frozen;
        {
            EventQueue old(store, make, resumed, warning, {}, [&] {
                return live;
            });
            old.start(ready, error);
            CHECK_EQ(store.rows().back().seq, 941U);
            if (old.engine() && old.engine()->activeWave()) {
                frozen = old.engine()->activeWave()->plan;
                CHECK_EQ(old.engine()->activeWave()->attempt, 1U);
            }
            store.defer(true);
            clock = stoppedAt;
            live = false;
            old.recover("receiver removed during SIGTERM shutdown");
            old.start(ready, error); // even a late app retry cannot reconnect/replay
            CHECK_EQ(store.pending(), 0U);
            CHECK_EQ(store.rows().back().seq, 941U); // no shutdown Resumed #942
            CHECK(!old.engine());
        }
        store.defer(false);
        if (historicalShutdownResume) {
            // Model history left by the old broken binary: #942 advanced attempt 2
            // during shutdown, but its one-shot dispatch never reached GitHub.
            store.append(
                Resumed{stoppedAt},
                [](auto seq) {
                    CHECK_EQ(seq, 942U);
                },
                [](const auto&) {
                    CHECK(false);
                });
        }
        clock = stoppedAt + 2;
        EventQueue fresh(store, make, resumed, warning);
        std::vector<Command> delivered;
        fresh.start(
            [&] {
                delivered = actions(fresh); // app drives state on the new process's Resumed
            },
            error);
        const std::vector<Command> expected{{"w940", 2, {"o", "Packages", "main"}, "feed"}};
        CHECK(delivered == expected);
        CHECK_EQ(store.rows().back().seq, historicalShutdownResume ? 943U : 942U);
        if (fresh.engine() && fresh.engine()->activeWave()) {
            CHECK(fresh.engine()->activeWave()->plan == frozen);
            CHECK_EQ(*fresh.engine()->nextDeadline(), stoppedAt + (historicalShutdownResume ? 900 : 902));
        } else {
            CHECK(false);
        }
    }
}
void uncertainExpiry() {
    for (const auto fault : {InMemoryEventStore::Failure::BeforeStore, InMemoryEventStore::Failure::AfterStore}) {
        InMemoryEventStore store;
        std::int64_t clock = 100;
        EventQueue queue(
            store,
            engine,
            [&] {
                return Resumed{clock};
            },
            [](auto, const auto&) {
                CHECK(false);
            });
        const auto reply = [](const auto&) {
        };
        queue.start(
            [] {
            },
            [](const auto&) {
                CHECK(false);
            });
        queue.enqueue(GraphChanged{100, pipeline()}, reply);
        queue.enqueue(tag("a"), reply);
        queue.enqueue(Tick{105}, reply);
        const auto wave = *queue.engine()->activeWave();
        queue.enqueue(WaveStarted{106, wave.id, 42, 1}, reply);
        const auto units = queue.engine()->units();
        clock = wave.deadline;
        store.failNextAppend(fault);
        queue.enqueue(WaveExpired{clock, wave.id}, [](const auto& result) {
            CHECK(std::holds_alternative<Unavailable>(result));
        });
        CHECK(queue.engine());
        CHECK_EQ(queue.engine()->activeWave() != nullptr, fault == InMemoryEventStore::Failure::BeforeStore);
        CHECK(queue.engine()->units() == units);
        if (queue.engine()->activeWave()) {
            queue.enqueue(WaveExpired{clock, wave.id}, reply);
        }
        CHECK(!queue.engine()->activeWave());
        unsigned expiries = 0;
        for (const auto& row : store.rows()) {
            expiries += row.eventType == "WaveExpired";
        }
        CHECK_EQ(expiries, 1U);
        CHECK(queue.engine()->units() == units);
    }
}

int main() {
    uncertainExpiry();
    shutdownReconnectAndResume();
    lostResume();
    restartDispatching();
    InMemoryEventStore store({std::get<EventRow>(encode(GraphChanged{0, pipeline()}, 1))});
    std::vector<Result> replies;
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
        [](auto, const auto&) {
            CHECK(false);
        },
        [&](auto, const Event&, const Engine& state) {
            if (reenter) {
                CHECK_EQ(state.commands().size(), 1U);
                reenter = false;
                active->enqueue(WaveDispatchAccepted{101, "w3", 1}, reply);
                CHECK(replies.empty()); // commit observer precedes original request reply
            }
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
    CHECK(actions(queue).empty());
    CHECK(replies.empty());
    CHECK(!queue.engine()->releases().at("a"));
    REQUIRE(store.completeOne());
    CHECK_EQ(actions(queue).size(), 1U);
    CHECK_EQ(replies.size(), 1U);
    CHECK(std::holds_alternative<Accepted>(replies.back()));
    CHECK_EQ(store.pending(), 1U);
    CHECK(queue.engine()->activeWave()->phase == WavePhase::Dispatching);
    queue.enqueue(tag("a", 102), reply); // duplicate waits behind pending dispatch result
    CHECK_EQ(replies.size(), 1U);
    REQUIRE(store.completeOne());
    CHECK_EQ(replies.size(), 3U);
    CHECK(std::holds_alternative<Accepted>(replies[1]));
    CHECK(std::holds_alternative<Ignored>(replies[2]));
    CHECK_EQ(store.rows().size(), rows + 2);
    CHECK_EQ(store.pending(), 0U);
    CHECK(queue.engine()->activeWave()->phase == WavePhase::Dispatching);

    // The committed row and engine both receive the normalized free text.
    store.defer(false);
    queue.enqueue(UnitBuildFailed{103, "w3", "w3~a~t", std::string("A\0B\xff", 4)}, reply);
    CHECK(std::holds_alternative<Accepted>(replies.back()));
    CHECK_EQ(*store.rows().back().reason, "AB\xef\xbf\xbd");
    CHECK_EQ(queue.engine()->units().at({"a", "t"}).failure->reason, "AB\xef\xbf\xbd");

    externalRecovery();
    stopping();
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
