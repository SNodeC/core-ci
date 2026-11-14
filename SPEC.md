# core-ci — Implementation Specification for Codex

This is the master contract for the slim controller. It manages GitHub package workflows and serves an authenticated SSE dashboard. No compatibility with earlier event histories is required: start with a fresh database. The GitHub workflows themselves remain to be specified in section 7.

---

## 0. Goal in one paragraph

`core-ci` is a small, single-threaded C++20 daemon that **orchestrates** package builds for a set of GitHub repositories (first deployment: the SNode.C ecosystem). It contains no knowledge of any particular repository, package, distribution, architecture or release: everything it builds is declared in JSON files in the participating repositories (section 3.3), so the same program can be deployed for other repositories by configuration alone. It does no expensive work itself: every build, every signing operation and every feed publication runs as a GitHub Actions workflow. `core-ci` only (a) receives notifications from GitHub, (b) maintains a dependency graph of packages × targets modelled in C++, (c) decides which unit is allowed to build or publish next, and (d) triggers the corresponding GitHub workflows via the GitHub REST API. Dependency chains, including diamonds, are resolved by `core-ci` and **never** by workflows triggering each other.

---

## 1. Non-negotiable constraints

1. **The graph is declared in repositories and modelled in C++.** Packages, targets, feeds and edges are validated into a typed Pipeline. Persistence uses typed tables, never a JSON graph. Runtime dashboard JSON may expose prerequisites; it is never persisted or read back.
2. **No JSON for core-ci's own data.** Persistence (MariaDB, section 5), configuration and the callback protocol (section 6) are not JSON — no JSON columns, no JSON SQL functions, no JSON text stored in any column. JSON is used only inside `adapters/github/`: parsing GitHub webhook payloads, REST responses and the declaration files of section 3.3, and encoding REST request bodies. Tests of `adapters/github/` and the fake GitHub server in `tests/` may contain GitHub wire-format JSON and declaration files as test data. `#include <nlohmann/json.hpp>` (or any JSON library) outside `adapters/github/` is a defect. Two exceptions, never stored, logged or read back: the read-only HTTP response `GET /api/v1/status.json` of 6.6, written in `app/` by a small hand-written JSON writer (no JSON library; unit states, no graph edges); and the dashboard of 6.5 (`GET /events` and the requests and responses of `POST /api/<operation>`), encoded and parsed only inside `adapters/dashboard/`, which may use the same JSON library as `adapters/github/`.
3. **Expensive work happens only on GitHub.** core-ci never compiles, packages, downloads artifacts or writes feeds. Its GitHub App private key is used only to authenticate.
4. **Only core-ci chains builds.** Workflows never chain downstream builds.
5. **Feed publication is serialized by core-ci**, not by GitHub `concurrency:` groups. (GitHub concurrency groups keep at most one pending run and cancel older pending runs — that silently drops publications.)
6. **Single-threaded.** No `std::thread`, no mutexes, no blocking sleeps, no polling loops. Everything runs in the SNode.C event loop; timers use SNode.C's timer facility.
7. **The engine is pure.** The scheduling engine (section 4) performs no I/O, reads no clock, generates no randomness. Time arrives only as the `at` timestamp carried by every event (section 4.3). Same event sequence ⇒ same state and same commands, always.
8. **Never pin to commits.** Workflows are dispatched on each repository's configured default branch (do not assume `main` — some repositories use `master`, and default branch names are never renamed). GitHub Actions are referenced by tag (e.g. `@v4`), never by commit SHA.
9. Build flags: `-Wall -Wextra -Wpedantic -Werror`. Every phase ends with a clean build and all tests passing.
10. **Development scope:** canonical checkout only; code, SPEC and tests. No production access, deployment or tags. Never consult the pre-greenfield implementation.

---

## 2. Repository layout

```
/home/voc/projects/snodec/core-ci/
  CMakeLists.txt
  core/                     # library coreci_core — pure C++20, depends on nothing but the STL
    Version.h/.cpp          # semver parsing and ordering
    Graph.h/.cpp            # Package, Target, Feed, Pipeline builder, validation, topo order
    Engine.h/.cpp           # state + event handling + command emission
    Events.h  Commands.h    # std::variant types
    Status.h/.cpp           # derived unit status
  persistence/              # library coreci_persistence — pure C++20, no SNode.C, no database
    EventRow.h/.cpp         # Event <-> typed row mapping (pure, fully tested)
    EventStore.h            # asynchronous store interface
  adapters/mariadb/         # MariaDBEventStore — implements EventStore with SNode.C's MariaDB client
    Schema.sql
  adapters/github/          # GitHub REST client, webhooks, HMAC, declaration loading (only place with JSON)
  app/                      # SNode.C express server, wiring, timers, main() — knows no repository
  tests/                    # CTest executables and test-only InMemoryEventStore
```

`core/` and `persistence/` must compile and be tested without SNode.C installed.

---

## 3. Domain model (core/Graph.h)

### 3.1 Types

- **Feed** — `id`, publisher workflow `{owner, repo, workflowFile, branch}`. Publications to one feed are strictly serialized.
- **Target** — `id` (an opaque string, e.g. `debian-trixie-amd64`) and `feed` (exactly one Feed id); a target id denotes the same target, with the same feed, in every package that declares it. core-ci attaches no meaning to a target: distribution, release, architecture and build details are read by the workflows from the declaration (section 7).
- **Package** — `name`, `sourceRepo {owner, repo, branch}`, `buildWorkflow {owner, repo, workflowFile, branch}`, `targets` (set of Target ids), `prerequisites` (set of Package names; every edge means "must be built against the published version of"), `acceptPrereleases`, `revisionBase` (0–1 000 000). There is no configurable tag pattern: every package uses the tag format of section 3.4.
- **Unit** — the scheduling unit `(package, target)`. A dependency edge `P → Q` applies per target: unit `(P,T)` depends on `(Q,T)`.
- **Pipeline** — the immutable set of feeds and packages. Two pipelines are equal if all their feeds and packages are equal, compared in canonical order (by id and name).

Every `branch` is the repository's default branch as reported by GitHub when the declarations were loaded — never assumed, never pinned. Repository owners and names are kept in GitHub's canonical spelling — the `full_name` returned by `GET /repos/{owner}/{repo}` during loading, however the registry spells them — and are never lowercased; that spelling is stored, shown in status, and used in API URLs. As identities they are compared ASCII case-insensitively, as GitHub does: uniqueness (3.2), webhook matching, and the `sourceRepo`/`buildWorkflow` change checks of rule 13 and 4.7. Pipeline equality compares the spelling exactly, so a change of spelling alone is recorded as `GraphChanged` without any other effect. Package names, feed ids and target ids are case-sensitive.

