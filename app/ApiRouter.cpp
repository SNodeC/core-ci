#include "Http.h"
#include "Routers.h"
#include "core/Badge.h"

#include <charconv>
#include <limits>

namespace coreci::app {
    namespace {
        unsigned long long positive(const std::string& text, unsigned long long upper = std::numeric_limits<unsigned long long>::max()) {
            unsigned long long value = 0;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0 || value > upper) {
                throw std::invalid_argument("invalid positive integer");
            }
            return value;
        }

        Json report(const github::Form& fields, bool claim, bool publication) {
            Json result{{"attempt", positive(fields.at("attempt"), std::numeric_limits<unsigned>::max())},
                        {"run_id", positive(fields.at("run_id"))},
                        {"run_attempt", positive(fields.at("run_attempt"))}};
            if (!claim) {
                const auto& status = fields.at("status");
                if (status == "finished") {
                    const auto& outcome = fields.at("outcome");
                    if (outcome != "failed" && outcome != (publication ? "published" : "built")) {
                        throw std::invalid_argument("invalid outcome");
                    }
                    result["outcome"] = outcome;
                    if (outcome == "built") {
                        const auto& digest = fields.at("artifact_digest");
                        const auto inventory = Json::parse(fields.at("inventory"));
                        if (digest.size() != 64 || digest.find_first_not_of("0123456789abcdef") != std::string::npos ||
                            !inventory.is_array() || inventory.empty()) {
                            throw std::invalid_argument("invalid build artifact");
                        }
                        result["artifact_id"] = positive(fields.at("artifact_id"));
                        result["artifact_digest"] = digest;
                        result["inventory"] = inventory;
                    }
                    if (fields.contains("reason")) {
                        result["reason"] = fields.at("reason");
                    }
                    if (fields.contains("timings")) {
                        result["timings"] = Json::parse(fields.at("timings"));
                    }
                } else if (publication && status == "item") {
                    result["receipt"] = Json::parse(fields.at("receipt"));
                } else if (status != "started") {
                    throw std::invalid_argument("invalid report status");
                }
                result["status"] = status;
            }
            return result;
        }
    } // namespace

    express::Router apiRouter(persistence::StateSession& state, const std::string& secret) {
        express::Router router;
        for (const bool publication : {false, true}) {
            const auto route = std::string("/api/v1/") + (publication ? "publications/" : "requests/") + ":id";
            for (const bool claim : {false, true}) {
                const auto endpoint = route + (claim ? "/claim" : "/report");
                router.post(endpoint, [&state, secret, publication, claim](const Req& req, const Res& res) {
                    const std::string body(req->body.begin(), req->body.end());
                    const auto timestamp = req->get("X-CoreCI-Timestamp");
                    if (!github::verifyCallback(secret, body, timestamp, req->get("X-CoreCI-Signature-256"), now())) {
                        answer(res, 401, "unauthorized");
                    } else {
                        try {
                            const auto form = github::form(body);
                            if (!contentType(req, "application/x-www-form-urlencoded") || std::holds_alternative<github::Error>(form)) {
                                throw std::invalid_argument("invalid form");
                            }
                            const auto input = report(std::get<github::Form>(form), claim, publication);
                            const auto id = req->param("id");
                            state.enqueue(
                                claim ? "claim " + id : "report " + id,
                                [&, input, id, claim, publication](auto& facts) {
                                    const auto& scheduler = state.scheduler();
                                    return claim ? scheduler.claim(facts, publication, id, input, now())
                                                 : scheduler.report(facts, publication, id, input, now());
                                },
                                [res, secret, claim](const auto& result) {
                                    if (claim && result.code == 200) {
                                        const auto stamp = std::to_string(now());
                                        res->set("Content-Type", "application/json")
                                            .set("Cache-Control", "no-store")
                                            .set("X-CoreCI-Timestamp", stamp)
                                            .set("X-CoreCI-Signature-256", github::hmac(secret, stamp + "." + result.text))
                                            .send(result.text);
                                    } else {
                                        answer(res, result.code, result.text);
                                    }
                                });
                        } catch (const std::exception&) {
                            answer(res, 400, "invalid form");
                        }
                    }
                });
                router.all(endpoint, method);
            }
            const auto document = route + "/attempts/:attempt";
            router.head(document, method);
            router.get(document, [&state, publication](const Req& req, const Res& res) {
                const auto* facts = state.state();
                const auto key = req->param("id") + "/" + req->param("attempt");
                if (!facts) {
                    answer(res, 503, "unavailable");
                } else {
                    const auto& attempts = facts->at("attempts");
                    if (attempts.contains(key) && attempts.at(key).at("publication") == publication &&
                        attempts.at(key).at("document") != "") {
                        res->set("Content-Type", "application/json")
                            .set("Cache-Control", "no-store")
                            .send(attempts.at(key).at("document").get<std::string>());
                    } else {
                        answer(res, 404, "not found");
                    }
                }
            });
            router.all(document, method);
        }
        router.head("/api/v1/status.json", method);
        router.get("/api/v1/status.json", [&state](const Req&, const Res& res) {
            if (const auto* facts = state.state()) {
                res->set("Content-Type", "application/json")
                    .set("Cache-Control", "no-store")
                    .send(view(*facts, state.scheduler().options()).dump());
            } else {
                answer(res, 503, "unavailable");
            }
        });
        router.all("/api/v1/status.json", method);
        const auto badgePath = "/api/v1/badge/:package/:target.svg";
        router.head(badgePath, method);
        router.get(badgePath, [&state](const Req& req, const Res& res) {
            const auto package = req->param("package");
            const auto target = req->param("target");
            std::string message = "unavailable";
            if (const auto* facts = state.state()) {
                message = "unknown";
                unsigned order = 0;
                for (const auto& item : facts->at("items")) {
                    if (item.at("package") == package && item.at("target") == target && item.at("order").get<unsigned>() >= order) {
                        order = item.at("order").get<unsigned>();
                        message = item.at("state").get<std::string>();
                        if (item.value("condition", "") != "") {
                            message = item.at("condition").get<std::string>();
                        }
                    }
                }
            }
            res->set("Content-Type", "image/svg+xml; charset=utf-8")
                .set("Cache-Control", "no-store")
                .send(badge(package, message, message == "published" ? "#207544" : "#6e7781"));
        });
        router.all(badgePath, method);
        return router;
    }
} // namespace coreci::app
