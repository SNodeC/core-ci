#include "Database.h"
#include "DatabaseConfig.h"
#include "Enrollment.h"
#include "GitHub.h"
#include "ManagedDatabase.h"
#include "Pipeline.h"
#include "PipelineConfig.h"
#include "UiAssets.h"

#include <Log.h>
#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <database/mariadb/MariaDBClient.h>
#include <exception>
#include <express/Response.h>
#include <express/legacy/in/WebApp.h>
#include <express/tls/in/WebApp.h>
#include <list>
#include <memory>
#include <utility>
#include <utils/Config.h>
#include <web/http/ConfigHttpParser.h>
#include <web/http/server/ConfigHttpServer.h>
#include <web/http/server/SocketContext.h>

int main(int argc, char* argv[]) {
    auto* databaseConfig = utils::Config::configRoot.newSubCommand<coreci::DatabaseConfig>();

    auto* githubConfig = utils::Config::configRoot.newSubCommand<coreci::GitHubConfig>();
    coreci::GitHub github(*githubConfig);
    coreci::SourceReader sources(github);

    auto* pipelineConfig = utils::Config::configRoot.newSubCommand<coreci::PipelineConfig>();

    coreci::FeedProof feeds(*pipelineConfig, github, sources);

    const express::legacy::in::WebApp app("web");
    app.getConfig()->setReuseAddress();
    app.getConfig()->Local::setHost("127.0.0.1")->setPort(8080);

    app.getConfig()
        ->net::config::ConfigInstance::getSubCommand<web::http::server::ConfigHttpServer>()
        ->getParserConfig()
        ->setMaximumBodyBytes(2 * 1024 * 1024);

    const express::tls::in::WebApp webhooks("webhook");
    webhooks.getConfig()->setDisabled();
    webhooks.getConfig()->setReuseAddress();
    webhooks.getConfig()->Local::setHost("127.0.0.1")->setPort(8443);
    webhooks.getConfig()
        ->net::config::ConfigInstance::getSubCommand<web::http::server::ConfigHttpServer>()
        ->getParserConfig()
        ->setMaximumBodyBytes(2 * 1024 * 1024);

    core::SNodeC::init(argc, argv);

    std::unique_ptr<coreci::ManagedDatabase> managed;
    std::unique_ptr<database::mariadb::MariaDBClient> database;
    std::unique_ptr<coreci::Database> store;
    std::unique_ptr<coreci::GraphStore> graphStore;
    std::unique_ptr<coreci::Enrollment> enrollment;
    std::unique_ptr<coreci::Pipeline> pipeline;
    bool initialConnection = true;
    std::list<std::shared_ptr<express::Response>> eventReceivers;
    const auto sendEvent = [](const auto& response, const std::string& event, const nlohmann::json& data) {
        if (response->isConnected()) {
            response->sendFragment("event:" + event);
            response->sendFragment("data:" + data.dump());
            response->sendFragment();
        }
    };

    try {
        github.loadCredentials();
        feeds.configure();

        database = std::make_unique<database::mariadb::MariaDBClient>(
            databaseConfig->connection(), [&](const database::mariadb::MariaDBState& state) {
                if (state.error != 0) {
                    snode::log::application().error() << "MariaDB connection failed: " << state.errorMessage << " [" << state.error << "]";
                    if (std::exchange(initialConnection, false)) {
                        pipeline.reset();
                        enrollment.reset();
                        graphStore.reset();
                        store.reset();
                        database.reset();
                        if (managed) {
                            managed->stop();
                        }
                    }
                } else if (state.connected && std::exchange(initialConnection, false)) {
                    snode::log::application().info() << "Initial MariaDB connection established";

                    enrollment->restore([&](const coreci::EnrollmentResult& restored) {
                        if (restored.status != 200) {
                            snode::log::application().error() << "Database schema/configuration failed: " << restored.body.dump();
                            pipeline.reset();
                            enrollment.reset();
                            graphStore.reset();
                            store.reset();
                            database.reset();
                            if (managed) {
                                managed->stop();
                            }
                        } else {
                            webhooks.post("/github/webhook", [&] APPLICATION(req, res) {
                                pipeline->event(req->get("X-GitHub-Event"),
                                                req->get("X-Hub-Signature-256"),
                                                std::string(req->body.begin(), req->body.end()),
                                                [res](const coreci::PipelineResult& result) {
                                                    res->status(result.status).type("application/json").send(result.body.dump());
                                                });
                            });

                            const auto publicStatus = [&] APPLICATION(req, res) {
                                res->set("Cache-Control", "no-store").type("application/json");
                                if (pipelineConfig->statusWorkflow.empty()) {
                                    res->status(404).send(R"({"error":"public status is disabled"})");
                                } else if (!store->mutationsAllowed()) {
                                    res->status(503).send(R"({"error":"graph authority is unavailable"})");
                                } else {
                                    auto facts = graphStore->publicView();
                                    facts["feed"] = {{"repository", pipelineConfig->feedRepository},
                                                     {"repository_id", pipelineConfig->feedId},
                                                     {"branch", pipelineConfig->feedBranch}};
                                    res->send(facts.dump());
                                }
                            };
                            webhooks.get("/public/status", publicStatus);
                            app.get("/public/status", publicStatus);

                            app.use([&] MIDDLEWARE(req, res, next) {
                                res->set("Cache-Control", "no-store");
                                res->set("X-Content-Type-Options", "nosniff");
                                res->set("Content-Security-Policy",
                                         "default-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'");
                                if (req->method == "POST" && req->get("Content-Type").find("application/json") != 0) {
                                    res->status(403).send(R"({"error":"configuration writes require JSON"})");
                                } else {
                                    next();
                                }
                            });

                            app.get("/", [] APPLICATION(req, res) {
                                res->type("text/html").send(coreci::ui::html);
                            });
                            app.get("/style.css", [] APPLICATION(req, res) {
                                res->type("text/css").send(coreci::ui::css);
                            });
                            app.get("/app.js", [] APPLICATION(req, res) {
                                res->type("application/javascript").send(coreci::ui::javascript);
                            });

                            app.get("/api/graph", [&] APPLICATION(req, res) {
                                res->type("application/json").send(graphStore->view().dump());
                            });

                            graphStore->onChanged = [&](bool committed) {
                                if (committed) {
                                    pipeline->refreshStatus();
                                }
                                const auto receivers = eventReceivers;
                                const auto graph = graphStore->view();
                                for (const auto& response : receivers) {
                                    sendEvent(response, "graph", graph);
                                }
                                if (!receivers.empty()) {
                                    enrollment->list([receivers, sendEvent](const coreci::EnrollmentResult& result) {
                                        for (const auto& response : receivers) {
                                            sendEvent(response, result.status == 200 ? "repositories" : "unavailable", result.body);
                                        }
                                    });
                                }
                            };

                            app.get("/api/events", [&] APPLICATION(req, res) {
                                res->set("Content-Type", "text/event-stream").set("Connection", "keep-alive").sendHeader();
                                eventReceivers.push_back(res);
                                const auto heartbeat = std::make_shared<core::timer::Timer>(core::timer::Timer::intervalTimer(
                                    [response = std::weak_ptr<express::Response>(res)] {
                                        const auto receiver = response.lock();
                                        if (receiver && receiver->isConnected()) {
                                            receiver->sendFragment(":keep-alive");
                                            receiver->sendFragment();
                                        }
                                    },
                                    30));
                                res->getSocketContext()->setOnDisconnected([&, receiver = res.get(), heartbeat]() {
                                    heartbeat->cancel();
                                    eventReceivers.remove_if([receiver](const auto& response) {
                                        return response.get() == receiver;
                                    });
                                });
                                sendEvent(res, "graph", graphStore->view());
                                enrollment->list([res, sendEvent](const coreci::EnrollmentResult& result) {
                                    sendEvent(res, result.status == 200 ? "repositories" : "unavailable", result.body);
                                });
                            });

                            app.get("/api/repositories", [&] APPLICATION(req, res) {
                                enrollment->list([res](const coreci::EnrollmentResult& result) {
                                    res->status(result.status).type("application/json").send(result.body.dump());
                                });
                            });

                            app.post("/api/repositories/save", [&] APPLICATION(req, res) {
                                try {
                                    const auto body = nlohmann::json::parse(req->body.begin(), req->body.end());
                                    enrollment->save(body, [&, res](const coreci::EnrollmentResult& result) {
                                        if (result.status == 200 && result.body.value("new_enrollment", false)) {
                                            pipeline->request(result.body.at("repository").get<std::string>(),
                                                              "",
                                                              true,
                                                              [res, result](const coreci::PipelineResult& work) {
                                                                  auto response = result.body;
                                                                  response["work"] = {{"status", work.status}, {"result", work.body}};
                                                                  res->status(result.status).type("application/json").send(response.dump());
                                                              });
                                        } else {
                                            res->status(result.status).type("application/json").send(result.body.dump());
                                        }
                                    });
                                } catch (const std::exception&) {
                                    res->status(422).type("application/json").send(R"({"error":"invalid enrollment JSON"})");
                                }
                            });

                            app.post("/api/repositories/refresh", [&] APPLICATION(req, res) {
                                try {
                                    const auto body = nlohmann::json::parse(req->body.begin(), req->body.end());
                                    enrollment->refresh(body, [&, res](const coreci::EnrollmentResult& result) {
                                        if (result.status == 200 && result.body.value("new_enrollment", false)) {
                                            pipeline->request(result.body.at("repository").get<std::string>(),
                                                              "",
                                                              true,
                                                              [res, result](const coreci::PipelineResult& work) {
                                                                  auto response = result.body;
                                                                  response["work"] = {{"status", work.status}, {"result", work.body}};
                                                                  res->status(result.status).type("application/json").send(response.dump());
                                                              });
                                        } else {
                                            res->status(result.status).type("application/json").send(result.body.dump());
                                        }
                                    });
                                } catch (const std::exception&) {
                                    res->status(422).type("application/json").send(R"({"error":"invalid enrollment JSON"})");
                                }
                            });

                            app.post("/api/repositories/workflow", [&] APPLICATION(req, res) {
                                try {
                                    enrollment->workflow(nlohmann::json::parse(req->body.begin(), req->body.end()),
                                                         [res](const coreci::EnrollmentResult& result) {
                                                             res->status(result.status).type("application/json").send(result.body.dump());
                                                         });
                                } catch (const std::exception&) {
                                    res->status(422).type("application/json").send(R"({"error":"invalid workflow request JSON"})");
                                }
                            });

                            app.post("/api/work/request", [&] APPLICATION(req, res) {
                                try {
                                    const auto body = nlohmann::json::parse(req->body.begin(), req->body.end());
                                    pipeline->request(body.at("repository").get<std::string>(),
                                                      body.value("tag", std::string{}),
                                                      false,
                                                      [res](const coreci::PipelineResult& result) {
                                                          res->status(result.status).type("application/json").send(result.body.dump());
                                                      });
                                } catch (const std::exception&) {
                                    res->status(422).type("application/json").send(R"({"error":"invalid source request JSON"})");
                                }
                            });
                            app.post("/api/work/retry", [&] APPLICATION(req, res) {
                                try {
                                    const auto body = nlohmann::json::parse(req->body.begin(), req->body.end());
                                    pipeline->retry(body.at("execution").get<std::string>(), [res](const coreci::PipelineResult& result) {
                                        res->status(result.status).type("application/json").send(result.body.dump());
                                    });
                                } catch (const std::exception&) {
                                    res->status(422).type("application/json").send(R"({"error":"invalid retry JSON"})");
                                }
                            });
                            app.post("/api/work/reconcile", [&] APPLICATION(req, res) {
                                pipeline->refreshStatus();
                                pipeline->reconcile([res](const coreci::PipelineResult& result) {
                                    res->status(result.status).type("application/json").send(result.body.dump());
                                });
                            });

                            app.get("/api/status", [&] APPLICATION(req, res) {
                                store->query("SELECT JSON_OBJECT('database', 'available')", [res](const coreci::DatabaseResult& result) {
                                    res->type("application/json");
                                    if (result.error.empty() && result.rows.size() == 1) {
                                        res->send(result.rows.front().dump());
                                    } else {
                                        res->status(503);
                                        res->send(R"({"database":"unavailable"})");
                                    }
                                });
                            });

                            app.listen([&](const auto& address, const core::socket::State& listenerState) {
                                if (listenerState == core::socket::State::OK) {
                                    snode::log::application().info() << "Listening on " << address.toString();
                                    webhooks.listen([](const auto& webhookAddress, const core::socket::State& webhookState) {
                                        if (webhookState == core::socket::State::OK) {
                                            snode::log::application().info() << "Webhooks listening on " << webhookAddress.toString();
                                        } else if (webhookState == core::socket::State::ERROR ||
                                                   webhookState == core::socket::State::FATAL) {
                                            snode::log::application().error() << "Webhook listener failed: " << webhookState.what();
                                        }
                                    });

                                    pipeline->refreshStatus();
                                    pipeline->reconcile([](const coreci::PipelineResult& result) {
                                        snode::log::application().info() << "Startup graph reconciliation: " << result.body.dump();
                                    });
                                } else if (listenerState == core::socket::State::ERROR || listenerState == core::socket::State::FATAL) {
                                    snode::log::application().error() << "Listener failed: " << listenerState.what();
                                    pipeline.reset();
                                    enrollment.reset();
                                    graphStore.reset();
                                    store.reset();
                                    database.reset();
                                    if (managed) {
                                        managed->stop();
                                    }
                                }
                            });
                        }
                    });
                }
            });

        store = std::make_unique<coreci::Database>(*database);
        graphStore = std::make_unique<coreci::GraphStore>(*store);
        enrollment = std::make_unique<coreci::Enrollment>(*store, github, *graphStore, sources, *pipelineConfig);
        pipeline = std::make_unique<coreci::Pipeline>(*pipelineConfig, *githubConfig, github, sources, *enrollment, *graphStore, feeds);

        if (databaseConfig->mode == "managed") {
            managed = std::make_unique<coreci::ManagedDatabase>();
            managed->start(*databaseConfig, [&]() {
                database->connect();
            });
        } else {
            database->connect();
        }
    } catch (const std::exception& error) {
        snode::log::application().error() << "Controller startup failed: " << error.what();
        pipeline.reset();
        enrollment.reset();
        graphStore.reset();
        store.reset();
        database.reset();
        if (managed) {
            managed->stop();
        }
    }

    return core::SNodeC::start();
}
