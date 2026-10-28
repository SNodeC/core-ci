#pragma once

#include "Crypto.h"
#include "HttpClient.h"

#include <ctime>
#include <map>
#include <utils/SubCommand.h>

namespace coreci {
    class GitHubConfig : public utils::SubCommand {
    public:
        static constexpr std::string_view NAME{"github"};
        static constexpr std::string_view DESCRIPTION{"GitHub App and explicit repository grants"};
        explicit GitHubConfig(utils::SubCommand* parent);
        std::uint64_t appId = 0;
        std::uint64_t installationId = 0;
        std::uint64_t botId = 0;
        std::string privateKeyFile;
        std::string webhookSecretFile;
        std::string allowedOwner;
        std::string apiVersion;
        std::vector<std::string> repositories;
    };
    struct GitHubResult {
        int status = 0;
        Json body;
        std::string error;
        bool submitted = false;
        bool uncertain() const {
            return submitted && (!error.empty() || status == 0 || status == 408 || status >= 500);
        }
    };
    class GitHub {
    public:
        using Callback = std::function<void(const GitHubResult&)>;
        explicit GitHub(const GitHubConfig& config);
        void loadCredentials();
        bool verifyWebhook(const std::string& body, const std::string& signature) const;
        std::uint64_t repositoryId(const std::string& repository) const;
        static bool validRepository(const std::string& repository);
        void request(const std::string& repository,
                     const std::string& method,
                     const std::string& suffix,
                     const Json& body,
                     const Json& permissions,
                     const Callback& callback,
                     bool publicRead = false);
        void publicRead(const std::string& repository, const std::string& suffix, const Callback& callback);

    private:
        void send(const std::string& method,
                  const std::string& path,
                  const std::string& authorization,
                  const Json& body,
                  const Callback& callback);
        void authorize(const Json& grant, const Callback& callback);
        std::string jwt() const;
        struct Token {
            GitHubResult result;
            std::time_t expires = 0;
            std::vector<Callback> waiting;
        };
        const GitHubConfig& config;
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key{nullptr, EVP_PKEY_free};
        std::string secret;
        std::map<std::string, std::uint64_t> repositories;
        std::map<std::string, Token> tokens;
        std::map<std::string, std::vector<Callback>> reads;
        HttpClient http;
    };
} // namespace coreci
