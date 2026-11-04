#include "Status.h"

#include <algorithm>

namespace coreci {
    namespace {
        std::string label(const Unit& u) {
            return u.package + "@" + u.target;
        }
        std::string quoted(const std::string& s) {
            std::string result = "\"";
            for (const char c : s) {
                if (c == '\\' || c == '"') {
                    result += '\\';
                    result += c;
                } else if (c == '\n') {
                    result += "\\n";
                } else if (c == '\r') {
                    result += "\\r";
                } else {
                    result += c;
                }
            }
            result += '"';
            return result;
        }
    } // namespace
    std::map<Unit, std::string> statuses(const Engine& engine) {
        std::map<Unit, std::string> result;
        const auto derived = engine.derive();
        for (const auto& [unit, state] : engine.units()) {
            const auto& d = derived.at(unit);
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
                        status = f.runId ? "Building(run " + std::to_string(*f.runId) + ")" : "Building";
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
    std::string statusText(const Engine& engine) {
        const auto states = statuses(engine);
        std::string result = "PACKAGE\tTARGET\tRELEASE\tPUBLISHED\tSTATUS\n";
        for (const auto& [target, definition] : engine.pipeline().targets()) {
            for (const auto& package : engine.pipeline().topologicalOrder(target)) {
                const Unit u{package, target};
                const auto& r = engine.releases().at(package);
                const auto& p = engine.units().at(u).published;
                result += package + "\t" + target + "\t" + (r ? r->version.text() : "-") + "\t";
                result += (p ? p->version.text() + "-" + std::to_string(p->revision) : "-") + "\t" + states.at(u) + "\n";
            }
        }
        return result;
    }
    std::string statusDot(const Engine& engine) {
        std::string result = "digraph coreci {\n";
        for (const auto& [u, status] : statuses(engine)) {
            std::string color = "gray";
            if (status == "Settled") {
                color = "green";
            } else if (status.starts_with("Failed") || status.starts_with("Blocked")) {
                color = status.starts_with("Failed") ? "red" : "orange";
            } else if (status == "Ready") {
                color = "cyan";
            } else if (engine.units().at(u).inflight) {
                color = "yellow";
            }
            result +=
                "  " + quoted(label(u)) + " [label=" + quoted(label(u) + "\n" + status) + ", style=filled, fillcolor=" + color + "];\n";
            for (const auto& q : engine.pipeline().prerequisites(u.package)) {
                result += "  " + quoted(label({q, u.target})) + " -> " + quoted(label(u)) + ";\n";
            }
        }
        result += "}\n";
        return result;
    }
} // namespace coreci
