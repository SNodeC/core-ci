#include "Auth.h"
#include "Controller.h"

#include <algorithm>
#include <ctime>
#include <regex>
#include <set>

namespace ci {

    void require(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    std::string utc() {
        const std::time_t now = std::time(nullptr);
        std::tm time{};
        gmtime_r(&now, &time);
        char text[32]{};
        std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &time);
        return text;
    }

    namespace {

        std::vector<std::string> parts(std::string tag) {
            require(validVersion(tag), "Invalid SemVer tag");
            if (tag.front() == 'v') {
                tag.erase(0, 1);
            }
            tag = tag.substr(0, tag.find('+'));
            const auto dash = tag.find('-');
            std::vector<std::string> result;
            const std::string core = tag.substr(0, dash);
            std::size_t start = 0;
            for (std::size_t index = 0; index <= core.size(); ++index) {
                if (index == core.size() || core[index] == '.') {
                    result.push_back(core.substr(start, index - start));
                    start = index + 1;
                }
            }
            if (dash != std::string::npos) {
                start = dash + 1;
                for (std::size_t index = start; index <= tag.size(); ++index) {
                    if (index == tag.size() || tag[index] == '.') {
                        result.push_back(tag.substr(start, index - start));
                        start = index + 1;
                    }
                }
            }
            return result;
        }

        bool numeric(const std::string& text) {
            return !text.empty() && text.find_first_not_of("0123456789") == std::string::npos;
        }

        std::uint64_t revision(const Json& graph, const Json& work) {
            std::uint64_t result = 1;
            for (const auto& candidate : graph.at("executions")) {
                if (candidate.at("source").at("package") == work.at("source").at("package") &&
                    compareVersions(candidate.at("source").at("tag"), work.at("source").at("tag")) == 0 &&
                    coordinates(candidate.at("target")) == coordinates(work.at("target"))) {
                    for (const auto& attempt : candidate.at("attempts")) {
                        require(attempt.at("revision").get<std::uint64_t>() < UINT64_MAX, "Rebuild revision exhausted");
                        result = std::max(result, attempt.at("revision").get<std::uint64_t>() + 1);
                    }
                }
            }
            return result;
        }

        Json attempt(const Json& graph, const Json& work) {
            const auto number = revision(graph, work);
            return {{"revision", number},
                    {"id", work.at("id").get<std::string>() + ":" + std::to_string(number)},
                    {"state", "waiting"},
                    {"created", utc()},
                    {"facts", Json::object()}};
        }

    } // namespace

    bool validVersion(const std::string& tag) {
        static const std::regex syntax(R"(v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*))"
                                       R"((-([0-9A-Za-z-]+\.)*[0-9A-Za-z-]+)?(\+([0-9A-Za-z-]+\.)*[0-9A-Za-z-]+)?)");
        bool valid = tag.size() <= 240 && std::regex_match(tag, syntax);
        if (valid && tag.find('-') != std::string::npos && tag.find('-') < tag.find('+')) {
            const std::string pre = tag.substr(tag.find('-') + 1, tag.find('+') - tag.find('-') - 1);
            std::size_t start = 0;
            for (std::size_t index = 0; index <= pre.size(); ++index) {
                if (index == pre.size() || pre[index] == '.') {
                    const std::string identifier = pre.substr(start, index - start);
                    valid = valid && !(numeric(identifier) && identifier.size() > 1 && identifier.front() == '0');
                    start = index + 1;
                }
            }
        }
        return valid;
    }

    int compareVersions(const std::string& left, const std::string& right) {
        const auto a = parts(left);
        const auto b = parts(right);
        int result = 0;
        for (std::size_t index = 0; index < std::min(a.size(), b.size()) && result == 0; ++index) {
            if (index == 3 && (a.size() == 3 || b.size() == 3)) {
                result = a.size() == 3 ? 1 : -1;
            } else if (numeric(a[index]) && numeric(b[index])) {
                result = a[index].size() == b[index].size() ? a[index].compare(b[index]) : (a[index].size() < b[index].size() ? -1 : 1);
            } else if (numeric(a[index]) != numeric(b[index])) {
                result = numeric(a[index]) ? -1 : 1;
            } else {
                result = a[index].compare(b[index]);
            }
        }
        if (result == 0 && a.size() != b.size()) {
            result = a.size() == 3 ? 1 : (b.size() == 3 ? -1 : (a.size() < b.size() ? -1 : 1));
        }
        return result;
    }

