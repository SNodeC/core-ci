#include "Declarations.h"

#include <nlohmann/json.hpp>
#include <regex>
#include <stdexcept>
namespace coreci::github {
    namespace {
        using Json = nlohmann::json;
        Json parse(const std::string& body) {
            if (body.size() > 1024U * 1024U) {
                throw std::invalid_argument("file exceeds 1 MiB");
            }
            std::vector<std::set<std::string>> keys;
            const auto callback = [&](int, Json::parse_event_t event, Json& value) {
                if (event == Json::parse_event_t::object_start) {
                    keys.emplace_back();
                } else if (event == Json::parse_event_t::object_end) {
                    keys.pop_back();
                } else if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) {
                    throw std::invalid_argument("duplicate object key");
                }
                return true;
            };
            return Json::parse(body, callback);
        }
        Repo repo(const std::string& value) {
            const auto slash = value.find('/');
            if (slash == std::string::npos || value.find('/', slash + 1) != std::string::npos) {
                throw std::invalid_argument("invalid repository reference");
            }
            const Repo result{value.substr(0, slash), value.substr(slash + 1), {}};
            validateRepository(result.owner, result.repo);
            return result;
        }
        Workflow workflow(const std::string& value) {
            const std::string marker = "/.github/workflows/";
            const auto at = value.find(marker);
            if (at == std::string::npos) {
                throw std::invalid_argument("invalid workflow reference");
            }
            const auto source = repo(value.substr(0, at));
            Workflow result{source.owner, source.repo, value.substr(at + marker.size()), {}};
            validateWorkflowFile(result.workflowFile);
            return result;
        }
        std::string key(const Repo& value) {
            return repoKey(value.owner + "/" + value.repo);
        }
        struct Load : std::enable_shared_from_this<Load> {
            DeclarationFile file;
            std::string apiPath;
            Fetch fetch;
            std::function<void(std::variant<LoadedDeclarations, Error>)> done;
            std::map<std::string, Repo> repositories;
            std::vector<Repo> sources;
            std::vector<Feed> feeds;
            std::map<std::string, Workflow> presentations;
            std::map<std::string, CatalogTarget> catalog;
            std::map<std::string, PackageDetails> details;
            std::map<std::string, std::string> commits;
            Workflow build;
            std::string defaultFeed;
            std::size_t remaining = 0;
            Pipeline::Builder builder;
            void fail(const std::string& repository, const std::string& path, const std::string& reason) {
                auto callback = std::move(done);
                std::string line = repository + ":" + path + ": " + reason;
                for (auto& c : line) {
                    if (c == '\r' || c == '\n') {
                        c = ' ';
                    }
                }
                callback(Error{line});
            }
            void get(const Repo& source, const std::string& path, std::function<void(Json)> callback, bool info = false) {
                const auto self = shared_from_this();
                const auto& target = info ? source : repositories.at(key(source));
                std::string url = apiPath + "/repos/" + escape(target.owner) + "/" + escape(target.repo);
                if (!info) {
                    url += "/contents/" + escape(path) + "?ref=" + escape(target.branch);
                }
                fetch(std::move(url), !info, [self, target, path, callback, info](int status, std::string body, std::string) {
                    if (!self->done) {
                        // Another request has already failed this whole load.
                    } else if (status != 200) {
                        self->fail(target.owner + "/" + target.repo, path, "HTTP " + std::to_string(status));
                    } else {
                        try {
                            callback(info ? Json::parse(body) : parse(body));
                        } catch (const std::exception& error) {
                            self->fail(target.owner + "/" + target.repo, path, error.what());
                        }
                    }
                });
            }
            void branch(const Repo& source, std::function<void()> next) {
                if (repositories.contains(key(source))) {
                    next();
                } else {
                    const auto self = shared_from_this();
                    get(
                        source,
                        "<repository>",
                        [self, source, next](Json value) {
                            auto canonical = repo(value.at("full_name").get<std::string>());
                            canonical.branch = value.at("default_branch").get<std::string>();
                            if (!validBranch(canonical.branch)) {
                                throw std::invalid_argument("invalid default branch");
                            }
                            self->repositories[key(source)] = std::move(canonical);
                            const auto& target = self->repositories.at(key(source));
                            self->fetch(self->apiPath + "/repos/" + escape(target.owner) + "/" + escape(target.repo) + "/commits/" +
                                            escape(target.branch),
                                        false,
                                        [self, source, next](int status, std::string body, std::string) {
                                            if (self->done) {
                                                try {
                                                    const auto sha = Json::parse(body).at("sha").get<std::string>();
                                                    if (status != 200 || !std::regex_match(sha, std::regex("[a-f0-9]{40}"))) {
                                                        throw std::invalid_argument("invalid HEAD commit");
                                                    }
                                                    self->commits[key(source)] = sha;
                                                    next();
                                                } catch (const std::exception& error) {
                                                    self->fail(source.owner + "/" + source.repo, "<commit>", error.what());
                                                }
                                            }
                                        });
                        },
                        true);
                }
            }
            void start() {
                const auto self = shared_from_this();
                const Repo registry{file.owner, file.repo, {}};
                branch(registry, [self, registry] {
                    self->get(registry, self->file.path, [self, registry](Json value) {
                        const auto& feeds = value.at("feeds");
                        if (!feeds.is_object()) {
                            throw std::invalid_argument("feeds must be an object");
                        }
                        self->defaultFeed = value.at("default_feed").get<std::string>();
                        self->build = workflow(value.at("build").get<std::string>());
                        for (const auto& [id, definition] : feeds.items()) {
                            self->feeds.push_back({id, workflow(definition.at("publisher").get<std::string>())});
                            if (definition.contains("presentation")) {
                                self->presentations.emplace(id, workflow(definition.at("presentation").get<std::string>()));
                                if (!httpsUrl(value.at("status_url").get<std::string>())) {
                                    throw std::invalid_argument("invalid status_url");
                                }
                            }
                        }
                        if (!feeds.contains(self->defaultFeed)) {
                            throw std::invalid_argument("unknown default_feed");
                        }
                        const auto& sources = value.at("repositories");
                        if (!sources.is_array()) {
                            throw std::invalid_argument("repositories must be an array");
                        }
                        for (const auto& source : sources) {
                            self->sources.push_back(repo(source.get<std::string>()));
                        }
                        const auto path = value.at("targets").get<std::string>();
                        const auto checked = registryFile(registry.owner + "/" + registry.repo + ":" + path);
                        if (std::holds_alternative<Error>(checked)) {
                            throw std::invalid_argument("invalid catalog path");
                        }
                        self->get(registry, path, [self](Json data) {
                            const auto& targets = data.at("targets");
                            if (!targets.is_array()) {
                                throw std::invalid_argument("catalog targets must be an array");
                            }
                            for (const auto& entry : targets) {
                                const auto id = entry.at("id").get<std::string>();
                                validateIdentifier(id, "catalog target");
                                CatalogTarget coordinates{entry.at("distribution").get<std::string>(),
                                                          entry.at("suite").get<std::string>(),
                                                          entry.at("arch").get<std::string>(),
                                                          entry.at("build").at("format").get<std::string>()};
                                if (!self->catalog.emplace(id, std::move(coordinates)).second) {
                                    throw std::invalid_argument("duplicate catalog id: " + id);
                                }
                            }
                            self->resolve();
                        });
                    });
                });
            }
            void resolve() {
                std::map<std::string, Repo> repositories;
                for (const auto& source : sources) {
                    repositories.emplace(key(source), source);
                }
                repositories.emplace(key({build.owner, build.repo}), Repo{build.owner, build.repo, {}});
                for (const auto& feed : feeds) {
                    repositories.emplace(key({feed.workflow.owner, feed.workflow.repo}), Repo{feed.workflow.owner, feed.workflow.repo, {}});
                }
                for (const auto& [id, presentation] : presentations) {
                    repositories.emplace(key({presentation.owner, presentation.repo}), Repo{presentation.owner, presentation.repo, {}});
                }
                remaining = repositories.size();
                const auto self = shared_from_this();
                for (const auto& [name, source] : repositories) {
                    branch(source, [self] {
                        if (--self->remaining == 0) {
                            self->packages();
                        }
                    });
                }
            }
            void finish() {
                const auto pipeline = builder.build();
                auto callback = std::move(done);
                auto writers = presentations;
                for (auto& [id, writer] : writers) {
                    writer = resolved(writer);
                    writer.branch = repositories.at(key({file.owner, file.repo})).branch;
                }
                callback(LoadedDeclarations{pipeline, writers, catalog, details});
            }
            Workflow resolved(const Workflow& workflow) const {
                const auto& canonical = repositories.at(key({workflow.owner, workflow.repo}));
                return {canonical.owner, canonical.repo, workflow.workflowFile, canonical.branch};
            }
            void packages() {
                build = resolved(build);
                for (auto feed : feeds) {
                    feed.workflow = resolved(feed.workflow);
                    builder.feed(feed);
                }
                remaining = sources.size();
                if (sources.empty()) {
                    finish();
                } else {
                    const auto self = shared_from_this();
                    for (const auto& source : sources) {
                        get(source, "ci/packages.json", [self, source](Json value) {
                            Package pkg;
                            pkg.name = value.at("package").get<std::string>();
                            pkg.sourceRepo = self->repositories.at(key(source));
                            pkg.buildWorkflow = self->build;
                            self->details[pkg.name] = {value.value("title", pkg.sourceRepo.owner + "/" + pkg.sourceRepo.repo),
                                                       self->commits.at(key(source))};
                            const auto& prerequisites = value.at("depends_on");
                            if (!prerequisites.is_array()) {
                                throw std::invalid_argument("depends_on must be an array");
                            }
                            for (const auto& q : prerequisites) {
                                pkg.prerequisites.push_back(q.get<std::string>());
                            }
                            pkg.acceptPrereleases = value.value("accept_prereleases", false);
                            const auto base = value.value("revision_base", Json(0));
                            if (!base.is_number_integer() || base < 0 || base > 1000000) {
                                throw std::invalid_argument("revision_base must be an integer in 0–1000000");
                            }
                            pkg.revisionBase = base.get<unsigned>();
                            const auto& matrix = value.at("matrix");
                            if (!matrix.is_array()) {
                                throw std::invalid_argument("matrix must be an array");
                            }
                            for (const auto& target : matrix) {
                                const auto id = target.at("id").get<std::string>();
                                if (!target.is_object() || target.size() > (target.contains("feed") ? 2U : 1U) ||
                                    !self->catalog.contains(id)) {
                                    throw std::invalid_argument("unknown catalog id or extra matrix field: " + id);
                                }
                                if (!pkg.targets.insert(id).second) {
                                    throw std::invalid_argument("duplicate target id: " + id);
                                }
                                self->builder.target({id, target.value("feed", self->defaultFeed)});
                            }
                            self->builder.package(pkg);
                            if (--self->remaining == 0) {
                                self->finish();
                            }
                        });
                    }
                }
            }
        };
    } // namespace
    std::variant<DeclarationFile, Error> registryFile(const std::string& value) {
        std::variant<DeclarationFile, Error> result;
        try {
            const auto colon = value.find(':');
            if (colon == std::string::npos) {
                throw std::invalid_argument("invalid registry reference");
            }
            const auto source = repo(value.substr(0, colon));
            const auto path = value.substr(colon + 1);
            if (path.size() > 255 || !std::regex_match(path, std::regex("[A-Za-z0-9._-]+(/[A-Za-z0-9._-]+)*"))) {
                throw std::invalid_argument("invalid registry path");
            }
            std::size_t start = 0;
            for (std::size_t i = 0; i <= path.size(); ++i) {
                if (i == path.size() || path[i] == '/') {
                    const auto segment = path.substr(start, i - start);
                    if (segment == "." || segment == "..") {
                        throw std::invalid_argument("invalid registry path segment");
                    }
                    start = i + 1;
                }
            }
            result = DeclarationFile{source.owner, source.repo, path};
        } catch (const std::exception& error) {
            result = Error{error.what()};
        }
        return result;
    }
    void loadDeclarations(const DeclarationFile& registry,
                          const std::string& apiPath,
                          Fetch fetch,
                          std::function<void(std::variant<LoadedDeclarations, Error>)> done) {
        auto load = std::make_shared<Load>();
        load->file = registry;
        load->apiPath = apiPath;
        load->fetch = std::move(fetch);
        load->done = std::move(done);
        load->start();
    }
} // namespace coreci::github
