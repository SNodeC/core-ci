#include "Http.h"
#include "Routers.h"
#include "StatusJson.h"
#include "core/Badge.h"

namespace coreci::app {
    namespace {
        void report(persistence::StateSession& state, const Req& req, const Res& res, bool publication) {
            const auto* engine = state.engine();
            if (!engine) {
                answer(res, 503, "unavailable");
            } else if (!contentType(req, "application/x-www-form-urlencoded")) {
                answer(res, 400, "invalid: content type");
            } else {
                const auto decoded = github::form(std::string(req->body.begin(), req->body.end()));
                if (std::holds_alternative<github::Error>(decoded)) {
                    answer(res, 400, "invalid: " + std::get<github::Error>(decoded).reason);
                } else {
                    const auto id = req->param("id");
                    std::optional<std::uint64_t> owner;
                    if (publication && engine->publications().contains(id)) {
                        owner = engine->publications().at(id).run.runId;
                    } else if (!publication && engine->waves().contains(id)) {
                        owner = engine->waves().at(id).runId;
                    }
                    const auto input = github::report(id, std::get<github::Form>(decoded), now(), owner, publication);
                    if (std::holds_alternative<github::Error>(input)) {
                        answer(res, 400, "invalid: " + std::get<github::Error>(input).reason);
                    } else if (std::holds_alternative<std::monostate>(input)) {
                        answer(res, 200, "acknowledged");
                    } else {
                        state.enqueue(std::get<Event>(input), [res](const auto& result) {
                            const auto [code, message] = response(result);
                            answer(res, code, message);
                        });
                    }
                }
            }
        }
    } // namespace
    express::Router apiRouter(persistence::StateSession& state, const std::string& secret) {
        express::Router router;
        const auto auth = [secret](const Req& req, const Res& res, express::Next& next) {
            if (github::verifyCallback(secret,
                                       std::string(req->body.begin(), req->body.end()),
                                       req->get("X-CoreCI-Timestamp"),
                                       req->get("X-CoreCI-Signature-256"),
                                       now())) {
                next();
            } else {
                answer(res, 401, "unauthorized");
            }
        };
        for (const bool publication : {false, true}) {
            const auto path = std::string("/api/v1/") + (publication ? "publications/" : "waves/") + ":id";
            router.post(path + "/report", auth, [&state, publication](const Req& req, const Res& res) {
                report(state, req, res, publication);
            });
            router.all(path + "/report", method);
            router.head(path + "/plan.json", method);
            router.get(path + "/plan.json", [&state, publication](const Req& req, const Res& res) {
                const auto* engine = state.engine();
                const auto id = req->param("id");
                if (!engine) {
                    answer(res, 503, "unavailable");
                } else if (publication ? !engine->publications().contains(id) : !engine->waves().contains(id)) {
                    answer(res, 404, "not found");
                } else {
                    res->set("Content-Type", "application/json")
                        .set("Cache-Control", "no-store")
                        .send(publication ? publicationPlanJson(engine->publications().at(id), engine->waves())
                                          : planJson(engine->waves().at(id)));
                }
            });
            router.all(path + "/plan.json", method);
        }
        router.head("/api/v1/status.json", method);
        router.get("/api/v1/status.json", [&state](const Req&, const Res& res) {
            if (const auto* engine = state.engine()) {
                res->set("Content-Type", "application/json")
                    .set("Cache-Control", "no-store")
                    .send(statusJson(statusSnapshot(*engine), engine->stored().sequence));
            } else {
                answer(res, 503, "unavailable");
            }
        });
        router.all("/api/v1/status.json", method);
        router.head("/api/v1/badge/:package/:target.svg", method);
        router.get("/api/v1/badge/:package/:target.svg", [&state](const Req& req, const Res& res) {
            const Unit unit{req->param("package"), req->param("target")};
            res->set("Content-Type", "image/svg+xml; charset=utf-8")
                .set("Cache-Control", "no-cache, no-store, must-revalidate")
                .send(state.engine() ? unitBadge(*state.engine(), unit) : badge(unit.package, "unavailable", "#6e7781"));
        });
        router.all("/api/v1/badge/:package/:target.svg", method);
        return router;
    }
} // namespace coreci::app
