#include "Status.h"

#include <algorithm>

namespace coreci {
    namespace {
        std::string label(const Unit& u) {
            return u.package + "@" + u.target;
        }
    } // namespace
    std::map<Unit, std::string> statuses(const Engine& engine) {
        std::map<Unit, std::string> result;
        const auto derived = engine.derive();
        for (const auto& [unit, d] : derived) {
            const auto& state = engine.units().at(unit);
            std::string status;
            if (!engine.releases().at(unit.package)) {
                status = "NoRelease";
            } else if (state.inflight) {
                const auto& f = *state.inflight;
                switch (f.phase) {
                    case Phase::Dispatching:
                        status = "Dispatching";
                        break;
                    case Phase::Building:
                        status = state.build.runId ? "Building(run " + std::to_string(*state.build.runId) + ")" : "Building";
                        break;
                    case Phase::AwaitingPublish: {
                        const auto& feed = engine.pipeline().targets().at(unit.target).feed;
                        const auto& queue = engine.feeds().at(feed).queue;
                        const auto pos = std::find(queue.begin(), queue.end(), unit) - queue.begin() + 1;
                        status = "AwaitingPublish(position " + std::to_string(pos) + " on " + feed + ")";
                        break;
                    }
                    case Phase::PublishDispatching:
                    case Phase::Publishing:
                        status = "Publishing";
                        break;
                }
            } else if (d.settled) {
                status = "Settled";
            } else if (d.failed) {
                status = "Failed(" + state.failure->reason + ")";
            } else if (d.blocker) {
                status = "Blocked(by " + label(*d.blocker) + ")";
            } else if (d.ready) {
                status = "Ready";
            } else if (d.waiting) {
                status = "Waiting(on " + label(*d.waiting) + ")";
            }
            result.emplace(unit, std::move(status));
        }
        return result;
    }
} // namespace coreci
