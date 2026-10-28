#include "FeedProof.h"

#include "Graph.h"

#include <regex>
#include <web/http/http_utils.h>

namespace coreci {
    FeedProof::FeedProof(const PipelineConfig& config, GitHub& github, SourceReader& sources)
        : config(config)
        , github(github)
        , sources(sources) {
    }
    void FeedProof::configure() {
        if (!config.feedBaseUrl.empty()) {
            static const std::regex url(R"(^https://([A-Za-z0-9.-]+)(?::([0-9]{1,5}))?(/[^?#\\]*)$)");
            std::smatch parts;
            require(std::regex_match(config.feedBaseUrl, parts, url) && config.feedBaseUrl.ends_with('/') &&
                        config.feedBaseUrl.find("/../") == std::string::npos && config.feedBaseUrl.find("/./") == std::string::npos,
                    "feed base URL must be a fixed HTTPS origin and directory");
            const auto port = parts[2].matched ? std::stoul(parts[2]) : 443;
            require(port > 0 && port <= 65535, "invalid feed HTTPS port");
            http.configure(parts[1], port);
        }
    }
    void FeedProof::verify(const Json& node, const Callback& callback) {
        try {
            const auto& attempt = node.at("attempts").back();
            const auto feed = node.at("source").at("deployment").at("feed");
            require(feed.at("base_url") == config.feedBaseUrl && feed.at("repository") == config.feedRepository &&
                        feed.at("repository_id") == config.feedId && feed.at("branch") == config.feedBranch,
                    "assigned feed differs from configured feed");
            const auto repository = feed.at("repository").get<std::string>();
            const auto path = "receipts/" + node.at("id").get<std::string>() + "/" + attempt.at("revision").dump() + ".json";
            const auto fail = [callback](const std::string& error) {
                callback({503, nullptr, "", "", error});
            };
            github.publicRead(repository, "", [=, this](const GitHubResult& metadata) {
                if (metadata.status != 200 || !metadata.error.empty() || metadata.body.value("id", std::uint64_t{0}) != config.feedId) {
                    fail("public feed repository identity is unavailable");
                } else {
                    github.publicRead(repository,
                                      "/commits?sha=" + httputils::url_encode(feed.at("branch")) + "&path=" + httputils::url_encode(path) +
                                          "&per_page=1",
                                      [=, this](const GitHubResult& commits) {
                                          try {
                                              require(commits.status == 200 && commits.error.empty() && commits.body.is_array() &&
                                                          commits.body.size() == 1,
                                                      "committed publication receipt is unavailable; outcome remains uncertain");
                                              const auto commit = commits.body.at(0).at("sha").get<std::string>();
                                              static const std::regex sha("^[a-f0-9]{40}$");
                                              require(std::regex_match(commit, sha), "invalid feed commit identity");
                                              sources.file(
                                                  repository,
                                                  commit,
                                                  path,
                                                  [=, this](const SourceResult& file) {
                                                      if (!file.error.empty()) {
                                                          fail(file.error);
                                                      } else {
                                                          const auto base = config.feedBaseUrl.substr(config.feedBaseUrl.find('/', 8));
                                                          http.request("GET", base + path, {}, "", [=](const HttpResult& response) {
                                                              auto result = file;
                                                              try {
                                                                  require(response.status == 200 && response.error.empty() &&
                                                                              response.body == file.content,
                                                                          "public HTTPS receipt does not match committed bytes");
                                                                  auto receipt = Json::parse(file.content);
                                                                  require(receipt.at("build_result") == attempt.at("build_result"),
                                                                          "receipt build artifact evidence differs");
                                                                  receipt.erase("publication_verified_utc");
                                                                  receipt["feed_revision"] = commit;
                                                                  auto verified = node;
                                                                  graph::published(verified, attempt.at("id"), receipt);
                                                                  result.source = receipt;
                                                              } catch (const std::exception& error) {
                                                                  result.status = 503;
                                                                  result.error = error.what();
                                                              }
                                                              callback(result);
                                                          });
                                                      }
                                                  },
                                                  true,
                                                  commits.body.at(0).at("commit").at("tree").at("sha"));
                                          } catch (const std::exception& error) {
                                              fail(error.what());
                                          }
                                      });
                }
            });
        } catch (const std::exception& error) {
            callback({503, nullptr, "", "", error.what()});
        }
    }
} // namespace coreci
