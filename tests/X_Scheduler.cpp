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

        config.slots = 2;
        const Scheduler fair(config);
        auto queues = ready(fair, {{idp, package("producer")}, {consumer, package("consumer")}}, Json::array());
        fair.admit(queues, idp, "v1.0.0", head);
        fair.admit(queues, consumer, "v1.0.0", head);
        fair.admit(queues, idp, "v1.1.0", head);
        turn(fair, queues, 12);
        CHECK_EQ(queues.at("items").at("producer/1.0.0/native").at("state"), "processing");
        CHECK_EQ(queues.at("items").at("consumer/1.0.0/native").at("state"), "processing");
        CHECK_EQ(queues.at("items").at("producer/1.1.0/native").at("state"), "enrolled");
    }

    void registryArrivals() {
        Options options;
        options.slots = 4;
        const Scheduler scheduler(options);
        const std::string extra = "example/Extra", other = "example/Other";
        const std::string original = "producer/1.0.0/native", originalNext = "producer/1.1.0/native";
        const std::string extraNewest = "extra/1.10.0/native", otherNewest = "other/2.0.0/native";
        const std::string historical = "extra/1.9.0/native";
        Json declarations{{idp, package("producer")}};
        auto state = ready(scheduler, declarations, Json::array({release(idp, "v1.0.0")}));
        scheduler.turn(state, 10);
        CHECK_EQ(state.at("pass"), 1);
        CHECK_EQ(state.at("cutoff_open"), true);

        declarations[extra] = package("extra");
        declarations[other] = package("other");
        const auto tags = Json::array({release(extra, "v1.10.0"),
                                       release(other, "v2.0.0"),
                                       release(extra, "v1.9.0"),
                                       release(other, "v1.0.0"),
                                       release(idp, "v1.1.0"),
                                       release(idp, "v1.0.0")});
        CHECK_EQ(scheduler.declarations(state, {{"native", target}}, declarations, tags).code, 200);
        CHECK_EQ(state.at("releases").size(), 4U);
        CHECK(!state.at("items").contains(historical));
        CHECK(!state.at("items").contains("other/1.0.0/native"));
        CHECK_EQ(scheduler.admit(state, extra, "v1.9.0", head).code, 200);
        const auto admissions = state.at("releases");
        CHECK_EQ(scheduler.declarations(state, {{"native", target}}, declarations, tags).code, 200);
        CHECK_EQ(scheduler.admit(state, extra, "v1.9.0", head).code, 200);
        CHECK_EQ(scheduler.admit(state, extra, "v1.9.0", std::string(40, 'b')).code, 409);
        CHECK_EQ(state.at("releases"), admissions);

        state = Json::parse(state.dump());
        scheduler.turn(state, 11);
        CHECK_EQ(state.at("pass"), 1);
        CHECK_EQ(state.at("cutoff_open"), false);
        CHECK_EQ(state.at("attempts").size(), 1U);
        CHECK_EQ(state.at("items").at(original).at("state"), "processing");
        for (const auto& item : {originalNext, extraNewest, otherNewest, historical}) {
            CHECK_EQ(state.at("items").at(item).at("state"), "enrolled");
        }

        turn(scheduler, state, 12);
        CHECK_EQ(state.at("pass"), 2);
        CHECK_EQ(state.at("attempts").size(), 4U);
        for (const auto& item : {original, originalNext, extraNewest, otherNewest}) {
            CHECK_EQ(state.at("items").at(item).at("state"), "processing");
        }
        CHECK_EQ(state.at("items").at(historical).at("state"), "enrolled");
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

    void activeArrivals() {
        Options options;
        options.slots = 2;
        const Scheduler scheduler(options);
        const std::string independent = "example/Independent", newcomer = "example/Newcomer";
        const std::string running = "consumer/1.0.0/native", next = "consumer/1.1.0/native";
        const std::string occupied = "independent/1.0.0/native", waiting = "newcomer/2.0.0/native";
        Json declarations{{idp, package("producer")},
                          {consumer, package("consumer", Json::array({Json{{"package", "producer"}, {"version", ">=1"}}}))},
                          {independent, package("independent")}};
        auto state = ready(scheduler, declarations, Json::array({release(consumer, "v1.0.0"), release(independent, "v1.0.0")}));
        const auto published = output("producer", "1.0.0", 1, 5);
        state["entries"] = Json::array({published});
        turn(scheduler, state, 10);
        const auto request = state.at("items").at(running).at("request").get<std::string>();
        const auto key = attemptKey(request, 1);
        const auto old = state.at("items").at(occupied).at("request").get<std::string>();
        CHECK_EQ(scheduler.claim(state, false, request, owner(1, 1), 10).code, 200);
        CHECK_EQ(scheduler.claim(state, false, old, owner(1, 2), 10).code, 200);
        const auto frozen = state.at("attempts").at(key);
        CHECK_EQ(Json::parse(frozen.at("document").get<std::string>()).at("inputs").at("closure").at(0).at("identity"),
                 published.at("identity"));

        declarations[consumer]["tree"] = std::string(40, 'b');
        declarations[consumer]["ci_commit"] = std::string(40, 'c');
        declarations[newcomer] = package("newcomer", Json::array({Json{{"package", "consumer"}, {"version", ">=1.1"}}}));
        const auto tags = Json::array({release(idp, "v2.0.0"), release(consumer, "v1.1.0"), release(newcomer, "v2.0.0")});
        CHECK_EQ(scheduler.declarations(state, {{"native", target}}, declarations, tags).code, 200);
        CHECK_EQ(scheduler.action(state, "dispatch-paused", {{"paused", true}}).code, 200);
        scheduler.turn(state, 11);
        CHECK_EQ(state.at("cutoff_open"), true);
        state = Json::parse(state.dump());
        scheduler.turn(state, 12);
        CHECK_EQ(state.at("attempts").size(), 2U);
        CHECK_EQ(state.at("items").at(next).at("condition"), "waiting");
        CHECK_EQ(state.at("items").at(waiting).at("condition"), "waiting");
        CHECK_EQ(state.at("attempts").at(key), frozen);

        auto failed = owner(1, 2);
        failed.update(Json{{"status", "finished"}, {"outcome", "failed"}, {"reason", "fixture failure"}});
        CHECK_EQ(scheduler.report(state, false, old, failed, 13).code, 200);
        turn(scheduler, state, 13);
        CHECK_EQ(state.at("items").at(occupied).at("state"), "processing");
        CHECK_EQ(view(state, options).at("reservations").at("running"), 2);
        scheduler.observed(state, attemptKey(old, 1), {{"id", 2}, {"status", "completed"}, {"conclusion", "failure"}}, 14);
        turn(scheduler, state, 14);
        CHECK_EQ(state.at("items").at(occupied).at("state"), "failed");
        CHECK_EQ(state.at("attempts").size(), 2U);
        CHECK_EQ(view(state, options).at("reservations").at("running"), 1);
        CHECK_EQ(scheduler.action(state, "dispatch-paused", {{"paused", false}}).code, 200);
        turn(scheduler, state, 15);
        CHECK_EQ(state.at("attempts").size(), 3U);
        CHECK_EQ(state.at("items").at("producer/2.0.0/native").at("state"), "processing");
        CHECK_EQ(state.at("items").at(next).at("condition"), "waiting");
        CHECK_EQ(state.at("items").at(waiting).at("condition"), "waiting");
        CHECK_EQ(state.at("attempts").at(key), frozen);
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

        config.batch = 0;
        const Scheduler publishing(config);
        auto idle = ready(publishing, Json::object(), Json::array());
        idle["retirement_at"] = 10;
        turn(publishing, idle, 10);
        const auto publication = idle.at("attempts").begin()->at("id").get<std::string>();
        const auto old = attemptKey(publication, 1);
        publishing.observed(idle, old, {{"not_found", true}}, 14);
        CHECK_EQ(idle.at("attempts").size(), 1U);
        publishing.observed(idle, old, {{"not_found", true}}, 15);
        CHECK_EQ(idle.at("attempts").at(old).at("run_state"), "superseded");
        CHECK_EQ(view(idle, config).at("reservations").at("publication"), 1);
        publishing.observed(idle, old, {{"id", 10}, {"status", "queued"}}, 16);
        CHECK_EQ(publishing.claim(idle, true, publication, owner(1, 10), 16).code, 409);
        CHECK_EQ(view(idle, config).at("reservations").at("publication"), 1);
        CHECK_EQ(publishing.claim(idle, true, publication, owner(2, 11), 16).code, 200);
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
        state = Json::parse(state.dump());
        turn(scheduler, state, 29);
        CHECK_EQ(state.at("attempts").at(attemptKey(id, 1)).at("dispatch"), "batching");
        CHECK_EQ(state.at("attempts").at(attemptKey(id, 1)).at("batch_at"), 30);
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
        state = Json::parse(state.dump());
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
        state = Json::parse(state.dump());
        turn(scheduler, state, 43);
        auto next = std::ranges::find_if(state.at("attempts"), [&](const Json& attempt) {
            return attempt.at("publication") == true && attempt.at("id") != id;
        });
        CHECK(next != state.at("attempts").end());
        CHECK_EQ(Json::parse(next->at("document").get<std::string>()).at("fault"), "none");

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

    void handoff() {
        Options options;
        options.slots = 2;
        options.batch = 10;
        const Scheduler scheduler(options);
        auto state = ready(scheduler,
                           {{idp, package("producer")}, {consumer, package("consumer")}},
                           Json::array({release(idp, "v1.0.0"), release(consumer, "v1.0.0")}));
        turn(scheduler, state, 1);
        scheduler.admit(state, idp, "v1.1.0", head);
        turn(scheduler, state, 2);
        CHECK_EQ(state.at("items").at("producer/1.1.0/native").at("state"), "enrolled");
        const auto id = state.at("items").at("producer/1.0.0/native").at("request").get<std::string>();
        complete(scheduler, state, id, 1, 47, 3, true);
        turn(scheduler, state, 3);
        CHECK_EQ(view(state, options).at("reservations").at("publication"), 1);
        CHECK_EQ(state.at("items").at("producer/1.1.0/native").at("state"), "enrolled");
        turn(scheduler, state, 12);
        const auto reserved = std::ranges::find_if(state.at("attempts"), [](const auto& attempt) {
            return attempt.at("publication") == true;
        });
        CHECK_EQ(reserved->at("dispatch"), "batching");
        CHECK_EQ(reserved->at("batch_at"), 13);
        CHECK_EQ(state.at("items").at("producer/1.1.0/native").at("state"), "enrolled");
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

    void expiry(bool beforeTurn) {
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
                if (!beforeTurn) {
                    turn(scheduler, state, 13);
                }
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
            const auto item = state["items"].begin().value();
            CHECK_EQ(item.at("revision"), change ? 2 : 1);
            CHECK_EQ(item.at("attempt"), change ? 1 : 2);
            const auto key = attemptKey(item.at("request").get<std::string>(), item.at("attempt").get<unsigned>());
            if (!change) {
                CHECK_EQ(Json::parse(state.at("attempts").at(key).at("document").get<std::string>()).at("inputs"), frozen);
                CHECK_EQ(scheduler.claim(state, false, first, owner(1, 51), 16).code, 409);
            }
        }
    }

    void closure() {
        const Scheduler scheduler({});
        const std::string item = "consumer/1.0.0/native";
        const auto dependencies =
            Json::array({Json{{"package", "left"}, {"version", ">=1"}}, Json{{"package", "right"}, {"version", ">=1"}}});
        auto state = ready(scheduler,
                           {{consumer, package("consumer", dependencies)}, {idp, package("shared")}},
                           Json::array({release(consumer, "v1.0.0")}));
        auto left = output("left", "1.0.0", 1, 1);
        auto right = output("right", "1.0.0", 1, 1);
        auto original = output("shared", "1.0.0", 1, 1), newer = output("shared", "1.0.0", 2, 1);
        for (auto* record : {&original, &newer}) {
            (*record)["target"] = "native";
            (*record)["repository"] = idp;
            record->erase("closure");
        }
        left["closure"] = Json::array({original});
        right["closure"] = Json::array({newer});
        state["entries"] = Json::array({left, right});
        turn(scheduler, state, 2);
        CHECK_EQ(state.at("items").at(item).at("condition"), "blocked");
        CHECK_EQ(state.at("items").at(item).at("reason"), "closure conflict");
        scheduler.admit(state, idp, "v1.0.0", head);
        turn(scheduler, state, 3);
        CHECK_EQ(state.at("items").at(item).at("condition"), "waiting");
        CHECK_EQ(state.at("items").at(item).at("reason"), "checking dependency refresh");
        left["closure"] = Json::array({newer});
        state["entries"] = Json::array({left, right});
        turn(scheduler, state, 4);
        CHECK_EQ(state.at("items").at(item).at("state"), "processing");
    }

    void identity() {
        const Scheduler scheduler({});
        const std::string item = "consumer/1.0.0/native";
        const auto tags = Json::array({release(consumer, "v1.0.0")});
        auto decl = package("consumer", Json::array({Json{{"package", "shared"}, {"version", ">=1"}}}));
        auto environment = target;
        auto state = ready(scheduler, {{consumer, decl}}, tags);
        auto bound = output("shared", "1.0.0", 1, 1);
        state["entries"] = Json::array({bound});
        turn(scheduler, state, 2);
        const auto original = state.at("items").at(item).at("identity");
        CHECK_EQ(original, sha256(Json::array({head, head, target, "Xv2", Json::array({bound.at("identity")})}).dump()));
        state["items"][item]["state"] = "published";
        decl["ci_commit"] = std::string(40, 'b');
        scheduler.declarations(state, {{"native", target}}, {{consumer, decl}}, tags);
        turn(scheduler, state, 3);
        CHECK_EQ(state.at("items").at(item).at("state"), "published");
        CHECK_EQ(state.at("items").at(item).at("identity"), original);

        const auto unchanged = state;
        bound["published_at"] = 20;
        state["entries"] = Json::array({bound});
        turn(scheduler, state, 4);
        CHECK_EQ(state.at("items").at(item).at("state"), "published");
        bound["identity"] = sha256("new binding");
        state["entries"] = Json::array({bound});
        turn(scheduler, state, 5);
        CHECK_EQ(state.at("items").at(item).at("revision"), 2);
        CHECK(state.at("items").at(item).at("identity") != original);

        for (const bool tree : {false, true}) {
            state = unchanged;
            state["items"][item]["state"] = "failed";
            auto next = decl;
            environment = target;
            if (tree) {
                next["tree"] = std::string(40, 'c');
            } else {
                environment["image"] = "different environment digest";
            }
            scheduler.declarations(state, {{"native", environment}}, {{consumer, next}}, tags);
            turn(scheduler, state, 6);
            CHECK_EQ(state.at("items").at(item).at("revision"), 2);
            CHECK(state.at("items").at(item).at("identity") != original);
            CHECK_EQ(scheduler.action(state, "retry", {{"item", item}}).code, 409);
        }
    }

    void observation() {
        Options options;
        options.slots = 1;
        options.queue = 5;
        options.execution = 7;
        options.grace = 2;
        const Scheduler scheduler(options);
        const auto base = ready(scheduler, {{idp, package("producer")}}, Json::array({release(idp, "v1.0.0")}));
        const std::string item = "producer/1.0.0/native";
        for (const std::string conclusion :
             {"success", "failure", "cancelled", "timed_out", "startup_failure", "action_required", "stale", "skipped", "neutral"}) {
            auto state = base;
            turn(scheduler, state, 10);
            state = Json::parse(state.dump());
            const auto id = state.at("items").at(item).at("request").get<std::string>();
            const auto key = attemptKey(id, 1);
            scheduler.observed(state, key, {{"id", 8}, {"status", "queued"}}, 14);
            CHECK(!state.at("attempts").at(key).value("cancel_requested", false));
            scheduler.observed(state, key, {{"not_found", true}}, 15);
            CHECK_EQ(state.at("attempts").at(key).at("run_state"), "dispatched");
            CHECK_EQ(view(state, options).at("reservations").at("queued"), 1);
            scheduler.observed(state, key, {{"id", 8}, {"status", "queued"}}, 15);
            CHECK(state.at("attempts").at(key).at("cancel_requested") == true);
            CHECK_EQ(state.at("attempts").size(), 1U);
            CHECK_EQ(scheduler.claim(state, false, id, owner(1, 8), 16).code, 200);
            state = Json::parse(state.dump());
            scheduler.observed(state, key, {{"id", 8}, {"status", "completed"}, {"conclusion", conclusion}}, 19);
            CHECK_EQ(view(state, options).at("reservations").at("running"), 0);
            CHECK_EQ(state.at("attempts").at(key).at("grace_at"), 21);
            CHECK_EQ(state.at("items").at(item).at("state"), "processing");
            state = Json::parse(state.dump());
            scheduler.action(state, "dispatch-paused", {{"paused", true}});
            turn(scheduler, state, 20);
            CHECK_EQ(state.at("items").at(item).at("state"), "processing");
            turn(scheduler, state, 21);
            CHECK_EQ(state.at("items").at(item).at("reason"), "conclusion " + conclusion + ", no report");
            CHECK(view(state, options).at("items").at(0).at("retryable") == true);
            CHECK(view(state, options).at("paused") == true);
        }
        auto execution = base;
        turn(scheduler, execution, 10);
        const auto id = execution.at("items").at(item).at("request").get<std::string>();
        const auto key = attemptKey(id, 1);
        CHECK_EQ(scheduler.claim(execution, false, id, owner(1, 9), 11).code, 200);
        scheduler.observed(execution, key, {{"id", 9}, {"status", "in_progress"}}, 17);
        CHECK(!execution.at("attempts").at(key).value("cancel_requested", false));
        scheduler.observed(execution, key, {{"id", 9}, {"status", "in_progress"}}, 18);
        CHECK(execution.at("attempts").at(key).at("cancel_requested") == true);
        CHECK_EQ(view(execution, options).at("reservations").at("running"), 1);
    }

    void snapshots() {
        Options options;
        options.batch = 0;
        options.slots = 2;
        const Scheduler scheduler(options);
        const std::string base = "refs/snapshots/feed/";
        auto state = ready(scheduler, {{idp, package("producer")}}, Json::array({release(idp, "v1.0.0")}));
        state["snapshots"] = {{base + "pre-x", std::string(40, '0')}, {base + "older-head", head}, {base + "stale", std::string(40, '1')}};
        for (const char recent : {'2', '3', '4'}) {
            const auto ref = base + recent;
            state["snapshots"][ref] = std::string(40, recent);
            state["rollback"].push_back(ref);
        }
        auto findPlan = [&](const Json& facts) {
            const auto pending = std::ranges::find_if(facts.at("attempts"), [](const auto& record) {
                return record.at("publication") == true && record.at("reconciled") == false;
            });
            return Json::parse(pending->at("document").template get<std::string>());
        };
        state["retirement_at"] = 1;
        turn(scheduler, state, 2);
        const auto first = findPlan(state);
        CHECK_EQ(first.at("delete_snapshots"), Json::array({base + "stale"}));
        const std::string item = "producer/1.0.0/native";
        const auto request = state.at("items").at(item).at("request").get<std::string>();
        CHECK_EQ(state.at("attempts").at(attemptKey(request, 1)).at("pin"), head);
        auto refs = state.at("snapshots");
        refs[base + "published"] = std::string(40, '5');
        auto rotated = state;
        refs[base + first.at("publication").get<std::string>()] = std::string(40, '5');
        scheduler.feed(rotated, std::string(40, '5'), refs, Json::array(), 3);
        CHECK_EQ(rotated.at("rollback"), Json::array({base + "3", base + "4", base + first.at("publication").get<std::string>()}));
        CHECK_EQ(rotated.at("attempts").at(attemptKey(request, 1)).at("pin"), head);
        for (const std::string stage : {"processing", "built", "retry", "released"}) {
            auto copy = state;
            copy["attempts"].erase(attemptKey(first.at("publication").get<std::string>(), 1));
            copy["items"][item]["state"] = stage == "retry" || stage == "released" ? "failed" : stage;
            if (stage != "processing") {
                auto& record = copy["attempts"][attemptKey(request, 1)];
                record["run_state"] = "completed";
                record["reconciled"] = true;
                record["report"] = {{"status", "finished"}, {"outcome", "built"}, {"artifact_id", 1}, {"inventory", Json::array()}};
            }
            if (stage == "released") {
                copy["declarations"][idp]["releases"] = ">=2";
            }
            scheduler.feed(copy, std::string(40, '5'), refs, Json::array(), 3);
            turn(scheduler, copy, 4);
            const auto next = findPlan(copy);
            const auto& deletion = next.at("delete_snapshots");
            CHECK_EQ(std::ranges::find(deletion, Json(base + "older-head")) != deletion.end(), stage == "released");
            for (const auto* protectedName : {"pre-x", "published", "2", "3", "4"}) {
                CHECK(std::ranges::find(deletion, Json(base + protectedName)) == deletion.end());
            }
        }
    }

    void backoff() {
        Options options;
        options.batch = 0;
        options.grace = 1;
        const Scheduler scheduler(options);
        auto state = ready(scheduler, {{idp, package("producer")}}, Json::array({release(idp, "v1.0.0")}));
        turn(scheduler, state, 1);
        const auto request = state.at("items").at("producer/1.0.0/native").at("request").get<std::string>();
        complete(scheduler, state, request, 1, 8, 2, true);
        turn(scheduler, state, 2);
        const auto first = std::ranges::find_if(state.at("attempts"), [](const auto& record) {
            return record.at("publication") == true;
        });
        const auto id = first->at("id").get<std::string>();
        for (unsigned attempt = 1; attempt <= 4; ++attempt) {
            const auto key = attemptKey(id, attempt);
            const auto run = attempt + 20;
            const auto at = 200 * static_cast<Time>(attempt);
            const auto token = owner(attempt, run);
            CHECK_EQ(scheduler.claim(state, true, id, token, at).code, 200);
            auto failed = token;
            failed.update(Json{{"status", "finished"}, {"outcome", "failed"}, {"reason", "feed push failed"}});
            scheduler.report(state, true, id, failed, at);
            scheduler.observed(state, key, {{"id", run}, {"status", "completed"}, {"conclusion", "failure"}}, at);
            const auto refs = state.at("snapshots");
            scheduler.feed(state, head, refs, Json::array(), at);
            state = Json::parse(state.dump());
            if (attempt < 4) {
                const auto next = state.at("attempts").at(attemptKey(id, attempt + 1));
                CHECK_EQ(next.at("retry_at"), at + 60 * (1 << (attempt - 1)));
                CHECK_EQ(scheduler.claim(state, true, id, token, at).code, 409);
            }
        }
        const auto item = state.at("items").at("producer/1.0.0/native");
        CHECK_EQ(item.at("state"), "failed");
        CHECK_EQ(item.at("reason"), "feed push failed");
        CHECK(!state.at("attempts").contains(attemptKey(id, 5)));
    }

    void publicationRetryCapacity() {
        Options options;
        options.slots = 1;
        options.batch = 0;
        const Scheduler scheduler(options);
        auto state = ready(scheduler, {{idp, package("producer")}}, Json::array({release(idp, "v1.0.0")}));
        turn(scheduler, state, 10);
        const auto request = state.at("items").at("producer/1.0.0/native").at("request").get<std::string>();
        complete(scheduler, state, request, 1, 1, 11, true);
        turn(scheduler, state, 11);
        const auto publication = view(state, options).at("publications").at(0).at("id").get<std::string>();
        CHECK_EQ(scheduler.claim(state, true, publication, owner(1, 2), 12).code, 200);
        CHECK_EQ(scheduler.admit(state, idp, "v2.0.0", head).code, 200);
        turn(scheduler, state, 13);
        CHECK_EQ(state.at("items").at("producer/2.0.0/native").at("state"), "enrolled");
        auto failed = owner(1, 2);
        failed.update(Json{{"status", "finished"}, {"outcome", "failed"}, {"reason", "fixture push failure"}});
        CHECK_EQ(scheduler.report(state, true, publication, failed, 14).code, 200);
        scheduler.observed(state, attemptKey(publication, 1), {{"id", 2}, {"status", "completed"}, {"conclusion", "failure"}}, 15);
        turn(scheduler, state, 15);
        const auto occupied = view(state, options).at("reservations");
        CHECK_EQ(occupied.at("publication"), 0U);
        CHECK_EQ(occupied.at("queued"), 1U);

        state = Json::parse(state.dump());
        const auto snapshots = state.at("snapshots");
        scheduler.feed(state, head, snapshots, Json::array(), 16);
        const auto after = view(state, options).at("reservations");
        CHECK_EQ(after.at("queued"), 1U);
        CHECK_EQ(after.at("queued").get<unsigned>() + after.at("running").get<unsigned>() + after.at("publication").get<unsigned>(),
                 options.slots);

        const auto nextBuild = state.at("items").at("producer/2.0.0/native").at("request").get<std::string>();
        CHECK_EQ(scheduler.claim(state, false, nextBuild, owner(1, 3), 17).code, 200);
        scheduler.observed(state, attemptKey(nextBuild, 1), {{"id", 3}, {"status", "completed"}, {"conclusion", "failure"}}, 18);
        scheduler.feed(state, head, snapshots, Json::array(), 18);
        turn(scheduler, state, 18);
        const auto retried = view(state, options);
        CHECK_EQ(retried.at("reservations").at("publication"), 1U);
        CHECK_EQ(retried.at("publications").back().at("id"), publication);
        CHECK_EQ(retried.at("publications").back().at("attempt"), 2U);
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
    } else if (mode == "registry_arrivals") {
        registryArrivals();
    } else if (mode == "active_arrivals") {
        activeArrivals();
    } else if (mode == "binding") {
        binding();
    } else if (mode == "attempts") {
        attempts();
    } else if (mode == "publications") {
        publications();
    } else if (mode == "publication_retry_capacity") {
        publicationRetryCapacity();
    } else if (mode == "handoff") {
        handoff();
    } else if (mode == "retention") {
        retention();
    } else if (mode == "expiry") {
        expiry(false);
    } else if (mode == "expiry_before_turn") {
        expiry(true);
    } else if (mode == "closure") {
        closure();
    } else if (mode == "identity") {
        identity();
    } else if (mode == "observation") {
        observation();
    } else if (mode == "snapshots") {
        snapshots();
    } else if (mode == "backoff") {
        backoff();
    } else if (mode == "persistence") {
        persistenceTest();
    } else {
        CHECK(false);
    }
    return checkResult();
}
