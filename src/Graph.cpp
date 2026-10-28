#include "Graph.h"

#include "Version.h"

#include <algorithm>
#include <ctime>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
#include <vector>

namespace coreci {
    namespace graph {
        namespace {
            bool newer(const Json& a, const Json& b) {
                const auto order = Version(a.at("tag")).compare(Version(b.at("tag")));
                return order > 0 || (order == 0 && (a.at("tag") != b.at("tag") || a.at("tag_object_sha") != b.at("tag_object_sha") ||
                                                    a.at("commit") != b.at("commit")));
            }

            Json closure(const Json& state, const Json& node) {
                auto result = Json::array();
                std::set<std::string> visited;
                std::function<void(const Json&)> visit = [&](const Json& input) {
                    if (visited.insert(input.at("id")).second) {
                        result.push_back(input.at("source"));
                        for (const auto& id : input.at("prerequisites")) {
                            visit(state.at("executions").at(id.get<std::string>()));
                        }
                    }
                };
                visit(node);
                return result;
            }

            Json waiting(Json& state, const std::string& id) {
                const auto revision = state.at("next_revision").get<std::uint64_t>();
                require(revision > 0 && revision < UINT64_MAX, "rebuild revision exhausted");
                state["next_revision"] = revision + 1;
                return {{"id", id + ":" + std::to_string(revision)},
                        {"revision", revision},
                        {"state", "waiting"},
                        {"assignment", nullptr},
                        {"build_run", nullptr},
                        {"build_result", nullptr},
                        {"publication_run", nullptr},
                        {"receipt", nullptr},
                        {"evidence", Json::object()},
                        {"failure", ""}};
            }

            Json& current(Json& node, const std::string& id) {
                auto& attempt = node.at("attempts").back();
                require(attempt.at("id") == id, "result does not belong to the current attempt");
                return attempt;
            }

            Json assignment(const Json& state, const Json& node) {
                const auto& attempt = node.at("attempts").back();
                auto dependencies = Json::array();
                if (node.at("attempts").size() > 1) {
                    dependencies = node.at("attempts").front().at("assignment").at("dependencies");
                } else {
                    for (const auto& id : node.at("prerequisites")) {
                        const auto& other = state.at("executions").at(id.get<std::string>()).at("attempts").back();
                        auto receipt = other.at("receipt");
                        receipt.erase("indexes");
                        dependencies.push_back({{"execution", id}, {"attempt", other.at("id")}, {"receipt", receipt}});
                    }
                }
                const auto& source = node.at("source");
                const auto& target = node.at("target_row");
                const auto revision = std::to_string(attempt.at("revision").get<std::uint64_t>());
                const auto label = source.at("repository").get<std::string>() + " " + source.at("tag").get<std::string>() + " | " +
                                   target.at("distribution").get<std::string>() + " " + target.at("suite").get<std::string>() + " | " +
                                   target.at("arch").get<std::string>() + " | " + target.at("build").at("format").get<std::string>() +
                                   " | r" + revision;
                Json result{{"schema", 1},
                            {"label", label},
                            {"execution", node.at("id")},
                            {"attempt", attempt.at("id")},
                            {"revision", attempt.at("revision")},
                            {"artifact_name", "core-ci-build-" + node.at("id").get<std::string>() + "-r" + revision},
                            {"source", source},
                            {"target", target},
                            {"dependencies", dependencies}};
                auto previous = Json::array();
                for (const auto& earlier : node.at("attempts")) {
                    if (earlier.at("id") != attempt.at("id") && !earlier.at("build_run").is_null()) {
                        const auto& assigned = earlier.at("assignment");
                        previous.push_back({{"run", earlier.at("build_run")},
                                            {"attempt", earlier.at("id")},
                                            {"assignment_sha256", digest(assigned)},
                                            {"label", assigned.at("label")},
                                            {"artifact_name", assigned.at("artifact_name").get<std::string>() + "-environment"}});
                    }
                }
                if (!previous.empty()) {
                    result["environment_from"] = previous;
                }
                return result;
            }
        } // namespace

