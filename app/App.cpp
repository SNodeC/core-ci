#include "App.h"

#include "adapters/github/HttpClient.h"
#include "adapters/mariadb/MariaDBEventStore.h"
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
#include <regex>
#include <set>
#include <utils/Config.h>
namespace coreci::app {
    namespace {
        namespace gh = github;
        namespace ps = persistence;
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
            explicit Application(Pipeline p)
                : pipeline(std::move(p))
                , server("coreci") {
                const std::map<std::string, std::string> defaults{{"callback-base-url", ""},
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
                server.getConfig()->Local::setPort(8443);
                server.use([this](const Req& req, const Res& res) {
                    route(req, res);
                });
            }
            int start(int argc, char** argv) {
                core::SNodeC::init(argc, argv);
                // HTTP diagnostics can render Authorization headers; never enable them for this daemon.
                logger::LogManager::setComponentLevel("web.http.client", logger::LogLevel::Warn);
                logger::LogManager::setComponentLevel("web.http.server", logger::LogLevel::Warn);
                logger::LogManager::setInstanceLevel("coreci", logger::LogLevel::Warn);
                bool valid = true;
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
                            return Engine(pipeline,
                                          {buildTimeout,
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
                                    reconcile();
                                    ticks = std::make_unique<core::timer::Timer>(core::timer::Timer::intervalTimer(
                                        [this] {
                                            tick();
                                        },
                                        30));
                                    reconcileTimer = std::make_unique<core::timer::Timer>(core::timer::Timer::intervalTimer(
                                        [this] {
                                            reconcile();
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
                if (queue->engine()) {
                    auto request = gh::dispatch(command, get("callback-base-url"), token.token());
                    request.path = base->path + request.path;
                    http->request(std::move(request), [this, command](gh::Response response) {
                        if (queue->engine()) {
                            std::visit(
                                [&](const auto& c) {
                                    constexpr auto kind =
                                        std::is_same_v<std::decay_t<decltype(c)>, DispatchBuild> ? Kind::Build : Kind::Publish;
                                    Event event = DispatchAccepted{now(), c.buildId, kind, c.attempt};
                                    if (response.status != 204) {
                                        event = DispatchRejected{now(), c.buildId, kind, c.attempt, response.status, response.body};
                                    }
                                    queue->enqueue(std::move(event), [](const auto&) {
                                    });
                                },
                                command);
                        } else {
                            std::cerr << "dispatch result dropped during recovery\n";
                        }
                    });
                } else {
                    std::cerr << "dispatch deferred to Resumed during recovery\n";
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
            void route(const Req& req, const Res& res) {
                const auto& path = req->path;
                std::smatch match;
                const bool report = std::regex_match(path, match, std::regex("^/api/v1/builds/([^/]+)/report$"));
                const std::string buildId = report ? match[1].str() : "";
                const bool retryUnit = std::regex_match(path, match, std::regex("^/api/v1/units/([^/]+)/([^/]+)/retry$"));
                const bool hook = path == "/api/v1/github/webhook", reconcileRoute = path == "/api/v1/reconcile";
                const bool status = path == "/api/v1/status", dot = path == "/api/v1/graph.dot";
                const bool known = hook || report || retryUnit || reconcileRoute || status || dot;
                const std::string body(req->body.begin(), req->body.end());
                const auto at = now();
                bool auth = true;
                if (hook) {
                    auth = gh::verify(environment("CORECI_WEBHOOK_SECRET"), body, req->get("X-Hub-Signature-256"));
                } else if (report) {
                    auth = gh::verifyCallback(environment("CORECI_CALLBACK_SECRET"),
                                              body,
                                              req->get("X-CoreCI-Timestamp"),
                                              req->get("X-CoreCI-Signature-256"),
                                              at);
                } else if (retryUnit || reconcileRoute) {
                    const auto expected = "Bearer " + environment("CORECI_ADMIN_TOKEN"), supplied = req->get("Authorization");
                    auth = supplied.size() == expected.size() && CRYPTO_memcmp(supplied.data(), expected.data(), expected.size()) == 0;
                }
                if (!known) {
                    answer(res, 404, "not found");
                } else if (req->method != ((status || dot) ? "GET" : "POST")) {
                    answer(res, 405, "method not allowed");
                } else if (!auth) {
                    answer(res, 401, "unauthorized");
                } else if (body.size() > (hook ? 25U * 1024U * 1024U : 64U * 1024U)) {
                    answer(res, 413, "body too large");
                } else if (status || dot) {
                    if (queue->engine()) {
                        res->status(200)
                            .set("Content-Type", dot ? "text/vnd.graphviz" : "text/plain")
                            .send(dot ? statusDot(*queue->engine()) : statusText(*queue->engine()));
                    } else {
                        answer(res, 503, "unavailable");
                    }
                } else if (reconcileRoute) {
                    const bool running = reconciling;
                    reconcile();
                    answer(res, 202, running ? "already running" : "started");
                } else if (retryUnit) {
                    const auto package = gh::decodePath(match[1]), target = gh::decodePath(match[2]);
                    if (package && target) {
                        submit(RetryRequested{at, *package, *target}, res);
                    } else {
                        answer(res, 400, "invalid: path encoding");
                    }
                } else if (report) {
                    if (!contentType(req, "application/x-www-form-urlencoded")) {
                        answer(res, 400, "invalid: content type");
                    } else {
                        const auto decoded = gh::form(body);
                        if (std::holds_alternative<gh::Error>(decoded)) {
                            answer(res, 400, "invalid: " + std::get<gh::Error>(decoded).reason);
                        } else {
                            const auto event = gh::report(buildId, std::get<gh::Form>(decoded), at);
                            if (std::holds_alternative<gh::Error>(event)) {
                                answer(res, 400, "invalid: " + std::get<gh::Error>(event).reason);
                            } else if (std::holds_alternative<std::monostate>(event)) {
                                answer(res, 200, "ignored: no event");
                            } else {
                                submit(std::get<Event>(event), res);
                            }
                        }
                    }
                } else {
                    webhook(req, res, body);
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
                        const auto packages = slash == std::string::npos
                                                  ? std::vector<std::string>{}
                                                  : pipeline.packagesForRepo(push.repo.substr(0, slash), push.repo.substr(slash + 1));
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
                                                           deliveries.push_back(delivery);
                                                           if (deliveries.size() > 1000) {
                                                               deliveries.pop_front();
                                                           }
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
            struct Round {
                std::size_t remaining;
            };
            bool reconcile() {
                const bool started = !reconciling;
                if (started) {
                    reconciling = true;
                    std::set<std::pair<std::string, std::string>> repos;
                    for (const auto& [name, p] : pipeline.packages()) {
                        repos.emplace(p.sourceRepo.owner, p.sourceRepo.repo);
                    }
                    const auto round = std::make_shared<Round>(Round{repos.size()});
                    if (repos.empty()) {
                        reconciling = false;
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
                gh::Request request{"GET",
                                    path,
                                    "",
                                    {{"Accept", "application/vnd.github+json"},
                                     {"Authorization", "Bearer " + token.token()},
                                     {"X-GitHub-Api-Version", "2022-11-28"},
                                     {"User-Agent", "core-ci"}}};
                http->request(std::move(request),
                              [this, owner, repo, collected = std::move(collected), visited = std::move(visited), round](
                                  gh::Response response) mutable {
                                  bool valid = response.status == 200;
                                  std::string next;
                                  const auto parsed = gh::tags(response.body);
                                  valid = valid && std::holds_alternative<std::vector<gh::Tag>>(parsed);
                                  if (valid) {
                                      const auto& found = std::get<std::vector<gh::Tag>>(parsed);
                                      for (const auto& tag : found) {
                                          if (tag.sha.size() != 40 || tag.sha.find_first_not_of("0123456789abcdef") != std::string::npos) {
                                              valid = false;
                                          }
                                      }
                                      collected.insert(collected.end(), found.begin(), found.end());
                                      for (const auto& [key, value] : response.headers) {
                                          if (key == "link" || key == "Link") {
                                              const auto links = gh::nextLink(value, *base);
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
                                  }
                                  if (valid && !next.empty()) {
                                      page(owner, repo, next, std::move(collected), std::move(visited), round);
                                  } else {
                                      if (valid) {
                                          for (const auto& package : pipeline.packagesForRepo(owner, repo)) {
                                              std::optional<gh::Tag> best;
                                              for (const auto& tag : collected) {
                                                  const auto version = Version::parse(tag.tag);
                                                  const auto normalized =
                                                      ps::normalize(TagObserved{now(), package, tag.tag, tag.sha, Origin::Reconcile});
                                                  if (version && std::holds_alternative<Event>(normalized) &&
                                                      (!version->prerelease() || pipeline.packages().at(package).acceptPrereleases) &&
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
                                          reconciling = false;
                                      }
                                  }
                              });
            }
            Pipeline pipeline;
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
            bool listening = false, fatal = false, reconciling = false, tickPending = false;
        };
    } // namespace
    int run(int argc, char** argv, Pipeline pipeline) {
        Application application(std::move(pipeline));
        return application.start(argc, argv);
    }
} // namespace coreci::app
