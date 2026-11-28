#include "Dashboard.h"

#include "DashboardAssets.h"
#include "Http.h"
#include "auth/OAuth2CallbackHandler.h"
#include "web/http/server/SocketContext.h"

namespace coreci::app {
    namespace {
        void result(const Dashboard::Res& res, int status, const std::string& message) {
            res->status(status)
                .set("Content-Type", "application/json")
                .set("Cache-Control", "no-store")
                .send(Json{{"message", message}, {"error", status >= 400}}.dump());
        }
    } // namespace
    Dashboard::Dashboard(std::shared_ptr<snodec::auth::OAuthClient> client,
                         std::set<std::string> users,
                         persistence::StateSession& queue,
                         std::function<void(std::function<void(bool)>)> reload,
                         std::function<void(const Json&, persistence::StateSession::Reply)> admit)
        : client(std::move(client))
        , users(std::move(users))
        , queue(queue)
        , reload(std::move(reload))
        , admit(std::move(admit)) {
        timer = std::make_unique<core::timer::Timer>(core::timer::Timer::intervalTimer(
            [this] {
                tick();
            },
            1));
    }
    Dashboard::~Dashboard() {
        timer->cancel();
    }
    void Dashboard::failed(const std::string& message) {
        loadError = message;
        changed();
    }
    void Dashboard::changed() {
        dirty = true;
    }
    void Dashboard::routes(const express::Router& router) {
        const snodec::auth::middleware::JwtAuthMiddleware auth(client);
        const auto gate = [this](const auto& req, const Res& res, express::Next& next) {
            bool allowed = false;
            req->template getAttribute<snodec::auth::UserContext>([&](const auto& user) {
                allowed = users.contains(user.id);
            });
            if (allowed) {
                next();
            } else {
                res->status(403).send("Forbidden");
            }
        };
        router.get("/auth/callback", snodec::auth::callback::OAuth2CallbackHandler(client));
        for (const auto& [path, type, bytes] : assets) {
            router.get(path, auth, gate, [type, bytes](const auto&, const Res& res) {
                res->set("Content-Type", type).set("Cache-Control", "no-cache").send(bytes);
            });
            router.all(path, auth, gate, [](const auto&, const Res& res) {
                res->status(405).send("Method not allowed");
            });
        }

        router.get("/events", auth, gate, [this](const auto& req, const Res& res) {
            std::erase_if(streams, [](const auto& stream) {
                return !stream->res || !stream->res->isConnected();
            });
            if (streams.size() >= 64) {
                res->status(503).send("Too many streams");
            } else {
                auto stream = std::make_shared<Stream>();
                stream->res = res;
                const auto header = req->get("Authorization");
                stream->bearer = header.starts_with("Bearer ");
                stream->token = stream->bearer ? header.substr(7) : req->cookie(client->cookieName());
                req->template getAttribute<snodec::auth::UserContext>([&](const auto& user) {
                    stream->email = user.email;
                });
                stream->checked = stream->keepAlive = std::chrono::steady_clock::now();
                streams.push_back(stream);
                res->set("Content-Type", "text/event-stream")
                    .set("Cache-Control", "no-store")
                    .set("Connection", "close")
                    .set("Transfer-Encoding", "chunked")
                    .sendHeader();
            }
        });
        router.all("/events", auth, gate, [](const auto&, const Res& res) {
            res->status(405).send("Method not allowed");
        });
        router.post("/api/:operation", auth, gate, [this](const auto& req, const Res& res) {
            const auto operation = req->param("operation");
            if (!contentType(req, "application/json")) {
                result(res, 400, "invalid content type");
            } else if (operation == "reconcile") {
                reload([res](bool loaded) {
                    result(res, loaded ? 200 : 503, loaded ? "Reconciled" : "Reconciliation failed");
                });
            } else {
                try {
                    const auto body = Json::parse(std::string(req->body.begin(), req->body.end()));
                    auto reply = [res](const auto& value) {
                        result(res, value.code, value.text);
                    };
                    if (operation == "admit-tag") {
                        admit(body, std::move(reply));
                    } else {
                        queue.enqueue(
                            operation,
                            [this, operation, body](auto& state) {
                                return queue.scheduler().action(state, operation, body);
                            },
                            std::move(reply));
                    }
                } catch (const std::exception&) {
                    result(res, 400, "invalid dashboard request");
                }
            }
        });
        router.all("/api/:operation", auth, gate, [](const auto&, const Res& res) {
            res->status(405).send("Method not allowed");
        });
    }
    void Dashboard::tick() {
        const auto at = std::chrono::steady_clock::now();
        std::erase_if(streams, [](const auto& stream) {
            return !stream->res || !stream->res->isConnected();
        });
        for (const auto& stream : streams) {
            if (!stream->pending &&
                (at - stream->checked >= std::chrono::seconds(59) || (!stream->bearer && client->renewalDue(stream->token)))) {
                stream->pending = true;
                stream->checked = at;
                const auto done = [allowed = users, weak = std::weak_ptr<Stream>(stream)](auto status, const auto& claims) {
                    if (const auto current = weak.lock()) {
                        current->pending = false;
                        if (status == snodec::auth::OAuthClient::Authorization::Rejected ||
                            (status == snodec::auth::OAuthClient::Authorization::Accepted && !allowed.contains(claims.subject))) {
                            current->res->sendFragment("");
                            current->res->getSocketContext()->shutdownWrite();
                            current->res.reset();
                        } else {
                            const bool accepted = status == snodec::auth::OAuthClient::Authorization::Accepted;
                            current->snapshot = current->snapshot || (accepted && !current->authorized);
                            current->authorized = accepted;
                        }
                    }
                };
                client->authenticate(stream->token, stream->bearer, done);
            }
        }
        const auto* state = queue.state();
        if (state) {
            const bool update = dirty;
            dirty = false;
            for (const auto& stream : streams) {
                if (stream->res && stream->res->isConnected()) {
                    if (update && (stream->pending || !stream->authorized)) {
                        stream->snapshot = true;
                    }
                    if (stream->authorized && !stream->pending && (stream->snapshot || update)) {
                        auto model = view(*state, queue.scheduler().options());
                        model["email"] = stream->email;
                        model["load_error"] = loadError;
                        stream->res->sendFragment(std::string("event: ") + (stream->snapshot ? "snapshot" : "update") +
                                                  "\ndata: " + model.dump() + "\n\n");
                        stream->snapshot = false;
                    }
                    if (at - stream->keepAlive >= std::chrono::seconds(30)) {
                        stream->res->sendFragment(": keep-alive\n\n");
                        stream->keepAlive = at;
                    }
                }
            }
        }
    }
} // namespace coreci::app
