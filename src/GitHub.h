#ifndef CORE_CI_GITHUB_H
#define CORE_CI_GITHUB_H

#include "HttpClient.h"

#include <cstdint>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <string>
#include <string_view>
#include <utils/SubCommand.h>
#include <vector>

namespace coreci {

    class GitHubConfig : public utils::SubCommand {
    public:
        constexpr static std::string_view NAME{"github"};
        constexpr static std::string_view DESCRIPTION{"Explicit controller GitHub App installation"};
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
        nlohmann::json body;
        std::string error;
        bool submitted = false;
    };

    class GitHub {
    public:
        using Callback = std::function<void(const GitHubResult&)>;
        explicit GitHub(const GitHubConfig& config);
        void loadCredentials();
        std::uint64_t repositoryId(const std::string& repository) const;
        bool verifyWebhook(const std::string& body, const std::string& signature) const;
        static bool validRepository(const std::string& repository);
        void publicRead(const std::string& repository, const std::string& suffix, const Callback& callback);
        void request(const std::string& repository,
                     const std::string& method,
                     const std::string& suffix,
                     const nlohmann::json& body,
                     const nlohmann::json& permissions,
                     const Callback& callback,
                     bool publicRepository = false);

    private:
        struct Token {
            GitHubResult result;
            std::time_t expires = 0;
            std::vector<Callback> waiting;
        };
        std::map<std::string, Token> tokens;
        void authorize(const nlohmann::json& grant, const Callback& callback);
        std::string jwt() const;
        std::map<std::string, std::uint64_t> repositoryIds;
        void send(const std::string& method,
                  const std::string& path,
                  const std::string& authorization,
                  const nlohmann::json& body,
                  const Callback& callback);

        const GitHubConfig& config;
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key{nullptr, EVP_PKEY_free};
        std::string webhookSecret;
        HttpClient http;
    };

} // namespace coreci

#endif
