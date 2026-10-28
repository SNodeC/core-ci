#pragma once

#include "Error.h"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

namespace coreci {
    using Json = nlohmann::json;
    std::string hex(const unsigned char* bytes, std::size_t size);
    std::string hash(const std::string& bytes, const EVP_MD* algorithm);
    std::string digest(const Json& value);
    std::string base64(const std::string& bytes);
    std::string unbase64(std::string text);
} // namespace coreci
