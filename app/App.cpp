#include "App.h"

#include "Config.h"
#include "Dashboard.h"
#include "Http.h"
#include "Routers.h"
#include "adapters/github/AppToken.h"
#include "adapters/github/Declarations.h"
#include "adapters/github/HttpClient.h"
#include "adapters/mariadb/MariaDBStateStore.h"
#include "persistence/StateSession.h"

#include <algorithm>
#include <charconv>
#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <csignal>
#include <express/legacy/in/WebApp.h>
#include <express/tls/in/WebApp.h>
#include <filesystem>
#include <iostream>
#include <log/SemanticLogger.h>
#include <set>
#include <sstream>
#include <tuple>
#include <utils/Config.h>
#include <web/http/ConfigHttpParser.h>
namespace coreci::app {
    namespace {
        namespace gh = github;
        namespace ps = persistence;
        constexpr web::http::ParserLimits maximumParserLimits{.maximumStartLineBytes = 8U * 1024U,
                                                              .maximumHeaderBytes = 64U * 1024U,
                                                              .maximumHeaderFields = 100,
                                                              .maximumBodyBytes = 25U * 1024U * 1024U};
        using Server = express::tls::in::WebApp;
        class Application {
        public:
            Application()
                : server("coreci")
                , config(static_cast<net::config::ConfigInstance&>(*server.getConfig())) {
            }
            int start(int argc, char** argv) {
                core::SNodeC::init(argc, argv);
                // HTTP diagnostics can render Authorization headers; never enable them for this daemon.
                logger::LogManager::setComponentLevel("web.http.client", logger::LogLevel::Warn);
                logger::LogManager::setComponentLevel("web.http.server", logger::LogLevel::Warn);
                logger::LogManager::setInstanceLevel("coreci", logger::LogLevel::Warn);
                bool valid = true;
                const auto limits = server.getConfig()
                                        ->net::config::ConfigInstance::getSubCommand<web::http::server::ConfigHttpServer>()
                                        ->getParserLimits();
                for (const auto& [name, value, maximum] :
                     {std::tuple{"maximum-body-bytes", limits.maximumBodyBytes, maximumParserLimits.maximumBodyBytes},
                      std::tuple{"maximum-header-bytes", limits.maximumHeaderBytes, maximumParserLimits.maximumHeaderBytes},
                      std::tuple{"maximum-header-fields", limits.maximumHeaderFields, maximumParserLimits.maximumHeaderFields},
                      std::tuple{"maximum-start-line-bytes", limits.maximumStartLineBytes, maximumParserLimits.maximumStartLineBytes}}) {
                    if (value == 0 || value > maximum) {
                        Config::invalid(name, valid);
                    }
                }
                for (const auto& key : {"github-app-id",
                                        "github-installation-id",
                                        "github-app-key-file",
                                        "target-catalog",
                                        "workflow-repository",
                                        "callback-base-url",
                                        "db-user",
                                        "db-name",
                                        "webhook-secret-file",
                                        "callback-secret-file",
                                        "idp-client-id",
                                        "idp-public-key-path",
                                        "dashboard-users"}) {
                    if (config.get(key).empty()) {
                        Config::invalid(key, valid);
                    }
                }
                webhookSecret = config.secret("webhook-secret-file", valid);
                callbackSecret = config.secret("callback-secret-file", valid);
                databasePassword = config.secret("db-password-file", valid);
                if (webhookSecret.empty() || callbackSecret.empty()) {
                    Config::invalid("HMAC secret files", valid);
                }
                const auto registry = gh::registryFile(config.get("registry"));
                if (std::holds_alternative<gh::Error>(registry)) {
                    Config::invalid("registry", valid);
                } else {
                    registryDefinition = std::get<gh::DeclarationFile>(registry);
                }
                if (std::holds_alternative<gh::Error>(gh::registryFile(config.get("registry").substr(0, config.get("registry").find(':')) +
                                                                       ":" + config.get("target-catalog"))) ||
                    std::holds_alternative<gh::Error>(gh::registryFile(config.get("workflow-repository") + ":file"))) {
                    Config::invalid("declarations", valid);
                }
                for (const auto& key : {"github-app-id", "github-installation-id"}) {
                    if (config.get(key).empty() || config.get(key).find_first_not_of("0123456789") != std::string::npos ||
                        config.get(key).find_first_not_of('0') == std::string::npos) {
                        Config::invalid(key, valid);
                    }
                }
                base = gh::httpsUrl(config.get("github-api-url"));
                if (base) {
                    while (!base->path.empty() && base->path.back() == '/') {
                        base->path.pop_back();
                    }
                }
                if (!base || config.get("github-api-url").find('?') != std::string::npos) {
                    Config::invalid("github-api-url", valid);
                }
                if (!gh::httpsUrl(config.get("callback-base-url"))) {
                    Config::invalid("callback-base-url", valid);
                }
                for (const auto& key : {"github-ca-cert", "idp-public-key-path", "github-app-key-file"}) {
                    std::error_code error;
                    if (!config.get(key).empty() && !std::filesystem::is_regular_file(config.get(key), error)) {
                        Config::invalid(key, valid);
                    }
                }
                if (!config.get("dispatch-paused").empty() && config.get("dispatch-paused") != "true" &&
                    config.get("dispatch-paused") != "false") {
                    Config::invalid("dispatch-paused", valid);
                }
                scheduling.slots = static_cast<unsigned>(config.number("github_slots", 1, 1000, valid));
                scheduling.batch = config.number("batch-delay", 0, 86400, valid);
                scheduling.queue = config.number("queue-deadline", 1, 86400, valid);
                scheduling.execution = config.number("execution-deadline", 1, 21600, valid);
                scheduling.grace = config.number("callback-grace", 1, 86400, valid);
                scheduling.poll = config.number("run-poll", 1, 86400, valid);
                scheduling.branch = config.get("feed-branch");
                const auto fault = config.get("qualification_fault");
                if (!fault.empty()) {
                    if (scheduling.branch == "feed-staging" && fault.starts_with("after_push:") && fault.size() > 11) {
                        scheduling.fault = fault.substr(11);
                    } else {
                        Config::invalid("qualification_fault", valid);
                    }
                }
                if (config.get("feed-branch") != "feed" && config.get("feed-branch") != "feed-staging") {
                    Config::invalid("feed-branch", valid);
                }
                interval = config.number("reconcile-interval", 60, 86400, valid);
                const auto dbPort = config.number("db-port", 0, 65535, valid);
                std::shared_ptr<snodec::auth::OAuthClient> auth;
                if (base) {
                    http = std::make_unique<gh::HttpClient>(*base, config.get("github-ca-cert"));
                    try {
                        github = std::make_unique<gh::AppToken>(
                            config.get("github-app-id"),
                            config.get("github-installation-id"),
                            config.get("github-app-key-file"),
                            base->path,
                            [this](auto path, auto credential, auto done, auto body, bool raw) {
                                http->request(std::move(path), std::move(credential), std::move(done), std::move(body), raw);
                            },
                            now);
                    } catch (const std::exception&) {
                        Config::invalid("github-app-key-file", valid);
                    }
                }
                try {
                    auth = std::make_shared<snodec::auth::OAuthClient>(
                        config.get("idp-base-url"),
                        config.get("idp-client-id"),
                        snodec::auth::endpoint(config.get("callback-base-url")).origin + "/auth/callback",
                        snodec::auth::middleware::loadPublicKey(config.get("idp-public-key-path")),
                        snodec::auth::httpExchange,
                        "coreci_access_token",
                        43200);
                } catch (const std::exception&) {
                    Config::invalid("dashboard-auth", valid);
                }
                int result = 1;
                if (valid) {
                    database::mariadb::MariaDBConnectionDetails details;
                    details.connectionName = "coreci";
                    details.hostname = config.get("db-host");
                    details.port = static_cast<unsigned>(dbPort);
                    details.socket = config.get("db-socket").empty() ? MYSQL_UNIX_ADDR : config.get("db-socket");
                    details.username = config.get("db-user");
                    details.database = config.get("db-name");
                    details.password = databasePassword;
                    store = std::make_unique<mariadb::MariaDBStateStore>(details);
                    scheduling.workflowRepository = config.get("workflow-repository");
                    remote = base->path + "/repos/" + config.get("workflow-repository");
                    scheduling.paused =
                        config.get("dispatch-paused").empty() ? std::nullopt : std::optional<bool>{config.get("dispatch-paused") == "true"};
                    queue = std::make_unique<ps::StateSession>(
                        *store,
                        scheduling,
                        [this] {
                            dashboard->changed();
                        },
                        [] {
                            return core::SNodeC::state() != core::State::STOPPING;
                        });
                    store->setConnectionLost([this](const auto& reason) {
                        queue->recover(reason);
                    });
                    std::set<std::string> users;
                    std::istringstream names(config.get("dashboard-users"));
                    std::string name;
                    while (std::getline(names, name, ',')) {
                        users.insert(name);
                    }
                    dashboard = std::make_unique<Dashboard>(
                        auth,
                        users,
                        *queue,
                        [this](auto done) {
                            reload(std::move(done));
                        },
                        [this](const auto& body, auto done) {
                            admit(body, std::move(done));
                        });
                    routes();
                    if (!redirect.getConfig()->net::config::ConfigInstance::getDisabled()) {
                        redirect.all(
                            "*", [origin = snodec::auth::endpoint(config.get("callback-base-url")).origin](const Req& req, const Res& res) {
                                res->redirect(308, origin + req->originalUrl);
                            });
                        redirect.listen([](const auto&, const auto&) {
                        });
                    }
                    connectStore();
                    result = core::SNodeC::start();
                    if (result == -SIGTERM || result == -SIGINT) {
                        result = 0; // regular stop by signal, not a failure
                    }
                    if (fatal) {
                        result = 1;
                    }
                }
                return result;
            }

