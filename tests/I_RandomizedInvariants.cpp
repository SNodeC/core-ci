#include "Check.h"
#include "Fixture.h"
#include "core/Status.h"
#include "persistence/InMemoryEventStore.h"

#include <cstdlib>
#include <random>
#include <set>
using namespace coreci;
using namespace coreci::persistence;

int checkEvent(Engine& e, const Event& event, std::set<std::string>& allocations, InMemoryEventStore& store) {
    const auto previousUnits = e.units();
    const auto previousFeeds = e.feeds();
    const auto previousReleases = e.releases();
    const auto previousTime = e.time();
    const auto classification = e.classify(event);
    std::vector<Command> commands;
    const auto normalized = normalize(event);
    REQUIRE(std::holds_alternative<Event>(normalized));
    if (std::holds_alternative<Accept>(classification)) {
        store.append(
            std::get<Event>(normalized),
            [&] {
                commands = e.handle(std::get<Event>(normalized));
            },
            [](const auto&) {
                CHECK(false);
            });
    } else {
        commands = e.handle(std::get<Event>(normalized));
    }
    if (std::holds_alternative<Ignore>(classification)) {
        CHECK(commands.empty());
        CHECK(e.units() == previousUnits);
        CHECK(e.feeds() == previousFeeds);
        CHECK(e.releases() == previousReleases);
        CHECK(e.time() == previousTime);
    }
    for (const auto& command : commands) {
        std::visit(
            [&](const auto& c) {
                using C = std::decay_t<decltype(c)>;
                const Unit u{c.package, c.target};
                REQUIRE(e.units().at(u).inflight);
                const auto& current = *e.units().at(u).inflight;
                CHECK_EQ(current.buildId, c.buildId);
                CHECK_EQ(current.attempt, c.attempt);
                if constexpr (std::is_same_v<C, DispatchBuild>) {
                    if (allocations.insert(c.buildId).second) {
                        // I1 applies at allocation, not retry/resume of an existing buildId.
                        for (const auto& p : e.pipeline().prerequisites(c.package)) {
                            const auto& prerequisite = e.units().at({p, c.target});
                            CHECK(!prerequisite.inflight);
                            REQUIRE(prerequisite.published);
                            CHECK(prerequisite.published->version == e.releases().at(p)->version);
                            // Check all ancestors from their published facts, independently of Engine::derive().
                            std::vector<Unit> ancestors{{p, c.target}};
                            while (!ancestors.empty()) {
                                const auto ancestor = ancestors.back();
                                ancestors.pop_back();
                                const auto& published = e.units().at(ancestor).published;
                                REQUIRE(published);
                                CHECK(!e.units().at(ancestor).inflight);
                                CHECK(published->version == e.releases().at(ancestor.package)->version);
                                for (const auto& parent : e.pipeline().prerequisites(ancestor.package)) {
                                    const Unit parentUnit{parent, ancestor.target};
                                    const auto& parentPublished = e.units().at(parentUnit).published;
                                    REQUIRE(parentPublished);
                                    CHECK((published->inputs.at(parent) == Input{parentPublished->version, parentPublished->revision}));
                                    ancestors.push_back(parentUnit);
                                }
                            }
                            CHECK((c.inputs.at(p) == Input{prerequisite.published->version, prerequisite.published->revision}));
                        }
                        const auto& revisions = previousUnits.at(u).lastRevision;
                        const auto it = revisions.find(c.version);
                        CHECK_EQ(c.revision, it == revisions.end() ? 1 : it->second + 1);
                    } else {
                        REQUIRE(previousUnits.at(u).inflight);
                        CHECK_EQ(previousUnits.at(u).inflight->buildId, c.buildId);
                    }
                }
                return checkResult();
            },
            command);
    }
    // I2: all active assignments are distinct and agree with emitted commands.
    std::set<std::string> inflights;
    for (const auto& [u, s] : e.units()) {
        if (s.inflight) {
            CHECK(inflights.insert(s.inflight->buildId).second);
            CHECK_EQ(s.inflight->revision, s.lastRevision.at(s.inflight->version));
        }
    }
    // I3: slots/queues exactly describe publication phases; no assignment is duplicated.
    for (const auto& [feedId, feed] : e.feeds()) {
        unsigned active = 0;
        std::set<Unit> queued;
        for (const auto& u : feed.queue) {
            CHECK(queued.insert(u).second);
            REQUIRE(e.units().at(u).inflight);
            CHECK(e.units().at(u).inflight->phase == Phase::AwaitingPublish);
            CHECK_EQ(e.pipeline().targets().at(u.target).feed, feedId);
        }
        for (const auto& [u, s] : e.units()) {
            if (e.pipeline().targets().at(u.target).feed == feedId && s.inflight) {
                if (s.inflight->phase == Phase::PublishDispatching || s.inflight->phase == Phase::Publishing) {
                    ++active;
                    CHECK(feed.active == u);
                }
                if (s.inflight->phase == Phase::AwaitingPublish) {
                    CHECK(queued.contains(u));
                }
            }
        }
        CHECK(active <= 1);
        CHECK_EQ(active, feed.active ? 1U : 0U);
    }
    return checkResult();
}
int verifyReplay(const Engine& live, InMemoryEventStore& store, Options options) {
    Engine replay(options);
    bool done = false;
    store.loadAll(
        [&](std::uint64_t seq, const Event& event) {
            const auto row = encode(event, seq);
            REQUIRE(std::holds_alternative<EventRow>(row));
            const auto restored = decode(std::get<EventRow>(row));
            REQUIRE(std::holds_alternative<Event>(restored));
            replay.handle(std::get<Event>(restored));
            return checkResult();
        },
        [&] {
            done = true;
        },
        [](const auto&) {
            CHECK(false);
        });
    CHECK(done);
    CHECK_EQ(statusText(replay), statusText(live));
    CHECK(replay.units() == live.units());
    CHECK(replay.feeds() == live.feeds());
    CHECK(replay.releases() == live.releases());
    CHECK(replay.time() == live.time());
    CHECK(replay.nextDeadline() == live.nextDeadline());
    return checkResult();
}
int main() {
    const auto seed = std::getenv("CORECI_TEST_SEED") ? std::stoull(std::getenv("CORECI_TEST_SEED")) : 0xC0DEC1ULL;
    const auto runs = std::getenv("CORECI_TEST_RUNS") ? std::stoull(std::getenv("CORECI_TEST_RUNS")) : 10000ULL;
    std::cout << "seed=" << seed << " runs=" << runs << '\n';
    std::mt19937_64 random(seed);
    for (std::uint64_t run = 0; run < runs && checkFailures == 0; ++run) {
        const Options options{100, 50, 8, run % 2 == 0 ? std::optional<std::size_t>{2} : std::nullopt};
        Engine e(options);
        InMemoryEventStore store;
        std::set<std::string> allocations;
        std::map<std::string, unsigned> versions;
        std::int64_t now = 100;
        checkEvent(e, GraphChanged{now, testPipeline()}, allocations, store);
        for (const auto& p : {"b", "c", "d", "e", "a"}) {
            versions[p] = 0;
            checkEvent(e, TagObserved{now++, p, "v1.0.0", std::string(40, 'a'), Origin::Webhook}, allocations, store);
        }
        for (unsigned step = 0; step < 60 && checkFailures == 0; ++step) {
            const auto choice = random() % 10;
            std::vector<Unit> units;
            for (const auto& [u, s] : e.units()) {
                units.push_back(u);
            }
            const auto u = units.at(random() % units.size());
            const auto state = e.units().at(u);
            Event event = Resumed{now};
            if (choice == 0) {
                Pipeline::Builder changed;
                for (const auto& [id, feed] : e.pipeline().feeds()) {
                    changed.feed(feed);
                }
                for (const auto& [id, target] : e.pipeline().targets()) {
                    changed.target(target);
                }
                for (const auto& [name, definition] : e.pipeline().packages()) {
                    auto pkg = definition;
                    if (name == "c") {
                        pkg.prerequisites =
                            pkg.prerequisites.size() == 1 ? std::vector<std::string>{"a", "b"} : std::vector<std::string>{"a"};
                    }
                    changed.package(pkg);
                }
                event = GraphChanged{now, changed.build()};
            } else if (choice < 2) {
                const auto n = ++versions[u.package];
                event = TagObserved{now, u.package, "v1.0." + std::to_string(n), std::string(40, 'a'), Origin::Reconcile};
            } else if (choice == 2) {
                event = Tick{e.nextDeadline().value_or(now)};
            } else if (choice == 3) {
                event = BuildSucceeded{now, "unknown", 999, "artifact"};
            } else if (state.inflight) {
                const auto& f = *state.inflight;
                const auto runId = f.runId.value_or(101);
                if (f.phase == Phase::Dispatching || f.phase == Phase::PublishDispatching) {
                    const auto kind = f.phase == Phase::Dispatching ? Kind::Build : Kind::Publish;
                    event = choice == 4 ? Event{DispatchRejected{now, f.buildId, kind, f.attempt, 503, "busy"}}
                                        : Event{DispatchAccepted{now, f.buildId, kind, f.attempt}};
                } else if (f.phase == Phase::Building) {
                    if (choice == 4) {
                        event = BuildFailed{now, f.buildId, runId, "failure"};
                    } else if (choice == 5) {
                        event = BuildStarted{now, f.buildId, runId, 1};
                    } else {
                        event = BuildSucceeded{now, f.buildId, runId, "artifact"};
                    }
                } else if (f.phase == Phase::Publishing) {
                    event = choice == 4 ? Event{PublishFailed{now, f.buildId, 201, "failure"}}
                                        : Event{PublishSucceeded{now, f.buildId, 201, std::string(40, 'c')}};
                }
            } else if (state.failure) {
                event = RetryRequested{now, u.package, u.target};
            }
            checkEvent(e, event, allocations, store);
            now = e.time().value_or(now) + 1;
        }
        verifyReplay(e, store, options); // I5 in changing/inflight states, before successful drain.
        // I4: cease tag changes/failures, clear blocked failures and finish all work.
        bool quiescent = false;
        for (unsigned step = 0; step < 400 && !quiescent && checkFailures == 0; ++step) {
            std::optional<Event> event;
            for (const auto& [u, state] : e.units()) {
                if (!event && state.inflight) {
                    const auto& f = *state.inflight;
                    if (f.retryAt) {
                        event = Tick{*f.retryAt};
                    } else if (f.phase == Phase::Dispatching || f.phase == Phase::Building) {
                        event = BuildSucceeded{now, f.buildId, f.runId.value_or(101), "artifact"};
                    } else if (f.phase == Phase::PublishDispatching || f.phase == Phase::Publishing) {
                        event = PublishSucceeded{now, f.buildId, 201, std::string(40, 'c')};
                    }
                } else if (!event && state.failure && !state.inflight) {
                    event = RetryRequested{now, u.package, u.target};
                }
            }
            if (event) {
                checkEvent(e, *event, allocations, store);
                now = e.time().value_or(now) + 1;
            } else {
                quiescent = true;
            }
        }
        CHECK(quiescent);
        for (const auto& [u, d] : e.derive()) {
            CHECK(d.settled);
            const auto& state = e.units().at(u);
            REQUIRE(state.published);
            CHECK(!state.inflight);
            CHECK_EQ(state.published->version.text(), "1.0." + std::to_string(versions.at(u.package)));
            CHECK_EQ(state.published->inputs.size(), e.pipeline().prerequisites(u.package).size());
            for (const auto& prerequisite : e.pipeline().prerequisites(u.package)) {
                const auto& input = e.units().at({prerequisite, u.target}).published;
                REQUIRE(input);
                CHECK((state.published->inputs.at(prerequisite) == Input{input->version, input->revision}));
            }
            CHECK_EQ(statuses(e).at(u), "Settled");
        }
        verifyReplay(e, store, options); // I5 again at quiescence.
        if (checkFailures != 0) {
            std::cerr << "failed run=" << run << " seed=" << seed << '\n';
        }
    }
    return checkResult();
}
