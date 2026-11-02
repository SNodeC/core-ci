#include "fixtures.h"

#include <iostream>
#include <openssl/pem.h>

namespace {
    std::string response;
    std::string expectedMethod;
    int status = 200;
    unsigned int calls = 0;
    const std::string sentinel = "private-response-sentinel";
} // namespace

// Test-only linker redirection at the public HTTPS boundary. All JSON parsing
// and Controller token/api/checked behavior below is the actual production code.
extern "C" void wrappedHttps(const std::string&, const std::string&, const std::string&, const std::string&, const ci::HttpDone&) asm(
    "__wrap__ZN2ci5httpsERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES7_S7_S7_RKSt8functionIFviS7_S7_S7_EE");

extern "C" void
wrappedHttps(const std::string& url, const std::string& method, const std::string&, const std::string& access, const ci::HttpDone& done) {
    ci::require(url.starts_with("https://api.github.com/") && method == expectedMethod && !access.empty(), "Expected public boundary");
    ++calls;
    done(status, response, "", method + " https://api.github.com/fixture response: HTTP " + std::to_string(status));
}

int main() {
    int result = 0;
    try {
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", 2048), EVP_PKEY_free);
        std::unique_ptr<BIO, decltype(&BIO_free)> buffer(BIO_new(BIO_s_mem()), BIO_free);
        ci::require(key && buffer && PEM_write_bio_PrivateKey(buffer.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1,
                    "Disposable signing fixture");
        char* bytes = nullptr;
        const auto size = BIO_get_mem_data(buffer.get(), &bytes);
        ci::Store store;
        ci::Controller controller(store, fixture::deployment(), "17", "19", std::string(bytes, size));
        std::string error;
        ci::Json value;
        const ci::Value checked = [&](const ci::Json& received, const std::string& failure) {
            value = received;
            error = failure;
        };
        ci::Controller invalidKey(store, fixture::deployment(), "17", "19", "");
        calls = 0;
        invalidKey.token([&](const std::string& token, const std::string& failure) {
            ci::require(token.empty(), "Signing failure returns no token");
            error = failure;
        });
        ci::require(calls == 0 && error == "GitHub App identity/key missing or oversized",
                    "Synchronous token signing failure reaches callback safely");
        for (const std::string body : {"{\"token\":\"" + sentinel, "{\"token\":[\"" + sentinel + "\"]}"}) {
            expectedMethod = "POST";
            response = body;
            status = 201;
            calls = 0;
            controller.token([&](const std::string& token, const std::string& failure) {
                ci::require(token.empty(), "A malformed token cannot escape");
                error = failure;
            });
            ci::require(calls == 1 && error.starts_with("Invalid JSON: ") && error.find(sentinel) == std::string::npos,
                        "Installation-token JSON failures are controller-authored and token POST is not retried");
            std::cout << error << '\n';
        }
        expectedMethod = "GET";
        response = "{\"private\":\"" + sentinel;
        status = 200;
        calls = 0;
        controller.api("fixture-access", "/fixture?secret", checked);
        ci::require(calls == 1 && error.starts_with("Invalid JSON: ") && error.find(sentinel) == std::string::npos && value.is_null(),
                    "Malformed API JSON never exposes a remote excerpt");
        std::cout << error << '\n';
        ci::checked(checked, [](const ci::Json&) {
            const auto parsed = ci::Json::parse("{\"receipt\":\"" + sentinel);
            ci::require(parsed.is_object(), "Receipt object expected");
        })({}, "");
        ci::require(error.starts_with("Invalid JSON: ") && error.find(sentinel) == std::string::npos, "Receipt JSON is classified safely");
        std::cout << error << '\n';
        status = 403;
        response = sentinel;
        controller.api("fixture-access", "/fixture?secret", checked);
        ci::require(error == "GET https://api.github.com/fixture response: HTTP 403", "Semantic HTTP error preserves safe context only");
        std::cout << "Token, API and checked JSON secrecy passed\n";
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        result = 1;
    }
    return result;
}
