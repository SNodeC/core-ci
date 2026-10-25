#include "Enrollment.h"

#include <algorithm>
#include <cctype>
#include <openssl/evp.h>
#include <regex>
#include <stdexcept>

namespace coreci {

    namespace {
        std::string encoded(const std::string& value) {
            constexpr char digits[] = "0123456789ABCDEF";
            std::string result;
            for (const unsigned char character : value) {
                if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                    (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.' ||
                    character == '~') {
                    result.push_back(static_cast<char>(character));
                } else {
                    result.push_back('%');
                    result.push_back(digits[character >> 4]);
                    result.push_back(digits[character & 15]);
                }
            }
            return result;
        }

        std::string base64(const std::string& value) {
            std::string result(4 * ((value.size() + 2) / 3) + 1, '\0');
            const int size = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(result.data()),
                                             reinterpret_cast<const unsigned char*>(value.data()),
                                             static_cast<int>(value.size()));
            result.resize(static_cast<std::size_t>(size));
            return result;
        }

        void require(bool condition, const std::string& message) {
            if (!condition) {
                throw std::invalid_argument(message);
            }
        }
    } // namespace

    Enrollment::Enrollment(Database& database, GitHub& github)
        : database(database)
        , github(github) {
    }

    void Enrollment::restore(const Callback& callback) {
        database.query("SELECT JSON_OBJECT('version', version) FROM schema_version", [this, callback](const DatabaseResult& result) {
            if (!result.error.empty() || result.rows.size() != 1 || result.rows.front().value("version", 0) != 1) {
                callback({503, {{"error", "database schema is unavailable; apply sql/schema.sql before startup"}}});
            } else {
                list(callback);
            }
        });
    }

    void Enrollment::list(const Callback& callback) {
        database.query(
            "SELECT JSON_OBJECT('id', r.github_id, 'name', r.name, 'repository', r.full_name, "
            "'branch', r.editing_ref, 'revision', r.configuration_revision, 'definition', JSON_EXTRACT(d.document, '$')) "
            "FROM repositories r JOIN definitions d ON d.repository_id=r.github_id AND d.revision=r.configuration_revision ORDER BY r.name",
            [callback](const DatabaseResult& result) {
                if (result.error.empty()) {
                    try {
                        std::map<std::string, Definition> definitions;
                        for (const auto& row : result.rows) {
                            require(definitions.emplace(row.at("name").get<std::string>(), Definition::parse(row.at("definition"))).second,
                                    "duplicate enrolled repository name");
                        }
                        Definition::validateGraph(definitions);
                        callback({200, {{"repositories", result.rows}}});
                    } catch (const std::exception& error) {
                        callback({503, {{"error", error.what()}}});
                    }
                } else {
                    callback({503, {{"error", "could not read enrolled configurations"}}});
                }
            });
    }

    void Enrollment::save(const nlohmann::json& request, const Callback& callback) {
        if (!database.mutationsAllowed()) {
            callback({503, {{"error", "database mutation outcome is uncertain; restart and reconcile before configuration changes"}}});
        } else if (saving) {
            callback({409, {{"error", "another configuration write is in progress; refresh before saving"}}});
        } else {
            try {
                const auto save = std::make_shared<Save>();
                save->repository = request.at("repository").get<std::string>();
                save->branch = request.at("branch").get<std::string>();
                save->expectedRevision = request.value("revision", std::string{});
                save->definition = Definition::parse(request.at("definition"));
                save->callback = callback;
                const auto separator = save->repository.find('/');
                static const std::regex syntax(R"(^[A-Za-z0-9_-]+/[a-z0-9][a-z0-9.-]*$)");
                require(save->repository.size() <= 256 && std::regex_match(save->repository, syntax),
                        "use an owner/repository name; enrolled names must be lowercase");
                require(!save->branch.empty() && save->branch.size() <= 256, "specify the configuration editing branch");
                save->name = save->repository.substr(separator + 1);
                saving = true;
                list([this, save](const EnrollmentResult& listed) {
                    if (listed.status != 200) {
                        finish(save, listed.status, listed.body);
                    } else {
                        try {
                            std::map<std::string, Definition> definitions;
                            bool enrolled = false;
                            for (const auto& row : listed.body.at("repositories")) {
                                const auto name = row.at("name").get<std::string>();
                                if (name == save->name) {
                                    require(row.at("repository") == save->repository,
                                            "repository name is already enrolled under another owner");
                                    require(row.at("branch") == save->branch, "configuration branch must match the enrolled branch");
                                    require(row.at("revision") == save->expectedRevision,
                                            "configuration revision changed; refresh before saving");
                                    save->id = row.at("id").get<std::uint64_t>();
                                    enrolled = true;
                                }
                                definitions.emplace(name, Definition::parse(row.at("definition")));
                            }
                            require(enrolled || save->expectedRevision.empty(),
                                    "new enrollment must not claim an existing configuration revision");
                            definitions.insert_or_assign(save->name, save->definition);
                            Definition::validateGraph(definitions);
                            github.request(
                                save->repository, "GET", "", nullptr, {{"contents", "read"}}, [this, save](const GitHubResult& metadata) {
                                    try {
                                        require(metadata.status == 200 && metadata.error.empty(),
                                                "could not read authorized GitHub repository");
                                        require(metadata.body.at("full_name") == save->repository,
                                                "GitHub repository identity differs from enrollment");
                                        const auto id = metadata.body.at("id").get<std::uint64_t>();
                                        require(id != 0 && (save->id == 0 || save->id == id), "GitHub repository ID changed");
                                        save->id = id;
                                        readHead(save);
                                    } catch (const std::exception& error) {
                                        finish(save,
                                               502,
                                               {{"error", error.what()}, {"github_status", metadata.status}, {"github_committed", false}});
                                    }
                                });
                        } catch (const std::exception& error) {
                            finish(save, 409, {{"error", error.what()}, {"github_committed", false}});
                        }
                    }
                });
            } catch (const std::exception& error) {
                callback({422, {{"error", error.what()}, {"github_committed", false}}});
            }
        }
    }

    void Enrollment::refresh(const nlohmann::json& request, const Callback& callback) {
        if (!database.mutationsAllowed()) {
            callback({503, {{"error", "database mutation outcome is uncertain; restart and reconcile before configuration changes"}}});
        } else if (saving) {
            callback({409, {{"error", "another configuration operation is in progress"}}});
        } else {
            try {
                const auto save = std::make_shared<Save>();
                save->repository = request.at("repository").get<std::string>();
                save->branch = request.at("branch").get<std::string>();
                save->callback = callback;
                save->importing = true;
                static const std::regex syntax(R"(^[A-Za-z0-9_-]+/[a-z0-9][a-z0-9.-]*$)");
                require(save->repository.size() <= 256 && std::regex_match(save->repository, syntax), "invalid repository name");
                require(!save->branch.empty() && save->branch.size() <= 256, "specify the configuration editing branch");
                save->name = save->repository.substr(save->repository.find('/') + 1);
                saving = true;
                github.request(save->repository, "GET", "", nullptr, {{"contents", "read"}}, [this, save](const GitHubResult& metadata) {
                    try {
                        require(metadata.status == 200 && metadata.error.empty(), "could not read authorized GitHub repository");
                        require(metadata.body.at("full_name") == save->repository, "GitHub repository identity differs from enrollment");
                        save->id = metadata.body.at("id").get<std::uint64_t>();
                        require(save->id != 0, "invalid GitHub repository ID");
                        list([this, save](const EnrollmentResult& current) {
                            try {
                                require(current.status == 200, "could not read enrolled repository identities");
                                for (const auto& row : current.body.at("repositories")) {
                                    if (row.at("name") == save->name) {
                                        require(row.at("id") == save->id && row.at("repository") == save->repository &&
                                                    row.at("branch") == save->branch,
                                                "repository ID, owner and editing branch must match enrollment");
                                    }
                                }
                                readHead(save);
                            } catch (const std::exception& error) {
                                finish(save, 409, {{"error", error.what()}});
                            }
                        });
                    } catch (const std::exception& error) {
                        finish(save, 502, {{"error", error.what()}, {"github_status", metadata.status}});
                    }
                });
            } catch (const std::exception& error) {
                callback({422, {{"error", error.what()}}});
            }
        }
    }

    void Enrollment::readHead(const std::shared_ptr<Save>& save) {
        github.request(save->repository,
                       "GET",
                       "/git/ref/heads/" + encoded(save->branch),
                       nullptr,
                       {{"contents", "read"}},
                       [this, save](const GitHubResult& head) {
                           try {
                               require(head.status == 200 && head.error.empty(), "could not resolve configuration editing branch");
                               save->head = head.body.at("object").at("sha").get<std::string>();
                               static const std::regex sha(R"(^[a-f0-9]{40}$)");
                               require(std::regex_match(save->head, sha), "GitHub returned an invalid commit identity");
                               readFile(save);
                           } catch (const std::exception& error) {
                               finish(save, 502, {{"error", error.what()}, {"github_status", head.status}, {"github_committed", false}});
                           }
                       });
    }

    void Enrollment::readFile(const std::shared_ptr<Save>& save) {
        github.request(
            save->repository,
            "GET",
            "/contents/ci/packages.json?ref=" + save->head,
            nullptr,
            {{"contents", "read"}},
            [this, save](const GitHubResult& file) {
                try {
                    require(file.error.empty() && (file.status == 200 || file.status == 404), "could not read current GitHub definition");
                    if (file.status == 200) {
                        require(file.body.at("type") == "file", "ci/packages.json must be a regular GitHub file");
                        save->blob = file.body.at("sha").get<std::string>();
                    }
                    // Existing enrollment checks the selected branch revision before replacing its definition.
                    if (save->importing) {
                        require(file.status == 200 && file.body.at("encoding") == "base64", "no committed packages.json to import");
                        auto content = file.body.at("content").get<std::string>();
                        content.erase(std::remove_if(content.begin(),
                                                     content.end(),
                                                     [](unsigned char ch) {
                                                         return std::isspace(ch);
                                                     }),
                                      content.end());
                        require(content.size() <= 1400000 && content.size() % 4 == 0, "invalid definition encoding/size");
                        std::string decoded(content.size() / 4 * 3 + 1, '\0');
                        const int count = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(decoded.data()),
                                                          reinterpret_cast<const unsigned char*>(content.data()),
                                                          static_cast<int>(content.size()));
                        require(count >= 0, "invalid base64 definition");
                        std::size_t size = static_cast<std::size_t>(count);
                        if (!content.empty() && content.back() == '=') {
                            --size;
                        }
                        if (content.size() >= 2 && content[content.size() - 2] == '=') {
                            --size;
                        }
                        decoded.resize(size);
                        save->definition = Definition::parse(nlohmann::json::parse(decoded));
                        list([this, save](const EnrollmentResult& current) {
                            try {
                                require(current.status == 200, "cannot validate effective dependency graph");
                                std::map<std::string, Definition> definitions;
                                for (const auto& row : current.body.at("repositories")) {
                                    definitions.emplace(row.at("name").get<std::string>(), Definition::parse(row.at("definition")));
                                }
                                definitions.insert_or_assign(save->name, save->definition);
                                Definition::validateGraph(definitions);
                                remember(save, {200, {{"commit", {{"sha", save->head}}}, {"content", {{"sha", save->blob}}}}, ""});
                            } catch (const std::exception& error) {
                                finish(save, 422, {{"error", error.what()}, {"github_committed", false}});
                            }
                        });
                    } else if ((!save->expectedRevision.empty() && save->expectedRevision != save->head) ||
                               (save->expectedRevision.empty() && file.status == 200)) {
                        finish(save,
                               409,
                               {{"error", "GitHub branch advanced; refresh/reconcile its definition before saving"},
                                {"github_revision", save->head},
                                {"github_committed", false}});
                    } else {
                        commit(save);
                    }
                } catch (const std::exception& error) {
                    finish(save, 502, {{"error", error.what()}, {"github_status", file.status}, {"github_committed", false}});
                }
            });
    }

    void Enrollment::commit(const std::shared_ptr<Save>& save) {
        nlohmann::json body{{"message", "Configure package definition through core-ci"},
                            {"branch", save->branch},
                            {"content", base64(save->definition.json.dump(2) + "\n")}};
        if (!save->blob.empty()) {
            body["sha"] = save->blob;
        }
        github.request(
            save->repository, "PUT", "/contents/ci/packages.json", body, {{"contents", "write"}}, [this, save](const GitHubResult& result) {
                if ((result.status == 200 || result.status == 201) && result.error.empty()) {
                    remember(save, result);
                } else {
                    finish(
                        save,
                        result.status == 409 ? 409 : 502,
                        {{"error", "GitHub configuration write did not return verified success; refresh before retrying"},
                         {"github_status", result.status},
                         {"github_committed", (result.status >= 400 && result.status < 500) ? nlohmann::json(false) : nlohmann::json{}}});
                }
            });
    }

    void Enrollment::remember(const std::shared_ptr<Save>& save, const GitHubResult& result) {
        try {
            const auto revision = result.body.at("commit").at("sha").get<std::string>();
            const auto blob = result.body.at("content").at("sha").get<std::string>();
            static const std::regex sha(R"(^[a-f0-9]{40}$)");
            require(std::regex_match(revision, sha) && std::regex_match(blob, sha), "GitHub write returned invalid revision identities");
            const auto id = std::to_string(save->id);
            database.write(
                {"INSERT INTO repositories(github_id,name,full_name,editing_ref,configuration_revision) VALUES (" + id + "," +
                     Database::text(save->name) + "," + Database::text(save->repository) + "," + Database::text(save->branch) + "," +
                     Database::text(revision) + ") ON DUPLICATE KEY UPDATE configuration_revision=VALUES(configuration_revision)",
                 "INSERT INTO definitions(repository_id,revision,blob_sha,document) VALUES (" + id + "," + Database::text(revision) + "," +
                     Database::text(blob) + "," + Database::text(save->definition.json.dump()) +
                     ") ON DUPLICATE KEY UPDATE revision=VALUES(revision)",
                 "DELETE FROM namespace_claims WHERE repository_id=" + id,
                 "INSERT INTO namespace_claims(namespace,repository_id,definition_revision) VALUES (" +
                     Database::text(save->definition.json.at("package").get<std::string>()) + "," + id + "," + Database::text(revision) +
                     ")"},
                [this, save, revision](const DatabaseResult& persisted) {
                    if (persisted.error.empty()) {
                        finish(save,
                               200,
                               {{"repository", save->repository}, {"revision", revision}, {"github_committed", true}, {"enrolled", true}});
                    } else {
                        finish(save,
                               503,
                               {{"error", "GitHub committed the definition but database enrollment needs reconciliation"},
                                {"revision", revision},
                                {"github_committed", true},
                                {"enrolled", nullptr}});
                    }
                });
        } catch (const std::exception& error) {
            finish(save, 502, {{"error", error.what()}, {"github_committed", nullptr}});
        }
    }

    void Enrollment::finish(const std::shared_ptr<Save>& save, int status, const nlohmann::json& body) {
        saving = false;
        save->callback({status, body});
    }

} // namespace coreci
