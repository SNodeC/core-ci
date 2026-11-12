#include "StatusJson.h"

#include "core/Status.h"

#include <charconv>
namespace coreci::app {
    namespace {
        std::string quote(const std::string& value) {
            const char* hex = "0123456789abcdef";
            std::string result = "\"";
            for (const unsigned char c : value) {
                if (c == '"' || c == '\\') {
                    result += '\\';
                    result += static_cast<char>(c);
                } else if (c < 32) {
                    result += "\\u00";
                    result += hex[c >> 4];
                    result += hex[c & 15];
                } else {
                    result += static_cast<char>(c);
                }
            }
            result += '"';
            return result;
        }
        std::string object(std::map<std::string, std::string> fields) {
            std::string result = "{";
            for (const auto& [key, value] : fields) {
                result.append(result.size() == 1 ? "" : ",").append(quote(key)).append(":").append(value);
            }
            return result + "}";
        }
        std::string array(const std::vector<std::string>& values) {
            std::string result = "[";
            for (const auto& value : values) {
                result += (result.size() == 1 ? "" : ",") + value;
            }
            return result + "]";
        }
        std::string run(const std::optional<std::uint64_t>& value) {
            return value ? std::to_string(*value) : "null";
        }
    } // namespace
    StatusSnapshot statusSnapshot(const Engine& engine, const std::string& feed) {
        StatusSnapshot result;
        for (const auto& [name, package] : engine.pipeline().packages()) {
            bool included = feed.empty();
            for (const auto& target : package.targets) {
                included = included || engine.pipeline().targets().at(target).feed == feed;
            }
            if (included) {
                result.packages.emplace(name, PackageView{package.sourceRepo, engine.releases().at(name)});
            }
        }
        for (const auto& [unit, status] : statuses(engine)) {
            const auto& targetFeed = engine.pipeline().targets().at(unit.target).feed;
            if (feed.empty() || targetFeed == feed) {
                UnitView view;
                view.feed = targetFeed;
                view.status = status;
                const auto& state = engine.units().at(unit);
                if (state.inflight) {
                    const auto& f = *state.inflight;
                    view.inflight = BuildView{{f.version.text(), f.revision}, f.buildId, f.phase, f.runId, f.attempt};
                }
                if (state.published) {
                    const auto& p = *state.published;
                    view.published = PublicationView{{p.version.text(), p.revision}, p.runId, p.at, p.feedCommit};
                }
                if (state.failure) {
                    const auto& f = *state.failure;
                    view.failure = FailureView{{f.version.text(), f.revision}, f.runId, f.at, f.reason};
                }
                if (!state.inflight && status != "Settled" && engine.releases().at(unit.package)) {
                    view.next = VersionView{engine.releases().at(unit.package)->version.text(), engine.nextRevision(unit)};
                }
                result.units.emplace(unit, std::move(view));
            }
        }
        return result;
    }
    std::string statusJson(const StatusSnapshot& snapshot, std::uint64_t generation) {
        std::vector<std::string> packages, units;
        for (const auto& [name, package] : snapshot.packages) {
            const auto& r = package.released;
            packages.push_back(object({{"name", quote(name)},
                                       {"released", r ? object({{"tag", quote(r->tag)}, {"version", quote(r->version.text())}}) : "null"},
                                       {"sourceRepo",
                                        object({{"branch", quote(package.sourceRepo.branch)},
                                                {"owner", quote(package.sourceRepo.owner)},
                                                {"repo", quote(package.sourceRepo.repo)}})}}));
        }
        for (const auto& [unit, state] : snapshot.units) {
            const auto& status = state.status;
            const auto& targetFeed = state.feed;
            const auto paren = status.find('(');
            const auto detail = paren == std::string::npos ? "" : status.substr(paren + 1, status.size() - paren - 2);
            std::string position = "null";
            if (status.starts_with("AwaitingPublish(")) {
                unsigned n = 0;
                std::from_chars(detail.data() + 9, detail.data() + detail.size(), n);
                position = std::to_string(n);
            }
            std::string inflight = "null", published = "null", failure = "null", next = "null";
            if (state.inflight) {
                const auto& f = *state.inflight;
                const char* phases[] = {"Dispatching", "Building", "AwaitingPublish", "PublishDispatching", "Publishing"};
                inflight = object({{"attempt", std::to_string(f.attempt)},
                                   {"buildId", quote(f.buildId)},
                                   {"phase", quote(phases[static_cast<unsigned>(f.phase)])},
                                   {"revision", std::to_string(f.selection.revision)},
                                   {"runId", run(f.runId)},
                                   {"version", quote(f.selection.version)}});
            }
            if (state.published) {
                const auto& p = *state.published;
                published = object({{"at", std::to_string(p.at)},
                                    {"feedCommit", quote(p.feedCommit)},
                                    {"revision", std::to_string(p.selection.revision)},
                                    {"runId", run(p.runId)},
                                    {"version", quote(p.selection.version)}});
            }
            if (state.failure) {
                const auto& f = *state.failure;
                failure = object({{"at", std::to_string(f.at)},
                                  {"reason", quote(f.reason)},
                                  {"revision", std::to_string(f.selection.revision)},
                                  {"runId", run(f.runId)},
                                  {"version", quote(f.selection.version)}});
            }
            if (state.next) {
                next = object({{"revision", std::to_string(state.next->revision)}, {"version", quote(state.next->version)}});
            }
            units.push_back(object({{"detail", detail.empty() ? "null" : quote(detail)},
                                    {"failure", failure},
                                    {"feed", quote(targetFeed)},
                                    {"inflight", inflight},
                                    {"next", next},
                                    {"package", quote(unit.package)},
                                    {"position", position},
                                    {"published", published},
                                    {"status", quote(status.substr(0, paren))},
                                    {"target", quote(unit.target)}}));
        }
        return object(
            {{"generation", std::to_string(generation)}, {"packages", array(packages)}, {"schema", "1"}, {"units", array(units)}});
    }
} // namespace coreci::app
