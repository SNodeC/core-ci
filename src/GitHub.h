#ifndef CORE_CI_GITHUB_H
#define CORE_CI_GITHUB_H

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <string>
#include <string_view>
#include <utils/SubCommand.h>
#include <vector>
#include <web/http/tls/in/Client.h>

namespace coreci {

    class GitHubConfig : public utils::SubCommand {
    public:
        constexpr static std::string_view NAME{"github"};
        constexpr static std::string_view DESCRIPTION{"Explicit controller GitHub App installation"};
        explicit GitHubConfig(utils::SubCommand* parent);

        std::uint64_t appId = 0;
        std::uint64_t installationId = 0;
        std::string privateKeyFile;
        std::string allowedOwner;
        std::string apiVersion;
        std::vector<std::string> repositories;
    };

    struct GitHubResult {
        int status = 0;
        nlohmann::json body;
        std::string error;
    };

    class GitHub {
    public:
        using Callback = std::function<void(const GitHubResult&)>;
        explicit GitHub(const GitHubConfig& config);
        void loadCredentials();
        static bool validRepository(const std::string& repository);
        void request(const std::string& repository,
                     const std::string& method,
                     const std::string& suffix,
                     const nlohmann::json& body,
                     const nlohmann::json& permissions,
                     const Callback& callback);

    private:
        struct Operation {
            std::string method;
            std::string path;
            std::string authorization;
            nlohmann::json body;
            Callback callback;
            GitHubResult result;
            std::weak_ptr<web::http::client::MasterRequest> master;
            std::uint64_t connectionId = 0;
        };

        std::string jwt() const;
        std::map<std::string, std::uint64_t> repositoryIds;
        void send(const std::string& method,
                  const std::string& path,
                  const std::string& authorization,
                  const nlohmann::json& body,
                  const Callback& callback);
        void connect();
        void finish();

        const GitHubConfig& config;
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key{nullptr, EVP_PKEY_free};
        std::deque<std::shared_ptr<Operation>> requests;
        web::http::tls::in::Client client;
    };

} // namespace coreci

#endif
