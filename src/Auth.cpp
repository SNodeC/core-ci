#include "Auth.h"

#include <algorithm>
#include <array>
#include <climits>
#include <ctime>
#include <memory>
#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <stdexcept>

namespace ci {

    namespace {

        std::string hex(const unsigned char* bytes, std::size_t size) {
            constexpr std::string_view digits = "0123456789abcdef";
            std::string result;
            for (std::size_t index = 0; index < size; ++index) {
                result += digits[bytes[index] >> 4];
                result += digits[bytes[index] & 15];
            }
            return result;
        }

        std::string base64url(std::string_view bytes) {
            std::string result = base64(bytes);
            std::replace(result.begin(), result.end(), '+', '-');
            std::replace(result.begin(), result.end(), '/', '_');
            result.erase(result.find_last_not_of('=') + 1);
            return result;
        }

    } // namespace

    std::string hash(std::string_view bytes, const char* algorithm) {
        std::array<unsigned char, EVP_MAX_MD_SIZE> output{};
        unsigned int size = 0;
        if (EVP_Digest(bytes.data(), bytes.size(), output.data(), &size, EVP_get_digestbyname(algorithm), nullptr) != 1) {
            throw std::runtime_error("Digest calculation failed");
        }
        return hex(output.data(), size);
    }

    std::string base64(std::string_view bytes) {
        if (bytes.size() > static_cast<std::size_t>(INT_MAX / 4)) {
            throw std::runtime_error("Base64 input too large");
        }
        std::string result(4 * ((bytes.size() + 2) / 3) + 1, '\0');
        const int size = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(result.data()),
                                         reinterpret_cast<const unsigned char*>(bytes.data()),
                                         static_cast<int>(bytes.size()));
        result.resize(static_cast<std::size_t>(size));
        return result;
    }

    std::string unbase64(std::string text) {
        std::erase(text, '\n');
        std::erase(text, '\r');
        if (text.size() > static_cast<std::size_t>(INT_MAX) || text.size() % 4 != 0) {
            throw std::runtime_error("Invalid base64 size");
        }
        std::string result(text.size() / 4 * 3 + 1, '\0');
        const int size = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(result.data()),
                                         reinterpret_cast<const unsigned char*>(text.data()),
                                         static_cast<int>(text.size()));
        if (size < 0) {
            throw std::runtime_error("Invalid base64 encoding");
        }
        std::size_t padding = 0;
        if (!text.empty() && text.back() == '=') {
            padding = text.size() > 1 && text[text.size() - 2] == '=' ? 2 : 1;
        }
        result.resize(static_cast<std::size_t>(size) - padding);
        return result;
    }

    std::string appJwt(std::string_view pem, const std::string& appId) {
        if (pem.empty() || pem.size() > 65536 || appId.empty()) {
            throw std::runtime_error("GitHub App identity/key missing or oversized");
        }
        std::unique_ptr<BIO, decltype(&BIO_free)> input(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(PEM_read_bio_PrivateKey(
                                                                    input.get(),
                                                                    nullptr,
                                                                    [](char*, int, int, void*) {
                                                                        return 0;
                                                                    },
                                                                    nullptr),
                                                                EVP_PKEY_free);
        if (!key || EVP_PKEY_base_id(key.get()) != EVP_PKEY_RSA) {
            throw std::runtime_error("GitHub App requires an unencrypted RSA key");
        }
        const auto now = std::time(nullptr);
        const nlohmann::json claims = {{"iat", now - 60}, {"exp", now + 540}, {"iss", appId}};
        const std::string message = base64url(R"({"alg":"RS256","typ":"JWT"})") + "." + base64url(claims.dump());
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        std::size_t size = 0;
        if (!context || EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1 ||
            EVP_DigestSign(context.get(), nullptr, &size, reinterpret_cast<const unsigned char*>(message.data()), message.size()) != 1) {
            throw std::runtime_error("GitHub App signing initialization failed");
        }
        std::string signature(size, '\0');
        if (EVP_DigestSign(context.get(),
                           reinterpret_cast<unsigned char*>(signature.data()),
                           &size,
                           reinterpret_cast<const unsigned char*>(message.data()),
                           message.size()) != 1) {
            throw std::runtime_error("GitHub App signing failed");
        }
        signature.resize(size);
        return message + "." + base64url(signature);
    }

    bool validWebhook(std::string_view secret, std::string_view body, std::string_view signature) {
        bool valid = false;
        if (!secret.empty() && secret.size() <= 65536 && signature.size() == 71) {
            std::array<unsigned char, EVP_MAX_MD_SIZE> output{};
            unsigned int size = 0;
            if (HMAC(EVP_sha256(),
                     secret.data(),
                     static_cast<int>(secret.size()),
                     reinterpret_cast<const unsigned char*>(body.data()),
                     body.size(),
                     output.data(),
                     &size) != nullptr) {
                const std::string expected = "sha256=" + hex(output.data(), size);
                valid = expected.size() == signature.size() && CRYPTO_memcmp(expected.data(), signature.data(), signature.size()) == 0;
            }
        }
        return valid;
    }

} // namespace ci
