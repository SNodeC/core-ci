# core-ci — Implementation Specification for Codex

Read this entire document before writing any code. It is the complete specification. Where something is not specified, **stop and ask** instead of inventing behaviour. Do not add features that are not listed here.

---

## 0. Goal in one paragraph

`core-ci` is a small, single-threaded C++20 daemon that **orchestrates** package builds for a set of GitHub repositories (first deployment: the SNode.C ecosystem). It contains no knowledge of any particular repository, package, distribution, architecture or release: everything it builds is declared in JSON files in the participating repositories (section 3.3), so the same program can be deployed for other repositories by configuration alone. It does no expensive work itself: every build, every signing operation and every feed publication runs as a GitHub Actions workflow. `core-ci` only (a) receives notifications from GitHub, (b) maintains a dependency graph of packages × targets modelled in C++, (c) decides which unit is allowed to build or publish next, and (d) triggers the corresponding GitHub workflows via the GitHub REST API. Dependency chains, including diamonds, are resolved by `core-ci` and **never** by workflows triggering each other.

---

## 1. Non-negotiable constraints

1. **The graph is declared in the repositories and modelled in C++.** Packages, targets, feeds and edges are declared in JSON files in the participating repositories (section 3.3); core-ci reads them and builds the graph as C++ objects. The graph as a whole is never represented, stored, exported or logged as JSON — not in persistence, not for debugging; it is stored only in typed database tables (section 5.2). One exception: the dashboard stream of 6.5 lists, at runtime only, each execution's prerequisites for the browser; it is never stored, logged or read back. Human-readable output is a text table and Graphviz DOT only. core-ci code contains no repository-, package-, distribution-, architecture- or release-specific knowledge (test data excepted).
2. **No JSON for core-ci's own data.** Persistence (MariaDB, section 5), configuration and the callback protocol (section 6) are not JSON — no JSON columns, no JSON SQL functions, no JSON text stored in any column. JSON is used only inside `adapters/github/`: parsing GitHub webhook payloads, REST responses and the declaration files of section 3.3, and encoding REST request bodies. Tests of `adapters/github/` and the fake GitHub server in `tests/` may contain GitHub wire-format JSON and declaration files as test data. `#include <nlohmann/json.hpp>` (or any JSON library) outside `adapters/github/` is a defect. Two exceptions, never stored, logged or read back: the read-only HTTP response `GET /api/v1/status.json` of 7.8, written in `app/` by a small hand-written JSON writer (no JSON library; unit states, no graph edges); and the dashboard of 6.5 (`GET /events` and the requests and responses of `POST /api/<operation>`), encoded and parsed only inside `adapters/dashboard/`, which may use the same JSON library as `adapters/github/`.
3. **Expensive work happens only on GitHub.** `core-ci` never compiles, packages, signs, holds signing keys, downloads artifacts, or writes to a feed repository; it reads feed repositories only through the REST API, for 6.5.
4. **Only core-ci chains builds.** No workflow may trigger another repository's build: no `repository_dispatch` chaining, no `workflow_run` triggers, no pushes that trigger downstream builds. Existing direct chaining in DistributionPackages (e.g. SNode.C target push → MQTTSuite build) must be removed in phase 4.
5. **Feed publication is serialized by core-ci**, not by GitHub `concurrency:` groups. (GitHub concurrency groups keep at most one pending run and cancel older pending runs — that silently drops publications.)
6. **Single-threaded.** No `std::thread`, no mutexes, no blocking sleeps, no polling loops. Everything runs in the SNode.C event loop; timers use SNode.C's timer facility.
7. **The engine is pure.** The scheduling engine (section 4) performs no I/O, reads no clock, generates no randomness. Time arrives only as the `at` timestamp carried by every event (section 4.3). Same event sequence ⇒ same state and same commands, always.
8. **Never pin to commits.** Workflows are dispatched on each repository's configured default branch (do not assume `main` — some repositories use `master`, and default branch names are never renamed). GitHub Actions are referenced by tag (e.g. `@v4`), never by commit SHA.
9. Build flags: `-Wall -Wextra -Wpedantic -Werror`. Every phase ends with a clean build and all tests passing.
10. **All development happens in the local repository `/home/voc/projects/snodec/core-ci`.** It already contains an earlier implementation. That code is discarded: Phase 0 (section 9) removes it, and nothing from it may be copied, adapted, consulted or reintroduced later. One owner-approved exception: the dashboard assets of 6.5 (HTML, CSS and JavaScript) may be reused and adapted; no other part of the earlier implementation. Development starts on a green field.

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
    Status.h/.cpp           # derived status, text table, DOT output
  persistence/              # library coreci_persistence — pure C++20, no SNode.C, no database
    EventRow.h/.cpp         # Event <-> typed row mapping (pure, fully tested)
    EventStore.h            # asynchronous store interface
    InMemoryEventStore.h/.cpp  # test implementation
  adapters/mariadb/         # MariaDBEventStore — implements EventStore with SNode.C's MariaDB client
    Schema.sql
  adapters/github/          # GitHub REST client, webhooks, HMAC, declaration loading (only place with JSON)
  app/                      # SNode.C express server, wiring, timers, main() — knows no repository
  tests/                    # plain CTest executables (no test framework) — see section 8
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

Every `branch` is the repository's default branch as reported by GitHub when the declarations were loaded — never assumed, never pinned. Repository owners and names are kept in GitHub's canonical spelling — the `full_name` returned by `GET /repos/{owner}/{repo}` during loading, however the registry spells them — and are never lowercased; that spelling is stored, shown in status and DOT, and used in API URLs. As identities they are compared ASCII case-insensitively, as GitHub does: uniqueness (3.2), webhook matching, and the `sourceRepo`/`buildWorkflow` change checks of rule 13 and 4.7. Pipeline equality compares the spelling exactly, so a change of spelling alone is recorded as `GraphChanged` without any other effect. Package names, feed ids and target ids are case-sensitive.

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

The graph is declared in JSON files, read through the GitHub REST API at the HEAD of each repository's default branch (never a feature branch, never a pinned commit). core-ci contains no knowledge of the declared repositories; deploying it for other repositories needs only another `registry` setting.

**Registry.** The `registry` setting names one file as `<owner>/<repo>:<path>`, where `<path>` is a relative path of at most 255 characters whose `/`-separated segments are non-empty, consist of `A–Z a–z 0–9 . _ -` and are not `.` or `..` (first deployment: `SNodeC/DistributionPackages:ci/registry.json`). core-ci reads only these fields and ignores all others:

```json
{
  "feeds": { "<feed id>": { "publisher": "<owner>/<repo>/.github/workflows/<file>" } },
  "default_feed": "<feed id>",
  "build": "<owner>/<repo>/.github/workflows/<file>",
  "repositories": [ "<owner>/<repo>" ],
  "targets": "<relative path in the registry repository>"
}
```

All five fields are required (`targets` from Phase 4e; first deployment `ci/targets.json`). `build` is the build workflow (section 7.1) of every package. A workflow reference has exactly the form shown; its workflow file name is the last path segment.

**Package declaration.** Every listed repository contains `ci/packages.json`. core-ci reads only these fields; the target details come only from the target catalog below:

```json
{
  "package": "<package name>",
  "depends_on": [ "<package name>" ],
  "accept_prereleases": false,
  "revision_base": 0,
  "matrix": [ { "id": "<target id>", "feed": "<feed id>" } ]
}
```

`package`, `depends_on` and `matrix` (with `id` in every entry) are required; `accept_prereleases` defaults to `false`; `revision_base` (an integer 0–1 000 000) defaults to `0`; an entry's `feed` defaults to `default_feed`. The package's `sourceRepo` is its repository, its `buildWorkflow` the registry's `build`. A wrong JSON type, a duplicate object key, a file larger than 1 MiB, or malformed JSON is invalid. An optional top-level `title` (string) is kept for the dashboard (6.5).

**Target catalog.** The registry's `targets` file lists every supported target exactly once: `{"targets": [ { "id": "<target id>", ... } ]}`, each entry with exactly the target fields the workflows use today (`distribution`, `suite`, `arch`, `build` and the rest), moved out of `ci/packages.json`. Target ids are unique. A matrix entry of `ci/packages.json` contains only `id` (a catalog id) and optionally `feed`; an id missing from the catalog, a duplicate id, or any other field in a matrix entry invalidates the load. core-ci keeps `distribution`, `suite`, `arch` and `build.format` of every catalog entry for the dashboard; the workflows read the remaining fields from the catalog. Same file rules as above.

**Loading.** A load fetches the registry; the default branch of every listed repository and every workflow repository (`GET /repos/{owner}/{repo}` → `default_branch` and the canonical `full_name`); and every `ci/packages.json` (`GET /repos/{owner}/{repo}/contents/{path}?ref=<branch>` with `Accept: application/vnd.github.raw`; path and `ref` are percent-encoded). Each repository is read at the HEAD of its default branch; the load also fetches the target catalog from the registry repository and records, per repository, the commit SHA of that HEAD (`GET /repos/{owner}/{repo}/commits/{branch}`) for the dashboard. It builds the `Pipeline` with 3.2. Any failure — HTTP error, missing file, invalid file, validation error — fails the whole load: the graph stays unchanged, and `app/` shows the failure (repository, file, reason) as the first line of `/api/v1/status` until a later load succeeds (this line is not part of the engine status of 4.6). A successful load enqueues `GraphChanged` (rule 13); the engine ignores it when nothing changed, and a `GraphChanged` answered unavailable is retried by the next trigger.

