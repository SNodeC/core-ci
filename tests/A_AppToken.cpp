#include "Check.h"
#include "adapters/github/AppToken.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <sstream>
using namespace coreci::github;
std::string decode64(std::string value) {
    std::replace(value.begin(), value.end(), '-', '+');
    std::replace(value.begin(), value.end(), '_', '/');
    const auto original = value.size();
    value.resize((value.size() + 3) / 4 * 4, '=');
    std::string result(value.size(), '\0');
    const int size = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(result.data()),
                                     reinterpret_cast<const unsigned char*>(value.data()),
                                     static_cast<int>(value.size()));
    CHECK(size >= 0);
    result.resize(size < 0 ? 0 : static_cast<std::size_t>(size) - (value.size() - original));
    return result;
}
std::string date(std::int64_t now) {
    const std::time_t at = now;
    std::ostringstream result;
    result << std::put_time(std::gmtime(&at), "%Y-%m-%dT%H:%M:%SZ");
    return result.str();
}
int main() {
    const auto file = std::filesystem::current_path() / "tmp" / "app-token-test.pem";
    std::filesystem::create_directories(file.parent_path());
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_RSA_gen(2048), EVP_PKEY_free);
    const auto write = [&](EVP_PKEY* value) {
        std::unique_ptr<BIO, decltype(&BIO_free)> output(BIO_new_file(file.c_str(), "w"), BIO_free);
        CHECK(output && PEM_write_bio_PrivateKey(output.get(), value, nullptr, nullptr, 0, nullptr, nullptr) == 1);
    };
    write(key.get());
    std::int64_t now = 1700000000;
    int mints = 0, calls = 0, failures = 0, answers = 0, mode = 0;
    std::optional<Completion> pending;
    std::vector<std::tuple<std::string, std::optional<std::string>, bool>> requests;
    const auto send = [&](std::string path, std::string credential, Completion done, std::optional<std::string> body, bool raw) {
        if (path == "/api/app/installations/42/access_tokens") {
            ++mints;
            const auto first = credential.find('.'), second = credential.find('.', first + 1);
            const auto header = nlohmann::json::parse(decode64(credential.substr(0, first)));
            const auto claims = nlohmann::json::parse(decode64(credential.substr(first + 1, second - first - 1)));
            CHECK(header.at("alg") == "RS256");
            CHECK(header.at("typ") == "JWT");
            CHECK(claims.at("iss") == "123");
            CHECK(claims.at("iat") == now - 60);
            CHECK(claims.at("exp") == now + 540);
            const auto signature = decode64(credential.substr(second + 1));
            const auto message = credential.substr(0, second);
            std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
            CHECK(EVP_DigestVerifyInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) == 1);
            CHECK(EVP_DigestVerify(context.get(),
                                   reinterpret_cast<const unsigned char*>(signature.data()),
                                   signature.size(),
                                   reinterpret_cast<const unsigned char*>(message.data()),
                                   message.size()) == 1);
            CHECK(body == std::optional<std::string>("{}") && !raw);
            if (mode == 1) {
                pending = std::move(done);
            } else {
                done(mode == 2 ? 401 : 201,
                     nlohmann::json{{"token", "installation-" + std::to_string(mints)}, {"expires_at", date(now + 3600)}}.dump(),
                     {});
            }
        } else {
            ++calls;
            CHECK(credential.starts_with("installation-"));
            requests.emplace_back(path, body, raw);
            const auto status = failures > 0 ? 401 : 200;
            if (failures > 0) {
                --failures;
            }
            done(status, "response", "link");
        }
    };
    AppToken app("123", "42", file, "/api", send, [&] {
        return now;
    });
    const auto request = [&](int expected) {
        app.request(
            "/api/repos/o/r/contents/file?ref=tag-commit",
            [&, expected](int status, std::string, std::string) {
                CHECK_EQ(status, expected);
                ++answers;
            },
            {},
            true);
    };
    request(200);
    CHECK_EQ(mints, 1);
    now += 3299;
    request(200);
    CHECK_EQ(mints, 1);
    ++now;
    request(200);
    CHECK_EQ(mints, 2);
    failures = 1;
    request(200);
    CHECK_EQ(mints, 3);
    CHECK(requests[requests.size() - 1] == requests[requests.size() - 2]);
    failures = 2;
    request(401);
    CHECK_EQ(mints, 4);
    CHECK_EQ(failures, 0);
    mode = 1;
    now += 3300;
    request(200);
    request(200);
    CHECK_EQ(mints, 5);
    CHECK_EQ(answers, 5);
    CHECK(pending.has_value());
    auto complete = std::move(*pending);
    pending.reset();
    complete(201, nlohmann::json{{"token", "installation-concurrent"}, {"expires_at", date(now + 3600)}}.dump(), {});
    CHECK_EQ(answers, 7);
    mode = 2;
    failures = 1;
    request(0);
    CHECK_EQ(mints, 6);
    mode = 0;
    request(200);
    CHECK_EQ(mints, 7);
    key.reset(EVP_RSA_gen(2048));
    write(key.get());
    now += 3300;
    request(200); // A rotated key signs the next JWT with the new public-key identity.
    CHECK_EQ(mints, 8);
    failures = 1;
    app.request(
        "/api/repos/o/r/actions/workflows/build.yml/dispatches",
        [&](int status, std::string, std::string) {
            CHECK_EQ(status, 200);
        },
        std::string("{\"ref\":\"trunk\"}"));
    CHECK(requests[requests.size() - 1] == requests[requests.size() - 2]);
    CHECK_EQ(mints, 9);
    std::filesystem::remove(file);
    failures = 1;
    const auto before = calls;
    request(0); // Rotation removed the file: mint rereads it and fails without leaking credentials.
    CHECK_EQ(calls, before + 1);
    CHECK_EQ(mints, 9);
    bool rejected = false;
    try {
        AppToken missing("123", "42", file, "", send, [&] {
            return now;
        });
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CHECK(rejected);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> ec(EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "prime256v1"), EVP_PKEY_free);
    write(ec.get());
    rejected = false;
    try {
        AppToken wrongType("123", "42", file, "", send, [&] {
            return now;
        });
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CHECK(rejected);
    std::filesystem::remove(file);
    return checkResult();
}
