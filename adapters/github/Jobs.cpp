#include "Jobs.h"

#include <nlohmann/json.hpp>
namespace coreci::github {
    namespace {
        using Json = nlohmann::json;
        std::string field(const Json& value, const char* name) {
            return value.contains(name) && !value.at(name).is_null() ? value.at(name).get<std::string>() : "";
        }
        struct Lookup : std::enable_shared_from_this<Lookup> {
            JobRequest request;
            Url base;
            Fetch fetch;
            std::function<void(std::optional<RunJob>)> done;
            std::optional<Json> selected;
            std::set<std::string> visited;
            std::string prefix;
            void page(const std::string& path, bool jobs = false) {
                visited.insert(path);
                const auto self = shared_from_this();
                fetch(path, false, [self, jobs](int status, std::string body, std::string link) {
                    try {
                        if (status != 200) {
                            throw std::invalid_argument("run unavailable");
                        }
                        const auto data = Json::parse(body);
                        if (jobs) {
                            for (const auto& job : data.at("jobs")) {
                                if (job.at("name") == self->request.mainJob) {
                                    const auto& run = *self->selected;
                                    self->done(RunJob{run.at("id").get<std::uint64_t>(),
                                                      field(run, "html_url"),
                                                      field(run, "status"),
                                                      field(run, "conclusion"),
                                                      Job{field(job, "html_url"),
                                                          field(job, "started_at"),
                                                          field(job, "completed_at"),
                                                          field(job, "status"),
                                                          field(job, "conclusion")}});
                                    self->done = {};
                                }
                            }
                        } else if (data.contains("workflow_runs")) {
                            for (const auto& run : data.at("workflow_runs")) {
                                if (run.at("display_title") == self->request.buildId &&
                                    run.at("path") == ".github/workflows/" + self->request.workflow.workflowFile &&
                                    (!self->selected || run.at("id") > self->selected->at("id"))) {
                                    self->selected = run;
                                }
                            }
                        } else {
                            self->selected = data;
                        }
                        if (self->done) {
                            const auto next = nextLink(link, self->base);
                            if (std::holds_alternative<Error>(next)) {
                                throw std::invalid_argument("invalid pagination");
                            }
                            const auto& url = std::get<std::optional<std::string>>(next);
                            if (url) {
                                const auto target = httpsUrl(*url);
                                if (!target || self->visited.contains(target->path)) {
                                    throw std::invalid_argument("invalid pagination");
                                }
                                self->page(target->path, jobs);
                            } else if (!jobs && self->selected) {
                                const auto& run = *self->selected;
                                self->page(self->prefix + "/" + std::to_string(run.at("id").get<std::uint64_t>()) + "/attempts/" +
                                               std::to_string(run.at("run_attempt").get<unsigned>()) + "/jobs?per_page=100",
                                           true);
                            } else {
                                self->done({});
                                self->done = {};
                            }
                        }
                    } catch (const std::exception&) {
                        if (self->done) {
                            self->done({});
                            self->done = {};
                        }
                    }
                });
            }
        };
    } // namespace
    std::string JobRequest::key() const {
        return workflow.owner + "/" + workflow.repo + "/" + workflow.workflowFile + "/" + buildId;
    }
    void loadJob(const JobRequest& request, const Url& base, Fetch fetch, std::function<void(std::optional<RunJob>)> done) {
        auto lookup = std::make_shared<Lookup>();
        lookup->request = request;
        lookup->base = base;
        lookup->fetch = std::move(fetch);
        lookup->done = std::move(done);
        lookup->prefix = base.path + "/repos/" + escape(request.workflow.owner) + "/" + escape(request.workflow.repo) + "/actions/runs";
        lookup->page(lookup->prefix + (request.runId ? std::string("/").append(std::to_string(*request.runId)) : "?per_page=100"));
    }
} // namespace coreci::github