        private:
            void connectStore() {
                queue->start(
                    [this] {
                        recoveryDelay = 1;
                        nextFeed = 0;
                        dashboard->changed();
                        tick();
                        if (listening) {
                            reload();
                        }
                        if (retry) {
                            retry->cancel();
                        }
                        if (!listening) {
                            listening = true;
                            server.listen([this](const auto&, const auto& state) {
                                if (state == core::socket::State::OK) {
                                    reload();
                                    ticks = std::make_unique<core::timer::Timer>(core::timer::Timer::intervalTimer(
                                        [this] {
                                            tick();
                                        },
                                        1));
                                    reconcileTimer = std::make_unique<core::timer::Timer>(core::timer::Timer::intervalTimer(
                                        [this] {
                                            reload();
                                        },
                                        static_cast<double>(interval)));
                                } else {
                                    fatal = true;
                                    core::SNodeC::stop();
                                }
                            });
                        }
                    },
                    [this](const auto& error) {
                        if (error.kind == ps::StoreError::Kind::Corrupt) {
                            std::cerr << error.message << '\n';
                            fatal = true;
                            core::SNodeC::stop();
                        } else {
                            std::cerr << "store unavailable; recovering\n";
                            retry = std::make_unique<core::timer::Timer>(core::timer::Timer::singleshotTimer(
                                [this] {
                                    connectStore();
                                },
                                recoveryDelay));
                            recoveryDelay = std::min(recoveryDelay * 2, 60.0);
                        }
                    });
            }
            void tick() {
                if (const auto* state = queue->state()) {
                    const auto at = now();
                    const auto& records = state->at("attempts");
                    const bool changingHead = std::ranges::any_of(records, [&](const Json& record) {
                        return record.at("publication") == true && reserved(record) && record.at("dispatch") == "sent";
                    });
                    const bool ready = std::ranges::any_of(state->at("items"), [](const Json& item) {
                        return item.at("state") == "enrolled" && item.value("condition", "") == "";
                    });
                    const bool pick = changingHead && ready && state->at("paused") == false &&
                                      std::ranges::count_if(records, reserved) < scheduling.slots;
                    if (at >= nextFeed || pick) {
                        refreshFeed(pick);
                    }
                    if (!pick) {
                        turn();
                    }
                    state = queue->state();
                    if (state) {
                        const bool paused = state->at("paused").get<bool>();
                        const auto attempts = state->at("attempts");
                        for (const auto& [key, record] : attempts.items()) {
                            const auto phase = record.at("run_state");
                            const auto dispatch = record.at("dispatch");
                            if (!busy.contains(key) && dispatch == "pending" && !paused && record.value("retry_at", Time{0}) <= at) {
                                send(key, record);
                            } else if (!busy.contains(key) && dispatch == "sent" && phase != "completed" &&
                                       !record.value("superseded_completed", false) &&
                                       (at >= record.at("poll_at").get<Time>() ||
                                        (phase != "superseded" && !record.value("cancel_sent", false) &&
                                         at >= record.value("execute_at", record.at("queue_at").get<Time>())))) {
                                observe(key, record);
                            }
                        }
                    }
                }
            }

