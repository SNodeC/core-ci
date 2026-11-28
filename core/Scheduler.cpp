#include "Scheduler.h"

#include "Version.h"

#include <algorithm>
#include <charconv>
#include <openssl/sha.h>
#include <ranges>
#include <set>
#include <tuple>

namespace coreci {
    namespace {
        using Text = std::string;

        Text keyFor(const Json& item) {
            const auto& target = item.at("target");
            return item.at("package").get<Text>() + "/" + item.at("version").get<Text>() + "/" +
                   (target.is_string() ? target : target.at("id")).get<Text>();
        }

        unsigned eligibility(const Json& state) {
            return state.at("pass").get<unsigned>() + 1;
        }

        bool eligible(const Json& state, const Json& item) {
            return item.at("eligible") <= state.at("pass");
        }

        bool wasPublished(const Json& item) {
            return item.value("published_revision", 0U) == item.at("revision") && item.at("revision") != 0;
        }

        bool eligibleOutput(const Json& state, const Json& output) {
            const auto& items = state.at("items");
            const auto current = items.find(keyFor(output));
            return current == items.end() || current->at("revision") != output.at("revision") ||
                   current->value("output_eligible", 0U) <= state.at("pass").get<unsigned>();
        }

        Version version(const Json& item) {
            return Version::parse(item.at("version").get<Text>()).value();
        }

        bool newer(const Json& candidate, const Json& incumbent) {
            return version(candidate) > version(incumbent) ||
                   (version(candidate) == version(incumbent) && candidate.at("revision") > incumbent.at("revision"));
        }

        bool currentPublication(const Json& attempt) {
            return attempt.at("publication") == true && (reserved(attempt) || !attempt.value("reconciled", false));
        }

        void finalizeBuild(Json& state, Json& record, Time now, const Options& options);
        void replaceAttempt(Json& state, Json& old, Time now, const Options& options);

        Json attempt(const Text& id, unsigned number, bool publication, const Text& document, Time now, const Options& options) {
            return {{"id", id},
                    {"attempt", number},
                    {"publication", publication},
                    {"document", document},
                    {"run_state", "dispatched"},
                    {"dispatch", "pending"},
                    {"run", 0},
                    {"poll_at", now},
                    {"queue_at", now + options.queue},
                    {"reconciled", false}};
        }

        void enroll(Json& state, const Json& release) {
            const auto& declaration = state.at("declarations").at(release.at("repo").get<Text>());
            for (const auto& target : declaration.at("targets")) {
                Json item = release;
                item["package"] = declaration.at("package");
                item["target"] = target;
                item["revision"] = 0;
                item["state"] = "enrolled";
                item["eligible"] = eligibility(state);
                state["items"].emplace(keyFor(item), std::move(item));
            }
        }

        struct Selection {
            Text condition, reason;
            Json direct = Json::array(), closure = Json::array();
        };

        bool retired(const Json& state, const Json& output) {
            const auto& target = output.at("target");
            const auto key = output.at("package").get<Text>() + "/" + output.at("version").get<Text>() + "/" +
                             std::to_string(output.at("revision").get<unsigned>()) + "/" +
                             (target.is_string() ? target : target.at("id")).get<Text>();
            return std::ranges::any_of(state.at("attempts"), [&](const auto& publication) {
                return currentPublication(publication) && publication.contains("retiring") && publication.at("retiring").contains(key);
            });
        }

        bool inProgress(const Json& item) {
            return item.at("state") == "processing" || item.at("state") == "built" ||
                   (item.at("state") == "enrolled" && item.value("condition", "") != "blocked" &&
                    item.value("condition", "") != "unsupported");
        }

        void mergeBinding(Selection& selection, const Json& output, const Json& state) {
            const auto found = std::ranges::find_if(selection.closure, [&](const auto& bound) {
                return bound.at("package") == output.at("package");
            });
            if (found == selection.closure.end()) {
                selection.closure.push_back(output);
            } else if (found->at("identity") != output.at("identity")) {
                const bool pending = std::ranges::any_of(state.at("items"), [&](const auto& item) {
                    return eligible(state, item) && item.at("package") == output.at("package") &&
                           item.at("target") == output.at("target") && inProgress(item);
                });
                selection.condition = pending ? "waiting" : "blocked";
                selection.reason = pending ? "checking dependency refresh" : "closure conflict";
            }
        }

