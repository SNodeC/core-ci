#include "Protocol.h"

#include <charconv>
#include <cstdlib>
#include <limits>
#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <sstream>
namespace coreci::github {
    namespace {
        template <class T>
        bool number(const std::string& value, T& result) {
            const auto p = std::from_chars(value.data(), value.data() + value.size(), result);
            return p.ec == std::errc{} && p.ptr == value.data() + value.size();
        }
        int digit(char c) {
            int n = -1;
            if (c >= '0' && c <= '9') {
                n = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                n = c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                n = c - 'A' + 10;
            }
            return n;
        }
        std::optional<std::string> unescape(const std::string& text, bool plus) {
            std::optional<std::string> result = std::string{};
            for (std::size_t i = 0; result && i < text.size(); ++i) {
                if (text[i] == '%') {
                    if (i + 2 >= text.size() || digit(text[i + 1]) < 0 || digit(text[i + 2]) < 0) {
                        result.reset();
                    } else {
                        *result += static_cast<char>(digit(text[i + 1]) * 16 + digit(text[i + 2]));
                        i += 2;
                    }
                } else {
                    *result += plus && text[i] == '+' ? ' ' : text[i];
                }
            }
            return result;
        }
    } // namespace
    std::optional<std::string> decodePath(const std::string& value) {
        return unescape(value, false);
    }
    std::variant<std::optional<std::string>, Error> nextLink(const std::string& value, const Url& origin) {
        std::optional<std::string> next;
        bool valid = true;
        std::size_t pos = 0;
        const auto space = [&] {
            while (pos < value.size() && (value[pos] == ' ' || value[pos] == '\t')) {
                ++pos;
            }
        };
        while (valid && pos < value.size()) {
            space();
            valid = pos < value.size() && value[pos] == '<';
            const auto end = valid ? value.find('>', pos) : std::string::npos;
            valid = valid && end != std::string::npos;
            std::string url;
            if (valid) {
                url = value.substr(pos + 1, end - pos - 1);
                pos = end + 1;
                const auto parsed = httpsUrl(url);
                valid = parsed && parsed->host == origin.host && parsed->port == origin.port;
            }
            while (valid && pos < value.size() && value[pos] != ',') {
                space();
                if (pos < value.size() && value[pos] != ';') {
                    valid = false;
                } else if (pos < value.size()) {
                    ++pos;
                    space();
                    const auto start = pos;
                    while (pos < value.size() && value[pos] != '=' && value[pos] != ';' && value[pos] != ',') {
                        ++pos;
                    }
                    auto name = value.substr(start, pos - start);
                    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
                        name.pop_back();
                    }
                    valid = !name.empty() && pos < value.size() && value[pos] == '=';
                    std::string parameter;
                    if (valid) {
                        ++pos;
                        space();
                        if (pos < value.size() && value[pos] == '"') {
                            ++pos;
                            bool closed = false;
                            while (pos < value.size() && !closed) {
                                if (value[pos] == '"') {
                                    closed = true;
                                    ++pos;
                                } else if (value[pos] == '\\' && pos + 1 < value.size()) {
                                    parameter += value[pos + 1];
                                    pos += 2;
                                } else {
                                    parameter += value[pos++];
                                }
                            }
                            valid = closed;
                        } else {
                            const auto first = pos;
                            while (pos < value.size() && value[pos] != ';' && value[pos] != ',' && value[pos] != ' ' &&
                                   value[pos] != '\t') {
                                ++pos;
                            }
                            parameter = value.substr(first, pos - first);
                            valid = !parameter.empty();
                        }
                        if (name == "rel") {
                            std::istringstream relations(parameter);
                            std::string relation;
                            while (relations >> relation) {
                                if (relation == "next") {
                                    valid = valid && !next;
                                    next = url;
                                }
                            }
                        }
                    }
                }
                space();
            }
            if (valid && pos < value.size() && value[pos] == ',') {
                ++pos;
                space();
                valid = pos < value.size();
            }
        }
        std::variant<std::optional<std::string>, Error> result = next;
        if (!valid) {
            result = Error{"malformed Link header"};
        }
        return result;
    }
    std::optional<Url> httpsUrl(const std::string& value) {
        std::optional<Url> result;
        if (value.starts_with("https://") && value.find_first_of("\r\n\t @#") == std::string::npos) {
            const auto slash = value.find('/', 8);
            const auto authority = value.substr(8, slash == std::string::npos ? std::string::npos : slash - 8);
            const auto colon = authority.find(':');
            Url url{authority.substr(0, colon), slash == std::string::npos ? "" : value.substr(slash), 443};
            unsigned port = 443;
            const bool valid = colon == std::string::npos || number(authority.substr(colon + 1), port);
            if (valid && !url.host.empty() && port > 0 && port <= 65535) {
                url.port = static_cast<unsigned short>(port);
                result = url;
            }
        }
        return result;
    }
    std::string escape(const std::string& value) {
        const char* hex = "0123456789ABCDEF";
        std::string result;
        for (const unsigned char c : value) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                c == '~') {
                result += c;
            } else {
                result += '%';
                result += hex[c >> 4];
                result += hex[c & 15];
            }
        }
        return result;
    }
    std::string hmac(const std::string& secret, const std::string& body) {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned len = 0;
        HMAC(EVP_sha256(),
             secret.data(),
             static_cast<int>(secret.size()),
             reinterpret_cast<const unsigned char*>(body.data()),
             body.size(),
             digest,
             &len);
        std::string result = "sha256=";
        const char* hex = "0123456789abcdef";
        for (unsigned i = 0; i < len; ++i) {
            result += hex[digest[i] >> 4];
            result += hex[digest[i] & 15];
        }
        return result;
    }
    bool verify(const std::string& secret, const std::string& body, const std::string& signature) {
        const auto expected = hmac(secret, body);
        return expected.size() == 71 && signature.size() == expected.size() &&
               CRYPTO_memcmp(signature.data(), expected.data(), expected.size()) == 0;
    }
    bool verifyCallback(
        const std::string& secret, const std::string& body, const std::string& timestamp, const std::string& signature, std::int64_t now) {
        std::int64_t stamp = 0;
        const bool parsed = number(timestamp, stamp);
        const bool near = parsed && (stamp >= now ? static_cast<std::uint64_t>(stamp) - static_cast<std::uint64_t>(now) <= 300
                                                  : static_cast<std::uint64_t>(now) - static_cast<std::uint64_t>(stamp) <= 300);
        return near && verify(secret, timestamp + "." + body, signature);
    }
    std::variant<Form, Error> form(const std::string& body) {
        Form result;
        std::string error;
        std::size_t start = 0;
        while (start < body.size() && error.empty()) {
            const auto end = body.find('&', start);
            const auto part = body.substr(start, end == std::string::npos ? std::string::npos : end - start);
            const auto equal = part.find('=');
            const auto key = unescape(part.substr(0, equal), true);
            const auto value = equal == std::string::npos ? std::optional<std::string>{} : unescape(part.substr(equal + 1), true);
            if (!key || !value || key->empty() || !result.emplace(*key, *value).second) {
                error = "malformed form or duplicate key";
            }
            start = end == std::string::npos ? body.size() : end + 1;
        }
        std::variant<Form, Error> output = result;
        if (!error.empty()) {
            output = Error{error};
        }
        return output;
    }
    std::variant<Event, Error, std::monostate> report(const std::string& buildId, const Form& values, std::int64_t at) {
        const auto get = [&](const std::string& key) {
            const auto i = values.find(key);
            return i == values.end() ? std::string{} : i->second;
        };
        const auto phase = get("phase"), status = get("status");
        std::uint64_t run = 0;
        unsigned attempt = 0;
        std::variant<Event, Error, std::monostate> output = Error{"invalid callback fields"};
        if (!buildId.empty()) {
            if (phase == "publish" && status == "started") {
                output = std::monostate{};
            } else if (number(get("run_id"), run)) {
                if (phase == "build" && status == "started" && number(get("run_attempt"), attempt) && attempt > 0) {
                    output = Event{BuildStarted{at, buildId, run, attempt}};
                } else if (phase == "build" && status == "succeeded" && !get("artifact").empty()) {
                    output = Event{BuildSucceeded{at, buildId, run, get("artifact")}};
                } else if (phase == "build" && status == "failed" && !get("reason").empty()) {
                    output = Event{BuildFailed{at, buildId, run, get("reason")}};
                } else if (phase == "publish" && status == "succeeded" && !get("feed_commit").empty()) {
                    output = Event{PublishSucceeded{at, buildId, run, get("feed_commit")}};
                } else if (phase == "publish" && status == "failed" && !get("reason").empty()) {
                    output = Event{PublishFailed{at, buildId, run, get("reason")}};
                }
            }
        }
        return output;
    }
    std::variant<Push, Error, std::monostate> webhook(const std::string& body, const std::string& event) {
        std::variant<Push, Error, std::monostate> output = std::monostate{};
        try {
            const auto j = nlohmann::json::parse(body);
            if (!j.is_object()) {
                output = Error{"expected object"};
            } else if (event == "push") {
                const auto ref = j.at("ref").get<std::string>();
                if (ref.starts_with("refs/tags/") && j.at("created").get<bool>() && !j.at("deleted").get<bool>()) {
                    output = Push{j.at("repository").at("full_name").get<std::string>(), ref.substr(10), j.at("after").get<std::string>()};
                } else if (ref.starts_with("refs/heads/") && !j.at("deleted").get<bool>()) {
                    output = Push{j.at("repository").at("full_name").get<std::string>(),
                                  {},
                                  {},
                                  ref.substr(11),
                                  j.at("repository").at("default_branch").get<std::string>()};
                }
            }
        } catch (const nlohmann::json::exception&) {
            output = Error{"malformed webhook"};
        }
        return output;
    }
    std::variant<std::vector<Tag>, Error> tags(const std::string& body) {
        std::variant<std::vector<Tag>, Error> result = Error{"malformed tag page"};
        try {
            const auto j = nlohmann::json::parse(body);
            if (j.is_array()) {
                std::vector<Tag> parsed;
                for (const auto& tag : j) {
                    parsed.push_back({tag.at("name").get<std::string>(), tag.at("commit").at("sha").get<std::string>()});
                }
                result = parsed;
            }
        } catch (const nlohmann::json::exception&) {
            result = Error{"malformed tag page"};
        }
        return result;
    }
    Request dispatch(const Command& command, const std::string& callbackBase, const std::string& token) {
        Request request{"POST",
                        {},
                        {},
                        {{"Accept", "application/vnd.github+json"},
                         {"X-GitHub-Api-Version", "2022-11-28"},
                         {"Authorization", "Bearer " + token},
                         {"Content-Type", "application/json"},
                         {"User-Agent", "core-ci"}}};
        std::visit(
            [&](const auto& c) {
                auto base = callbackBase;
                while (!base.empty() && base.back() == '/') {
                    base.pop_back();
                }
                nlohmann::json inputs = {{"build_id", c.buildId},
                                         {"package", c.package},
                                         {"version", c.version.text()},
                                         {"revision", std::to_string(c.revision)},
                                         {"target", c.target},
                                         {"callback_url", base + "/api/v1/builds/" + escape(c.buildId) + "/report"}};
                if constexpr (std::is_same_v<std::decay_t<decltype(c)>, DispatchBuild>) {
                    std::string prerequisites;
                    for (const auto& [name, version] : c.inputs) {
                        if (!prerequisites.empty()) {
                            prerequisites += ',';
                        }
                        prerequisites += name + "=" + version.version.text() + "-" + std::to_string(version.revision);
                    }
                    inputs["source_repo"] = c.sourceRepo.owner + "/" + c.sourceRepo.repo;
                    inputs["tag"] = c.tag;
                    inputs["prerequisites"] = prerequisites;
                } else {
                    inputs["build_repo"] = c.buildRepo.owner + "/" + c.buildRepo.repo;
                    inputs["build_run_id"] = std::to_string(c.runId);
                    inputs["artifact"] = c.artifact;
                }
                request.path = "/repos/" + escape(c.workflow.owner) + "/" + escape(c.workflow.repo) + "/actions/workflows/" +
                               escape(c.workflow.workflowFile) + "/dispatches";
                request.body = nlohmann::json{{"ref", c.workflow.branch}, {"inputs", inputs}}.dump();
            },
            command);
        return request;
    }
    std::string EnvironmentTokenProvider::token() const {
        const auto* value = std::getenv("CORECI_GITHUB_TOKEN");
        return value == nullptr ? "" : value;
    }
} // namespace coreci::github
