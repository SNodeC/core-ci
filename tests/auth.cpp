#include "Auth.h"

#include <algorithm>
#include <ctime>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <stdexcept>

namespace {

    void require(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    std::string decodeUrl(std::string text) {
        std::replace(text.begin(), text.end(), '-', '+');
        std::replace(text.begin(), text.end(), '_', '/');
        text.append((4 - text.size() % 4) % 4, '=');
        return ci::unbase64(text);
    }

    void jwtContract() {
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", 2048), EVP_PKEY_free);
        require(static_cast<bool>(key), "Test RSA key generation failed");
        std::unique_ptr<BIO, decltype(&BIO_free)> output(BIO_new(BIO_s_mem()), BIO_free);
        require(PEM_write_bio_PrivateKey(output.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1,
                "Test PEM serialization failed");
        char* bytes = nullptr;
        const long size = BIO_get_mem_data(output.get(), &bytes);
        const std::string token = ci::appJwt(std::string_view(bytes, static_cast<std::size_t>(size)), "test-app-id");
        const std::size_t first = token.find('.');
        const std::size_t second = token.find('.', first + 1);
        require(first != std::string::npos && second != std::string::npos && token.find('.', second + 1) == std::string::npos,
                "JWT must have three segments");
        const auto header = nlohmann::json::parse(decodeUrl(token.substr(0, first)));
        const auto claims = nlohmann::json::parse(decodeUrl(token.substr(first + 1, second - first - 1)));
        require(header == nlohmann::json{{"alg", "RS256"}, {"typ", "JWT"}}, "JWT uses the GitHub-required algorithm");
        const auto now = std::time(nullptr);
        require(claims.at("iss") == "test-app-id", "JWT preserves the supplied App identity");
        require(claims.at("iat").get<std::time_t>() <= now && claims.at("iat").get<std::time_t>() >= now - 65,
                "JWT issued-at compensates for clock drift");
        require(claims.at("exp").get<std::time_t>() > now && claims.at("exp").get<std::time_t>() <= now + 600,
                "JWT expiry satisfies GitHub's ten-minute maximum");
        const std::string signature = decodeUrl(token.substr(second + 1));
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> verify(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        require(EVP_DigestVerifyInit(verify.get(), nullptr, EVP_sha256(), nullptr, key.get()) == 1, "Test signature verifier failed");
        require(EVP_DigestVerify(verify.get(),
                                 reinterpret_cast<const unsigned char*>(signature.data()),
                                 signature.size(),
                                 reinterpret_cast<const unsigned char*>(token.data()),
                                 second) == 1,
                "JWT signature verifies independently with the RSA public key");
        bool rejected = false;
        try {
            ci::appJwt("not a key", "test-app-id");
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected, "Invalid PEM cannot authorize a GitHub token request");
    }

} // namespace

int main() {
    int result = 0;
    try {
        // GitHub's published webhook test vector; no real credentials are used.
        const std::string signature = "sha256=757107ea0eb2509fc211221cce984b8a37570b6d7586c22c46f4379c8b043e17";
        require(ci::validWebhook("It's a Secret to Everybody", "Hello, World!", signature), "Official GitHub vector must authenticate");
        require(!ci::validWebhook("It's a Secret to Everybody", "Hello, World!\n", signature), "Changed raw body must be rejected");
        require(!ci::validWebhook("wrong secret", "Hello, World!", signature), "Wrong HMAC key must be rejected");
        require(!ci::validWebhook("", "Hello, World!", signature), "Missing HMAC key must be rejected");
        require(!ci::validWebhook("It's a Secret to Everybody", "Hello, World!", ""), "Missing signature must be rejected");
        require(ci::hash("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "Known SHA-256 digest");
        const std::string blob("blob 5\0test\n", 12);
        require(ci::hash(blob, "SHA1") == "9daeafb9864cf43055ae93beb0afd6c7d144bfa4", "Git blob identity includes type and length");
        const std::string text = "{\"message\":\"Grüße\"}\n";
        require(ci::unbase64(ci::base64(text)) == text, "GitHub contents base64 preserves exact UTF-8 bytes");
        const nlohmann::json canonical = {{"z", "Grüße"}, {"a", nlohmann::json::array({1, true})}};
        require(canonical.dump() == "{\"a\":[1,true],\"z\":\"Grüße\"}", "Assignment hashing matches worker canonical JSON");
        jwtContract();
        std::cout << "GitHub authentication contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    return result;
}
