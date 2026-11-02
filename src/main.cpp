#include "Auth.h"
#include "Controller.h"
#include "Managed.h"

#include <Log.h>
#include <algorithm>
#include <core/DescriptorEventReceiver.h>
#include <core/SNodeC.h>
#include <core/socket/stream/SocketConnection.h>
#include <core/timer/Timer.h>
#include <express/legacy/in/WebApp.h>
#include <express/tls/in/WebApp.h>
#include <fstream>
#include <map>
#include <regex>
#include <utils/Config.h>
#include <web/http/server/SocketContext.h>

namespace {

    void report(const std::string& error) {
        if (!error.empty()) {
            snode::log::application().error() << error;
        }
    }

    void sendEvent(const std::shared_ptr<express::Response>& response, std::size_t snapshotEnd, const std::string& event) {
        if (response->isConnected()) {
            auto* connection = response->getSocketContext()->getSocketConnection();
            const auto pending = connection->getTotalQueued() - std::max(connection->getTotalSent(), snapshotEnd);
            // Reserve the maximum hexadecimal HTTP chunk length plus its two CRLF pairs.
            const auto framed = event.size() + 2 * sizeof(std::size_t) + 4;
            if (pending > 65536 || framed > 65536 - pending) {
                connection->close();
            } else {
                response->sendFragment(event);
            }
        }
    }

} // namespace