### 3.2 Builder and validation

A fluent builder (`Pipeline::Builder`) produces an immutable `Pipeline`. `build()` rejects, with a precise error message:
- unknown package / target / feed references,
- duplicate package names, feed ids, or target ids within a package,
- cycles — the message must print the cycle path, e.g. `cycle: a -> b -> c -> a`,
- a package P with target T whose prerequisite Q does not have target T,
- two packages with the same `sourceRepo` (each repository declares exactly one package),
- a target id declared with different feeds,
- a package name, feed id or target id that is empty, longer than 64 characters, or contains a character other than `A–Z a–z 0–9 . _ -`,
- an owner that is not 1–39 characters of `A–Z a–z 0–9 -`; a repository name that is not 1–100 characters of `A–Z a–z 0–9 . _ -`, or is `.` or `..`,
- a workflow file name that is not 1–255 characters of `A–Z a–z 0–9 . _ -` ending in `.yml` or `.yaml`,
- a branch that is empty, longer than 255 characters, or not a valid branch name under the rules of `git check-ref-format --branch` (no control characters, space, `~ ^ : ? * [ \`, `..`, `@{` or `//`; not `@`; no leading `-` or `/`; no trailing `/`, `.` or `.lock`).

`Pipeline` provides: per-target topological order (deterministic: ties broken by package name), direct prerequisites, direct dependents, `packagesForRepo(owner, repo)`, and the set of declaring repositories.

### 3.3 Declarations (adapters/github/Declarations)

Configuration supplies `registry` as `<owner>/<repo>:<path>`, `target-catalog` as a relative path in that repository, and `workflow-repository` as `<owner>/<repo>`. There is no built-in repository name. Paths contain nonempty slash-separated segments of `A–Z a–z 0–9 . _ -`, excluding `.` and `..`, and are at most 255 characters. Repository and workflow identifiers obey 3.2.

**Registry.** Read from the registry repository's default branch, at its resolved HEAD commit. Required fields:

```json
{
  "feeds": { "<feed id>": { "publisher": "<workflow file.yml>" } },
  "default_feed": "<feed id>",
  "build": "<workflow file.yml>",
  "repositories": [ "<owner>/<repo>" ]
}
```

Both build and publisher workflows belong to the configured workflow repository, dispatched on its reported default branch. Unknown fields are ignored. There is no presentation writer or status URL contract.

**Package declaration.** Each source contains `ci/packages.json`:

```json
{
  "package": "<package name>",
  "depends_on": [ "<package name>" ],
  "accept_prereleases": false,
  "revision_base": 0,
  "matrix": [ { "id": "<target id>", "feed": "<feed id>" } ]
}
```

`package`, `depends_on` and `matrix` are required. `accept_prereleases` defaults false, `revision_base` defaults 0 (integer 0–1000000), and each feed defaults to `default_feed`. Optional `title` is used by the dashboard, otherwise the canonical source repository name. Matrix entries accept only `id` and optional `feed`.

**Target catalog.** The configured file in the registry repository contains `{"targets":[{"id":"<id>",...}]}`. Every id is unique. Each entry supplies `distribution`, `suite`, `arch` and `build.format` for the dashboard; workflows interpret the remaining fields. Every matrix id must exist in the catalog.

**Tag-bound loading.** Resolve canonical repository spelling and default branches with `GET /repos/{owner}/{repo}`. Resolve the registry HEAD with `GET /repos/{owner}/{repo}/commits/{branch}`; read registry and catalog at that commit. List source tags with pagination, ordered by 3.4 (equal precedence: lexicographically smallest tag). Read each candidate declaration at the tag's **commit SHA** from the tags API, never at the source default branch when building a tag. Select the highest version whose declaration permits its prerelease status. A repository without any accepted release is bootstrapped from its default-branch declaration solely to show NoRelease units; it is never built until a tag-bound load succeeds. Metadata commits for source default branches are needed only for that bootstrap.

A load validates all files and the complete Pipeline before changing state. Malformed JSON, duplicate object keys, files over 1 MiB, wrong types, invalid identifiers or any HTTP failure fail the load, retaining the previous graph and releases. Errors identify repository, file and reason and appear in the authenticated dashboard response/stream. The successful load commits GraphChanged, then the selected TagObserved events through the same write-ahead queue. Units that remain settled are not rebuilt by a reload alone; added units become schedulable and removed units are retired from the active graph under rule 13. Changed dependency/feed/workflow definitions retain rule 13 semantics.

Loads run at startup after the listener is up, every reconcileInterval, after a registry default-branch push, after a known source tag push, and through the dashboard reload/reconcile operations. Source default-branch pushes never trigger a load or rebuild. Concurrent load triggers coalesce to one further load; different source repositories are fetched independently. Reconciliation uses this same tag-bound loading path.

### 3.4 Tag format and version ordering (core/Version.h)

- A tag is accepted as a version only if it matches exactly `v?MAJOR.MINOR.PATCH(-PRERELEASE)?(+BUILD)?`, with every part as defined by Semantic Versioning 2.0.0 (numeric identifiers without leading zeros; pre-release and build identifiers `[0-9A-Za-z-]`, dot-separated, non-empty). An optional single lowercase `v` prefix is allowed and is not part of the version. Anything else (e.g. `V1.0.0`, `1.0`, `release-1.0.0`, `v01.0.0`), and any version longer than 100 characters, is not a version.
- `version` is the tag without the `v` prefix (e.g. tag `v2.0.1` → version `2.0.1`); `tag` keeps the original string for checkout.
- Ordering is Semantic Versioning 2.0.0 precedence: MAJOR, MINOR, PATCH numerically; a pre-release sorts before the same version without one; pre-release identifiers compared left to right (numeric < alphanumeric, numeric compared numerically, alphanumeric compared in ASCII order, fewer identifiers < more); build metadata is ignored for precedence.
- Two tags of equal precedence (e.g. `v1.0.0` and `1.0.0`, or `1.0.0+a` and `1.0.0+b`) are equal: the one observed later is ignored by rule 4.5.1.

---

## 4. Engine (core/Engine.h) — the heart. Implement exactly this.

### 4.1 Stored state (minimal; everything else is derived)

The engine is constructed with its configuration only and starts with an empty pipeline. The pipeline arrives as `GraphChanged` events (rule 13) and is part of the replayed history.

Per package:
- `released`: optional `{version, tag}` — highest accepted tag seen.

Per unit `(P,T)`:
- `published`: optional `{version, revision, inputs}` where `inputs` is a sorted map `prerequisite → {version, revision}` — exactly what is in the feed.
- `inflight`: optional `{buildId, version, tag, revision, inputs, phase, attempt, retryAt, deadline, runId, artifact}` with `phase ∈ {Dispatching, Building, AwaitingPublish, PublishDispatching, Publishing}`; `attempt` counts dispatch attempts of the current phase; `retryAt` is set only while waiting to retry a dispatch.
- `failure`: optional `{version, inputs, reason}`.
- `retryAfter`: optional engine time of the automatic retry (rule 15).
- `autoRetried`: optional `{version, inputs}` of the last automatic retry (rule 15).
- `rebuild`: set by `RebuildRequested` (rule 14), cleared by `PublishSucceeded` (rule 6).
- `lastRevision[version]`: highest revision ever allocated for this version.

Per feed: FIFO queue of units in `AwaitingPublish`, and at most one unit in `PublishDispatching/Publishing`.

### 4.2 Derived predicates (recomputed after every event, per target in topological order)

- `desiredInputs(P,T)` = for each prerequisite Q: `published(Q,T).{version,revision}`; **undefined** unless every prerequisite `(Q,T)` has `published` (no prerequisites → empty map). If undefined, `satisfied` is false.
- `satisfied(P,T)` ⇔ `published` exists ∧ `published.version == released(P).version` ∧ `published.inputs == desiredInputs(P,T)` ∧ ¬`rebuild`.
- `settled(P,T)` ⇔ `satisfied(P,T)` ∧ no `inflight` ∧ every prerequisite `(Q,T)` is `settled`.
- `ready(P,T)` ⇔ `released(P)` exists ∧ ¬`satisfied` ∧ no `inflight` ∧ ¬(`failure` exists ∧ `failure.version == released.version` ∧ `failure.inputs == desiredInputs`) ∧ every prerequisite `(Q,T)` is `settled`.

**Why this solves diamonds:** with A → B, A → C, {B, C} → D, a new tag on A makes A unsettled, so B and C are not ready, and D is not ready because B and C are not settled. When A publishes, B and C become ready. When only B has published, C is still not settled, so D waits. D becomes ready exactly once, when both B and C are settled, and is built against both new versions. No eager "dirty marking" is stored — it is derived, so it cannot drift.

### 4.3 Events (engine input; every accepted event is persisted — see 5)

**Every event carries `at`** (UTC seconds, `int64`), stamped by `app/` when the event enters the event queue (section 5.3) — never by `core/`. The table lists the additional fields.

**Engine time:** the engine keeps `engineTime`, initially unset. Before handling an **accepted** event (section 4.7) it sets `engineTime = max(engineTime, event.at)` (the first event sets it; time never goes backwards, so clock adjustments on the host cannot move deadlines into the past). All deadlines and backoff times are computed from `engineTime`. Ignored events change nothing, including `engineTime`. Because `at` is persisted with the event, replay reproduces exactly the same deadlines.

| Event | Fields (in addition to `at`) |
|---|---|
| `TagObserved` | package, tag, sourceSha, origin (`webhook`/`reconcile`) |
| `DispatchAccepted` | buildId, kind (`build`/`publish`), attempt |
| `DispatchRejected` | buildId, kind, attempt, httpStatus, message |
| `BuildStarted` | buildId, runId, runAttempt |
| `PublishStarted` | buildId, runId, runAttempt |
| `BuildSucceeded` | buildId, runId, artifact |
| `BuildFailed` | buildId, runId, reason |
| `PublishSucceeded` | buildId, runId, feedCommit |
| `PublishFailed` | buildId, runId, reason |
| `RetryRequested` | package, target |
| `RebuildRequested` | package, target |
| `Tick` | — (only `at`) |
| `Resumed` | — (only `at`; rule 12) |
| `GraphChanged` | pipeline (complete, section 3.1; rule 13) |

### 4.4 Commands (engine output; not persisted — they are reproduced by replay)

- `DispatchBuild{buildId, attempt, package, sourceRepo, tag, version, revision, target, inputs, workflow}`
- `DispatchPublish{buildId, attempt, package, version, revision, target, feed, buildRepo, runId, artifact, workflow}`

### 4.5 Rules

1. **TagObserved:** parse the tag as specified in section 3.4; ignore if not a version, if prerelease and not accepted, or if `≤ released`. Otherwise set `released`.
2. **After every accepted event**, in this order: (a) every unit in `AwaitingPublish` whose `inflight.version ≠ released.version` or `inflight.inputs ≠ desiredInputs` is superseded: remove it from the feed queue, clear `inflight`; (b) evaluate all targets in topological order; for every `ready` unit (deterministic order: target id, then topo order) allocate `revision = max(lastRevision[released.version], revisionBase) + 1` and store it as `lastRevision[released.version]` (so the first revision of a version is `revisionBase + 1`, and lowering `revisionBase` never makes revisions go back), set `buildId = "<package>~<target>~<version>~r<revision>"` (`~` occurs in no package name, target id or version, so a buildId is unambiguous; at most 252 characters by 3.2 and 3.4), set `inflight{phase=Dispatching, attempt=1}`, emit `DispatchBuild`; (c) for every feed with no publication in progress, pop the queue head, set its phase to `PublishDispatching` with `attempt=1`, emit `DispatchPublish`.
3. **DispatchAccepted:** `Dispatching → Building` (deadline = `engineTime` + buildTimeout) / `PublishDispatching → Publishing` (deadline = `engineTime` + publishTimeout).
4. **DispatchRejected:** HTTP 401/403/404/422 → failure (rule 7) immediately. Otherwise, if `attempt == maxDispatchAttempts` (default 8) → failure. Otherwise `retryAt = engineTime + min(30 s · 2^(attempt−1), 30 min)`. On the first accepted `Tick` with `engineTime ≥ retryAt`: `attempt += 1`, clear `retryAt`, emit the same command again (same `buildId`, new `attempt`). A re-emission does not re-check readiness: it resends a dispatch already decided by rule 2. If the unit's inputs change meanwhile, its build is superseded by rule 5; if only an upstream unit changes, the build is consistent with the feed, may publish, and is rebuilt by rule 2 once that change is published. `httpStatus = 0` (transport error) is not permanent.
5. **BuildSucceeded:** if `inflight.version == released.version` ∧ `inflight.inputs == desiredInputs` → phase `AwaitingPublish`, append to the feed queue. Otherwise the build is **superseded**: clear `inflight` (the unit becomes ready again in step 2). Superseded artifacts are never published.
6. **PublishSucceeded:** `published = {inflight.version, inflight.revision, inflight.inputs}`, clear `inflight`, clear `failure` and `rebuild`. (Even if superseded meanwhile, record the truth of what is in the feed; the unit then becomes unsatisfied and is rebuilt.)
7. **BuildFailed / PublishFailed / deadline exceeded on Tick:** `failure = {inflight.version, inflight.inputs, reason}`, clear `inflight`, remove from feed queue/slot. Dependents stay blocked (the unit is not settled). A new tag or new prerequisite publication changes the desired inputs and therefore automatically allows a retry; otherwise only `RetryRequested` clears `failure`. An automatic retry is scheduled when `retryDelay > 0`, the failure is not a `DispatchRejected` with HTTP 401/403/404/422, and `autoRetried ≠ {failure.version, failure.inputs}`: `retryAfter = engineTime + retryDelay`.
8. **Acceptance** is defined only by the table in section 4.7. Ignored events are not persisted and change nothing, so duplicates are idempotent. The engine exposes a pure, `const` `classify(event) → Accept | Ignore(reason)` implementing that table (needed by section 5).
9. `BuildStarted` and `PublishStarted` record their phase's runId, runAttempt and start time. Terminal callbacks record end time and state, with durations derived from recorded start/end times; a missing start stays absent. `BuildStarted` and `BuildSucceeded` record `runId`; `BuildSucceeded` also records `artifact` (both used by `DispatchPublish`). A build callback accepted in `Dispatching` (or a publish callback in `PublishDispatching`) proves the run exists: first clear `retryAt` and enter `Building` (`Publishing`) with deadline = `engineTime` + timeout, then apply the event.
10. Optional global `maxConcurrentBuilds` (default unlimited); a unit occupies a slot while its phase is `Dispatching` (also while waiting for `retryAt`) or `Building`; when no slot is free, ready units wait.
11. The engine exposes `nextDeadline()` (earliest pending timeout or retry-backoff time, or none). `app/` only enqueues a `Tick` when the current UTC time ≥ `nextDeadline()`, so every `Tick` changes state. Before the first deadline exists no `Tick` is needed: every deadline is created by an event that itself carries `at`.
12. **Resumed** (enqueued once by `app/` after replay, section 5.4): re-emit the command, with unchanged `buildId` and `attempt`, for every unit in `Dispatching`/`PublishDispatching` without `retryAt` (its dispatch result was lost); units waiting for `retryAt` keep their backoff. Like rule 4, this re-emission does not re-check readiness.
13. **GraphChanged:** replace the pipeline. State is keyed by package name and by `(package, target)`. Packages and units that remain keep their state. Packages and units no longer in the pipeline become **dormant**: their `inflight` is cleared (later callbacks are ignored, a held build slot is freed); `released`, `published`, `failure` and `lastRevision` are kept, together with the package's last `sourceRepo` and `buildWorkflow` and each unit's last feed, but neither evaluated nor shown. If they reappear in a later `GraphChanged` they continue from that state — the change rules below are applied against these retained values — so revisions are never reused. Changed prerequisites act through the derived predicates: a unit whose `published.inputs` no longer match its new `desiredInputs` is rebuilt, and builds with outdated inputs are superseded by rules 2(a) and 5. A unit whose feed changes loses `published` and `failure` (its packages are not in the new feed), keeps `lastRevision` and is rebuilt into the new feed. If a package's `sourceRepo` changes, its `released` is cleared; the next tag reconciliation re-establishes it from the new repository. If a package's `sourceRepo` or `buildWorkflow` changes, its units in `Dispatching` or `Building` are abandoned like dormant ones and rebuilt through the new workflow with a new revision.

14. **RebuildRequested:** set `rebuild`. The unit is no longer satisfied, so rule 2 rebuilds the same version with a new revision and it is published normally; its dependents on the same target follow through their `desiredInputs`. A failure of that build is handled by rule 7 and `RetryRequested`; `rebuild` stays set until the publication.
15. **Automatic retry (on Tick):** a unit with `failure` and `retryAfter ≤ engineTime` records `autoRetried = {failure.version, failure.inputs}`; then `failure` and `retryAfter` are cleared exactly as by `RetryRequested`. `RetryRequested` and every change of version or inputs clear `retryAfter`. At most one automatic retry per `{version, inputs}`.

### 4.6 Status (core/Status.h)

Per unit, derive one of: `NoRelease`, `Settled`, `Waiting(on <unit>)`, `Ready`, `Dispatching`, `Building(run <id>)`, `AwaitingPublish(position n on <feed>)`, `Publishing`, `Failed(<reason>)`, `Blocked(by <failed unit>)`. Status is derived for status.json, badges and the dashboard; there are no text or DOT renderers.

### 4.6.1 Badges (core/Badge.h)

`badge(label, message, colour) → SVG` and the mapping from a unit to its badge are pure. Label: the package name. Message and colour by unit status (4.6), with `v` the released version and `p` the published `version-revision`:

| Status | Message | Colour |
|---|---|---|
| `Settled` | `p` | `#2ea44f` |
| `Ready`, `Waiting`, `Dispatching`, `Building`, `AwaitingPublish`, `Publishing` | `building v` | `#0969da` |
| `Failed` | `failed v` | `#cf222e` |
| `Blocked` | `blocked` | `#bf8700` |
| `NoRelease` | `no release` | `#6e7781` |
| unknown or dormant unit | `unknown` | `#6e7781` |
| core-ci in recovery (decided by `app/`) | `unavailable` | `#6e7781` |

SVG: height 20; label part on `#555`, message part on the colour; white text, `font-family="Verdana,DejaVu Sans,sans-serif" font-size="11"`; each part is `6 × characters + 10` pixels wide (a deterministic estimate, no font metrics); `role="img"` with `<title>label: message</title>`; all text XML-escaped (`& < > " '`). The exact output is fixed by a golden test.

### 4.7 Event acceptance (classify)

| Event | Accepted only if (otherwise ignored) |
|---|---|
| `TagObserved` | package known; tag is a version (3.4); not a rejected prerelease; version > `released` |
| `DispatchAccepted`, `DispatchRejected` | `buildId` = a unit's `inflight.buildId`; phase `Dispatching` (kind `build`) or `PublishDispatching` (kind `publish`); `attempt` = `inflight.attempt`; no `retryAt` |
| `BuildStarted` | `buildId` matches; phase `Dispatching` or `Building`; no `runId` recorded |
| `BuildSucceeded`, `BuildFailed` | `buildId` matches; phase `Dispatching` or `Building`; no `runId` recorded or the same `runId` |
| `PublishStarted` | `buildId` matches; phase `PublishDispatching` or `Publishing`; no publisher runId recorded |
| `PublishSucceeded`, `PublishFailed` | `buildId` matches; phase `PublishDispatching` or `Publishing`; no publisher runId recorded or the same runId |
| `RetryRequested` | unit exists; `failure` set; no `inflight` |
| `RebuildRequested` | unit exists and is not dormant; `satisfied`; no `inflight` |
| `Tick` | `nextDeadline()` exists and `max(engineTime, at) ≥ nextDeadline()` |
| `Resumed` | always |
| `GraphChanged` | the pipeline differs from the current one, and none of these holds (otherwise the next load retries): a unit in `AwaitingPublish`, `PublishDispatching` or `Publishing` would be removed or change its feed; a package with a unit in one of these phases changes its `sourceRepo` or `buildWorkflow`; a feed with a unit in `PublishDispatching` or `Publishing` changes its publisher workflow |

A workflow re-run started in the GitHub UI is not a retry; once the unit has left the phase its callbacks are ignored. Use `RetryRequested`.

---

## 5. Persistence — MariaDB via SNode.C's MariaDB client

Persistence is **event sourcing in MariaDB**: an append-only event table is the single source of truth; engine state is rebuilt by replaying it. The graph is stored only as part of `GraphChanged` events, in typed tables (5.2) — never as JSON.

### 5.1 Database API

Use the MariaDB client that SNode.C already provides. **Do not use libmariadb/libmysqlclient directly, do not add an ORM or another database library, and do not guess the API**: find SNode.C's MariaDB client in the SNode.C repository (master HEAD) and its examples/apps, and cite the source files you based your code on in the PR description. The client is asynchronous and runs in the SNode.C event loop; use it that way (callbacks), never block.

Verified facts (SNode.C master, `src/database/mariadb/MariaDBClientASyncAPI.h`, `MariaDBClient.h`, `commands/async/MariaDBFetchRowCommand.cpp`): the client offers asynchronous `exec`, `query`, `startTransactions`/`commit`/`rollback`, but **no prepared statements and no escaping function**. Therefore every string value is written as a hexadecimal literal with charset introducer, `_utf8mb4 X'…'` (the bytes of the normalized UTF-8 value; the empty string as `_utf8mb4 X''`), every integer as a decimal literal and every absent value as `NULL` — no other form, so SQL injection is impossible by construction. The SQL text is produced by the pure `persistence/EventSql.h/.cpp` (S14). After every (re)connect, `open` executes `SET NAMES utf8mb4` and `SET autocommit = 1` before any other statement. An `INSERT` runs via `exec` in autocommit mode; its success callback is `onCommitted`. `query` delivers rows through `onQuery(MYSQL_ROW)` and ends with a final `nullptr` row. SNode.C reconnects lazily on the next command after a lost connection and reports state through `onStateChanged`. The only non-literal expression allowed in an `INSERT` is `UTC_TIMESTAMP(3)` for `recorded_at` (audit only: never read back, never decoded). Reads select every column except `recorded_at`, string columns as `HEX(col) AS col` and numeric columns as decimal text; `adapters/mariadb` decodes the hex purely (invalid hex or non-decimal number → `Corrupt`) before `EventRow` validation, so an embedded NUL is detected. A `nullptr` field is SQL `NULL`; `HEX('')` is an empty, non-null field.

### 5.2 Schema (adapters/mariadb/Schema.sql, created by core-ci at startup with `CREATE TABLE IF NOT EXISTS`, InnoDB, utf8mb4)

```sql
CREATE TABLE IF NOT EXISTS coreci_schema (
  version INT NOT NULL PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS coreci_events (
  seq          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  recorded_at  DATETIME(3)  NOT NULL,
  event_type   VARCHAR(32)  NOT NULL,   -- TagObserved, DispatchAccepted, ... (section 4.3)
  package      VARCHAR(128) NULL,
  target       VARCHAR(128) NULL,
  tag          VARCHAR(255) NULL,
  source_sha   CHAR(40)     NULL,
  origin       VARCHAR(16)  NULL,
  build_id     VARCHAR(255) NULL,
  kind         VARCHAR(16)  NULL,
  http_status  INT          NULL,
  message      TEXT         NULL,
  run_id       BIGINT UNSIGNED NULL,
  run_attempt  INT          NULL,
  attempt      INT          NULL,       -- dispatch attempt
  artifact     VARCHAR(255) NULL,
  feed_commit  CHAR(40)     NULL,
  reason       TEXT         NULL,
  event_at     BIGINT       NOT NULL,   -- the event's `at` (UTC seconds), section 4.3
  INDEX idx_build_id (build_id),
  INDEX idx_unit (package, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS coreci_graph_feeds (
  event_seq     BIGINT UNSIGNED NOT NULL,
  feed          VARCHAR(64)  NOT NULL,
  owner         VARCHAR(100) NOT NULL,
  repo          VARCHAR(100) NOT NULL,
  workflow_file VARCHAR(255) NOT NULL,
  branch        VARCHAR(255) NOT NULL,
  PRIMARY KEY (event_seq, feed)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_graph_packages (
  event_seq           BIGINT UNSIGNED NOT NULL,
  package             VARCHAR(64)  NOT NULL,
  source_owner        VARCHAR(100) NOT NULL,
  source_repo         VARCHAR(100) NOT NULL,
  source_branch       VARCHAR(255) NOT NULL,
  build_owner         VARCHAR(100) NOT NULL,
  build_repo          VARCHAR(100) NOT NULL,
  build_workflow_file VARCHAR(255) NOT NULL,
  build_branch        VARCHAR(255) NOT NULL,
  accept_prereleases  TINYINT      NOT NULL,
  revision_base       INT UNSIGNED NOT NULL,
  PRIMARY KEY (event_seq, package)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_graph_targets (
  event_seq BIGINT UNSIGNED NOT NULL,
  package   VARCHAR(64) NOT NULL,
  target    VARCHAR(64) NOT NULL,
  feed      VARCHAR(64) NOT NULL,
  PRIMARY KEY (event_seq, package, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_graph_prerequisites (
  event_seq    BIGINT UNSIGNED NOT NULL,
  package      VARCHAR(64) NOT NULL,
  prerequisite VARCHAR(64) NOT NULL,
  PRIMARY KEY (event_seq, package, prerequisite)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
```

`coreci_schema` holds exactly one row, `version = 4`. In `open`: if `coreci_schema` has no row and all `coreci_*` tables are empty, insert `version = 4`; no row while any table has rows, more than one row, or a version other than 4 → `Corrupt` (older databases are not migrated). Every event type uses only its own columns; the others are `NULL`.

**GraphChanged storage.** A `GraphChanged` is one `coreci_events` row (`event_type = 'GraphChanged'`, only `event_at` set) plus its graph rows, written atomically on one connection as `startTransactions` (autocommit off) → the event `INSERT` → the graph `INSERT`s → `commit` → `endTransactions` (autocommit on again), as implemented in SNode.C's `MariaDBClientASyncAPI.cpp` (`MariaDBAutoCommitCommand`, `MariaDBCommitCommand`). The graph rows use `LAST_INSERT_ID()` as `event_seq`, the second allowed non-literal expression. `onCommitted` is called only after `endTransactions` succeeded. On any error in this sequence the store aborts the connection instead of issuing `rollback` (the server discards uncommitted work and releases the writer lock) and reports an unknown commit outcome (5.3 step 4); a connection is never reused with autocommit off. `normalize` checks a `GraphChanged`'s strings against these columns. The graph tables use `utf8mb4_bin`, so identifiers compare exactly as in C++. `loadAll` reads the four graph tables ordered by primary key and attaches them to their `GraphChanged` rows; a `GraphChanged` without graph rows is the empty pipeline; graph rows without their `GraphChanged` row, or rows that fail 3.2, are `Corrupt`. Schema migrations are out of scope.

Every event field from 4.3 maps to exactly one typed column; unused columns are `NULL`. The mapping `Event ↔ EventRow` lives in `persistence/EventRow.cpp`, is pure and is round-trip tested for every event type.

**Field rules** (`persistence/EventRow`, pure): `normalize(event) → Event | Invalid(reason)`. Identifier fields (`package`, `target`, `tag`, `build_id`, `artifact`) must be non-empty valid UTF-8 without NUL and fit their column, counted in characters as MariaDB `utf8mb4` does. `source_sha` and `feed_commit` are exactly 40 lowercase hexadecimal characters. Numbers must fit their column (`run_attempt`, `attempt` ≥ 1; `http_status` 0–999). Free text (`message`, `reason`) is never rejected: invalid UTF-8 → U+FFFD, NUL removed, truncated to 4096 bytes at a character boundary. Every field listed for an event in 4.3 is required; `message`/`reason` may be empty. **Decoding** a row with an unknown `event_type`, an invalid `kind`/`origin`, a missing required field, a non-NULL field the event type does not use, or a value violating these rules is fatal: `loadAll` reports it through `onError` naming the row's `seq`, and core-ci refuses to start.

### 5.3 Write-ahead ordering (critical)

All inbound events (webhook, workflow callback, dispatch result, tick, dashboard operation) go through **one FIFO event queue** and are processed strictly one at a time:

0. Stamp `event.at` with the current UTC time (seconds) when the event enters the queue. This is the only place where core-ci reads the clock for events. Then `normalize(event)` (5.2); if invalid → answer `400 invalid: <reason>`, done (never retried, never classified, never stored).
1. `engine.classify(event)`; if `Ignore` → answer the HTTP request (`200 ignored: <reason>`), done.
2. `INSERT` the event row. **Wait for the commit callback.** No other event is processed meanwhile.
3. Only after a successful commit: `engine.handle(event)` → execute the returned commands → answer the originating HTTP request (`200 accepted`).
4. If `append` reports an error, the commit outcome is **unknown** (the row may or may not be stored): do not apply the event, answer `503`, and enter **recovery**: answer every further event `503`, discard the engine, then rebuild exactly as at startup (5.4: `loadAll`, replay, `Resumed`) and resume normal processing. If the row was in fact stored, the sender's retry is a duplicate and is answered `200 ignored`; commands lost with the discarded engine are re-emitted by `Resumed`.

Consequence: the in-memory state only ever contains committed events, so a crash at any point loses nothing that was acknowledged.

### 5.4 Startup and outages

- **Startup order:** connect to MariaDB → ensure schema → `SELECT … FROM coreci_events ORDER BY seq` → replay every row through the same engine, discarding commands → enqueue one `Resumed` event through the normal write-ahead queue (rule 12) → **only then** start the HTTP listener and timers. No inbound event may be accepted before replay has finished.
- During replay, a stored event that the engine classifies as `Ignore` (possible only if engine semantics changed between core-ci versions) is skipped with a warning naming its `seq`.
- **Store errors** from `open`/`loadAll` have a kind (5.5): `Corrupt` (a row fails decoding per 5.2, or an unknown schema version) → log it and exit with a non-zero status, at startup and at runtime. `Unavailable` (any database or connection error) → stay in recovery (every event `503`, no ticks); `app/` retries the rebuild with an SNode.C timer, backoff 1 s doubling to at most 60 s, at startup and at runtime. A successful rebuild always starts from a fresh engine and replay (5.3 step 4).
- Replaying the stored events must reproduce byte-identical status.json output (tested).
- Compaction/snapshots are **out of scope**.

### 5.5 Store interface (persistence/EventStore.h)

```cpp
class EventStore {
public:
    virtual ~EventStore() = default;
    struct StoreError { enum class Kind { Unavailable, Corrupt } kind; std::string message; };
    virtual void open(std::function<void()> onReady, std::function<void(const StoreError&)> onError) = 0;  // connect + ensure schema
    virtual void append(const Event&, std::function<void(std::uint64_t seq)> onCommitted, std::function<void(const std::string&)> onError) = 0;  // onError: commit outcome unknown (5.3 step 4)
    virtual void loadAll(std::function<void(std::uint64_t seq, const Event&)> onEvent, std::function<void()> onDone,
                         std::function<void(const StoreError&)> onError) = 0;
};
```

`loadAll` delivers events in ascending `seq` order with the row's actual `seq` (gaps are allowed). `onCommitted` receives the committed row's actual `seq`: `InMemoryEventStore` its own counter, `MariaDBEventStore` the result of `SELECT LAST_INSERT_ID()` on the same connection directly after the event `INSERT` (for a `GraphChanged` inside its transaction, before `commit`); an error of that `SELECT` is an unknown commit outcome (5.3 step 4). `app/` keeps the `seq` of the last committed event, from `loadAll` and from `onCommitted`, as the status.json/SSE event sequence. `tests/InMemoryEventStore` (pure, test-only) and `MariaDBEventStore` (SNode.C) implement it. The event-queue logic of 5.3 is written once against this interface and tested with `InMemoryEventStore`, including injected `append` failures.

---

## 6. External interfaces

### 6.1 Complete inbound route list

| Method and path | Authentication | Purpose |
|---|---|---|
| POST /api/v1/github/webhook | Raw-body HMAC-SHA256, X-Hub-Signature-256 | Push notifications: known source tag creations and registry default-branch pushes only; ping acknowledged. |
| POST /api/v1/builds/{buildId}/report | Callback HMAC | Workflow events below. |
| GET /api/v1/status.json | Public | Recorded unit status, 503 during recovery. |
| GET /api/v1/badge/{package}/{target}.svg | Public | Badge (4.6.1), always 200, unavailable during recovery. |
| GET /, /style.css, /app.js, /events | IDP and subject allowlist | Dashboard assets and SSE. |
| POST /api/{operation} | IDP and subject allowlist | retry, rebuild, reload, reconcile. |
| GET /auth/callback | IDP authorization-code validation | Complete login; protected access checks subject. |

No other routes are supported. Removed API paths return 404, including text status, DOT, unit retry/rebuild, reload and reconcile under /api/v1. There is no admin bearer token. Optional plain HTTP listener redirects all requests with 308 to the same path on the configured HTTPS origin.

Use SNode.C express routing, TLS, HTTP client and timer APIs. Per-path wrong-method handlers answer 405; a final fallback answers 404. Raw request limits: body 25 MiB for webhook, 64 KiB for all other routes, headers 64 KiB/100 fields, start line 8 KiB. Authenticate before parsing. Invalid encoding/form/required fields → 400; invalid authentication or callback timestamp skew over 300 seconds → 401. Badges use no-cache/no-store; status.json is application/json.

### 6.2 GitHub App authentication and complete outbound endpoints

Configuration: github-app-id, github-installation-id, github-app-key-file (RSA private key PEM). Read and validate the key at startup and read it again on each mint, never log its bytes or credentials. Sign JWTs with OpenSSL RS256: iat = now − 60 seconds, exp = now + 9 minutes, iss = app id. POST /app/installations/{installation_id}/access_tokens with that JWT. Cache the installation access token only in memory; reuse while now < expires_at − 5 minutes. Concurrent callers share a pending mint. On HTTP 401 from an authenticated repository API call, invalidate the rejected cached token, mint once and retry that exact call once. A second 401 fails. A failed token mint fails the waiting call; no recursive mint retries.

Complete endpoint shapes:
- POST /app/installations/{installation_id}/access_tokens
- GET /repos/{owner}/{repo}/contents/{path}?ref={ref}, Accept: application/vnd.github.raw
- GET /repos/{owner}/{repo}/tags (paginated)
- GET /repos/{owner}/{repo} (canonical spelling and default branch)
- GET /repos/{owner}/{repo}/commits/{branch} (registry HEAD or source bootstrap only)
- POST /repos/{owner}/{repo}/actions/workflows/{file}/dispatches

There are no Actions run or job lookups. No static bearer credential is accepted or configured. Repository calls use installation tokens; token mint uses the signed JWT. All use X-GitHub-Api-Version: 2022-11-28 and User-Agent core-ci.

Dispatch JSON remains {"ref":"<default branch>","inputs":{...}} with string inputs: build_id, package, target, version, revision, callback_url; builds additionally source_repo, tag and prerequisites, publications additionally build_repo, build_run_id and artifact. Dispatch 204 emits DispatchAccepted; other statuses emit DispatchRejected (transport failure status 0). No workflow dispatch for presentation exists.

Outbound TLS verifies the CA chain and hostname before sending request bytes, sets SNI, and uses system CA trust or github-ca-cert. One request per connection, 30-second deadline, no redirects. Pagination accepts only links on github-api-url's exact scheme/host/port and rejects loops. One 30-second engine timer enqueues Tick only when nextDeadline is due. There is no GitHub polling timer.

### 6.3 Configuration

Non-secret options use SNode.C configuration, command line overriding configuration file. Keys:
registry, target-catalog, workflow-repository, github-app-id, github-installation-id,
github-app-key-file, callback-base-url, github-api-url, github-ca-cert, db-host,
db-port, db-socket, db-user, db-name, build-timeout, publish-timeout, retry-delay,
max-dispatch-attempts, max-concurrent-builds, reconcile-interval, listen-address,
listen-port, tls-cert, tls-key, idp-base-url, idp-client-id, idp-public-key-path,
dashboard-users, dashboard-redirect-port.

Registry, target-catalog, workflow-repository, all three GitHub App keys,
callback-base-url, db-user, db-name, TLS files, idp-client-id, idp-public-key-path and
dashboard-users are required. App and installation ids are positive decimal ids.
Defaults: github-api-url https://api.github.com (HTTPS only); db-host localhost,
db-port 0, db-socket empty uses MYSQL_UNIX_ADDR; build-timeout 21600 and publish-timeout
3600 seconds (60–86400), retry-delay 600 (0–86400; zero disables), max-dispatch-attempts
8 (1–20), max-concurrent-builds 0/unlimited (0–1000), reconcile-interval 900 (60–86400),
listen-address 0.0.0.0, listen-port 8443, dashboard-redirect-port 0/off.
Missing or invalid settings fail before connecting and log only the option name.
Required secret environment variables: CORECI_WEBHOOK_SECRET and CORECI_CALLBACK_SECRET.
Optional CORECI_DB_PASSWORD (empty permits passwordless socket authentication).
No other production CORECI environment variables remain.

### 6.4 Callbacks, write-ahead and recovery

Callbacks are application/x-www-form-urlencoded, authenticated with
X-CoreCI-Timestamp and X-CoreCI-Signature-256: sha256=HMAC(secret,timestamp+"."+rawBody).

| phase | status | Required payload beyond phase/status |
|---|---|---|
| build | started | run_id, run_attempt |
| build | succeeded | run_id, artifact |
| build | failed | run_id, reason |
| publish | started | run_id, run_attempt |
| publish | succeeded | run_id, feed_commit |
| publish | failed | run_id, reason |

Publish started is a persisted PublishStarted event with publisher run id, attempt and start time.
Unused fields are ignored; an empty required value is absent. The router decodes buildId once.
Responses follow 5.3; duplicate/stale callbacks are ignored. Callback-before-dispatch,
dispatch backoff/deadlines, automatic retry, startup resumption and recovery are unchanged.
Webhooks remember the last 1000 successful delivery ids in memory. Failed loads are retryable.

MariaDB uses exactly one writer per database (GET_LOCK), retains the existing asynchronous
client and unknown-commit-outcome recovery. No production access is required for acceptance.

### 6.5 SSE dashboard

Preserve the existing layout and information about packages, targets, versions, dependencies,
attempts and workflow phases. Derive all phase information solely from committed engine events:
for build and publish, expose start, end, duration, state and run link
https://github.com/{workflowRepo}/actions/runs/{run_id}[/attempts/{run_attempt}].
Unknown values stay absent; dispatch acceptance must not invent a workflow start.
Retain publication dispatch/finish times and assignment order from recorded events.
Drop GitHub job queued/cancelled states, job cache, job log links and unsupported metadata
(proof, verified_at, uncertainty, publish_assigned, publish_run_bound, retire action, step).

Assets use no-cache. SSE sends a snapshot on connect and full model updates at most once per
second on committed events or successful declaration loads; no updates during recovery.
Keep alive every 30 seconds, at most 64 streams (excess 503). Until declarations are loaded,
ready is false and units are empty. A one-second SSE timer may coalesce events and check sessions;
it never requests GitHub data. Dependencies sort descending by level and allocation order.

Supported POST /api/{operation}: retry (failed unit), rebuild (settled unit), reload
(declarations and tags), reconcile (same reconciliation path). Unit operations take
{"package":"...","target":"..."} and answer after commit; reload/reconcile answer after loading.
Every other operation answers "Not supported by core-ci". The Refresh control sends reload.

IDP integration stays with snodec-auth 2: authorization code + PKCE S256/state/nonce,
openid profile email, MFA ACR urn:snodec:acr:mfa, RS256 public-key verification and
introspection on each protected request. Only validated subjects in dashboard-users are admitted;
email is display-only. Cookie coreci_access_token is HttpOnly/Secure/SameSite=Lax, path /,
lifetime 12 hours. Re-introspect open streams every 60 seconds and close invalid sessions.

### 6.6 Public status

status.json is generated from recorded unit state by the existing handwritten app/ writer,
without a JSON library. It contains the event sequence, packages and unit selections, published,
inflight and failure records. It is public alongside badges. No presentation dispatch,
status workflow, generation/dirty bookkeeping for a presentation writer, registry presentation
field or status_url field exists.


---

## 7. GitHub side — to be specified (slim CI)

## 8. Tests (CTest) — acceptance criteria

### 8.0 Infrastructure

CTest only; no test framework or fetched dependencies. Keep pure core/persistence tests buildable
without SNode.C. Full qualification builds all adapters and runs full CTest twice against a
disposable MariaDB, with browser, IDP and fake GitHub fixtures. No skipped required tests.
InMemoryEventStore is compiled only for tests. Tests use fixture repositories, never production.

### 8.1 Remaining acceptance scenarios

Retain S01 diamond, S02 per-feed serialization, S03 superseding, S04 failure/retry,
S05 idempotency, S06 semantic tags, S07 dependency publication rebuild, S08 waiting tags,
S09 validation, S10 deadlines/backoff, S11 replay, S12 write-ahead/ambiguous-outcome recovery,
S13 typed event rows, S14 SQL encoding, S15 graph changes, S16 declarations, S17 badges,
S18 revision base, S19 public status, S21 target catalog, S22 stream, S23 operations,
S24 IDP authentication, S26 rebuild, S27 dashboard history and S28 automatic retry.
Retain randomized invariants (10000 seeded runs), MariaDB integration, local smoke,
dashboard browser state coverage and dashboard end-to-end tests. Remove tests of job lookup,
presentation, text/DOT rendering and admin routes.

### 8.2 Slim regressions

- JWT header/claims and RS256 signature verified with a test key pair; unreadable/non-RSA key fails.
- Installation token reused strictly before expires_at minus five minutes; refreshed at boundary;
  concurrent mint sharing; key reread; one remint/retry on 401, second 401 fails; failed mint fails.
- Source scan rejects static credential providers/environment names in production.
- Declaration HTTP requests use the selected tag commit; pagination and prerelease selection;
  source default-branch push ignored; malformed loads leave state unchanged.
- Registry reload adds/schedules units, retires removed units and preserves settled units.
- PublishStarted is HMAC parsed, persisted, replayed and records publisher run id/attempt/start;
  duplicate/stale callbacks and callback-before-dispatch are covered.
- Dashboard phases show recorded starts/ends/durations/states and run/attempt links in populated,
  changing and completed states, including restart; no jobs are fetched.
- Every removed route returns 404, retained public routes remain public, all four supported
  operations require IDP login/subject allowlist and unsupported operations report errors.
- Functional SSE snapshot/update/coalescing/keepalive/reconnect/recovery and session expiry,
  stream limit and HTTP redirect remain covered. Browser checks verify phase rows and actions.

### 8.3 Development constraints

Canonical checkout /home/voc/projects/snodec/core-ci only. Never consult pre-greenfield tags.
Build with -Wall -Wextra -Wpedantic -Werror. Use shared formatting copies and explicit targets.
No production access, deployment or tags. No compatibility with previous persisted histories:
fresh schema version 4 is required. One coherent SPEC PR, then implementation PR with full
qualification before merge. Record baseline/final physical source lines separately for
core/, persistence/, adapters/, app/ and dashboard assets; test lines separately.