    Json coordinates(const Json& target) {
        Json result;
        for (const auto& key : {"family", "distribution", "suite", "arch"}) {
            result[key] = target.at(key);
        }
        result["format"] = target.at("build").at("format");
        const std::string field = target.at("family") == "native" ? "platform" : "target";
        result[field] = target.at("build").at(field);
        return result;
    }

    std::string sourceTag(const Json& tags, const std::string& requested) {
        std::string result = requested;
        for (const auto& item : tags) {
            const std::string name = item.at("name");
            if (requested.empty() && validVersion(name) && (result.empty() || compareVersions(name, result) > 0)) {
                result = name;
            }
        }
        require(validVersion(result), "No valid selected SemVer");
        if (requested.empty()) {
            for (const auto& item : tags) {
                const std::string name = item.at("name");
                require(!validVersion(name) || compareVersions(name, result) != 0 || name == result, "Ambiguous highest SemVer tags");
            }
        }
        return result;
    }

    Json& current(Json& execution) {
        return execution.at("attempts").back();
    }

    std::string
    selectWork(Json& graph, const Json& selection, const std::string& repository, const Json& target, std::vector<std::string> path) {
        require(std::find(path.begin(), path.end(), repository) == path.end(), "Dependency cycle");
        path.push_back(repository);
        const Json& selected = selection.at(repository);
        Json prerequisites = Json::array();
        for (const auto& name : selected.at("source").at("depends_on")) {
            std::string found;
            Json matching;
            for (const auto& [dependency, value] : selection.items()) {
                if (value.at("source").at("package") == name) {
                    for (const auto& candidate : value.at("definition").at("matrix")) {
                        if (coordinates(candidate) == coordinates(target)) {
                            require(found.empty(), "Ambiguous prerequisite target");
                            found = dependency;
                            matching = candidate;
                        }
                    }
                }
            }
            require(!found.empty(), "No exact matching prerequisite target");
            prerequisites.push_back(selectWork(graph, selection, found, matching, path));
        }
        const Json identity = {{"source", selected.at("source")}, {"target", target}, {"prerequisites", prerequisites}};
        const std::string id = hash(identity.dump());
        if (findId(graph.at("executions"), id) == nullptr) {
            Json work = identity;
            work["workflows"] = selected.at("workflows");
            work["definition"] = selected.at("definition");
            work["id"] = id;
            work["attempts"] = Json::array();
            work["attempts"].push_back(attempt(graph, work));
            graph["executions"].push_back(work);
        }
        return id;
    }

    std::vector<std::string> affectedRepositories(const Json& repositories, const std::string& repository) {
        std::vector<std::string> result{repository};
        for (std::size_t index = 0; index < result.size(); ++index) {
            const auto package = repositories.at(result[index]).at("definition").at("package");
            for (const auto& [name, value] : repositories.items()) {
                const auto& dependencies = value.at("definition").at("depends_on");
                if (std::find(dependencies.begin(), dependencies.end(), package) != dependencies.end() &&
                    std::find(result.begin(), result.end(), name) == result.end()) {
                    result.push_back(name);
                }
            }
        }
        return result;
    }

    bool selectedTag(const Json& graph, const std::string& repository, const std::string& tag, const std::string& commit) {
        const auto& executions = graph.at("executions");
        return std::any_of(executions.begin(), executions.end(), [&](const auto& work) {
            const auto& matrix = work.at("definition").at("matrix");
            return work.at("source").at("repository") == repository && work.at("source").at("tag") == tag &&
                   work.at("source").at("commit") == commit && !matrix.empty() &&
                   std::all_of(matrix.begin(), matrix.end(), [&](const auto& target) {
                       return std::any_of(executions.begin(), executions.end(), [&](const auto& candidate) {
                           return candidate.at("source") == work.at("source") && candidate.at("target") == target;
                       });
                   });
        });
    }

    Json selectRequest(Json& graph, const Json& selection, const std::string& repository, const std::string& commit) {
        const auto count = graph.at("executions").size();
        const auto& selected = selection.at(repository);
        require(commit.empty() || selected.at("source").at("commit") == commit, "Tag no longer matches authenticated notification");
        if (commit.empty() || !selectedTag(graph, repository, selected.at("source").at("tag"), commit)) {
            for (const auto& name : affectedRepositories(selection, repository)) {
                for (const auto& target : selection.at(name).at("definition").at("matrix")) {
                    selectWork(graph, selection, name, target);
                }
            }
        }
        for (std::size_t index = count; index < graph.at("executions").size(); ++index) {
            graph["executions"][index]["selection"] = count;
        }
        return graph.at("executions").size() > count ? commands(graph) : Json::array();
    }

