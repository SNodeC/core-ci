#pragma once
#include "Check.h"
#include "migration/legacy/EventRow.h"
#include "migration/legacy/StatusJson.h"

#include <algorithm>
#include <stdexcept>

namespace coreci::legacy {
    inline Pipeline testPipeline(unsigned base = 0) {
        Pipeline::Builder b;
        b.target({"t1"}).target({"t2"});
        for (const auto& name : {"a", "b", "c", "d", "e", "z"}) {
            const std::map<std::string, std::vector<std::string>> prerequisites{
                {"a", {}}, {"b", {"a"}}, {"c", {"a"}}, {"d", {"b", "c"}}, {"e", {"d"}}, {"z", {}}};
            b.package({name,
                       {"owner", name, "trunk"},
                       name == std::string("e") ? std::set<std::string>{"t1"} : std::set<std::string>{"t1", "t2"},
                       prerequisites.at(name),
                       base});
        }
        return b.build();
    }
    inline std::string hash(char value = 'a') {
        return std::string(40, value);
    }
    struct History {
        Options options;
        Engine engine;
        std::vector<Event> events;
        explicit History(Options options = {5, 100, 3, 0})
            : options(options)
            , engine(options) {
        }
        std::vector<Command> apply(Event event) {
            std::vector<Command> commands;
            const auto normalized = persistence::normalize(event);
            CHECK(std::holds_alternative<Event>(normalized));
            if (std::holds_alternative<Event>(normalized)) {
                event = engine.record(std::get<Event>(normalized));
                if (std::holds_alternative<Accept>(engine.classify(event))) {
                    events.push_back(event);
                    const auto row = std::get<persistence::EventRow>(persistence::encode(event, events.size()));
                    CHECK(std::get<Event>(persistence::decode(row)) == event);
                    engine.handle(event, events.size());
                    commands = engine.commands();
                } else {
                    engine.handle(event, events.size() + 1);
                }
                Engine replay(options);
                for (std::size_t n = 0; n < events.size(); ++n) {
                    replay.handle(events[n], n + 1);
                }
                CHECK(replay.units() == engine.units());
                CHECK(replay.waves() == engine.waves());
                CHECK(replay.releases() == engine.releases());
                CHECK(replay.commands() == engine.commands());
                CHECK(replay.nextDeadline() == engine.nextDeadline());
                CHECK(app::statusSnapshot(replay) == app::statusSnapshot(engine));
            }
            return commands;
        }
        void load(unsigned base = 0, std::int64_t at = 100) {
            const auto graph = testPipeline(base);
            std::vector<TagObserved> tags;
            for (const auto& [name, pkg] : graph.packages()) {
                tags.push_back({at, name, "v1.0.0", hash(), Origin::Reconcile});
            }
            apply(GraphChanged{at, graph, tags});
        }
        const Wave& wave() const {
            if (!engine.latestWave()) {
                throw std::runtime_error("test expected a wave");
            }
            return *engine.latestWave();
        }
        void complete(std::int64_t at = 110) {
            const auto active = wave();
            apply(WaveStarted{at, active.id, 42, 2});
            for (const auto& [target, entries] : active.plan) {
                for (const auto& entry : entries) {
                    if (entry.build) {
                        apply(UnitBuildStarted{at, active.id, entry.buildId});
                        apply(UnitBuildSucceeded{at + 1, active.id, entry.buildId, "artifact"});
                        apply(UnitPublishSucceeded{at + 2, active.id, entry.buildId, hash('b')});
                    }
                }
            }
            apply(WaveFinished{at + 3, active.id, 42, "success"});
        }
    };
} // namespace coreci::legacy
