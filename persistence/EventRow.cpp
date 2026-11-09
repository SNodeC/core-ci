#include "EventRow.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace coreci::persistence {
    namespace {
        constexpr std::array<std::string_view, std::variant_size_v<Event>> names{"TagObserved",
                                                                                 "DispatchAccepted",
                                                                                 "DispatchRejected",
                                                                                 "BuildStarted",
                                                                                 "BuildSucceeded",
                                                                                 "BuildFailed",
                                                                                 "PublishSucceeded",
                                                                                 "PublishFailed",
                                                                                 "RetryRequested",
                                                                                 "RebuildRequested",
                                                                                 "Tick",
                                                                                 "Resumed",
                                                                                 "GraphChanged"};
        struct Character {
            std::size_t bytes;
            bool valid;
        };
        Character character(const std::string& text, std::size_t at) {
            const auto first = static_cast<unsigned char>(text[at]);
            const std::size_t width =
                first < 0x80
                    ? 1
                    : (first >= 0xc2 && first <= 0xdf ? 2
                                                      : (first >= 0xe0 && first <= 0xef ? 3 : (first >= 0xf0 && first <= 0xf4 ? 4 : 0)));
            std::size_t bytes = 1;
            bool following = width != 0;
            while (following && bytes < width && at + bytes < text.size()) {
                const auto next = static_cast<unsigned char>(text[at + bytes]);
                following = next >= 0x80 && next <= 0xbf;
                if (bytes == 1) {
                    following = following && !(first == 0xe0 && next < 0xa0) && !(first == 0xed && next >= 0xa0) &&
                                !(first == 0xf0 && next < 0x90) && !(first == 0xf4 && next >= 0x90);
                }
                if (following) {
                    ++bytes;
                }
            }
            return {bytes, width != 0 && bytes == width};
        }
        std::string identifier(const std::string& text, std::size_t maximum, const std::string& name) {
            std::string error;
            std::size_t count = 0;
            for (std::size_t at = 0; at < text.size() && error.empty();) {
                const auto c = character(text, at);
                if (!c.valid || text[at] == '\0') {
                    error = name + ": invalid UTF-8 or NUL";
                }
                ++count;
                at += c.bytes;
            }
            if (error.empty() && (count == 0 || count > maximum)) {
                error = name + ": must contain 1.." + std::to_string(maximum) + " characters";
            }
            return error;
        }
        std::string freeText(const std::string& text) {
            std::string result;
            bool full = false;
            for (std::size_t at = 0; at < text.size() && !full;) {
                const auto c = character(text, at);
                const std::string part = c.valid ? (text[at] == '\0' ? std::string{} : text.substr(at, c.bytes)) : "\xef\xbf\xbd";
                full = result.size() + part.size() > 4096;
                if (!full) {
                    result += part;
                }
                at += c.bytes;
            }
            return result;
        }
        bool sha(const std::string& text) {
            return text.size() == 40 && std::all_of(text.begin(), text.end(), [](char c) {
                       return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                   });
        }
        EventRow mapped(const Event& event, std::uint64_t seq) {
            EventRow row;
            row.seq = seq;
            row.eventType = names.at(event.index());
            std::visit(
                [&](const auto& e) {
                    row.eventAt = e.at;
                    if constexpr (requires { e.pipeline; }) {
                        row.graph = GraphRows{};
                        for (const auto& [id, feed] : e.pipeline.feeds()) {
                            row.graph->feeds.push_back(feed);
                        }
                        for (const auto& [name, pkg] : e.pipeline.packages()) {
                            auto package = pkg;
                            package.targets.clear();
                            package.prerequisites.clear();
                            row.graph->packages.push_back(package);
                            for (const auto& target : pkg.targets) {
                                row.graph->targets.push_back({name, target, e.pipeline.targets().at(target).feed});
                            }
                            for (const auto& q : pkg.prerequisites) {
                                row.graph->prerequisites.push_back({name, q});
                            }
                        }
                    }
                    if constexpr (requires { e.package; }) {
                        row.package = e.package;
                    }
                    if constexpr (requires { e.target; }) {
                        row.target = e.target;
                    }
                    if constexpr (requires { e.tag; }) {
                        row.tag = e.tag;
                    }
                    if constexpr (requires { e.sourceSha; }) {
                        row.sourceSha = e.sourceSha;
                        row.origin = e.origin == Origin::Webhook ? "webhook" : "reconcile";
                    }
                    if constexpr (requires { e.buildId; }) {
                        row.buildId = e.buildId;
                    }
                    if constexpr (requires { e.kind; }) {
                        row.kind = e.kind == Kind::Build ? "build" : "publish";
                        row.attempt = e.attempt;
                    }
                    if constexpr (requires { e.httpStatus; }) {
                        row.httpStatus = e.httpStatus;
                        row.message = e.message;
                    }
                    if constexpr (requires { e.runId; }) {
                        row.runId = e.runId;
                    }
                    if constexpr (requires { e.runAttempt; }) {
                        row.runAttempt = e.runAttempt;
                    }
                    if constexpr (requires { e.artifact; }) {
                        row.artifact = e.artifact;
                    }
                    if constexpr (requires { e.feedCommit; }) {
                        row.feedCommit = e.feedCommit;
                    }
                    if constexpr (requires { e.reason; }) {
                        row.reason = e.reason;
                    }
                },
                event);
            return row;
        }
        template <class T>
        const T& required(const std::optional<T>& value, const char* name) {
            if (!value) {
                throw std::invalid_argument(std::string("missing ") + name);
            }
            return *value;
        }
        unsigned attempt(const std::optional<std::int64_t>& value, const char* name) {
            const auto n = required(value, name);
            if (n < 1 || n > std::numeric_limits<std::int32_t>::max()) {
                throw std::invalid_argument(std::string(name) + ": outside INT range or less than 1");
            }
            return static_cast<unsigned>(n);
        }
        Kind kind(const EventRow& row) {
            const auto& k = required(row.kind, "kind");
            if (k != "build" && k != "publish") {
                throw std::invalid_argument("invalid kind");
            }
            return k == "build" ? Kind::Build : Kind::Publish;
        }
    } // namespace
    Normalized normalize(Event event) {
        std::string error;
        std::visit(
            [&](auto& e) {
                const auto check = [&](const std::string& value, std::size_t limit, const std::string& name) {
                    if (error.empty()) {
                        error = identifier(value, limit, name);
                    }
                };
                if constexpr (requires { e.pipeline; }) {
                    for (const auto& [id, feed] : e.pipeline.feeds()) {
                        check(feed.workflow.branch, 255, "branch");
                    }
                    for (const auto& [name, pkg] : e.pipeline.packages()) {
                        check(pkg.sourceRepo.branch, 255, "source_branch");
                        check(pkg.buildWorkflow.branch, 255, "build_branch");
                    }
                }
                if constexpr (requires { e.package; }) {
                    check(e.package, 128, "package");
                }
                if constexpr (requires { e.target; }) {
                    check(e.target, 128, "target");
                }
                if constexpr (requires { e.tag; }) {
                    check(e.tag, 255, "tag");
                }
                if constexpr (requires { e.buildId; }) {
                    check(e.buildId, 255, "build_id");
                }
                if constexpr (requires { e.artifact; }) {
                    check(e.artifact, 255, "artifact");
                }
                if constexpr (requires { e.sourceSha; }) {
                    if (!sha(e.sourceSha)) {
                        error = "source_sha: expected 40 lowercase hex characters";
                    }
                    if (e.origin != Origin::Webhook && e.origin != Origin::Reconcile) {
                        error = "invalid origin";
                    }
                }
                if constexpr (requires { e.feedCommit; }) {
                    if (!sha(e.feedCommit)) {
                        error = "feed_commit: expected 40 lowercase hex characters";
                    }
                }
                if constexpr (requires { e.kind; }) {
                    if (e.kind != Kind::Build && e.kind != Kind::Publish) {
                        error = "invalid kind";
                    }
                    if (e.attempt < 1 || e.attempt > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max())) {
                        error = "attempt: outside INT range or less than 1";
                    }
                }
                if constexpr (requires { e.runAttempt; }) {
                    if (e.runAttempt < 1 || e.runAttempt > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max())) {
                        error = "run_attempt: outside INT range or less than 1";
                    }
                }
                if constexpr (requires { e.httpStatus; }) {
                    if (e.httpStatus < 0 || e.httpStatus > 999) {
                        error = "http_status: expected 0..999";
                    }
                    e.message = freeText(e.message);
                }
                if constexpr (requires { e.reason; }) {
                    e.reason = freeText(e.reason);
                }
            },
            event);
        return error.empty() ? Normalized{std::move(event)} : Normalized{Invalid{error}};
    }
    std::variant<EventRow, Invalid> encode(const Event& event, std::uint64_t seq) {
        const auto normalized = normalize(event);
        return std::holds_alternative<Invalid>(normalized) ? std::variant<EventRow, Invalid>{std::get<Invalid>(normalized)}
                                                           : std::variant<EventRow, Invalid>{mapped(std::get<Event>(normalized), seq)};
    }
    Normalized decode(const EventRow& row) {
        Normalized result = Invalid{"unknown event_type"};
        try {
            const auto at = required(row.eventAt, "event_at");
            if (row.eventType == "TagObserved") {
                const auto& origin = required(row.origin, "origin");
                if (origin != "webhook" && origin != "reconcile") {
                    throw std::invalid_argument("invalid origin");
                }
                result = Event{TagObserved{at,
                                           required(row.package, "package"),
                                           required(row.tag, "tag"),
                                           required(row.sourceSha, "source_sha"),
                                           origin == "webhook" ? Origin::Webhook : Origin::Reconcile}};
            } else if (row.eventType == "DispatchAccepted" || row.eventType == "DispatchRejected") {
                const auto& id = required(row.buildId, "build_id");
                const auto k = kind(row);
                const auto n = attempt(row.attempt, "attempt");
                if (row.eventType == "DispatchAccepted") {
                    result = Event{DispatchAccepted{at, id, k, n}};
                } else {
                    const auto status = required(row.httpStatus, "http_status");
                    if (status < 0 || status > 999) {
                        throw std::invalid_argument("invalid http_status");
                    }
                    result = Event{DispatchRejected{at, id, k, n, static_cast<int>(status), required(row.message, "message")}};
                }
            } else if (row.eventType == "BuildStarted") {
                result = Event{BuildStarted{
                    at, required(row.buildId, "build_id"), required(row.runId, "run_id"), attempt(row.runAttempt, "run_attempt")}};
            } else if (row.eventType == "BuildSucceeded") {
                result = Event{BuildSucceeded{
                    at, required(row.buildId, "build_id"), required(row.runId, "run_id"), required(row.artifact, "artifact")}};
            } else if (row.eventType == "BuildFailed") {
                result = Event{
                    BuildFailed{at, required(row.buildId, "build_id"), required(row.runId, "run_id"), required(row.reason, "reason")}};
            } else if (row.eventType == "PublishSucceeded") {
                result = Event{PublishSucceeded{
                    at, required(row.buildId, "build_id"), required(row.runId, "run_id"), required(row.feedCommit, "feed_commit")}};
            } else if (row.eventType == "PublishFailed") {
                result = Event{
                    PublishFailed{at, required(row.buildId, "build_id"), required(row.runId, "run_id"), required(row.reason, "reason")}};
            } else if (row.eventType == "RetryRequested") {
                result = Event{RetryRequested{at, required(row.package, "package"), required(row.target, "target")}};
            } else if (row.eventType == "RebuildRequested") {
                result = Event{RebuildRequested{at, required(row.package, "package"), required(row.target, "target")}};
            } else if (row.eventType == "Tick") {
                result = Event{Tick{at}};
            } else if (row.eventType == "GraphChanged") {
                Pipeline::Builder builder;
                const auto& graph = required(row.graph, "graph");
                std::map<std::string, Package> packages;
                for (const auto& feed : graph.feeds) {
                    builder.feed(feed);
                }
                for (const auto& pkg : graph.packages) {
                    if (!pkg.targets.empty() || !pkg.prerequisites.empty() || !packages.emplace(pkg.name, pkg).second) {
                        throw std::invalid_argument("invalid graph package row");
                    }
                }
                for (const auto& target : graph.targets) {
                    if (!packages.contains(target.package) || !packages.at(target.package).targets.insert(target.target).second) {
                        throw std::invalid_argument("unknown package or duplicate graph target");
                    }
                    builder.target({target.target, target.feed});
                }
                for (const auto& q : graph.prerequisites) {
                    if (!packages.contains(q.package)) {
                        throw std::invalid_argument("unknown graph package");
                    }
                    auto& prerequisites = packages.at(q.package).prerequisites;
                    if (std::find(prerequisites.begin(), prerequisites.end(), q.prerequisite) != prerequisites.end()) {
                        throw std::invalid_argument("duplicate graph prerequisite");
                    }
                    prerequisites.push_back(q.prerequisite);
                }
                for (const auto& [name, pkg] : packages) {
                    builder.package(pkg);
                }
                result = Event{GraphChanged{at, builder.build()}};
            } else if (row.eventType == "Resumed") {
                result = Event{Resumed{at}};
            }
            if (std::holds_alternative<Event>(result)) {
                const auto normalized = normalize(std::get<Event>(result));
                if (std::holds_alternative<Invalid>(normalized)) {
                    result = std::get<Invalid>(normalized);
                } else if (mapped(std::get<Event>(normalized), row.seq) != row) {
                    result = Invalid{"unused non-NULL field or non-normalized value"};
                }
            }
        } catch (const std::invalid_argument& error) {
            result = Invalid{error.what()};
        }
        if (std::holds_alternative<Invalid>(result)) {
            std::get<Invalid>(result).reason = "seq " + std::to_string(row.seq) + ": " + std::get<Invalid>(result).reason;
        }
        return result;
    }
} // namespace coreci::persistence
