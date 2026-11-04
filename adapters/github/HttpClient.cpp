#include "HttpClient.h"

#include <algorithm>
#include <core/timer/Timer.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <web/http/tls/in/Client.h>
namespace coreci::github {
    struct HttpClient::Exchange : public std::enable_shared_from_this<Exchange> {
        using Client = web::http::tls::in::Client;
        std::unique_ptr<Client> client;
        Client::FlowHandle flow;
        std::unique_ptr<core::timer::Timer> timer;
        std::function<void(Response)> callback;
        std::function<void()> remove;
        Request request;
        Url base;
        bool finished = false, verified = false;
        void finish(Response response) {
            if (!finished) {
                finished = true;
                if (timer) {
                    timer->cancel();
                }
                if (flow) {
                    flow->terminateFlow();
                }
                auto done = std::move(callback);
                remove();
                done(std::move(response));
            }
        }
        void start(const std::string& ca) {
            const std::weak_ptr<Exchange> weak = shared_from_this();
            client = std::make_unique<Client>(
                "",
                [weak](const auto& req) {
                    if (const auto self = weak.lock()) {
                        if (self->verified && !self->finished) {
                            req->method = self->request.method;
                            req->url = self->request.path;
                            req->host(self->base.host + ":" + std::to_string(self->base.port));
                            req->set(self->request.headers);
                            req->set("Connection", "close");
                            req->send(
                                self->request.body,
                                [weak](const auto&, const auto& res) {
                                    if (const auto e = weak.lock()) {
                                        Response response;
                                        response.status = std::stoi(res->statusCode);
                                        response.body.assign(res->body.begin(), res->body.end());
                                        for (const auto& [key, value] : res->headers) {
                                            std::string name = key;
                                            for (char& c : name) {
                                                if (c >= 'A' && c <= 'Z') {
                                                    c = static_cast<char>(c - 'A' + 'a');
                                                }
                                            }
                                            response.headers[name] = value;
                                        }
                                        e->finish(std::move(response));
                                    }
                                },
                                [weak](const auto&, const auto&) {
                                    if (const auto e = weak.lock()) {
                                        e->finish({});
                                    }
                                });
                        }
                    }
                },
                [weak](const auto&) {
                    if (const auto e = weak.lock()) {
                        e->finish({});
                    }
                });
            client->getConfig()->setRetry(false);
            client->getConfig()->setReconnect(false);
            client->getConfig()->setSni(base.host);
            if (ca.empty()) {
                client->getConfig()->setCaCertDirUseDefault();
            } else {
                client->getConfig()->setCaCert(ca);
            }
            client->setOnConnected([weak](auto* connection) {
                if (const auto self = weak.lock()) {
                    auto* ssl = connection->getSSL();
                    auto* certificate = SSL_get1_peer_certificate(ssl);
                    self->verified = certificate != nullptr && SSL_get_verify_result(ssl) == X509_V_OK &&
                                     X509_check_host(certificate, self->base.host.c_str(), self->base.host.size(), 0, nullptr) == 1;
                    X509_free(certificate);
                    if (!self->verified) {
                        connection->close();
                        self->finish({});
                    }
                }
            });
            timer = std::make_unique<core::timer::Timer>(core::timer::Timer::singleshotTimer(
                [weak] {
                    if (const auto self = weak.lock()) {
                        self->finish({});
                    }
                },
                30));
            flow = client->connect(base.host, base.port, [weak](const auto&, const auto& state) {
                if (state != core::socket::State::OK) {
                    if (const auto self = weak.lock()) {
                        self->finish({});
                    }
                }
            });
        }
    };
    HttpClient::HttpClient(Url base, std::string ca)
        : base(std::move(base))
        , ca(std::move(ca)) {
    }
    HttpClient::~HttpClient() {
        auto abandoned = std::move(active);
        for (const auto& e : abandoned) {
            e->finished = true;
            if (e->timer) {
                e->timer->cancel();
            }
            if (e->flow) {
                e->flow->terminateFlow();
            }
        }
    }
    void HttpClient::request(Request request, std::function<void(Response)> done) {
        auto e = std::make_shared<Exchange>();
        e->base = base;
        e->request = std::move(request);
        e->callback = std::move(done);
        e->remove = [this, weak = std::weak_ptr<Exchange>(e)] {
            if (const auto item = weak.lock()) {
                std::erase(active, item);
            }
        };
        active.push_back(e);
        e->start(ca);
    }
} // namespace coreci::github
