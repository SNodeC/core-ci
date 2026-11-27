#include "Check.h"
#include "core/Scheduler.h"
#include "persistence/StateSession.h"
#include "persistence/StateSql.h"

#include <algorithm>
#include <set>

using namespace coreci;

namespace {
    const std::string head(40, 'a');
    const std::string idp = "example/Provider", consumer = "example/Consumer";
    const Json target{{"id", "native"}, {"family", "deb"}, {"runner", "ubuntu-24.04"}, {"arch", "amd64"}};

    Json package(const std::string& name, Json dependencies = Json::array()) {
        return {{"package", name},
                {"releases", ">=1,<5"},
                {"targets", Json::array({"native"})},
                {"depends_on", dependencies},
                {"tree", head},
                {"ci_commit", head}};
    }

    Json release(const std::string& repo, const std::string& tag) {
        return {{"repo", repo}, {"tag", tag}, {"commit", head}};
    }

    Json ready(const Scheduler& scheduler, Json declared, Json tags) {
        auto state = initialState();
        scheduler.declarations(state, {{"native", target}}, declared, tags);
        scheduler.feed(state, head, {{"refs/snapshots/feed/pre-x", head}}, Json::array(), 1);
        return state;
    }

    void turn(const Scheduler& scheduler, Json& state, Time at) {
        scheduler.turn(state, at);
        scheduler.turn(state, at);
    }

    Json owner(unsigned attempt, unsigned run, unsigned runAttempt = 1) {
        return {{"attempt", attempt}, {"run_id", run}, {"run_attempt", runAttempt}};
    }

    void complete(
        const Scheduler& scheduler, Json& state, const std::string& request, unsigned attempt, unsigned run, Time at, bool reportFirst) {
        const auto key = attemptKey(request, attempt);
        const auto who = owner(attempt, run);
        CHECK_EQ(scheduler.claim(state, false, request, who, at).code, 200);
        Json finished = who;
        finished.update(Json{{"status", "finished"},
                             {"outcome", "built"},
                             {"artifact_id", 78},
                             {"artifact_digest", std::string(64, 'b')},
                             {"inventory", Json::array({Json::object()})}});
        const Json ended{{"id", run}, {"status", "completed"}, {"conclusion", "success"}};
        if (reportFirst) {
            CHECK_EQ(scheduler.report(state, false, request, finished, at).code, 200);
            scheduler.observed(state, key, ended, at);
        } else {
            scheduler.observed(state, key, ended, at);
            CHECK_EQ(state.at("items").at(state.at("attempts").at(key).at("item").get<std::string>()).at("state"), "processing");
            CHECK_EQ(scheduler.report(state, false, request, finished, at).code, 200);
        }
    }

    void turns() {
        Options config;
        config.slots = 3;
        const Scheduler scheduler(config);
        auto state = ready(scheduler,
                           {{idp, package("producer")}, {consumer, package("consumer")}},
                           Json::array({release(idp, "v2.0.0"), release(idp, "v1.0.0"), release(consumer, "v1.0.0")}));
        CHECK_EQ(state.at("items").size(), 2U);
        scheduler.turn(state, 10);
        CHECK_EQ(state.at("pass"), 1);
        scheduler.admit(state, idp, "v1.5.0", head);
        scheduler.turn(state, 10);
        CHECK_EQ(state.at("attempts").size(), 2U);
        CHECK_EQ(state.at("items").at("producer/1.5.0/native").at("state"), "enrolled");
        turn(scheduler, state, 11);
        CHECK_EQ(state.at("attempts").size(), 3U);
        CHECK_EQ(state.at("items").at("producer/1.5.0/native").at("state"), "processing");
        CHECK_EQ(scheduler.admit(state, idp, "v2.0.0", std::string(40, 'b')).code, 409);
    }

    Json output(const std::string& name, const std::string& release, unsigned rev, Time date, Json dependencies = Json::array()) {
        return {{"package", name},
                {"version", release},
                {"revision", rev},
                {"target", target},
                {"published_at", date},
                {"identity", sha256(name + release + std::to_string(rev))},
                {"repo", idp},
                {"commit", head},
                {"ci_commit", head},
                {"bindings", Json::array()},
                {"closure", Json::array()},
                {"inventory", Json::array({Json{{"name", name}, {"version", release}, {"runtime_dependencies", dependencies}}})}};
    }

