#include "Database.h"
#include "DatabaseConfig.h"
#include "Enrollment.h"
#include "EnvironmentResolver.h"
#include "GitHub.h"
#include "ManagedDatabase.h"
#include "Pipeline.h"
#include "PipelineConfig.h"
#include "UiAssets.h"
#include "UiConfig.h"

#include <Log.h>
#include <core/SNodeC.h>
#include <database/mariadb/MariaDBClient.h>
#include <exception>
#include <express/legacy/in/WebApp.h>
#include <memory>
#include <utility>
#include <utils/Config.h>
#include <web/http/ConfigHttpParser.h>
#include <web/http/server/ConfigHttpServer.h>

int main(int argc, char* argv[]) {
    auto* databaseConfig = utils::Config::configRoot.newSubCommand<coreci::DatabaseConfig>();

    auto* githubConfig = utils::Config::configRoot.newSubCommand<coreci::GitHubConfig>();
    coreci::GitHub github(*githubConfig);
    coreci::SourceReader sources(github);

    auto* pipelineConfig = utils::Config::configRoot.newSubCommand<coreci::PipelineConfig>();

    coreci::FeedProof feeds(*pipelineConfig, github, sources);

    auto* registryConfig = utils::Config::configRoot.newSubCommand<coreci::RegistryConfig>();
    coreci::EnvironmentResolver environments(*registryConfig);

    auto* uiConfig = utils::Config::configRoot.newSubCommand<coreci::UiConfig>();

    const express::legacy::in::WebApp app("web");
    app.getConfig()->setReuseAddress();
    app.getConfig()->Local::setHost("127.0.0.1")->setPort(8080);

    app.getConfig()
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

    try {
        github.loadCredentials();
        environments.configure();
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
                            app.post("/github/webhook", [&] APPLICATION(req, res) {
                                pipeline->event(req->get("X-GitHub-Event"),
                                                req->get("X-Hub-Signature-256"),
                                                std::string(req->body.begin(), req->body.end()),
                                                [res](const coreci::PipelineResult& result) {
                                                    res->status(result.status).type("application/json").send(result.body.dump());
                                                });
                            });

                            app.use([&] MIDDLEWARE(req, res, next) {
                                res->set("Cache-Control", "no-store");
                                res->set("X-Content-Type-Options", "nosniff");
                                res->set("Content-Security-Policy",
                                         "default-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'");
                                if (req->method == "POST" &&
                                    (req->get("Origin") != uiConfig->origin || req->get("Content-Type").find("application/json") != 0 ||
                                     uiConfig->origin.substr(uiConfig->origin.find("://") + 3) != req->get("Host"))) {
                                    res->status(403).send(
                                        R"({"error":"configuration writes require the configured same origin and JSON"})");
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
        pipeline = std::make_unique<coreci::Pipeline>(
            *pipelineConfig, *githubConfig, github, sources, environments, *enrollment, *graphStore, feeds);

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
