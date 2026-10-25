#include "HttpClient.h"

#include <arpa/inet.h>
#include <charconv>
#include <core/SNodeC.h>
#include <openssl/ssl.h>
#include <stdexcept>
#include <web/http/ConfigHttpParser.h>
#include <web/http/client/ConfigHTTP.h>

namespace coreci {
    HttpClient::HttpClient(const std::string& instance, const std::string& fixedPeer)
        : fixedPeer(fixedPeer)
        , client(
              instance,
              [this](const std::shared_ptr<web::http::client::MasterRequest>& request) {
                  if (!requests.empty()) {
                      const auto operation = requests.front();
                      operation->master = request;
                      request->method = operation->method;
                      request->url = operation->path;
                      request->set("User-Agent", "core-ci");
                      request->set("Accept-Encoding", "identity");
                      request->set("Connection", "close");
                      for (const auto& [key, value] : operation->headers) {
                          request->set(key, value);
                      }
                      operation->result.submitted = true;
                      const bool queued = request->send(
                          operation->body,
                          [operation](const auto&, const std::shared_ptr<web::http::client::Response>& response) {
                              const auto& status = response->statusCode;
                              const auto parsed = std::from_chars(status.data(), status.data() + status.size(), operation->result.status);
                              if (parsed.ec != std::errc{} || parsed.ptr != status.data() + status.size() ||
                                  operation->result.status < 100 || operation->result.status > 599) {
                                  operation->result.status = 0;
                                  operation->result.error = "HTTPS response has an invalid status";
                              } else {
                                  operation->result.headers = response->headers;
                                  operation->result.body.assign(response->body.begin(), response->body.end());
                              }
                          },
                          [operation](const auto&, const std::string&) {
                              operation->result.error =
                                  "HTTPS response could not be parsed within configured bounds; outcome may be unknown";
                              if (const auto master = operation->master.lock()) {
                                  master->disconnect();
                              }
                          });
                      if (!queued) {
                          operation->result.error = "HTTPS connection ended before request submission";
                          request->disconnect();
                      }
                  }
              },
              [this](const std::shared_ptr<web::http::client::MasterRequest>& request) {
                  if (!requests.empty() && requests.front()->master.lock() == request) {
                      finish();
                  }
              }) {
        client.getConfig()->Remote::setHost(fixedPeer.empty() ? "localhost" : fixedPeer)->setPort(443);
        client.getConfig()->Tls::setSni(fixedPeer.empty() ? "localhost" : fixedPeer);
        client.getConfig()->Tls::setCaCertDirUseDefault();
        client.getConfig()->Tls::setCaCertAcceptUnknown(false);
        client.getConfig()->setRetry(false);
        client.getConfig()->setReconnect(false);
        client.getConfig()
            ->net::config::ConfigInstance::getSubCommand<web::http::client::ConfigHTTP>()
            ->getParserConfig()
            ->setMaximumBodyBytes(2 * 1024 * 1024);
        client.setOnConnect([this](auto* connection) {
            if (!requests.empty()) {
                const auto operation = requests.front();
                operation->connectionId = connection->getConnectionId();
                // This framework callback precedes SSL_new; set verification on its SSL context.
                SSL_CTX* context = client.getConfig()->getSslCtx();
                unsigned char address[16]{};
                const bool ip = ::inet_pton(AF_INET, operation->host.c_str(), address) == 1 ||
                                ::inet_pton(AF_INET6, operation->host.c_str(), address) == 1;
                int verified = 0;
                if (context != nullptr) {
                    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
                    X509_VERIFY_PARAM_set1_host(SSL_CTX_get0_param(context), nullptr, 0);
                    X509_VERIFY_PARAM_set1_ip(SSL_CTX_get0_param(context), nullptr, 0);
                    verified = ip ? X509_VERIFY_PARAM_set1_ip_asc(SSL_CTX_get0_param(context), operation->host.c_str())
                                  : X509_VERIFY_PARAM_set1_host(SSL_CTX_get0_param(context), operation->host.c_str(), 0);
                }
                if (verified != 1) {
                    connection->close();
                }
            }
        });
        client.setOnDisconnect([this](auto* connection) {
            if (!requests.empty() && requests.front()->connectionId == connection->getConnectionId()) {
                finish();
            }
        });
    }

    void HttpClient::configure() {
        if (client.getConfig()->getRetry() || client.getConfig()->getReconnect()) {
            throw std::runtime_error("controller HTTPS operations require framework retry and reconnect to remain disabled");
        }
        if (fixedPeer.empty() && (client.getConfig()->Remote::getHost() != "localhost" || client.getConfig()->Remote::getPort() != 443)) {
            throw std::runtime_error("dynamic HTTPS endpoints come from approved origins, not the HTTP instance's remote options");
        }
    }

    void HttpClient::request(const std::string& host,
                             unsigned int port,
                             const std::string& method,
                             const std::string& path,
                             const web::http::CiStringMap<std::string>& headers,
                             const std::string& body,
                             const Callback& callback) {
        if (core::SNodeC::state() == core::State::STOPPING) {
            callback({0, {}, "", "HTTPS request was not submitted during shutdown"});
        } else if (host.empty() || port == 0 || port > 65535 || path.empty() || path.front() != '/' ||
                   (!fixedPeer.empty() && (host != fixedPeer || port != 443))) {
            callback({0, {}, "", "HTTPS endpoint differs from the configured service"});
        } else {
            auto operation = std::make_shared<Operation>();
            operation->host = host;
            operation->port = port;
            operation->method = method;
            operation->path = path;
            operation->headers = headers;
            operation->body = body;
            operation->callback = callback;
            requests.push_back(operation);
            if (requests.size() == 1) {
                connect();
            }
        }
    }

    void HttpClient::connect() {
        const auto operation = requests.front();
        client.getConfig()->net::config::ConfigInstance::getSubCommand<web::http::client::ConfigHTTP>()->setHostHeader(
            operation->host + (operation->port == 443 ? "" : ":" + std::to_string(operation->port)));
        if (fixedPeer.empty()) {
            if (client.getConfig()->Remote::getHost() != operation->host || client.getConfig()->Remote::getPort() != operation->port) {
                client.getConfig()->Remote::setHost(operation->host)->setPort(operation->port);
                client.getConfig()->Remote::renew();
            }
            client.getConfig()->Tls::setSni(operation->host);
        }
        client.connect([this, operation](const auto&, const core::socket::State& state) {
            if (state != core::socket::State::OK && !requests.empty() && requests.front() == operation) {
                operation->result.error = "HTTPS connection failed or is disabled; request outcome may be unknown";
                finish();
            }
        });
    }

    void HttpClient::finish() {
        const auto operation = requests.front();
        if (operation->result.status == 0 && operation->result.error.empty()) {
            operation->result.error = "HTTPS disconnected without a verified response; request outcome is unknown";
        }
        operation->callback(operation->result);
        requests.pop_front();
        if (core::SNodeC::state() == core::State::STOPPING) {
            while (!requests.empty()) {
                const auto canceled = requests.front();
                requests.pop_front();
                canceled->callback({0, {}, "", "HTTPS request was not submitted during shutdown"});
            }
        } else if (!requests.empty()) {
            connect();
        }
    }
} // namespace coreci