        Json empty() {
            return {{"generation", 0}, {"next_revision", 1}, {"repositories", Json::object()}, {"executions", Json::object()}};
        }

        Json coordinates(const Json& target) {
            Json result;
            for (const auto* key : {"family", "distribution", "suite", "arch"}) {
                result[key] = target.at(key);
            }
            result["format"] = target.at("build").at("format");
            const auto* platform = target.at("family") == "native" ? "platform" : "target";
            result[platform] = target.at("build").at(platform);
            return result;
        }

        std::vector<std::string> validateSources(const Json& sources, const Json& executions) {
            std::map<std::pair<std::string, std::string>, std::string> namespaces;
            const auto reserve = [&](const std::string& repository, const std::string& package, const Json& target) {
                auto coordinate = coordinates(target);
                for (const auto* key : {"family", "platform", "target"}) {
                    coordinate.erase(key);
                }
                const auto space = std::make_pair(coordinate.dump(), package);
                for (const auto& [previous, owner] : namespaces) {
                    require(owner == repository || previous.first != space.first ||
                                (previous.second != package && !previous.second.starts_with(package + "-") &&
                                 !package.starts_with(previous.second + "-")),
                            "package namespace overlaps: " + repository);
                }
                namespaces[space] = repository;
            };
            std::map<std::string, int> colors;
            std::vector<std::string> order;
            std::function<void(const std::string&)> visit = [&](const std::string& name) {
                require(sources.contains(name), "required prerequisite source missing: " + name);
                require(colors[name] != 1, "dependency cycle at " + name);
                if (colors[name] == 0) {
                    colors[name] = 1;
                    const auto& source = sources.at(name);
                    for (const auto& dependency : source.at("definition").at("depends_on")) {
                        visit(dependency.get<std::string>());
                        for (const auto& target : source.at("definition").at("matrix")) {
                            const auto& candidates = sources.at(dependency.get<std::string>()).at("definition").at("matrix");
                            require(std::any_of(candidates.begin(),
                                                candidates.end(),
                                                [&](const Json& other) {
                                                    return coordinates(other) == coordinates(target);
                                                }),
                                    "required prerequisite target missing: " + dependency.get<std::string>());
                        }
                    }
                    for (const auto& target : source.at("definition").at("matrix")) {
                        reserve(source.at("repository"), source.at("definition").at("package"), target);
                    }
                    colors[name] = 2;
                    order.push_back(name);
                }
            };
            for (const auto& [name, source] : sources.items()) {
                visit(name);
            }
            for (const auto& [id, node] : executions.items()) {
                reserve(node.at("source").at("repository"), node.at("source").at("package"), node.at("target_row"));
            }
            return order;
        }

