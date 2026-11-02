#include "Controller.h"

#include <Log.h>
#include <algorithm>
#include <fcntl.h>
#include <fstream>
#include <openssl/ssl.h>
#include <regex>
#include <sys/stat.h>
#include <unistd.h>
#include <web/http/ConfigHttpParser.h>
#include <web/http/tls/in/Client.h>

namespace ci {

    Value checked(const Value& done, const std::function<void(const Json&)>& next) {
        return [done, next](const Json& value, const std::string& error) {
            try {
                require(error.empty(), error);
                next(value);
            } catch (const Json::exception& failure) {
                done({}, "Invalid JSON: " + std::to_string(failure.id));
            } catch (const std::exception& failure) {
                done({}, failure.what());
            }
        };
    }

    std::string encode(const std::string& text) {
        constexpr char digits[] = "0123456789ABCDEF";
        std::string result;
        for (const unsigned char byte : text) {
            if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') || byte == '-' ||
                byte == '_' || byte == '.' || byte == '~') {
                result += static_cast<char>(byte);
            } else {
                result += '%';
                result += digits[byte >> 4];
                result += digits[byte & 15];
            }
        }
        return result;
    }

    std::string protectedFile(const std::string& path) {
        const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        require(fd >= 0, "Cannot open protected input");
        FILE* file = fdopen(fd, "r");
        if (file == nullptr) {
            close(fd);
        }
        std::unique_ptr<FILE, int (*)(FILE*)> input(file, fclose);
        struct stat status{};
        require(input && fstat(fd, &status) == 0 && S_ISREG(status.st_mode) && status.st_uid == geteuid() && (status.st_mode & 077) == 0 &&
                    status.st_size > 0 && status.st_size <= 65536,
                "Unsafe protected input");
        std::string result(static_cast<std::size_t>(status.st_size), '\0');
        require(fread(result.data(), 1, result.size(), input.get()) == result.size(), "Cannot read protected input");
        return result;
    }

    void https(const std::string& url, const std::string& method, const std::string& body, const std::string& token, const HttpDone& done) {
        std::smatch match;
        require(std::regex_match(url, match, std::regex(R"(https://([A-Za-z0-9.-]+)(:([0-9]+))?(/[^\r\n]*)?)")), "Invalid HTTPS URL");
        const std::string host = match[1];
        const int port = match[3].matched ? std::stoi(match[3]) : 443;
        require(port > 0 && port <= 65535, "Invalid HTTPS port");
        const std::string path = match[4].matched ? match[4].str() : "/";
        const auto start = [=](const auto& self, unsigned int attempt) -> void {
            auto completion = std::make_shared<HttpDone>(done);
            auto phase = std::make_shared<std::string>("tls: connection ended before HTTP request (cause unavailable from public API)");
            const auto finish =
                [=](int status, const std::string& data, const std::string& location, const std::string& reason, bool eligible) {
                    if (*completion) {
                        auto callback = std::exchange(*completion, {});
                        const std::string context =
                            method + " https://" + host + (host == "api.github.com" ? path.substr(0, path.find('?')) : "") + " " + reason;
                        if (status == 0) {
                            snode::log::application().error() << context << " " << attempt << "/3";
                        }
                        if (status == 0 && eligible && method == "GET" && attempt < 3) {
                            self(self, attempt + 1);
                        } else {
                            callback(status, data, location, context + " " + std::to_string(attempt) + "/3");
                        }
                    }
                };
            try {
                const std::shared_ptr<web::http::tls::in::Client> client = std::make_shared<web::http::tls::in::Client>(
                    "",
                    [host, port, path, method, body, token, finish, phase](const auto& request) {
                        *phase = "response: connection closed before HTTP response";
                        request->host(host + (port == 443 ? "" : ":" + std::to_string(port)));
                        request->method = method;
                        request->url = path;
                        request->set("Accept", "application/vnd.github+json")
                            .set("X-GitHub-Api-Version", "2026-03-10")
                            .set("User-Agent", "core-ci")
                            .set("Connection", "close")
                            .type("application/json");
                        if (!token.empty()) {
                            request->set("Authorization", "Bearer " + token);
                        }
                        request->send(
                            body,
                            [request, finish, phase](const auto&, const auto& response) {
                                const int status = std::stoi(response->statusCode);
                                finish(status,
                                       std::string(response->body.begin(), response->body.end()),
                                       response->get("Location"),
                                       status == 0 ? *phase : "response: HTTP " + std::to_string(status),
                                       true);
                                request->disconnect();
                            },
                            [finish](const auto&, const std::string&) {
                                finish(0, "", "", "HTTP delivery/parse failure", false);
                            });
                    },
                    [finish, phase](const auto&) {
                        finish(0, "", "", *phase, true);
                    });
                client->getConfig()->Instance::configurable(false);
                client->getConfig()->setRetry(false);
                client->getConfig()->setRetryOnFatal(false);
                client->getConfig()->setReconnect(false);
                client->getConfig()->setSni(host);
                client->getConfig()->setCaCertDirUseDefault(true);
                client->getConfig()->setCaCertAcceptUnknown(false);
                client->getConfig()
                    ->Instance::template getSubCommand<web::http::client::ConfigHTTP>()
                    ->getParserConfig()
                    ->setMaximumBodyBytes(512 * 1024 * 1024);
                client->setOnConnect([host, config = client->getConfig(), finish](auto* connection) {
                    auto* parameters = SSL_CTX_get0_param(config->getSslCtx());
                    const int verified = host.find_first_not_of("0123456789.") == std::string::npos
                                             ? X509_VERIFY_PARAM_set1_ip_asc(parameters, host.c_str())
                                             : X509_VERIFY_PARAM_set1_host(parameters, host.c_str(), 0);
                    if (verified != 1) {
                        finish(0, "", "", "setup: hostname verification configuration failed", false);
                        connection->close();
                    }
                });
                client->setOnDisconnect([finish, phase](auto*) {
                    finish(0, "", "", *phase, true);
                });
                client->connect(host, static_cast<std::uint16_t>(port), [client, finish](const auto&, const core::socket::State& state) {
                    if (state != core::socket::State::OK) {
                        finish(0, "", "", "connect: connection unavailable", state == core::socket::State::ERROR);
                    }
                });
            } catch (const std::exception&) {
                finish(0, "", "", "setup: HTTPS client unavailable", false);
            }
        };
        start(start, 1);
    }

} // namespace ci
