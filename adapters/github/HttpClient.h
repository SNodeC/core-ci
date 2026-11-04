#pragma once
#include "Protocol.h"

#include <functional>
#include <memory>
namespace coreci::github {
    struct Response {
        int status = 0;
        std::string body;
        std::map<std::string, std::string> headers;
    };
    class HttpClient {
    public:
        HttpClient(Url base, std::string ca);
        ~HttpClient();
        void request(Request request, std::function<void(Response)> done);

    private:
        struct Exchange;
        Url base;
        std::string ca;
        std::vector<std::shared_ptr<Exchange>> active;
    };
} // namespace coreci::github