        Selection resolve(const Json& state, const Json& item) {
            Selection result;
            const auto repository = item.at("repo").get<Text>();
            const auto target = item.at("target").get<Text>();
            if (!state.at("declarations").contains(repository) || !state.at("catalog").contains(target)) {
                result.condition = "blocked";
                result.reason = "target missing";
            } else {
                const auto& declaration = state.at("declarations").at(repository);
                if (!version(item).satisfies(declaration.at("releases").get<Text>())) {
                    result.condition = "unsupported";
                    result.reason = "release outside declaration range";
                } else if (std::ranges::find(declaration.at("targets"), Json(target)) == declaration.at("targets").end()) {
                    result.condition = "blocked";
                    result.reason = "target missing";
                }
                for (const auto& dependency : declaration.at("depends_on")) {
                    if (result.condition.empty()) {
                        const auto& package = dependency.at("package");
                        const auto range = dependency.at("version").get<Text>();
                        const Json* best = nullptr;
                        bool pending = false, failed = false, hasTarget = false;
                        for (const auto& output : state.at("entries")) {
                            if (!output.value("legacy", false) && output.at("package") == package &&
                                output.at("target").at("id") == target) {
                                hasTarget = true;
                                if (eligibleOutput(state, output) && version(output).satisfies(range) && (!best || newer(output, *best)) &&
                                    !retired(state, output) && std::ranges::none_of(output.at("closure"), [&](const auto& bound) {
                                        return retired(state, bound);
                                    })) {
                                    best = &output;
                                }
                            }
                        }
                        for (const auto& candidate : state.at("items")) {
                            if (eligible(state, candidate) && candidate.at("package") == package && candidate.at("target") == target) {
                                hasTarget = true;
                                if (version(candidate).satisfies(range)) {
                                    pending = pending || ((!best || version(candidate) > version(*best)) && inProgress(candidate));
                                    failed = failed || candidate.at("state") == "failed" || candidate.value("condition", "") == "blocked";
                                }
                            }
                        }
                        if (pending || !best) {
                            result.condition = pending ? "waiting" : "blocked";
                            result.reason = pending
                                                ? "newer prerequisite in progress"
                                                : (failed ? "prerequisite failed" : (hasTarget ? "range unsatisfiable" : "target missing"));
                        } else {
                            for (const auto& bound : best->at("closure")) {
                                mergeBinding(result, bound, state);
                            }
                            auto output = *best;
                            output["target"] = target;
                            output["repository"] = output.at("repo");
                            output.erase("closure");
                            output.erase("repo");
                            mergeBinding(result, output, state);
                            result.direct.push_back({{"package", best->at("package")}, {"identity", best->at("identity")}});
                        }
                    }
                }
            }
            return result;
        }

        bool reaches(const Json& state, const Json& item, const Json& root, std::set<Text>& visited) {
            const auto id = keyFor(item);
            bool found = false;
            if (visited.insert(id).second && state.at("declarations").contains(item.at("repo").get<Text>())) {
                const auto& declaration = state.at("declarations").at(item.at("repo").get<Text>());
                for (const auto& dependency : declaration.at("depends_on")) {
                    found = found || std::ranges::any_of(state.at("items"), [&](const auto& related) {
                                return eligible(state, related) && related.at("package") == dependency.at("package") &&
                                       related.at("target") == item.at("target") &&
                                       version(related).satisfies(dependency.at("version").get<Text>()) &&
                                       related.at("state") == "enrolled" && related.value("condition", "") != "unsupported" &&
                                       (keyFor(related) == keyFor(root) || reaches(state, related, root, visited));
                            });
                }
            }
            return found;
        }

        Json frozen(const Json& state, const Json& item, const Selection& selection) {
            const auto& declaration = state.at("declarations").at(item.at("repo").get<Text>());
            return {{"package", item.at("package")},
                    {"repo", item.at("repo")},
                    {"commit", item.at("commit")},
                    {"ci_commit", declaration.at("ci_commit")},
                    {"version", item.at("version")},
                    {"revision", item.at("revision")},
                    {"target", state.at("catalog").at(item.at("target").get<Text>())},
                    {"feed_commit", state.at("head")},
                    {"closure", selection.closure},
                    {"contract", "Xv2"}};
        }

        Text buildIdentity(const Json& state, const Json& item, const Selection& selection) {
            const auto& declaration = state.at("declarations").at(item.at("repo").get<Text>());
            Json identities = Json::array();
            for (const auto& binding : selection.closure) {
                identities.push_back(binding.at("identity"));
            }
            const auto hash = Json::array(
                {item.at("commit"), declaration.at("tree"), state.at("catalog").at(item.at("target").get<Text>()), "Xv2", identities});
            return sha256(hash.dump());
        }

        void updateDesired(Json& state) {
            for (auto& item : state["items"]) {
                if (eligible(state, item)) {
                    const auto selection = resolve(state, item);
                    if (item.at("state") == "enrolled") {
                        item["condition"] = selection.condition;
                        item["reason"] = selection.reason;
                    }
                    if (selection.condition.empty()) {
                        const auto identity = buildIdentity(state, item, selection);
                        item["desired"] = identity;
                        if ((item.at("state") == "published" || item.at("state") == "failed") && item.value("identity", "") != identity) {
                            const auto declaration = state.at("declarations").at(item.at("repo").get<Text>());
                            const bool declarationChanged = declaration.at("tree") != item.value("tree", "");
                            const bool newest = !std::ranges::any_of(state.at("entries"), [&](const auto& output) {
                                return !output.value("legacy", false) && output.at("package") == item.at("package") &&
                                       eligibleOutput(state, output) && output.at("target").at("id") == item.at("target") &&
                                       version(output) > version(item);
                            });
                            if (declarationChanged || newest || item.at("state") == "failed") {
                                item["state"] = "enrolled";
                            }
                        }
                    } else {
                        item.erase("desired");
                    }
                }
            }
            for (auto& item : state["items"]) {
                if (eligible(state, item) && item.at("state") == "enrolled" && item.value("condition", "") == "waiting") {
                    std::set<Text> visited;
                    if (reaches(state, item, item, visited)) {
                        item["condition"] = "blocked";
                        item["reason"] = "cycle";
                    }
                }
            }
        }

