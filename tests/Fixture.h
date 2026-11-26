#pragma once
#include "Check.h"
#include "app/StatusJson.h"
#include "persistence/StateSql.h"

#include <algorithm>
#include <stdexcept>

namespace coreci {
    inline Pipeline testPipeline(unsigned base = 0) {
        Pipeline::Builder b;
        b.target({"t1"}).target({"t2"});
        for (const auto& name : {"a", "b", "c", "d", "e", "z"}) {
            const std::map<std::string, std::vector<std::string>> prerequisites{
                {"a", {}}, {"b", {"a"}}, {"c", {"a"}}, {"d", {"b", "c"}}, {"e", {"d"}}, {"z", {}}};
            b.package({name,
                       {"owner", name, "trunk"},
                       name == std::string("e") ? std::set<std::string>{"t1"} : std::set<std::string>{"t1", "t2"},
                       requirements(prerequisites.at(name)),
                       base});
        }
        return b.build();
    }
    inline std::string hash(char value = 'a') {
        return std::string(40, value);
    }
    struct History {
        Options options;
        WaveController engine;
        explicit History(Options options = {5, 100, 3, 0})
            : options(options)
            , engine(options) {
        }
        std::vector<Command> apply(const Event& event) {
            engine.handle(event);
            const auto state = persistence::decodeState(persistence::encodeState(engine.stored()));
            CHECK(state == engine.stored());
            WaveController restarted(options, state);
            CHECK(restarted.commands() == engine.commands());
            CHECK(restarted.nextDeadline() == engine.nextDeadline());
            CHECK(app::statusSnapshot(restarted) == app::statusSnapshot(engine));
            return engine.commands();
        }
        void load(unsigned base = 0, std::int64_t at = 100) {
            const auto graph = testPipeline(base);
            std::vector<TagObserved> tags;
            for (const auto& [name, package] : graph.packages()) {
                tags.push_back({at, name, "v1.0.0", hash(), Origin::Reconcile});
            }
            apply(GraphChanged{at, graph, tags});
        }
        const Wave& wave() const {
            if (!engine.latestWave()) {
                throw std::runtime_error("expected wave");
            }
            return *engine.latestWave();
        }
        const Publication& publication() const {
            const Publication* result = nullptr;
            for (const auto& [id, publication] : engine.publications()) {
                if (!result || std::stoull(id.substr(1)) > std::stoull(result->run.id.substr(1))) {
                    result = &publication;
                }
            }
            if (!result) {
                throw std::runtime_error("expected publication");
            }
            return *result;
        }
        void publish(std::int64_t at) {
            const auto p = publication();
            apply(PublicationStarted{at, p.run.id, 314, 1});
            for (const auto& item : p.items) {
                for (const auto& id : item.buildIds) {
                    apply(UnitPublishSucceeded{at + 1, p.run.id, id, hash('b')});
                }
            }
            apply(PublicationFinished{at + 2, p.run.id, 314, "success"});
        }
        void complete(std::int64_t at = 110) {
            const auto w = wave();
            apply(WaveStarted{at, w.id, 42, 2});
            for (const auto& [target, entries] : w.plan) {
                for (const auto& entry : entries) {
                    if (entry.build) {
                        apply(UnitBuildStarted{at, w.id, entry.buildId});
                        apply(UnitBuildSucceeded{at + 1, w.id, entry.buildId});
                    }
                }
                apply(TargetFinished{at + 2, w.id, target, "artifact-" + target});
                if (publication().run.phase != WavePhase::Finished) {
                    publish(at + 3);
                }
            }
            apply(WaveFinished{at + 6, w.id, 42, "success"});
        }
    };
} // namespace coreci