    void binding() {
        const Scheduler scheduler(Options{});
        auto state = ready(scheduler,
                           {{idp, package("producer")},
                            {consumer, package("consumer", Json::array({Json{{"package", "producer"}, {"version", ">=1,<3"}}}))}},
                           Json::array({release(idp, "v2.0.0"), release(consumer, "v1.0.0")}));
        state["entries"] = Json::array({output("producer", "1.0.0", 3, 5)});
        turn(scheduler, state, 10);
        CHECK_EQ(state.at("items").at("consumer/1.0.0/native").at("condition"), "waiting");
        state["items"]["producer/2.0.0/native"]["state"] = "failed";
        turn(scheduler, state, 11);
        const Json& selected = state.at("items").at("consumer/1.0.0/native");
        CHECK_EQ(selected.at("state"), "processing");
        if (selected.contains("request")) {
            const auto record = state.at("attempts").at(attemptKey(selected.at("request").get<std::string>(), 1));
            const auto input = Json::parse(record.at("document").get<std::string>()).at("inputs");
            CHECK_EQ(input.at("closure").at(0).at("revision"), 3);
        }
        auto cyclic = ready(scheduler,
                            {{idp, package("producer", Json::array({Json{{"package", "consumer"}, {"version", ">=1"}}}))},
                             {consumer, package("consumer", Json::array({Json{{"package", "producer"}, {"version", ">=1"}}}))}},
                            Json::array({release(idp, "v1.0.0"), release(consumer, "v1.0.0")}));
        turn(scheduler, cyclic, 10);
        CHECK_EQ(cyclic.at("attempts").size(), 0U);
        CHECK_EQ(cyclic.at("items").at("producer/1.0.0/native").at("reason"), "cycle");
    }

    void attempts() {
        Options config;
        config.slots = 1;
        config.queue = 5;
        config.grace = 2;
        const Scheduler scheduler(config);
        auto state = ready(scheduler, {{idp, package("producer")}}, Json::array({release(idp, "v1.0.0")}));
        turn(scheduler, state, 10);
        const auto request = state.at("items").begin()->at("request").get<std::string>();
        const auto key = attemptKey(request, 1);
        const auto frozen = Json::parse(state.at("attempts").at(key).at("document").get<std::string>()).at("inputs").dump();
        scheduler.observed(state, key, {{"not_found", true}}, 15);
        CHECK_EQ(state.at("attempts").at(key).at("run_state"), "superseded");
        CHECK_EQ(state.at("items").begin()->at("attempt"), 2);
        const auto next = attemptKey(request, 2);
        CHECK_EQ(Json::parse(state.at("attempts").at(next).at("document").get<std::string>()).at("inputs").dump(), frozen);
        CHECK_EQ(scheduler.claim(state, false, request, owner(1, 7), 16).code, 409);
        CHECK_EQ(scheduler.claim(state, false, request, owner(2, 8, 2), 16).code, 409);
        CHECK_EQ(scheduler.claim(state, false, request, owner(2, 8), 16).code, 200);
        const auto claim = scheduler.claim(state, false, request, owner(2, 8), 18);
        CHECK_EQ(Json::parse(claim.text).at("document_sha256"), sha256(state.at("attempts").at(next).at("document").get<std::string>()));
        scheduler.observed(state, key, {{"id", 7}, {"status", "completed"}, {"conclusion", "success"}}, 18);
        CHECK_EQ(state.at("attempts").at(key).at("run_state"), "superseded");
        scheduler.observed(state, next, {{"id", 8}, {"status", "completed"}, {"conclusion", "cancelled"}}, 20);
        CHECK_EQ(state.at("items").begin()->at("state"), "processing");
        scheduler.turn(state, 22);
        CHECK_EQ(state.at("items").begin()->at("reason"), "conclusion cancelled, no report");
        CHECK_EQ(scheduler.action(state, "retry", {{"item", "producer/1.0.0/native"}}).code, 200);
        turn(scheduler, state, 23);
        scheduler.turn(state, 24);
        const auto retry = attemptKey(request, 3);
        CHECK_EQ(Json::parse(state.at("attempts").at(retry).at("document").get<std::string>()).at("inputs").dump(), frozen);
    }

