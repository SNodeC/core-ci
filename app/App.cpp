#include "App.h"

#include "adapters/github/Declarations.h"
#include "adapters/github/HttpClient.h"
#include "adapters/mariadb/MariaDBEventStore.h"
#include "core/Badge.h"
#include "core/Status.h"
#include "persistence/EventQueue.h"

#include <charconv>
#include <chrono>
#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <cstdlib>
#include <deque>
#include <express/tls/in/WebApp.h>
#include <filesystem>
#include <iostream>
#include <log/SemanticLogger.h>
#include <openssl/crypto.h>
#include <set>
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
        std::int64_t now() {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
        std::string environment(const char* key) {
            const auto* v = std::getenv(key);
            return v == nullptr ? "" : v;
        }
        void answer(const Res& res, int code, const std::string& text) {
            res->status(code).set("Content-Type", "text/plain").send(text);
        }
        bool contentType(const Req& req, const std::string& expected) {
            auto value = req->get("Content-Type");
            value = value.substr(0, value.find(';'));
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
                value.pop_back();
            }
            for (char& c : value) {
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c - 'A' + 'a');
                }
            }
            return value == expected;
        }
        std::pair<int, std::string> response(const ps::Result& result) {
            std::pair<int, std::string> output;
            std::visit(
                [&](const auto& r) {
                    using T = std::decay_t<decltype(r)>;
                    if constexpr (std::is_same_v<T, ps::Accepted>) {
                        output = {200, "accepted"};
                    } else if constexpr (std::is_same_v<T, ps::Ignored>) {
                        output = {200, "ignored: " + r.reason};
                    } else if constexpr (std::is_same_v<T, ps::Invalid>) {
                        output = {400, "invalid: " + r.reason};
                    } else {
                        output = {503, "unavailable"};
                    }
                },
                result);
            return output;
        }
        class Application {
        public:
            Application()
                : server("coreci") {
                const std::map<std::string, std::string> defaults{{"registry", ""},
                                                                  {"callback-base-url", ""},
                                                                  {"github-api-url", "https://api.github.com"},
                                                                  {"github-ca-cert", ""},
                                                                  {"db-host", "localhost"},
                                                                  {"db-port", "0"},
                                                                  {"db-socket", ""},
                                                                  {"db-user", ""},
                                                                  {"db-name", ""},
                                                                  {"build-timeout", "21600"},
                                                                  {"publish-timeout", "3600"},
                                                                  {"max-dispatch-attempts", "8"},
                                                                  {"max-concurrent-builds", "0"},
                                                                  {"reconcile-interval", "900"},
                                                                  {"listen-address", "0.0.0.0"},
                                                                  {"listen-port", "8443"},
                                                                  {"tls-cert", ""},
                                                                  {"tls-key", ""}};
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
                routes();
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
                for (const auto& key : {"callback-base-url", "db-user", "db-name", "tls-cert", "tls-key"}) {
                    if (get(key).empty()) {
                        invalid(key, valid);
                    }
                }
                for (const auto& key : {"CORECI_GITHUB_TOKEN", "CORECI_WEBHOOK_SECRET", "CORECI_CALLBACK_SECRET", "CORECI_ADMIN_TOKEN"}) {
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
                for (const auto& key : {"tls-cert", "tls-key", "github-ca-cert"}) {
                    std::error_code error;
                    if (!get(key).empty() && !std::filesystem::is_regular_file(get(key), error)) {
                        invalid(key, valid);
                    }
                }
                buildTimeout = number("build-timeout", 60, 86400, valid);
                publishTimeout = number("publish-timeout", 60, 86400, valid);
                attempts = number("max-dispatch-attempts", 1, 20, valid);
                concurrent = number("max-concurrent-builds", 0, 1000, valid);
                interval = number("reconcile-interval", 60, 86400, valid);
                const auto port = number("listen-port", 1, 65535, valid), dbPort = number("db-port", 0, 65535, valid);
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
                    http = std::make_unique<gh::HttpClient>(*base, get("github-ca-cert"));
                    queue = std::make_unique<ps::EventQueue>(
                        *store,
                        [this] {
                            return Engine({buildTimeout,
                                           publishTimeout,
                                           static_cast<unsigned>(attempts),
                                           concurrent == 0 ? std::nullopt : std::optional<std::size_t>(concurrent)});
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
                        });
                    store->setConnectionLost([this](const auto& reason) {
                        queue->recover(reason);
                    });
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
                                        30));
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
                const auto classification = std::visit(
                    [this](const auto& c) -> Classification {
                        constexpr auto kind = std::is_same_v<std::decay_t<decltype(c)>, DispatchBuild> ? Kind::Build : Kind::Publish;
                        return queue->engine() ? queue->engine()->classify(DispatchAccepted{0, c.buildId, kind, c.attempt})
                                               : Classification{Ignore{"recovering"}};
                    },
                    command);
                if (std::holds_alternative<Accept>(classification)) {
                    http->request(
                        base->path + gh::dispatchPath(command),
                        token.token(),
                        [this, command](int status, std::string body, std::string) {
                            if (queue->engine()) {
                                std::visit(
                                    [&](const auto& c) {
                                        constexpr auto kind =
                                            std::is_same_v<std::decay_t<decltype(c)>, DispatchBuild> ? Kind::Build : Kind::Publish;
                                        Event event = DispatchAccepted{now(), c.buildId, kind, c.attempt};
                                        if (status != 204) {
                                            event = DispatchRejected{now(), c.buildId, kind, c.attempt, status, body};
                                        }
                                        queue->enqueue(std::move(event), [](const auto&) {
                                        });
                                    },
                                    command);
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
                const auto size = [](const Req& req, const Res& res, express::Next& next) {
                    if (req->body.size() > 64U * 1024U) {
                        answer(res, 413, "body too large");
                    } else {
                        next();
                    }
                };
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
                const auto adminAuth = [](const Req& req, const Res& res, express::Next& next) {
                    const auto expected = "Bearer " + environment("CORECI_ADMIN_TOKEN"), supplied = req->get("Authorization");
                    if (supplied.size() == expected.size() && CRYPTO_memcmp(supplied.data(), expected.data(), expected.size()) == 0) {
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
                server.post("/api/v1/builds/:buildId/report", size, callbackAuth, [this](const Req& req, const Res& res) {
                    report(req, res);
                });
                server.all("/api/v1/builds/:buildId/report", size, method);
                server.post("/api/v1/units/:package/:target/retry", size, adminAuth, [this](const Req& req, const Res& res) {
                    submit(RetryRequested{now(), req->param("package"), req->param("target")}, res);
                });
                server.all("/api/v1/units/:package/:target/retry", size, method);
                server.post("/api/v1/reload", size, adminAuth, [this](const Req&, const Res& res) {
                    const bool running = loading;
                    reload();
                    answer(res, 202, running ? "already running" : "started");
                });
                server.all("/api/v1/reload", size, method);
                server.post("/api/v1/reconcile", size, adminAuth, [this](const Req&, const Res& res) {
                    const bool running = reconciling;
                    reconcile();
                    answer(res, 202, running ? "already running" : "started");
                });
                server.all("/api/v1/reconcile", size, method);
                server.head("/api/v1/status", size, method);
                server.get("/api/v1/status", size, [this](const Req&, const Res& res) {
                    showStatus(res, false);
                });
                server.all("/api/v1/status", size, method);
                server.head("/api/v1/graph.dot", size, method);
                server.get("/api/v1/graph.dot", size, [this](const Req&, const Res& res) {
                    showStatus(res, true);
                });
                server.all("/api/v1/graph.dot", size, method);
                server.head("/api/v1/badge/:package/:target.svg", size, method);
                server.get("/api/v1/badge/:package/:target.svg", size, [this](const Req& req, const Res& res) {
                    const Unit unit{req->param("package"), req->param("target")};
                    res->status(200)
                        .set("Content-Type", "image/svg+xml; charset=utf-8")
                        .set("Cache-Control", "no-cache, no-store, must-revalidate")
                        .send(queue->engine() ? unitBadge(*queue->engine(), unit) : badge(unit.package, "unavailable", "#6e7781"));
                });
                server.all("/api/v1/badge/:package/:target.svg", size, method);
                server.use(size, [](const Req&, const Res& res) {
                    answer(res, 404, "not found");
                });
            }
            void showStatus(const Res& res, bool dot) {
                if (queue->engine()) {
                    res->status(200)
                        .set("Content-Type", dot ? "text/vnd.graphviz" : "text/plain")
                        .send(dot ? statusDot(*queue->engine())
                                  : (loadError.empty() ? "" : loadError + "\n") + statusText(*queue->engine()));
                } else {
                    answer(res, 503, "unavailable");
                }
            }
            void report(const Req& req, const Res& res) {
                if (!contentType(req, "application/x-www-form-urlencoded")) {
                    answer(res, 400, "invalid: content type");
                } else {
                    const auto decoded = gh::form(std::string(req->body.begin(), req->body.end()));
                    if (std::holds_alternative<gh::Error>(decoded)) {
                        answer(res, 400, "invalid: " + std::get<gh::Error>(decoded).reason);
                    } else {
                        const auto event = gh::report(req->param("buildId"), std::get<gh::Form>(decoded), now());
                        if (std::holds_alternative<gh::Error>(event)) {
                            answer(res, 400, "invalid: " + std::get<gh::Error>(event).reason);
                        } else if (std::holds_alternative<std::monostate>(event)) {
                            answer(res, 200, "ignored: no event");
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
                            const bool known =
                                repoKey(push.repo) == repoKey(registryDefinition.owner + "/" + registryDefinition.repo) ||
                                (queue->engine() &&
                                 !pipeline().packagesForRepo(push.repo.substr(0, slash), push.repo.substr(slash + 1)).empty());
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
                                struct Batch {
                                    std::vector<std::string> lines;
                                    std::size_t left;
                                    int code = 200;
                                };
                                const auto batch =
                                    std::make_shared<Batch>(Batch{std::vector<std::string>(packages.size()), packages.size(), 200});
                                for (std::size_t i = 0; i < packages.size(); ++i) {
                                    queue->enqueue(TagObserved{now(), packages[i], push.tag, push.sha, Origin::Webhook},
                                                   [this, res, batch, delivery, p = packages[i], i](const auto& result) {
                                                       const auto [code, text] = response(result);
                                                       batch->code = std::max(batch->code, code);
                                                       batch->lines[i] = p + ": " + text;
                                                       if (--batch->left == 0) {
                                                           if (batch->code == 200) {
                                                               rememberDelivery(delivery);
                                                           }
                                                           std::string text;
                                                           for (const auto& line : batch->lines) {
                                                               text += line + '\n';
                                                           }
                                                           answer(res, batch->code, text);
                                                       }
                                                   });
                                }
                            }
                        }
                    }
                }
            }
            struct Round {
                std::size_t remaining;
            };
            bool reconcile(bool afterLoad = false) {
                const bool started = (!reconciling || afterLoad) && queue->engine();
                if (started) {
                    ++reconciling;
                    const auto repos = pipeline().repositories();
                    const auto round = std::make_shared<Round>(Round{repos.size()});
                    if (repos.empty()) {
                        --reconciling;
                    }
                    for (const auto& [owner, repo] : repos) {
                        page(owner,
                             repo,
                             base->path + "/repos/" + gh::escape(owner) + "/" + gh::escape(repo) + "/tags?per_page=100",
                             {},
                             {},
                             round);
                    }
                }
                return started;
            }
            void page(const std::string& owner,
                      const std::string& repo,
                      const std::string& path,
                      std::vector<gh::Tag> collected,
                      std::set<std::string> visited,
                      const std::shared_ptr<Round>& round) {
                visited.insert(path);
                http->request(path,
                              token.token(),
                              [this, owner, repo, collected = std::move(collected), visited = std::move(visited), round](
                                  int status, std::string body, std::string linkHeader) mutable {
                                  bool valid = status == 200;
                                  std::string next;
                                  const auto parsed = gh::tags(body);
                                  valid = valid && std::holds_alternative<std::vector<gh::Tag>>(parsed);
                                  if (valid) {
                                      const auto& found = std::get<std::vector<gh::Tag>>(parsed);
                                      for (const auto& tag : found) {
                                          if (tag.sha.size() != 40 || tag.sha.find_first_not_of("0123456789abcdef") != std::string::npos) {
                                              valid = false;
                                          }
                                      }
                                      collected.insert(collected.end(), found.begin(), found.end());
                                      if (!linkHeader.empty()) {
                                          const auto links = gh::nextLink(linkHeader, *base);
                                          valid = valid && !std::holds_alternative<gh::Error>(links);
                                          if (valid) {
                                              const auto& link = std::get<std::optional<std::string>>(links);
                                              if (link) {
                                                  const auto url = gh::httpsUrl(*link);
                                                  valid = url && url->host == base->host && url->port == base->port;
                                                  if (valid) {
                                                      next = url->path;
                                                      valid = !visited.contains(next);
                                                  }
                                              }
                                          }
                                      }
                                  }
                                  if (valid && !next.empty()) {
                                      page(owner, repo, next, std::move(collected), std::move(visited), round);
                                  } else {
                                      if (valid && queue->engine()) {
                                          for (const auto& package : pipeline().packagesForRepo(owner, repo)) {
                                              std::optional<gh::Tag> best;
                                              for (const auto& tag : collected) {
                                                  const auto version = Version::parse(tag.tag);
                                                  const auto normalized =
                                                      ps::normalize(TagObserved{now(), package, tag.tag, tag.sha, Origin::Reconcile});
                                                  if (version && std::holds_alternative<Event>(normalized) &&
                                                      (!version->prerelease() || pipeline().packages().at(package).acceptPrereleases) &&
                                                      (!best || *version > *Version::parse(best->tag) ||
                                                       (*version == *Version::parse(best->tag) && tag.tag < best->tag))) {
                                                      best = tag;
                                                  }
                                              }
                                              if (best) {
                                                  queue->enqueue(TagObserved{now(), package, best->tag, best->sha, Origin::Reconcile},
                                                                 [](const auto&) {
                                                                 });
                                              }
                                          }
                                      } else {
                                          std::cerr << "tag reconciliation aborted for " << owner << '/' << repo << '\n';
                                      }
                                      if (--round->remaining == 0) {
                                          --reconciling;
                                      }
                                  }
                              });
            }
            const Pipeline& pipeline() const {
                return queue->engine()->pipeline();
            }
            void finishLoad() {
                loading = false;
                reconcile(true);
                if (reloadPending) {
                    reloadPending = false;
                    reload();
                }
            }
            void reload() {
                if (loading) {
                    reloadPending = true;
                } else {
                    loading = true;
                    gh::loadDeclarations(
                        registryDefinition,
                        base->path,
                        [this](std::string path, bool raw, auto done) {
                            http->request(std::move(path), token.token(), std::move(done), {}, raw);
                        },
                        [this](auto result) {
                            if (std::holds_alternative<gh::Error>(result)) {
                                loadError = std::get<gh::Error>(result).reason;
                                std::cerr << loadError << '\n';
                                finishLoad();
                            } else {
                                loadError.clear();
                                queue->enqueue(GraphChanged{now(), std::get<gh::LoadedDeclarations>(result).pipeline}, [this](const auto&) {
                                    finishLoad();
                                });
                            }
                        });
                }
            }
            gh::DeclarationFile registryDefinition;
            std::string loadError;
            bool loading = false, reloadPending = false;
            Server server;
            std::map<std::string, CLI::Option*> options;
            std::optional<gh::Url> base;
            gh::EnvironmentTokenProvider token;
            std::unique_ptr<gh::HttpClient> http;
            std::unique_ptr<mariadb::MariaDBEventStore> store;
            std::unique_ptr<ps::EventQueue> queue;
            std::unique_ptr<core::timer::Timer> retry, ticks, reconcileTimer;
            std::deque<std::string> deliveries;
            std::int64_t buildTimeout = 0, publishTimeout = 0, attempts = 0, concurrent = 0, interval = 0;
            double recoveryDelay = 1;
            std::size_t reconciling = 0;
            bool listening = false, fatal = false, tickPending = false;
        };
    } // namespace
    int run(int argc, char** argv) {
        Application application;
        return application.start(argc, argv);
    }
} // namespace coreci::app
