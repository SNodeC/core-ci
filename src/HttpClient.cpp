#include "HttpClient.h"

#include "Error.h"

#include <algorithm>
#include <arpa/inet.h>
#include <charconv>
#include <openssl/ssl.h>
#include <utility>
#include <web/http/ConfigHttpParser.h>
#include <web/http/client/ConfigHTTP.h>

namespace coreci {
    HttpClient::HttpClient(const std::string& name, const std::string& initialPeer)
        : peer(initialPeer)
        , client(
              name,
              [this](const auto& request) {
                  const auto operation = find(request->getSocketContext()->getSocketConnection()->getConnectionId());
                  if (operation) {
                      operation->master = request;
                      request->method = operation->method;
                      request->url = operation->path;
                      request->set("User-Agent", "core-ci").set("Connection", "close").set("Accept-Encoding", "identity");
                      for (const auto& [key, value] : operation->headers) {
                          request->set(key, value);
                      }
                      operation->result.submitted = true;
                      const bool sent = request->send(
                          operation->body,
                          [operation](const auto&, const auto& response) {
                              const auto& status = response->statusCode;
                              const auto parsed = std::from_chars(status.data(), status.data() + status.size(), operation->result.status);
                              if (parsed.ec != std::errc{} || parsed.ptr != status.data() + status.size() ||
                                  operation->result.status < 100 || operation->result.status > 599) {
                                  operation->result.status = 0;
                                  operation->result.error = "HTTPS response has an invalid status";
                              } else {
                                  operation->result.body.assign(response->body.begin(), response->body.end());
                              }
                          },
                          [operation](const auto&, const std::string&) {
                              operation->result.error = "HTTPS response parsing failed; submitted outcome is unknown";
                              if (const auto master = operation->master.lock()) {
                                  master->disconnect();
                              }
                          });
                      if (!sent) {
                          operation->result.error = "HTTPS request submission failed; preserve its assignment";
                          request->disconnect();
                      }
                  } else {
                      request->disconnect();
                  }
              },
              [this](const auto& request) {
                  // MasterRequest has already detached its socket context at this hook.
                  const auto found = std::find_if(operations.begin(), operations.end(), [&](const auto& operation) {
                      return operation->master.lock() == request;
                  });
                  if (found != operations.end()) {
                      const auto operation = *found;
                      deliver(operation);
                  }
              }) {
        client.getConfig()->Remote::setHost(peer)->setPort(443);
        client.getConfig()->Tls::setSni(peer);
        client.getConfig()->Tls::setCaCertDirUseDefault();
        client.getConfig()->Tls::setCaCertAcceptUnknown(false);
        client.getConfig()->setRetry(false);
        client.getConfig()->setReconnect(false);
        auto* config = static_cast<net::config::ConfigInstance*>(client.getConfig());
        config->setConfigurable(
            config->addOptionVariable("--max-in-flight", maximum, "Concurrent HTTP operations", "uint", 8u, CLI::Range(1u, 16u)), true);
        config->getSubCommand<web::http::client::ConfigHTTP>()->getParserConfig()->setMaximumBodyBytes(2 * 1024 * 1024);
        client.setOnConnect([this](auto* connection) {
            const auto operation = std::exchange(connecting, nullptr);
            if (operation) {
                operation->id = connection->getConnectionId();
                auto* context = client.getConfig()->getSslCtx();
                unsigned char address[16];
                const bool ip = ::inet_pton(AF_INET, peer.c_str(), address) == 1 || ::inet_pton(AF_INET6, peer.c_str(), address) == 1;
                int verified = 0;
                if (context) {
                    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
                    auto* parameters = SSL_CTX_get0_param(context);
                    X509_VERIFY_PARAM_set1_host(parameters, nullptr, 0);
                    X509_VERIFY_PARAM_set1_ip(parameters, nullptr, 0);
                    verified = ip ? X509_VERIFY_PARAM_set1_ip_asc(parameters, peer.c_str())
                                  : X509_VERIFY_PARAM_set1_host(parameters, peer.c_str(), 0);
                }
                if (verified != 1) {
                    connection->close();
                }
            } else {
                connection->close();
            }
        });
        client.setOnDisconnect([this](auto* connection) {
            if (const auto operation = find(connection->getConnectionId())) {
                finish(operation);
            }
        });
    }