    Json commands(Json& graph) {
        Json result = Json::array();
        for (auto& work : graph.at("executions")) {
            Json& active = current(work);
            if (active.at("state") == "waiting") {
                Json dependencies = Json::array();
                for (const auto& id : work.at("prerequisites")) {
                    for (auto& prerequisite : graph.at("executions")) {
                        if (prerequisite.at("id") == id && current(prerequisite).at("state") == "published") {
                            const auto& previous = current(prerequisite);
                            Json dependency = {{"execution", id}, {"attempt", previous.at("id")}, {"receipt", previous.at("receipt")}};
                            dependency["receipt"].erase("indexes");
                            dependencies.push_back(std::move(dependency));
                        }
                    }
                }
                if (dependencies.size() == work.at("prerequisites").size()) {
                    if (!active.contains("assignment")) {
                        const std::string id = work.at("id");
                        const std::string label = work.at("source").at("repository").get<std::string>() + " " +
                                                  work.at("source").at("tag").get<std::string>() + " " +
                                                  work.at("target").at("id").get<std::string>();
                        active["assignment"] = {{"schema", 1},
                                                {"execution", id},
                                                {"attempt", active.at("id")},
                                                {"revision", active.at("revision")},
                                                {"artifact_name", "ci_" + id + "_" + active.at("revision").dump()},
                                                {"label", label.substr(0, 150) + " r" + active.at("revision").dump() + " " + id},
                                                {"source", work.at("source")},
                                                {"target", work.at("target")},
                                                {"dependencies", dependencies}};
                    }
                    active["state"] = "building";
                    active["build_authorized"] = utc();
                    active["build_assignment"] = work.at("workflows").at("build");
                    result.push_back({{"execution", work.at("id")},
                                      {"attempt", active.at("id")},
                                      {"stage", "build"},
                                      {"command", active.at("build_assignment")},
                                      {"assignment", active.at("assignment")}});
                }
            }
            if (active.at("state") == "built") {
                active["state"] = "publishing";
                active["publication_authorized"] = utc();
                active["publish_assignment"] = work.at("workflows").at("publish");
                result.push_back({{"execution", work.at("id")},
                                  {"attempt", active.at("id")},
                                  {"stage", "publish"},
                                  {"command", active.at("publish_assignment")},
                                  {"assignment", active.at("assignment")},
                                  {"build_result", active.at("build_result")}});
            }
        }
        return result;
    }

    void retry(Json& graph, const std::string& execution) {
        auto* work = findId(graph.at("executions"), execution);
        require(work != nullptr, "Unknown execution");
        const Json previous = current(*work);
        require(previous.at("state") == "failed" && previous.value("failure_kind", "") == "definite",
                "Only definite failures permit retry");
        (*work)["attempts"].push_back(attempt(graph, *work));
    }

    void retire(Json& graph, const std::string& execution, const std::string& acknowledgement) {
        require(!acknowledgement.empty() && acknowledgement.size() <= 4096, "Explicit retirement acknowledgement required");
        auto* work = findId(graph.at("executions"), execution);
        require(work != nullptr, "Unknown execution");
        auto& active = current(*work);
        require(active.at("state") == "publishing" && !active.at("facts").contains("publish_run") &&
                    !active.at("facts").contains("publish_run_id"),
                "Only an unbound publication may retire");
        active["state"] = "failed";
        active["failure_kind"] = "retirement";
        active["retirement"] = {{"acknowledgement", acknowledgement}, {"at", utc()}, {"remote_outcome", "unknown"}};
    }

