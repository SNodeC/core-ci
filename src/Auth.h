#ifndef CORE_CI_AUTH_H
#define CORE_CI_AUTH_H

#include <string>
#include <string_view>

namespace ci {

    std::string hash(std::string_view bytes, const char* algorithm = "SHA256");
    std::string base64(std::string_view bytes);
    std::string unbase64(std::string text);
    std::string appJwt(std::string_view pem, const std::string& appId);
    bool validWebhook(std::string_view secret, std::string_view body, std::string_view signature);

} // namespace ci

#endif
