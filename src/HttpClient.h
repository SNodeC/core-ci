#ifndef CORE_CI_HTTP_CLIENT_H
#define CORE_CI_HTTP_CLIENT_H

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <web/http/CiStringMap.h>
#include <web/http/tls/in/Client.h>

namespace coreci {
    struct HttpResult {
        int status = 0;
        web::http::CiStringMap<std::string> headers;
        std::string body;
        std::string error;
        bool submitted = false;
    };

    class HttpClient {
    public:
        using Callback = std::function<void(const HttpResult&)>;
        explicit HttpClient(const std::string& instance, const std::string& fixedPeer = "");
        void configure();
        void request(const std::string& host,
                     unsigned int port,
                     const std::string& method,
                     const std::string& path,
                     const web::http::CiStringMap<std::string>& headers,
                     const std::string& body,
                     const Callback& callback);

    private:
        struct Operation {
            std::string host;
            unsigned int port = 443;
            std::string method;
            std::string path;
            web::http::CiStringMap<std::string> headers;
            std::string body;
            Callback callback;
            HttpResult result;
            std::weak_ptr<web::http::client::MasterRequest> master;
            std::uint64_t connectionId = 0;
        };

        void connect();
        void finish();
        std::string fixedPeer;
        std::deque<std::shared_ptr<Operation>> requests;
        web::http::tls::in::Client client;
    };
} // namespace coreci

#endif