int main(int argc, char* argv[]) {
    std::string deploymentFile;
    std::string appId;
    std::string installation;
    std::string keyFile;
    std::string webhookFile;
    std::string database;
    std::string username;
    std::string passwordFile;
    std::string socket;
    std::string managedDirectory;
    std::string uiRoot = CORE_CI_UI_ROOT;
    auto& config = utils::Config::configRoot;
    for (const auto& option : std::vector<std::pair<std::string, std::string*>>{{"deployment-file", &deploymentFile},
                                                                                {"app-id", &appId},
                                                                                {"installation-id", &installation},
                                                                                {"app-key-file", &keyFile},
                                                                                {"webhook-secret-file", &webhookFile},
                                                                                {"database", &database},
                                                                                {"database-user", &username},
                                                                                {"database-password-file", &passwordFile},
                                                                                {"database-socket", &socket},
                                                                                {"managed-datadir", &managedDirectory},
                                                                                {"ui-root", &uiRoot}}) {
        config.addOptionVariable(
            "--" + option.first, *option.second, option.first, "TEXT", *option.second, CLI::TypeValidator<std::string>());
    }
    express::legacy::in::WebApp web("web");
    express::tls::in::WebApp webhook("webhook");
    web.getConfig()->setReuseAddress();
    webhook.getConfig()->setReuseAddress();
    web.getConfig()->Local::setHost("127.0.0.1")->setPort(8080);
    webhook.getConfig()->Local::setHost("127.0.0.1")->setPort(8443);
    webhook.getConfig()->Instance::setDisabled(true);

    ci::Store store;
    ci::ManagedDatabase managed;
    std::unique_ptr<ci::Controller> controller;
    const auto release = [&]() {
        controller.reset();
        store.db.reset();
        managed.stop();
    };
    std::map<std::shared_ptr<express::Response>, std::size_t> clients;
    core::SNodeC::init(argc, argv);
    try {
        ci::require(!deploymentFile.empty() && !database.empty() && !username.empty() && !socket.empty(),
                    "Explicit deployment/database inputs required");
        std::ifstream input(deploymentFile);
        const ci::Json deployment = ci::Json::parse(input);
        ci::require(!appId.empty() && !installation.empty() && deployment.at("repositories").is_object() &&
                        !deployment.at("repositories").empty(),
                    "GitHub App identities and repository grants required");
        for (const auto& [repository, id] : deployment.at("repositories").items()) {
            ci::require(std::regex_match(repository, std::regex("[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+")) && id.is_number_unsigned() &&
                            id.get<std::uint64_t>() > 0 && repository.starts_with(deployment.at("owner").get<std::string>() + "/"),
                        "Invalid repository grant");
        }
        for (const auto& pin : {deployment.at("worker").at("tooling_sha"),
                                deployment.at("source_workflow").at("blob_sha"),
                                deployment.at("publication_workflow").at("blob_sha")}) {
            ci::require(std::regex_match(pin.get<std::string>(), std::regex("[a-f0-9]{40}")), "Approved immutable pins required");
        }
        ci::require(deployment.at("repositories").at(deployment.at("worker").at("repository").get<std::string>()) ==
                        deployment.at("worker").at("repository_id"),
                    "Worker grant identity mismatch");
        ci::require(deployment.at("feed").at("base_url").get<std::string>().starts_with("https://"), "HTTPS feed required");
        const std::string key = ci::protectedFile(keyFile);
        const std::string secret = ci::protectedFile(webhookFile);
        ci::require(secret.size() >= 32, "Webhook secret must contain at least 32 bytes");
        ci::appJwt(key, appId);

        const database::mariadb::MariaDBConnectionDetails details = {.connectionName = "controller",
                                                                     .hostname = "localhost",
                                                                     .username = username,
                                                                     .password =
                                                                         passwordFile.empty() ? "" : ci::protectedFile(passwordFile),
                                                                     .database = database,
                                                                     .port = 0,
                                                                     .socket = socket,
                                                                     .flags = 0};
        store.db = std::make_unique<database::mariadb::MariaDBClient>(
            details,
            [&, deployment, key, secret](const database::mariadb::MariaDBState& state) {
                if (state.connected && !controller) {
                    controller = std::make_unique<ci::Controller>(store, deployment, appId, installation, key);
                    store.initialize([&, secret](const std::string& error) {
                        if (!error.empty()) {
                            report(error);
                            release();
                        } else {
                            store.dispatch = [&](const ci::Json& actions) {
                                controller->send(actions);
                            };
                            store.changed = [&](const ci::Json& updates) {
                                const std::string event = "data: " + updates.dump() + "\n\n";
                                const auto connected = clients;
                                for (const auto& [client, snapshotEnd] : connected) {
                                    sendEvent(client, snapshotEnd, event);
                                }
                            };
                            web.get("/api/graph", [&](const auto&, const auto& response) {
                                store.read([response](const ci::Json& snapshot, const std::string& error) {
                                    if (error.empty()) {
                                        response->json(snapshot.at("graph"));
                                    } else {
                                        response->status(503).json({{"error", error}});
                                    }
                                });
                            });
                            web.get("/events", [&](const auto&, const auto& response) {
                                store.read([&, response](const ci::Json& snapshot, const std::string& error) {
                                    if (error.empty() && response->isConnected()) {
                                        response->getSocketContext()->getSocketConnection()->setReadTimeout(
                                            core::DescriptorEventReceiver::TIMEOUT::DISABLE);
                                        const auto heartbeat = std::make_shared<core::timer::Timer>(core::timer::Timer::intervalTimer(
                                            [&, weak = std::weak_ptr<express::Response>(response)] {
                                                const auto client = weak.lock();
                                                if (client && clients.contains(client)) {
                                                    sendEvent(client, clients.at(client), ": keep-alive\n\n");
                                                }
                                            },
                                            30));
                                        response->getSocketContext()->setOnDisconnected([&, client = response.get(), heartbeat]() {
                                            heartbeat->cancel();
                                            std::erase_if(clients, [client](const auto& candidate) {
                                                return candidate.first.get() == client;
                                            });
                                        });
                                        response->type("text/event-stream").set("Cache-Control", "no-store").sendHeader();
                                        response->sendFragment("data: " + snapshot.at("graph").dump() + "\n\n");
                                        if (response->isConnected()) {
                                            clients.emplace(response,
                                                            response->getSocketContext()->getSocketConnection()->getTotalQueued());
                                        }
                                    } else if (response->isConnected()) {
                                        response->sendStatus(503);
                                    }
                                });
                            });
                            for (const std::string operation : {"enroll", "refresh", "request", "retry", "reconcile", "retire"}) {
                                web.post("/api/" + operation, [&, operation](const auto& request, const auto& response) {
                                    const ci::Done done = [response](const std::string& error) {
                                        response->status(error.empty() ? 200 : 409).json({{"error", error}});
                                    };
                                    try {
                                        ci::require(request->get("Content-Type").starts_with("application/json"), "JSON request required");
                                        const auto body = ci::Json::parse(request->body);
                                        if (operation == "enroll" || operation == "refresh") {
                                            controller->enroll(body.at("repository"), body.at("ref"), done);
                                        } else if (operation == "request") {
                                            controller->request(body.at("repository"), body.value("tag", ""), done);
                                        } else if (operation == "reconcile") {
                                            controller->reconcile(body.at("execution"), done);
                                        } else {
                                            store.change(
                                                [operation, body](ci::Json& graph) {
                                                    ci::Json authorized = ci::Json::array();
                                                    if (operation == "retry") {
                                                        ci::retry(graph, body.at("execution"));
                                                        authorized = ci::commands(graph);
                                                    } else {
                                                        ci::retire(graph, body.at("execution"), body.at("acknowledgement"));
                                                    }
                                                    return authorized;
                                                },
                                                done);
                                        }
                                    } catch (const std::exception& failure) {
                                        done(failure.what());
                                    }
                                });
                            }
                            webhook.post("/github/webhook", [&, secret](const auto& request, const auto& response) {
                                try {
                                    const std::string body(request->body.begin(), request->body.end());
                                    ci::require(ci::validWebhook(secret, body, request->get("X-Hub-Signature-256")),
                                                "Invalid GitHub signature");
                                    if (request->get("X-GitHub-Event") == "ping") {
                                        response->sendStatus(200);
                                    } else {
                                        controller->webhook(request->get("X-GitHub-Event"), ci::Json::parse(body), report);
                                        response->sendStatus(202);
                                    }
                                } catch (const std::exception&) {
                                    response->sendStatus(401);
                                }
                            });
                            for (const auto& [route, file] : std::map<std::string, std::string>{
                                     {"/", "index.html"}, {"/app.js", "app.js"}, {"/style.css", "style.css"}}) {
                                web.get(route, [&, file](const auto&, const auto& response) {
                                    response->sendFile(uiRoot + "/" + file, [response](int error) {
                                        if (error != 0) {
                                            response->sendStatus(404);
                                        }
                                    });
                                });
                            }
                            web.listen([&](const auto&, const core::socket::State& state) {
                                if (state == core::socket::State::OK) {
                                    webhook.listen([](const auto&, const core::socket::State& state) {
                                        if (state == core::socket::State::ERROR || state == core::socket::State::FATAL) {
                                            report(state.what());
                                        }
                                    });
                                } else if (state == core::socket::State::ERROR || state == core::socket::State::FATAL) {
                                    report(state.what());
                                    release();
                                }
                            });
                        }
                    });
                } else if (state.error != 0) {
                    report(state.errorMessage);
                    if (!controller) {
                        release();
                    }
                }
            },
            "database");
        if (managedDirectory.empty()) {
            store.db->connect();
        } else {
            managed.start(managedDirectory, socket, [&]() {
                store.db->connect();
            });
        }
    } catch (const std::exception& error) {
        report(error.what());
        release();
    }
    return core::SNodeC::start();
}