            void turn() {
                queue->enqueue("", [this](auto& facts) {
                    return queue->scheduler().turn(facts, now());
                });
            }

            std::string workflow(const Json& record) const {
                return remote + "/actions/workflows/" + (record.at("publication") == true ? "publish.yml" : "build.yml");
            }

            void send(const std::string& key, const Json& record) {
                busy.insert(key);
                queue->enqueue(
                    "",
                    [key](auto& state) {
                        state["attempts"][key]["dispatch"] = "sent";
                        return Result{};
                    },
                    [this, key, record](const auto& result) {
                        if (result.code != 200) {
                            busy.erase(key);
                        } else {
                            const bool publication = record.at("publication").get<bool>();
                            Json inputs{{publication ? "publication_id" : "request_id", record.at("id")},
                                        {"attempt", std::to_string(record.at("attempt").get<unsigned>())},
                                        {"feed_branch", scheduling.branch}};
                            if (publication) {
                                const auto plan = Json::parse(record.at("document").get<std::string>());
                                inputs["fault"] = plan.at("fault");
                            }
                            const Json request{{"ref", "main"}, {"return_run_details", true}, {"inputs", inputs}};
                            github->request(
                                workflow(record) + "/dispatches",
                                [this, key](int status, std::string body, const auto&) {
                                    busy.erase(key);
                                    const auto run = Json::parse(body, nullptr, false);
                                    if (status >= 200 && status < 300 && run.is_object() && run.contains("workflow_run_id")) {
                                        applyObservation(key, Json{{"id", run.at("workflow_run_id")}, {"status", "queued"}});
                                    }
                                },
                                request.dump());
                        }
                    });
            }

