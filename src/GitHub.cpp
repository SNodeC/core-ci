#include "GitHub.h"

#include "PrivateFile.h"

#include <algorithm>
#include <charconv>
#include <core/SNodeC.h>
#include <ctime>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <regex>
#include <stdexcept>
#include <utility>
#include <web/http/ConfigHttpParser.h>
#include <web/http/client/ConfigHTTP.h>

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
        setConfigurable(
            addOptionVariable(
                "--private-key-file", privateKeyFile, "Protected controller App RSA private-key file", "path", CLI::ExistingFile),
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
        , client(
              "github-http",
              [this](const std::shared_ptr<web::http::client::MasterRequest>& request) {
                  if (!requests.empty()) {
                      const auto operation = requests.front();
                      operation->master = request;
                      request->method = operation->method;
                      request->url = operation->path;
                      request->set("Host", "api.github.com");
                      request->set("User-Agent", "core-ci");
                      request->set("Accept", "application/vnd.github+json");
                      request->set("Authorization", "Bearer " + operation->authorization);
                      request->set("X-GitHub-Api-Version", this->config.apiVersion);
                      request->set("Connection", "close");
                      request->type("application/json");
                      const bool queued = request->send(
                          operation->body.is_null() ? std::string{} : operation->body.dump(),
                          [operation](const auto&, const std::shared_ptr<web::http::client::Response>& response) {
                              try {
                                  operation->result.status = std::stoi(response->statusCode);
                                  if (!response->body.empty()) {
                                      operation->result.body = nlohmann::json::parse(response->body.begin(), response->body.end());
                                  }
                              } catch (const std::exception&) {
                                  operation->result.error = "GitHub response is not valid bounded JSON";
                              }
                          },
                          [operation](const auto&, const std::string&) {
                              operation->result.error = "GitHub HTTP response could not be parsed; request outcome may be unknown";
                              if (const auto master = operation->master.lock()) {
                                  master->disconnect();
                              }
                          });
                      if (!queued) {
                          operation->result.error = "GitHub connection ended before request submission";
                          request->disconnect();
                      }
                  }
              },
              [this](const std::shared_ptr<web::http::client::MasterRequest>& request) {
                  if (!requests.empty() && requests.front()->master.lock() == request) {
                      finish();
                  }
              }) {
        client.getConfig()->Remote::setHost("api.github.com")->setPort(443);
        client.getConfig()->Tls::setSni("api.github.com");
        client.getConfig()->Tls::setCaCertDirUseDefault();
        client.getConfig()->Tls::setCaCertAcceptUnknown(false);
        client.getConfig()
            ->net::config::ConfigInstance::getSubCommand<web::http::client::ConfigHTTP>()
            ->getParserConfig()
            ->setMaximumBodyBytes(2 * 1024 * 1024);
        client.setOnConnect([this](auto* connection) {
            if (!requests.empty()) {
                requests.front()->connectionId = connection->getConnectionId();
            }
            // This hook precedes SSL_new in SNode.C; configure the shared SSL context.
            SSL_CTX* context = client.getConfig()->getSslCtx();
            if (context != nullptr) {
                SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
            }
            if (context == nullptr || X509_VERIFY_PARAM_set1_host(SSL_CTX_get0_param(context), "api.github.com", 0) != 1) {
                connection->close();
            }
        });
        client.setOnDisconnect([this](auto* connection) {
            if (!requests.empty() && requests.front()->connectionId == connection->getConnectionId()) {
                finish();
            }
        });
    }

    void GitHub::loadCredentials() {
        if (!config.privateKeyFile.empty()) {
            if (config.appId == 0 || config.installationId == 0 || config.allowedOwner.empty()) {
                throw std::runtime_error("GitHub App requires explicit App ID, installation ID and allowed owner");
            }
            for (const auto& entry : config.repositories) {
                const auto separator = entry.find('=');
                if (separator == std::string::npos) {
                    throw std::runtime_error("GitHub repository grants must use owner/repository=numeric-ID");
                }
                const auto repository = entry.substr(0, separator);
                std::uint64_t id = 0;
                const auto parsed = std::from_chars(entry.data() + separator + 1, entry.data() + entry.size(), id);
                if (!repository.starts_with(config.allowedOwner + "/") || parsed.ec != std::errc{} ||
                    parsed.ptr != entry.data() + entry.size() || id == 0 || !repositoryIds.emplace(repository, id).second) {
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

    void GitHub::request(const std::string& repository,
                         const std::string& method,
                         const std::string& suffix,
                         const nlohmann::json& body,
                         const nlohmann::json& permissions,
                         const Callback& callback) {
        try {
            static const std::regex name(R"(^[A-Za-z0-9_-]+/[A-Za-z0-9_.-]+$)");
            const auto slash = repository.find('/');
            if (!std::regex_match(repository, name) || repository.substr(0, slash) != config.allowedOwner ||
                repository.substr(slash + 1) == "." || repository.substr(slash + 1) == ".." || (!suffix.empty() && suffix.front() != '/')) {
                throw std::invalid_argument("repository must belong to the explicitly authorized GitHub owner");
            }
            const auto grant = repositoryIds.find(repository);
            if (grant == repositoryIds.end()) {
                throw std::invalid_argument("repository is not in the deployment-owned name/ID grants");
            }
            const auto authorization = jwt();
            send("POST",
                 "/app/installations/" + std::to_string(config.installationId) + "/access_tokens",
                 authorization,
                 {{"repository_ids", {grant->second}}, {"permissions", permissions}},
                 [this, repository, method, suffix, body, callback](const GitHubResult& token) {
                     if (token.status == 201 && token.error.empty() && token.body.contains("token") && token.body.at("token").is_string()) {
                         send(method,
                              "/repos/" + repository + suffix,
                              token.body.at("token").get<std::string>(),
                              body,
                              [this, repository, method, suffix, callback](const GitHubResult& result) {
                                  if (method == "GET" && suffix.empty() && result.status == 200 &&
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

    void GitHub::send(const std::string& method,
                      const std::string& path,
                      const std::string& authorization,
                      const nlohmann::json& body,
                      const Callback& callback) {
        if (core::SNodeC::state() == core::State::STOPPING) {
            callback({0, nullptr, "GitHub request was not submitted during shutdown"});
        } else {
            auto operation = std::make_shared<Operation>();
            operation->method = method;
            operation->path = path;
            operation->authorization = authorization;
            operation->body = body;
            operation->callback = callback;
            requests.push_back(operation);
            if (requests.size() == 1) {
                connect();
            }
        }
    }

    void GitHub::connect() {
        const auto operation = requests.front();
        client.connect([this, operation](const auto&, const core::socket::State& state) {
            if (state != core::socket::State::OK && !requests.empty() && requests.front() == operation) {
                operation->result.error = "GitHub HTTPS connection failed or is disabled; request outcome may be unknown";
                finish();
            }
        });
    }

    void GitHub::finish() {
        const auto operation = requests.front();
        if (operation->result.status == 0 && operation->result.error.empty()) {
            operation->result.error = "GitHub disconnected without a verified response; request outcome is unknown";
        }
        // Retain the front operation while its callback may append a follow-up request.
        operation->callback(operation->result);
        requests.pop_front();
        if (core::SNodeC::state() == core::State::STOPPING) {
            while (!requests.empty()) {
                const auto canceled = requests.front();
                requests.pop_front();
                canceled->callback({0, nullptr, "GitHub request was not submitted during shutdown"});
            }
        } else if (!requests.empty()) {
            connect();
        }
    }

} // namespace coreci
