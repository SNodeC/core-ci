#include "Check.h"
#include "app/StatusJson.h"
#include "migration/legacy/EventSql.h"
#include "migration/legacy/StatusJson.h"
#include "persistence/StateSql.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace coreci::legacy;
using coreci::Pipeline;
using Json = nlohmann::json;

#include "MigrationFixture.h"

Engine history(const std::filesystem::path& directory,
               std::int64_t configuredDeadline,
               std::vector<std::pair<std::uint64_t, Event>>* saved = nullptr) {
    const auto plan = readJson(directory / "legacy-w964-plan.json");
    const auto origins = readJson(directory / "production/origins.json");
    Pipeline::Builder builder;
    std::map<std::string, Json> packages;
    for (const auto& target : plan.at("targets")) {
        builder.target({target.at("id")});
        for (const auto& entry : target.at("units")) {
            packages.emplace(entry.at("package"), entry);
        }
    }
    std::vector<TagObserved> tags;
    constexpr std::int64_t start = 1794852832;
    for (const auto& [name, entry] : packages) {
        const std::string repo = entry.at("repo");
        const auto slash = repo.find('/');
        const auto declaration = readJson(directory / "production" / (repo.substr(slash + 1) + ".json"));
        builder.package({name,
                         {repo.substr(0, slash), repo.substr(slash + 1), origins.at(repo.substr(slash + 1)).at("branch")},
                         declaration.at("targets").get<std::set<std::string>>(),
                         declaration.at("depends_on").get<std::vector<std::string>>(),
                         entry.at("revision").get<unsigned>() - 1});
        tags.push_back({start - 6, name, entry.at("tag"), entry.at("commit"), Origin::Reconcile});
    }
    Engine engine({5, configuredDeadline, 8, 600});
    const auto apply = [&](const Event& event, std::uint64_t seq) {
        if (saved) {
            saved->push_back({seq, event});
        }
        engine.handle(event, seq);
    };
    apply(GraphChanged{start - 6, builder.build(), tags}, 963);
    apply(Tick{start - 1}, 964);
    apply(WaveStarted{start, "w964", 43926583894, 1}, 965);
    std::uint64_t seq = 966;
    const auto outcomes = [&](const Wave& wave, unsigned built, unsigned failed, unsigned building, std::int64_t at) {
        unsigned index = 0;
        for (const auto& [target, entries] : wave.plan) {
            for (const auto& entry : entries) {
                if (entry.build) {
                    if (index < built) {
                        apply(UnitBuildSucceeded{at, wave.id, entry.buildId, "artifact"}, seq++);
                    } else if (index < built + failed) {
                        apply(UnitBuildFailed{at, wave.id, entry.buildId, "compiler"}, seq++);
                    } else if (index < built + failed + building) {
                        apply(UnitBuildStarted{at, wave.id, entry.buildId}, seq++);
                    }
                    ++index;
                }
            }
        }
        CHECK_EQ(index, 173U);
    };
    outcomes(*engine.latestWave(), 148, 24, 1, start + 23380);
    apply(Tick{start + 23399}, 1300);
    apply(Tick{start + 23999}, 1337);
    apply(TagObserved{start + 24000, "core-ci", "v0.1.5", std::string(40, 'c'), Origin::Webhook}, 1338);
    apply(Tick{start + 24005}, 1339);
    apply(WaveStarted{start + 24006, "w1339", 43991561301, 1}, 1340);
    CHECK(engine.waves().contains("w1339"));
    seq = 1341;
    outcomes(*engine.latestWave(), 148, 10, 3, start + 47385);
    apply(Tick{start + 47405}, 1700);
    apply(Tick{start + 48005}, 1701);
    apply(Tick{start + 48010}, 1702);
    apply(Resumed{start + 48011}, 1703);
    return engine;
}