    void HttpClient::configure(const std::string& host, unsigned int port) {
        require(operations.empty() && !client.getConfig()->getRetry() && !client.getConfig()->getReconnect(),
                "configure HTTP origins before requests, with replay disabled");
        if (!host.empty()) {
            peer = host;
            client.getConfig()->Remote::setHost(peer)->setPort(port);
            client.getConfig()->Remote::renew();
        }
        client.getConfig()->Tls::setSni(peer);
        client.getConfig()->net::config::ConfigInstance::getSubCommand<web::http::client::ConfigHTTP>()->setHostHeader(
            peer + (port == 443 ? "" : ":" + std::to_string(port)));
    }

    std::shared_ptr<HttpClient::Operation> HttpClient::find(std::uint64_t id) const {
        const auto found = std::find_if(operations.begin(), operations.end(), [id](const auto& operation) {
            return operation->id == id;
        });
        return found == operations.end() ? nullptr : *found;
    }

    void HttpClient::request(
        const std::string& method, const std::string& path, const Headers& headers, const std::string& body, const Callback& callback) {
        require(!path.empty() && path.front() == '/' && path.find_first_of("\r\n") == std::string::npos, "invalid HTTP path");
        auto operation = std::make_shared<Operation>();
        operation->method = method;
        operation->path = path;
        operation->headers = headers;
        operation->body = body;
        operation->callback = callback;
        operations.push_back(operation);
        pump();
    }

    void HttpClient::deliver(const std::shared_ptr<Operation>& operation) {
        if (operation->result.status == 0 && operation->result.error.empty()) {
            operation->result.error =
                operation->result.submitted ? "HTTPS response lost; outcome unknown" : "HTTPS request was not submitted";
        }
        if (const auto callback = std::exchange(operation->callback, nullptr)) {
            callback(operation->result);
        }
    }

    void HttpClient::finish(const std::shared_ptr<Operation>& operation) {
        const auto found = std::find(operations.begin(), operations.end(), operation);
        if (found != operations.end()) {
            const auto owned = *found;
            operations.erase(found);
            deliver(owned);
            pump();
        }
    }

    void HttpClient::pump() {
        if (!pumping) {
            pumping = true;
            bool available = true;
            while (available) {
                const auto next = std::find_if(operations.begin(), operations.end(), [](const auto& operation) {
                    return !operation->flow;
                });
                const auto active = std::count_if(operations.begin(), operations.end(), [](const auto& operation) {
                    return bool(operation->flow);
                });
                available = next != operations.end() && active < maximum;
                if (available) {
                    const auto operation = *next;
                    if (core::SNodeC::state() == core::State::STOPPING) {
                        operation->result.error = "HTTPS request was not submitted during shutdown";
                        finish(operation);
                    } else {
                        operation->flow = client.connect([this, operation](const auto&, const core::socket::State& state) {
                            if (state == core::socket::State::OK) {
                                operation->result.error.clear();
                                connecting = operation;
                            } else if (state == core::socket::State::DISABLED) {
                                operation->result.error = "HTTPS instance disabled; request was not submitted";
                                finish(operation);
                            } else {
                                operation->result.error = "HTTPS connection failed before request submission";
                            }
                        });
                        operation->flow->setOnFlowTerminated([this, weak = std::weak_ptr<Operation>(operation)](auto*) {
                            if (const auto current = weak.lock(); current && current->id == 0) {
                                finish(current);
                            }
                        });
                        if (operation->flow->isTerminated()) {
                            finish(operation);
                        }
                    }
                }
            }
            pumping = false;
        }
    }
} // namespace coreci
