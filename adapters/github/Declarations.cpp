#include "Declarations.h"

#include "core/Version.h"

#include <algorithm>
#include <regex>
#include <set>
#include <stdexcept>

namespace coreci::github {
    namespace {
        using Text = std::string;

        bool id(const Text& value) {
            static const std::regex pattern("[A-Za-z0-9_][A-Za-z0-9._-]{0,63}");
            return std::regex_match(value, pattern);
        }

        bool repository(const Text& value) {
            static const std::regex pattern("[A-Za-z0-9-]{1,39}/[A-Za-z0-9_][A-Za-z0-9._-]{0,99}");
            return std::regex_match(value, pattern);
        }

        bool path(const Text& value) {
            static const std::regex pattern(R"([A-Za-z0-9._-]+(/[A-Za-z0-9._-]+)*)");
            return value.size() <= 255 && std::regex_match(value, pattern) && value != "." && value != ".." && !value.starts_with("../") &&
                   !value.starts_with("./") && value.find("/../") == Text::npos && value.find("/./") == Text::npos &&
                   !value.ends_with("/..") && !value.ends_with("/.");
        }

        Text folded(Text value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return std::tolower(ch);
            });
            return value;
        }

        void require(bool condition, const Text& reason) {
            if (!condition) {
                throw std::invalid_argument(reason);
            }
        }

        Json declaration(const Text& body) {
            require(body.size() <= 1024U * 1024U, "file exceeds 1 MiB");
            std::vector<std::set<Text>> keys;
            const auto check = [&](int, Json::parse_event_t event, Json& value) {
                if (event == Json::parse_event_t::object_start) {
                    keys.emplace_back();
                } else if (event == Json::parse_event_t::object_end) {
                    keys.pop_back();
                } else if (event == Json::parse_event_t::key) {
                    require(keys.back().insert(value.get<Text>()).second, "duplicate JSON key");
                }
                return true;
            };
            return Json::parse(body, check);
        }

        void validatePackage(const Json& data) {
            require(data.at("schema") == 2 && id(data.at("package").get<Text>()) && !data.at("title").get<Text>().empty() &&
                        Version::validRange(data.at("releases").get<Text>()),
                    "invalid declaration identity or releases range");
            std::set<Text> targets, dependencies;
            require(data.at("targets").is_array() && data.at("depends_on").is_array(), "invalid declaration arrays");
            for (const auto& target : data.at("targets")) {
                require(id(target.get<Text>()) && targets.insert(target.get<Text>()).second, "invalid target or duplicate");
            }
            for (const auto& dependency : data.at("depends_on")) {
                const auto name = dependency.at("package").get<Text>();
                require(id(name) && dependencies.insert(name).second && Version::validRange(dependency.at("version").get<Text>()),
                        "invalid dependency");
            }
            require(data.at("build_dependencies").is_object() && data.at("build").is_object(), "invalid build declaration");
            const std::regex selector(R"((deb|rpm|openwrt)(@[A-Za-z0-9._]+(-[A-Za-z0-9._]+)+)?)");
            for (const auto& [key, entries] : data.at("build_dependencies").items()) {
                require(std::regex_match(key, selector) && entries.is_array(), "invalid dependency selector");
                for (const auto& entry : entries) {
                    require(entry.is_string() && !entry.get<Text>().empty() && !entry.get<Text>().starts_with('-'),
                            "invalid build package");
                }
            }
            for (const auto& [family, hook] : data.at("build").items()) {
                const auto value = hook.get<Text>();
                require((family == "deb" || family == "rpm" || family == "openwrt" || family == "openwrt_register") &&
                            value.starts_with("ci/") && path(value),
                        "invalid build hook");
            }
        }

        struct Load : std::enable_shared_from_this<Load> {
            DeclarationFile file;
            Text catalogPath;
            Url base;
            Fetch request;
            std::function<void(std::variant<LoadedDeclarations, Error>)> reply;
            LoadedDeclarations loaded;
            std::set<Text> packages;
            std::size_t left = 0;

            void get(Text path, bool raw, std::function<void(Json, Text)> done) {
                const auto self = shared_from_this();
                request(base.path + path, raw, [self, path, raw, done](int status, Text body, Text link) {
                    if (self->reply) {
                        try {
                            require(status == 200, "HTTP " + std::to_string(status));
                            done(raw ? declaration(body) : Json::parse(body), std::move(link));
                        } catch (const std::exception& error) {
                            auto callback = std::move(self->reply);
                            if (callback) {
                                callback(Error{path + ": " + error.what()});
                            }
                        }
                    }
                });
            }

            void repo(const Text& name, std::function<void(Text, Text, Text)> done) {
                const auto self = shared_from_this();
                get("/repos/" + name, false, [self, name, done](const Json& data, const Text&) {
                    const auto canonical = data.at("full_name").get<Text>();
                    const auto branch = data.at("default_branch").get<Text>();
                    require(repository(canonical) && folded(canonical) == folded(name) && !branch.empty() && branch.size() <= 255 &&
                                branch.find_first_of("\r\n\0") == Text::npos,
                            "invalid repository response");
                    self->get("/repos/" + canonical + "/commits/" + escape(branch),
                              false,
                              [done, canonical, branch](const Json& commit, const Text&) {
                                  const auto sha = commit.at("sha").get<Text>();
                                  require(std::regex_match(sha, std::regex("[0-9a-f]{40}")), "invalid HEAD commit");
                                  done(canonical, branch, sha);
                              });
                });
            }

            void start() {
                const auto self = shared_from_this();
                repo(file.owner + "/" + file.repo, [self](const Text& name, const Text&, const Text& sha) {
                    const auto prefix = "/repos/" + name + "/contents/";
                    self->get(prefix + self->file.path + "?ref=" + sha, true, [self, prefix, sha](const Json& registry, const Text&) {
                        require(registry.at("schema") == 1 && registry.at("repositories").is_array(), "invalid registry");
                        std::set<Text> seen;
                        for (const auto& name : registry.at("repositories")) {
                            const auto value = name.get<Text>();
                            require(repository(value) && seen.insert(folded(value)).second, "invalid or duplicate repository");
                        }
                        self->get(prefix + self->catalogPath + "?ref=" + sha, true, [self, registry](const Json& catalog, const Text&) {
                            require(catalog.at("targets").is_array(), "invalid catalog");
                            for (const auto& target : catalog.at("targets")) {
                                const auto idValue = target.at("id").get<Text>();
                                const auto family = target.at("family").get<Text>();
                                const auto& runner = catalog.at("runners").at(target.at("runner").get<Text>());
                                require(
                                    id(idValue) && !self->loaded.catalog.contains(idValue) &&
                                        std::regex_match(runner.at("request_prefix").get<Text>(), std::regex("[a-z0-9][a-z0-9-]{0,23}")) &&
                                        (family == "deb" || family == "rpm" || family == "ipk" || family == "apk"),
                                    "invalid catalog target");
                                self->loaded.catalog[idValue] = target;
                                self->loaded.catalog[idValue]["runner_prefix"] = runner.at("request_prefix");
                            }
                            self->left = registry.at("repositories").size();
                            if (self->left == 0) {
                                self->finish();
                            }
                            for (const auto& name : registry.at("repositories")) {
                                self->source(name.get<Text>());
                            }
                        });
                    });
                });
            }

            void source(const Text& name) {
                const auto self = shared_from_this();
                repo(name, [self](const Text& canonical, const Text& branch, const Text& sha) {
                    const auto prefix = "/repos/" + canonical;
                    self->get(
                        prefix + "/contents/?ref=" + sha, false, [self, canonical, branch, sha, prefix](const Json& tree, const Text&) {
                            const auto found = std::find_if(tree.begin(), tree.end(), [](const Json& entry) {
                                return entry.at("name") == "ci";
                            });
                            require(found != tree.end() && found->at("type") == "dir", "repository has no ci tree");
                            const auto hash = found->at("sha").get<Text>();
                            self->get(prefix + "/contents/ci/packages.json?ref=" + sha,
                                      true,
                                      [self, canonical, branch, sha, hash, prefix](Json data, const Text&) {
                                          validatePackage(data);
                                          require(self->packages.insert(data.at("package").get<Text>()).second, "duplicate package");
                                          data.update({{"ci_commit", sha}, {"tree", hash}, {"branch", branch}});
                                          self->loaded.declarations[canonical] = std::move(data);
                                          self->tagPages(canonical, prefix + "/tags?per_page=100", {});
                                      });
                        });
                });
            }

            void tagPages(const Text& name, const Text& page, std::set<Text> visited) {
                const auto self = shared_from_this();
                require(visited.insert(page).second, "tag pagination loop");
                get(page, false, [self, name, visited = std::move(visited)](const Json& tags, const Text& link) {
                    require(tags.is_array(), "invalid tag page");
                    for (const auto& tag : tags) {
                        const auto value = tag.at("name").get<Text>();
                        if (stableTag(value)) {
                            const auto commit = tag.at("commit").at("sha").get<Text>();
                            require(std::regex_match(commit, std::regex("[0-9a-f]{40}")), "invalid tagged commit");
                            self->loaded.releases.push_back({{"repo", name}, {"tag", value}, {"commit", commit}});
                        }
                    }
                    const auto next = nextLink(link, self->base);
                    require(!std::holds_alternative<Error>(next), "invalid tag pagination");
                    if (const auto& url = std::get<std::optional<Text>>(next); url) {
                        auto location = httpsUrl(*url).value().path;
                        require(location.starts_with(self->base.path), "invalid API path");
                        self->tagPages(name, location.substr(self->base.path.size()), visited);
                    } else if (--self->left == 0) {
                        self->finish();
                    }
                });
            }

            void finish() {
                auto callback = std::move(reply);
                callback(std::move(loaded));
            }
        };
    } // namespace

    std::variant<DeclarationFile, Error> registryFile(const std::string& value) {
        const auto slash = value.find('/');
        const auto colon = value.find(':');
        std::variant<DeclarationFile, Error> result = Error{"invalid registry location"};
        if (slash != std::string::npos && colon != std::string::npos && slash < colon && repository(value.substr(0, colon)) &&
            path(value.substr(colon + 1))) {
            result = DeclarationFile{value.substr(0, slash), value.substr(slash + 1, colon - slash - 1), value.substr(colon + 1)};
        }
        return result;
    }

    void loadDeclarations(const DeclarationFile& registry,
                          const std::string& catalogPath,
                          const Url& base,
                          Fetch fetch,
                          std::function<void(std::variant<LoadedDeclarations, Error>)> done) {
        auto load = std::make_shared<Load>();
        load->file = registry;
        load->catalogPath = catalogPath;
        load->base = base;
        load->request = std::move(fetch);
        load->reply = std::move(done);
        load->start();
    }
} // namespace coreci::github
