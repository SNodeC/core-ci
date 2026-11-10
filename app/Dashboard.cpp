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
                         persistence::EventQueue& queue,
                         github::Url base,
                         github::Fetch fetch,
                         std::function<void(std::function<void(bool)>)> reload,
                         std::function<std::uint64_t()> generation)
        : client(std::move(client))
        , users(std::move(users))
        , queue(queue)
        , base(std::move(base))
        , fetch(std::move(fetch))
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
    void Dashboard::changed() {
        dirty = true;
    }
    void Dashboard::loaded(const github::LoadedDeclarations& value) {
        declarations = value;
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
        router.get("/favicon.ico", auth, gate, [](const auto&, const Res& res) {
            res->status(204).end();
        });
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
                stream->token = header.starts_with("Bearer ") ? header.substr(7) : req->cookie(client->cookieName());
                req->template getAttribute<snodec::auth::UserContext>([&](const auto& user) {
                    stream->email = user.email;
                });
                stream->checked = stream->keepAlive = now();
                streams.push_back(stream);
                res->set("Content-Type", "text/event-stream")
                    .set("Cache-Control", "no-store")
                    .set("Connection", "close")
                    .set("Transfer-Encoding", "chunked")
                    .sendHeader();
                if (queue.engine()) {
                    res->sendFragment("event: snapshot\ndata: " +
                                      dashboard::model(*queue.engine(), declarations, jobs, generation(), ready, stream->email) + "\n\n");
                    stream->snapshot = false;
                }
            }
        });
        router.all("/events", auth, gate, [](const auto&, const Res& res) {
            res->status(405).send("Method not allowed");
        });
        router.post("/api/:operation", auth, gate, [this](const auto& req, const Res& res) {
            const auto operation = req->param("operation");
            if (!contentType(req, "application/json")) {
                result(res, 400, "invalid content type");
            } else if (operation == "retry") {
                const auto parsed = dashboard::retry(std::string(req->body.begin(), req->body.end()));
                if (std::holds_alternative<github::Error>(parsed)) {
                    result(res, 400, std::get<github::Error>(parsed).reason);
                } else {
                    const auto unit = std::get<Unit>(parsed);
                    queue.enqueue(RetryRequested{now(), unit.package, unit.target}, [res](const auto& value) {
                        const auto [code, text] = response(value);
                        result(res, code, text);
                    });
                }
            } else if (operation == "refresh") {
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
        const auto at = now();
        std::erase_if(streams, [](const auto& stream) {
            return !stream->res || !stream->res->isConnected();
        });
        for (const auto& stream : streams) {
            if (!stream->pending && at - stream->checked >= 60) {
                stream->pending = true;
                stream->checked = at;
                client->authorize(stream->token,
                                  [allowed = users, weak = std::weak_ptr<Stream>(stream)](bool accepted, const auto& claims) {
                                      if (const auto current = weak.lock()) {
                                          current->pending = false;
                                          if (!accepted || !allowed.contains(claims.subject)) {
                                              current->res->sendFragment("");
                                              current->res->getSocketContext()->shutdownWrite();
                                              current->res.reset();
                                          }
                                      }
                                  });
            }
        }
        const auto* engine = queue.engine();
        if (engine) {
            if (ready) {
                jobs.refresh(dashboard::jobRequests(*engine), at, base, fetch, [this] {
                    changed();
                });
            }
            const bool update = dirty;
            dirty = false;
            for (const auto& stream : streams) {
                if (stream->res && stream->res->isConnected()) {
                    if (stream->snapshot || update) {
                        stream->res->sendFragment(
                            std::string("event: ") + (stream->snapshot ? "snapshot" : "update") +
                            "\ndata: " + dashboard::model(*engine, declarations, jobs, generation(), ready, stream->email) + "\n\n");
                        stream->snapshot = false;
                    }
                    if (at - stream->keepAlive >= 30) {
                        stream->res->sendFragment(": keep-alive\n\n");
                        stream->keepAlive = at;
                    }
                }
            }
        }
    }
} // namespace coreci::app
