#include "FeedProof.h"

#include "HttpsUrl.h"

#include <regex>
#include <stdexcept>
#include <web/http/http_utils.h>

namespace coreci {
    namespace {
        void require(bool condition, const std::string& message) {
            if (!condition) {
                throw std::invalid_argument(message);
            }
        }
    } // namespace

    FeedProof::FeedProof(const PipelineConfig& config, GitHub& github, SourceReader& sources)
        : config(config)
        , github(github)
        , sources(sources)
        , http("feed-http") {
    }

    void FeedProof::configure() {
        http.configure();
    }

    void FeedProof::verify(const Node& node, const Callback& callback) {
        try {
            const auto& attempt = node.attempts.back();
            require(attempt.state == "publishing" && !attempt.publicationRun.is_null(), "feed proof requires assigned publication");
            const auto& feed = node.source.at("deployment").at("feed");
            const HttpsUrl selected(feed.at("base_url").get<std::string>());
            const HttpsUrl configured(config.feedBaseUrl);
            require(selected.origin == configured.origin, "assigned feed origin differs from configured endpoint; preserve publication");
            const auto path = "receipts/" + node.id + "/" + std::to_string(attempt.revision) + ".json";
            const auto repository = feed.at("repository").get<std::string>();
            github.publicRead(repository, "", [this, node, repository, feed, path, callback](const GitHubResult& metadata) {
                if (metadata.status != 200 || !metadata.error.empty() || !metadata.body.is_object() || !metadata.body.contains("id") ||
                    metadata.body.at("id") != feed.at("repository_id")) {
                    callback({nullptr, "public feed repository ID is inaccessible or differs from assignment"});
                } else {
                    github.publicRead(repository,
                                      "/commits?sha=" + httputils::url_encode(feed.at("branch").get<std::string>()) +
                                          "&path=" + httputils::url_encode(path) + "&per_page=1",
                                      [this, node, path, callback](const GitHubResult& result) {
                                          try {
                                              require(result.status == 200 && result.error.empty() && result.body.is_array() &&
                                                          result.body.size() == 1,
                                                      "feed receipt commit is not accessible; publication remains assigned");
                                              const auto revision = result.body.front().at("sha").get<std::string>();
                                              static const std::regex sha("^[a-f0-9]{40}$");
                                              require(std::regex_match(revision, sha), "invalid feed commit identity");
                                              read(node,
                                                   revision,
                                                   result.body.front().at("commit").at("tree").at("sha").get<std::string>(),
                                                   path,
                                                   callback);
                                          } catch (const std::exception& error) {
                                              callback({nullptr, error.what()});
                                          }
                                      });
                }
            });
        } catch (const std::exception& error) {
            callback({nullptr, error.what()});
        }
    }

    void FeedProof::read(
        const Node& node, const std::string& revision, const std::string& rootTree, const std::string& path, const Callback& callback) {
        const auto feed = node.source.at("deployment").at("feed");
        const auto repository = feed.at("repository").get<std::string>();
        sources.file(
            repository,
            revision,
            path,
            [this, node, feed, revision, path, callback](const SourceResult& result) {
                try {
                    require(result.status == 200 && result.error.empty(), "committed feed receipt is unavailable or not regular");
                    const auto content = result.content;
                    auto receipt = nlohmann::json::parse(content);
                    const auto& attempt = node.attempts.back();
                    require(receipt.at("schema") == 1 && receipt.at("execution") == node.id && receipt.at("attempt") == attempt.id &&
                                receipt.at("assignment_sha256") == Graph::digest(attempt.assignment) &&
                                receipt.at("source_commit") == node.source.at("commit") && receipt.at("run") == attempt.publicationRun &&
                                receipt.at("build_result") == attempt.buildResult && receipt.at("feed") == feed &&
                                receipt.at("packages").is_array() && !receipt.at("packages").empty(),
                            "feed receipt does not match assigned execution/source/build/publication");
                    const HttpsUrl url(feed.at("base_url").get<std::string>());
                    const auto root = url.path.ends_with('/') ? url.path : url.path + "/";
                    http.request(url.host,
                                 url.port,
                                 "GET",
                                 root + path,
                                 {{"Cache-Control", "no-cache"}},
                                 "",
                                 [receipt, revision, content, callback](const HttpResult& deployed) mutable {
                                     if (deployed.status != 200 || !deployed.error.empty() || deployed.body != content) {
                                         callback({nullptr, "HTTPS receipt does not match committed feed bytes; preserve publication"});
                                     } else {
                                         receipt["feed_revision"] = revision;
                                         callback({receipt, ""});
                                     }
                                 });
                } catch (const std::exception& error) {
                    callback({nullptr, error.what()});
                }
            },
            true,
            rootTree);
    }
} // namespace coreci