**Triggers.** A load runs after startup (after the listener is up, before the first tag reconciliation); on a webhook `push` to the default branch of the registry repository or of a repository in the current graph; every `reconcileInterval`, before tag reconciliation; and on `POST /api/v1/reload`. At most one load runs at a time; triggers during a load schedule exactly one further load afterwards.

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
7. **BuildFailed / PublishFailed / deadline exceeded on Tick:** `failure = {inflight.version, inflight.inputs, reason}`, clear `inflight`, remove from feed queue/slot. Dependents stay blocked (the unit is not settled). A new tag or new prerequisite publication changes the desired inputs and therefore automatically allows a retry; otherwise only `RetryRequested` clears `failure`.
8. **Acceptance** is defined only by the table in section 4.7. Ignored events are not persisted and change nothing, so duplicates are idempotent. The engine exposes a pure, `const` `classify(event) → Accept | Ignore(reason)` implementing that table (needed by section 5).
9. `BuildStarted` and `BuildSucceeded` record `runId`; `BuildSucceeded` also records `artifact` (both used by `DispatchPublish`). A build callback accepted in `Dispatching` (or a publish callback in `PublishDispatching`) proves the run exists: first clear `retryAt` and enter `Building` (`Publishing`) with deadline = `engineTime` + timeout, then apply the event.
10. Optional global `maxConcurrentBuilds` (default unlimited); a unit occupies a slot while its phase is `Dispatching` (also while waiting for `retryAt`) or `Building`; when no slot is free, ready units wait.
11. The engine exposes `nextDeadline()` (earliest pending timeout or retry-backoff time, or none). `app/` only enqueues a `Tick` when the current UTC time ≥ `nextDeadline()`, so every `Tick` changes state. Before the first deadline exists no `Tick` is needed: every deadline is created by an event that itself carries `at`.
12. **Resumed** (enqueued once by `app/` after replay, section 5.4): re-emit the command, with unchanged `buildId` and `attempt`, for every unit in `Dispatching`/`PublishDispatching` without `retryAt` (its dispatch result was lost); units waiting for `retryAt` keep their backoff. Like rule 4, this re-emission does not re-check readiness.
13. **GraphChanged:** replace the pipeline. State is keyed by package name and by `(package, target)`. Packages and units that remain keep their state. Packages and units no longer in the pipeline become **dormant**: their `inflight` is cleared (later callbacks are ignored, a held build slot is freed); `released`, `published`, `failure` and `lastRevision` are kept, together with the package's last `sourceRepo` and `buildWorkflow` and each unit's last feed, but neither evaluated nor shown. If they reappear in a later `GraphChanged` they continue from that state — the change rules below are applied against these retained values — so revisions are never reused. Changed prerequisites act through the derived predicates: a unit whose `published.inputs` no longer match its new `desiredInputs` is rebuilt, and builds with outdated inputs are superseded by rules 2(a) and 5. A unit whose feed changes loses `published` and `failure` (its packages are not in the new feed), keeps `lastRevision` and is rebuilt into the new feed. If a package's `sourceRepo` changes, its `released` is cleared; the next tag reconciliation re-establishes it from the new repository. If a package's `sourceRepo` or `buildWorkflow` changes, its units in `Dispatching` or `Building` are abandoned like dormant ones and rebuilt through the new workflow with a new revision.

14. **RebuildRequested:** set `rebuild`. The unit is no longer satisfied, so rule 2 rebuilds the same version with a new revision and it is published normally; its dependents on the same target follow through their `desiredInputs`. A failure of that build is handled by rule 7 and `RetryRequested`; `rebuild` stays set until the publication.

### 4.6 Status (core/Status.h)

Per unit, derive one of: `NoRelease`, `Settled`, `Waiting(on <unit>)`, `Ready`, `Dispatching`, `Building(run <id>)`, `AwaitingPublish(position n on <feed>)`, `Publishing`, `Failed(<reason>)`, `Blocked(by <failed unit>)`. Render as a plain-text table and as Graphviz DOT (nodes coloured by status).

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
| `PublishSucceeded`, `PublishFailed` | `buildId` matches; phase `PublishDispatching` or `Publishing` |
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

`coreci_schema` holds exactly one row, `version = 3`. In `open`: if `coreci_schema` has no row and all `coreci_*` tables are empty, insert `version = 3`; no row while any table has rows, more than one row, or a version other than 3 → `Corrupt` (older databases are not migrated). Every event type uses only its own columns; the others are `NULL`.

**GraphChanged storage.** A `GraphChanged` is one `coreci_events` row (`event_type = 'GraphChanged'`, only `event_at` set) plus its graph rows, written atomically on one connection as `startTransactions` (autocommit off) → the event `INSERT` → the graph `INSERT`s → `commit` → `endTransactions` (autocommit on again), as implemented in SNode.C's `MariaDBClientASyncAPI.cpp` (`MariaDBAutoCommitCommand`, `MariaDBCommitCommand`). The graph rows use `LAST_INSERT_ID()` as `event_seq`, the second allowed non-literal expression. `onCommitted` is called only after `endTransactions` succeeded. On any error in this sequence the store aborts the connection instead of issuing `rollback` (the server discards uncommitted work and releases the writer lock) and reports an unknown commit outcome (5.3 step 4); a connection is never reused with autocommit off. `normalize` checks a `GraphChanged`'s strings against these columns. The graph tables use `utf8mb4_bin`, so identifiers compare exactly as in C++. `loadAll` reads the four graph tables ordered by primary key and attaches them to their `GraphChanged` rows; a `GraphChanged` without graph rows is the empty pipeline; graph rows without their `GraphChanged` row, or rows that fail 3.2, are `Corrupt`. Schema migrations are out of scope.

Every event field from 4.3 maps to exactly one typed column; unused columns are `NULL`. The mapping `Event ↔ EventRow` lives in `persistence/EventRow.cpp`, is pure and is round-trip tested for every event type.

**Field rules** (`persistence/EventRow`, pure): `normalize(event) → Event | Invalid(reason)`. Identifier fields (`package`, `target`, `tag`, `build_id`, `artifact`) must be non-empty valid UTF-8 without NUL and fit their column, counted in characters as MariaDB `utf8mb4` does. `source_sha` and `feed_commit` are exactly 40 lowercase hexadecimal characters. Numbers must fit their column (`run_attempt`, `attempt` ≥ 1; `http_status` 0–999). Free text (`message`, `reason`) is never rejected: invalid UTF-8 → U+FFFD, NUL removed, truncated to 4096 bytes at a character boundary. Every field listed for an event in 4.3 is required; `message`/`reason` may be empty. **Decoding** a row with an unknown `event_type`, an invalid `kind`/`origin`, a missing required field, a non-NULL field the event type does not use, or a value violating these rules is fatal: `loadAll` reports it through `onError` naming the row's `seq`, and core-ci refuses to start.

### 5.3 Write-ahead ordering (critical)

All inbound events (webhook, workflow callback, dispatch result, tick, admin request) go through **one FIFO event queue** and are processed strictly one at a time:

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
- Replaying the stored events must reproduce byte-identical status output (tested).
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

`loadAll` delivers events in ascending `seq` order with the row's actual `seq` (gaps are allowed). `onCommitted` receives the committed row's actual `seq`: `InMemoryEventStore` its own counter, `MariaDBEventStore` the result of `SELECT LAST_INSERT_ID()` on the same connection directly after the event `INSERT` (for a `GraphChanged` inside its transaction, before `commit`); an error of that `SELECT` is an unknown commit outcome (5.3 step 4). `app/` keeps the `seq` of the last committed event, from `loadAll` and from `onCommitted`, as the `generation` of 7.8. `InMemoryEventStore` (pure, for tests) and `MariaDBEventStore` (SNode.C) implement it. The event-queue logic of 5.3 is written once against this interface and tested with `InMemoryEventStore`, including injected `append` failures.

---

## 6. External interfaces (app/ and adapters/github/)

### 6.1 Inbound HTTP (SNode.C express)

| Method & path | Auth | Purpose |
|---|---|---|
| `POST /api/v1/github/webhook` | `X-Hub-Signature-256` (HMAC-SHA256 over raw body, webhook secret, constant-time compare) | GitHub org webhook. Handle `ping` (200) and `push` where `ref` starts with `refs/tags/`, `created == true`, `deleted == false` → `TagObserved` for every package whose `sourceRepo` matches `repository.full_name`. Deduplicate by `X-GitHub-Delivery` (keep last 1000 ids). `push` to `refs/heads/<repository.default_branch>` of the registry repository or of a repository in the current graph → declaration load (3.3), `202 reload scheduled`. Everything else: `202 ignored`. |
| `POST /api/v1/builds/<buildId>/report` | `X-CoreCI-Timestamp` + `X-CoreCI-Signature-256: sha256=<hex HMAC(secret, timestamp + "." + body)>`; reject if clock skew > 300 s | Workflow callbacks. Body `application/x-www-form-urlencoded`: `phase` (`build`/`publish`), `status` (`started`/`succeeded`/`failed`), `run_id`, `run_attempt`, `artifact`, `feed_commit`, `reason`. Maps to the events in 4.3. Responds `200 accepted` or `200 ignored: <reason>`. |
| `GET /api/v1/status` | none | Text table (`text/plain`). |
| `GET /api/v1/graph.dot` | none | DOT (`text/vnd.graphviz`). |
| `GET /api/v1/status.json` | none | Presentation data of 7.8 (`application/json`); `503` during recovery. |
| `GET /api/v1/badge/<package>/<target>.svg` | none | Badge of 4.6.1 (`image/svg+xml; charset=utf-8`, `Cache-Control: no-cache, no-store, must-revalidate`). Always `200`: an unknown unit gets the `unknown` badge. |
| `POST /api/v1/units/<package>/<target>/retry` | `Authorization: Bearer <admin token>` | `RetryRequested`. |
| `POST /api/v1/units/<package>/<target>/rebuild` | admin token | `RebuildRequested`. |
| `POST /api/v1/reconcile` | admin token | Trigger tag reconciliation now. |
| `POST /api/v1/reload` | admin token | Trigger a declaration load now (3.3); `202 started` or `202 already running`. |

Every inbound request that carries an event goes through the event queue of 5.3 and is answered only after its event row is committed (or with `200 ignored` / `503`).

**Routing uses SNode.C's express router.** One route per endpoint, registered with the router's method functions (`get`, `post`) and `:name` path parameters; path parameters are percent-decoded exactly once (verify whether the router already decodes them). The size limit, webhook HMAC, callback HMAC and admin token are separate middleware chained in front of exactly the routes that need them; a `405` handler per path and a final `404` fallback. Hand-written dispatch on `req->path` or `req->method` (string comparison or regular expression) is forbidden. The order size check → authentication → parsing is unchanged.