    namespace {
        void merge(Json& previous, const Json& incoming) {
            require(previous.is_null() || !previous.contains("identity") || previous.at("identity") == incoming.at("identity"),
                    "Conflicting assigned run evidence");
            Json next = incoming;
            if (!previous.is_null()) {
                for (const auto& [key, value] : previous.items()) {
                    if (!next.contains(key) || next.at(key).is_null()) {
                        next[key] = value;
                    }
                }
                if (previous.at("status") == "completed" ||
                    (previous.at("status") == "in_progress" && incoming.at("status") != "completed")) {
                    for (const std::string field : {"status", "conclusion", "started_at", "completed_at"}) {
                        if (previous.contains(field)) {
                            next[field] = previous.at(field);
                        }
                    }
                }
                for (const std::string field : {"jobs", "steps"}) {
                    if (previous.contains(field)) {
                        const std::string identity = field == "jobs" ? "id" : "number";
                        for (const auto& old : previous.at(field)) {
                            auto found = std::find_if(next.at(field).begin(), next.at(field).end(), [&](const Json& item) {
                                return old.at(identity) == item.at(identity);
                            });
                            if (found == next.at(field).end()) {
                                next[field].push_back(old);
                            } else {
                                Json accumulated = old;
                                merge(accumulated, *found);
                                *found = std::move(accumulated);
                            }
                        }
                    }
                }
            }
            previous = std::move(next);
        }

    } // namespace
    Json applyEvidence(
        Json& graph, const Json& selected, const std::string& stage, const Json& observed, const Json& evidence, std::string error) {
        bool ready = false;
        if (auto* work = findId(graph.at("executions"), selected.at("id"))) {
            if (auto* active = findId(work->at("attempts"), selected.at("attempts").back().at("id"))) {
                const bool awaiting = active->at("id") == work->at("attempts").back().at("id") &&
                                      ((stage == "build" && active->at("state") == "building") ||
                                       (stage == "publish" && active->at("state") == "publishing"));
                if (awaiting) {
                    if (evidence.contains("receipt") &&
                        (!evidence.at("receipt").is_object() || evidence.at("receipt").value("execution", Json()) != selected.at("id") ||
                         evidence.at("receipt").value("attempt", Json()) != active->at("id"))) {
                        error = "Unrelated publisher receipt";
                    }
                    const Json previous = active->value("uncertainty", Json::object());
                    const std::string kind = observed.is_null() && previous.value("stage", stage) == stage
                                                 ? previous.value("kind", previous.empty() ? "read" : "evidence")
                                                 : (observed.is_null() ? "read" : "evidence");
                    if (!error.empty()) {
                        if (previous.value("stage", stage) != stage || previous.value("kind", "evidence") != kind ||
                            previous.value("reason", "") != error) {
                            const Json at = previous.value("reason", "") == error ? previous.at("at") : Json(utc());
                            (*active)["uncertainty"] = {{"at", at}, {"stage", stage}, {"kind", kind}, {"reason", error}};
                        }
                    } else if (observed.is_object() && previous.value("stage", stage) == stage &&
                               previous.value("kind", "evidence") == "read") {
                        active->erase("uncertainty");
                    }
                }
                if (!observed.is_null() && (error.empty() || awaiting)) {
                    merge((*active)["facts"][stage + "_run"], observed);
                    if (evidence.contains("verification_started_at")) {
                        const std::string started = evidence.at("verification_started_at");
                        (*active)["verification_started_at"] = std::min(active->value("verification_started_at", started), started);
                    }
                    const auto& facts = active->at("facts").at(stage + "_run");
                    (*active)["facts"][stage + "_run_id"] = observed.at("identity").at("id");
                    if (awaiting && error.empty() && stage == "build" && evidence.contains("result")) {
                        (*active)["build_result"] = evidence.at("result");
                        (*active)["build_manifest"] = evidence.at("manifest");
                        (*active)["state"] = "built";
                        (*active)["built_at"] = utc();
                        active->erase("uncertainty");
                        ready = true;
                    } else if (awaiting && error.empty() && stage == "publish" && evidence.contains("receipt")) {
                        (*active)["receipt"] = evidence.at("receipt");
                        (*active)["proof"] = evidence;
                        (*active)["proof"].erase("receipt");
                        active->erase("uncertainty");
                        (*active)["state"] = "published";
                        ready = true;
                    } else if (awaiting && facts.at("status") == "completed" && facts.at("conclusion") == "failure") {
                        bool definite = stage == "build";
                        for (const auto& job : facts.at("jobs")) {
                            if (job.at("id") == facts.at("job_id")) {
                                for (const auto& step : job.at("steps")) {
                                    definite = definite ||
                                               (step.at("name") == "Publish checked feed update" && step.at("conclusion") == "skipped");
                                }
                            }
                        }
                        if (definite) {
                            (*active)["state"] = "failed";
                            (*active)["failure_kind"] = "definite";
                            (*active)["failure"] = "Verified workflow/job failed before publication could mutate the feed";
                        }
                    }
                }
            }
        }
        Json authorized = Json::array();
        if (ready) {
            authorized = commands(graph);
        }
        return authorized;
    }
} // namespace ci