            void applyObservation(const std::string& key, Json run) {
                if ((run.value("status", "") == "in_progress" || run.value("status", "") == "completed") && run.contains("created_at") &&
                    run.contains("run_started_at")) {
                    try {
                        run["queue_seconds"] = std::max<Time>(0,
                                                              gh::isoTime(run.at("run_started_at").get<std::string>()) -
                                                                  gh::isoTime(run.at("created_at").get<std::string>()));
                    } catch (const std::exception&) {
                        // An absent timing does not prevent observing completion.
                    }
                }
                queue->enqueue(
                    "",
                    [this, key, run](auto& state) {
                        return queue->scheduler().observed(state, key, run, now());
                    },
                    [this, key](const auto&) {
                        if (const auto* state = queue->state(); state && state->at("attempts").contains(key)) {
                            const auto& record = state->at("attempts").at(key);
                            if (record.at("publication") == true && record.at("run_state") == "completed") {
                                nextFeed = 0;
                                refreshFeed();
                            }
                            if (record.value("cancel_requested", false) && !record.value("cancel_sent", false) &&
                                record.at("run_state") != "completed" && !busy.contains(key)) {
                                busy.insert(key);
                                github->request(
                                    remote + "/actions/runs/" + std::to_string(record.at("run").get<std::uint64_t>()) + "/cancel",
                                    [this, key](int status, const auto&, const auto&) {
                                        busy.erase(key);
                                        if (status == 202 || status == 409) {
                                            queue->enqueue("", [key](auto& state) {
                                                state["attempts"][key]["cancel_sent"] = true;
                                                return Result{};
                                            });
                                        }
                                    },
                                    "{}");
                            }
                        }
                    });
            }

