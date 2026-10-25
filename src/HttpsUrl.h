#ifndef CORE_CI_HTTPS_URL_H
#define CORE_CI_HTTPS_URL_H

#include <string>

namespace coreci {
    struct HttpsUrl {
        explicit HttpsUrl(const std::string& value);
        std::string host;
        unsigned int port = 443;
        std::string origin;
        std::string path;
    };
} // namespace coreci

#endif
