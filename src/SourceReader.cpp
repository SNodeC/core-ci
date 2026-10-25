#include "SourceReader.h"

#include "Definition.h"

#include <algorithm>
#include <cctype>
#include <openssl/evp.h>
#include <regex>
#include <stdexcept>

namespace coreci {
    namespace {
        void require(bool condition, const std::string& message) {
            if (!condition) {
                throw std::invalid_argument(message);
            }
        }

        bool validSha(const std::string& value) {
            static const std::regex syntax(R"(^[a-f0-9]{40}$)");
            return std::regex_match(value, syntax);
        }
    } // namespace

    SourceReader::SourceReader(GitHub& github)
        : github(github) {
    }

    std::string SourceReader::encode(const std::string& value) {
        constexpr char digits[] = "0123456789ABCDEF";
        std::string result;
        for (const unsigned char character : value) {
            if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.' || character == '~') {
                result.push_back(static_cast<char>(character));
            } else {
                result.push_back('%');
                result.push_back(digits[character >> 4]);
                result.push_back(digits[character & 15]);
            }
        }
        return result;
    }

    void SourceReader::select(const std::string& repository, const std::string& tag, const Callback& callback) {
        try {
            if (!tag.empty()) {
                const Version selected(tag);
            }
            const auto reading = std::make_shared<Reading>();
            reading->repository = repository;
            reading->tag = tag;
            reading->callback = callback;
            github.request(repository, "GET", "", nullptr, {{"contents", "read"}}, [this, reading](const GitHubResult& metadata) {
                if (metadata.status != 200 || !metadata.error.empty()) {
                    reading->callback({metadata.status == 404 ? 404 : 502, nullptr, "", "", "could not read authorized source repository"});
                } else {
                    try {
                        require(metadata.body.at("full_name") == reading->repository, "source repository name changed");
                        const auto separator = reading->repository.find('/');
                        reading->source = {{"repository", reading->repository},
                                           {"name", reading->repository.substr(separator + 1)},
                                           {"repository_id", metadata.body.at("id")}};
                        if (reading->tag.empty()) {
                            tags(reading);
                        } else {
                            reference(reading);
                        }
                    } catch (const std::exception& error) {
                        reading->callback({422, nullptr, "", "", error.what()});
                    }
                }
            });
        } catch (const std::exception& error) {
            callback({422, nullptr, "", "", error.what()});
        }
    }

    void SourceReader::tags(const std::shared_ptr<Reading>& reading) {
        github.request(
            reading->repository,
            "GET",
            "/tags?per_page=100&page=" + std::to_string(reading->page),
            nullptr,
            {{"contents", "read"}},
            [this, reading](const GitHubResult& result) {
                if (result.status != 200 || !result.error.empty() || !result.body.is_array() || result.body.size() > 100) {
                    reading->callback({502, nullptr, "", "", "could not enumerate source version tags"});
                } else {
                    try {
                        for (const auto& item : result.body) {
                            const auto name = item.at("name").get<std::string>();
                            std::optional<Version> version;
                            try {
                                version.emplace(name);
                            } catch (const std::invalid_argument&) {
                                // Repositories may also contain tags that are not semantic versions.
                            }
                            if (version) {
                                const int comparison = reading->highest ? version->compare(*reading->highest) : 1;
                                if (comparison > 0) {
                                    reading->highest = version;
                                    reading->ambiguous = false;
                                } else if (comparison == 0 && version->tag != reading->highest->tag) {
                                    reading->ambiguous = true;
                                }
                            }
                        }
                        if (result.body.size() == 100 && reading->page < 128) {
                            ++reading->page;
                            tags(reading);
                        } else if (result.body.size() == 100) {
                            reading->callback({422, nullptr, "", "", "tag enumeration exceeds the 128-page bound; request an exact tag"});
                        } else if (!reading->highest) {
                            reading->callback({404, nullptr, "", "", "repository has no numeric semantic-version tag"});
                        } else if (reading->ambiguous) {
                            reading->callback({409, nullptr, "", "", "highest SemVer precedence is ambiguous; request an exact tag"});
                        } else {
                            reading->tag = reading->highest->tag;
                            reference(reading);
                        }
                    } catch (const std::exception& error) {
                        reading->callback({422, nullptr, "", "", error.what()});
                    }
                }
            });
    }

    void SourceReader::reference(const std::shared_ptr<Reading>& reading) {
        github.request(
            reading->repository,
            "GET",
            "/git/ref/tags/" + encode(reading->tag),
            nullptr,
            {{"contents", "read"}},
            [this, reading](const GitHubResult& result) {
                if (result.status != 200 || !result.error.empty()) {
                    reading->callback({result.status == 404 ? 404 : 502, nullptr, "", "", "selected version tag is unavailable"});
                } else {
                    try {
                        require(result.body.at("ref") == "refs/tags/" + reading->tag, "GitHub returned another source reference");
                        const auto sha = result.body.at("object").at("sha").get<std::string>();
                        reading->source["tag"] = reading->tag;
                        reading->source["tag_object_sha"] = sha;
                        peel(reading, result.body.at("object").at("type").get<std::string>(), sha, 0);
                    } catch (const std::exception& error) {
                        reading->callback({422, nullptr, "", "", error.what()});
                    }
                }
            });
    }

    void SourceReader::peel(const std::shared_ptr<Reading>& reading, const std::string& type, const std::string& sha, unsigned int depth) {
        if (!validSha(sha)) {
            reading->callback({422, nullptr, "", "", "tag object has an invalid immutable identity"});
        } else if (type == "commit") {
            definition(reading->repository, sha, [reading, sha](const SourceResult& result) {
                if (result.status != 200) {
                    reading->callback(result);
                } else {
                    try {
                        auto selected = reading->source;
                        selected["commit"] = sha;
                        selected["definition_revision"] = sha;
                        selected["definition_blob_sha"] = result.blobSha;
                        selected["definition"] = Definition::parse(nlohmann::json::parse(result.content)).json;
                        reading->callback({200, selected, result.content, result.blobSha, ""});
                    } catch (const std::exception& error) {
                        reading->callback({422, nullptr, "", result.blobSha, error.what()});
                    }
                }
            });
        } else if (type == "tag" && depth < 16) {
            github.request(reading->repository,
                           "GET",
                           "/git/tags/" + sha,
                           nullptr,
                           {{"contents", "read"}},
                           [this, reading, sha, depth](const GitHubResult& result) {
                               if (result.status != 200 || !result.error.empty()) {
                                   reading->callback({502, nullptr, "", "", "could not peel selected annotated tag"});
                               } else {
                                   try {
                                       require(result.body.at("sha") == sha, "annotated tag identity differs from request");
                                       peel(reading,
                                            result.body.at("object").at("type").get<std::string>(),
                                            result.body.at("object").at("sha").get<std::string>(),
                                            depth + 1);
                                   } catch (const std::exception& error) {
                                       reading->callback({422, nullptr, "", "", error.what()});
                                   }
                               }
                           });
        } else {
            reading->callback({422, nullptr, "", "", "tag must resolve to a commit within the 16-object bound"});
        }
    }

    void SourceReader::definition(const std::string& repository, const std::string& commit, const Callback& callback) {
        if (!validSha(commit)) {
            callback({422, nullptr, "", "", "definition requires an exact source commit"});
        } else {
            const auto reading = std::make_shared<Reading>();
            reading->repository = repository;
            reading->source = {{"commit", commit}};
            reading->callback = callback;
            github.request(repository,
                           "GET",
                           "/git/commits/" + commit,
                           nullptr,
                           {{"contents", "read"}},
                           [this, reading, commit](const GitHubResult& result) {
                               if (result.status != 200 || !result.error.empty()) {
                                   reading->callback(
                                       {result.status == 404 ? 404 : 502, nullptr, "", "", "selected definition commit is unavailable"});
                               } else {
                                   try {
                                       require(result.body.at("sha") == commit, "definition commit differs from selected source");
                                       tree(reading, result.body.at("tree").at("sha").get<std::string>(), 0);
                                   } catch (const std::exception& error) {
                                       reading->callback({422, nullptr, "", "", error.what()});
                                   }
                               }
                           });
        }
    }

    void SourceReader::tree(const std::shared_ptr<Reading>& reading, const std::string& sha, unsigned int position) {
        if (!validSha(sha)) {
            reading->callback({422, nullptr, "", "", "definition tree has an invalid immutable identity"});
        } else {
            github.request(reading->repository,
                           "GET",
                           "/git/trees/" + sha,
                           nullptr,
                           {{"contents", "read"}},
                           [this, reading, sha, position](const GitHubResult& result) {
                               if (result.status != 200 || !result.error.empty()) {
                                   reading->callback({502, nullptr, "", "", "could not read selected definition tree"});
                               } else {
                                   try {
                                       require(result.body.at("sha") == sha && !result.body.value("truncated", true),
                                               "definition tree is truncated or has changed identity");
                                       const std::string name = position == 0 ? "ci" : "packages.json";
                                       nlohmann::json selected;
                                       for (const auto& entry : result.body.at("tree")) {
                                           if (entry.at("path") == name) {
                                               require(selected.is_null(), "ambiguous definition tree entry");
                                               selected = entry;
                                           }
                                       }
                                       if (selected.is_null()) {
                                           reading->callback(
                                               {404, reading->source, "", "", "ci/packages.json is absent at the selected commit"});
                                       } else if (position == 0) {
                                           require(selected.at("type") == "tree" && selected.at("mode") == "040000",
                                                   "ci must be a directory inside the source checkout");
                                           tree(reading, selected.at("sha").get<std::string>(), 1);
                                       } else {
                                           require(selected.at("type") == "blob" &&
                                                       (selected.at("mode") == "100644" || selected.at("mode") == "100755"),
                                                   "ci/packages.json must be a regular file, not a symlink or submodule");
                                           blob(reading, selected.at("sha").get<std::string>());
                                       }
                                   } catch (const std::exception& error) {
                                       reading->callback({422, nullptr, "", "", error.what()});
                                   }
                               }
                           });
        }
    }

    std::string SourceReader::decode(const nlohmann::json& blob, const std::string& sha) {
        require(validSha(sha) && blob.at("sha") == sha && blob.at("encoding") == "base64",
                "definition blob identity/encoding differs from its tree");
        auto encoded = blob.at("content").get<std::string>();
        encoded.erase(std::remove_if(encoded.begin(),
                                     encoded.end(),
                                     [](unsigned char ch) {
                                         return std::isspace(ch);
                                     }),
                      encoded.end());
        require(encoded.size() <= 1398104 && encoded.size() % 4 == 0, "definition exceeds the one-MiB bound or has invalid base64 length");
        std::size_t padding = 0;
        if (!encoded.empty() && encoded.back() == '=') {
            ++padding;
        }
        if (encoded.size() >= 2 && encoded[encoded.size() - 2] == '=') {
            ++padding;
        }
        for (std::size_t index = 0; index < encoded.size(); ++index) {
            const unsigned char ch = encoded[index];
            require((index >= encoded.size() - padding && ch == '=') ||
                        (index < encoded.size() - padding &&
                         ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '+' || ch == '/')),
                    "invalid base64 definition byte");
        }
        std::string content(encoded.size() / 4 * 3 + 1, '\0');
        const int count = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(content.data()),
                                          reinterpret_cast<const unsigned char*>(encoded.data()),
                                          static_cast<int>(encoded.size()));
        require(count >= static_cast<int>(padding) && static_cast<std::size_t>(count) - padding <= 1024 * 1024,
                "invalid or oversized definition encoding");
        content.resize(static_cast<std::size_t>(count) - padding);
        require(blobSha(content) == sha, "definition bytes do not match the selected Git blob");
        return content;
    }

    std::string SourceReader::blobSha(const std::string& content) {
        std::string object = "blob " + std::to_string(content.size());
        object.push_back('\0');
        object += content;
        unsigned char hash[EVP_MAX_MD_SIZE]{};
        unsigned int size = 0;
        require(EVP_Digest(object.data(), object.size(), hash, &size, EVP_sha1(), nullptr) == 1 && size == 20,
                "could not verify Git blob identity");
        constexpr char digits[] = "0123456789abcdef";
        std::string actual;
        for (unsigned int index = 0; index < size; ++index) {
            actual.push_back(digits[hash[index] >> 4]);
            actual.push_back(digits[hash[index] & 15]);
        }
        return actual;
    }

    void SourceReader::blob(const std::shared_ptr<Reading>& reading, const std::string& sha) {
        if (!validSha(sha)) {
            reading->callback({422, nullptr, "", "", "definition blob has an invalid immutable identity"});
        } else {
            github.request(reading->repository,
                           "GET",
                           "/git/blobs/" + sha,
                           nullptr,
                           {{"contents", "read"}},
                           [reading, sha](const GitHubResult& result) {
                               if (result.status != 200 || !result.error.empty()) {
                                   reading->callback({502, nullptr, "", sha, "could not read selected definition blob"});
                               } else {
                                   try {
                                       const auto content = decode(result.body, sha);
                                       reading->callback({200, reading->source, content, sha, ""});
                                   } catch (const std::exception& error) {
                                       reading->callback({422, reading->source, "", sha, error.what()});
                                   }
                               }
                           });
        }
    }
} // namespace coreci
