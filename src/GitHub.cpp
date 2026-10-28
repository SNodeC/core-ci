#include "GitHub.h"

#include "PrivateFile.h"

#include <charconv>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <regex>
#include <set>
#include <sstream>

namespace coreci {
    GitHubConfig::GitHubConfig(utils::SubCommand* parent)
        : SubCommand(parent, this, "GitHub App") {
        const std::pair<const char*, std::uint64_t&> numbers[]{{"app-id", appId}, {"installation-id", installationId}, {"bot-id", botId}};
        for (const auto& [name, value] : numbers) {
            setConfigurable(
                addOptionVariable(std::string("--") + name, value, name, "id", std::uint64_t{0}, CLI::TypeValidator<std::uint64_t>()),
                true);
        }
        const std::pair<const char*, std::string&> strings[]{
            {"private-key-file", privateKeyFile}, {"webhook-secret-file", webhookSecretFile}, {"allowed-owner", allowedOwner}};
        for (const auto& [name, value] : strings) {
            setConfigurable(addOptionVariable(std::string("--") + name, value, name, "string", CLI::TypeValidator<std::string>()), true);
        }
        setConfigurable(
            addOptionVariable(
                "--api-version", apiVersion, "REST API version", "version", std::string("2026-03-10"), CLI::TypeValidator<std::string>()),
            true);
        setConfigurable(
            addOptionVariable("--repository", repositories, "Authorized repository=id", "grant", CLI::TypeValidator<std::string>())
                ->take_all(),
            true);
    }
    GitHub::GitHub(const GitHubConfig& config)
        : config(config)
        , http("github-http", "api.github.com") {
    }
    bool GitHub::validRepository(const std::string& repository) {
        static const std::regex syntax("^[A-Za-z0-9_-]{1,128}/[A-Za-z0-9_.-]{1,128}$");
        const auto name = repository.substr(repository.find('/') + 1);
        return std::regex_match(repository, syntax) && name != "." && name != "..";
    }
    std::uint64_t GitHub::repositoryId(const std::string& repository) const {
        return repositories.at(repository);
    }
    void GitHub::loadCredentials() {
        http.configure();
        if (!config.webhookSecretFile.empty()) {
            secret = readPrivateFile(config.webhookSecretFile, 4096);
            require(secret.size() >= 32, "webhook HMAC secret must contain at least 32 bytes");
        }
        if (!config.privateKeyFile.empty()) {
            require(config.appId && config.installationId && !config.allowedOwner.empty(), "configure the GitHub App and authorized owner");
            std::set<std::uint64_t> ids;
            for (const auto& grant : config.repositories) {
                const auto split = grant.find('=');
                const auto name = grant.substr(0, split);
                require(split != std::string::npos && validRepository(name) && name.starts_with(config.allowedOwner + "/"),
                        "invalid repository grant");
                std::uint64_t id = 0;
                const auto parsed = std::from_chars(grant.data() + split + 1, grant.data() + grant.size(), id);
                require(parsed.ec == std::errc{} && parsed.ptr == grant.data() + grant.size() && id && ids.insert(id).second &&
                            repositories.emplace(name, id).second,
                        "invalid or duplicate repository grant ID");
            }
            require(!repositories.empty(), "configure explicit repository grants");
            const auto pem = readPrivateFile(config.privateKeyFile, 65536);
            std::unique_ptr<BIO, decltype(&BIO_free)> input(BIO_new_mem_buf(pem.data(), pem.size()), BIO_free);
            require(bool(input), "cannot load App key");
            key.reset(PEM_read_bio_PrivateKey(
                input.get(),
                nullptr,
                [](char*, int, int, void*) {
                    return 0;
                },
                nullptr));
            require(key && EVP_PKEY_base_id(key.get()) == EVP_PKEY_RSA, "App key must be unencrypted RSA PEM");
        }
    }
    bool GitHub::verifyWebhook(const std::string& body, const std::string& signature) const {
        unsigned char output[EVP_MAX_MD_SIZE];
        unsigned int size = 0;
        const auto* result = HMAC(
            EVP_sha256(), secret.data(), secret.size(), reinterpret_cast<const unsigned char*>(body.data()), body.size(), output, &size);
        const auto expected = "sha256=" + hex(output, result ? size : 0);
        return !secret.empty() && result && size == 32 && signature.size() == expected.size() &&
               CRYPTO_memcmp(signature.data(), expected.data(), expected.size()) == 0;
    }
    std::string GitHub::jwt() const {
        require(bool(key), "GitHub App key is not configured");
        const auto encode = [](const std::string& text) {
            auto result = base64(text);
            std::replace(result.begin(), result.end(), '+', '-');
            std::replace(result.begin(), result.end(), '/', '_');
            result.erase(result.find_last_not_of('=') + 1);
            return result;
        };
        const auto now = std::time(nullptr);
        const auto content = encode(R"({"alg":"RS256","typ":"JWT"})") + "." +
                             encode(Json{{"iat", now - 60}, {"exp", now + 540}, {"iss", std::to_string(config.appId)}}.dump());
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        std::string signature(EVP_PKEY_get_size(key.get()), '\0');
        std::size_t size = signature.size();
        require(context && EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) == 1 &&
                    EVP_DigestSign(context.get(),
                                   reinterpret_cast<unsigned char*>(signature.data()),
                                   &size,
                                   reinterpret_cast<const unsigned char*>(content.data()),
                                   content.size()) == 1,
                "App JWT signing failed");
        signature.resize(size);
        return content + "." + encode(signature);
    }
    void GitHub::send(
        const std::string& method, const std::string& path, const std::string& authorization, const Json& body, const Callback& callback) {
        HttpClient::Headers headers{{"Accept", "application/vnd.github+json"},
                                    {"Content-Type", "application/json"},
                                    {"X-GitHub-Api-Version", config.apiVersion},
                                    {"Authorization", "Bearer " + authorization}};
        http.request(method, path, headers, body.is_null() ? "" : body.dump(), [callback](const HttpResult& response) {
            GitHubResult result{response.status, nullptr, response.error, response.submitted};
            if (!response.body.empty()) {
                result.body = Json::parse(response.body, nullptr, false);
                if (result.body.is_discarded()) {
                    result.error = "GitHub response is not valid bounded JSON";
                }
            }
            callback(result);
        });
    }
    void GitHub::authorize(const Json& grant, const Callback& callback) {
        const auto scope = grant.dump();
        auto& token = tokens[scope];
        if (token.expires > std::time(nullptr) + 300) {
            callback(token.result);
        } else {
            const auto authorization = token.waiting.empty() ? jwt() : std::string{};
            token.waiting.push_back(callback);
            if (token.waiting.size() == 1) {
                send("POST",
                     "/app/installations/" + std::to_string(config.installationId) + "/access_tokens",
                     authorization,
                     grant,
                     [this, scope](const GitHubResult& response) {
                         auto& entry = tokens.at(scope);
                         auto result = response;
                         try {
                             if (result.status == 201 && result.error.empty()) {
                                 const auto expiry = result.body.at("expires_at").get<std::string>();
                                 std::tm time{};
                                 std::istringstream input(expiry);
                                 input >> std::get_time(&time, "%Y-%m-%dT%H:%M:%SZ");
                                 const auto expires = ::timegm(&time);
                                 std::ostringstream normalized;
                                 normalized << std::put_time(&time, "%Y-%m-%dT%H:%M:%SZ");
                                 require(!input.fail() && normalized.str() == expiry && expires > std::time(nullptr) &&
                                             !result.body.at("token").get<std::string>().empty(),
                                         "invalid installation token identity/expiry");
                                 entry.result = result;
                                 entry.expires = expires;
                             }
                         } catch (const std::exception&) {
                             result = {502, nullptr, "invalid GitHub installation token"};
                         }
                         const auto waiting = std::move(entry.waiting);
                         entry.waiting.clear();
                         for (const auto& done : waiting) {
                             done(result);
                         }
                     });
            }
        }
    }
    void GitHub::publicRead(const std::string& repository, const std::string& suffix, const Callback& callback) {
        request(repository, "GET", suffix, nullptr, {{"contents", "read"}}, callback, true);
    }
    void GitHub::request(const std::string& repository,
                         const std::string& method,
                         const std::string& suffix,
                         const Json& body,
                         const Json& permissions,
                         const Callback& callback,
                         bool publicRead) {
        Callback complete = callback;
        try {
            require(validRepository(repository) && (suffix.empty() || suffix.front() == '/'), "invalid GitHub repository/path");
            const bool publicContents = publicRead && method == "GET" && body.is_null() && permissions == Json{{"contents", "read"}};
            require(publicContents || (repository.starts_with(config.allowedOwner + "/") && repositories.contains(repository)),
                    "repository is not authorized");
            require(!repositories.empty(), "GitHub repository grants are not configured");
            bool execute = true;
            if (method == "GET") {
                const auto question = Json{repository, suffix, permissions, publicContents}.dump();
                auto& waiting = reads[question];
                waiting.push_back(callback);
                execute = waiting.size() == 1;
                complete = [this, question](const GitHubResult& result) {
                    const auto waiting = std::move(reads.at(question));
                    reads.erase(question);
                    for (const auto& done : waiting) {
                        done(result);
                    }
                };
            }
            if (execute) {
                const auto id = repositories.contains(repository) ? repositoryId(repository) : repositories.begin()->second;
                authorize({{"repository_ids", {id}}, {"permissions", permissions}},
                          [this, repository, method, suffix, body, complete](const GitHubResult& token) {
                              if (token.status == 201 && token.error.empty()) {
                                  send(method, "/repos/" + repository + suffix, token.body.at("token"), body, complete);
                              } else {
                                  complete({token.status, nullptr, "GitHub installation token request failed; verify repository grants"});
                              }
                          });
            }
        } catch (const std::exception& error) {
            complete({0, nullptr, error.what()});
        }
    }
} // namespace coreci
