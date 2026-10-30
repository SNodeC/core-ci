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
            const std::string retiredUncertain = "operator abandoned unconfirmed dispatch; remote outcome unknown";
            bool newer(const Json& a, const Json& b) {
                const auto order = Version(a.at("tag")).compare(Version(b.at("tag")));
                return order > 0 || (order == 0 && (a.at("tag") != b.at("tag") || a.at("tag_object_sha") != b.at("tag_object_sha") ||
                                                    a.at("commit") != b.at("commit")));
            }

            Json feedCoordinates(const Json& target) {
                auto result = coordinates(target);
                for (const auto* key : {"family", "platform", "target"}) {
                    result.erase(key);
                }
                result["distribution"] = target.at("family") == "openwrt" ? Json("openwrt") : target.at("distribution");
                return result;
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

            Json dependencies(const Json& state, const Json& node) {
                const auto& assigned = node.at("attempts").front().at("assignment");
                auto result = assigned.is_null() ? Json::array() : assigned.at("dependencies");
                if (assigned.is_null()) {
                    for (const auto& id : node.at("prerequisites")) {
                        const auto& other = state.at("executions").at(id.get<std::string>()).at("attempts").back();
                        auto receipt = other.at("receipt");
                        if (receipt.is_object()) {
                            receipt.erase("indexes");
                        }
                        result.push_back({{"execution", id}, {"attempt", other.at("id")}, {"receipt", receipt}});
                    }
                }
                return result;
            }

            Json assignment(const Json& state, const Json& node) {
                const auto& attempt = node.at("attempts").back();
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
                            {"dependencies", dependencies(state, node)}};
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

        bool eligible(const Json& state, const Json& node, bool pending) {
            const auto& current = node.at("attempts").back();
            auto result = node.at("superseded_by") == "" && !current.contains("expired_by");
            for (const auto& dependency : dependencies(state, node)) {
                const auto& other = state.at("executions").at(dependency.at("execution").get<std::string>());
                const auto& attempts = other.at("attempts");
                const bool available = std::any_of(attempts.begin(), attempts.end(), [&](const auto& attempt) {
                    return attempt.at("id") == dependency.at("attempt") && attempt.at("state") == "published" &&
                           !attempt.contains("expired_by");
                });
                result = result && (available || (pending && attempts.back().at("id") == dependency.at("attempt") &&
                                                  attempts.back().at("state") != "failed" && eligible(state, other, true)));
            }
            return result;
        }

        std::vector<std::string> workIds(const Json& state) {
            std::vector<std::string> ids;
            for (const auto* collection : {"executions", "retention"}) {
                if (state.contains(collection)) {
                    for (const auto& [id, node] : state.at(collection).items()) {
                        ids.push_back(id);
                    }
                }
            }
            return ids;
        }

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
                const auto space = std::make_pair(feedCoordinates(target).dump(), package);
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
                                        candidate.at("superseded_by") == "" && !candidate.at("attempts").back().contains("expired_by") &&
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
                        require(!state.at("executions").contains(id) ||
                                    !state.at("executions").at(id).at("attempts").back().contains("expired_by"),
                                "requested historical input expired by retention");
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
            const auto& attempt = node.at("attempts").back();
            if (result.empty() && !eligible(state, node, false)) {
                result.push_back(attempt.contains("expired_by") ? "inputs expired by retention"
                                                                : "exact prerequisite inputs expired or not published");
            }
            const bool publish = attempt.at("state") == "building" && !attempt.at("build_result").is_null();
            if (result.empty() && (attempt.at("state") == "waiting" || publish)) {
                auto candidate = node;
                candidate["attempts"].back()["assignment"] = publish ? attempt.at("assignment") : assignment(state, node);
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
            if (publication && !assigned.contains("kind")) {
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
                    if (build) {
                        attempt["assignment"] = assignment(state, node);
                    }
                    attempt["state"] = publish ? "publishing" : "building";
                    dispatches.push_back({{"execution", id}, {"attempt", attempt.at("id")}, {"publication", publish}});
                }
            }
            return dispatches;
        }

        Json retain(Json& state, const Json& deployment) {
            std::map<std::string, Json*> publications;
            std::map<std::string, const Json*> owners;
            std::map<std::string, std::vector<std::string>> groups;
            std::set<std::string> inputs, packages, claimed, keep, busy;
            for (const auto& operation : state.value("retention", Json::object())) {
                const auto& attempt = operation.at("attempts").back();
                if (operation.at("source").at("deployment").at("feed") == deployment.at("feed")) {
                    if (attempt.at("state") != "published") {
                        busy.insert(attempt.at("assignment").at("scope"));
                    }
                    for (const auto& object : attempt.at("assignment").at("objects")) {
                        claimed.insert(object.at("path"));
                    }
                }
            }
            for (auto& node : state.at("executions")) {
                const auto scope = Json{node.at("source").at("repository_id"), feedCoordinates(node.at("target_row"))}.dump();
                for (auto& attempt : node.at("attempts")) {
                    if (attempt.at("failure") == retiredUncertain ||
                        (attempt.at("id") == node.at("attempts").back().at("id") &&
                         (attempt.at("state") == "building" || attempt.at("state") == "publishing" ||
                          (attempt.at("state") == "waiting" && eligible(state, node, true))))) {
                        for (const auto& dependency : dependencies(state, node)) {
                            inputs.insert(dependency.at("attempt"));
                        }
                    }
                    if (attempt.at("state") == "published" && attempt.at("receipt").at("feed") == deployment.at("feed")) {
                        const auto reference = attempt.at("id").get<std::string>();
                        publications[reference] = &attempt;
                        owners[reference] = &node;
                        groups[scope].push_back(reference);
                    }
                }
            }
            for (auto& [scope, order] : groups) {
                std::sort(order.begin(), order.end(), [&](const auto& left, const auto& right) {
                    const auto comparison =
                        Version(owners.at(left)->at("source").at("tag")).compare(Version(owners.at(right)->at("source").at("tag")));
                    return comparison == 0 ? publications.at(left)->at("revision") > publications.at(right)->at("revision")
                                           : comparison > 0;
                });
                std::string previous;
                unsigned int versions = 0;
                for (const auto& reference : order) {
                    const auto tag = owners.at(reference)->at("source").at("tag").get<std::string>();
                    if (previous.empty() || Version(previous).compare(Version(tag)) != 0) {
                        if (++versions <= 3 && !publications.at(reference)->contains("expired_by")) {
                            inputs.insert(reference);
                        }
                        previous = tag;
                    }
                }
            }
            std::function<void(const std::string&)> visit = [&](const std::string& reference) {
                if (publications.contains(reference) && packages.insert(reference).second) {
                    for (const auto& dependency : publications.at(reference)->at("assignment").at("dependencies")) {
                        visit(dependency.at("attempt"));
                    }
                }
            };
            for (const auto& reference : inputs) {
                visit(reference);
            }
            for (const auto& [reference, attempt] : publications) {
                for (const auto& object : attempt->at("receipt").at("packages")) {
                    if (packages.contains(reference)) {
                        keep.insert(object.at("path"));
                    }
                }
                const auto& development = attempt->at("receipt").at("development_export");
                if (inputs.contains(reference) && !development.is_null()) {
                    keep.insert(development.at("path"));
                }
            }
            auto commands = Json::array();
            for (const auto& [scope, order] : groups) {
                if (!busy.contains(scope)) {
                    const auto& basis = *owners.at(order.front());
                    const auto id = digest(Json{scope, state.at("next_revision")});
                    Json request{{"schema", 1},
                                 {"kind", "retention"},
                                 {"scope", scope},
                                 {"source", {{"deployment", deployment}}},
                                 {"target", basis.at("target_row")},
                                 {"environment", publications.at(order.front())->at("receipt").at("environment")},
                                 {"objects", Json::array()},
                                 {"execution", id},
                                 {"attempt", id + ":" + state.at("next_revision").dump()},
                                 {"revision", state.at("next_revision")},
                                 {"label", "retention " + id},
                                 {"artifact_name", "core-ci-retention-" + id}};
                    auto expires = Json::array();
                    for (auto iterator = order.rbegin(); iterator != order.rend(); ++iterator) {
                        const auto& reference = *iterator;
                        const auto& attempt = *publications.at(reference);
                        if (!inputs.contains(reference)) {
                            const auto offset = request.at("objects").size();
                            bool fits = true;
                            auto objects = attempt.at("receipt").at("packages");
                            const auto& development = attempt.at("receipt").at("development_export");
                            if (!development.is_null()) {
                                objects.push_back(development);
                            }
                            if (std::none_of(attempt.at("receipt").at("packages").begin(),
                                             attempt.at("receipt").at("packages").end(),
                                             [&](const auto& object) {
                                                 return keep.contains(object.at("path"));
                                             })) {
                                auto receipt = attempt.at("receipt");
                                receipt.erase("feed_revision");
                                receipt.erase("publication_verified_utc");
                                const auto bytes = receipt.dump() + "\n";
                                const auto path = "receipts/" + owners.at(reference)->at("id").get<std::string>() + "/" +
                                                  attempt.at("revision").dump() + ".json";
                                objects.push_back({{"path", path}, {"size", bytes.size()}, {"sha256", hash(bytes, EVP_sha256())}});
                            }
                            for (auto object = objects.begin(); object != objects.end() && fits; ++object) {
                                const auto path = object->at("path").get<std::string>();
                                if (!keep.contains(path) && claimed.insert(path).second) {
                                    request["objects"].push_back(
                                        {{"path", path}, {"size", object->at("size")}, {"sha256", object->at("sha256")}});
                                    const Json node{{"id", id}, {"attempts", {{{"id", request.at("attempt")}, {"assignment", request}}}}};
                                    fits = request.dump().size() <= 60000 && graph::inputs(node, true).dump().size() <= 65535;
                                    if (!fits) {
                                        request["objects"].erase(request["objects"].end() - 1);
                                        claimed.erase(path);
                                    }
                                }
                            }
                            require(fits || !request.at("objects").empty(), "indivisible retention object exceeds dispatch bounds");
                            if ((fits || request.at("objects").size() > offset) && !attempt.contains("expired_by")) {
                                expires.push_back(reference);
                            }
                        }
                    }
                    for (const auto& reference : expires) {
                        (*publications.at(reference))["expired_by"] = request.at("objects").empty() ? Json("retention policy") : Json(id);
                    }
                    if (!request.at("objects").empty()) {
                        auto attempt = waiting(state, id);
                        attempt["assignment"] = request;
                        attempt["state"] = "publishing";
                        state["retention"][id] = {{"id", id}, {"source", request.at("source")}, {"attempts", {attempt}}};
                        commands.push_back({{"execution", id}, {"attempt", attempt.at("id")}, {"publication", true}});
                    }
                }
            }
            return commands;
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
            failed(node, attemptId, retiredUncertain);
        }

        Json retry(Json& state, const std::string& id) {
            auto& node = work(state, id);
            require(node.at("attempts").back().at("state") == "failed", "work cannot be retried");
            auto commands = Json::array();
            auto attempt = waiting(state, id);
            if (state.at("executions").contains(id)) {
                require(node.at("superseded_by") == "", "execution cannot be retried");
                require(eligible(state, node, false), "historical retry input expired or unavailable");
            } else {
                attempt["assignment"] = node.at("attempts").back().at("assignment");
                attempt["assignment"]["attempt"] = attempt.at("id");
                attempt["assignment"]["revision"] = attempt.at("revision");
                attempt["assignment"]["label"] = "retention " + id + " | r" + attempt.at("revision").dump();
                attempt["state"] = "publishing";
                const Json candidate{{"id", id}, {"attempts", {attempt}}};
                require(attempt.at("assignment").dump().size() <= 60000 && inputs(candidate, true).dump().size() <= 65535,
                        "GitHub dispatch payload exceeds supported input bounds");
                commands.push_back({{"execution", id}, {"attempt", attempt.at("id")}, {"publication", true}});
            }
            node["attempts"].push_back(attempt);
            return commands;
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
