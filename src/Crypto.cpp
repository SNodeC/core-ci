#include "Crypto.h"

#include <algorithm>
#include <cctype>

namespace coreci {
    std::string hex(const unsigned char* bytes, std::size_t size) {
        std::string result;
        for (std::size_t index = 0; index < size; ++index) {
            result += "0123456789abcdef"[bytes[index] >> 4];
            result += "0123456789abcdef"[bytes[index] & 15];
        }
        return result;
    }
    std::string hash(const std::string& bytes, const EVP_MD* algorithm) {
        unsigned char output[EVP_MAX_MD_SIZE];
        unsigned int size = 0;
        require(EVP_Digest(bytes.data(), bytes.size(), output, &size, algorithm, nullptr) == 1, "digest calculation failed");
        return hex(output, size);
    }
    std::string digest(const Json& value) {
        return hash(value.dump(), EVP_sha256());
    }
    std::string base64(const std::string& bytes) {
        std::string result(4 * ((bytes.size() + 2) / 3) + 1, '\0');
        const auto size = EVP_EncodeBlock(
            reinterpret_cast<unsigned char*>(result.data()), reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
        require(size >= 0, "base64 encoding failed");
        result.resize(size);
        return result;
    }
    std::string unbase64(std::string text) {
        std::erase_if(text, [](unsigned char character) {
            return std::isspace(character);
        });
        require(text.size() <= 3 * 1024 * 1024 && text.size() % 4 == 0, "invalid base64 size");
        std::string result(text.size() / 4 * 3 + 1, '\0');
        const auto size = EVP_DecodeBlock(
            reinterpret_cast<unsigned char*>(result.data()), reinterpret_cast<const unsigned char*>(text.data()), text.size());
        const auto padding = text.empty() || text.back() != '=' ? 0 : text.size() > 1 && text[text.size() - 2] == '=' ? 2 : 1;
        require(size >= padding, "invalid base64 data");
        result.resize(size - padding);
        require(base64(result) == text, "noncanonical base64 data");
        return result;
    }
} // namespace coreci
