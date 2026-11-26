#include "Dashboard.h"

#include "DashboardAssets.h"
#include "Http.h"
#include "adapters/dashboard/Model.h"
#include "auth/OAuth2CallbackHandler.h"
#include "web/http/server/SocketContext.h"

namespace coreci::app {
    namespace {
        void result(const Dashboard::Res& res, int status, const std::string& message) {
            res->status(status)
                .set("Content-Type", "application/json")
                .set("Cache-Control", "no-store")
                .send(dashboard::result(message, status >= 400));
        }
    } // namespace
    Dashboard::Dashboard(std::shared_ptr<snodec::auth::OAuthClient> client,
                         std::set<std::string> users,
                         persistence::StateSession& queue,
                         std::function<void(std::function<void(bool)>)> reload,
                         std::function<std::uint64_t()> generation)
        : client(std::move(client))
        , users(std::move(users))
        , queue(queue)
        , reload(std::move(reload))
        , generation(std::move(generation)) {
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
    void Dashboard::loaded(const github::LoadedDeclarations& value) {
        declarations = value;
        loadError.clear();
        ready = true;
        changed();
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
            } else if (operation == "retry" || operation == "rebuild" || operation == "dispatch-paused") {
                const auto parsed = dashboard::operation(operation, std::string(req->body.begin(), req->body.end()), now());
                if (std::holds_alternative<github::Error>(parsed)) {
                    result(res, 400, std::get<github::Error>(parsed).reason);
                } else {
                    const auto apply = [this, res, event = std::get<Event>(parsed)] {
                        queue.enqueue(event, [res](const auto& value) {
                            const auto [code, text] = response(value);
                            result(res, code, text);
                        });
                    };
                    if (operation == "dispatch-paused" || !queue.engine()) {
                        apply();
                    } else {
                        reload([apply, res](bool loaded) {
                            if (loaded) {
                                apply();
                            } else {
                                result(res, 503, "Declaration load failed");
                            }
                        });
                    }
                }
            } else if (operation == "reload" || operation == "reconcile") {
                reload([res](bool loaded) {
                    result(res, loaded ? 200 : 503, loaded ? "Declarations loaded" : "Declaration load failed");
                });
            } else {
                result(res, 400, "Not supported by core-ci: " + operation + " has no dashboard operation");
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
        const auto* engine = queue.engine();
        if (engine) {
            const bool update = dirty;
            dirty = false;
            for (const auto& stream : streams) {
                if (stream->res && stream->res->isConnected()) {
                    if (update && (stream->pending || !stream->authorized)) {
                        stream->snapshot = true;
                    }
                    if (stream->authorized && !stream->pending && (stream->snapshot || update)) {
                        stream->res->sendFragment(
                            std::string("event: ") + (stream->snapshot ? "snapshot" : "update") +
                            "\ndata: " + dashboard::model(*engine, declarations, generation(), ready, stream->email, loadError) + "\n\n");
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
