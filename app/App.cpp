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
                scheduling.waveDelay = config.number("wave-delay", 0, 86400, valid);
                scheduling.waveDeadline = config.number("wave-deadline", 60, 86400, valid);
                scheduling.waveStartTimeout = config.number("start-timeout", 1, 86400, valid);
                scheduling.publicationDeadline = config.number("publication-deadline", 60, 86400, valid);
                scheduling.retryDelay = config.number("retry-delay", 0, 86400, valid);
                scheduling.maxDispatchAttempts = static_cast<unsigned>(config.number("max-dispatch-attempts", 1, 20, valid));
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
                    const auto repository = config.get("workflow-repository");
                    const auto slash = repository.find('/');
                    scheduling.feedBranch = config.get("feed-branch");
                    scheduling.workflow = {repository.substr(0, slash), repository.substr(slash + 1), "main"};
                    scheduling.dispatchPaused =
                        config.get("dispatch-paused").empty() ? std::nullopt : std::optional<bool>{config.get("dispatch-paused") == "true"};
                    queue = std::make_unique<ps::StateSession>(
                        *store,
                        scheduling,
                        [this](auto seq) {
                            generation = seq;
                            if (dashboard && queue->engine()) {
                                dashboard->changed();
                                dispatch();
                            }
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
                        [this] {
                            return generation;
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
                        generation = queue->engine()->stored().sequence;
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
                const auto* engine = queue->engine();
                const auto at = now();
                if (engine && !tickPending && engine->nextDeadline() && at >= *engine->nextDeadline()) {
                    tickPending = true;
                    queue->enqueue(Tick{at}, [this](const auto&) {
                        tickPending = false;
                        dispatch();
                    });
                }
                dispatch();
            }
            void dispatch() {
                if (const auto* engine = queue->engine(); engine && !tickPending) {
                    for (const auto& command : engine->commands()) {
                        if (std::find(dispatching.begin(), dispatching.end(), command) == dispatching.end()) {
                            dispatching.push_back(command);
                            github->request(
                                base->path + gh::dispatchPath(command),
                                [this, command](int status, std::string body, std::string) {
                                    Event event = command.publication
                                                      ? Event{PublicationDispatchAccepted{now(), command.id, command.attempt}}
                                                      : Event{WaveDispatchAccepted{now(), command.id, command.attempt}};
                                    if (status != 204) {
                                        event = command.publication
                                                    ? Event{PublicationDispatchRejected{now(), command.id, command.attempt, status, body}}
                                                    : Event{WaveDispatchRejected{now(), command.id, command.attempt, status, body}};
                                    }
                                    queue->enqueue(std::move(event), [this, command](const auto&) {
                                        std::erase(dispatching, command);
                                    });
                                },
                                gh::dispatchBody(command, config.get("callback-base-url")));
                        }
                    }
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
                            if (std::holds_alternative<gh::LoadedDeclarations>(result) && queue->engine()) {
                                const auto& loaded = std::get<gh::LoadedDeclarations>(result);
                                bool compatible = true;
                                for (const auto& [package, release] : queue->engine()->releases()) {
                                    if (release && loaded.pipeline.packages().contains(package) &&
                                        queue->engine()->sameSource(package, loaded.pipeline.packages().at(package).sourceRepo)) {
                                        const auto selected = loaded.releases.find(package);
                                        compatible = compatible && selected != loaded.releases.end() &&
                                                     (*Version::parse(selected->second.tag) > release->version ||
                                                      (selected->second.tag == release->tag && selected->second.sha == release->sourceSha));
                                    }
                                }
                                if (!compatible) {
                                    result = gh::Error{"tag-bound load would regress a recorded release"};
                                }
                            }
                            if (std::holds_alternative<gh::Error>(result)) {
                                const auto message = std::get<gh::Error>(result).reason;
                                std::cerr << message << '\n';
                                dashboard->failed(message);
                                finishLoad(false);
                            } else {
                                const auto loaded = std::get<gh::LoadedDeclarations>(result);
                                std::vector<TagObserved> tags;
                                const auto at = now();
                                for (const auto& [package, tag] : loaded.releases) {
                                    tags.push_back({at, package, tag.tag, tag.sha, Origin::Reconcile});
                                }
                                queue->enqueue(GraphChanged{at, loaded.pipeline, tags}, [this, loaded](const auto&) {
                                    if (queue->engine() && queue->engine()->pipeline() == loaded.pipeline) {
                                        dashboard->loaded(loaded);
                                    }
                                    finishLoad(queue->engine() && queue->engine()->pipeline() == loaded.pipeline);
                                });
                            }
                        });
                }
            }
            std::uint64_t generation = 0;
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
            std::vector<Command> dispatching;
            std::unique_ptr<core::timer::Timer> retry, ticks, reconcileTimer;
            Options scheduling;
            std::int64_t interval = 0;
            double recoveryDelay = 1;
            bool listening = false, fatal = false, tickPending = false;
        };
    } // namespace
    int run(int argc, char** argv) {
        Application application;
        return application.start(argc, argv);
    }
} // namespace coreci::app
