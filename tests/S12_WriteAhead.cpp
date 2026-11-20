#include "Fixture.h"
#include "InMemoryStateStore.h"
#include "persistence/StateSession.h"

using namespace coreci;
using namespace coreci::persistence;
using namespace coreci::test;

namespace {
    class UnavailableStore : public InMemoryStateStore {
    public:
        explicit UnavailableStore(bool opening)
            : opening(opening) {
        }
        void open(std::function<void()> ready, std::function<void(const StoreError&)> error) override {
            if (opening) {
                error({StoreError::Kind::Unavailable, "open unavailable"});
            } else {
                ready();
            }
        }
        void load(std::function<void(StoredState)>, std::function<void(const StoreError&)> error) override {
            error({StoreError::Kind::Unavailable, "load unavailable"});
        }

    private:
        bool opening;
    };
    void lifecycle() {
        for (const bool opening : {false, true}) {
            UnavailableStore store(opening);
            unsigned failed = 0;
            StateSession session(
                store,
                {},
                [](auto) {
                    CHECK(false);
                },
                [] {
                    return true;
                });
            session.start(
                [] {
                    CHECK(false);
                },
                [&](const auto& error) {
                    CHECK(error.kind == StoreError::Kind::Unavailable);
                    CHECK_EQ(error.message, opening ? "open unavailable" : "load unavailable");
                    ++failed;
                });
            CHECK_EQ(failed, 1U);
            CHECK(!session.engine());
            session.enqueue(Tick{100}, [](const auto& reply) {
                CHECK(std::holds_alternative<Unavailable>(reply));
            });
            CHECK(store.log.empty());
        }
        for (unsigned phase = 0; phase < 3; ++phase) {
            InMemoryStateStore store;
            store.deferred = true;
            bool live = true;
            unsigned ready = 0;
            StateSession session(
                store,
                {},
                [](auto) {
                    CHECK(false);
                },
                [&] {
                    return live;
                });
            session.start(
                [&] {
                    ++ready;
                },
                [](const auto&) {
                    CHECK(false);
                });
            for (unsigned completed = 0; completed < phase; ++completed) {
                CHECK(store.complete());
            }
            live = false;
            session.recover("late connection loss during stop");
            session.start(
                [] {
                    CHECK(false);
                },
                [](const auto&) {
                    CHECK(false);
                });
            while (store.complete()) {
            }
            CHECK(!session.engine());
            CHECK_EQ(ready, phase == 2 ? 1U : 0U);
            CHECK(store.log.empty());
        }
    }
    void reentrant() {
        InMemoryStateStore store;
        StateSession* active = nullptr;
        StateSession session(
            store,
            {0, 100, 3, 0},
            [&](auto) {
                if (active && !active->engine()->commands().empty()) {
                    const auto command = active->engine()->commands()[0];
                    active->enqueue(WaveDispatchAccepted{101, command.id, command.attempt}, [](const auto& reply) {
                        CHECK(std::holds_alternative<Accepted>(reply));
                    });
                }
            },
            [] {
                return true;
            });
        session.start(
            [] {
            },
            [](const auto&) {
                CHECK(false);
            });
        session.enqueue(GraphChanged{100, testPipeline()}, [](const auto&) {
        });
        const auto before = store.state;
        session.enqueue(TagObserved{999, "a", "v1.0.0", "bad", Origin::Webhook}, [](const auto& reply) {
            CHECK(std::holds_alternative<Invalid>(reply));
        });
        CHECK(store.state == before);
        store.deferred = true;
        active = &session;
        session.enqueue(TagObserved{100, "a", "v1.0.0", hash(), Origin::Webhook}, [](const auto& reply) {
            CHECK(std::holds_alternative<Accepted>(reply));
        });
        CHECK(session.engine()->commands().empty());
        CHECK(store.complete());
        CHECK_EQ(session.engine()->commands().size(), 1U);
        session.enqueue(TagObserved{102, "a", "v1.0.0", hash(), Origin::Webhook}, [](const auto& reply) {
            CHECK(std::holds_alternative<Ignored>(reply));
        });
        CHECK(store.complete());
        CHECK(session.engine()->commands().empty());
        CHECK_EQ(store.log.size(), 3U);
        store.deferred = false;
        const auto wave = *session.engine()->latestWave();
        session.enqueue(WaveStarted{102, wave.id, 42, 1}, [](const auto&) {
        });
        session.enqueue(UnitBuildFailed{103, wave.id, wave.plan.at("t1")[0].buildId, std::string("A\0B\xff", 4)}, [](const auto&) {
        });
        CHECK_EQ(session.engine()->units().at({"a", "t1"}).failure->reason, "AB\xef\xbf\xbd");
        CHECK_EQ(store.state.unitStates.at({"a", "t1"}).failure->reason, "AB\xef\xbf\xbd");
    }
} // namespace
int main() {
    lifecycle();
    reentrant();
    for (const auto failure : {InMemoryStateStore::Failure::BeforeCommit, InMemoryStateStore::Failure::AfterCommit}) {
        InMemoryStateStore store;
        unsigned ready = 0, failures = 0, changes = 0;
        bool operational = true;
        StateSession session(
            store,
            {0, 100, 3, 0},
            [&](auto) {
                ++changes;
            },
            [&] {
                return operational;
            });
        const auto start = [&] {
            session.start(
                [&] {
                    ++ready;
                },
                [&](const auto& error) {
                    CHECK(error.kind == StoreError::Kind::Unavailable);
                    ++failures;
                });
        };
        start();
        CHECK(session.engine());
        CHECK_EQ(ready, 1U);
        session.enqueue(GraphChanged{100, testPipeline()}, [](const auto& result) {
            CHECK(std::holds_alternative<Accepted>(result));
        });
        CHECK_EQ(store.log.size(), 1U);
        store.deferred = true;
        store.failure = failure;
        std::vector<Result> replies;
        const auto reply = [&](Result result) {
            replies.push_back(std::move(result));
        };
        session.enqueue(TagObserved{101, "a", "v1.0.0", hash(), Origin::Webhook}, reply);
        session.enqueue(TagObserved{102, "z", "v1.0.0", hash(), Origin::Webhook}, reply);
        CHECK(!session.engine()->releases().at("a"));
        CHECK(session.engine()->commands().empty());
        CHECK(replies.empty());
        CHECK(store.complete());
        CHECK(!session.engine());
        CHECK_EQ(failures, 1U);
        CHECK_EQ(replies.size(), 2U);
        CHECK(std::holds_alternative<Unavailable>(replies[0]));
        CHECK(std::holds_alternative<Unavailable>(replies[1]));
        const auto committed = store.state;
        start();
        CHECK(!session.engine());
        CHECK(store.complete());
        CHECK(!session.engine());
        CHECK(store.complete());
        CHECK(session.engine());
        CHECK_EQ(ready, 2U);
        CHECK(session.engine()->stored() == committed);
        const bool stored = failure == InMemoryStateStore::Failure::AfterCommit;
        CHECK_EQ(session.engine()->commands().size(), stored ? 1U : 0U);
        CHECK_EQ(store.log.size(), stored ? 2U : 1U);
        CHECK(!session.engine()->releases().at("z"));
        if (stored) {
            const auto command = session.engine()->commands()[0];
            CHECK(session.engine()->commands()[0] == command);
            StateSession restarted(
                store,
                {0, 100, 3, 0},
                [](auto) {
                },
                [] {
                    return true;
                });
            store.deferred = false;
            restarted.start(
                [] {
                },
                [](const auto&) {
                    CHECK(false);
                });
            CHECK(restarted.engine());
            CHECK_EQ(restarted.engine()->commands()[0], command);
            CHECK_EQ(restarted.engine()->units().at({"a", "t1"}).lastRevision.at(*Version::parse("1.0.0")), 1U);
        }
        store.deferred = true;
        session.enqueue(TagObserved{103, "z", "v1.0.0", hash(), Origin::Webhook}, reply);
        const auto beforeStop = changes;
        operational = false;
        CHECK(!session.engine());
        CHECK(store.complete());
        CHECK_EQ(changes, beforeStop);
        CHECK_EQ(replies.size(), 2U);
        CHECK(!session.engine());
        session.recover("late lost connection");
        CHECK_EQ(failures, 1U);
    }
    History h({0, 100, 3, 0});
    h.load();
    const auto wave = h.wave();
    h.apply(WaveStarted{101, wave.id, 42, 1});
    for (const auto& entry : wave.plan.at("t1")) {
        h.apply(UnitBuildSucceeded{102, wave.id, entry.buildId});
    }
    h.apply(TargetFinished{103, wave.id, "t1", "artifact"});
    InMemoryStateStore store;
    store.state = h.engine.stored();
    StateSession publicationRestart(
        store,
        h.options,
        [](auto) {
        },
        [] {
            return true;
        });
    publicationRestart.start(
        [] {
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK(publicationRestart.engine());
    CHECK_EQ(publicationRestart.engine()->commands().size(), 1U);
    CHECK(publicationRestart.engine()->commands()[0].publication);
    CHECK(publicationRestart.engine()->stored() == h.engine.stored());
    return checkResult();
}
