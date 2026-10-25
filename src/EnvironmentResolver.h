#ifndef CORE_CI_ENVIRONMENT_RESOLVER_H
#define CORE_CI_ENVIRONMENT_RESOLVER_H

#include "HttpClient.h"

#include <nlohmann/json.hpp>
#include <set>
#include <utils/SubCommand.h>

namespace coreci {
    class RegistryConfig : public utils::SubCommand {
    public:
        constexpr static std::string_view NAME{"registry"};
        constexpr static std::string_view DESCRIPTION{"Approved image-registry and SDK origins"};
        explicit RegistryConfig(utils::SubCommand* parent);
        std::string defaultRegistry;
        std::string defaultNamespace;
        std::vector<std::string> allowedOrigins;
        std::vector<std::string> sdkOrigins;
    };

    struct EnvironmentResult {
        nlohmann::json source;
        std::string error;
    };

    class EnvironmentResolver {
    public:
        using Callback = std::function<void(const EnvironmentResult&)>;
        explicit EnvironmentResolver(const RegistryConfig& config);
        void configure();
        void resolve(const nlohmann::json& source, const Callback& callback);

    private:
        struct Reading {
            nlohmann::json source;
            Callback callback;
            std::size_t row = 0;
            std::string origin;
            std::string repository;
            std::string reference;
            std::string token;
            std::string os;
            std::string arch;
            std::string variant;
            std::string rootDigest;
            std::string manifestDigest;
        };

        void next(const std::shared_ptr<Reading>& reading);
        void manifest(const std::shared_ptr<Reading>& reading, const nlohmann::json& descriptor, unsigned int depth);
        void configuration(const std::shared_ptr<Reading>& reading, const nlohmann::json& descriptor);
        void fetch(const std::shared_ptr<Reading>& reading,
                   const std::string& url,
                   const HttpClient::Callback& callback,
                   bool authenticate = true,
                   unsigned int redirects = 0);
        void authorize(const std::shared_ptr<Reading>& reading, const HttpResult& challenge, const std::function<void()>& next);
        static std::string verify(const HttpResult& result, const nlohmann::json& descriptor);

        const RegistryConfig& config;
        std::set<std::string> allowedOrigins;
        std::set<std::string> sdkOrigins;
        HttpClient http;
    };
} // namespace coreci

#endif