        void makeBuild(Json& state, Json& item, const Selection& selection, Time now, const Options& options) {
            const auto id = keyFor(item);
            unsigned number = 1;
            Text request;
            Json inputs, bindings = selection.direct;
            if (item.value("retry", false) && !wasPublished(item) && item.at("desired") == item.at("identity")) {
                request = item.at("request").get<Text>();
                number = item.at("attempt").get<unsigned>() + 1;
                auto& previous = state["attempts"][attemptKey(request, number - 1)];
                inputs = Json::parse(previous.at("document").get<Text>()).at("inputs");
                bindings = previous.at("bindings");
                previous["fenced"] = true;
            } else {
                const auto revision = state.at("revisions").value(id, 1U);
                state["revisions"][id] = revision + 1;
                item["revision"] = revision;
                inputs = frozen(state, item, selection);
                state["next"] = state.at("next").get<unsigned>() + 1;
                const bool arm = inputs.at("target").at("runner") == "ubuntu-24.04-arm";
                request = (arm ? "arm." : "b.") + std::to_string(state.at("next").get<unsigned>());
                item["tree"] = state.at("declarations").at(item.at("repo").get<Text>()).at("tree");
                item["identity"] = item.at("desired");
            }
            const Json document{{"request", request}, {"attempt", number}, {"inputs", inputs}};
            auto record = attempt(request, number, false, document.dump(), now, options);
            record["item"] = id;
            record["pin"] = inputs.at("feed_commit");
            record["bindings"] = bindings;
            item["request"] = request;
            item["attempt"] = number;
            item["state"] = "processing";
            item.erase("condition");
            item.erase("retry");
            state["attempts"][attemptKey(request, number)] = std::move(record);
        }

        bool publicationWork(const Json& state, Time now) {
            const bool retirement = state.value("retirement_at", Time{0}) > 0 && state.value("retirement_at", Time{0}) <= now &&
                                    !state.value("legacy_retired", false);
            return retirement || std::ranges::any_of(state.at("items"), [&](const auto& item) {
                       return item.at("state") == "built" && eligible(state, item) && !item.contains("publication");
                   });
        }

        void reservePublication(Json& state, Time now, const Options& options) {
            const auto serial = state.at("next").get<unsigned>() + 1;
            const auto id = "p." + std::to_string(serial);
            state["next"] = serial;
            auto record = attempt(id, 1, true, "", now, options);
            record["dispatch"] = "batching";
            record["batch_at"] = now + options.batch;
            state["attempts"][attemptKey(id, 1)] = std::move(record);
        }

        void freezePublication(Json& state, Json& record, Time now, const Options& options) {
            Json items = Json::array(), deletion = Json::array();
            const auto number = record.at("attempt").get<unsigned>();
            const auto id = record.at("id").get<Text>();
            for (auto& item : state["items"]) {
                if (item.at("state") == "built" && eligible(state, item) && !item.contains("publication")) {
                    const auto& source =
                        state.at("attempts").at(attemptKey(item.at("request").get<Text>(), item.at("attempt").get<unsigned>()));
                    const auto frozen = Json::parse(source.at("document").get<Text>()).at("inputs");
                    Json entry = source.at("report");
                    for (const auto* key : {"package", "version", "revision", "target", "repo", "commit", "ci_commit", "closure"}) {
                        entry[key] = frozen.at(key);
                    }
                    entry["bindings"] = source.at("bindings");
                    entry["request"] = item.at("request");
                    entry["attempt"] = item.at("attempt");
                    entry["run_id"] = source.at("run");
                    entry["identity"] = item.at("identity");
                    items.push_back(std::move(entry));
                    item["publication"] = id;
                }
            }
            const auto retire = retirements(state.at("entries"), now, items);
            record["retiring"] = Json::object();
            for (const auto& item : retire) {
                const Text key = item[0].get<Text>() + "/" + item[1].get<Text>() + "/" + std::to_string(item[2].get<unsigned>()) + "/" +
                                 item[3].get<Text>();
                record["retiring"][key] = true;
            }
            std::set<Text> pins{state.at("head").get<Text>()};
            for (const auto& candidate : state.at("attempts")) {
                if (candidate.contains("pin")) {
                    const auto& item = state.at("items").at(candidate.at("item").get<Text>());
                    if (reserved(candidate) ||
                        (item.at("request") == candidate.at("id") &&
                         (item.at("state") == "built" || item.value("retry", false) ||
                          (item.at("state") == "failed" && item.value("identity", "") == item.value("desired", ""))))) {
                        pins.insert(candidate.at("pin").get<Text>());
                    }
                }
            }
            for (const auto& [ref, commit] : state.at("snapshots").items()) {
                if (!ref.ends_with("/pre-x") && !pins.contains(commit.get<Text>()) &&
                    std::ranges::find(state.at("rollback"), Json(ref)) == state.at("rollback").end()) {
                    deletion.push_back(ref);
                }
            }
            const bool fault = !options.fault.empty() && state["faults"].emplace(options.fault, true).second;
            const bool legacy = state.value("retirement_at", Time{0}) > 0 && state.value("retirement_at", Time{0}) <= now &&
                                !state.value("legacy_retired", false);
            const Json plan{{"publication", id},
                            {"attempt", number},
                            {"feed_branch", options.branch},
                            {"expected_head", state.at("head")},
                            {"items", items},
                            {"retirements", retire},
                            {"retire_legacy_apt", legacy},
                            {"delete_snapshots", deletion},
                            {"deadline", now + options.queue + options.execution},
                            {"acknowledged_receipts", state.at("acks")},
                            {"legacy_dates", state.at("legacy_dates")},
                            {"fault", fault ? "after_push" : "none"}};
            record["document"] = plan.dump();
            record["dispatch"] = "pending";
            record["queue_at"] = now + options.queue;
        }
    } // namespace

