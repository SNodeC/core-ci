#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>
namespace coreci::github {
    struct Error {
        std::string reason;
    };
    struct Url {
        std::string host, path;
        unsigned short port = 443;
    };
    std::optional<Url> httpsUrl(const std::string& value);
    std::optional<std::string> decodePath(const std::string& value);
    std::variant<std::optional<std::string>, Error> nextLink(const std::string& value, const Url& origin);
    std::string escape(const std::string& value);
    std::int64_t isoTime(const std::string& value);
    std::string hmac(const std::string& secret, const std::string& body);
    bool verify(const std::string& secret, const std::string& body, const std::string& signature);
    bool verifyCallback(
        const std::string& secret, const std::string& body, const std::string& timestamp, const std::string& signature, std::int64_t now);
    using Form = std::map<std::string, std::string>;
    std::variant<Form, Error> form(const std::string& body);
    struct Push {
        std::string repo, tag, sha;
        std::string branch = {}, defaultBranch = {};
    };
    std::variant<Push, Error, std::monostate> webhook(const std::string& body, const std::string& event);
    struct Tag {
        std::string tag, sha;
    };
    std::variant<std::vector<Tag>, Error> tags(const std::string& body);
} // namespace coreci::github