            void observe(const std::string& key, const Json& record) {
                busy.insert(key);
                if (record.at("run") != 0) {
                    github->request(remote + "/actions/runs/" + std::to_string(record.at("run").get<std::uint64_t>()),
                                    [this, key](int status, std::string body, const auto&) {
                                        busy.erase(key);
                                        const auto result = Json::parse(body, nullptr, false);
                                        if (status == 200 && result.is_object()) {
                                            applyObservation(key, result);
                                        }
                                    });
                } else {
                    const auto title = std::string(record.at("publication") == true ? "publish " : "build ") + key;
                    locate(key, title, workflow(record) + "/runs?branch=main&event=workflow_dispatch&per_page=100", {});
                }
            }

            void locate(const std::string& key, const std::string& title, const std::string& page, std::set<std::string> visited) {
                visited.insert(page);
                github->request(page, [this, key, title, visited = std::move(visited)](int status, std::string body, std::string link) {
                    const auto result = Json::parse(body, nullptr, false);
                    if (status == 200 && result.is_object() && result.contains("workflow_runs")) {
                        const auto& runs = result.at("workflow_runs");
                        const auto found = std::ranges::find_if(runs, [&](const auto& run) {
                            return run.value("display_title", "") == title && run.value("run_attempt", 0U) >= 1;
                        });
                        if (found != runs.end()) {
                            busy.erase(key);
                            applyObservation(key, *found);
                        } else if (const auto next = gh::nextLink(link, *base); std::holds_alternative<std::optional<std::string>>(next)) {
                            const auto uri = std::get<std::optional<std::string>>(next);
                            if (uri && !visited.contains(gh::httpsUrl(*uri).value().path)) {
                                locate(key, title, gh::httpsUrl(*uri).value().path, visited);
                            } else {
                                busy.erase(key);
                                if (!uri) {
                                    applyObservation(key, Json{{"not_found", true}});
                                }
                            }
                        } else {
                            busy.erase(key);
                        }
                    } else {
                        busy.erase(key);
                    }
                });
            }

