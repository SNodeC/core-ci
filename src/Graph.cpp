#include "Graph.h"

#include "Version.h"

#include <algorithm>
#include <ctime>
#include <functional>
#include <iomanip>
#include <limits>
#include <openssl/evp.h>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace coreci {

    namespace {
        void require(bool condition, const std::string& message) {
            if (!condition) {
                throw std::invalid_argument(message);
            }
        }

        nlohmann::json mergeEvidence(nlohmann::json previous, const nlohmann::json& incoming) {
            require(previous.is_object() && incoming.is_object(), "execution evidence must be an object");
            const std::map<std::string, unsigned int> progress{
                {"requested", 0}, {"waiting", 0}, {"pending", 0}, {"queued", 0}, {"in_progress", 1}, {"completed", 2}};
            for (const auto& [phase, facts] : incoming.items()) {
                require((phase == "build" || phase == "publication") && facts.is_object(), "unknown execution evidence phase");
                auto& known = previous[phase];
                for (const auto& [key, value] : facts.items()) {
                    if (!value.is_null()) {
                        if (key == "status" || key == "job_status") {
                            const auto rank = progress.at(value.get<std::string>());
                            if (!known.contains(key) || rank >= progress.at(known.at(key).get<std::string>())) {
                                known[key] = value;
                            }
                        } else {
                            require(!known.contains(key) || known.at(key) == value, "conflicting execution evidence: " + key);
                            known[key] = value;
                        }
                    }
                }
            }
            return previous;
        }

        nlohmann::json identity(const Node& node) {
            return {{"source", node.source},
                    {"target", node.target},
                    {"prerequisites", node.prerequisites},
                    {"reactivated", node.cause.value("reactivated", std::string{})}};
        }

        std::string publicationFeed(const Node& node) {
            const auto& feed = node.source.at("deployment").at("feed");
            const nlohmann::json identity{feed.at("repository_id"), feed.at("branch")};
            return identity.dump();
        }

        Attempt waiting(const std::string& id, std::uint64_t revision) {
            require(revision > 0 && revision < std::numeric_limits<std::uint64_t>::max(), "package rebuild revision is exhausted");
            Attempt result;
            result.id = id + ":" + std::to_string(revision);
            result.revision = revision;
            return result;
        }

        bool sameSource(const nlohmann::json& left, const nlohmann::json& right) {
            bool same = true;
            for (const auto* key : {"repository_id", "repository", "tag", "commit", "definition_revision"}) {
                same = same && left.at(key) == right.at(key);
            }
            return same;
        }

        void validateSource(const std::string& name, const nlohmann::json& source) {
            static const std::regex sha(R"(^[a-f0-9]{40}$)");
            require(source.at("name") == name && source.at("repository_id").get<std::uint64_t>() != 0,
                    "invalid selected repository identity");
            require(std::regex_match(source.at("commit").get<std::string>(), sha) &&
                        std::regex_match(source.at("definition_revision").get<std::string>(), sha) &&
                        std::regex_match(source.at("definition_blob_sha").get<std::string>(), sha),
                    "source must select immutable commits");
            const Version version(source.at("tag").get<std::string>());
            Definition::parse(source.at("definition"));
            require(source.at("deployment").is_object() && !source.at("deployment").empty(),
                    "trusted deployment must be pinned at selection");
        }

        nlohmann::json executionSource(const nlohmann::json& source) {
            auto result = source;
            const auto& definition = source.at("definition");
            for (const auto* key : {"title", "package", "depends_on", "build"}) {
                result[key] = definition.at(key);
            }
            result.erase("definition");
            return result;
        }

        void validateExecutionSource(const Node& node) {
            auto selected = node.source;
            selected["definition"] = {{"schema", 1},
                                      {"title", selected.at("title")},
                                      {"package", selected.at("package")},
                                      {"depends_on", selected.at("depends_on")},
                                      {"build", selected.at("build")},
                                      {"matrix", {node.target}}};
            validateSource(selected.at("name").get<std::string>(), selected);
        }

        Attempt& current(Node& node, const std::string& attemptId) {
            require(!node.attempts.empty() && node.attempts.back().id == attemptId, "report does not belong to the active attempt");
            return node.attempts.back();
        }
    } // namespace

    void Graph::verifyDefinition(const Node& node, const nlohmann::json& document, const std::string& blobSha) {
        const auto definition = Definition::parse(document);
        require(node.source.at("definition_blob_sha") == blobSha, "selected definition blob differs from its persisted reference");
        for (const auto* key : {"title", "package", "depends_on", "build"}) {
            require(node.source.at(key) == definition.json.at(key), "execution inputs differ from the authoritative committed definition");
        }
        require(std::count(definition.json.at("matrix").begin(), definition.json.at("matrix").end(), node.target) == 1,
                "selected target differs from its committed definition");
    }

    std::string Graph::digest(const nlohmann::json& value) {
        const auto bytes = value.dump();
        unsigned char hash[EVP_MAX_MD_SIZE]{};
        unsigned int length = 0;
        require(EVP_Digest(bytes.data(), bytes.size(), hash, &length, EVP_sha256(), nullptr) == 1 && length == 32,
                "could not hash selection");
        constexpr char digits[] = "0123456789abcdef";
        std::string result;
        for (unsigned int index = 0; index < length; ++index) {
            result.push_back(digits[hash[index] >> 4]);
            result.push_back(digits[hash[index] & 15]);
        }
        return result;
    }

    const std::map<std::string, Node>& Graph::nodes() const {
        return records;
    }

    std::uint64_t Graph::nextRebuildRevision() const {
        return nextRevision;
    }

    const Node& Graph::node(const std::string& id) const {
        return records.at(id);
    }

    void Graph::restore(const std::vector<Node>& nodes, std::uint64_t next) {
        std::map<std::string, Node> restored;
        std::set<std::uint64_t> revisions;
        std::set<std::string> publicationFeeds;
        for (const auto& node : nodes) {
            require(!node.id.empty() && !node.attempts.empty() && restored.emplace(node.id, node).second,
                    "duplicate or empty persisted execution");
            validateExecutionSource(node);
            require(node.attempts.back().state != "publishing" || publicationFeeds.insert(publicationFeed(node)).second,
                    "persisted graph has conflicting assigned feed writers");
            require(node.id == digest(identity(node)), "persisted execution selection identity changed");
            std::uint64_t previous = 0;
            for (const auto& attempt : node.attempts) {
                require(attempt.revision > previous && attempt.revision < next && revisions.insert(attempt.revision).second &&
                            attempt.id == node.id + ":" + std::to_string(attempt.revision),
                        "invalid persisted attempt/rebuild revision");
                previous = attempt.revision;
                require(attempt.state == "waiting" || attempt.state == "building" || attempt.state == "publishing" ||
                            attempt.state == "published" || attempt.state == "failed",
                        "unknown persisted attempt state");
                require(attempt.state == "waiting" || !attempt.assignment.is_null(), "assigned attempt lost its immutable inputs");
                require(mergeEvidence(nlohmann::json::object(), attempt.evidence) == attempt.evidence,
                        "invalid persisted execution evidence");
                require(attempt.state != "published" || (!attempt.receipt.is_null() && !attempt.publicationRun.is_null()),
                        "published attempt lost its receipt");
            }
        }
        std::map<std::string, unsigned int> color;
        std::function<void(const std::string&)> visit = [&](const std::string& id) {
            require(color[id] != 1, "persisted execution dependency cycle");
            if (color[id] == 0) {
                color[id] = 1;
                for (const auto& prerequisite : restored.at(id).prerequisites) {
                    require(restored.contains(prerequisite), "persisted prerequisite execution is missing");
                    visit(prerequisite);
                }
                color[id] = 2;
            }
        };
        for (const auto& [id, node] : restored) {
            visit(id);
            require(node.supersededBy.empty() || restored.contains(node.supersededBy), "superseding execution is missing");
        }
        require(next > 0, "invalid next rebuild revision");
        records = std::move(restored);
        nextRevision = next;
    }

    std::vector<std::string> Graph::blockers(const std::string& id) const {
        const auto& selected = records.at(id);
        std::vector<std::string> reasons;
        if (!selected.supersededBy.empty()) {
            reasons.push_back("superseded by " + selected.supersededBy);
        }
        if (selected.attempts.back().state != "waiting") {
            reasons.push_back("attempt is " + selected.attempts.back().state);
        }
        for (const auto& prerequisite : selected.prerequisites) {
            const auto& other = records.at(prerequisite);
            if (other.attempts.back().state != "published") {
                reasons.push_back(other.source.at("name").get<std::string>() + "/" + other.target.at("id").get<std::string>() + " is " +
                                  other.attempts.back().state);
            }
        }
        if (reasons.empty() || (selected.attempts.back().state == "building" && !selected.attempts.back().buildResult.is_null())) {
            const auto reason = dispatchBlocker(selected, selected.attempts.back().state == "building");
            if (!reason.empty()) {
                reasons.push_back(reason);
            }
        }
        return reasons;
    }

    SourceTraversal Graph::traverseSources(const std::string& root, const std::map<std::string, nlohmann::json>& sources) {
        require(sources.contains(root), "requested repository has no pinned semantic-version source");
        SourceTraversal result;
        std::map<std::string, std::vector<std::string>> dependents;
        for (const auto& [name, source] : sources) {
            for (const auto& dependency : source.at("definition").at("depends_on")) {
                dependents[dependency.get<std::string>()].push_back(name);
            }
        }
        result.affected.insert(root);
        std::vector<std::string> traversal{root};
        for (std::size_t index = 0; index < traversal.size(); ++index) {
            for (const auto& dependent : dependents[traversal[index]]) {
                if (result.affected.insert(dependent).second) {
                    traversal.push_back(dependent);
                }
            }
        }
        std::set<std::string> visited;
        std::map<std::string, Definition> definitions;
        std::function<void(const std::string&)> visit = [&](const std::string& name) {
            require(sources.contains(name), "required prerequisite has no selected version tag: " + name);
            if (visited.insert(name).second) {
                definitions.emplace(name, Definition::parse(sources.at(name).at("definition")));
                for (const auto& dependency : sources.at(name).at("definition").at("depends_on")) {
                    visit(dependency.get<std::string>());
                }
                result.order.push_back(name);
            }
        };
        for (const auto& name : traversal) {
            visit(name);
        }
        Definition::validateGraph(definitions);
        return result;
    }

    GraphChange Graph::select(const std::string& root, const std::map<std::string, nlohmann::json>& sources, bool explicitRequest) const {
        const auto traversal = traverseSources(root, sources);
        const auto& affected = traversal.affected;
        for (const auto& name : traversal.order) {
            validateSource(name, sources.at(name));
        }

        GraphChange change;
        change.nextRevision = nextRevision;
        std::map<std::string, Node> selected;
        const nlohmann::json cause{{"repository", sources.at(root).at("repository")},
                                   {"tag", sources.at(root).at("tag")},
                                   {"commit", sources.at(root).at("commit")},
                                   {"explicit", explicitRequest}};
        bool acceptedRoot = false;
        for (const auto& name : traversal.order) {
            if (affected.contains(name) && (name == root || acceptedRoot)) {
                const auto& source = sources.at(name);
                for (const auto& row : source.at("definition").at("matrix")) {
                    Node next;
                    next.source = executionSource(source);
                    next.target = row;
                    next.cause = cause;
                    next.automatic = name != root || !explicitRequest;
                    for (const auto& dependency : source.at("definition").at("depends_on")) {
                        const auto dependencyName = dependency.get<std::string>();
                        const auto coordinate = Definition::coordinates(row);
                        const Node* prerequisite = nullptr;
                        if (affected.contains(dependencyName)) {
                            const auto found = selected.find(dependencyName + ":" + coordinate.dump());
                            require(found != selected.end(), "affected prerequisite target is missing");
                            prerequisite = &found->second;
                        } else {
                            for (const auto& [id, candidate] : records) {
                                if (candidate.supersededBy.empty() && candidate.source.at("name") == dependencyName &&
                                    sameSource(candidate.source, sources.at(dependencyName)) &&
                                    Definition::coordinates(candidate.target) == coordinate &&
                                    (prerequisite == nullptr ||
                                     candidate.attempts.front().revision > prerequisite->attempts.front().revision)) {
                                    prerequisite = &candidate;
                                }
                            }
                            require(prerequisite != nullptr,
                                    "missing selected prerequisite work for " + dependencyName + "; request it explicitly first");
                        }
                        next.prerequisites.push_back(prerequisite->id);
                    }
                    next.id = digest(identity(next));
                    if (name == root && explicitRequest && records.contains(next.id) && !records.at(next.id).supersededBy.empty()) {
                        next.cause["reactivated"] = next.id;
                        next.id = digest(identity(next));
                    }
                    if (records.contains(next.id)) {
                        next = records.at(next.id);
                        if (name == root && explicitRequest && next.automatic && next.attempts.back().state == "waiting" &&
                            next.supersededBy.empty()) {
                            next.automatic = false;
                            change.nodes.push_back(next);
                        }
                    } else {
                        acceptedRoot = acceptedRoot || name == root;
                        next.attempts.push_back(waiting(next.id, change.nextRevision));
                        ++change.nextRevision;
                        change.nodes.push_back(next);
                        const auto definition = std::find_if(change.definitions.begin(), change.definitions.end(), [&](const auto& known) {
                            return known.repositoryId == source.at("repository_id").get<std::uint64_t>() &&
                                   known.revision == source.at("definition_revision").get<std::string>();
                        });
                        if (definition == change.definitions.end()) {
                            change.definitions.push_back({source.at("repository_id").get<std::uint64_t>(),
                                                          source.at("definition_revision").get<std::string>(),
                                                          source.at("definition_blob_sha").get<std::string>(),
                                                          source.at("definition")});
                        }
                    }
                    selected.emplace(name + ":" + Definition::coordinates(row).dump(), next);
                }
            }
        }
        // Only newly accepted selections supersede older waiting automatic work.
        const auto created = change.nodes;
        for (const auto& replacement : created) {
            for (const auto& [id, existing] : records) {
                if (!records.contains(replacement.id) && existing.automatic && existing.supersededBy.empty() &&
                    existing.attempts.back().state == "waiting" && existing.source.at("name") == replacement.source.at("name") &&
                    Definition::coordinates(existing.target) == Definition::coordinates(replacement.target)) {
                    auto superseded = existing;
                    superseded.supersededBy = replacement.id;
                    change.nodes.push_back(std::move(superseded));
                }
            }
        }
        return change;
    }

    GraphChange Graph::retry(const std::string& id) const {
        auto selected = records.at(id);
        require(selected.attempts.back().state == "failed" && selected.supersededBy.empty(),
                "only definitively failed work can be retried");
        selected.attempts.push_back(waiting(id, nextRevision));
        return {{selected}, nextRevision + 1, {}};
    }

    std::string Graph::label(const Node& selected) {
        return selected.source.at("repository").get<std::string>() + " " + selected.source.at("tag").get<std::string>() + " | " +
               selected.target.at("distribution").get<std::string>() + " " + selected.target.at("suite").get<std::string>() + " | " +
               selected.target.at("arch").get<std::string>() + " | " + selected.target.at("build").at("format").get<std::string>() +
               " | r" + std::to_string(selected.attempts.back().revision);
    }

    nlohmann::json Graph::buildAssignment(const Node& selected) const {
        const auto& attempt = selected.attempts.back();
        const auto& id = selected.id;
        nlohmann::json dependencies = nlohmann::json::array();
        if (selected.attempts.size() > 1) {
            dependencies = selected.attempts.front().assignment.at("dependencies");
        } else {
            for (const auto& prerequisite : selected.prerequisites) {
                const auto& other = records.at(prerequisite);
                dependencies.push_back(
                    {{"execution", other.id}, {"attempt", other.attempts.back().id}, {"receipt", other.attempts.back().receipt}});
            }
        }
        nlohmann::json result = {{"schema", 1},
                                 {"label", label(selected)},
                                 {"execution", id},
                                 {"attempt", attempt.id},
                                 {"revision", attempt.revision},
                                 {"artifact_name", "core-ci-build-" + id + "-r" + std::to_string(attempt.revision)},
                                 {"source", selected.source},
                                 {"target", selected.target},
                                 {"dependencies", dependencies}};
        nlohmann::json previous = nlohmann::json::array();
        for (const auto& earlier : selected.attempts) {
            if (earlier.id != attempt.id && !earlier.buildRun.is_null()) {
                previous.push_back({{"run", earlier.buildRun},
                                    {"attempt", earlier.id},
                                    {"assignment_sha256", digest(earlier.assignment)},
                                    {"label", earlier.assignment.value("label", std::string{})},
                                    {"artifact_name", earlier.assignment.at("artifact_name").get<std::string>() + "-environment"}});
            }
        }
        if (!previous.empty()) {
            result["environment_from"] = previous;
        }
        return result;
    }

    nlohmann::json Graph::dispatchInputs(const Node& selected, bool publication) const {
        const auto& attempt = selected.attempts.back();
        const auto assignment = attempt.assignment.is_null() ? buildAssignment(selected) : attempt.assignment;
        nlohmann::json result{{"execution", selected.id},
                              {"attempt", attempt.id},
                              {"assignment_sha256", digest(assignment)},
                              {"assignment", assignment.dump()}};
        if (publication) {
            result["build_result"] = attempt.buildResult.dump();
        }
        return result;
    }

    std::string Graph::dispatchBlocker(const Node& selected, bool publication) const {
        const auto inputs = dispatchInputs(selected, publication);
        std::string reason;
        if (inputs.at("assignment").get_ref<const std::string&>().size() > 60000 || inputs.dump().size() > 65535) {
            reason = "GitHub dispatch payload exceeds supported input bounds";
        }
        return reason;
    }

    Node Graph::beginBuild(const std::string& id) const {
        require(blockers(id).empty(), "execution is not eligible");
        auto selected = records.at(id);
        selected.attempts.back().assignment = buildAssignment(selected);
        selected.attempts.back().state = "building";
        return selected;
    }

    Node Graph::bindRun(const std::string& id,
                        const std::string& attemptId,
                        bool publication,
                        const nlohmann::json& run,
                        const nlohmann::json& observation) const {
        auto selected = records.at(id);
        require(run.at("id").get<std::uint64_t>() != 0 && run.at("run_attempt").get<std::uint64_t>() == 1 &&
                    run.at("repository_id").get<std::uint64_t>() != 0 && run.contains("workflow_blob_sha") && run.contains("actor_id"),
                "incomplete GitHub run identity");
        auto found = std::find_if(selected.attempts.begin(), selected.attempts.end(), [&](const auto& attempt) {
            return attempt.id == attemptId;
        });
        require(found != selected.attempts.end() && !found->assignment.is_null(), "run report does not match assigned work");
        const auto& deployment = selected.source.at("deployment");
        const auto expectedRepository = publication ? deployment.at("worker").at("repository_id") : selected.source.at("repository_id");
        const auto expectedWorkflow = deployment.at(publication ? "publication_workflow" : "source_workflow").at("blob_sha");
        require(run.at("repository_id") == expectedRepository && run.at("workflow_blob_sha") == expectedWorkflow &&
                    run.at("actor_id") == deployment.at("controller_app_bot_id"),
                "GitHub run repository/workflow/actor differs from assignment");
        auto& assigned = publication ? found->publicationRun : found->buildRun;
        require(assigned.is_null() || assigned == run, "conflicting GitHub run identity");
        assigned = run;
        if (!observation.empty()) {
            require(observation.at("run_id") == assigned.at("id"), "execution evidence belongs to another run");
            found->evidence = mergeEvidence(found->evidence, {{publication ? "publication" : "build", observation}});
        }
        return selected;
    }

    Node Graph::built(const std::string& id, const std::string& attemptId, const nlohmann::json& result) const {
        auto selected = records.at(id);
        auto& attempt = current(selected, attemptId);
        require(attempt.state == "building" && !attempt.buildRun.is_null(), "build result does not match an assigned building attempt");
        require(result.at("run") == attempt.buildRun && result.at("assignment_sha256") == digest(attempt.assignment) &&
                    result.at("artifacts").is_array() && !result.at("artifacts").empty(),
                "verified build artifact identity does not match assignment");
        require(attempt.buildResult.is_null() || attempt.buildResult == result, "conflicting build result");
        attempt.buildResult = result;
        return selected;
    }

    bool Graph::publicationReady(const std::string& id) const {
        const auto& selected = records.at(id);
        const auto& attempt = selected.attempts.back();
        bool ready = attempt.state == "building" && !attempt.buildResult.is_null() && dispatchBlocker(selected, true).empty();
        if (ready) {
            const auto feed = publicationFeed(selected);
            for (const auto& [otherId, other] : records) {
                if (other.attempts.back().state == "publishing" && publicationFeed(other) == feed) {
                    ready = false;
                }
            }
        }
        return ready;
    }

    Node Graph::beginPublication(const std::string& id) const {
        auto selected = records.at(id);
        auto& attempt = selected.attempts.back();
        require(publicationReady(id), "publication requires a verified assigned build and no uncertain writer for this feed");
        attempt.state = "publishing";
        return selected;
    }

    Node Graph::published(const std::string& id, const std::string& attemptId, const nlohmann::json& receipt) const {
        auto selected = records.at(id);
        auto& attempt = current(selected, attemptId);
        require((attempt.state == "publishing" || attempt.state == "published") && !attempt.publicationRun.is_null(),
                "publication does not match assigned work");
        require(receipt.at("execution") == id && receipt.at("attempt") == attemptId &&
                    receipt.at("assignment_sha256") == digest(attempt.assignment) && receipt.at("run") == attempt.publicationRun &&
                    receipt.at("source_commit") == selected.source.at("commit") && receipt.at("feed_revision").is_string() &&
                    receipt.at("environment").is_object() && !receipt.at("environment").empty() && receipt.at("packages").is_array() &&
                    !receipt.at("packages").empty(),
                "publication receipt identities differ from assignment");
        // Verification time belongs to the controller; the feed cannot supply or refresh it.
        auto verified = receipt;
        verified.erase("publication_verified_utc");
        auto previous = attempt.receipt;
        if (!previous.is_null()) {
            previous.erase("publication_verified_utc");
        }
        require(previous.is_null() || previous == verified, "conflicting publication receipt");
        if (attempt.receipt.is_null()) {
            const auto now = std::time(nullptr);
            const auto* utc = std::gmtime(&now);
            require(utc != nullptr, "publication verification clock is unavailable");
            std::ostringstream timestamp;
            timestamp << std::put_time(utc, "%Y-%m-%dT%H:%M:%SZ");
            verified["publication_verified_utc"] = timestamp.str();
            attempt.receipt = verified;
        }
        attempt.state = "published";
        return selected;
    }

    Node Graph::failed(const std::string& id,
                       const std::string& attemptId,
                       const std::string& reason,
                       const nlohmann::json& buildRun,
                       const nlohmann::json& observation) const {
        auto selected = buildRun.is_null() ? records.at(id) : bindRun(id, attemptId, false, buildRun, observation);
        auto& attempt = current(selected, attemptId);
        require((attempt.state == "building" || attempt.state == "publishing" || attempt.state == "failed") && !reason.empty(),
                "failure must belong to assigned work");
        require((attempt.state != "building" || !attempt.buildRun.is_null()) &&
                    (attempt.state != "publishing" || !attempt.publicationRun.is_null()),
                "unknown execution outcome is not proof of failure");
        require(attempt.failure.empty() || attempt.failure == reason, "conflicting failure result");
        attempt.failure = reason;
        attempt.state = "failed";
        return selected;
    }

    Node Graph::rejectedDispatch(const std::string& id,
                                 const std::string& attemptId,
                                 bool publication,
                                 int status,
                                 bool submitted,
                                 const std::string& reason) const {
        auto selected = records.at(id);
        auto& attempt = current(selected, attemptId);
        require(attempt.state == (publication ? "publishing" : "building") && !attempt.assignment.is_null() &&
                    (publication ? attempt.publicationRun : attempt.buildRun).is_null(),
                "dispatch failure does not match assigned work");
        require(!reason.empty() && (!submitted || (status >= 400 && status < 500 && status != 408)),
                "uncertain dispatch outcome is not proof of failure");
        attempt.state = "failed";
        attempt.failure = reason;
        return selected;
    }

    void Graph::validate(const GraphChange& change) const {
        require(change.nextRevision >= nextRevision, "graph revision allocator regressed");
        std::map<std::string, const Node*> changed;
        std::set<std::uint64_t> allocated;
        std::set<std::string> publicationFeeds;
        for (const auto& node : change.nodes) {
            require(changed.emplace(node.id, &node).second && !node.attempts.empty(), "duplicate/empty execution mutation");
            require(node.id == digest(identity(node)), "execution selection identity changed");
            const auto previous = records.find(node.id);
            if (previous != records.end()) {
                const auto& old = previous->second;
                require(
                    old.source == node.source && old.target == node.target && old.cause == node.cause &&
                        old.prerequisites == node.prerequisites &&
                        (old.automatic == node.automatic || (old.automatic && !node.automatic && old.attempts.back().state == "waiting")),
                    "execution selection inputs changed");
                require(old.supersededBy.empty() || old.supersededBy == node.supersededBy, "superseded history was rewritten");
                require(node.attempts.size() >= old.attempts.size() && node.attempts.size() <= old.attempts.size() + 1,
                        "attempt history was removed/duplicated");
                if (node.attempts.size() > old.attempts.size()) {
                    require(old.attempts.back().state == "failed" && node.supersededBy.empty(), "retry requires definitive failed work");
                }
                if (old.supersededBy != node.supersededBy) {
                    require(old.automatic && old.attempts.back().state == "waiting", "assigned/explicit work was superseded");
                }
                for (std::size_t index = 0; index < old.attempts.size(); ++index) {
                    const auto& before = old.attempts[index];
                    const auto& after = node.attempts[index];
                    require(mergeEvidence(before.evidence, after.evidence) == after.evidence,
                            "execution evidence regressed or disappeared");
                    require(before.id == after.id && before.revision == after.revision, "attempt identity changed");
                    require(before.assignment.is_null() || before.assignment == after.assignment, "assigned inputs changed");
                    require(before.buildRun.is_null() || before.buildRun == after.buildRun, "assigned build run changed");
                    require(before.publicationRun.is_null() || before.publicationRun == after.publicationRun,
                            "assigned publication run changed");
                    require(before.buildResult.is_null() || before.buildResult == after.buildResult, "accepted build result changed");
                    require(before.receipt.is_null() || before.receipt == after.receipt, "published receipt changed");
                    if (before.state == "building" && after.state == "publishing") {
                        require(publicationReady(node.id) && publicationFeeds.insert(publicationFeed(node)).second,
                                "publication conflicts with assigned work for the same feed");
                    }
                    bool allowed = before.state == after.state;
                    if (index + 1 == old.attempts.size()) {
                        allowed = allowed || (before.state == "waiting" && after.state == "building") ||
                                  (before.state == "building" && (after.state == "publishing" || after.state == "failed")) ||
                                  (before.state == "publishing" && (after.state == "published" || after.state == "failed"));
                    }
                    require(allowed, "attempt state regressed or skipped a required transition");
                }
            } else {
                validateExecutionSource(node);
                require(node.attempts.size() == 1 && node.supersededBy.empty(), "new execution has unexpected history");
                const auto definition = std::find_if(change.definitions.begin(), change.definitions.end(), [&](const auto& selected) {
                    return selected.repositoryId == node.source.at("repository_id").get<std::uint64_t>() &&
                           selected.revision == node.source.at("definition_revision").get<std::string>();
                });
                require(definition != change.definitions.end(), "new execution lost its committed definition reference");
                verifyDefinition(node, definition->document, definition->blobSha);
            }
            const std::size_t oldCount = previous == records.end() ? 0 : previous->second.attempts.size();
            for (std::size_t index = oldCount; index < node.attempts.size(); ++index) {
                const auto& attempt = node.attempts[index];
                require(attempt.state == "waiting" && attempt.assignment.is_null() && attempt.revision >= nextRevision &&
                            attempt.revision < change.nextRevision && allocated.insert(attempt.revision).second &&
                            attempt.id == node.id + ":" + std::to_string(attempt.revision),
                        "new attempt/rebuild revision is invalid");
            }
        }
        require(allocated.size() == change.nextRevision - nextRevision, "reserved rebuild revisions do not match new attempts");
        for (const auto& [id, node] : changed) {
            for (const auto& prerequisite : node->prerequisites) {
                require(changed.contains(prerequisite) || records.contains(prerequisite), "execution prerequisite is missing");
            }
            require(node->supersededBy.empty() || changed.contains(node->supersededBy) || records.contains(node->supersededBy),
                    "superseding execution is missing");
        }
    }

    void Graph::apply(const GraphChange& change) {
        validate(change);
        for (const auto& node : change.nodes) {
            records.insert_or_assign(node.id, node);
        }
        nextRevision = change.nextRevision;
    }

} // namespace coreci
