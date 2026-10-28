#include "SourceReader.h"

#include "Graph.h"

#include <regex>
#include <set>
#include <sstream>
#include <utility>
#include <web/http/http_utils.h>

namespace coreci {
    std::string SourceReader::blobSha(const std::string& content) {
        return hash("blob " + std::to_string(content.size()) + '\0' + content, EVP_sha1());
    }
    void SourceReader::finish(const std::shared_ptr<Reading>& reading, const SourceResult& result) {
        if (const auto callback = std::exchange(reading->callback, nullptr)) {
            callback(result);
        }
    }
    void SourceReader::read(const std::shared_ptr<Reading>& reading, const std::string& suffix, const GitHub::Callback& callback) {
        github.request(
            reading->repository,
            "GET",
            suffix,
            nullptr,
            {{"contents", "read"}},
            [reading, callback](const GitHubResult& response) {
                try {
                    if (response.status == 200 && response.error.empty()) {
                        callback(response);
                    } else {
                        reading->result.status = response.error.empty() && response.status ? response.status : 503;
                        reading->result.error = response.error.empty() ? "GitHub source evidence is unavailable" : response.error;
                        finish(reading, reading->result);
                    }
                } catch (const std::exception& error) {
                    reading->result.status = 422;
                    reading->result.error = error.what();
                    finish(reading, reading->result);
                }
            },
            reading->publicRepository);
    }
    void SourceReader::select(const std::string& repository, const std::string& tag, const Callback& callback) {
        try {
            const Version version(tag.empty() ? "0.0.0" : tag);
            auto reading = std::make_shared<Reading>();
            reading->repository = repository;
            reading->tag = tag;
            reading->callback = callback;
            read(reading, "", [this, reading](const GitHubResult& result) {
                require(result.body.at("id") == github.repositoryId(reading->repository), "repository ID differs from enrollment/grant");
                reading->result.source = {{"repository", reading->repository},
                                          {"repository_id", result.body.at("id")},
                                          {"name", reading->repository.substr(reading->repository.find('/') + 1)}};
                if (reading->tag.empty()) {
                    tags(reading);
                } else {
                    reference(reading);
                }
            });
        } catch (const std::exception& error) {
            callback({422, nullptr, "", "", error.what()});
        }
    }
    void SourceReader::tags(const std::shared_ptr<Reading>& reading) {
        read(reading, "/tags?per_page=100&page=" + std::to_string(reading->page), [this, reading](const GitHubResult& result) {
            require(result.body.is_array() && result.body.size() <= 100, "invalid GitHub tag listing");
            for (const auto& row : result.body) {
                const auto name = row.at("name").get<std::string>();
                try {
                    const Version version(name);
                    const auto order = reading->tag.empty() ? 1 : version.compare(Version(reading->tag));
                    if (order > 0) {
                        reading->tag = name;
                        reading->ambiguous = false;
                    } else if (order == 0 && reading->tag != name) {
                        reading->ambiguous = true;
                    }
                } catch (const std::invalid_argument&) {
                }
            }
            if (result.body.size() == 100) {
                require(++reading->page <= 32, "tag listing exceeds supported bound");
                tags(reading);
            } else if (reading->tag.empty()) {
                reading->result.status = 404;
                reading->result.error = "repository has no semantic-version tag";
                finish(reading, reading->result);
            } else if (reading->ambiguous) {
                reading->result.status = 409;
                reading->result.error = "highest semantic-version tag is ambiguous";
                finish(reading, reading->result);
            } else {
                reference(reading);
            }
        });
    }
    void SourceReader::reference(const std::shared_ptr<Reading>& reading) {
        read(reading, "/git/ref/tags/" + httputils::url_encode(reading->tag), [this, reading](const GitHubResult& result) {
            reading->result.source["tag"] = reading->tag;
            reading->result.source["tag_object_sha"] = result.body.at("object").at("sha");
            peel(reading, result.body.at("object"), 0);
        });
    }
    void SourceReader::peel(const std::shared_ptr<Reading>& reading, const Json& object, unsigned int depth) {
        static const std::regex sha("^[a-f0-9]{40}$");
        const auto id = object.at("sha").get<std::string>();
        require(depth < 16 && std::regex_match(id, sha), "invalid or excessive tag indirection");
        if (object.at("type") == "tag") {
            read(reading, "/git/tags/" + id, [this, reading, depth](const GitHubResult& result) {
                peel(reading, result.body.at("object"), depth + 1);
            });
        } else {
            require(object.at("type") == "commit", "selected tag does not resolve to a commit");
            definition(reading->repository, id, [reading](const SourceResult& result) {
                auto selected = result;
                selected.source.update(reading->result.source);
                finish(reading, selected);
            });
        }
    }
    void SourceReader::definition(const std::string& repository, const std::string& ref, const Callback& callback) {
        file(repository, ref, "ci/packages.json", [callback](const SourceResult& file) {
            auto result = file;
            if (result.error.empty()) {
                try {
                    result.source["definition"] = parseDefinition(result.content);
                    result.source["definition_blob_sha"] = result.blobSha;
                    result.source["definition_revision"] = result.source.at("commit");
                } catch (const std::exception& error) {
                    result.status = 422;
                    result.error = error.what();
                }
            }
            callback(result);
        });
    }
    void SourceReader::file(const std::string& repository,
                            const std::string& ref,
                            const std::string& path,
                            const Callback& callback,
                            bool publicRepository,
                            const std::string& root,
                            bool contents) {
        auto reading = std::make_shared<Reading>();
        reading->repository = repository;
        reading->callback = callback;
        reading->publicRepository = publicRepository;
        reading->contents = contents;
        reading->result.source = {{"commit", ref}};
        reading->path = pathParts(path);
        if (!root.empty()) {
            tree(reading, root);
        } else {
            read(reading, "/commits/" + httputils::url_encode(ref), [this, reading, ref](const GitHubResult& result) {
                static const std::regex sha("^[a-f0-9]{40}$");
                require(std::regex_match(result.body.at("sha").get<std::string>(), sha) &&
                            (!std::regex_match(ref, sha) || result.body.at("sha") == ref),
                        "Git commit identity differs from lookup");
                reading->result.source["commit"] = result.body.at("sha");
                tree(reading, result.body.at("commit").at("tree").at("sha"));
            });
        }
    }
    void SourceReader::tree(const std::shared_ptr<Reading>& reading, const std::string& sha) {
        read(reading, "/git/trees/" + sha + "?recursive=1", [this, reading](const GitHubResult& result) {
            require(!result.body.value("truncated", true) && result.body.at("tree").is_array() && result.body.at("tree").size() <= 20000,
                    "incomplete or excessive Git tree");
            const auto& entries = result.body.at("tree");
            auto found = entries.end();
            std::string path;
            bool present = true;
            for (std::size_t position = 0; present && position < reading->path.size(); ++position) {
                path += (path.empty() ? "" : "/") + reading->path.at(position);
                found = std::find_if(entries.begin(), entries.end(), [&](const Json& entry) {
                    return entry.at("path") == path;
                });
                present = found != entries.end();
                if (present && position + 1 < reading->path.size()) {
                    require(found->at("type") == "tree" && found->at("mode") == "040000", "Git path contains a link/submodule");
                }
            }
            if (!present) {
                reading->result.status = 404;
                reading->result.error = "Git file is absent";
                finish(reading, reading->result);
            } else {
                require(found->at("type") == "blob" && (found->at("mode") == "100644" || found->at("mode") == "100755"),
                        "Git file is not regular");
                reading->result.blobSha = found->at("sha");
                require(std::regex_match(reading->result.blobSha, std::regex("^[a-f0-9]{40}$")), "invalid Git blob identity");
                if (reading->contents) {
                    read(reading, "/git/blobs/" + reading->result.blobSha, [reading](const GitHubResult& blob) {
                        require(blob.body.at("encoding") == "base64" && blob.body.at("size").get<std::size_t>() <= 2 * 1024 * 1024,
                                "invalid or oversized Git blob");
                        reading->result.content = unbase64(blob.body.at("content"));
                        require(reading->result.content.size() == blob.body.at("size") &&
                                    blobSha(reading->result.content) == reading->result.blobSha,
                                "Git blob bytes do not match their identity");
                        finish(reading, reading->result);
                    });
                } else {
                    finish(reading, reading->result);
                }
            }
        });
    }
    std::vector<std::string> SourceReader::pathParts(const std::string& path) {
        std::vector<std::string> result;
        std::istringstream components(path);
        std::string component;
        require(!path.empty() && path.size() <= 512 && !path.ends_with('/') && path.find('\\') == std::string::npos &&
                    std::none_of(path.begin(),
                                 path.end(),
                                 [](unsigned char c) {
                                     return c < 32 || c == 127;
                                 }),
                "invalid relative Git path");
        while (std::getline(components, component, '/')) {
            require(!component.empty() && component != "." && component != "..", "unsafe relative Git path");
            result.push_back(component);
        }
        require(result.size() <= 16, "excessive relative Git path");
        return result;
    }
    Json SourceReader::parseDefinition(const std::string& content) {
        auto definition = Json::parse(content);
        static const std::set<std::string> allowed{"schema", "title", "package", "depends_on", "build", "matrix"};
        for (const auto& [key, value] : definition.items()) {
            require(allowed.contains(key), "unsupported definition field: " + key);
        }
        static const std::regex package("^[a-z0-9][a-z0-9+.-]{0,127}$");
        require(definition.at("schema") == 1 && !definition.at("title").get<std::string>().empty() &&
                    std::regex_match(definition.at("package").get<std::string>(), package) && definition.at("depends_on").is_array() &&
                    definition.at("depends_on").size() <= 128 && definition.at("matrix").is_array() && !definition.at("matrix").empty() &&
                    definition.at("matrix").size() <= 256,
                "invalid package definition");
        const auto fields = [](const Json& object, const std::set<std::string>& allowed) {
            require(object.is_object(), "definition section must be an object");
            for (const auto& [key, value] : object.items()) {
                require(allowed.contains(key), "unsupported definition field: " + key);
            }
        };
        fields(definition.at("build"), {"native", "openwrt"});
        std::set<std::string> dependencies, ids, coordinates;
        static const std::regex coordinate("^[A-Za-z0-9][A-Za-z0-9_.+-]{0,127}$");
        for (const auto& dependency : definition.at("depends_on")) {
            const auto name = dependency.get<std::string>();
            require(GitHub::validRepository("Owner/" + name) && dependencies.insert(name).second, "invalid or duplicate dependency");
        }
        for (const auto& target : definition.at("matrix")) {
            fields(target, {"id", "family", "distribution", "suite", "arch", "runner", "build"});
            const auto family = target.at("family").get<std::string>();
            const auto format = target.at("build").at("format").get<std::string>();
            fields(target.at("build"),
                   family == "native" ? std::set<std::string>{"format", "image", "platform"} : std::set<std::string>{"format", "target"});
            for (const auto& [key, value] : target.at("build").items()) {
                require(value.is_string() && !value.get_ref<const std::string&>().empty(), "invalid target build value");
            }
            require(family != "native" || target.at("build").contains("image"), "native target image is required");
            for (const auto* key : {"id", "distribution", "suite", "arch"}) {
                require(std::regex_match(target.at(key).get<std::string>(), coordinate), "invalid target coordinate");
            }
            require(ids.insert(target.at("id")).second && coordinates.insert(graph::coordinates(target).dump()).second &&
                        ((family == "native" && (format == "deb" || format == "rpm")) ||
                         (family == "openwrt" && (format == "ipk" || format == "apk"))),
                    "invalid or duplicate package target");
            pathParts(definition.at("build").at(family));
            require(target.at("runner").is_string() || (target.at("runner").is_array() && !target.at("runner").empty()),
                    "invalid runner selector");
            const auto labels = target.at("runner").is_string() ? Json::array({target.at("runner")}) : target.at("runner");
            require(labels.size() <= 32, "excessive runner selector");
            for (const auto& label : labels) {
                require(label.is_string() && !label.get_ref<const std::string&>().empty() &&
                            label.get_ref<const std::string&>().size() <= 128,
                        "invalid runner label");
            }
        }
        return definition;
    }
} // namespace coreci
