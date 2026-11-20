#include "Input.h"

#include "core/Version.h"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace coreci::persistence {
    namespace {
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
                    std::set<std::string> packages;
                    for (const auto& tag : e.tags) {
                        const auto normalized = normalize(Event{tag});
                        if (std::holds_alternative<Invalid>(normalized) || tag.at != e.at || !e.pipeline.packages().contains(tag.package) ||
                            !Version::parse(tag.tag) || !packages.insert(tag.package).second) {
                            error = "invalid graph release";
                        }
                    }
                    std::sort(e.tags.begin(), e.tags.end(), [](const auto& a, const auto& b) {
                        return a.package < b.package;
                    });
                    for (const auto& [name, pkg] : e.pipeline.packages()) {
                        check(pkg.sourceRepo.branch, 255, "source_branch");
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
                if constexpr (requires { e.waveId; }) {
                    check(e.waveId, 64, "wave_id");
                }
                if constexpr (requires { e.publicationId; }) {
                    check(e.publicationId, 64, "publication_id");
                }
                if constexpr (requires { e.conclusion; }) {
                    check(e.conclusion, 64, "conclusion");
                }
                if constexpr (requires { e.runId; }) {
                    if (e.runId == 0) {
                        error = "run_id: must be positive";
                    }
                }
                if constexpr (requires { e.artifact; }) {
                    if (!e.artifact.empty()) {
                        check(e.artifact, 255, "artifact");
                    }
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
                if constexpr (requires { e.attempt; }) {
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
    std::string describe(const Event& input) {
        std::string text = std::visit(
            [](const auto& value) {
                std::string result;
                if constexpr (requires { value.waveId; }) {
                    result += value.waveId + " ";
                }
                if constexpr (requires { value.publicationId; }) {
                    result += value.publicationId + " ";
                }
                if constexpr (requires { value.buildId; }) {
                    result += value.buildId + " ";
                }
                if constexpr (requires { value.package; }) {
                    result += value.package + " ";
                }
                if constexpr (requires { value.target; }) {
                    result += value.target + " ";
                }
                if constexpr (requires { value.reason; }) {
                    result += value.reason;
                }
                if constexpr (requires { value.conclusion; }) {
                    result += value.conclusion;
                }
                return result;
            },
            input);
        return std::to_string(input.index()) + " " + text;
    }
} // namespace coreci::persistence