        Json select(Json& state, const std::string& root, const Json& sources, bool explicitRequest, std::uint64_t since) {
            require(sources.contains(root), "requested repository has no selected source");
            for (const auto& execution : state.at("executions")) {
                const auto& source = execution.at("source");
                const auto name = source.at("name").get<std::string>();
                if (execution.at("attempts").front().at("revision") >= since && sources.contains(name)) {
                    require(
                        !newer(source, sources.at(name)) ||
                            (explicitRequest && name == root && Version(source.at("tag")).compare(Version(sources.at(name).at("tag"))) > 0),
                        "newer source selection committed during source selection: " + name);
                }
            }
            const auto order = validateSources(sources, state.at("executions"));
            std::set<std::string> affected{root};
            for (const auto& name : order) {
                for (const auto& dependency : sources.at(name).at("definition").at("depends_on")) {
                    if (affected.contains(dependency.get<std::string>())) {
                        affected.insert(name);
                    }
                }
            }
            auto selected = Json::object();
            auto created = Json::array();
            bool acceptedRoot = false;
            for (const auto& name : order) {
                if (affected.contains(name) && (name == root || acceptedRoot)) {
                    auto source = sources.at(name);
                    const auto definition = source.at("definition");
                    source.erase("definition");
                    for (const auto* key : {"title", "package", "depends_on", "build"}) {
                        source[key] = definition.at(key);
                    }
                    for (const auto& target : definition.at("matrix")) {
                        const auto coordinate = coordinates(target).dump();
                        auto prerequisites = Json::array();
                        for (const auto& dependency : definition.at("depends_on")) {
                            const auto other = dependency.get<std::string>();
                            std::string prerequisite;
                            if (affected.contains(other)) {
                                prerequisite = selected.at(other + ":" + coordinate).get<std::string>();
                            } else {
                                std::uint64_t newest = 0;
                                for (const auto& [id, candidate] : state.at("executions").items()) {
                                    const auto& original = candidate.at("source");
                                    if (original.at("name") == other && original.at("commit") == sources.at(other).at("commit") &&
                                        original.at("tag") == sources.at(other).at("tag") &&
                                        original.at("definition_blob_sha") == sources.at(other).at("definition_blob_sha") &&
                                        original.at("deployment") == sources.at(other).at("deployment") &&
                                        coordinates(candidate.at("target_row")) == coordinates(target) &&
                                        candidate.at("superseded_by") == "" &&
                                        candidate.at("attempts").back().at("revision").get<std::uint64_t>() > newest) {
                                        prerequisite = id;
                                        newest = candidate.at("attempts").back().at("revision").get<std::uint64_t>();
                                    }
                                }
                            }
                            require(!prerequisite.empty(), "required prerequisite execution missing: " + other);
                            prerequisites.push_back(prerequisite);
                        }
                        Json identity{{"source", source}, {"target", target}, {"prerequisites", prerequisites}};
                        auto id = digest(identity);
                        if (explicitRequest && name == root && state.at("executions").contains(id) &&
                            state.at("executions").at(id).at("superseded_by") != "") {
                            identity["reactivated"] = state.at("next_revision");
                            id = digest(identity);
                        }
                        if (!state.at("executions").contains(id)) {
                            Json node{{"id", id},
                                      {"source", source},
                                      {"target_row", target},
                                      {"prerequisites", prerequisites},
                                      {"automatic", name != root || !explicitRequest},
                                      {"superseded_by", ""},
                                      {"attempts", {waiting(state, id)}}};
                            const auto inputs = closure(state, node);
                            for (auto& [previousId, previous] : state.at("executions").items()) {
                                if (previous.at("automatic") && previous.at("superseded_by") == "" &&
                                    previous.at("attempts").back().at("state") == "waiting" &&
                                    previous.at("source").at("repository_id") == source.at("repository_id") &&
                                    coordinates(previous.at("target_row")) == coordinates(target)) {
                                    bool older = true;
                                    for (const auto& prior : closure(state, previous)) {
                                        for (const auto& next : inputs) {
                                            older = older && (prior.at("name") != next.at("name") || !newer(prior, next));
                                        }
                                    }
                                    if (older) {
                                        previous["superseded_by"] = id;
                                    }
                                }
                            }
                            acceptedRoot = acceptedRoot || name == root;
                            state["executions"][id] = node;
                            created.push_back(id);
                        } else if (explicitRequest && name == root &&
                                   state.at("executions").at(id).at("attempts").back().at("state") == "waiting" &&
                                   state.at("executions").at(id).at("superseded_by") == "") {
                            state["executions"][id]["automatic"] = false;
                        }
                        selected[name + ":" + coordinate] = id;
                    }
                }
            }
            return created;
        }