void publishedHistory() {
    Pipeline::Builder builder;
    builder.target({"t"}).package({"a", {"Owner", "a", "main"}, {"t"}, {}, 10}).package({"b", {"Owner", "b", "main"}, {"t"}, {"a"}, 100});
    Engine old({0, 100, 3, 0});
    std::uint64_t seq = 0;
    const auto apply = [&](const Event& event) {
        old.handle(event, ++seq);
    };
    const std::string sha(40, 'a'), feed(40, 'b');
    apply(GraphChanged{100, builder.build(), {{100, "a", "v1.0.0", sha, Origin::Reconcile}, {100, "b", "v1.0.0", sha, Origin::Reconcile}}});
    const auto first = *old.latestWave();
    apply(WaveStarted{101, first.id, 7001, 2});
    for (const auto& entry : first.plan.at("t")) {
        apply(UnitBuildSucceeded{102, first.id, entry.buildId, "unit-artifact"});
        apply(UnitPublishSucceeded{103, first.id, entry.buildId, feed});
    }
    apply(WaveFinished{104, first.id, 7001, "success"});
    apply(RebuildRequested{105, "a", "t"});
    const auto second = *old.latestWave();
    apply(WaveStarted{106, second.id, 7002, 3});
    for (const auto& entry : second.plan.at("t")) {
        apply(UnitBuildSucceeded{107, second.id, entry.buildId, "unit-artifact"});
        if (entry.unit.package == "a") {
            apply(UnitPublishSucceeded{108, second.id, entry.buildId, std::string(40, 'c')});
        } else {
            apply(UnitPublishFailed{108, second.id, entry.buildId, "feed failed"});
        }
    }
    apply(WaveFinished{109, second.id, 7002, "failure"});
    apply(TagObserved{110, "a", "v2.0.0", std::string(40, 'd'), Origin::Webhook});
    const coreci::WaveController converted({}, old.convert());
    CHECK(snapshot(converted) == snapshot(old));
    const coreci::WaveController loaded({}, coreci::persistence::decodeState(coreci::persistence::encodeState(converted.stored())));
    CHECK(snapshot(loaded) == snapshot(old));
    const auto& a = *loaded.units().at({"a", "t"}).published;
    const auto& b = *loaded.units().at({"b", "t"}).published;
    CHECK_EQ(a.revision, 12U);
    CHECK_EQ(*a.runId, 7002U);
    CHECK_EQ(*b.runId, 7001U);
    CHECK_EQ(b.revision, 101U);
    CHECK_EQ(b.inputs.at("a").revision, 11U);
    CHECK_EQ(loaded.nextRevision({"a", "t"}), 12U);
    CHECK_EQ(loaded.nextRevision({"b", "t"}), 104U);
}

int main(int argc, char** argv) {
    publishedHistory();
    CHECK(argc == 2 || argc == 3);
    if (argc == 2 || argc == 3) {
        const std::filesystem::path directory(argv[1]);
        const auto baseline = readJson(directory / "legacy-replay-baseline.json");
        for (const std::int64_t duration : {23400, 86400, 60}) {
            const auto engine = history(directory, duration);
            CHECK(snapshot(engine) == baseline);
            const coreci::WaveController converted({}, engine.convert());
            CHECK(snapshot(converted) == baseline);
            const auto loaded = coreci::persistence::decodeState(coreci::persistence::encodeState(converted.stored()));
            CHECK(loaded == converted.stored());
            CHECK(snapshot(coreci::WaveController({}, loaded)) == baseline);
            coreci::WaveController driven({}, loaded);
            driven.handle(
                coreci::TagObserved{*converted.time() + 1, "core-ci", "v999.0.0", std::string(40, 'd'), coreci::Origin::Reconcile});
            CHECK(driven.publications().empty());
            CHECK(engine.waves().at("w964").phase == WavePhase::Finished);
            CHECK(engine.waves().at("w1339").phase == WavePhase::Finished);
            for (const auto& [unit, state] : engine.units()) {
                for (const auto& [version, revision] : state.lastRevision) {
                    CHECK(revision >= baseline.at("units").at(unit.package + "/" + unit.target).at("revisions").at(version.text()));
                }
            }
        }
    }
    if (argc == 3 && std::string(argv[2]) == "--sql" && checkFailures == 0) {
        std::vector<std::pair<std::uint64_t, Event>> events;
        history(argv[1], 23400, &events);
        for (const auto& [seq, event] : events) {
            auto statement = std::get<std::string>(persistence::insertSql(event));
            const auto columns = statement.find('(');
            statement.insert(columns + 1, "seq,");
            const auto values = statement.find("VALUES (");
            statement.insert(values + 8, std::to_string(seq) + ",");
            std::cout << statement << ";\n";
            if (std::holds_alternative<GraphChanged>(event)) {
                const auto row = std::get<persistence::EventRow>(persistence::encode(event, seq));
                for (auto graph : persistence::graphInsertSql(*row.graph)) {
                    auto at = graph.find("LAST_INSERT_ID()");
                    while (at != std::string::npos) {
                        graph.replace(at, 16, std::to_string(seq));
                        at = graph.find("LAST_INSERT_ID()");
                    }
                    std::cout << graph << ";\n";
                }
            }
        }
    }
    return checkResult();
}
