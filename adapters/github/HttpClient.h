#pragma once
#include "Protocol.h"

#include <functional>
#include <memory>
namespace coreci::github {
    using Completion = std::function<void(int status, std::string body, std::string link)>;
    class HttpClient {
    public:
        HttpClient(Url base, std::string ca);
        ~HttpClient();
        void request(std::string path, std::string token, Completion done, std::optional<std::string> body = {}, bool raw = false);

    private:
        struct Exchange;
        Url base;
        std::string ca;
        std::vector<std::shared_ptr<Exchange>> active;
    };
} // namespace coreci::github
