#include "Http.h"
#include "Routers.h"

#include <algorithm>
#include <deque>
#include <strings.h>

namespace coreci::app {
    express::Router webhookRouter(persistence::StateSession& state,
                                  const github::DeclarationFile& registry,
                                  const std::string& secret,
                                  std::function<void(std::function<void(bool)>)> reload) {
        express::Router router;
        auto deliveries = std::make_shared<std::deque<std::string>>();
        router.post(
            "/api/v1/github/webhook",
            [secret](const Req& req, const Res& res, express::Next& next) {
                if (github::verify(secret, std::string(req->body.begin(), req->body.end()), req->get("X-Hub-Signature-256"))) {
                    next();
                } else {
                    answer(res, 401, "unauthorized");
                }
            },
            [&state, registry, reload, deliveries](const Req& req, const Res& res) {
                const auto event = req->get("X-GitHub-Event"), delivery = req->get("X-GitHub-Delivery");
                if (event.empty() || delivery.empty() || !contentType(req, "application/json")) {
                    answer(res, 400, "invalid: webhook headers");
                } else {
                    const auto parsed = github::webhook(std::string(req->body.begin(), req->body.end()), event);
                    if (std::holds_alternative<github::Error>(parsed)) {
                        answer(res, 400, "invalid: " + std::get<github::Error>(parsed).reason);
                    } else if (event == "ping") {
                        answer(res, 200, "pong");
                    } else if (std::find(deliveries->begin(), deliveries->end(), delivery) != deliveries->end()) {
                        answer(res, 200, "ignored: delivery");
                    } else if (std::holds_alternative<std::monostate>(parsed)) {
                        answer(res, 202, "ignored");
                    } else {
                        const auto push = std::get<github::Push>(parsed);

                        const auto* facts = state.state();
                        const auto registryName = registry.owner + "/" + registry.repo;
                        const bool registryPush = !push.branch.empty() && push.branch == push.defaultBranch &&
                                                  strcasecmp(push.repo.c_str(), registryName.c_str()) == 0;
                        bool sourcePush = false;
                        if (facts) {
                            for (const auto& [name, declaration] : facts->at("declarations").items()) {
                                sourcePush =
                                    sourcePush || (strcasecmp(name.c_str(), push.repo.c_str()) == 0 &&
                                                   (push.branch.empty() || push.branch == declaration.at("branch").get<std::string>()));
                            }
                        }
                        if (!registryPush && !facts) {
                            answer(res, 503, "unavailable");
                        } else if (registryPush) {
                            reload([deliveries, delivery](bool success) {
                                if (success) {
                                    deliveries->push_back(delivery);
                                    if (deliveries->size() > 1000) {
                                        deliveries->pop_front();
                                    }
                                }
                            });
                            answer(res, 202, "scheduled");
                        } else if (sourcePush) {
                            reload([deliveries, delivery, res](bool success) {
                                if (success) {
                                    deliveries->push_back(delivery);
                                    if (deliveries->size() > 1000) {
                                        deliveries->pop_front();
                                    }
                                }
                                answer(res, success ? 200 : 503, success ? "accepted" : "unavailable");
                            });
                        } else {
                            answer(res, 202, "ignored");
                        }
                    }
                }
            });
        router.all("/api/v1/github/webhook", method);
        return router;
    }
} // namespace coreci::app
