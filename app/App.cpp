#include "App.h"

#include "Dashboard.h"
#include "Http.h"
#include "StatusJson.h"
#include "adapters/github/AppToken.h"
#include "adapters/github/Declarations.h"
#include "adapters/github/HttpClient.h"
#include "adapters/mariadb/MariaDBEventStore.h"
#include "core/Badge.h"
#include "core/Status.h"
#include "persistence/EventQueue.h"

#include <charconv>
#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <cstdlib>
#include <deque>
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
        using Req = std::shared_ptr<express::Request>;
        using Res = std::shared_ptr<express::Response>;
        std::string environment(const char* key) {
            const auto* v = std::getenv(key);
            return v == nullptr ? "" : v;
        }
        void answer(const Res& res, int code, const std::string& text) {
            res->status(code).set("Content-Type", "text/plain").send(text);
        }
        class Application {
        public:
            Application()
                : server("coreci") {
                const std::map<std::string, std::string> defaults{{"registry", "SNodeC/Packages:ci/registry.json"},
                                                                  {"target-catalog", "ci/targets.json"},
                                                                  {"workflow-repository", "SNodeC/Packages"},
                                                                  {"github-app-id", ""},
                                                                  {"github-installation-id", ""},
                                                                  {"github-app-key-file", ""},
                                                                  {"callback-base-url", ""},
                                                                  {"github-api-url", "https://api.github.com"},
                                                                  {"github-ca-cert", ""},
                                                                  {"db-host", "localhost"},
                                                                  {"db-port", "0"},
                                                                  {"db-socket", ""},
                                                                  {"db-user", ""},
                                                                  {"db-name", ""},
                                                                  {"wave-delay", "120"},
                                                                  {"wave-deadline", "23400"},
                                                                  {"retry-delay", "600"},
                                                                  {"max-dispatch-attempts", "8"},
                                                                  {"feed-branch", "feed"},
                                                                  {"reconcile-interval", "900"},
                                                                  {"listen-address", "0.0.0.0"},
                                                                  {"listen-port", "8443"},
                                                                  {"tls-cert", ""},
                                                                  {"tls-key", ""},
                                                                  {"idp-base-url", "https://core.snodec.org:8088"},
                                                                  {"idp-client-id", ""},
                                                                  {"idp-public-key-path", ""},
                                                                  {"dashboard-users", ""},
                                                                  {"dashboard-redirect-port", "0"}};
                for (const auto& [name, value] : defaults) {
                    options[name] = utils::Config::configRoot.addOption("--" + name, name, "value", value, CLI::Validator());
                }
                server.getConfig()
                    ->net::config::ConfigInstance::getSubCommand<web::http::server::ConfigHttpServer>()
                    ->getParserConfig()
                    ->setMaximumBodyBytes(maximumParserLimits.maximumBodyBytes)
                    ->setMaximumHeaderBytes(maximumParserLimits.maximumHeaderBytes)
                    ->setMaximumHeaderFields(maximumParserLimits.maximumHeaderFields)
                    ->setMaximumStartLineBytes(maximumParserLimits.maximumStartLineBytes);
                server.getConfig()->Local::setPort(8443);
                redirect.getConfig()->net::config::ConfigInstance::forceUnrequired(true);
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
                        invalid(name, valid);
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
                                        "tls-cert",
                                        "tls-key",
                                        "idp-client-id",
                                        "idp-public-key-path",
                                        "dashboard-users"}) {
                    if (get(key).empty()) {
                        invalid(key, valid);
                    }
                }
                for (const auto& key : {"CORECI_WEBHOOK_SECRET", "CORECI_CALLBACK_SECRET"}) {
                    if (environment(key).empty()) {
                        invalid(key, valid);
                    }
                }
                const auto registry = gh::registryFile(get("registry"));
                if (std::holds_alternative<gh::Error>(registry)) {
                    invalid("registry", valid);
                } else {
                    registryDefinition = std::get<gh::DeclarationFile>(registry);
                }
                if (std::holds_alternative<gh::Error>(
                        gh::registryFile(get("registry").substr(0, get("registry").find(':')) + ":" + get("target-catalog"))) ||
                    std::holds_alternative<gh::Error>(gh::registryFile(get("workflow-repository") + ":file"))) {
                    invalid("declarations", valid);
                }
                for (const auto& key : {"github-app-id", "github-installation-id"}) {
                    if (get(key).empty() || get(key).find_first_not_of("0123456789") != std::string::npos ||
                        get(key).find_first_not_of('0') == std::string::npos) {
                        invalid(key, valid);
                    }
                }
                base = gh::httpsUrl(get("github-api-url"));
                if (base) {
                    while (!base->path.empty() && base->path.back() == '/') {
                        base->path.pop_back();
                    }
                }
                if (!base || get("github-api-url").find('?') != std::string::npos) {
                    invalid("github-api-url", valid);
                }
                if (!gh::httpsUrl(get("callback-base-url"))) {
                    invalid("callback-base-url", valid);
                }
                for (const auto& key : {"tls-cert", "tls-key", "github-ca-cert", "idp-public-key-path", "github-app-key-file"}) {
                    std::error_code error;
                    if (!get(key).empty() && !std::filesystem::is_regular_file(get(key), error)) {
                        invalid(key, valid);
                    }
                }
                waveDelay = number("wave-delay", 0, 86400, valid);
                waveDeadline = number("wave-deadline", 60, 86400, valid);
                retryDelay = number("retry-delay", 0, 86400, valid);
                attempts = number("max-dispatch-attempts", 1, 20, valid);
                if (get("feed-branch") != "feed" && get("feed-branch") != "feed-staging") {
                    invalid("feed-branch", valid);
                }
                interval = number("reconcile-interval", 60, 86400, valid);
                const auto port = number("listen-port", 1, 65535, valid), dbPort = number("db-port", 0, 65535, valid);
                const auto redirectPort = number("dashboard-redirect-port", 0, 65535, valid);
                if (redirectPort == port) {
                    invalid("dashboard-redirect-port", valid);
                }
                std::shared_ptr<snodec::auth::OAuthClient> auth;
                if (base) {
                    http = std::make_unique<gh::HttpClient>(*base, get("github-ca-cert"));
                    try {
                        github = std::make_unique<gh::AppToken>(
                            get("github-app-id"),
                            get("github-installation-id"),
                            get("github-app-key-file"),
                            base->path,
                            [this](auto path, auto credential, auto done, auto body, bool raw) {
                                http->request(std::move(path), std::move(credential), std::move(done), std::move(body), raw);
                            },
                            now);
                    } catch (const std::exception&) {
                        invalid("github-app-key-file", valid);
                    }
                }
                try {
                    auth = std::make_shared<snodec::auth::OAuthClient>(get("idp-base-url"),
                                                                       get("idp-client-id"),
                                                                       snodec::auth::endpoint(get("callback-base-url")).origin +
                                                                           "/auth/callback",
                                                                       snodec::auth::middleware::loadPublicKey(get("idp-public-key-path")),
                                                                       snodec::auth::httpExchange,
                                                                       "coreci_access_token",
                                                                       43200);
                } catch (const std::exception&) {
                    invalid("dashboard-auth", valid);
                }
                int result = 1;
                if (valid) {
                    server.getConfig()->setCert(get("tls-cert"));
                    server.getConfig()->setCertKey(get("tls-key"));
                    server.getConfig()->Local::setHost(get("listen-address"))->setPort(static_cast<std::uint16_t>(port));
                    server.getConfig()->setReuseAddress();
                    database::mariadb::MariaDBConnectionDetails details;
                    details.connectionName = "coreci";
                    details.hostname = get("db-host");
                    details.port = static_cast<unsigned>(dbPort);
                    details.socket = get("db-socket").empty() ? MYSQL_UNIX_ADDR : get("db-socket");
                    details.username = get("db-user");
                    details.database = get("db-name");
                    details.password = environment("CORECI_DB_PASSWORD");
                    store = std::make_unique<mariadb::MariaDBEventStore>(details);
                    queue = std::make_unique<ps::EventQueue>(
                        *store,
                        [this] {
                            const auto repository = get("workflow-repository");
                            const auto slash = repository.find('/');
                            return Engine({waveDelay,
                                           waveDeadline,
                                           static_cast<unsigned>(attempts),
                                           retryDelay,
                                           get("feed-branch"),
                                           {repository.substr(0, slash), repository.substr(slash + 1), "main"}});
                        },
                        [] {
                            return Resumed{now()};
                        },
                        [this](const auto& commands) {
                            for (const auto& command : commands) {
                                core::timer::Timer::singleshotTimer(
                                    [this, command] {
                                        dispatch(command);
                                    },
                                    0);
                            }
                        },
                        [](auto seq, const auto&) {
                            std::cerr << "replay ignored seq " << seq << '\n';
                        },
                        [this](auto seq, const Event&, const Engine&) {
                            generation = seq;
                            if (queue->engine()) {
                                dashboard->changed();
                            }
                        });
                    store->setConnectionLost([this](const auto& reason) {
                        queue->recover(reason);
                    });
                    std::set<std::string> users;
                    std::istringstream names(get("dashboard-users"));
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
                    if (redirectPort != 0) {
                        redirect.getConfig()->Local::setHost(get("listen-address"))->setPort(static_cast<std::uint16_t>(redirectPort));
                        redirect.all("*",
                                     [origin = snodec::auth::endpoint(get("callback-base-url")).origin](const Req& req, const Res& res) {
                                         res->redirect(308, origin + req->originalUrl);
                                     });
                        redirect.listen([](const auto&, const auto&) {
                        });
                    }
                    rebuild();
                    result = core::SNodeC::start();
                    if (fatal) {
                        result = 1;
                    }
                }
                return result;
            }

        private:
            void invalid(const std::string& option, bool& valid) {
                valid = false;
                std::cerr << "invalid option: " << option << '\n';
            }
            std::string get(const std::string& name) const {
                return options.at(name)->as<std::string>();
            }
            std::int64_t number(const std::string& name, std::int64_t low, std::int64_t high, bool& valid) {
                const auto value = get(name);
                std::int64_t n = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), n);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || n < low || n > high) {
                    invalid(name, valid);
                }
                return n;
            }
            void rebuild() {
                queue->start(
                    [this] {
                        recoveryDelay = 1;
                        dashboard->changed();
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
                        if (error.kind == ps::EventStore::StoreError::Kind::Corrupt) {
                            std::cerr << error.message << '\n';
                            fatal = true;
                            core::SNodeC::stop();
                        } else {
                            std::cerr << "store unavailable; recovering\n";
                            retry = std::make_unique<core::timer::Timer>(core::timer::Timer::singleshotTimer(
                                [this] {
                                    rebuild();
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
                    });
                }
            }
            void dispatch(const Command& command) {
                const auto classification = queue->engine()
                                                ? queue->engine()->classify(WaveDispatchAccepted{0, command.waveId, command.attempt})
                                                : Classification{Ignore{"recovering"}};
                if (std::holds_alternative<Accept>(classification)) {
                    github->request(
                        base->path + gh::dispatchPath(command),
                        [this, command](int status, std::string body, std::string) {
                            if (queue->engine()) {
                                Event event = WaveDispatchAccepted{now(), command.waveId, command.attempt};
                                if (status != 204) {
                                    event = WaveDispatchRejected{now(), command.waveId, command.attempt, status, body};
                                }
                                queue->enqueue(std::move(event), [](const auto&) {
                                });
                            } else {
                                std::cerr << "dispatch result dropped during recovery\n";
                            }
                        },
                        gh::dispatchBody(command, get("callback-base-url")));
                } else {
                    std::cerr << "dispatch skipped: stale command or recovery\n";
                }
            }
            void submit(Event event, const Res& res) {
                std::visit(
                    [](auto& e) {
                        e.at = now();
                    },
                    event);
                queue->enqueue(std::move(event), [res](const auto& result) {
                    const auto [code, text] = response(result);
                    answer(res, code, text);
                });
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
                const auto webhookAuth = [](const Req& req, const Res& res, express::Next& next) {
                    if (gh::verify(environment("CORECI_WEBHOOK_SECRET"),
                                   std::string(req->body.begin(), req->body.end()),
                                   req->get("X-Hub-Signature-256"))) {
                        next();
                    } else {
                        answer(res, 401, "unauthorized");
                    }
                };
                const auto callbackAuth = [](const Req& req, const Res& res, express::Next& next) {
                    if (gh::verifyCallback(environment("CORECI_CALLBACK_SECRET"),
                                           std::string(req->body.begin(), req->body.end()),
                                           req->get("X-CoreCI-Timestamp"),
                                           req->get("X-CoreCI-Signature-256"),
                                           now())) {
                        next();
                    } else {
                        answer(res, 401, "unauthorized");
                    }
                };
                const auto method = [](const Req&, const Res& res) {
                    answer(res, 405, "method not allowed");
                };
                server.post("/api/v1/github/webhook", webhookAuth, [this](const Req& req, const Res& res) {
                    webhook(req, res, std::string(req->body.begin(), req->body.end()));
                });
                server.all("/api/v1/github/webhook", method);
                server.post("/api/v1/waves/:waveId/report", callbackAuth, [this](const Req& req, const Res& res) {
                    report(req, res);
                });
                server.all("/api/v1/waves/:waveId/report", method);
                server.head("/api/v1/waves/:waveId/plan.json", method);
                server.get("/api/v1/waves/:waveId/plan.json", [this](const Req& req, const Res& res) {
                    const auto* engine = queue->engine();
                    if (!engine) {
                        answer(res, 503, "unavailable");
                    } else if (const auto wave = engine->waves().find(req->param("waveId")); wave != engine->waves().end()) {
                        res->status(200)
                            .set("Content-Type", "application/json")
                            .set("Cache-Control", "no-store")
                            .send(planJson(wave->second));
                    } else {
                        answer(res, 404, "not found");
                    }
                });
                server.all("/api/v1/waves/:waveId/plan.json", method);
                server.head("/api/v1/status.json", method);
                server.get("/api/v1/status.json", [this](const Req&, const Res& res) {
                    if (queue->engine()) {
                        res->status(200)
                            .set("Content-Type", "application/json")
                            .send(statusJson(statusSnapshot(*queue->engine()), generation));
                    } else {
                        answer(res, 503, "unavailable");
                    }
                });
                server.all("/api/v1/status.json", method);
                server.head("/api/v1/badge/:package/:target.svg", method);
                server.get("/api/v1/badge/:package/:target.svg", [this](const Req& req, const Res& res) {
                    const Unit unit{req->param("package"), req->param("target")};
                    res->status(200)
                        .set("Content-Type", "image/svg+xml; charset=utf-8")
                        .set("Cache-Control", "no-cache, no-store, must-revalidate")
                        .send(queue->engine() ? unitBadge(*queue->engine(), unit) : badge(unit.package, "unavailable", "#6e7781"));
                });
                server.all("/api/v1/badge/:package/:target.svg", method);
                dashboard->routes(server);
                server.use([](const Req&, const Res& res) {
                    answer(res, 404, "not found");
                });
            }
            void report(const Req& req, const Res& res) {
                if (!contentType(req, "application/x-www-form-urlencoded")) {
                    answer(res, 400, "invalid: content type");
                } else {
                    const auto decoded = gh::form(std::string(req->body.begin(), req->body.end()));
                    if (std::holds_alternative<gh::Error>(decoded)) {
                        answer(res, 400, "invalid: " + std::get<gh::Error>(decoded).reason);
                    } else {
                        const auto event = gh::report(req->param("waveId"), std::get<gh::Form>(decoded), now());
                        if (std::holds_alternative<gh::Error>(event)) {
                            answer(res, 400, "invalid: " + std::get<gh::Error>(event).reason);

                        } else if (std::holds_alternative<std::monostate>(event)) {
                            answer(res, 200, "acknowledged");
                        } else {
                            submit(std::get<Event>(event), res);
                        }
                    }
                }
            }
            void rememberDelivery(const std::string& delivery) {
                deliveries.push_back(delivery);
                if (deliveries.size() > 1000) {
                    deliveries.pop_front();
                }
            }
            void webhook(const Req& req, const Res& res, const std::string& body) {
                const auto event = req->get("X-GitHub-Event"), delivery = req->get("X-GitHub-Delivery");
                if (event.empty() || delivery.empty() || !contentType(req, "application/json")) {
                    answer(res, 400, "invalid: webhook headers");
                } else {
                    const auto parsed = gh::webhook(body, event);
                    if (std::holds_alternative<gh::Error>(parsed)) {
                        answer(res, 400, "invalid: " + std::get<gh::Error>(parsed).reason);
                    } else if (event == "ping") {
                        answer(res, 200, "pong");
                    } else if (std::find(deliveries.begin(), deliveries.end(), delivery) != deliveries.end()) {
                        answer(res, 200, "ignored: delivery");
                    } else if (std::holds_alternative<std::monostate>(parsed)) {
                        answer(res, 202, "ignored");
                    } else {
                        const auto push = std::get<gh::Push>(parsed);
                        const auto slash = push.repo.find('/');
                        if (!push.branch.empty()) {
                            const bool known = repoKey(push.repo) == repoKey(registryDefinition.owner + "/" + registryDefinition.repo);
                            if (known && push.branch == push.defaultBranch) {
                                reload();
                                rememberDelivery(delivery);
                                answer(res, 202, "reload scheduled");
                            } else {
                                answer(res, 202, "ignored");
                            }
                        } else if (!queue->engine()) {
                            answer(res, 503, "unavailable");
                        } else {
                            const auto packages = slash == std::string::npos
                                                      ? std::vector<std::string>{}
                                                      : pipeline().packagesForRepo(push.repo.substr(0, slash), push.repo.substr(slash + 1));
                            if (packages.empty()) {
                                answer(res, 202, "ignored");
                            } else {
                                reload([this, res, delivery](bool success) {
                                    if (success) {
                                        rememberDelivery(delivery);
                                    }
                                    answer(res, success ? 200 : 503, success ? "accepted" : "unavailable");
                                });
                            }
                        }
                    }
                }
            }
            const Pipeline& pipeline() const {
                return queue->engine()->pipeline();
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
                        get("target-catalog"),
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
                                    if (queue->engine() && pipeline() == loaded.pipeline) {
                                        dashboard->loaded(loaded);
                                    }
                                    finishLoad(queue->engine() && pipeline() == loaded.pipeline);
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
            std::map<std::string, CLI::Option*> options;
            std::optional<gh::Url> base;
            std::unique_ptr<gh::AppToken> github;
            std::unique_ptr<gh::HttpClient> http;
            std::unique_ptr<mariadb::MariaDBEventStore> store;
            std::unique_ptr<ps::EventQueue> queue;
            std::unique_ptr<core::timer::Timer> retry, ticks, reconcileTimer;
            std::deque<std::string> deliveries;
            std::int64_t waveDelay = 0, waveDeadline = 0, attempts = 0, interval = 0, retryDelay = 0;
            double recoveryDelay = 1;
            bool listening = false, fatal = false, tickPending = false;
        };
    } // namespace
    int run(int argc, char** argv) {
        Application application;
        return application.start(argc, argv);
    }
} // namespace coreci::app
