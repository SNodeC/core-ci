#include "StateSql.h"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace coreci::persistence {
    namespace {
        Field value(const std::string& text) {
            return text;
        }
        Field value(const Version& version) {
            return version.text();
        }
        template <typename T>
        Field value(T number) {
            return std::to_string(number);
        }
        template <typename T>
        Field value(const std::optional<T>& item) {
            return item ? value(*item) : Field{};
        }
        const std::string& text(const Field& field) {
            if (!field || field->find('\0') != std::string::npos) {
                throw std::invalid_argument("NULL or NUL in required state field");
            }
            return *field;
        }
        template <typename T = std::uint64_t>
        T number(const Field& field) {
            const auto& input = text(field);
            T result{};
            const auto parsed = std::from_chars(input.data(), input.data() + input.size(), result);
            if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size()) {
                throw std::invalid_argument("invalid state number");
            }
            return result;
        }
        template <typename T>
        std::optional<T> optional(const Field& field) {
            return field ? std::optional<T>{number<T>(field)} : std::nullopt;
        }
        void validateNumber(const Field& field) {
            if (text(field).starts_with('-')) {
                number<std::int64_t>(field);
            } else {
                number<std::uint64_t>(field);
            }
        }
        Version version(const Field& field) {
            const auto parsed = Version::parse(text(field));
            if (!parsed) {
                throw std::invalid_argument("invalid stored version");
            }
            return *parsed;
        }
        State status(const Field& field) {
            State result = State::Idle;
            bool found = false;
            for (unsigned index = 0; index <= static_cast<unsigned>(State::Publishing); ++index) {
                if (stateName(static_cast<State>(index)) == text(field)) {
                    result = static_cast<State>(index);
                    found = true;
                }
            }
            if (!found) {
                throw std::invalid_argument("invalid stored unit status");
            }
            return result;
        }
        Wave run(const Row& r) {
            Wave result;
            result.id = text(r.at(0));
            result.feedBranch = text(r.at(1));
            result.workflow = {text(r.at(2)), text(r.at(3)), text(r.at(4))};
            const auto phase = text(r.at(5));
            if (phase != "Dispatching" && phase != "Running" && phase != "Finished") {
                throw std::invalid_argument("invalid run phase");
            }
            result.phase =
                phase == "Dispatching" ? WavePhase::Dispatching : (phase == "Running" ? WavePhase::Running : WavePhase::Finished);
            result.runId = optional<std::uint64_t>(r.at(6));
            result.runAttempt = optional<unsigned>(r.at(7));
            result.attempt = number<unsigned>(r.at(8));
            result.retryAt = optional<std::int64_t>(r.at(9));
            result.dispatchedAt = number<std::int64_t>(r.at(10));
            result.deadline = number<std::int64_t>(r.at(11));
            result.startDeadline = number<std::int64_t>(r.at(12));
            result.dispatchAccepted = number<unsigned>(r.at(13)) != 0;
            result.runFinished = number<unsigned>(r.at(14)) != 0;
            result.expired = number<unsigned>(r.at(15)) != 0;
            result.started = optional<std::int64_t>(r.at(16));
            result.finished = optional<std::int64_t>(r.at(17));
            result.conclusion = text(r.at(18));
            if (result.attempt == 0 || result.runId.has_value() != result.runAttempt.has_value() || (result.runId && *result.runId == 0)) {
                throw std::invalid_argument("invalid stored run ownership");
            }
            return result;
        }
        template <class... T>
        Row row(const T&... fields) {
            return {value(fields)...};
        }
        Row runRow(const Wave& run) {
            return row(run.id,
                       run.feedBranch,
                       run.workflow.owner,
                       run.workflow.repo,
                       run.workflow.branch,
                       phaseName(run.phase),
                       run.runId,
                       run.runAttempt,
                       run.attempt,
                       run.retryAt,
                       run.dispatchedAt,
                       run.deadline,
                       run.startDeadline,
                       run.dispatchAccepted,
                       run.runFinished,
                       run.expired,
                       run.started,
                       run.finished,
                       run.conclusion);
        }
        std::string literal(const Field& field, bool numeric) {
            std::string result = "NULL";
            if (field) {
                if (numeric) {
                    validateNumber(field);
                    result = *field;
                } else {
                    result = sqlText(*field);
                }
            }
            return result;
        }
        std::string keyWhere(const Table& table, const Row& row) {
            std::string result;
            for (std::size_t index = 0; index < table.keys; ++index) {
                result +=
                    (index == 0 ? "" : " AND ") + table.columns[index].name + "=" + literal(row.at(index), table.columns[index].numeric);
            }
            return result;
        }
    } // namespace
    const std::vector<Table>& stateTables() {
        static const std::vector<Column> runColumns = {{"id", false},
                                                       {"feed_branch", false},
                                                       {"owner", false},
                                                       {"repo", false},
                                                       {"branch", false},
                                                       {"phase", false},
                                                       {"owner_run_id", true},
                                                       {"run_attempt", true},
                                                       {"attempt", true},
                                                       {"retry_at", true},
                                                       {"dispatched_at", true},
                                                       {"deadline", true},
                                                       {"start_deadline", true},
                                                       {"accepted", true},
                                                       {"run_finished", true},
                                                       {"expired", true},
                                                       {"started", true},
                                                       {"finished", true},
                                                       {"conclusion", false}};
        static const std::vector<Table> tables{
            {"coreci_targets", 1, {{"target", false}}},
            {"coreci_target_builds", 3, {{"wave_id", false}, {"target", false}, {"build_id", false}, {"succeeded", true}}},
            {"coreci_state", 1, {{"singleton", true}, {"sequence", true}, {"clock", true}, {"quiet_until", true}, {"latest", false}}},
            {"coreci_packages", 1, {{"package", false}, {"owner", false}, {"repo", false}, {"branch", false}, {"revision_base", true}}},
            {"coreci_memberships", 2, {{"package", false}, {"target", false}}},
            {"coreci_prerequisites", 2, {{"package", false}, {"prerequisite", false}}},
            {"coreci_tags",
             1,
             {{"package", false},
              {"owner", false},
              {"repo", false},
              {"branch", false},
              {"version", false},
              {"tag", false},
              {"sha", false}}},
            {"coreci_units",
             2,
             {{"package", false},
              {"target", false},
              {"status", false},
              {"wave_id", false},
              {"allocation_order", true},
              {"artifact", false},
              {"build_start", true},
              {"build_end", true},
              {"publish_end", true},
              {"requested", true},
              {"retry_after", true},
              {"auto_version", false}}},
            {"coreci_published",
             2,
             {{"package", false},
              {"target", false},
              {"version", false},
              {"revision", true},
              {"owner", false},
              {"repo", false},
              {"branch", false},
              {"wave_id", false},
              {"published_at", true},
              {"feed_commit", false},
              {"run_id", true}}},
            {"coreci_failures",
             2,
             {{"package", false},
              {"target", false},
              {"version", false},
              {"reason", false},
              {"revision", true},
              {"failed_at", true},
              {"build_failed", true}}},
            {"coreci_revisions", 3, {{"package", false}, {"target", false}, {"version", false}, {"revision", true}}},
            {"coreci_inputs",
             5,
             {{"context", false},
              {"owner_id", false},
              {"package", false},
              {"target", false},
              {"prerequisite", false},
              {"version", false},
              {"revision", true}}},
            {"coreci_waves", 1, runColumns},
            {"coreci_plan",
             3,
             {{"wave_id", false},
              {"target", false},
              {"position", true},
              {"package", false},
              {"owner", false},
              {"repo", false},
              {"branch", false},
              {"version", false},
              {"tag", false},
              {"sha", false},
              {"revision", true},
              {"build_id", false},
              {"build", true}}},
            {"coreci_wave_targets",
             2,
             {{"wave_id", false},
              {"target", false},
              {"status", false},
              {"artifact", false},
              {"finished", true},
              {"available", true},
              {"publication_id", false}}},
            {"coreci_publications", 1, runColumns},
            {"coreci_publication_items", 3, {{"publication_id", false}, {"wave_id", false}, {"target", false}, {"artifact", false}}},
            {"coreci_publication_units",
             4,
             {{"publication_id", false}, {"wave_id", false}, {"target", false}, {"build_id", false}, {"succeeded", true}}}};
        return tables;
    }
    StateRows encodeState(const StoredState& s) {
        StateRows result;
        for (const auto& table : stateTables()) {
            result[table.name] = {};
        }
        const auto add = [&](const std::string& table, const auto&... fields) {
            result.at("coreci_" + table).push_back(row(fields...));
        };
        const auto inputs = [&](const std::string& context, const std::string& owner, const Unit& unit, const Inputs& values) {
            for (const auto& [name, input] : values) {
                add("inputs", context, owner, unit.package, unit.target, name, input.version, input.revision);
            }
        };
        add("state", 1, s.sequence, s.engineTime, s.quietUntil, s.latest);
        for (const auto& [id, target] : s.graph.targets()) {
            add("targets", id);
        }
        for (const auto& [name, package] : s.graph.packages()) {
            add("packages", name, package.sourceRepo.owner, package.sourceRepo.repo, package.sourceRepo.branch, package.revisionBase);
            for (const auto& target : package.targets) {
                add("memberships", name, target);
            }
            for (const auto& prerequisite : package.prerequisites) {
                add("prerequisites", name, prerequisite);
            }
        }
        for (const auto& [name, source] : s.sources) {
            const auto& release = s.released.at(name);
            add("tags",
                name,
                source.owner,
                source.repo,
                source.branch,
                release ? value(release->version) : Field{},
                release ? release->tag : Field{},
                release ? release->sourceSha : Field{});
        }
        for (const auto& [key, unit] : s.unitStates) {
            add("units",
                key.package,
                key.target,
                value(stateName(unit.state)),
                unit.waveId,
                unit.order,
                unit.artifact,
                unit.buildStart,
                unit.buildEnd,
                unit.publishEnd,
                unit.requested,
                unit.retryAfter,
                unit.autoRetried ? value(unit.autoRetried->first) : Field{});
            if (unit.published) {
                const auto& p = *unit.published;
                add("published",
                    key.package,
                    key.target,
                    p.version,
                    p.revision,
                    p.source.owner,
                    p.source.repo,
                    p.source.branch,
                    p.waveId,
                    p.at,
                    p.feedCommit,
                    p.runId);
                inputs("published", "", key, p.inputs);
            }
            if (unit.failure) {
                const auto& f = *unit.failure;
                add("failures", key.package, key.target, f.version, f.reason, f.revision, f.at, f.buildFailed);
                inputs("failure", "", key, f.inputs);
            }
            if (unit.autoRetried) {
                inputs("auto", "", key, unit.autoRetried->second);
            }
            for (const auto& [version, revision] : unit.lastRevision) {
                add("revisions", key.package, key.target, version, revision);
            }
        }
        for (const auto& [id, wave] : s.waveStates) {
            result.at("coreci_waves").push_back(runRow(wave));
            for (const auto& [target, entries] : wave.plan) {
                unsigned position = 0;
                for (const auto& entry : entries) {
                    add("plan",
                        id,
                        target,
                        position++,
                        entry.unit.package,
                        entry.repo.owner,
                        entry.repo.repo,
                        entry.repo.branch,
                        entry.release.version,
                        entry.release.tag,
                        entry.release.sourceSha,
                        entry.revision,
                        entry.buildId,
                        entry.build);
                    inputs("plan", id, entry.unit, entry.inputs);
                }
            }
        }
        for (const auto& [key, target] : s.waveTargets) {
            for (const auto& [build, succeeded] : target.builds) {
                add("target_builds", key.first, key.second, build, succeeded);
            }
            add("wave_targets",
                key.first,
                key.second,
                value(stateName(target.status)),
                target.artifact,
                target.finished,
                target.artifactAvailable,
                target.publicationId);
        }
        for (const auto& [id, publication] : s.publicationStates) {
            result.at("coreci_publications").push_back(runRow(publication.run));
            for (const auto& item : publication.items) {
                add("publication_items", id, item.waveId, item.target, item.artifact);
                for (const auto& build : item.buildIds) {
                    add("publication_units",
                        id,
                        item.waveId,
                        item.target,
                        build,
                        publication.results.contains(build) ? value(publication.results.at(build)) : Field{});
                }
            }
        }
        for (const auto& table : stateTables()) {
            auto& rows = result.at(table.name);
            std::sort(rows.begin(), rows.end(), [&](const Row& left, const Row& right) {
                bool less = false, different = false;
                for (std::size_t index = 0; index < table.keys && !different; ++index) {
                    if (left[index] != right[index]) {
                        less = table.columns[index].numeric ? number<std::int64_t>(left[index]) < number<std::int64_t>(right[index])
                                                            : left[index] < right[index];
                        different = true;
                    }
                }
                return less;
            });
        }
        return result;
    }
    StoredState decodeState(const StateRows& rows) {
        StoredState s;
        const auto& metadata = rows.at("coreci_state");
        if (metadata.size() != 1 || number(metadata[0].at(0)) != 1) {
            throw std::invalid_argument("state singleton missing or duplicated");
        }
        const auto& m = metadata[0];
        s.sequence = number(m.at(1));
        s.engineTime = optional<std::int64_t>(m.at(2));
        s.quietUntil = optional<std::int64_t>(m.at(3));
        s.latest = text(m.at(4));
        std::map<std::string, Package> packages;
        for (const auto& r : rows.at("coreci_packages")) {
            const auto name = text(r.at(0));
            packages.emplace(name, Package{name, {text(r.at(1)), text(r.at(2)), text(r.at(3))}, {}, {}, number<unsigned>(r.at(4))});
        }
        Pipeline::Builder builder;
        for (const auto& r : rows.at("coreci_targets")) {
            builder.target({text(r.at(0))});
        }
        for (const auto& r : rows.at("coreci_memberships")) {
            packages.at(text(r.at(0))).targets.insert(text(r.at(1)));
        }
        for (const auto& r : rows.at("coreci_prerequisites")) {
            packages.at(text(r.at(0))).prerequisites.push_back(text(r.at(1)));
        }
        for (const auto& [name, package] : packages) {
            builder.package(package);
        }
        s.graph = builder.build();
        for (const auto& r : rows.at("coreci_tags")) {
            const auto name = text(r.at(0));
            s.sources.emplace(name, Repo{text(r.at(1)), text(r.at(2)), text(r.at(3))});
            s.released[name] = r.at(4) ? std::optional<Released>{Released{version(r.at(4)), text(r.at(5)), text(r.at(6))}} : std::nullopt;
        }
        for (const auto& r : rows.at("coreci_units")) {
            UnitState u;
            u.state = status(r.at(2));
            u.waveId = text(r.at(3));
            u.order = number(r.at(4));
            u.artifact = text(r.at(5));
            u.buildStart = optional<std::int64_t>(r.at(6));
            u.buildEnd = optional<std::int64_t>(r.at(7));
            u.publishEnd = optional<std::int64_t>(r.at(8));
            u.requested = number<unsigned>(r.at(9)) != 0;
            u.retryAfter = optional<std::int64_t>(r.at(10));
            if (r.at(11)) {
                u.autoRetried = std::pair{version(r.at(11)), Inputs{}};
            }
            s.unitStates.emplace(Unit{text(r.at(0)), text(r.at(1))}, std::move(u));
        }
        for (const auto& r : rows.at("coreci_published")) {
            s.unitStates.at({text(r.at(0)), text(r.at(1))}).published = Published{version(r.at(2)),
                                                                                  number(r.at(3)),
                                                                                  {},
                                                                                  {text(r.at(4)), text(r.at(5)), text(r.at(6))},
                                                                                  text(r.at(7)),
                                                                                  number<std::int64_t>(r.at(8)),
                                                                                  text(r.at(9)),
                                                                                  optional<std::uint64_t>(r.at(10))};
        }
        for (const auto& r : rows.at("coreci_failures")) {
            s.unitStates.at({text(r.at(0)), text(r.at(1))}).failure = Failure{
                version(r.at(2)), {}, text(r.at(3)), number(r.at(4)), number<std::int64_t>(r.at(5)), number<unsigned>(r.at(6)) != 0};
        }
        for (const auto& r : rows.at("coreci_revisions")) {
            s.unitStates.at({text(r.at(0)), text(r.at(1))}).lastRevision.emplace(version(r.at(2)), number(r.at(3)));
        }
        for (const auto& r : rows.at("coreci_waves")) {
            auto wave = run(r);
            s.waveStates.emplace(wave.id, std::move(wave));
        }
        for (const auto& r : rows.at("coreci_plan")) {
            const Unit unit{text(r.at(3)), text(r.at(1))};
            auto& entries = s.waveStates.at(text(r.at(0))).plan[unit.target];
            if (entries.size() != number(r.at(2))) {
                throw std::invalid_argument("non-contiguous plan order");
            }
            entries.push_back({unit,
                               {text(r.at(4)), text(r.at(5)), text(r.at(6))},
                               {version(r.at(7)), text(r.at(8)), text(r.at(9))},
                               number(r.at(10)),
                               {},
                               text(r.at(11)),
                               number<unsigned>(r.at(12)) != 0});
        }
        for (const auto& r : rows.at("coreci_inputs")) {
            const auto context = text(r.at(0)), owner = text(r.at(1));
            const Unit unit{text(r.at(2)), text(r.at(3))};
            Inputs* inputs = nullptr;
            if (context == "plan") {
                auto& entries = s.waveStates.at(owner).plan.at(unit.target);
                const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& p) {
                    return p.unit == unit;
                });
                if (entry != entries.end()) {
                    inputs = &entry->inputs;
                }
            } else {
                auto& state = s.unitStates.at(unit);
                if (context == "published" && state.published) {
                    inputs = &state.published->inputs;
                } else if (context == "failure" && state.failure) {
                    inputs = &state.failure->inputs;
                } else if (context == "auto" && state.autoRetried) {
                    inputs = &state.autoRetried->second;
                }
            }
            if (!inputs || !inputs->emplace(text(r.at(4)), Input{version(r.at(5)), number(r.at(6))}).second) {
                throw std::invalid_argument("orphan or duplicate input");
            }
        }
        for (const auto& r : rows.at("coreci_wave_targets")) {
            s.waveTargets.emplace(
                std::pair{text(r.at(0)), text(r.at(1))},
                WaveTarget{status(r.at(2)), number<unsigned>(r.at(4)) != 0, number<unsigned>(r.at(5)) != 0, text(r.at(3)), text(r.at(6))});
        }
        for (const auto& r : rows.at("coreci_publications")) {
            auto p = run(r);
            const auto id = p.id;
            s.publicationStates.emplace(id, Publication{std::move(p), {}});
        }
        for (const auto& r : rows.at("coreci_target_builds")) {
            s.waveTargets.at({text(r.at(0)), text(r.at(1))}).builds.emplace(text(r.at(2)), number<unsigned>(r.at(3)) != 0);
        }
        for (const auto& r : rows.at("coreci_publication_items")) {
            s.publicationStates.at(text(r.at(0))).items.push_back({text(r.at(1)), text(r.at(2)), text(r.at(3)), {}});
        }
        for (const auto& r : rows.at("coreci_publication_units")) {
            auto& items = s.publicationStates.at(text(r.at(0))).items;
            const auto item = std::find_if(items.begin(), items.end(), [&](const auto& i) {
                return i.waveId == text(r.at(1)) && i.target == text(r.at(2));
            });
            if (item == items.end()) {
                throw std::invalid_argument("orphan publication unit");
            }
            item->buildIds.push_back(text(r.at(3)));
            if (r.at(4)) {
                s.publicationStates.at(text(r.at(0))).results.emplace(text(r.at(3)), number<unsigned>(r.at(4)) != 0);
            }
        }
        if ((!s.latest.empty() && !s.waveStates.contains(s.latest)) || encodeState(s) != rows) {
            throw std::invalid_argument("incomplete or noncanonical state");
        }
        return s;
    }
    std::string sqlText(const std::string& text) {
        constexpr char hex[] = "0123456789ABCDEF";
        std::string result = "_utf8mb4 X'";
        for (const unsigned char c : text) {
            result += hex[c >> 4];
            result += hex[c & 15];
        }
        return result + "'";
    }
    std::string selectState(const Table& table) {
        std::string columns, order;
        for (std::size_t index = 0; index < table.columns.size(); ++index) {
            const auto& column = table.columns[index];
            columns += (index == 0 ? "" : ",") + (column.numeric ? column.name : "HEX(" + column.name + ")");
            if (index < table.keys) {
                order += (index == 0 ? "" : ",") + column.name;
            }
        }
        return "SELECT " + columns + " FROM " + table.name + " ORDER BY " + order;
    }
    Row decodeFields(const Table& table, const std::vector<Field>& fields) {
        Row result = fields;
        if (fields.size() != table.columns.size()) {
            throw std::invalid_argument("wrong state field count");
        }
        for (std::size_t index = 0; index < fields.size(); ++index) {
            if (fields[index] && !table.columns[index].numeric) {
                const auto& hex = *fields[index];
                std::string decoded;
                bool valid = hex.size() % 2 == 0;
                for (std::size_t at = 0; valid && at < hex.size(); at += 2) {
                    unsigned byte = 0;
                    const auto parsed = std::from_chars(hex.data() + at, hex.data() + at + 2, byte, 16);
                    valid = parsed.ec == std::errc{} && parsed.ptr == hex.data() + at + 2 && byte != 0;
                    if (valid) {
                        decoded += static_cast<char>(byte);
                    }
                }
                if (!valid) {
                    throw std::invalid_argument("invalid state hex or NUL");
                }
                result[index] = decoded;
            } else if (fields[index]) {
                validateNumber(fields[index]);
            }
        }
        return result;
    }
    std::vector<std::string> stateSql(const StoredState& before, const StoredState& after, const std::string& description) {
        auto previous = encodeState(before), next = encodeState(after);
        std::vector<std::string> statements;
        for (const auto& table : stateTables()) {
            const auto keyed = [&](const auto& rows) {
                std::map<Row, Row> result;
                for (const auto& row : rows.at(table.name)) {
                    result.emplace(Row(row.begin(), row.begin() + static_cast<std::ptrdiff_t>(table.keys)), row);
                }
                return result;
            };
            const auto oldRows = keyed(previous), newRows = keyed(next);
            for (const auto& [key, row] : oldRows) {
                if (!newRows.contains(key)) {
                    statements.push_back("DELETE FROM " + table.name + " WHERE " + keyWhere(table, row));
                }
            }
            for (const auto& [key, row] : newRows) {
                if (!oldRows.contains(key) || oldRows.at(key) != row) {
                    std::string columns, values, updates;
                    for (std::size_t index = 0; index < row.size(); ++index) {
                        const auto& column = table.columns[index];
                        columns += (index == 0 ? "" : ",") + column.name;
                        values += (index == 0 ? "" : ",") + literal(row[index], column.numeric);
                        updates += (index == 0 ? "" : ",") + column.name + "=VALUES(" + column.name + ")";
                    }
                    statements.push_back("INSERT INTO " + table.name + " (" + columns + ") VALUES (" + values +
                                         ") ON DUPLICATE KEY UPDATE " + updates);
                }
            }
        }
        statements.push_back("INSERT INTO coreci_log (seq,logged_at,description) VALUES (" + std::to_string(after.sequence) + "," +
                             std::to_string(after.engineTime.value_or(0)) + "," + sqlText(description) + ")");
        return statements;
    }
} // namespace coreci::persistence