        Json blockers(const Json& state, const Json& node) {
            auto result = Json::array();
            if (node.at("superseded_by") != "") {
                result.push_back("superseded by " + node.at("superseded_by").get<std::string>());
            }
            for (const auto& id : node.at("prerequisites")) {
                if (!state.at("executions").contains(id.get<std::string>()) ||
                    state.at("executions").at(id.get<std::string>()).at("attempts").back().at("state") != "published") {
                    result.push_back("waiting for published prerequisite " + id.get<std::string>());
                }
            }
            const auto& attempt = node.at("attempts").back();
            const bool publish = attempt.at("state") == "building" && !attempt.at("build_result").is_null();
            if (result.empty() && (attempt.at("state") == "waiting" || publish)) {
                auto candidate = node;
                if (!publish) {
                    candidate["attempts"].back()["assignment"] = assignment(state, node);
                }
                const auto payload = inputs(candidate, publish);
                if (payload.at("assignment").get_ref<const std::string&>().size() > 60000 || payload.dump().size() > 65535) {
                    result.push_back("GitHub dispatch payload exceeds supported input bounds");
                }
            }
            return result;
        }

        Json inputs(const Json& node, bool publication) {
            const auto& attempt = node.at("attempts").back();
            const auto& assigned = attempt.at("assignment");
            Json result{{"execution", node.at("id")},
                        {"attempt", attempt.at("id")},
                        {"assignment_sha256", digest(assigned)},
                        {"assignment", assigned.dump()}};
            if (publication) {
                result["build_result"] = attempt.at("build_result").dump();
            }
            return result;
        }

        Json advance(Json& state) {
            std::vector<std::string> order;
            for (const auto& [id, node] : state.at("executions").items()) {
                order.push_back(id);
            }
            std::sort(order.begin(), order.end(), [&](const auto& left, const auto& right) {
                return state.at("executions").at(left).at("attempts").back().at("revision") <
                       state.at("executions").at(right).at("attempts").back().at("revision");
            });
            auto dispatches = Json::array();
            for (const auto& id : order) {
                auto& node = state.at("executions").at(id);
                auto& attempt = node.at("attempts").back();
                const bool build = attempt.at("state") == "waiting";
                const bool publish = attempt.at("state") == "building" && !attempt.at("build_result").is_null();
                if ((build || publish) && blockers(state, node).empty()) {
                    auto candidate = node;
                    auto& next = candidate.at("attempts").back();
                    if (build) {
                        next["assignment"] = assignment(state, node);
                    }
                    next["state"] = publish ? "publishing" : "building";
                    dispatches.push_back({{"execution", id}, {"attempt", next.at("id")}, {"publication", publish}});
                    node = std::move(candidate);
                }
            }
            return dispatches;
        }

        bool retirable(const Json& node) {
            const auto& attempt = node.at("attempts").back();
            return attempt.at("state") == "publishing" && attempt.at("publication_run").is_null() &&
                   attempt.value("dispatch", Json::object()).value("publication", Json()).is_null() &&
                   attempt.at("evidence").value("publication", Json::object()).empty();
        }
        void retire(Json& state, const std::string& id, const std::string& attemptId) {
            auto& node = state.at("executions").at(id);
            require(node.at("attempts").back().at("id") == attemptId && retirable(node), "publication cannot be retired");
            failed(node, attemptId, "operator abandoned unconfirmed dispatch; remote outcome unknown");
        }

        void retry(Json& state, const std::string& id) {
            auto& node = state.at("executions").at(id);
            require(node.at("attempts").back().at("state") == "failed" && node.at("superseded_by") == "", "execution cannot be retried");
            node["attempts"].push_back(waiting(state, id));
        }