    Json initialState() {
        Json state = {{"pass", 0},
                      {"next", 0},
                      {"paused", false},
                      {"initialized", false},
                      {"discovery_ready", false},
                      {"head", ""},
                      {"head_changed_at", 0},
                      {"observed_sequence", 0},
                      {"feed_status", ""},
                      {"legacy_retired", false},
                      {"retirement_at", 0},
                      {"cutoff_open", false}};
        for (const auto* name :
             {"catalog", "declarations", "releases", "items", "attempts", "revisions", "snapshots", "faults", "legacy_dates"}) {
            state[name] = Json::object();
        }
        for (const auto* name : {"entries", "rollback", "acks"}) {
            state[name] = Json::array();
        }
        return state;
    }

    std::string sha256(const std::string& bytes) {
        static constexpr char digits[] = "0123456789abcdef";
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), hash);
        std::string result;
        for (const auto byte : hash) {
            result += digits[byte >> 4];
            result += digits[byte & 15];
        }
        return result;
    }

    std::string attemptKey(const std::string& id, unsigned number) {
        return id + "/" + std::to_string(number);
    }

    bool stableTag(const std::string& tag) {
        const auto parsed = Version::parse(tag);
        return parsed && tag.starts_with('v') && tag.find_first_of("-+") == std::string::npos;
    }

    bool reserved(const Json& attempt) {
        return attempt.at("run_state") != "completed" && attempt.at("run_state") != "superseded";
    }

    std::optional<unsigned> feedSequence(const Json& ledger) {
        std::optional<unsigned> sequence = ledger.is_array() ? std::optional{0U} : std::nullopt;
        if (ledger.contains("publication") && ledger.at("publication").is_string()) {
            const auto id = ledger.at("publication").get<Text>();
            unsigned number = 0;
            if (id.starts_with("p.")) {
                const auto parsed = std::from_chars(id.data() + 2, id.data() + id.size(), number);
                if (parsed.ec == std::errc{} && parsed.ptr == id.data() + id.size() && number > 0) {
                    sequence = number;
                }
            }
        }
        return sequence;
    }

    Scheduler::Scheduler(Options options)
        : config(std::move(options)) {
    }

    const Options& Scheduler::options() const {
        return config;
    }

    Result Scheduler::admit(Json& state, const std::string& repo, const std::string& tag, const std::string& commit) const {
        Result result;
        const auto key = repo + ":" + tag;
        if (!stableTag(tag) || !state.at("declarations").contains(repo)) {
            result = {400, "invalid release or repository"};
        } else if (state.at("releases").contains(key)) {
            if (state.at("releases").at(key).at("commit") != commit) {
                result = {409, "retag conflict"};
            }
        } else {
            const auto order = state.at("next").get<unsigned>() + 1;
            state["next"] = order;
            const Json release{{"repo", repo},
                               {"tag", tag},
                               {"version", tag.substr(1)},
                               {"commit", commit},
                               {"order", order},
                               {"eligible", eligibility(state)}};
            state["releases"][key] = release;
            enroll(state, release);
        }
        return result;
    }

    Result Scheduler::declarations(Json& state, const Json& catalog, const Json& declarations, const Json& releases) const {
        state["discovery_ready"] = true;
        Json effective = declarations;
        for (auto& [repo, declaration] : effective.items()) {
            const auto& published = state.at("declarations");
            if (published.contains(repo) && published.at(repo).at("tree") == declaration.at("tree")) {
                declaration["ci_commit"] = published.at(repo).at("ci_commit");
            }
        }
        if (state.at("catalog") != catalog || state.at("declarations") != effective) {
            for (auto& item : state["items"]) {
                const auto repo = item.at("repo").get<Text>();
                const auto target = item.at("target").get<Text>();
                if (state.at("declarations").value(repo, Json{}) != effective.value(repo, Json{}) ||
                    state.at("catalog").value(target, Json{}) != catalog.value(target, Json{})) {
                    item["eligible"] = eligibility(state);
                }
            }
            state["catalog"] = catalog;
            state["declarations"] = effective;
            for (const auto& release : state.at("releases")) {
                if (declarations.contains(release.at("repo").get<Text>())) {
                    enroll(state, release);
                }
            }
        }
        Result result;
        for (const auto& [repo, _] : effective.items()) {
            std::optional<Version> previous;
            for (const auto& existing : state.at("releases")) {
                if (existing.at("repo") == repo && (!previous || version(existing) > *previous)) {
                    previous = version(existing);
                }
            }
            std::vector<const Json*> candidates;
            for (const auto& release : releases) {
                const auto tag = release.at("tag").get<Text>();
                if (release.at("repo") == repo &&
                    (state.at("releases").contains(repo + ":" + tag) || !previous || Version::parse(tag).value() > *previous)) {
                    candidates.push_back(&release);
                }
            }
            std::sort(candidates.begin(), candidates.end(), [](const Json* a, const Json* b) {
                return Version::parse(a->at("tag").get<Text>()).value() < Version::parse(b->at("tag").get<Text>()).value();
            });
            for (const auto* release : candidates) {
                if (previous || release == candidates.back()) {
                    const auto admitted = admit(state, repo, release->at("tag").get<Text>(), release->at("commit").get<Text>());
                    if (admitted.code != 200) {
                        result = admitted;
                    }
                }
            }
        }
        return result;
    }

    Result Scheduler::action(Json& state, const std::string& name, const Json& body) const {
        Result result;
        if (name == "dispatch-paused" && body.contains("paused") && body.at("paused").is_boolean()) {
            state["paused"] = body.at("paused");
            if (state.at("paused") == false && state.at("feed_status").get<Text>().starts_with("restored to ")) {
                state["feed_status"] = "";
            }
        } else if (name == "retry" && body.contains("item") && body.at("item").is_string() &&
                   state.at("items").contains(body.at("item").get<Text>())) {
            auto& item = state["items"][body.at("item").get<Text>()];
            const auto selection = resolve(state, item);
            if (item.at("state") == "failed" && selection.condition.empty() &&
                buildIdentity(state, item, selection) == item.at("identity").get<Text>()) {
                item["state"] = wasPublished(item) ? "built" : "enrolled";
                item["retry"] = !wasPublished(item);
                item["eligible"] = eligibility(state);
            } else {
                result = {409, "item is not eligible for retry"};
            }
        } else {
            result = {400, "invalid dashboard action"};
        }
        return result;
    }

    Result Scheduler::turn(Json& state, Time now) const {
        for (auto& record : state["attempts"]) {
            if (record.at("publication") == false && record.at("run_state") == "completed") {
                finalizeBuild(state, record, now, config);
            }
        }
        if (!state.value("cutoff_open", false) && std::ranges::any_of(state.at("items"), [&](const auto& item) {
                return !eligible(state, item);
            })) {
            state["pass"] = eligibility(state);
            state["cutoff_open"] = true;
        } else {
            updateDesired(state);
            if (state.at("paused") == false && state.at("feed_status") == "" &&
                std::ranges::any_of(state.at("snapshots"), [&](const Json& commit) {
                    return commit == state.at("head");
                })) {
                auto& attempts = state["attempts"];
                auto used = static_cast<unsigned>(std::ranges::count_if(attempts, reserved));
                const auto publication = std::ranges::find_if(attempts, currentPublication);
                if (used < config.slots && publication == attempts.end() && publicationWork(state, now)) {
                    reservePublication(state, now, config);
                    ++used;
                } else if (used < config.slots && publication != attempts.end() && !reserved(*publication) &&
                           publication->contains("retry_at")) {
                    replaceAttempt(state, *publication, now, config);
                    ++used;
                }
                for (auto& record : state["attempts"]) {
                    if (record.at("dispatch") == "batching" && record.at("batch_at").get<Time>() <= now) {
                        freezePublication(state, record, now, config);
                        if (!record.at("retiring").empty()) {
                            updateDesired(state);
                        }
                    }
                }
                std::vector<Json*> ready;
                for (auto& item : state["items"]) {
                    if (eligible(state, item) && item.at("state") == "enrolled" && item.value("condition", "").empty() &&
                        item.contains("desired")) {
                        ready.push_back(&item);
                    }
                }
                std::stable_sort(ready.begin(), ready.end(), [&](const auto& a, const auto& b) {
                    return a->at("order") < b->at("order");
                });
                for (std::size_t i = 0; i < ready.size() && used < config.slots; ++i, ++used) {
                    makeBuild(state, *ready[i], resolve(state, *ready[i]), now, config);
                }
            }
            state["cutoff_open"] = false;
        }
        return {};
    }

    namespace {
        void failPublication(Json& state, Json& record, const Text& reason) {
            for (auto& item : state["items"]) {
                if (item.contains("publication") && item.at("publication") == record.at("id")) {
                    item["state"] = "failed";
                    item["reason"] = reason;
                    item["eligible"] = eligibility(state);
                    item.erase("publication");
                }
            }
            record["reconciled"] = true;
        }

        void finalizeBuild(Json& state, Json& record, Time now, const Options& options) {
            if (!record.value("reconciled", false) && record.at("run_state") == "completed") {
                if (!record.contains("report") && !record.contains("grace_at")) {
                    record["grace_at"] = record.at("completed_at").get<Time>() + options.grace;
                }
                if (record.contains("report") || record.at("grace_at").get<Time>() <= now) {
                    auto& item = state["items"][record.at("item").get<Text>()];
                    item["eligible"] = eligibility(state);
                    const auto report = record.value("report", Json::object());
                    item["state"] = report.value("outcome", "failed");
                    item["reason"] =
                        report.empty() ? "conclusion " + record.at("conclusion").get<Text>() + ", no report" : report.value("reason", "");
                    record["reconciled"] = true;
                }
            }
        }

        void replaceAttempt(Json& state, Json& old, Time now, const Options& options) {
            auto document = Json::parse(old.at("document").get<Text>());
            const auto id = old.at("id").get<Text>();
            const auto number = old.at("attempt").get<unsigned>() + 1;
            const bool publication = old.at("publication").get<bool>();
            const auto queueStart = std::max(now, old.value("retry_at", now));
            document["attempt"] = number;
            if (publication) {
                document["fault"] = "none";
                document["deadline"] = queueStart + options.queue + options.execution;
            }
            auto next = attempt(id, number, publication, document.dump(), queueStart, options);
            for (const auto* field : {"item", "pin", "bindings", "retiring", "failures"}) {
                if (old.contains(field)) {
                    next[field] = old.at(field);
                }
            }
            if (!publication) {
                state["items"][old.at("item").get<Text>()]["attempt"] = number;
            } else if (old.at("run_state") == "completed") {
                next["failures"] = old.value("failures", 0U) + 1;
                next["retry_at"] = old.at("retry_at");
            }
            old["reconciled"] = true;
            old["fenced"] = true;
            state["attempts"][attemptKey(id, number)] = std::move(next);
        }

        bool sameOwner(const Json& attempt, const Json& owner) {
            return owner.at("run_attempt") == 1 && owner.at("run_id") == attempt.at("run");
        }
    } // namespace

    Result Scheduler::claim(Json& state, bool publication, const std::string& id, const Json& owner, Time now) const {
        Result result{409, "stale attempt or foreign run"};
        const auto key = attemptKey(id, owner.at("attempt").get<unsigned>());
        if (state.at("attempts").contains(key)) {
            auto& record = state["attempts"][key];
            const auto phase = record.at("run_state");
            const bool unclaimed = phase == "dispatched" && record.at("dispatch") != "batching";
            const bool known = record.at("run") != 0;
            const bool current = record.at("publication") == true ||
                                 (state.at("items").at(record.at("item").get<Text>()).at("request") == id &&
                                  state.at("items").at(record.at("item").get<Text>()).at("attempt") == owner.at("attempt"));
            if (record.at("publication") == publication && owner.at("run_attempt") == 1 && owner.at("run_id") != 0 && current &&
                !record.value("fenced", false) && phase != "superseded" && (unclaimed || record.contains("claimed_at")) &&
                (!known || sameOwner(record, owner))) {
                if (unclaimed && !record.contains("claimed_at")) {
                    record["run"] = owner.at("run_id");
                    record["run_state"] = "claimed";
                    record["dispatch"] = "sent";
                    record["claimed_at"] = now;
                    record["execute_at"] = now + config.execution;
                }
                Json grant = owner;
                grant["id"] = id;
                grant["granted"] = true;
                grant["document_sha256"] = sha256(record.at("document").get<Text>());
                result = {200, grant.dump()};
            }
        }
        return result;
    }

    Result Scheduler::report(Json& state, bool publication, const std::string& id, const Json& report, Time now) const {
        const auto key = attemptKey(id, report.at("attempt").get<unsigned>());
        Result result{409, "unknown report owner"};
        if (state.at("attempts").contains(key)) {
            auto& record = state["attempts"][key];
            if (record.at("publication") == publication && record.contains("claimed_at") && !record.value("fenced", false) &&
                sameOwner(record, report)) {
                const auto status = report.at("status").get<Text>();
                if (!record.value("reconciled", false)) {
                    if (status == "started" && !record.contains("started_at")) {
                        record["started_at"] = now;
                    } else if (status == "finished" && !record.contains("report")) {
                        record["report"] = report;
                        if (!publication) {
                            finalizeBuild(state, record, now, config);
                        }
                    } else if (publication && status == "item") {
                        const auto& receipt = report.at("receipt");
                        record["reports"][receipt.at("request").get<Text>()] = receipt;
                    }
                }
                result = {};
            }
        }
        return result;
    }

    Result Scheduler::dispatchFailed(Json& state, const std::string& key, int status, Time now) const {
        auto& record = state.at("attempts").at(key);
        if (record.at("run_state") == "dispatched" && record.at("dispatch") == "sent" && record.at("run") == 0) {
            const auto failures = record.value("dispatch_failures", 0U) + 1;
            const bool rejected = status >= 400 && status < 500;
            const bool uncertain = !rejected || record.value("dispatch_uncertain", false);
            record["dispatch_failures"] = failures;
            record["dispatch_uncertain"] = uncertain;
            if (failures <= 3) {
                record["dispatch"] = "pending";
                record["retry_at"] = now + (1 << (failures - 1));
            } else if (!uncertain) {
                const auto reason = "dispatch rejected " + std::to_string(status);
                record["run_state"] = "completed";
                record["completed_at"] = now;
                record["fenced"] = true;
                record["report"] = {{"outcome", "failed"}, {"reason", reason}};
                if (record.at("publication") == true) {
                    failPublication(state, record, reason);
                } else {
                    finalizeBuild(state, record, now, config);
                }
            }
        }
        return {};
    }

    Result Scheduler::observed(Json& state, const std::string& key, const Json& run, Time now) const {
        if (state.at("attempts").contains(key)) {
            auto& record = state["attempts"][key];
            const bool publication = record.at("publication").get<bool>();
            record["poll_at"] = now + config.poll;
            if (run.contains("id") && (record.at("run") == 0 || record.at("run") == run.at("id"))) {
                record["run"] = run.at("id");
                record["dispatch"] = "sent";
                if (run.contains("queue_seconds")) {
                    record["github_queue"] = run.at("queue_seconds");
                }
                if (record.at("run_state") == "superseded") {
                    record["superseded_completed"] = run.at("status") == "completed";
                } else if (record.at("run_state") != "completed" && run.at("status") == "completed") {
                    record["run_state"] = "completed";
                    record.erase("retry_at");
                    record["completed_at"] = now;
                    record["conclusion"] = run.at("conclusion");
                    if (!publication) {
                        finalizeBuild(state, record, now, config);
                    }
                } else if (reserved(record) && now >= record.value("execute_at", record.at("queue_at").get<Time>())) {
                    record["cancel_requested"] = true;
                }
            } else if (run.value("not_found", false) && reserved(record) && record.at("run") == 0 && !record.contains("claimed_at") &&
                       record.at("queue_at").get<Time>() <= now) {
                record["run_state"] = "superseded";
                replaceAttempt(state, record, now, config);
            }
        }
        return {};
    }

    namespace {
        void restoreOutputs(Json& state, const Json& entries) {
            for (auto& item : state["items"]) {
                if (wasPublished(item)) {
                    const auto present = [&](const Json& output) {
                        return keyFor(output) == keyFor(item) && output.at("revision") == item.at("revision") &&
                               output.value("identity", "") == item.at("identity").get<Text>();
                    };
                    const bool available = std::ranges::any_of(entries, present);
                    if (item.at("state") == "published" && !available && std::ranges::any_of(state.at("entries"), present)) {
                        item["state"] = "built";
                        item["eligible"] = eligibility(state);
                    } else if (item.at("state") == "built" && available && !item.contains("publication")) {
                        item["state"] = "published";
                        item["reason"] = "already published";
                        item["eligible"] = item["output_eligible"] = eligibility(state);
                    }
                }
            }
        }

        void reconcileFeed(Json& state, const Json& snapshots, const Json& ledger, Time now, const Options& config) {
            const auto receipts = ledger.is_object() ? ledger.at("receipts") : Json::array();
            state["acks"] = Json::array();
            for (const auto& receipt : receipts) {
                const auto key = attemptKey(receipt.at("publication").get<Text>(), receipt.at("attempt").get<unsigned>());
                if (state.at("attempts").contains(key) && state.at("attempts").at(key).value("reconciled", false)) {
                    state["acks"].push_back(Json::array({receipt.at("publication"), receipt.at("attempt"), receipt.at("request")}));
                }
            }
            for (auto& record : state["attempts"]) {
                if (record.at("publication") == true && !record.value("reconciled", false) && record.at("document") != "") {
                    const auto plan = Json::parse(record.at("document").get<Text>());
                    const Text ref = "refs/snapshots/" + config.branch + "/" + record.at("id").get<Text>();
                    if (snapshots.contains(ref)) {
                        for (const auto& itemPlan : plan.at("items")) {
                            auto& item = state["items"][keyFor(itemPlan)];
                            item["eligible"] = eligibility(state);
                            const auto receipt = std::ranges::find_if(receipts, [&](const auto& value) {
                                return value.at("publication") == record.at("id") && value.at("attempt") == record.at("attempt") &&
                                       value.at("request") == itemPlan.at("request");
                            });
                            if (receipt != receipts.end()) {
                                item["state"] = receipt->at("outcome");
                                item["reason"] = receipt->value("reason", "");
                                if (receipt->value("failure", "") == "artifact-expired") {
                                    item["state"] = "enrolled";
                                    item["retry"] = !wasPublished(item);
                                } else if (item.at("state") == "published") {
                                    item["output_eligible"] = item.at("eligible");
                                    item["published_revision"] = item.at("revision");
                                }
                                state["acks"].push_back(
                                    Json::array({receipt->at("publication"), receipt->at("attempt"), receipt->at("request")}));
                            } else {
                                item["state"] = "failed";
                                item["reason"] = "publication receipt is missing";
                            }
                            item.erase("publication");
                            if (record.contains("started_at") && record.contains("completed_at")) {
                                item["publication_seconds"] = record.at("completed_at").get<Time>() - record.at("started_at").get<Time>();
                            }
                        }
                        if (plan.at("retire_legacy_apt") == true) {
                            state["legacy_retired"] = true;
                        }
                        state["rollback"].push_back(ref);
                        if (state["rollback"].size() > 3) {
                            state["rollback"].erase(state["rollback"].begin());
                        }
                        record["reconciled"] = true;
                    } else if (record.at("run_state") == "completed" &&
                               (record.contains("report") || record.at("completed_at").get<Time>() + config.grace <= now)) {
                        const auto failures = record.value("failures", 0U) + 1;
                        if (failures <= 3) {
                            if (!record.contains("retry_at")) {
                                record["retry_at"] = now + 60 * (1 << (failures - 1));
                            }
                        } else {
                            failPublication(state, record, record.value("report", Json::object()).value("reason", "publication failed"));
                        }
                    }
                }
            }
        }
    } // namespace

    Result Scheduler::feed(Json& state,
                           const std::string& head,
                           const Json& snapshots,
                           const Json& ledger,
                           Time now,
                           const std::optional<RestoreProof>& restore) const {
        Result result;
        const auto sequence = feedSequence(ledger);
        const bool changed = head != state.at("head").get_ref<const Text&>();
        const bool restored = restore && changed && restore->completed > state.at("head_changed_at").get<Time>();
        const auto observed = state.at("observed_sequence").get<unsigned>();
        const bool behind = sequence && *sequence < observed;
        const bool pinned = std::ranges::any_of(snapshots, [&](const Json& commit) {
            return commit.get_ref<const Text&>() == head;
        });
        if (!sequence || (!restored && (behind || !pinned))) {
            const auto reason =
                sequence ? (behind ? "feed head behind: " : "feed snapshots pending: ") + head : "invalid feed publication sequence";
            state["observed_sequence"] = std::max(observed, sequence.value_or(0U));
            state["feed_status"] = reason;
            result = {sequence ? 409 : 400, reason};
        } else {
            const auto entries = ledger.is_object() ? ledger.at("entries") : Json::array();
            if (restored) {
                restoreOutputs(state, entries);
                state["paused"] = true;
                state["feed_status"] = "restored to " + restore->snapshot;
            } else if (!state.at("feed_status").get<Text>().starts_with("restored to ")) {
                state["feed_status"] = "";
            }
            if (changed) {
                state["head_changed_at"] = now;
                state["head"] = head;
            }
            state["observed_sequence"] = *sequence;
            state["snapshots"] = snapshots;
            state["entries"] = entries;
            reconcileFeed(state, snapshots, ledger, now, config);
        }
        return result;
    }

    Json view(const Json& state, const Options& options) {
        Json items = Json::array(), publications = Json::array();
        Json reservations{{"slots", options.slots}, {"queued", 0}, {"running", 0}, {"publication", 0}};
        for (const auto& record : state.at("attempts")) {
            if (reserved(record)) {
                const auto* kind =
                    record.at("publication") == true ? "publication" : (record.at("run_state") == "claimed" ? "running" : "queued");
                reservations[kind] = reservations[kind].get<unsigned>() + 1;
            }
            if (record.at("publication") == true) {
                publications.push_back({{"id", record.at("id")},
                                        {"attempt", record.at("attempt")},
                                        {"run", record.at("run")},
                                        {"state", record.at("run_state")},
                                        {"reconciled", record.at("reconciled")}});
            }
        }
        for (const auto& [key, stored] : state.at("items").items()) {
            Json item = stored;
            item["id"] = key;
            item["retryable"] =
                stored.at("state") == "failed" && stored.contains("desired") && stored.at("desired") == stored.at("identity");
            if (stored.contains("request")) {
                const auto& record =
                    state.at("attempts").at(attemptKey(stored.at("request").get<Text>(), stored.at("attempt").get<unsigned>()));
                const auto inputs = Json::parse(record.at("document").get<Text>()).at("inputs");
                item["bindings"] = inputs.at("closure");
                item["feed_commit"] = inputs.at("feed_commit");
                item["ci_commit"] = inputs.at("ci_commit");
                item["run"] = record.at("run");
                if (record.contains("github_queue")) {
                    item["github_queue"] = record.at("github_queue");
                }
                if (record.contains("report") && record.at("report").contains("timings")) {
                    item["timings"] = record.at("report").at("timings");
                }
                if (stored.contains("publication_seconds")) {
                    item["timings"]["publication"] = stored.at("publication_seconds");
                }
            }
            items.push_back(std::move(item));
        }
        return {{"pass", state.at("pass")},
                {"paused", state.at("paused")},
                {"ready", state.at("head") != "" && state.at("discovery_ready") == true},
                {"workflow_repository", options.workflowRepository},
                {"feed_commit", state.at("head")},
                {"feed_status", state.at("feed_status")},
                {"items", items},
                {"publications", publications},
                {"reservations", reservations}};
    }
} // namespace coreci
