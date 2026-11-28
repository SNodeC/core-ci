#include "Check.h"
#include "core/Scheduler.h"
#include "core/Version.h"
#include "persistence/StateSession.h"
#include "persistence/StateSql.h"

#include <algorithm>
#include <cstddef>
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
        if (state.at("cutoff_open") == true) {
            // Opening and committing a new cutoff is separate from filling that pass.
            scheduler.turn(state, at);
        }
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

    void admissions() {
        const auto endpoint = Version::parse("3.0.0").value();
        CHECK(endpoint.satisfies("==3"));
        CHECK(endpoint.satisfies(">=3,<=3,>2,<4"));
        CHECK(!endpoint.satisfies(">3"));
        CHECK(!endpoint.satisfies("<3"));
        CHECK(!endpoint.satisfies(">=3,<3"));
        CHECK(!Version::validRange(">=3,,<4"));
        CHECK(!Version::validRange("=3"));
        CHECK(!stableTag("v4.0.0-rc.1"));
        CHECK(Version::parse("1.10.0").value() > Version::parse("1.9.0").value());

        Options options;
        options.slots = 3;
        const Scheduler scheduler(options);
        auto declaration = package("producer");
        declaration["releases"] = ">=1.10,<2";
        const Json declarations{{idp, declaration}};
        auto state = ready(scheduler, declarations, Json::array({release(idp, "v1.9.0")}));
        CHECK_EQ(scheduler
                     .declarations(state,
                                   {{"native", target}},
                                   declarations,
                                   Json::array({release(idp, "v1.11.0"), release(idp, "v1.10.0"), release(idp, "v1.12.0")}))
                     .code,
                 200);
        scheduler.turn(state, 10); // Commit the cutoff.
        scheduler.turn(state, 10); // This single fill must pick all three ready items.
        CHECK_EQ(state.at("attempts").size(), 3U);
        const auto& unsupported = state.at("items").at("producer/1.9.0/native");
        CHECK_EQ(unsupported.at("condition"), "unsupported");
        CHECK_EQ(unsupported.at("reason"), "release outside declaration range");
    }

    void admissionStress() {
        Options options;
        options.slots = 7;
        const Scheduler scheduler(options);
        const std::vector<std::string> names{"amber", "birch", "cedar", "dune", "elm"};
        auto catalog = Json{{"native", target}, {"second", target}};
        catalog["second"]["id"] = "second";
        Json declarations = Json::object();
        for (std::size_t i = 0; i < 3; ++i) {
            declarations["fixtures/" + names[i]] = package(names[i]);
            declarations["fixtures/" + names[i]]["targets"] = Json::array({"native", "second"});
        }
        auto state = initialState();
        CHECK_EQ(scheduler.declarations(state, catalog, declarations, Json::array()).code, 200);
        scheduler.feed(state, head, {{"refs/snapshots/feed/pre-x", head}}, Json::array(), 1);
        std::vector<std::string> expected;
        const auto admit = [&](const std::string& name, unsigned minor) {
            const auto version = "1." + std::to_string(minor) + ".0";
            CHECK_EQ(scheduler.admit(state, "fixtures/" + name, "v" + version, head).code, 200);
            CHECK_EQ(scheduler.admit(state, "fixtures/" + name, "v" + version, head).code, 200);
            expected.push_back(name + "/" + version + "/native");
            expected.push_back(name + "/" + version + "/second");
        };
        for (std::size_t i = 0; i < 3; ++i) {
            admit(names[i], 1);
        }
        scheduler.turn(state, 10); // Commit the cutoff for three repositories and six items.
        for (std::size_t i = 3; i < names.size(); ++i) {
            declarations["fixtures/" + names[i]] = package(names[i]);
            declarations["fixtures/" + names[i]]["targets"] = Json::array({"native", "second"});
        }
        CHECK_EQ(scheduler.declarations(state, catalog, declarations, Json::array()).code, 200);
        for (std::size_t i = 3; i < names.size(); ++i) {
            admit(names[i], 1);
        }
        for (unsigned minor = 2; minor <= 10; ++minor) {
            for (const auto& name : names) {
                admit(name, minor);
            }
        }
        CHECK_EQ(expected.size(), 100U);
        CHECK_EQ(state.at("releases").size(), 50U);
        scheduler.turn(state, 11); // Fill exactly the six eligible items, leaving one slot free.
        CHECK_EQ(view(state, options).at("reservations").at("queued"), 6U);
        CHECK_EQ(state.at("items").at(expected[6]).at("state"), "enrolled");
        turn(scheduler, state, 12);
        std::vector<std::string> documents;
        for (std::size_t completed = 0; completed < expected.size();) {
            const auto count = std::min<std::size_t>(options.slots, expected.size() - completed);
            CHECK_EQ(view(state, options).at("reservations").at("queued"), count);
            CHECK_EQ(state.at("attempts").size(), completed + count);
            const auto later = completed + count;
            for (auto index = completed; index < later; ++index) {
                const auto& item = state.at("items").at(expected[index]);
                CHECK_EQ(item.at("state"), "processing");
                const auto request = item.at("request").get<std::string>();
                documents.push_back(state.at("attempts").at(attemptKey(request, 1)).at("document").get<std::string>());
                CHECK_EQ(scheduler.claim(state, false, request, owner(1, index + 1), 13 + completed).code, 200);
            }
            state = Json::parse(state.dump());
            for (auto index = later; index > completed; --index) {
                const auto& item = state.at("items").at(expected[index - 1]);
                const auto request = item.at("request").get<std::string>();
                const auto key = attemptKey(request, 1);
                CHECK_EQ(state.at("attempts").at(key).at("document"), Json(documents[index - 1]));
                auto report = owner(1, index);
                report.update(Json{{"status", "finished"}, {"outcome", "failed"}});
                CHECK_EQ(scheduler.report(state, false, request, report, 14 + completed).code, 200);
                scheduler.observed(state, key, {{"id", index}, {"status", "completed"}, {"conclusion", "failure"}}, 14 + completed);
            }
            completed = later;
            turn(scheduler, state, 14 + completed);
        }
        CHECK_EQ(view(state, options).at("reservations"), (Json{{"slots", 7}, {"queued", 0}, {"running", 0}, {"publication", 0}}));
        CHECK_EQ(state.at("attempts").size(), 100U);
    }

    void activeArrivalStress() {
        struct Build {
            std::string key;
            std::string document;
            unsigned run;
        };
        const std::vector<std::string> names{
            "aster", "beech", "coral", "delta", "ember", "fern", "grove", "hazel", "iris", "juniper", "kelp", "larch"};
        const std::vector<std::string> targets{"apk-aarch64", "deb-amd64", "deb-arm64", "deb-riscv64", "ipk-mips64", "rpm-amd64"};
        auto catalog = Json::object();
        for (const auto& id : targets) {
            auto environment = target;
            const auto delimiter = id.find('-');
            environment["id"] = id;
            environment["family"] = id.substr(0, delimiter);
            environment["arch"] = id.substr(delimiter + 1);
            if (id == "apk-aarch64" || id == "deb-arm64") {
                environment["runner"] = "ubuntu-24.04-arm";
            }
            catalog[id] = std::move(environment);
        }
        for (const unsigned slots : {1U, 7U, 20U}) {
            Options options;
            options.slots = slots;
            const Scheduler scheduler(options);
            Json declarations = Json::object();
            const auto declare = [&](const std::string& name) {
                auto declaration = package(name);
                declaration["targets"] = targets;
                declarations["stress/" + name] = std::move(declaration);
            };
            for (std::size_t index = 0; index < 5; ++index) {
                declare(names[index]);
            }
            auto state = initialState();
            CHECK_EQ(scheduler.declarations(state, catalog, declarations, Json::array()).code, 200);
            scheduler.feed(state, head, {{"refs/snapshots/feed/pre-x", head}}, Json::array(), 1);
            std::vector<std::string> expected;
            std::set<std::pair<std::string, unsigned>> admitted;
            const auto admit = [&](const std::string& name, unsigned minor) {
                const auto version = "1." + std::to_string(minor) + ".0";
                CHECK_EQ(scheduler.admit(state, "stress/" + name, "v" + version, head).code, 200);
                if (admitted.emplace(name, minor).second) {
                    for (const auto& targetId : targets) {
                        expected.push_back(name + "/" + version + "/" + targetId);
                    }
                }
            };
            for (unsigned minor = 1; minor <= 4; ++minor) {
                for (std::size_t index = 0; index < 5; ++index) {
                    admit(names[index], minor);
                }
            }
            std::size_t dispatched = 0;
            std::vector<Build> active;
            Time now = 10;
            const auto claim = [&](std::size_t cutoff) {
                const auto count = std::min<std::size_t>(slots - active.size(), cutoff - dispatched);
                std::size_t running = 0, queued = 0;
                for (const auto& record : state.at("attempts")) {
                    running += record.at("run_state") == "claimed";
                    queued += record.at("run_state") == "dispatched";
                    CHECK_EQ(record.at("publication"), false);
                }
                CHECK_EQ(running, active.size());
                CHECK_EQ(queued, count);
                CHECK_EQ(state.at("attempts").size(), dispatched + count);
                const auto end = dispatched + count;
                for (; dispatched < end; ++dispatched) {
                    const auto& item = state.at("items").at(expected[dispatched]);
                    CHECK_EQ(item.at("state"), "processing");
                    const auto request = item.at("request").get<std::string>();
                    const auto key = attemptKey(request, 1);
                    const bool arm = item.at("target") == "apk-aarch64" || item.at("target") == "deb-arm64";
                    CHECK(request.starts_with(arm ? "arm." : "b."));
                    const auto run = static_cast<unsigned>(dispatched + 1);
                    CHECK_EQ(scheduler.claim(state, false, request, owner(1, run), now).code, 200);
                    active.push_back({key, state.at("attempts").at(key).at("document").get<std::string>(), run});
                }
            };
            const auto finish = [&](std::size_t index) {
                const auto build = active.at(index);
                const auto& record = state.at("attempts").at(build.key);
                CHECK_EQ(record.at("document"), Json(build.document));
                const auto request = record.at("id").get<std::string>();
                const Json ended{{"id", build.run}, {"status", "completed"}, {"conclusion", "failure"}};
                auto report = owner(1, build.run);
                report.update(Json{{"status", "finished"}, {"outcome", "failed"}});
                if (build.run % 2 == 0) {
                    scheduler.observed(state, build.key, ended, now);
                    CHECK_EQ(scheduler.report(state, false, request, report, now).code, 200);
                } else {
                    CHECK_EQ(scheduler.report(state, false, request, report, now).code, 200);
                    scheduler.observed(state, build.key, ended, now);
                }
                active.erase(active.begin() + static_cast<std::ptrdiff_t>(index));
            };

            turn(scheduler, state, now++);
            claim(expected.size());
            for (std::size_t newcomer = 5; newcomer < names.size(); ++newcomer) {
                admit(names.front(), static_cast<unsigned>(newcomer));
                scheduler.turn(state, now); // Establish what is eligible before new registry work.
                CHECK_EQ(state.at("cutoff_open"), true);
                const auto cutoff = expected.size();
                declare(names[newcomer]);
                CHECK_EQ(scheduler.declarations(state, catalog, declarations, Json::array()).code, 200);
                for (unsigned minor = 1; minor <= 12; ++minor) {
                    admit(names[newcomer], minor);
                    admit(names[newcomer], minor);
                }
                finish((newcomer * 3) % active.size());
                scheduler.turn(state, now++); // Fill one released slot without admitting the late arrivals.
                CHECK_EQ(state.at("items").at(expected.at(cutoff)).at("state"), "enrolled");
                claim(cutoff);
                CHECK_EQ(view(state, options).at("reservations").at("running"), slots);
                state = Json::parse(state.dump());
            }
            for (unsigned minor = 5; minor <= 12; ++minor) {
                for (std::size_t index = 0; index < 5; ++index) {
                    admit(names[index], minor);
                }
            }
            CHECK_EQ(expected.size(), 864U);
            CHECK_EQ(state.at("releases").size(), 144U);
            for (std::size_t batch = 0; !active.empty(); ++batch) {
                const auto take = std::min(active.size(), batch % 5 + 1);
                if (batch % 13 == 0) {
                    state = Json::parse(state.dump());
                }
                for (std::size_t offset = 0; offset < take; ++offset) {
                    finish((batch + offset) % active.size());
                }
                turn(scheduler, state, now++);
                claim(expected.size());
            }
            CHECK_EQ(dispatched, 864U);
            CHECK_EQ(state.at("attempts").size(), 864U);
            CHECK_EQ(view(state, options).at("reservations"), (Json{{"slots", slots}, {"queued", 0}, {"running", 0}, {"publication", 0}}));
        }
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

    void dependencyStress() {
        const std::vector<std::string> names{
            "atlas", "basil", "cairn", "dune", "elm", "fern", "grove", "hazel", "iris", "juniper", "kelp", "larch"};
        const std::vector<std::vector<std::size_t>> edges{{}, {}, {0}, {0}, {2, 3}, {1, 4}, {1}, {5, 6}, {7}, {6, 8}, {3, 9}, {4, 9}};
        Json declarations = Json::object();
        auto catalog = Json{{"native", target}, {"second", target}};
        catalog["second"]["id"] = "second";
        for (std::size_t index = 0; index < names.size(); ++index) {
            Json dependencies = Json::array();
            for (const auto provider : edges[index]) {
                dependencies.push_back({{"package", names[provider]}, {"version", provider == 1 ? "==1" : ">=1,<3"}});
            }
            auto declaration = package(names[index], dependencies);
            declaration["targets"] = Json::array({"native", "second"});
            declarations["dag/" + names[index]] = std::move(declaration);
        }
        bool mixedQuarantine = false;
        for (const unsigned slots : {1U, 7U, 20U}) {
            Options options;
            options.slots = slots;
            options.batch = 0;
            options.queue = 5;
            options.grace = 2;
            const Scheduler scheduler(options);
            auto state = initialState();
            CHECK_EQ(scheduler.declarations(state, catalog, declarations, Json::array()).code, 200);
            scheduler.feed(state, head, {{"refs/snapshots/feed/pre-x", head}}, Json::array(), 1);
            Time now = 10;
            unsigned run = 0;
            std::set<std::string> checked, injected;
            for (const std::string version : {"1.0.0", "2.0.0"}) {
                for (const auto& name : names) {
                    CHECK_EQ(scheduler.admit(state, "dag/" + name, "v" + version, sha256(name + version).substr(0, 40)).code, 200);
                }
                auto terminal = [&] {
                    return std::ranges::all_of(state.at("items"), [](const auto& item) {
                        return item.at("state") == "published" || item.at("state") == "failed";
                    });
                };
                for (unsigned round = 0; round < 300 && !terminal(); ++round) {
                    const auto used = std::ranges::count_if(state.at("attempts"), reserved);
                    const bool publicationPending = std::ranges::any_of(state.at("attempts"), [](const auto& record) {
                        return record.at("publication") == true && (reserved(record) || !record.value("reconciled", false));
                    });
                    const bool built = std::ranges::any_of(state.at("items"), [](const auto& item) {
                        return item.at("state") == "built" && !item.contains("publication");
                    });
                    turn(scheduler, state, now);
                    const auto counted = std::ranges::count_if(state.at("attempts"), reserved);
                    CHECK(counted <= slots);
                    std::vector<std::string> pending;
                    unsigned publishers = 0;
                    for (const auto& [key, record] : state.at("attempts").items()) {
                        if (reserved(record)) {
                            publishers += record.at("publication") == true;
                            pending.push_back(key);
                            if (record.at("publication") == false && checked.insert(key).second) {
                                const auto inputs = Json::parse(record.at("document").template get<std::string>()).at("inputs");
                                std::set<std::string> closure;
                                for (const auto& binding : inputs.at("closure")) {
                                    CHECK(closure.insert(binding.at("package").get<std::string>()).second);
                                    CHECK(std::ranges::any_of(state.at("entries"), [&](const auto& entry) {
                                        return entry.at("identity") == binding.at("identity") &&
                                               entry.at("target").at("id") == inputs.at("target").at("id") &&
                                               entry.at("revision") == binding.at("revision");
                                    }));
                                }
                                const auto& declaration = declarations.at(inputs.at("repo").get<std::string>());
                                for (const auto& required : declaration.at("depends_on")) {
                                    CHECK(closure.contains(required.at("package").get<std::string>()));
                                }
                            }
                        }
                    }
                    CHECK(publishers <= 1U);
                    CHECK_EQ(!built || publicationPending || used >= slots || publishers == 1, true);
                    if (round == 0 && version == "2.0.0") {
                        CHECK_EQ(state.at("items").at("cairn/2.0.0/native").at("condition"), "waiting");
                    }
                    for (const auto& key : pending) {
                        const auto record = state.at("attempts").at(key);
                        const auto id = record.at("id").get<std::string>();
                        const auto attempt = record.at("attempt").get<unsigned>();
                        if (record.at("publication") == true) {
                            const auto plan = Json::parse(record.at("document").get<std::string>());
                            CHECK_EQ(scheduler.claim(state, true, id, owner(attempt, ++run), now).code, 200);
                            scheduler.observed(state, key, {{"id", run}, {"status", "completed"}, {"conclusion", "success"}}, now);
                            auto entries = state.at("entries");
                            Json receipts = Json::array();
                            bool quarantined = false, succeeded = false;
                            for (auto entry : plan.at("items")) {
                                Json receipt{
                                    {"publication", id}, {"attempt", attempt}, {"request", entry.at("request")}, {"outcome", "published"}};
                                if (entry.at("package") == "larch" && entry.at("version") == "2.0.0" &&
                                    entry.at("target").at("id") == "native") {
                                    receipt.update(Json{{"outcome", "failed"}, {"reason", "immutable-conflict"}});
                                    quarantined = true;
                                    injected.insert("quarantine");
                                } else if (entry.at("package") == "kelp" && entry.at("version") == "1.0.0" &&
                                           entry.at("target").at("id") == "second" && injected.insert("expiry").second) {
                                    receipt.update(Json{{"outcome", "failed"}, {"failure", "artifact-expired"}});
                                } else {
                                    entry["published_at"] = now;
                                    entry["inventory"] = Json::array({Json{{"name", entry.at("package")},
                                                                           {"version", entry.at("version")},
                                                                           {"runtime_dependencies", Json::array()}}});
                                    entries.push_back(std::move(entry));
                                    succeeded = true;
                                }
                                receipts.push_back(std::move(receipt));
                            }
                            mixedQuarantine = mixedQuarantine || (quarantined && succeeded);
                            const auto commit = sha256(id).substr(0, 40);
                            auto snapshots = state.at("snapshots");
                            snapshots["refs/snapshots/feed/" + id] = commit;
                            CHECK_EQ(
                                scheduler
                                    .feed(
                                        state, commit, snapshots, {{"publication", id}, {"entries", entries}, {"receipts", receipts}}, now)
                                    .code,
                                200);
                        } else {
                            const auto item = record.at("item").get<std::string>();
                            if (item == "atlas/1.0.0/native" && injected.insert("supersede").second) {
                                scheduler.observed(state, key, {{"not_found", true}}, now + options.queue);
                                CHECK_EQ(view(state, options).at("reservations").at("queued").get<unsigned>() <= slots, true);
                                CHECK_EQ(scheduler.claim(state, false, id, owner(attempt, ++run), now).code, 409);
                            } else if ((item == "kelp/2.0.0/native" && injected.insert("deadline").second) ||
                                       (item == "larch/1.0.0/second" && injected.insert("retry").second)) {
                                const auto who = owner(attempt, ++run);
                                CHECK_EQ(scheduler.claim(state, false, id, who, now).code, 200);
                                if (item == "kelp/2.0.0/native") {
                                    now += options.execution;
                                    scheduler.observed(state, key, {{"id", run}, {"status", "in_progress"}}, now);
                                    CHECK_EQ(state.at("attempts").at(key).at("cancel_requested"), true);
                                } else {
                                    auto report = who;
                                    report.update(Json{{"status", "finished"}, {"outcome", "failed"}, {"reason", "fixture transient"}});
                                    CHECK_EQ(scheduler.report(state, false, id, report, now).code, 200);
                                }
                                scheduler.observed(state, key, {{"id", run}, {"status", "completed"}, {"conclusion", "cancelled"}}, now);
                                if (item == "larch/1.0.0/second") {
                                    CHECK_EQ(scheduler.action(state, "retry", {{"item", item}}).code, 200);
                                }
                            } else {
                                ++run;
                                complete(scheduler, state, id, attempt, run, now, run % 2 == 0);
                            }
                        }
                    }
                    ++now;
                }
                CHECK(terminal());
            }
            CHECK_EQ(injected.size(), 5U);
            CHECK_EQ(state.at("items").size(), 48U);
            for (const auto& [key, item] : state.at("items").items()) {
                const std::string reason = key == "kelp/2.0.0/native"    ? "conclusion cancelled, no report"
                                           : key == "larch/2.0.0/native" ? "immutable-conflict"
                                                                         : "";
                CHECK_EQ(item.at("state"), reason.empty() ? "published" : "failed");
                CHECK_EQ(item.at("reason"), reason);
            }
            CHECK_EQ(std::ranges::count_if(state.at("attempts"), reserved), 0);
        }
        CHECK(mixedQuarantine);
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
        const auto record = state.at("attempts").at(attemptKey(selected.at("request").get<std::string>(), 1));
        const auto input = Json::parse(record.at("document").get<std::string>()).at("inputs");
        CHECK_EQ(input.at("closure").at(0).at("revision"), 3);
        auto cyclic = ready(scheduler,
                            {{idp, package("producer", Json::array({Json{{"package", "consumer"}, {"version", ">=1"}}}))},
                             {consumer, package("consumer", Json::array({Json{{"package", "producer"}, {"version", ">=1"}}}))}},
                            Json::array({release(idp, "v1.0.0"), release(consumer, "v1.0.0")}));
        turn(scheduler, cyclic, 10);
        CHECK_EQ(cyclic.at("attempts").size(), 0U);
        CHECK_EQ(cyclic.at("items").at("producer/1.0.0/native").at("reason"), "cycle");
    }

    void targetsAndReceipts() {
        Options options;
        options.batch = 0;
        const Scheduler scheduler(options);
        auto producer = package("producer"),
             dependent = package("consumer", Json::array({Json{{"package", "producer"}, {"version", ">=1"}}}));
        producer["targets"] = Json::array({"native", "alt", "third"});
        dependent["targets"] = Json::array({"native", "alt", "third", "absent"});
        auto catalog = Json{{"native", target}};
        for (const auto* id : {"alt", "third", "absent"}) {
            catalog[id] = target;
            catalog[id]["id"] = id;
        }
        const auto tags = Json::array({release(idp, "v1.0.0"), release(consumer, "v1.0.0")});
        const Json refs{{"refs/snapshots/feed/pre-x", head}};
        auto state = initialState();
        CHECK_EQ(scheduler.declarations(state, catalog, {{idp, producer}, {consumer, dependent}}, tags).code, 200);
        scheduler.feed(state, head, refs, Json::array(), 1);

        auto unsupported = state;
        auto outOfRange = producer;
        outOfRange["releases"] = ">=2";
        CHECK_EQ(scheduler.declarations(unsupported, catalog, {{idp, outOfRange}, {consumer, dependent}}, tags).code, 200);
        turn(scheduler, unsupported, 9);
        scheduler.turn(unsupported, 9); // Reconsider waiting after the provider's unsupported condition is known.
        CHECK_EQ(unsupported.at("items").at("consumer/1.0.0/native").at("reason"), "range unsatisfiable");

        auto mismatch = state;
        auto needsNewer = dependent;
        needsNewer["depends_on"][0]["version"] = ">=2";
        CHECK_EQ(scheduler.declarations(mismatch, catalog, {{idp, producer}, {consumer, needsNewer}}, tags).code, 200);
        turn(scheduler, mismatch, 9);
        CHECK_EQ(mismatch.at("items").at("consumer/1.0.0/native").at("reason"), "range unsatisfiable");

        turn(scheduler, state, 10);
        CHECK_EQ(state.at("attempts").size(), 3U);
        CHECK_EQ(state.at("items").at("consumer/1.0.0/absent").at("reason"), "target missing");
        const auto requestAt = [&](const std::string& targetId) {
            return state.at("items").at("producer/1.0.0/" + targetId).at("request").get<std::string>();
        };
        const auto native = requestAt("native");
        CHECK_EQ(scheduler.claim(state, false, native, owner(1, 1), 11).code, 200);
        auto failure = owner(1, 1);
        failure.update(Json{{"status", "finished"}, {"outcome", "failed"}, {"reason", "fixture target failure"}});
        CHECK_EQ(scheduler.report(state, false, native, failure, 11).code, 200);
        scheduler.observed(state, attemptKey(native, 1), {{"id", 1}, {"status", "completed"}, {"conclusion", "failure"}}, 11);
        CHECK_EQ(scheduler.action(state, "dispatch-paused", {{"paused", true}}).code, 200);
        const auto alt = requestAt("alt"), third = requestAt("third");
        complete(scheduler, state, alt, 1, 2, 12, true);
        complete(scheduler, state, third, 1, 3, 12, false);
        turn(scheduler, state, 12);
        CHECK(view(state, options).at("publications").empty());
        CHECK_EQ(state.at("items").at("producer/1.0.0/alt").at("state"), "built");
        CHECK_EQ(state.at("items").at("consumer/1.0.0/native").at("reason"), "prerequisite failed");
        CHECK_EQ(scheduler.action(state, "dispatch-paused", {{"paused", false}}).code, 200);
        scheduler.turn(state, 13);
        const auto publication = view(state, options).at("publications").at(0).at("id").get<std::string>();
        const auto publicationKey = attemptKey(publication, 1);
        const auto plan = Json::parse(state.at("attempts").at(publicationKey).at("document").get<std::string>());
        CHECK_EQ(plan.at("items").size(), 2U);
        CHECK_EQ(scheduler.claim(state, true, publication, owner(1, 4), 13).code, 200);
        scheduler.observed(state, publicationKey, {{"id", 4}, {"status", "completed"}, {"conclusion", "success"}}, 14);
        const Json receipts = Json::array({Json{{"publication", publication}, {"attempt", 1}, {"request", alt}, {"outcome", "published"}},
                                           Json{{"publication", publication},
                                                {"attempt", 1},
                                                {"request", third},
                                                {"outcome", "failed"},
                                                {"reason", "immutable-conflict"}}});
        auto published = output("producer", "1.0.0", 1, 14);
        published["target"] = catalog.at("alt");
        const std::string newHead(40, 'b');
        CHECK_EQ(scheduler.admit(state, consumer, "v1.1.0", head).code, 200);
        scheduler.turn(state, 14); // Open the pass before the provider's publication outcome arrives.
        scheduler.feed(state,
                       newHead,
                       {{"refs/snapshots/feed/" + publication, newHead}},
                       {{"publication", publication}, {"entries", Json::array({published})}, {"receipts", receipts}},
                       14);
        scheduler.turn(state, 14);
        CHECK_EQ(state.at("items").at("consumer/1.0.0/alt").at("state"), "enrolled");
        turn(scheduler, state, 14);
        CHECK_EQ(state.at("items").at("producer/1.0.0/alt").at("state"), "published");
        CHECK_EQ(state.at("items").at("producer/1.0.0/third").at("state"), "failed");
        CHECK_EQ(state.at("items").at("producer/1.0.0/third").at("reason"), "immutable-conflict");
        CHECK_EQ(state.at("items").at("consumer/1.0.0/native").at("reason"), "prerequisite failed");
        CHECK_EQ(state.at("items").at("consumer/1.0.0/third").at("reason"), "prerequisite failed");
        const auto selected = state.at("items").at("consumer/1.0.0/alt").at("request").get<std::string>();
        const auto document = Json::parse(state.at("attempts").at(attemptKey(selected, 1)).at("document").get<std::string>());
        CHECK_EQ(document.at("inputs").at("closure").at(0).at("target"), "alt");
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
        scheduler.turn(state, 23);  // Close the cutoff that preceded Retry.
        turn(scheduler, state, 24); // Commit and fill the pass containing Retry.
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
        scheduler.feed(
            state,
            std::string(40, 'b'),
            {{ref, std::string(40, 'b')}},
            {{"publication", id}, {"entries", Json::array({output("producer", "1.0.0", 1, 33)})}, {"receipts", Json::array({receipt})}},
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

    void retiringBinding() {
        Options options;
        options.batch = 0;
        const Scheduler scheduler(options);
        const auto declaration = package("consumer", Json::array({Json{{"package", "producer"}, {"version", "==1"}}}));
        const auto transitive = package("transitive", Json::array({Json{{"package", "bridge"}, {"version", "==1"}}}));
        auto state = ready(
            scheduler, {{consumer, declaration}, {idp, transitive}}, Json::array({release(consumer, "v1.0.0"), release(idp, "v1.0.0")}));
        state["entries"] =
            Json::array({output("producer", "1.0.0", 1, 1), output("producer", "2.0.0", 1, 2), output("producer", "3.0.0", 1, 3)});
        auto bound = state.at("entries").at(0);
        bound["target"] = "native";
        bound["repository"] = bound.at("repo");
        bound.erase("repo");
        bound.erase("closure");
        auto bridge = output("bridge", "1.0.0", 1, 4);
        bridge["closure"] = Json::array({bound});
        state["entries"].push_back(bridge);
        const auto now = 8 * 86400;
        state["retirement_at"] = now;
        turn(scheduler, state, now);
        CHECK_EQ(state.at("attempts").size(), 1U);
        const auto publication = view(state, options).at("publications").at(0).at("id").get<std::string>();
        const auto plan = Json::parse(state.at("attempts").at(attemptKey(publication, 1)).at("document").get<std::string>());
        CHECK_EQ(plan.at("retirements"), Json::array({Json::array({"producer", "1.0.0", 1, "native"})}));
        CHECK_EQ(state.at("items").at("consumer/1.0.0/native").at("state"), "enrolled");
        CHECK_EQ(state.at("items").at("transitive/1.0.0/native").at("state"), "enrolled");
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
                           {{"publication", publication}, {"entries", Json::array()}, {"receipts", Json::array({receipt})}},
                           15);
            turn(scheduler, state, 15);
            const auto item = state["items"].begin().value();
            CHECK_EQ(item.at("revision"), change ? 2 : 1);
            CHECK_EQ(item.at("attempt"), change ? 1 : 2);
            const auto key = attemptKey(item.at("request").get<std::string>(), item.at("attempt").get<unsigned>());
            CHECK_EQ(Json::parse(state.at("attempts").at(key).at("document").get<std::string>()).at("inputs") == frozen, !change);
            CHECK_EQ(scheduler.claim(state, false, first, owner(1, 51), 16).code, 409);
        }
    }

    void restoreExpiry() {
        Options options;
        options.batch = 0;
        const Scheduler scheduler(options);
        auto state = ready(scheduler, {{idp, package("producer")}}, Json::array({release(idp, "v1.0.0")}));
        const std::string preX(40, 'b'), publishedHead(40, 'c'), expiredHead(40, 'd');
        const std::string prefix = "refs/snapshots/feed/";
        Json snapshots{{prefix + "pre-x", preX}, {prefix + "p.1", head}};
        CHECK_EQ(
            scheduler.feed(state, head, snapshots, {{"publication", "p.1"}, {"entries", Json::array()}, {"receipts", Json::array()}}, 2)
                .code,
            200);
        turn(scheduler, state, 3);
        const auto itemKey = "producer/1.0.0/native";
        const auto request = state.at("items").at(itemKey).at("request").get<std::string>();
        const auto original = state.at("attempts").at(attemptKey(request, 1)).at("document");
        complete(scheduler, state, request, 1, 10, 4, true);
        turn(scheduler, state, 4);
        const auto publication = view(state, options).at("publications").back().at("id").get<std::string>();
        const auto plan = Json::parse(state.at("attempts").at(attemptKey(publication, 1)).at("document").get<std::string>());
        CHECK_EQ(scheduler.claim(state, true, publication, owner(1, 11), 4).code, 200);
        scheduler.observed(state, attemptKey(publication, 1), {{"id", 11}, {"status", "completed"}, {"conclusion", "success"}}, 5);
        snapshots.erase(prefix + "p.1");
        snapshots[prefix + publication] = publishedHead;
        const Json published{{"publication", publication}, {"attempt", 1}, {"request", request}, {"outcome", "published"}};
        CHECK_EQ(scheduler
                     .feed(state,
                           publishedHead,
                           snapshots,
                           {{"publication", publication}, {"entries", plan.at("items")}, {"receipts", Json::array({published})}},
                           5)
                     .code,
                 200);
        CHECK_EQ(state.at("items").at(itemKey).at("state"), "published");
        CHECK_EQ(scheduler.feed(state, preX, snapshots, Json::array(), 7, RestoreProof{prefix + "pre-x", 6}).code, 200);
        CHECK_EQ(state.at("paused"), true);
        CHECK_EQ(state.at("items").at(itemKey).at("state"), "built");
        CHECK_EQ(scheduler.action(state, "dispatch-paused", {{"paused", false}}).code, 200);
        turn(scheduler, state, 8);
        const auto retry = view(state, options).at("publications").back().at("id").get<std::string>();
        const auto republish = Json::parse(state.at("attempts").at(attemptKey(retry, 1)).at("document").get<std::string>());
        CHECK_EQ(republish.at("items"), plan.at("items"));
        CHECK_EQ(scheduler.claim(state, true, retry, owner(1, 12), 8).code, 200);
        scheduler.observed(state, attemptKey(retry, 1), {{"id", 12}, {"status", "completed"}, {"conclusion", "success"}}, 9);
        snapshots[prefix + retry] = expiredHead;
        auto expired = published;
        expired.update(Json{{"publication", retry}, {"outcome", "failed"}, {"failure", "artifact-expired"}});
        CHECK_EQ(scheduler
                     .feed(state,
                           expiredHead,
                           snapshots,
                           {{"publication", retry}, {"entries", Json::array()}, {"receipts", Json::array({expired})}},
                           9)
                     .code,
                 200);
        turn(scheduler, state, 10);
        const auto newer = state.at("items").at(itemKey);
        const auto newerRequest = newer.at("request").get<std::string>();
        const auto replacement = Json::parse(state.at("attempts").at(attemptKey(newerRequest, 1)).at("document").get<std::string>());
        CHECK_EQ(newer.at("revision"), 2U);
        CHECK_EQ(newer.at("attempt"), 1U);
        CHECK_EQ(replacement.at("inputs").at("feed_commit"), expiredHead);
        CHECK_EQ(state.at("attempts").at(attemptKey(request, 1)).at("document"), original);
        CHECK(!state.at("snapshots").contains(prefix + "p.1"));
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
        CHECK_EQ(scheduler.admit(state, consumer, "v1.1.0", head).code, 200);
        scheduler.turn(state, 3); // Open the pass for the new consumer version.
        CHECK_EQ(scheduler.admit(state, idp, "v1.0.0", head).code, 200);
        scheduler.turn(state, 3); // The late prerequisite must not affect this pass.
        CHECK_EQ(state.at("items").at(item).at("reason"), "closure conflict");
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

    void retryIdentity() {
        const Scheduler scheduler({});
        const std::string key = "producer/1.0.0/native";
        const auto tags = Json::array({release(idp, "v1.0.0")});
        auto declaration = package("producer");
        auto base = ready(scheduler, {{idp, declaration}}, tags);
        turn(scheduler, base, 10);
        const auto request = base.at("items").at(key).at("request").get<std::string>();
        const auto oldKey = attemptKey(request, 1);
        const auto document = base.at("attempts").at(oldKey).at("document");
        CHECK_EQ(scheduler.claim(base, false, request, owner(1, 1), 11).code, 200);
        auto report = owner(1, 1);
        report.update(Json{{"status", "finished"}, {"outcome", "failed"}});
        CHECK_EQ(scheduler.report(base, false, request, report, 12).code, 200);
        scheduler.observed(base, oldKey, {{"id", 1}, {"status", "completed"}, {"conclusion", "failure"}}, 13);
        turn(scheduler, base, 13);
        declaration["tree"] = std::string(40, 'b');
        declaration["ci_commit"] = std::string(40, 'c');
        const Json action{{"item", key}};
        for (const bool requestedBeforeChange : {true, false}) {
            auto state = base;
            if (requestedBeforeChange) {
                CHECK_EQ(scheduler.action(state, "retry", action).code, 200);
            }
            CHECK_EQ(scheduler.declarations(state, {{"native", target}}, {{idp, declaration}}, tags).code, 200);
            if (!requestedBeforeChange) {
                CHECK_EQ(scheduler.action(state, "retry", action).code, 409);
            }
            turn(scheduler, state, 14);
            CHECK_EQ(state.at("items").at(key).at("revision"), 2U);
            CHECK_EQ(state.at("items").at(key).at("attempt"), 1U);
            CHECK_EQ(state.at("attempts").at(oldKey).at("document"), document);
        }
    }

    void revisionFloor() {
        const Scheduler scheduler({});
        const std::string older = "consumer/1.0.0/native", newest = "consumer/2.0.0/native";
        const auto dependency = package("consumer", Json::array({Json{{"package", "producer"}, {"version", ">=1"}}}));
        const Json declarations{{consumer, dependency}};
        auto state = ready(scheduler, declarations, Json::array({release(consumer, "v2.0.0")}));
        CHECK_EQ(scheduler.admit(state, consumer, "v1.0.0", head).code, 200);
        // Floors seeded at cutover from the larger historical allocation for each key.
        state["revisions"] = {{older, std::max(8, 3) + 1}, {newest, std::max(4, 11) + 1}};
        auto binding = output("producer", "1.0.0", 1, 5);
        state["entries"] = Json::array({binding, output("consumer", "1.0.0", 9, 5), output("consumer", "2.0.0", 12, 5)});
        turn(scheduler, state, 10);
        CHECK_EQ(state.at("items").at(older).at("revision"), 9U);
        CHECK_EQ(state.at("items").at(newest).at("revision"), 12U);
        state["items"][older]["state"] = "published";
        state["items"][newest]["state"] = "published";
        state["items"][newest]["identity"] = sha256(Json::array({head, head, target, "Xv1", Json::array({binding.at("identity")})}).dump());
        scheduler.turn(state, 11);
        CHECK_EQ(state.at("items").at(newest).at("revision"), 13U);
        state["items"][newest]["state"] = "published";
        binding = output("producer", "1.0.0", 2, 12);
        state["entries"][0] = binding;
        scheduler.turn(state, 12);
        CHECK_EQ(state.at("items").at(older).at("revision"), 9U);
        CHECK_EQ(state.at("items").at(older).at("state"), "published");
        CHECK_EQ(state.at("items").at(newest).at("revision"), 14U);
        const auto attempts = state.at("attempts").size();
        CHECK_EQ(scheduler.admit(state, consumer, "v2.0.0", head).code, 200);
        scheduler.turn(state, 13);
        CHECK_EQ(state.at("attempts").size(), attempts);
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
            scheduler.turn(state, at);
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

        CHECK_EQ(scheduler.admit(state, idp, "v3.0.0", head).code, 200);
        const auto nextBuild = state.at("items").at("producer/2.0.0/native").at("request").get<std::string>();
        CHECK_EQ(scheduler.claim(state, false, nextBuild, owner(1, 3), 17).code, 200);
        scheduler.observed(state, attemptKey(nextBuild, 1), {{"id", 3}, {"status", "completed"}, {"conclusion", "failure"}}, 18);
        scheduler.feed(state, head, snapshots, Json::array(), 18);
        turn(scheduler, state, 18);
        const auto retried = view(state, options);
        CHECK_EQ(retried.at("reservations").at("publication"), 1U);
        CHECK_EQ(retried.at("publications").back().at("id"), publication);
        CHECK_EQ(retried.at("publications").back().at("attempt"), 2U);
        CHECK_EQ(state.at("items").at("producer/3.0.0/native").at("state"), "enrolled");
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
    } else if (mode == "admissions") {
        admissions();
    } else if (mode == "admission_stress") {
        admissionStress();
    } else if (mode == "active_arrival_stress") {
        activeArrivalStress();
    } else if (mode == "dependency_stress") {
        dependencyStress();
    } else if (mode == "binding") {
        binding();
    } else if (mode == "targets_and_receipts") {
        targetsAndReceipts();
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
    } else if (mode == "retiring_binding") {
        retiringBinding();
    } else if (mode == "expiry") {
        expiry(false);
    } else if (mode == "expiry_before_turn") {
        expiry(true);
    } else if (mode == "restore_expiry") {
        restoreExpiry();
    } else if (mode == "closure") {
        closure();
    } else if (mode == "identity") {
        identity();
    } else if (mode == "retry_identity") {
        retryIdentity();
    } else if (mode == "revision_floor") {
        revisionFloor();
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
