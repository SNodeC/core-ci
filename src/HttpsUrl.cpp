#include "HttpsUrl.h"

#include <algorithm>
#include <regex>
#include <stdexcept>

namespace coreci {
    HttpsUrl::HttpsUrl(const std::string& value) {
        static const std::regex syntax(R"(^https://([A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?)(?::([0-9]{1,5}))?(/[^\s#\\]*)?$)");
        std::smatch match;
        if (value.size() > 4096 || !std::regex_match(value, match, syntax) || !std::all_of(value.begin(), value.end(), [](unsigned char c) {
                return c >= 33 && c <= 126;
            })) {
            throw std::invalid_argument("expected a bounded HTTPS URL with a DNS/IPv4 host and no user information or fragment");
        }
        host = match[1].str();
        if (match[2].matched) {
            port = static_cast<unsigned int>(std::stoul(match[2].str()));
        }
        if (port == 0 || port > 65535) {
            throw std::invalid_argument("HTTPS URL has an invalid port");
        }
        origin = "https://" + host + (port == 443 ? "" : ":" + std::to_string(port));
        path = match[3].matched ? match[3].str() : "/";
    }
} // namespace coreci