    void publications() {
        Options config;
        config.slots = 2;
        config.batch = 10;
        config.branch = "feed-staging";
        config.fault = "token1";
        const Scheduler scheduler(config);
        auto state = ready(scheduler,
                           {{idp, package("producer")}, {consumer, package("consumer")}},
                           Json::array({release(idp, "v1.0.0"), release(consumer, "v1.0.0")}));
        turn(scheduler, state, 10);
        const auto first = state.at("items").at("producer/1.0.0/native").at("request").get<std::string>();
        const auto last = state.at("items").at("consumer/1.0.0/native").at("request").get<std::string>();
        complete(scheduler, state, first, 1, 27, 20, true);
        turn(scheduler, state, 20);
        const auto found = std::ranges::find_if(state.at("attempts"), [](const Json& attempt) {
            return attempt.at("publication") == true;
        });
        if (found == state.at("attempts").end()) {
            throw std::runtime_error("publication reservation missing");
        }
        CHECK_EQ(found->at("dispatch"), "batching");
        const auto id = found->at("id").get<std::string>();
        turn(scheduler, state, 30);
        const auto key = attemptKey(id, 1);
        const auto document = Json::parse(state.at("attempts").at(key).at("document").get<std::string>());
        CHECK_EQ(document.at("fault"), "after_push");
        CHECK_EQ(document.at("items").size(), 1U);
        complete(scheduler, state, last, 1, 28, 31, false);
        turn(scheduler, state, 31);
        CHECK_EQ(state.at("attempts").size(), 3U);
        CHECK_EQ(scheduler.claim(state, true, id, owner(1, 29), 32).code, 200);
        scheduler.observed(state, key, {{"id", 29}, {"status", "completed"}, {"conclusion", "failure"}}, 33);
        turn(scheduler, state, 33);
        CHECK_EQ(state.at("attempts").size(), 3U);
        const std::string ref = "refs/snapshots/feed-staging/" + id;
        Json receipt{{"publication", id}, {"attempt", 1}, {"request", first}, {"outcome", "published"}};
        scheduler.feed(state,
                       std::string(40, 'b'),
                       {{ref, std::string(40, 'b')}},
                       {{"entries", Json::array({output("producer", "1.0.0", 1, 33)})}, {"receipts", Json::array({receipt})}},
                       33);
        turn(scheduler, state, 33);
        CHECK_EQ(state.at("attempts").size(), 4U);
        CHECK_EQ(state.at("rollback").at(0), ref);
        CHECK_EQ(state.at("acks").size(), 1U);
        CHECK_EQ(state.at("faults").size(), 1U);

        auto idle = ready(scheduler, Json::object(), Json::array());
        idle["retirement_at"] = 90;
        turn(scheduler, idle, 89);
        CHECK(idle.at("attempts").empty());
        turn(scheduler, idle, 90);
        turn(scheduler, idle, 100);
        const auto retirement = Json::parse(idle.at("attempts").begin()->at("document").get<std::string>());
        CHECK_EQ(retirement.at("retire_legacy_apt"), true);
        CHECK(retirement.at("items").empty());
    }

    void retention() {
        const auto entries = Json::array({output("needed", "1.0.0", 1, 1),
                                          output("needed", "2.0.0", 1, 1),
                                          output("needed", "3.0.0", 1, 1),
                                          output("needed", "4.0.0", 1, 1),
                                          output("other", "1.0.0", 1, 1),
                                          output("other", "1.0.0", 2, 1),
                                          output("other", "1.0.0", 3, 1),
                                          output("consumer", "1.0.0", 3, 1, Json::array({"needed (= 1.0.0)"}))});
        const auto selected = retirements(entries, 8 * 86400);
        CHECK_EQ(selected, Json::array({Json::array({"needed", "2.0.0", 1, "native"}), Json::array({"other", "1.0.0", 1, "native"})}));
    }