        void observe(Json& node, const std::string& attemptId, bool publication, const Json& run, const Json& facts) {
            auto& attempts = node.at("attempts");
            const auto found = std::find_if(attempts.begin(), attempts.end(), [&](const auto& attempt) {
                return attempt.at("id") == attemptId;
            });
            require(found != attempts.end(), "report does not belong to an assigned attempt");
            auto& attempt = *found;
            auto& known = attempt.at(publication ? "publication_run" : "build_run");
            require(!attempt.at("assignment").is_null() && run.at("run_attempt") == 1, "report does not match assigned attempt one");
            require(known.is_null() || known == run, "conflicting assigned run");
            known = run;
            auto& evidence = attempt["evidence"][publication ? "publication" : "build"];
            const std::map<std::string, int> rank{
                {"requested", 0}, {"waiting", 0}, {"pending", 0}, {"queued", 0}, {"in_progress", 1}, {"completed", 2}};
            const auto previousPhase = evidence.contains("job_status") ? rank.at(evidence.at("job_status").get<std::string>()) : 0;
            for (const auto& [key, value] : facts.items()) {
                if (!value.is_null()) {
                    if (key == "status" || key == "job_status") {
                        if (!evidence.contains(key) || rank.at(value.get<std::string>()) >= rank.at(evidence.at(key).get<std::string>())) {
                            evidence[key] = value;
                        }
                    } else if (key != "started_at" || rank.at(facts.value("job_status", "queued")) > 0) {
                        require(!evidence.contains(key) || evidence.at(key) == value || (key == "started_at" && previousPhase == 0),
                                "conflicting run/job evidence: " + key);
                        evidence[key] = value;
                    }
                }
            }
        }

        void built(Json& node, const std::string& attemptId, const Json& result) {
            auto& attempt = current(node, attemptId);
            require(attempt.at("state") == "building" && !attempt.at("build_run").is_null() && result.at("artifacts").is_array() &&
                        !result.at("artifacts").empty() && result.at("run") == attempt.at("build_run") &&
                        result.at("assignment_sha256") == digest(attempt.at("assignment")),
                    "build result differs from assignment");
            require(attempt.at("build_result").is_null() || attempt.at("build_result") == result, "conflicting build artifact result");
            attempt["build_result"] = result;
        }

        void published(Json& node, const std::string& attemptId, const Json& receipt) {
            auto& attempt = current(node, attemptId);
            require((attempt.at("state") == "publishing" || attempt.at("state") == "published") &&
                        receipt.at("execution") == node.at("id") && receipt.at("attempt") == attemptId &&
                        receipt.at("assignment_sha256") == digest(attempt.at("assignment")) && !attempt.at("publication_run").is_null() &&
                        receipt.at("run") == attempt.at("publication_run") &&
                        receipt.at("source_commit") == node.at("source").at("commit") &&
                        receipt.at("feed") == node.at("source").at("deployment").at("feed") && receipt.at("packages").is_array() &&
                        !receipt.at("packages").empty() && receipt.at("environment").is_object() && !receipt.at("environment").empty() &&
                        receipt.at("feed_revision").is_string(),
                    "receipt differs from assignment");
            auto verified = receipt;
            verified.erase("publication_verified_utc");
            auto previous = attempt.at("receipt");
            if (previous.is_object()) {
                previous.erase("publication_verified_utc");
            }
            require(previous.is_null() || previous == verified, "conflicting publication receipt");
            if (attempt.at("state") != "published") {
                const auto now = std::time(nullptr);
                std::ostringstream utc;
                const auto* time = std::gmtime(&now);
                require(time != nullptr, "UTC clock unavailable");
                utc << std::put_time(time, "%Y-%m-%dT%H:%M:%SZ");
                verified["publication_verified_utc"] = utc.str();
                attempt["receipt"] = verified;
                attempt["state"] = "published";
            }
        }

        void failed(Json& node, const std::string& attemptId, const std::string& reason) {
            auto& attempt = current(node, attemptId);
            require(!reason.empty() && (attempt.at("state") == "building" || attempt.at("state") == "publishing"),
                    "failure is not for assigned work");
            attempt["failure"] = reason;
            attempt["state"] = "failed";
        }
    } // namespace graph
} // namespace coreci
