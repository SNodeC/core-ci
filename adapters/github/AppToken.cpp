#include "AppToken.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <sstream>
#include <stdexcept>
namespace coreci::github {
    namespace {
        using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
        Key readKey(const std::string& path) {
            std::unique_ptr<BIO, decltype(&BIO_free)> input(BIO_new_file(path.c_str(), "r"), BIO_free);
            Key key(input ? PEM_read_bio_PrivateKey(
                                input.get(),
                                nullptr,
                                [](char*, int, int, void*) {
                                    return 0;
                                },
                                nullptr)
                          : nullptr,
                    EVP_PKEY_free);
            if (!key || EVP_PKEY_base_id(key.get()) != EVP_PKEY_RSA) {
                throw std::invalid_argument("github-app-key-file");
            }
            return key;
        }
        std::string base64(const std::string& value) {
            std::string result(4 * ((value.size() + 2) / 3), '\0');
            EVP_EncodeBlock(reinterpret_cast<unsigned char*>(result.data()),
                            reinterpret_cast<const unsigned char*>(value.data()),
                            static_cast<int>(value.size()));
            std::replace(result.begin(), result.end(), '+', '-');
            std::replace(result.begin(), result.end(), '/', '_');
            while (!result.empty() && result.back() == '=') {
                result.pop_back();
            }
            return result;
        }
        std::string jwt(const std::string& app, const std::string& file, std::int64_t now) {
            const auto key = readKey(file);
            std::string value = base64(R"({"alg":"RS256","typ":"JWT"})") + "." +
                                base64(nlohmann::json{{"iat", now - 60}, {"exp", now + 540}, {"iss", app}}.dump());
            std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
            std::size_t size = 0;
            if (!context || EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1 ||
                EVP_DigestSign(context.get(), nullptr, &size, reinterpret_cast<const unsigned char*>(value.data()), value.size()) != 1) {
                throw std::runtime_error("GitHub App signing failed");
            }
            std::string signature(size, '\0');
            if (EVP_DigestSign(context.get(),
                               reinterpret_cast<unsigned char*>(signature.data()),
                               &size,
                               reinterpret_cast<const unsigned char*>(value.data()),
                               value.size()) != 1) {
                throw std::runtime_error("GitHub App signing failed");
            }
            signature.resize(size);
            value += "." + base64(signature);
            return value;
        }
        std::int64_t expiry(const std::string& value) {
            std::tm date{};
            std::istringstream input(value);
            input >> std::get_time(&date, "%Y-%m-%dT%H:%M:%SZ");
            if (input.fail() || input.peek() != std::char_traits<char>::eof()) {
                throw std::invalid_argument("invalid token expiry");
            }
            return timegm(&date);
        }
    } // namespace
    AppToken::AppToken(std::string appId,
                       std::string installationId,
                       std::string keyFile,
                       std::string apiPath,
                       Send send,
                       std::function<std::int64_t()> clock)
        : appId(std::move(appId))
        , installationId(std::move(installationId))
        , keyFile(std::move(keyFile))
        , apiPath(std::move(apiPath))
        , send(std::move(send))
        , clock(std::move(clock)) {
        readKey(this->keyFile);
    }
    void AppToken::token(std::function<void(const std::string&)> done) {
        if (!cached.empty() && clock() < expires - 300) {
            done(cached);
        } else {
            waiting.push_back(std::move(done));
            if (waiting.size() == 1) {
                std::string signedJwt;
                try {
                    signedJwt = jwt(appId, keyFile, clock());
                } catch (const std::exception&) {
                    // Never propagate key, JWT or upstream credential response bytes.
                }
                const auto minted = [this](int status, std::string body, std::string) {
                    cached.clear();
                    try {
                        if (status == 201) {
                            const auto value = nlohmann::json::parse(body);
                            const auto until = expiry(value.at("expires_at").get<std::string>());
                            const auto credential = value.at("token").get<std::string>();
                            if (until > clock() + 300 && !credential.empty()) {
                                cached = credential;
                                expires = until;
                            }
                        }
                    } catch (const std::exception&) {
                        cached.clear();
                    }
                    auto callbacks = std::move(waiting);
                    waiting.clear();
                    const auto credential = cached;
                    for (const auto& callback : callbacks) {
                        callback(credential);
                    }
                };
                if (signedJwt.empty()) {
                    minted(0, {}, {});
                } else {
                    send(apiPath + "/app/installations/" + escape(installationId) + "/access_tokens",
                         std::move(signedJwt),
                         minted,
                         std::string("{}"),
                         false);
                }
            }
        }
    }
    void AppToken::call(std::string path, Completion done, std::optional<std::string> body, bool raw, bool retried) {
        token([this, path = std::move(path), done = std::move(done), body = std::move(body), raw, retried](const std::string& credential) {
            if (credential.empty()) {
                done(0, "GitHub App authentication failed", {});
            } else {
                send(
                    path,
                    credential,
                    [this, path, done, body, raw, retried, credential](int status, std::string response, std::string link) {
                        if (status == 401 && !retried) {
                            if (cached == credential) {
                                cached.clear();
                            }
                            call(path, done, body, raw, true);
                        } else {
                            done(status, std::move(response), std::move(link));
                        }
                    },
                    body,
                    raw);
            }
        });
    }
    void AppToken::request(std::string path, Completion done, std::optional<std::string> body, bool raw) {
        call(std::move(path), std::move(done), std::move(body), raw, false);
    }
} // namespace coreci::github