Use SNode.C's express API and TLS server. **Do not guess SNode.C APIs**: find the express router, HTTP client and timer facilities in the SNode.C repository (master HEAD) and its examples/apps, and cite the source files you based your code on in the PR description.

### 6.2 Outbound GitHub REST (adapters/github/)

- Dispatch: `POST /repos/{owner}/{repo}/actions/workflows/{workflowFile}/dispatches` with body `{"ref": "<branch>", "inputs": {...}}` (all values strings), headers `Accept: application/vnd.github+json`, `X-GitHub-Api-Version: 2022-11-28`, `Authorization: Bearer <token>`. `204` → `DispatchAccepted`; other status → `DispatchRejected`; transport error → `DispatchRejected` with `httpStatus = 0`. Both carry the command's `buildId`, `kind` and `attempt`.
- TLS: outbound connections use SNode.C's TLS HTTP client (`src/web/http/tls/in/Client.h`), set SNI to the host, and verify the server certificate against the system CA store (`setCaCertDirUseDefault`) or, if configured, the `githubCaCert` file. `setCaCertAcceptUnknown` is forbidden. The API base URL is `githubApiUrl` (default `https://api.github.com`). HMAC-SHA256 and its constant-time comparison use OpenSSL (`CRYPTO_memcmp`), already an SNode.C dependency.
- Token: fine-grained PAT from an environment variable, behind an interface `TokenProvider` so a GitHub App provider can be added later (do not implement the App now).
- **Tag reconciliation** (webhooks do not deliver tag events reliably, e.g. when several tags are pushed at once): after every declaration load attempt (at startup and every `reconcileInterval`, default 15 min) and on `/api/v1/reconcile`, list tags of every `sourceRepo` (`GET /repos/{owner}/{repo}/tags`, follow `Link` pagination) and emit `TagObserved{origin=reconcile}` for the highest accepted version. The engine ignores it if it is not newer.
- An SNode.C timer fires every 30 s; it enqueues a `Tick` only if `now ≥ engine.nextDeadline()` (see 4.5 rule 11).

### 6.3 Configuration

Non-secret settings through SNode.C's configuration (command line and configuration file; the command line wins), secrets only through the environment: listen address/port, TLS cert/key, `registry`, `callbackBaseUrl`, MariaDB host/port/socket/user/database, `githubApiUrl`, `githubCaCert` (optional CA file, used by the smoke test with a self-signed fake GitHub), timeouts, `maxDispatchAttempts`, `maxConcurrentBuilds`, `reconcileInterval`, `statusInterval`. Secrets only via environment: `CORECI_GITHUB_TOKEN`, `CORECI_WEBHOOK_SECRET`, `CORECI_CALLBACK_SECRET`, `CORECI_ADMIN_TOKEN`, `CORECI_DB_PASSWORD`. Never log secrets.

### 6.4 Operational contract

**Configuration.** Non-secret options use SNode.C's configuration (command line overrides config file); invalid or missing required settings → log the option name (never a value) and exit non-zero before connecting anywhere. Server: SNode.C TLS express instance `coreci`, default port 8443, certificate and key required. `registry` required, form `<owner>/<repo>:<path>`. `callbackBaseUrl` required, `https://`. `githubApiUrl` default `https://api.github.com`, `https://` only. `githubCaCert` optional. `dbHost` `localhost`, `dbPort` 0, `dbSocket` empty = the client library's default socket `MYSQL_UNIX_ADDR` (core-ci substitutes it, because SNode.C passes an empty string rather than `nullptr` to `mysql_real_connect`); `dbUser`, `dbName` required. `buildTimeout` 21600 s and `publishTimeout` 3600 s (each 60–86400); `maxDispatchAttempts` 8 (1–20); `maxConcurrentBuilds` 0 = unlimited (0–1000); `reconcileInterval` 900 s (60–86400); `statusInterval` 60 s (30–3600). `CORECI_GITHUB_TOKEN`, `CORECI_WEBHOOK_SECRET`, `CORECI_CALLBACK_SECRET`, `CORECI_ADMIN_TOKEN` required and non-empty; `CORECI_DB_PASSWORD` optional (empty = passwordless, e.g. unix_socket).

**HTTP responses.** Authenticate first (HMAC over the raw body, or bearer token), then parse. Only exception: a path whose `:name` parameters SNode.C's router cannot percent-decode is answered `400` by the framework before any middleware runs (no secret is checked, nothing is parsed). Missing/wrong signature or token, or timestamp skew > 300 s → `401 unauthorized` (no detail). Missing `X-GitHub-Event`/`X-GitHub-Delivery`, wrong content type, malformed JSON or form, duplicate form keys, malformed percent escapes, missing required field → `400 invalid: <reason>`. Size limits are enforced while reading, through SNode.C's server parser limits (`ConfigHttpParser`, whose defaults are unlimited): body ≤ 25 MiB, header block ≤ 64 KiB, ≤ 100 header fields, start line ≤ 8 KiB. Then, **before** authentication, a body over 64 KiB on any route except the webhook → `413`. Unknown route `404`, wrong method `405`. core-ci parses outbound status codes with `std::from_chars` (anything but exactly three digits → transport error, `httpStatus = 0`) and never throws on them. Known upstream defect: SNode.C's `src/web/http/client/ResponseParser.cpp` calls `std::stoi` on the status code before core-ci sees the response, so a non-numeric status line terminates the process; it is reachable only from the hostname-verified GitHub endpoint, must be fixed in SNode.C as a separate change, and is not tested in core-ci. Status and DOT during recovery → `503`; badges never answer an error (a broken image would replace them) and show `unavailable` during recovery.

**Callbacks.** `build`+`started` → `BuildStarted` (needs `run_id`, `run_attempt`); `build`+`succeeded` → `BuildSucceeded` (`run_id`, `artifact`); `build`+`failed` → `BuildFailed` (`run_id`, `reason`); `publish`+`succeeded` → `PublishSucceeded` (`run_id`, `feed_commit`); `publish`+`failed` → `PublishFailed` (`run_id`, `reason`); `publish`+`started` → `200 ignored: no event`; anything else → `400`. Fields an event does not use are ignored; an empty value counts as absent. `buildId` is the percent-decoded path segment.

**Webhook.** `sourceSha` = payload `after` (for an annotated tag the tag object); informational only — workflows always check out `tag`. A delivery that maps to several packages enqueues one `TagObserved` per package in package-name order; the response is `503` if any result is unavailable, otherwise `200` with one line per package (`<package>: accepted` or `<package>: ignored: <reason>`). The delivery id is remembered only if no result was unavailable; in memory, last 1000 ids, oldest evicted, empty after restart.

**Reconciliation.** Per package the highest accepted version over all pages; among equal precedence the lexicographically smallest tag string. A failed, malformed or off-origin page aborts that repository for this round (warning, no event); other repositories continue. Startup reconciliation starts after the listener is up and does not delay it. `POST /api/v1/reconcile` answers `202 started` or `202 already running` immediately.

**Outbound HTTP and TLS.** One request per connection; 30 s from connect to complete response; redirects are not followed (3xx: dispatch → `DispatchRejected` with that status, not permanent; reconciliation → abort that repository). `Link` URLs must have the scheme, host and port of `githubApiUrl`. SNode.C's TLS client sets SNI but does not verify the hostname, so in the client's `onConnected` — called after the handshake and before the HTTP context is attached (`src/core/socket/stream/tls/SocketConnector.hpp`) — require `SSL_get_verify_result(ssl) == X509_V_OK` and `X509_check_host(peerCertificate, host) == 1`; otherwise close before any request byte is sent (transport error, `httpStatus = 0`).

**Recovery integration.** Authorized additions: `EventQueue::recover(reason)` (pending → unavailable, discard engine, rebuild) and a `MariaDBEventStore` connection-lost callback that `app/` wires to it. While the queue is not ready no `Tick` is generated and dispatch results are dropped with a log line; `Resumed` re-emits the dispatch after the rebuild.

**Single writer.** Exactly one core-ci per event database, enforced: `open` runs `SELECT GET_LOCK(CONCAT('coreci:', DATABASE()), 0)` after `SET autocommit = 1`; result `0` → `Unavailable` (another instance holds the writer lock), retried like any outage. The lock is per session and re-acquired on every reconnect.

**App library and smoke test.** There is one executable, `core-ci`; `pipeline/` does not exist. The smoke test runs `core-ci` itself against the fake GitHub, which serves a registry and five `ci/packages.json` files declaring the S1 test pipeline. The smoke script uses `CORECI_TEST_DB_*` (may drop and recreate the two tables there; exit 77 if unset) and shows: verified TLS dispatch to the fake GitHub; a wrong-host certificate rejected with no request reaching the fake; the S1 diamond driven by signed `curl` callbacks; a restart midway with correct `Resumed` dispatches; a second instance on the same database refused by the writer lock; a changed declaration (a new prerequisite) announced by a signed `push` webhook, recorded as `GraphChanged` and causing exactly the rebuild of rule 13; an invalid declaration (a cycle) keeping the previous graph and shown on the status page; the badge of `d@t1` showing `building` while it builds and its `version-revision` after publication.

### 6.5 Dashboard

Owner requirement: the realtime dashboard of the earlier SNodeC instance (release `6f87ad7-release`, captured on 2026-11-08 in `phase-4e-clarification/`) keeps its visual appearance pixel for pixel: for the same information it renders identically. Its data protocol is new and follows core-ci's model; there is no new design.