    void expiry() {
        Options options;
        options.batch = 0;
        const Scheduler scheduler(options);
        for (const bool change : {false, true}) {
            const auto tags = Json::array({release(idp, "v1.0.0")});
            auto decl = package("producer");
            auto state = ready(scheduler, {{idp, decl}}, tags);
            turn(scheduler, state, 10);
            const auto first = state["items"].begin()->at("request").get<std::string>();
            const auto frozen = Json::parse(state["attempts"][attemptKey(first, 1)].at("document").get<std::string>()).at("inputs");
            complete(scheduler, state, first, 1, 51, 12, false);
            turn(scheduler, state, 12);
            const auto publication = std::ranges::find_if(state["attempts"],
                                                          [](const auto& record) {
                                                              return record.at("publication") == true;
                                                          })
                                         ->at("id")
                                         .get<std::string>();
            if (change) {
                decl["tree"] = std::string(40, 'c');
                scheduler.declarations(state, {{"native", target}}, {{idp, decl}}, tags);
                turn(scheduler, state, 13);
            }
            state = Json::parse(state.dump());
            CHECK_EQ(scheduler.claim(state, true, publication, owner(1, 52), 14).code, 200);
            scheduler.observed(state, attemptKey(publication, 1), {{"id", 52}, {"status", "completed"}, {"conclusion", "success"}}, 15);
            const Json receipt{
                {"publication", publication}, {"attempt", 1}, {"request", first}, {"outcome", "failed"}, {"failure", "artifact-expired"}};
            const std::string latest(40, 'd');
            scheduler.feed(state,
                           latest,
                           {{"refs/snapshots/feed/" + publication, latest}, {"refs/snapshots/feed/pre-x", head}},
                           {{"entries", Json::array()}, {"receipts", Json::array({receipt})}},
                           15);
            turn(scheduler, state, 15);
            const auto& item = state["items"].begin().value();
            CHECK_EQ(item.at("revision"), change ? 2 : 1);
            CHECK_EQ(item.at("attempt"), change ? 1 : 2);
            const auto key = attemptKey(item.at("request").get<std::string>(), item.at("attempt").get<unsigned>());
            if (!change) {
                CHECK_EQ(Json::parse(state.at("attempts").at(key).at("document").get<std::string>()).at("inputs"), frozen);
                CHECK_EQ(scheduler.claim(state, false, first, owner(1, 51), 16).code, 409);
            }
        }
    }

    struct Store : persistence::StateStore {
        Json persisted = initialState();
        bool loseCommitReply = false;

        void open(std::function<void()> ready, std::function<void(const persistence::StoreError&)>) override {
            ready();
        }
        void load(std::function<void(Json)> done, std::function<void(const persistence::StoreError&)>) override {
            done(persisted);
        }
        void commit(const Json&,
                    const Json& after,
                    const std::string&,
                    std::function<void()> done,
                    std::function<void(const persistence::StoreError&)> error) override {
            persisted = after;
            if (loseCommitReply) {
                loseCommitReply = false;
                error({persistence::StoreError::Kind::Unavailable, "lost commit response"});
            } else {
                done();
            }
        }
    };

    void persistenceTest() {
        auto initial = initialState(), after = initial;
        after["pass"] = 1;
        after["revisions"]["a/1.0.0/native"] = 3;
        CHECK_EQ(persistence::stateSql(initial, after, "ready").size(), 3U);
        CHECK(persistence::stateSql(after, after, "duplicate").empty());
        persistence::readRecord(initial, "70617373", "", "31");
        CHECK_EQ(initial.at("pass"), 1);
        Store store;
        persistence::StateSession session(
            store,
            {},
            [] {
            },
            [] {
                return true;
            });
        const auto failed = [](const auto& fault) {
            CHECK_EQ(fault.message, "lost commit response");
        };
        session.start(
            [] {
            },
            failed);
        store.loseCommitReply = true;
        session.enqueue(
            "pause",
            [](auto& facts) {
                facts["paused"] = true;
                return Result{};
            },
            [](const auto& reply) {
                CHECK_EQ(reply.code, 503);
            });
        CHECK(session.state() == nullptr);
        session.start(
            [] {
            },
            failed);
        CHECK_EQ(session.state()->at("paused"), true);
    }
} // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "turns") {
        turns();
    } else if (mode == "binding") {
        binding();
    } else if (mode == "attempts") {
        attempts();
    } else if (mode == "publications") {
        publications();
    } else if (mode == "retention") {
        retention();
    } else if (mode == "expiry") {
        expiry();
    } else if (mode == "persistence") {
        persistenceTest();
    } else {
        CHECK(false);
    }
    return checkResult();
}
