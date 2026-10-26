#include "GitHub.h"

#include "PrivateFile.h"
#include "RepositoryName.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <ctime>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace coreci {

    namespace {
        std::string base64url(const std::string& value) {
            std::string result(4 * ((value.size() + 2) / 3) + 1, '\0');
            const int size = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(result.data()),
                                             reinterpret_cast<const unsigned char*>(value.data()),
                                             static_cast<int>(value.size()));
            result.resize(static_cast<std::size_t>(size));
            std::replace(result.begin(), result.end(), '+', '-');
            std::replace(result.begin(), result.end(), '/', '_');
            while (!result.empty() && result.back() == '=') {
                result.pop_back();
            }
            return result;
        }
    } // namespace

    GitHubConfig::GitHubConfig(utils::SubCommand* parent)
        : SubCommand(parent, this, "GitHub App") {
        setConfigurable(
            addOptionVariable("--app-id", appId, "Controller App ID", "uint64", std::uint64_t{0}, CLI::TypeValidator<std::uint64_t>()),
            true);
        setConfigurable(addOptionVariable("--installation-id",
                                          installationId,
                                          "Authorized installation ID",
                                          "uint64",
                                          std::uint64_t{0},
                                          CLI::TypeValidator<std::uint64_t>()),
                        true);
        setConfigurable(addOptionVariable("--bot-id",
                                          botId,
                                          "Controller App bot user ID expected on dispatches",
                                          "uint64",
                                          std::uint64_t{0},
                                          CLI::TypeValidator<std::uint64_t>()),
                        true);
        setConfigurable(
            addOptionVariable(
                "--private-key-file", privateKeyFile, "Protected controller App RSA private-key file", "path", CLI::ExistingFile),
            true);
        setConfigurable(
            addOptionVariable(
                "--webhook-secret-file", webhookSecretFile, "Protected GitHub App webhook HMAC secret file", "path", CLI::ExistingFile),
            true);
        setConfigurable(addOptionVariable("--allowed-owner",
                                          allowedOwner,
                                          "Explicit authorized source organization or owner",
                                          "string",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--repository",
                                          repositories,
                                          "Authorized owner/repository=numeric-ID (repeatable)",
                                          "repository=id",
                                          CLI::TypeValidator<std::string>())
                            ->take_all(),
                        true);
        setConfigurable(addOptionVariable("--api-version",
                                          apiVersion,
                                          "Verified GitHub REST API version",
                                          "string",
                                          std::string("2026-03-10"),
                                          CLI::TypeValidator<std::string>()),
                        true);
    }

    GitHub::GitHub(const GitHubConfig& config)
        : config(config)
        , http("github-http", "api.github.com") {
    }

    std::uint64_t GitHub::repositoryId(const std::string& repository) const {
        return repositoryIds.at(repository);
    }

    bool GitHub::verifyWebhook(const std::string& body, const std::string& signature) const {
        bool valid = false;
        if (!webhookSecret.empty() && signature.size() == 71 && signature.starts_with("sha256=")) {
            std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
            unsigned int size = 0;
            const auto* calculated = HMAC(EVP_sha256(),
                                          webhookSecret.data(),
                                          static_cast<int>(webhookSecret.size()),
                                          reinterpret_cast<const unsigned char*>(body.data()),
                                          body.size(),
                                          digest.data(),
                                          &size);
            if (calculated != nullptr && size == 32) {
                std::string expected = "sha256=";
                constexpr char hex[] = "0123456789abcdef";
                for (unsigned int index = 0; index < size; ++index) {
                    expected.push_back(hex[digest[index] >> 4]);
                    expected.push_back(hex[digest[index] & 15]);
                }
                valid = CRYPTO_memcmp(expected.data(), signature.data(), expected.size()) == 0;
            }
        }
        return valid;
    }

    bool GitHub::validRepository(const std::string& repository) {
        const auto separator = repository.find('/');
        const std::string_view value(repository);
        const auto owner = value.substr(0, separator);
        const bool ownerCharacters = std::all_of(owner.begin(), owner.end(), [](unsigned char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
        });
        return separator != std::string::npos && !owner.empty() && owner.size() <= 128 && ownerCharacters &&
               repositoryName(value.substr(separator + 1));
    }

    void GitHub::loadCredentials() {
        http.configure();
        if (!config.webhookSecretFile.empty()) {
            webhookSecret = readPrivateFile(config.webhookSecretFile, 4096);
            if (webhookSecret.size() < 32) {
                throw std::invalid_argument("webhook secret file must contain at least 32 bytes");
            }
        }
        if (!config.privateKeyFile.empty()) {
            if (config.appId == 0 || config.installationId == 0 || config.allowedOwner.empty()) {
                throw std::runtime_error("GitHub App requires explicit App ID, installation ID and allowed owner");
            }
            std::set<std::uint64_t> ids;
            for (const auto& entry : config.repositories) {
                const auto separator = entry.find('=');
                if (separator == std::string::npos) {
                    throw std::runtime_error("GitHub repository grants must use owner/repository=numeric-ID");
                }
                const auto repository = entry.substr(0, separator);
                std::uint64_t id = 0;
                const auto parsed = std::from_chars(entry.data() + separator + 1, entry.data() + entry.size(), id);
                if (!validRepository(repository) || !repository.starts_with(config.allowedOwner + "/") || parsed.ec != std::errc{} ||
                    parsed.ptr != entry.data() + entry.size() || id == 0 || !ids.insert(id).second ||
                    !repositoryIds.emplace(repository, id).second) {
                    throw std::runtime_error("GitHub repository grant has an invalid/duplicate name or ID");
                }
            }
            if (repositoryIds.empty()) {
                throw std::runtime_error("configure explicit GitHub repository name/ID grants");
            }
            const auto pem = readPrivateFile(config.privateKeyFile, 65536);
            std::unique_ptr<BIO, decltype(&BIO_free)> input(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
            if (!input) {
                throw std::runtime_error("could not allocate GitHub App key reader");
            }
            key.reset(PEM_read_bio_PrivateKey(
                input.get(),
                nullptr,
                [](char*, int, int, void*) {
                    return 0;
                },
                nullptr));
            if (!key || EVP_PKEY_base_id(key.get()) != EVP_PKEY_RSA) {
                throw std::runtime_error("GitHub App key must be a valid unencrypted RSA PEM key in a protected file");
            }
        }
    }

    std::string GitHub::jwt() const {
        if (!key) {
            throw std::runtime_error("controller GitHub App credentials are not configured");
        }
        const auto now = std::time(nullptr);
        const nlohmann::json claims{{"iat", now - 60}, {"exp", now + 540}, {"iss", std::to_string(config.appId)}};
        const std::string content = base64url(R"({"alg":"RS256","typ":"JWT"})") + "." + base64url(claims.dump());
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        std::size_t size = 0;
        if (!context || EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1 ||
            EVP_DigestSign(context.get(), nullptr, &size, reinterpret_cast<const unsigned char*>(content.data()), content.size()) != 1) {
            throw std::runtime_error("could not initialize GitHub App JWT signing");
        }
        std::string signature(size, '\0');
        if (EVP_DigestSign(context.get(),
                           reinterpret_cast<unsigned char*>(signature.data()),
                           &size,
                           reinterpret_cast<const unsigned char*>(content.data()),
                           content.size()) != 1) {
            throw std::runtime_error("could not sign GitHub App JWT");
        }
        signature.resize(size);
        return content + "." + base64url(signature);
    }

    void GitHub::publicRead(const std::string& repository, const std::string& suffix, const Callback& callback) {
        request(repository, "GET", suffix, nullptr, {{"contents", "read"}}, callback, true);
    }

    void GitHub::request(const std::string& repository,
                         const std::string& method,
                         const std::string& suffix,
                         const nlohmann::json& body,
                         const nlohmann::json& permissions,
                         const Callback& callback,
                         bool publicRepository) {
        try {
            const bool publicRead =
                publicRepository && method == "GET" && body.is_null() && permissions == nlohmann::json{{"contents", "read"}};
            if (!validRepository(repository) || (!publicRead && !repository.starts_with(config.allowedOwner + "/")) ||
                (!suffix.empty() && suffix.front() != '/')) {
                throw std::invalid_argument("repository must belong to the explicitly authorized GitHub owner");
            }
            const auto grant = repositoryIds.find(repository);
            if (grant == repositoryIds.end() && !publicRead) {
                throw std::invalid_argument("repository is not in the deployment-owned name/ID grants");
            }
            authorize(
                {{"repository_ids", {grant == repositoryIds.end() ? repositoryIds.begin()->second : grant->second}},
                 {"permissions", permissions}},
                [this, repository, method, suffix, body, callback](const GitHubResult& token) {
                    if (token.status == 201 && token.error.empty() && token.body.contains("token") && token.body.at("token").is_string()) {
                        send(method,
                             "/repos/" + repository + suffix,
                             token.body.at("token").get<std::string>(),
                             body,
                             [this, repository, method, suffix, callback](const GitHubResult& result) {
                                 if (method == "GET" && suffix.empty() && result.status == 200 && repositoryIds.contains(repository) &&
                                     (!result.body.contains("id") || result.body.at("id") != repositoryIds.at(repository))) {
                                     callback({502, nullptr, "GitHub repository ID differs from the configured grant"});
                                 } else {
                                     callback(result);
                                 }
                             });
                    } else {
                        callback({token.status, nullptr, "GitHub installation token request failed; verify explicit repository grants"});
                    }
                });
        } catch (const std::exception& error) {
            callback({0, nullptr, error.what()});
        }
    }

    void GitHub::authorize(const nlohmann::json& grant, const Callback& callback) {
        const auto scope = grant.dump();
        auto& cached = tokens[scope];
        if (cached.expires > std::time(nullptr)) {
            callback(cached.result);
        } else {
            const auto authorization = cached.waiting.empty() ? jwt() : std::string{};
            cached.waiting.push_back(callback);
            if (cached.waiting.size() == 1) {
                send("POST",
                     "/app/installations/" + std::to_string(config.installationId) + "/access_tokens",
                     authorization,
                     grant,
                     [this, scope](const GitHubResult& response) {
                         auto& entry = tokens.at(scope);
                         auto result = response;
                         entry.expires = 0;
                         try {
                             if (result.status == 201 && result.error.empty()) {
                                 const auto expiry = result.body.at("expires_at").get<std::string>();
                                 std::tm utc{};
                                 std::istringstream input(expiry);
                                 input >> std::get_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
                                 const auto expires = ::timegm(&utc);
                                 std::ostringstream verified;
                                 verified << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
                                 if (input.fail() || verified.str() != expiry || expires <= std::time(nullptr) ||
                                     !result.body.at("token").is_string() || result.body.at("token").get<std::string>().empty()) {
                                     throw std::invalid_argument("invalid installation token expiry/identity");
                                 }
                                 entry.result = result;
                                 entry.expires = expires;
                             }
                         } catch (const std::exception&) {
                             result = {502, nullptr, "GitHub installation token has invalid expiry/identity"};
                         }
                         const auto waiting = std::move(entry.waiting);
                         entry.waiting.clear();
                         for (const auto& complete : waiting) {
                             complete(result);
                         }
                     });
            }
        }
    }

    void GitHub::send(const std::string& method,
                      const std::string& path,
                      const std::string& authorization,
                      const nlohmann::json& body,
                      const Callback& callback) {
        web::http::CiStringMap<std::string> headers{
            {"Accept", "application/vnd.github+json"}, {"X-GitHub-Api-Version", config.apiVersion}, {"Content-Type", "application/json"}};
        if (!authorization.empty()) {
            headers["Authorization"] = "Bearer " + authorization;
        }
        http.request("api.github.com",
                     443,
                     method,
                     path,
                     headers,
                     body.is_null() ? std::string{} : body.dump(),
                     [callback](const HttpResult& response) {
                         GitHubResult result;
                         result.status = response.status;
                         result.error = response.error;
                         result.submitted = response.submitted;
                         if (!response.body.empty()) {
                             try {
                                 result.body = nlohmann::json::parse(response.body);
                             } catch (const std::exception&) {
                                 result.error = "GitHub response is not valid bounded JSON";
                             }
                         }
                         callback(result);
                     });
    }

} // namespace coreci