- **Assets.** `app/dashboard/index.html`, `style.css` and `app.js`, starting from the captured files. Only what the user sees must stay; code structure, data types and the protocol need not resemble the earlier system. Keeping the captured HTML, CSS and drawing functions and rewriting only the data layer of `app.js` (connection, message handling, mapping into the structures the drawing functions consume) is the expected way; any part may be restructured as long as G5 holds. The TLS listener serves them at `GET /`, `GET /style.css` and `GET /app.js` (query string ignored), with the captured `Content-Type` and `Cache-Control: no-cache`.
- **Stream `GET /events`** (SSE: `Content-Type: text/event-stream`, `Cache-Control: no-store`, a keep-alive comment every 30 s, at most 64 concurrent streams, more → `503`). On connect one `snapshot` event, then, whenever the dashboard model changes (a committed event, a job-cache update or a declaration load), at most one `update` event per second; both carry the complete model, written by `adapters/dashboard/`. The model is `status.json` (7.8) extended per package by `title` (from `ci/packages.json`, absent → `owner/repo`) and per unit by its catalog coordinates (`distribution`, `suite`, `arch`, `format`), its prerequisites on the same target (package names), its build and publish run (`runId`, `html_url`) and the job data of the job cache. While the queue recovers no event is sent; until the first successful declaration load since start, the model has `ready: false` and no units.
- **Mapping in the browser.** `app.js` maps each unit onto what its drawing functions show: `Waiting`, `Blocked` → `waiting`; `Ready`, `Dispatching`, `Building` → `building`; `AwaitingPublish`, `PublishDispatching`, `Publishing` → `publishing`; `Settled` → `published`; `Failed` → `failed`; plus version, revision, tag, runs and job data. Information the model does not carry (older attempts, uncertainty, retirement, enrollment) is absent, and the drawing functions show what they show when it is absent.
- **Job cache** (in `app/`, in memory, refilled after a restart): for every unit with a build or publish run, the run found by its recorded `runId` or by its run name (= `buildId`) through `GET /repos/{owner}/{repo}/actions/runs` and its `jobs`; the job shown is the workflow's main job (`build` in `coreci-build.yml`, `publish` in `coreci-publish.yml`) of the run's latest attempt; refreshed every 30 s while the run is not completed, then kept; at most 4 requests in flight; an error leaves the fields absent.
- **Buttons.** `POST /api/<operation>` with core-ci-aligned JSON bodies. Retry (`{package, target}`) → `RetryRequested` under the rules of `POST /api/v1/units/<package>/<target>/retry`; Refresh → a declaration load (3.3). Every other button (Enroll, Request, Reconcile, Retire) stays visible and answers with the dashboard's own error line: `Not supported by core-ci: <reason>`. A request is answered after its event is committed.
- **Authentication.** The whole dashboard (assets, `/events`, `/api/<operation>`) requires a login through the OpenID Connect provider at `https://core.snodec.org:8088`, integrated with the `snodec-auth` client library (`find_package(snodec-auth 2)`, `snodec-auth::auth-client`) exactly as its `minimal-mfa-app` integrates it: authorization code with PKCE (S256), state and nonce; scopes `openid profile email`; required ACR `urn:snodec:acr:mfa`; public client without secret; RS256 validation against the configured public key; online introspection on every protected request; callback `GET /auth/callback`. core-ci uses its own cookie `coreci_access_token` (`HttpOnly`, `Secure`, `SameSite=Lax`, `Path=/`, lifetime 12 h); the configurable cookie name and lifetime are a small extension of the client library in `SNodeC/snode.c-idp`, made in this phase. Only accounts whose subject (the `sub` claim of the validated token, assigned by the provider and never chosen by the user) is listed in `dashboardUsers` are admitted (first deployment: the subject of the owner's account `me@vchrist.at`, looked up in the provider's account store at deployment); any other login → `403`. The `email` claim is shown, never used for access; no `email_verified` claim is required. An open stream is re-introspected every 60 s and closed when its session is no longer valid. Settings: `idpBaseUrl`, `idpClientId`, `idpPublicKeyPath`, `dashboardUsers`. The provider registers the client in its `IDP_CLIENTS` with the redirect `https://core.vchrist.at:8443/auth/callback` (a deployment step). The admin token keeps protecting only the `/api/v1` admin routes of 6.1.
- **Addresses.** The dashboard is served by the TLS listener (first deployment `https://core.vchrist.at:8443/`). The optional plain-HTTP listener `dashboardRedirectPort` (default off; first deployment 8081, once the earlier instance is retired) answers every request with `308` to the same path on the TLS listener.
- **Acceptance.** (G4) The report lists every difference of the three files from the captured ones. (G5) For each of the 28 reference screenshots, a fixture of the new model carrying the same information as the captured old state renders with a pixel difference of 0 (Chromium 156.0.8078.79, the reference fonts, locale `en-US`, time zone UTC, the same viewport and colour scheme); a reference showing information the new model cannot carry is listed as an exception. (G6) For fixtures covering every status kind, running and completed jobs and an empty model, the dashboard renders without a console error.

---

## 7. GitHub side (Phase 4)

### 7.0 Principles

