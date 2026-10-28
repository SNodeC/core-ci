#pragma once

#include <deque>
#include <web/http/tls/in/Client.h>

namespace coreci {
    struct HttpResult {
        int status = 0;
        std::string body;
        std::string error;
        bool submitted = false;
    };
    class HttpClient {
    public:
        using Callback = std::function<void(const HttpResult&)>;
        using Headers = web::http::CiStringMap<std::string>;
        explicit HttpClient(const std::string& name, const std::string& peer = "localhost");
        void configure(const std::string& peer = "", unsigned int port = 443);
        void request(
            const std::string& method, const std::string& path, const Headers& headers, const std::string& body, const Callback& callback);

    private:
        struct Operation {
            std::string method;
            std::string path;
            Headers headers;
            std::string body;
            Callback callback;
            HttpResult result;
            std::weak_ptr<web::http::client::MasterRequest> master;
            web::http::tls::in::Client::FlowHandle flow;
            std::uint64_t id = 0;
        };
        std::shared_ptr<Operation> find(std::uint64_t id) const;
        void pump();
        void finish(const std::shared_ptr<Operation>& operation);
        void deliver(const std::shared_ptr<Operation>& operation);
        std::string peer;
        unsigned int maximum = 8;
        bool pumping = false;
        std::deque<std::shared_ptr<Operation>> operations;
        std::shared_ptr<Operation> connecting;
        web::http::tls::in::Client client;
    };
} // namespace coreci