            void refreshFeed(bool pick = false) {
                if (!readingFeed && queue->state()) {
                    readingFeed = true;
                    nextFeed = now() + scheduling.poll;
                    github->request(
                        remote + "/git/ref/heads/" + scheduling.branch, [this, pick](int status, std::string body, const auto&) {
                            const auto ref = Json::parse(body, nullptr, false);
                            if (status == 200 && ref.is_object() && ref.contains("object") && ref["object"].contains("sha")) {
                                const auto head = ref["object"]["sha"].get<std::string>();
                                github->request(remote + "/git/matching-refs/snapshots/" + scheduling.branch + "/",
                                                [this, head, pick](int code, std::string body, const auto&) {
                                                    const auto refs = Json::parse(body, nullptr, false);
                                                    if (code == 200 && refs.is_array()) {
                                                        Json snapshots = Json::object();
                                                        for (const auto& ref : refs) {
                                                            snapshots[ref.at("ref").get<std::string>()] = ref.at("object").at("sha");
                                                        }
                                                        github->request(
                                                            remote + "/contents/publications.json?ref=" + head,
                                                            [this, head, snapshots, pick](int code, std::string body, const auto&) {
                                                                readingFeed = false;
                                                                const auto ledger = Json::parse(body, nullptr, false);
                                                                if (code == 200 && (ledger.is_array() || ledger.is_object())) {
                                                                    queue->enqueue(
                                                                        "",
                                                                        [this, head, snapshots, ledger](auto& state) {
                                                                            return queue->scheduler().feed(
                                                                                state, head, snapshots, ledger, now());
                                                                        },
                                                                        [this, pick](const auto& result) {
                                                                            if (pick && result.code == 200) {
                                                                                turn();
                                                                            }
                                                                        });
                                                                }
                                                            },
                                                            {},
                                                            true);
                                                    } else {
                                                        readingFeed = false;
                                                    }
                                                });
                            } else {
                                readingFeed = false;
                            }
                        });
                }
            }
            void routes() {
                server.setStrictRouting().setCaseInsensitiveRouting(false);
                server.use([](const Req& req, const Res& res, express::Next& next) {
                    if (req->path != "/api/v1/github/webhook" && req->body.size() > 64U * 1024U) {
                        answer(res, 413, "body too large");
                    } else {
                        next();
                    }
                });
                server.use(apiRouter(*queue, callbackSecret));
                server.use(webhookRouter(*queue, registryDefinition, webhookSecret, [this](auto done) {
                    reload(std::move(done));
                }));
                dashboard->routes(server);
                server.use([](const Req&, const Res& res) {
                    answer(res, 404, "not found");
                });
            }
            void finishLoad(bool success) {
                loading = false;
                if (reloadPending) {
                    reloadPending = false;
                    reload();
                } else {
                    auto replies = std::move(loadReplies);
                    loadReplies.clear();
                    for (const auto& reply : replies) {
                        reply(success);
                    }
                }
            }
            void reload(std::function<void(bool)> done = {}) {
                if (done) {
                    loadReplies.push_back(std::move(done));
                }
                if (loading) {
                    reloadPending = true;
                } else {
                    loading = true;
                    gh::loadDeclarations(
                        registryDefinition,
                        config.get("target-catalog"),
                        *base,
                        [this](std::string path, bool raw, auto done) {
                            github->request(std::move(path), std::move(done), {}, raw);
                        },
                        [this](auto result) {
                            if (const auto* problem = std::get_if<gh::Error>(&result)) {
                                std::cerr << problem->reason << '\n';
                                dashboard->failed(problem->reason);
                                finishLoad(false);
                            } else {
                                auto loaded = std::get<gh::LoadedDeclarations>(std::move(result));
                                discovered = loaded.releases;
                                queue->enqueue(
                                    "declarations reconciled",
                                    [this, loaded = std::move(loaded)](auto& state) {
                                        return queue->scheduler().declarations(state, loaded.catalog, loaded.declarations, loaded.releases);
                                    },
                                    [this](const auto& outcome) {
                                        dashboard->failed(outcome.code == 200 ? "" : outcome.text);
                                        nextFeed = 0;
                                        refreshFeed();
                                        finishLoad(outcome.code == 200);
                                    });
                            }
                        });
                }
            }

            void admit(const Json& body, ps::StateSession::Reply done) {
                const auto repo = body.at("repo").get<std::string>();
                const auto tag = body.at("tag").get<std::string>();
                reload([this, repo, tag, done = std::move(done)](bool loaded) {
                    const auto release = std::ranges::find_if(discovered, [&](const auto& value) {
                        return value.at("repo") == repo && value.at("tag") == tag;
                    });
                    if (!loaded || release == discovered.end()) {
                        done({loaded ? 404 : 503, "tag unavailable"});
                    } else {
                        const auto commit = release->at("commit").get<std::string>();
                        queue->enqueue(
                            "admit " + repo + ":" + tag,
                            [this, repo, tag, commit](auto& state) {
                                return queue->scheduler().admit(state, repo, tag, commit);
                            },
                            done);
                    }
                });
            }

            gh::DeclarationFile registryDefinition;
            bool loading = false, reloadPending = false;
            Server server;
            express::legacy::in::WebApp redirect{"coreci-redirect"};
            std::unique_ptr<Dashboard> dashboard;
            std::vector<std::function<void(bool)>> loadReplies;
            Config config;
            std::string webhookSecret, callbackSecret, databasePassword;
            std::optional<gh::Url> base;
            std::unique_ptr<gh::AppToken> github;
            std::unique_ptr<gh::HttpClient> http;
            std::unique_ptr<mariadb::MariaDBStateStore> store;
            std::unique_ptr<ps::StateSession> queue;
            std::set<std::string> busy;
            std::string remote;
            Json discovered = Json::array();
            Time nextFeed = 0;
            bool readingFeed = false;
            std::unique_ptr<core::timer::Timer> retry, ticks, reconcileTimer;
            Options scheduling;
            std::int64_t interval = 0;
            double recoveryDelay = 1;
            bool listening = false, fatal = false;
        };
    } // namespace
    int run(int argc, char** argv) {
        Application application;
        return application.start(argc, argv);
    }
} // namespace coreci::app