- The GitHub side is three `workflow_dispatch` workflows and one composite action in the repository that holds the registry (first deployment: `SNodeC/DistributionPackages`): `.github/workflows/coreci-build.yml` (the registry's `build`), `.github/workflows/coreci-publish.yml` (the publisher of every feed), `.github/workflows/coreci-status.yml` (the presentation writer of 7.8) and `.github/actions/coreci-report/action.yml`. The only other workflow is the manually dispatched image maintenance `.github/workflows/coreci-prepare-rootfs.yml` (Raspberry Pi OS build images).
- Workflows never start other workflows: no `repository_dispatch`, no `workflow_run`, no `workflow_call` into other repositories, no push that starts a build. The only workflow a publication causes is the feed repository's GitHub Pages deployment, which builds nothing. No `concurrency:` groups.
- Actions are referenced by major version tag (e.g. `actions/checkout@v4`), never by commit SHA. Tooling is used from the checkout of the workflow's own run, never from a pinned revision.
- **One tooling package.** All workflow code is the Python package `coreci/` in DistributionPackages; no function exists twice. `core_ci/` and the earlier `core-ci-*.yml` workflows are removed (Phase 4g); the functions of `core_ci/` that the workflows use are moved into `coreci/`. The workflows use no JSON assignments, attempt evidence or controller-App identity checks; receipts and the status renderer are used as 7.8 specifies; old package files are removed only by the retention rule of 7.5.
- **Boundary:** the workflows contain no package-, repository- or target-specific code. Handling of package formats and distributions (package managers, repository setup, container image and SDK bootstrap) is generic tooling and allowed. Everything package-specific comes from the registry, the source repository's `ci/` directory and the selected matrix entry.

### 7.1 Registry fields used by the workflows

core-ci ignores these fields; the workflows require them. Every `feeds.<id>` additionally has `repository` (`<owner>/<repo>` of the feed repository), `url` (its public `https://` base URL, ending in `/`), `environment` (the GitHub environment holding this feed's signing keys and push credential) and `keys`: `gpg` (`file`, `fingerprint`; signs APT `Release` files, RPM packages, `repomd.xml` and the development-export checksums), `usign` (`file`, `key_id`; opkg) and `apk` (`file`, `sha256`). The registry is the trust anchor: a key served by a feed is used only if it matches the registry. The registry may additionally have `badges`, the base URL of core-ci's badge route (staging: `https://core.vchrist.at:8443/api/v1/badge`). A feed may have `presentation`, the workflow reference of its presentation writer (7.8; production: `SNodeC/DistributionPackages/.github/workflows/coreci-status.yml`); core-ci reads this field and the feed's `repository` (6.5). With a `presentation`, the registry also has `status_url`, the full `https://` URL of core-ci's `GET /api/v1/status.json` (first deployment: `https://core.vchrist.at:8443/api/v1/status.json`). The production feed's `url` is `https://raw.githubusercontent.com/SNodeC/Packages/main/`, the base URL its install documentation uses; GitHub Pages serves only its presentation (7.8).

### 7.2 Source repository contract

- `ci/` is read at the recipe ref: the source repository's default-branch HEAD, or the optional build input `recipe_ref` (a branch; for manual staging runs only, core-ci never sends it). Everything else — including recipe directories that hooks copy from the source tree, such as `supplement/openwrt` — comes from the checkout at `tag`.
- Catalog fields (3.3) used by the workflows, per target: `family` (`native`/`openwrt`), `distribution`, `suite`, `arch`, `runner` and `build` (native: `image`, `platform`, `format` `deb`/`rpm`, optional `dependencies`; openwrt: `target`, `release`, `format` `ipk`/`apk`). The workflows expand a matrix entry with its catalog entry into `TARGET_JSON`. `ci/packages.json` fields used by the workflows, besides those of 3.3: top-level `build` (the hook per family), `build_dependencies` (per format) and `development_packages` (`deb`, `rpm`: the binary packages a dependent installs to build against this package).
- Hooks receive exactly the variables that today's hooks and the existing `native_container.py`/`openwrt_container.py` consume, filled from the new inputs: `SOURCE_DIR`, `CI_DIR`, `TARGET_JSON`, `OUTPUT_DIR`, `SOURCE_VERSION`, `SOURCE_TAG` (replaces `SOURCE_COMMIT`), `PACKAGE_NAMESPACE`, `PACKAGE_UPSTREAM_VERSION`, `PACKAGE_REVISION` (the numeric revision; the tooling computes the encoded release with 7.3), `PREREQUISITES_DIR`, and for OpenWrt `SDK_DIR`, `SDK_STAGING_DIR` and `CORE_CI_APK` (path of the SDK's verified `apk` tool, set by the existing SDK preparation). `PREREQUISITES_DIR/<package>/metadata.json` (`package`, `version`, `revision`, `upstream_version`, `release`, `target`, `sdk`) replaces the old receipts; for OpenWrt that directory also holds the prerequisite's extracted development export (7.4). If a hook or tool consumes any other variable, stop and ask. `outputs.json` remains the existing description of what a build produced.
- No hook and no build step receives a secret.
- Hooks compile with as many jobs as the runner has CPUs (`nproc`). The OpenWrt hook updates and installs only the SDK feeds `base` and `packages` and its own recipe feed, and writes a bounded `.config` (`CONFIG_ALL`, `CONFIG_ALL_NONSHARED`, `CONFIG_ALL_KMODS`, `CONFIG_AUTOREMOVE` and `CONFIG_SIGNED_PACKAGES` not set, and `CONFIG_PACKAGE_<package>=m`), as the build before core-ci did (`SNodeC/DistributionPackages` `7a28f2b`, `ci/build/openwrt.sh`).

### 7.3 Package versions

Versions are encoded and validated by one function in `coreci/` (7.0), so that package versions follow the feed before core-ci (`SNodeC/Packages` at `6c1007a`): `MAJOR.MINOR.PATCH`; a prerelease as order-preserving tokens (`~p…` for deb, rpm and ipk, `_pre0_p…` for apk); build metadata dropped; release `N~<codename>` for deb (the target's suite: `trixie`, `forky`, `sid`, `noble`, `resolute`, `bookworm`), `N.<disttag>` for rpm (`fc43`, `fc44`, `el9`, `el10`, from the target's distribution and release), `rN` for ipk and apk, where `N` is the revision. Examples: `2.1.0-123~trixie`, `2.1.0-123.fc44`, `2.1.0-123.el10`, `2.1.0-r123`. An encoding longer than the tooling's 240-character limit fails the build with that reason. Packages published before this change carry the release `N` without suffix; `N~<codename>` sorts below `N`, so the first suffixed build of a version needs a revision above every unsuffixed one, which rule 2 guarantees because revisions only grow.

### 7.4 `coreci-build.yml`

Inputs exactly as core-ci dispatches them: `build_id`, `package`, `version`, `revision`, `target`, `callback_url`, `source_repo`, `tag`, `prerequisites` (comma-separated `name=version-revision`; the revision is the part after the last `-`), plus the optional `recipe_ref`.

1. Report `build` / `started`.
2. On the matrix entry's `runner`: read `ci/packages.json` of `source_repo` at the recipe ref and select the entry whose `id` is `target` (none → fail) and expand it with the catalog entry of that id from the registry repository (none → fail); its feed is the entry's `feed` or the registry's `default_feed`. Check out the source at `tag`. Prepare the container or the OpenWrt SDK with the existing tooling, without a CPU limit; install `build_dependencies` and the entry's `build.dependencies`.
3. Prerequisites, each at exactly `version-revision` from the feed's `url` (a missing one fails the build): **native** — install its `development_packages`, with the repository verified by the registry's `gpg` key; **OpenWrt** — download its development export for this target, verify `SHA256SUMS.asc` with the registry's `gpg` key and the file against `SHA256SUMS`, require that its recorded SDK (file name and SHA-256) equals this build's SDK, extract it into `PREREQUISITES_DIR/<package>/` and stage it into `SDK_STAGING_DIR` as the existing tooling does. Write `metadata.json` for every prerequisite.
4. Run the hook. Packages stay **unsigned**. Verify every produced package with the existing inspection: its name belongs to `PACKAGE_NAMESPACE` and the components in `outputs.json`, version and release follow 7.3, and the architecture is the entry's or architecture-independent (`all`, `noarch`).
5. Upload one artifact `coreci-<build_id>` (retention 7 days): the packages, the OpenWrt development export if one was produced, and `manifest.json` (`build_id`, `package`, `version`, `revision`, `target`, `feed`, the matrix entry, for OpenWrt the SDK file name and SHA-256, and every file with its SHA-256).
6. Report (`if: always()`): `succeeded` with `artifact`, otherwise `failed` with a reason (also for `cancelled`).

Only the report steps receive `CORECI_CALLBACK_SECRET`; apart from `github.token` no other step receives a secret.

### 7.5 `coreci-publish.yml`

Inputs exactly as core-ci dispatches them: `build_id`, `package`, `version`, `revision`, `target`, `callback_url`, `build_repo`, `build_run_id`, `artifact`.

1. **Resolve and check provenance.** `GET /repos/{build_repo}/actions/runs/{build_run_id}` must show: the repository of the registry's `build`, its workflow path, event `workflow_dispatch`, conclusion `success`, and `head_branch` equal to the branch this publisher run uses. Download `artifact` from that run; verify `manifest.json` against the inputs and every file against its SHA-256; take the feed from the manifest and fail unless the registry names this workflow file as that feed's publisher. Output the feed's `environment`.
2. **Immutability.** The ledger `coreci/published/<target>/<package>_<version>-<revision>.sha256` in the feed lists the manifest's unsigned files with their SHA-256. An identical existing ledger makes the publication a no-op: no commit, report `succeeded` with the current feed commit. A different existing ledger, or an existing target file without a ledger, fails the publication (`immutable`).
3. **Publish** (in the feed's environment, on a sparse checkout of the feed that materializes only the files this target reads or writes): sign the packages (rpm, apk), add them in the existing layout and apply retention (below), regenerating and signing only the indices of directories that received or lost files:
   - APT: packages under `<distribution>/pool/<suite>/`; `<distribution>/dists/<suite>/main/binary-<arch>/Packages` and `Packages.gz`; the suite-level `Release`, `InRelease` and `Release.gpg` for the whole suite.
   - RPM: `<distribution>/<suite>/<arch>/` with `repodata/` and `repomd.xml.asc`.
   - OpenWrt: `openwrt/<suite>/<arch>/` with `Packages`, `Packages.gz` and `Packages.sig` (opkg) or the APKv3 `packages.adb` (apk).
   - OpenWrt development exports: `dev/<target>/<package>_<version>-<release>.tar.gz`, with `dev/<target>/SHA256SUMS` and `SHA256SUMS.asc`.
   - **Retention:** in this target's directories (for APT: the files of the target's architecture in the suite's pool), the package's files (those of its `PACKAGE_NAMESPACE`, including files published before core-ci) and its development exports are kept only for the two newest publications of the package on this target (ordered by version, then revision), the new one included; files of older publications are removed. Architecture-independent files (`all`, `noarch`), ledgers and receipts are never removed. Step 2 fails a publication whose ledger exists but whose files were removed by retention (`retired`) before applying its no-op rule.
   Then write the ledger. For a feed with `presentation`: write the receipt and render the presentation (7.8); no badge block. Otherwise, if the registry has `badges`, regenerate the badge block in the feed's `README.md` between `<!-- coreci:badges:begin -->` and `<!-- coreci:badges:end -->`: if both markers are absent the block is appended at the end after a blank line; any other number of markers fails the publication. One row per target (sorted by id), in each row the badge `<badges>/<package>/<target>.svg` of every package that has this target (sorted by name), taken from the registry and every listed `ci/packages.json` at HEAD. Everything else in the feed stays byte-identical, apart from the removals of the retention rule.
4. **Validate before pushing**, against the prepared checkout served locally (not the public URL), in a fresh container of the entry's `build.image`/`build.platform` (QEMU for foreign architectures; only the emulator the target's architecture needs is registered, none for the runner's own architecture), with the feed configured as its install documentation describes and the key taken from the registry: APT `apt-get update` without signature warnings and `apt-get install --dry-run` of the new packages at their version; RPM `dnf makecache` and `dnf install --downloadonly` of the new packages (exit code 0); OpenWrt, if an official rootfs image exists for suite and architecture, `opkg update` and `opkg install --noaction`, or `apk update` and `apk add --simulate`; otherwise verify signatures and index consistency with the SDK tools and say so in the job summary.
5. Commit with the existing orphan-root scheme and push with `--force-with-lease=<branch>:<sha fetched at checkout>`. core-ci serializes publications per feed; the only other writer is the presentation writer (7.8). On a lease failure, fetch the feed again and repeat steps 2–5 (immutability included) on the new head, at most 5 attempts with a random 5–30 s pause; then fail.
6. **Availability:** wait until the feed's `url` serves this target's index files with the pushed content (compared by SHA-256), at most 15 minutes. Then report `publish` / `succeeded` with `feed_commit`; on timeout or any earlier error report `failed`.

Signing keys and the feed push credential exist only in the feed's environment. An environment holding production keys allows deployments from the default branch only.

### 7.6 `coreci-report`

Composite action: builds the form body (`phase`, `status`, `run_id`, `run_attempt`, `artifact`, `feed_commit`, `reason` truncated to 1000 bytes), takes `timestamp`, computes `HMAC-SHA256(secret, timestamp + "." + body)` with `openssl dgst -sha256 -hmac`, and sends it with `curl --fail --retry 5 --retry-all-errors` to `callback_url`, used verbatim (core-ci sends it complete and percent-encoded). The secret is the organization secret `CORECI_CALLBACK_SECRET`. An empty `callback_url` makes the action do nothing; core-ci never sends one, manual staging runs do.

### 7.7 Staging

Phases 4b and 4c test only against `SNodeC/Packages-staging` (GitHub Pages enabled), the environment `package-publication-staging` with keys generated for it (never the production keys), and a test registry on a DistributionPackages branch whose feed, keys and fingerprints point to the staging repository; bootstrapping the staging feed may commit its public keys to its `keys/`. Staging runs are dispatched manually with `--ref <test-registry branch>`, core-ci's exact inputs, `recipe_ref` for the source-repository branches, and an empty `callback_url`; the proof is the run results and the staging feed. No run in 4b, 4c or 4c2 writes to the production feed or uses a production environment.

### 7.8 Presentation

Owner requirement: the presentation of `SNodeC/Packages` on github.com — `README.md`, `status/**` without the Pages-only HTML files, and `docs/**` with their assets, as listed in the baseline manifest of the 4d inventory (`phase-4d/baseline/manifest.json`, commit `f1271dd923e48dcbee04fe5b0eefbf240fad53b0`) — stays pixel-for-pixel identical: same files, same templates, same renderer, same structure; only the data changes. The Pages-only files (`index.html`, `status/index.html`, `status/*.html`) and the feed file `status.json` are not produced (Phase 4h); `GET /api/v1/status.json` of core-ci is unaffected. From Phase 4h, `status/README.md` and every project status page start with the page header of the docs pages (`<!-- snodec:begin page-header -->` … `<!-- snodec:end page-header -->`, as in `docs/installation/snodec/README.md` of the feed at commit `ed2fe778e9c2640fd27b00197bd9cb57c4f85c1c`): the same markup, the same buttons in the same order (account, organization, repository) and the same anchors, with only three differences — the banner `banner-package-status.svg` with its compact variant, the up link (a project page to `status/README.md`, `status/README.md` to `README.md`) and the relative asset paths. Everything below the header is unchanged.

- **Renderer.** The templates in `status-templates/` stay byte-identical, apart from the page header of Phase 4h. The renderer is `core_ci/feed_status.render` copied to `coreci/presentation.py` (7.0) with exactly two changes: the source repository of a publication comes from the receipt field `source.repository` when present (otherwise, as before, from the build-run URL); and whether a feed file exists is answered by an injected function, so the presentation writer renders from the feed's Git tree (`git ls-tree`) without package blobs. G1 holds for the copy. `coreci/` produces the inputs in their existing schemas: a receipt under `receipts/` per publication, written by the publisher with every field the renderer reads plus `source.repository`; and `status/controller.json`, projected from `status.json` joined with the registry, the target catalog and every listed `ci/packages.json` at HEAD (coordinates from the catalog; prerequisites and titles from the declarations). Identifiers the old controller generated (execution and attempt ids) are derived deterministically from `buildId` and `attempt`. Build job URLs are resolved from run ids through the Actions API with `github.token`, cached by run id in `coreci/presentation.json`; the publisher writes them into its receipt. Existing receipts stay, except the receipts in the pre-buildId layout (`receipts/<64 hex digits>/`), which Phase 4h removes once when no rendered row comes from them. core-ci receives no new callback field.
- **`GET /api/v1/status.json`** (no auth, `application/json`, `503` during recovery): canonical JSON (sorted keys, no insignificant whitespace) `{"schema": 1, "generation": <seq of the last committed event (5.5)>, "packages": [...], "units": [...]}`. Per package `name`, `sourceRepo`, `released` (`version`, `tag`) or null. Per unit, sorted by package and target, dormant units excluded: `package`, `target`, `feed`, `status` (the 4.6 kind), `detail` (the unit or reason in parentheses of 4.6, or null), `position` (or null), `inflight` (`version`, `revision`, `buildId`, `phase`, `runId`, `attempt`) or null, `published` (`version`, `revision`, `runId`, `at`, `feedCommit`) or null, `failure` (`version`, `revision`, `reason`, `runId`, `at`) or null, `next` (`version`, `revision`; for a unit neither settled nor in flight, the version it would build and the revision the engine would allocate for it) or null. Values the engine does not keep today are taken from the persisted events that carry them, so replay reproduces them; the event table schema does not change.
- **State mapping** onto the activities the unchanged templates render, as the old system showed them: `Waiting`, `Blocked` → waiting (the old system showed a unit behind a failed prerequisite as waiting); `Ready`, `Dispatching` → pending; `Building` → running; `AwaitingPublish`, `Publishing` → publishing; `Failed` → failed; `NoRelease`, `Settled` → no activity. No other activity is produced.
- **Generation guard.** The feed records the generation of its rendered presentation in `coreci/presentation.json`. A writer renders with `status.json` fetched in the same attempt and never replaces a presentation with one of a lower generation. If `status.json` cannot be fetched, the publisher keeps the current presentation (the next presentation run shows the new receipt) and the presentation writer fails.
- **Presentation writer** `coreci-status.yml`: `workflow_dispatch` with inputs `feed` and `generation`, in the feed's environment for its push credential only (no signing key is used). It fetches `status.json` from the registry's `status_url` and the feed head without package blobs (`--filter=blob:none`), renders from the Git tree, and is a no-op when the rendered files are unchanged or the feed's generation is not lower. It changes only presentation files and `coreci/presentation.json`, and removes the files Phase 4h drops; commit and push as 7.5 step 5, with the same lease retry.
- **Dispatch** (core-ci `app/`, not the engine, not persisted). A feed with `presentation` becomes dirty after every committed event that changes its part of `status.json`, and at startup. On the 30 s timer, a dirty feed whose last presentation dispatch is at least `statusInterval` old and none of whose units is in `PublishDispatching` or `Publishing` (a publication renders the presentation itself) is dispatched (ref: the registry repository's default branch) and becomes clean; a rejected dispatch leaves it dirty. Without `presentation`, no dispatch.
- **Size.** The renderer passes its unchanged context to the templates and writes neither the Pages-only files nor `status.json`; this, the job normalization below and the two changes above are the only changes of the copied renderer. A receipt's job objects keep exactly the fields the job objects of the baseline receipts carry; the presentation writer normalizes existing receipts to that form. The total output stays below the renderer's existing 32 MiB bound with all 178 units published, and further releases do not increase it.
- **Rendering failure.** A publication whose presentation rendering fails still publishes: it writes the ledger and the receipt, keeps every presentation file and `coreci/presentation.json` unchanged, and writes the error to the job summary. The presentation writer fails on a rendering error.
- **Delivery.** Packages are served from the feed `url`. The feed repository has no GitHub Pages deployment and no workflow. Development exports stay under `dev/`.
- **Golden acceptance.** (G1) Rendering the baseline commit's own receipts and `status/controller.json` with the unchanged renderer reproduces every baseline presentation file byte for byte. (G2) A `status.json` fixture equivalent to the baseline's `status/controller.json`, projected and rendered by `coreci/`, reproduces every baseline presentation file byte for byte after replacing the old controller's generated identifiers by the derived ones (each replaced position listed); any other difference is listed in the report as an exception for the owner. (G3) A receipt written by the new publisher renders a row identical in form to a baseline row of the same state. Byte-identical files with unchanged assets are pixel-identical. From Phase 4h, G1–G3 compare every presentation file the renderer still writes, with the page header removed before comparison.

---

## 8. Tests (CTest) — acceptance criteria

### 8.0 Test infrastructure

- Tests are run **only with CTest** (`enable_testing()`, `add_test()`, `ctest --output-on-failure`). **No test framework**: no GoogleTest, no Catch2, no doctest, no Boost.Test, nothing fetched with `FetchContent`.
- Every test is a small, self-contained executable in `tests/` with its own `main()`. It returns `0` on success and a non-zero exit code on failure, after printing `file:line: expected … got …` for each failed check.
- Shared helpers, written in this repository: `tests/Fixture.h` (test pipelines, `GraphChanged` setup, event builders) and `tests/Check.h` (≤ 60 lines): `CHECK(cond)` (records a failure, continues), `CHECK_EQ(a, b)` (prints both values), `REQUIRE(cond)` (prints and returns non-zero immediately), and a final `return checkResult();`.
- One CTest test per scenario, named after it (`S01_Diamond`, `S02_PublishSerialization`, …, `I_RandomizedInvariants`, `A_WebhookHmac`, …). Each test gets a CTest `LABELS` property: `core`, `persistence`, `adapters` or `integration`.
- A test that cannot run in the current environment (e.g. the MariaDB integration test without connection variables) prints the reason and exits with code `77`; it is registered with `set_tests_properties(<name> PROPERTIES SKIP_RETURN_CODE 77)` so CTest reports it as **Skipped**, never as passed.
- The randomized test uses a fixed default seed, prints the seed it uses, and accepts a seed and run count through environment variables (`CORECI_TEST_SEED`, `CORECI_TEST_RUNS`) so a failure is reproducible.
- `core` and `persistence` tests must build and pass without SNode.C installed (configure option `-DCORECI_WITH_SNODEC=OFF` builds only those).

Test pipeline: packages `a` (no prereqs), `b → a`, `c → a`, `d → b, c` (diamond), `e → d`. Targets `t1` (feed `f1`), `t2` (feed `f2`); `e` only on `t1`. Tests assert the **exact** command list returned for each event. Every event has an explicit `at`. **Setup**, unless a scenario says otherwise: `GraphChanged` with the test pipeline, then observe tag `v1.0.0` for `b`, `c`, `d`, `e` (no commands), then the scenario's events.

- **S1 Diamond:** tag `a v1.0.0` → exactly `DispatchBuild(a,t1)`, `DispatchBuild(a,t2)`. Build+publish `a@t1` → `DispatchBuild(b,t1)`, `DispatchBuild(c,t1)`, nothing for `d`. Publish `b@t1` → no command. Publish `c@t1` → exactly one `DispatchBuild(d,t1)` whose inputs contain the new `b` and `c`. `t2` progresses independently.
- **S2 Publish serialization:** `b@t1` and `c@t1` succeed back to back → one `DispatchPublish` on `f1`; the second only after `PublishSucceeded` of the first.
- **S3 Superseding:** `a@t1` building `1.0.0`; tag `1.1.0` arrives → no command; `BuildSucceeded` of `1.0.0` → no publish, `DispatchBuild(a,t1,1.1.0)`. `b@t1` waiting in `AwaitingPublish` behind `c@t1`; tag `b v1.1.0` → `b@t1` leaves the queue, is never published, `DispatchBuild(b,t1,1.1.0)`.
- **S4 Failure & retry:** `BuildFailed(b,t1)` → `c@t1` continues and publishes; `d@t1`, `e@t1` show `Blocked(by b@t1)`; `RetryRequested(b,t1)` → `DispatchBuild(b,t1)` with a new revision; `d@t1` is then built exactly once.
- **S5 Idempotency:** duplicate callbacks and callbacks with unknown/stale buildIds produce no commands and no state change (including `engineTime`); `BuildSucceeded` with a different `runId` and `DispatchRejected` for an old `attempt` are ignored.
- **S6 Tags:** older tag, equal tag, non-semver tag, prerelease (default policy) → ignored. `Version` unit cases: every example in section 3.4; the Semantic Versioning 2.0.0 precedence chain `1.0.0-alpha < 1.0.0-alpha.1 < 1.0.0-alpha.beta < 1.0.0-beta < 1.0.0-beta.2 < 1.0.0-beta.11 < 1.0.0-rc.1 < 1.0.0`; `v1.0.0` == `1.0.0` == `1.0.0+build`; leading zeros rejected.
- **S7 Rebuild on dependency change:** with everything settled, tag `a v1.0.1` → `b` is rebuilt with the **same** version and revision + 1.
- **S8 Tag on a waiting unit:** tag `d v2.0.0` while `b@t1` is building → `d` dispatched once, after `b` and `c` are settled, with version `2.0.0`.
- **S9 Validation:** cycle (message shows the path), prerequisite missing a target, unknown references, duplicate names, two packages from one repository, invalid ids (empty, longer than 64 characters, forbidden characters) → `build()` fails.
- **S10 Timeouts and dispatch retries:** all events with explicit `at` values. Deadline = `at` of `DispatchAccepted` + timeout; a `Tick` one second before the deadline changes nothing, one at the deadline → `Failed`. `DispatchRejected` 5xx → backoff schedule exactly as in 4.5, measured from the rejection's `at`; 404 → immediate failure. An event whose `at` is earlier than `engineTime` does not move time backwards (deadlines unchanged). Eighth rejection → `Failed`. `Resumed` during backoff emits nothing for that unit. Re-emission without readiness check: `a@t1` published, `DispatchBuild(b,t1)` rejected (5xx), tag `a v1.1.0`, retry `Tick` → `DispatchBuild(b,t1)` re-emitted with the same `buildId`; its `BuildSucceeded` → `AwaitingPublish` (inputs still match the published `a`); once `a 1.1.0` is published, `b@t1` is rebuilt with revision + 1.
- **S11 Replay:** after every step of S1–S8, take the events stored in `InMemoryEventStore`, map them through `EventRow` and back, replay into a fresh engine → identical status text; `Resumed` re-emits only units in `Dispatching`/`PublishDispatching` without `retryAt`.
- **S12 Write-ahead:** with `InMemoryEventStore` failing `append` on demand: the event is not applied, no command is executed, the caller gets `503`; the next event is processed only after the previous `append` completed; ignored and invalid events are never appended. Recovery: `append` failing before storing and `append` failing after storing (ambiguous outcome) both lead to rebuild; afterwards the engine state equals a fresh replay of the store, events during recovery get `503`, a retried duplicate gets `200 ignored`, and `Resumed` re-emits the lost commands.
- **S13 EventRow:** round-trip `Event → EventRow → Event` for every event type, including `NULL` handling, maximum lengths and characters that require escaping (`'`, `\`, `"`, NUL rejected, UTF-8); `normalize` cases for every field rule of 5.2 (character vs. byte counting, invalid UTF-8, NUL, 4096-byte truncation at a character boundary, 40-hex SHAs); every fatal decoding case of 5.2 reports the row's `seq`.
- **MariaDB integration test (phase 3):** against a real MariaDB server (CI service container; connection from `CORECI_TEST_DB_HOST`, `CORECI_TEST_DB_PORT`, `CORECI_TEST_DB_USER`, `CORECI_TEST_DB_PASSWORD`, `CORECI_TEST_DB_NAME`; the test drops and recreates the `coreci_*` tables in that database only): create schema, append the S1 event sequence, then, in the same test process, destroy queue, engine, store and SNode.C MariaDB client and create fresh ones with a new connection (this is the restart), verify identical status and correct `Resumed` commands; a row with an embedded NUL inserted directly via SQL, and an events table without a schema-version row, are each reported as `Corrupt` (the former naming its `seq`); a `GraphChanged` round-trips with all its graph rows, and graph rows without their `GraphChanged` row are `Corrupt`; package names `a` and `A` round-trip as distinct, and repository owners keep their spelling (`SNodeC`); `revision_base` round-trips. If the environment variables are not set the test exits with `77` and CTest reports it as **Skipped** — never as passed.
- **Randomized invariant test** (seeded, ≥ 10 000 runs over random event interleavings on the test pipeline): (I1) a new `buildId` is never allocated while any prerequisite of the unit is unsettled (re-emissions by rules 4 and 12 resend an existing `buildId` and are exempt); (I2) at most one inflight per unit; (I3) at most one publication per feed; (I4) when all builds succeed, the system quiesces with every unit settled against the latest tags; (I5) replay equivalence.
- Adapter tests: webhook HMAC verification (valid, invalid, missing), push-tag filtering, callback HMAC + skew rejection, form decoding, dispatch request body/headers built correctly (against a fake HTTP endpoint); the callback route accepts a `buildId` containing `.`, `+` and `~` (tag with build metadata).
- **S15 GraphChanged:** an unchanged pipeline is ignored; a new prerequisite edge rebuilds the dependent with the same version and revision + 1; a removed package becomes dormant (its callbacks ignored, its build slot freed, not shown) and, when re-added, continues its revisions without reuse; removing a unit in `AwaitingPublish` or `Publishing`, or changing its feed, is ignored; a new target is dispatched; a feed change clears `published` and rebuilds into the new feed; a `sourceRepo` change clears `released`; a `buildWorkflow` change abandons a `Building` unit and rebuilds it with a new revision; each of these changes is ignored while an affected unit is in a publish phase; re-adding a dormant package from another repository clears its `released`, and re-adding a dormant unit with another feed clears its `published`; package `a.b` with target `c` and package `a` with target `b.c` get distinct buildIds; a change of only the spelling of a repository is accepted, keeps `released` and abandons nothing; replay of a history with several `GraphChanged` reproduces identical status.
- **S16 Declarations:** a valid registry, target catalog and package files load (fixtures in the catalog form); unknown top-level fields are ignored, while any field other than `id` and `feed` in a matrix entry fails the load; every invalid case (missing required field, wrong type, duplicate key, malformed workflow reference, unknown feed, file over 1 MiB, HTTP error, every identifier rule of 3.2, an invalid registry path, one target id with two feeds, a `revision_base` that is not an integer in 0–1 000 000) fails the whole load naming repository and file; repositories are matched case-insensitively; a registry entry `snodec/SNODE.C` yields the canonical `SNodeC/snode.c` reported by the fake GitHub in the pipeline, the status text and the dispatch URL; copies of the real `ci/packages.json` of `SNodeC/snode.c` (76ca647) and `SNodeC/mqttsuite` (35a4f78) in `tests/data/` yield 76 targets each and the edge `mqttsuite → snodec`.
- **S17 Badge:** every row of the 4.6.1 table; XML escaping; the width formula; one golden SVG compared byte for byte.
- **S18 RevisionBase:** `revisionBase` 100 → the first build is `r101`; a later `GraphChanged` lowering it continues above the last revision; raising it jumps above the new base; a `GraphChanged` that changes only `revisionBase` is accepted and affects only future allocations.
- **S19 StatusJson:** the canonical form of 7.8 for every 4.6 kind; dormant units excluded; the run fields taken from their events; replay reproduces identical `status.json` bytes; `503` during recovery.
- **S20 PresentationDispatch:** dirty after a status change of the feed and at startup, not after a change of another feed only; at most one dispatch per `statusInterval`; a rejected dispatch stays dirty; no dispatch without `presentation`.
- **S21 Catalog:** an unknown catalog id, a duplicate id or an extra matrix field fails the load; the coordinates reach the dashboard model.
- **S22 DashboardStream:** snapshot, coalesced updates (at most one per second) after events, job-cache updates and declaration loads; keep-alive; headers; the stream limit; recovery and `ready`.
- **S23 DashboardButtons:** Retry and Refresh act as specified; the four other operations answer `Not supported by core-ci`.
- **S24 DashboardAuth:** against a fake provider: no cookie → login redirect with PKCE, state and nonce; a valid login of a listed subject → access; another subject, also with the same email → `403`; an open stream is closed within 60 s after the session becomes invalid; `/api/v1` is unaffected.
- **S25 JobCache:** a run found by `runId` and by run name; refresh while running, then kept; the 4-request limit; an error leaves the fields absent.
- **S26 Rebuild:** a satisfied unit → `RebuildRequested` → `DispatchBuild` of the same version with revision `lastRevision + 1`; ignored with `inflight`, without `published`, for a dormant unit or when not satisfied; after its publication every dependent on the same target is rebuilt exactly once and `rebuild` is cleared; a failed rebuild keeps `rebuild` and `RetryRequested` resumes it; replay reproduces every step; the route requires the admin token.
- **S14 EventSql:** for every event type the `INSERT` contains string values only as `_utf8mb4 X'…'`, numbers only as decimal literals, absent values only as `NULL`; values containing `'`, `\`, `"`, `;`, `--`, `/*` and multi-byte UTF-8 appear only hex-encoded; empty strings appear as `_utf8mb4 X''`; the only non-literal expression in an `INSERT` is `UTC_TIMESTAMP(3)`; the `SELECT` lists every column except `recorded_at` in a fixed order, string columns as `HEX(col) AS col`, `ORDER BY seq`; a `GraphChanged` is written as `startTransactions`, the `INSERT`s (graph rows with `LAST_INSERT_ID()`), `commit`, `endTransactions`.

---

## 9. Phases — stop after each phase and report

Work in `/home/voc/projects/snodec/core-ci`. Each phase is one branch off the repository's default branch (`coreci/phase-<n>`), with small, focused commits; if the repository has a GitHub remote, open one PR per phase. **Do not start the next phase until told to.**

0. **Phase 0 — green field.** Prepare the repository; do not write any core-ci code in this phase.
   1. **Inventory and stop if unsafe.** Report: `git status`, `git branch -a`, `git stash list`, `git remote -v`, the default branch, and the list of tracked files. If there are uncommitted changes, untracked files outside build directories, or stashes, **stop and ask** — never discard them yourself.
   2. **Preserve the old state.** Create the annotated tag `pre-greenfield` on the current default-branch HEAD (`git tag -a pre-greenfield -m "State before core-ci green-field restart"`). Do not delete or rewrite any branch, tag or commit; do not force-push; do not rename the default branch.
   3. **Remove everything.** On branch `coreci/phase-0`: `git rm -r` every tracked file and directory, including `.github/`, build scripts, CMake files, docs and configuration — **except** licence files (`LICENSE*`, `COPYING*`), which stay exactly as they are. Delete build output directories in the working tree (e.g. `build*/`, `cmake-build-*/`) only after listing them in the report.
   4. **Add the minimum.** Add this specification as `SPEC.md` (verbatim), a minimal `README.md` (title "core-ci" and one sentence: "Orchestrator for the SNode.C package build pipeline — see SPEC.md."), and a `.gitignore` for build directories. Nothing else.
   5. **Verify and report.** `git ls-files` must list only the licence files, `SPEC.md`, `README.md` and `.gitignore`. Report the tag name, the commit, and the output of `git ls-files`. Commit message: `Start core-ci from a clean slate`.
   From now on, the code at `pre-greenfield` is history only: it is never checked out, diffed against, copied from, or used as a reference.
1. **Phase 1:** `core/` + its tests (S1–S10, randomized test). No SNode.C, no I/O.
2. **Phase 2:** `persistence/` (EventRow, EventStore, InMemoryEventStore, the write-ahead event queue of 5.3) + S11–S13 + replay in the invariant test. Still no SNode.C, no database.
3. **Phase 3a:** `persistence/EventSql` + S14, `adapters/mariadb/` (`MariaDBEventStore`), the `StoreError` interface change, and the MariaDB integration test. Stop and report.
3. **Phase 3b:** `adapters/github/` + `app/` on SNode.C + adapter tests + `pipeline/Pipeline.cpp`. Provide a local smoke test script: run core-ci against a fake GitHub HTTP server (a tiny SNode.C app in `tests/`) and drive the S1 diamond via `curl` callbacks.
3. **Phase 3c:** declarations instead of a compiled graph: remove `pipeline/` and `core::Family`; `GraphChanged` in engine, persistence and MariaDB (schema version 2); `adapters/github/Declarations`; reload triggers and the status error line; tests S9, S14–S16 and the MariaDB integration and smoke test changes. Stop and report.
3. **Phase 3d:** express routing (6.1) replacing the hand-written dispatcher in `app/`, with all existing adapter and smoke tests passing (only `A_CallbackHmac` changes: the router decodes path parameters, so `report()` receives an already-decoded buildId and no longer decodes it); badges (4.6.1 and the badge route), `revision_base` (declaration, engine, `coreci_graph_packages`, schema version 3), tests S16–S18 and the MariaDB integration and smoke test changes. Stop and report.
4. **Phase 4a:** section 7 (specification only).
4. **Phase 4b:** in DistributionPackages: the `coreci/` package, the two workflows, the action and the registry fields of 7.1, as a PR that the owner merges before the proof runs — GitHub dispatches a `workflow_dispatch` workflow only if the file exists on the default branch; the workflows stay inert until 4d because nothing dispatches them for the production feed. Also `SNodeC/Packages-staging`, the staging environment with generated keys and a test registry on a branch (7.7). In `snode.c` and `mqttsuite` (branch and PR each, not merged): the native hook environment of 7.2 and `development_packages`. Proof (7.7): `snodec` and then `mqttsuite` on one deb and one rpm target, `mqttsuite` installing `snodec` from the staging feed; repeating a `snodec` publication is a no-op; a conflicting one fails `immutable`; the README badge block is generated. Stop and report.
4. **Phase 4c:** OpenWrt and production preparation. On the 4b branches of `snode.c` and `mqttsuite`: the OpenWrt hooks on the 7.2 interface (`SOURCE_TAG`, `PREREQUISITES_DIR` instead of receipts), the development export of `snodec`, removal of the old SHA-pinned build wrappers and status-notification workflows (effective only when merged in 4d), and `revision_base` = the highest revision any of the package's files has ever had in the production feed: over every branch and commit of `SNodeC/Packages` and of its retired predecessor `SNodeC/OpenWRT`, read read-only, so that a user who installed any earlier revision still receives the next build as an upgrade. In DistributionPackages (PR, not merged): the production `ci/registry.json` with the production key fingerprints. Proof (7.7): one ipk and one apk target, `mqttsuite` building against the development export of `snodec` from the staging feed. Stop and report.
4. **Phase 4c2:** presentation and `snode.c-idp`. core-ci: `GET /api/v1/status.json`, the run fields it needs, presentation dispatch, `statusInterval`, tests S19–S20 and the smoke test change (`status.json` served; a presentation dispatch after a publication). DistributionPackages: 7.8 (`coreci/presentation.py`, receipts, `coreci-status.yml`, the publisher's lease retry and badge rule) with G1–G3 as CTest tests; the production registry PR updated (feed `url` of 7.1, `presentation`, no `badges`, `SNodeC/snode.c-idp` added). `snode.c-idp` (branch and PR, not merged): its hook on the 7.2 interface, `ci/packages.json` with `snodec-idp` depending on `snodec`, its existing 18 deb and 8 rpm targets and `revision_base` 12, removal of its old build wrapper and status-notification workflows. Proof (7.7): `snodec-idp` on one deb and one rpm target installing `snodec` from the staging feed; the staging registry gets `presentation`; the staging presentation renders with the unchanged templates; a presentation run and a publication racing for the staging feed both land through the lease retry. Stop and report.
4. **Phase 4e:** dashboard and target catalog (6.5, 3.3, 4.3–4.7, 5.2). core-ci: the assets, `/events`, the projection, the job cache, the admin operations, the authentication (6.5), catalog loading, `title`, `dashboardRedirectPort`, `adapters/dashboard/`, the presentation dispatch rule of 7.8, tests S21–S25 and G4–G6. snode.c-idp: the configurable cookie name and lifetime in its client library (a PR, released before the core-ci deployment). DistributionPackages: `ci/targets.json` with every current target; the workflows read target details from the catalog and accept both matrix forms during the transition; `run-name: ${{ inputs.build_id }}` in `coreci-build.yml` and `coreci-publish.yml`; registry `targets`; the catalog's `openwrt-24.10-riscv64_generic` gets `arch` `riscv64_riscv64` (the profile's architecture). snode.c, mqttsuite and snode.c-idp: matrix entries reduced to catalog ids. Order: DistributionPackages, then the source repositories, then the core-ci deployment, then retirement of the earlier SNodeC instance and the 8081 redirect. Stop and report before the deployment.
4. **Phase 4d:** deployment on `core.vchrist.at` and switch-over, by separate instructions; it also retires the old `core-ci-*.yml` workflows and the old status writer.
4. **Phase 4d2:** package versions and rebuild (7.3; 4.1–4.7, 6.1). DistributionPackages: the release field of 7.3, with a test for every format and the ordering examples. core-ci: `RebuildRequested`, its route, S26 and a smoke-test step for the route; deployed on `core.vchrist.at` by separate instructions. Stop and report.
4. **Phase 4g:** cleanup and speed (7.0, 7.2–7.5). DistributionPackages: remove `core_ci/` and the `core-ci-*.yml` workflows and move the used functions into `coreci/` (7.0); rename the image maintenance to `coreci-prepare-rootfs.yml` with its tool in `coreci/`; retention, the sparse feed checkout and emulation only where needed (7.5); no CPU limit (7.4). snode.c and mqttsuite: the hook rules of 7.2. Acceptance: full CTest and actionlint; a retention test (keeps the two newest publications, also across a version change, removes older ones including files from before core-ci, keeps architecture-independent files, ledgers and receipts, regenerates the indices, fails `retired`); for one target of every distribution and every OpenWrt series, the produced package file names and dependency fields equal those of the last build before the change (every difference listed and explained); per family one build and one publication timed before and after. Merged only by explicit owner authorization; deployed by separate instructions. Stop and report.
4. **Phase 4h:** presentation cleanup (7.8). DistributionPackages: the renderer stops writing `index.html`, `status/index.html`, `status/*.html` and `status.json`; the templates only they use are removed; the presentation writer removes these files, `.github/workflows/pages.yml` and the orphaned pre-buildId receipts from the feed in its next commit. The status templates get the page header of 7.8. Acceptance: G1–G3 for the remaining files; on the same data, `README.md` and the badges are byte-identical before and after, and `status/README.md` and `status/*.md` differ only by the page header; in light and dark mode at 1280×900 on github.com, the header's buttons are pixel-identical to those of `docs/installation/snodec/README.md` and the banner and up link match their assets. Merged only by explicit owner authorization. Stop and report.

### Report format for every phase
- Files changed (paths).
- Exact configure/build commands and the complete `ctest --output-on-failure` output (passed/failed/skipped counts).
- Any deviation from this specification, with the reason. Unlisted deviations are defects.
- Open questions.

### Forbidden (each one is a rejected PR)
- Any test framework (GoogleTest, Catch2, doctest, Boost.Test, …) or `FetchContent`; tests run only through CTest.
- JSON anywhere outside `adapters/github/` and `adapters/dashboard/` other than the `status.json` writer of 7.8; any JSON representation of the graph; any stored or logged JSON representation of state; JSON columns or JSON text in the database.
- File-based persistence of state; any database access other than SNode.C's MariaDB client; SQL built from unescaped values.
- Applying an event or executing a command before its row is committed.
- Threads, mutexes, sleeps, busy polling.
- I/O, clocks or randomness inside `core/`.
- Workflows triggering other workflows or repositories.
- GitHub `concurrency:` groups used for feed serialization.
- Pinning repositories to commits or Actions to SHAs; assuming `main` as default branch.
- Stubs, TODOs or placeholder implementations reported as done.
- Changing engine semantics in later phases without asking.
- Repository-, package-, distribution-, architecture- or release-specific knowledge in core-ci code (test data excepted); a compiled-in graph.
- Hand-written request routing instead of SNode.C's express router.
- Working in any directory other than `/home/voc/projects/snodec/core-ci` (except reading the SNode.C sources, reading DistributionPackages in phases 3b and 4, reading the declared repositories' `ci/packages.json` in phase 3c, changing DistributionPackages and `SNodeC/Packages-staging` in phases 4b–4e, and changing `snode.c`, `mqttsuite` and `snode.c-idp` in phases 4b–4e).
- Before Phase 4d: any write to `SNodeC/Packages`, any use of a production environment or production signing key, any change to the old `core-ci-*.yml` workflows, merging anything (only the owner merges).
- Reusing, copying or consulting code from the pre-greenfield implementation (except the three dashboard assets of 6.5); deleting history, force-pushing, discarding uncommitted work, or renaming the default branch.
