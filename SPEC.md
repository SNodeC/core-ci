# core-ci — Implementation Specification for Codex

Read this entire document before writing any code. It is the complete specification. Where something is not specified, **stop and ask** instead of inventing behaviour. Do not add features that are not listed here.

---

## 0. Goal in one paragraph

`core-ci` is a small, single-threaded C++20 daemon that **orchestrates** package builds for the SNode.C ecosystem. It does no expensive work itself: every build, every signing operation and every feed publication runs as a GitHub Actions workflow. `core-ci` only (a) receives notifications from GitHub, (b) maintains a dependency graph of packages × targets modelled in C++, (c) decides which unit is allowed to build or publish next, and (d) triggers the corresponding GitHub workflows via the GitHub REST API. Dependency chains, including diamonds, are resolved by `core-ci` and **never** by workflows triggering each other.

---

## 1. Non-negotiable constraints

1. **The graph is C++.** Packages, targets, feeds and edges are C++ types built by C++ code. The graph is never represented, stored, exported, logged or configured as JSON — not for configuration, not for persistence, not for debugging. Human-readable output is a text table and Graphviz DOT only.
2. **No JSON for core-ci's own data.** Persistence (MariaDB, section 5), configuration and the callback protocol (section 6) are not JSON — no JSON columns, no JSON SQL functions, no JSON text stored in any column. JSON is used only inside `adapters/github/`: parsing GitHub webhook payloads and REST responses, and encoding REST request bodies, because GitHub's API requires it. Tests of `adapters/github/` and the fake GitHub server in `tests/` may contain GitHub wire-format JSON as test data. `#include <nlohmann/json.hpp>` (or any JSON library) outside `adapters/github/` is a defect.
3. **Expensive work happens only on GitHub.** `core-ci` never compiles, packages, signs, holds signing keys, downloads artifacts, or touches a feed repository.
4. **Only core-ci chains builds.** No workflow may trigger another repository's build: no `repository_dispatch` chaining, no `workflow_run` triggers, no pushes that trigger downstream builds. Existing direct chaining in DistributionPackages (e.g. SNode.C target push → MQTTSuite build) must be removed in phase 4.
5. **Feed publication is serialized by core-ci**, not by GitHub `concurrency:` groups. (GitHub concurrency groups keep at most one pending run and cancel older pending runs — that silently drops publications.)
6. **Single-threaded.** No `std::thread`, no mutexes, no blocking sleeps, no polling loops. Everything runs in the SNode.C event loop; timers use SNode.C's timer facility.
7. **The engine is pure.** The scheduling engine (section 4) performs no I/O, reads no clock, generates no randomness. Time arrives only as the `at` timestamp carried by every event (section 4.3). Same event sequence ⇒ same state and same commands, always.
8. **Never pin to commits.** Workflows are dispatched on each repository's configured default branch (do not assume `main` — some repositories use `master`, and default branch names are never renamed). GitHub Actions are referenced by tag (e.g. `@v4`), never by commit SHA.
9. Build flags: `-Wall -Wextra -Wpedantic -Werror`. Every phase ends with a clean build and all tests passing.
10. **All development happens in the local repository `/home/voc/projects/snodec/core-ci`.** It already contains an earlier implementation. That code is discarded: Phase 0 (section 9) removes it, and nothing from it may be copied, adapted, consulted or reintroduced later. Development starts on a green field.

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
  adapters/github/          # GitHub REST client, webhook parsing, HMAC verification (only place with JSON)
  app/                      # SNode.C express server, wiring, timers, main()
  pipeline/
    Pipeline.cpp            # THE real graph definition (C++ code) — see 3.3
  tests/                    # plain CTest executables (no test framework) — see section 8
  workflows/                # templates copied into recipe/publisher repos in phase 4
    coreci-build.yml
    coreci-publish.yml
    actions/coreci-report/action.yml
```

`core/` and `persistence/` must compile and be tested without SNode.C installed.

---

## 3. Domain model (core/Graph.h)

### 3.1 Types

- **Feed** — `id` (e.g. `packages`), publisher workflow: `{owner, repo, workflowFile, branch}`. Publications to one feed are strictly serialized.
- **Target** — `id` (e.g. `debian-12-amd64`, `raspios-12-arm64`, `fedora-41-x86_64`, `openwrt-24.10-aarch64_cortex-a53`), `family` (enum: `Debian`, `Ubuntu`, `RaspberryPiOS`, `Fedora`, `RockyLinux`, `OpenWrtOpkg`, `OpenWrtApk`; an OpenWrt target maps to `OpenWrtOpkg` or `OpenWrtApk` by the package manager of its OpenWrt release as defined in DistributionPackages), `feed` (exactly one Feed id).
- **Package** — `name` (e.g. `snode.c`), `sourceRepo {owner, repo}`, `buildWorkflow {owner, repo, workflowFile, branch}` (recipes may live in another repo, e.g. DistributionPackages), `targets` (set of Target ids), `prerequisites` (list of Package names; every edge means "must be built against the published version of"), `acceptPrereleases` (default false). There is no configurable tag pattern: every package uses the tag format of section 3.4.
- **Unit** — the scheduling unit `(package, target)`. A dependency edge `P → Q` applies per target: unit `(P,T)` depends on `(Q,T)`.

### 3.2 Builder and validation

A fluent builder (`Pipeline::Builder`) produces an immutable `Pipeline`. `build()` rejects, with a precise error message:
- unknown package / target / feed references,
- duplicate names,
- cycles — the message must print the cycle path, e.g. `cycle: a -> b -> c -> a`,
- a package P with target T whose prerequisite Q does not have target T,
- two packages with the same `sourceRepo` and overlapping targets.

`Pipeline` provides: per-target topological order (deterministic: ties broken by package name), direct prerequisites, direct dependents, `packagesForRepo(owner, repo)`.

### 3.3 The real pipeline (pipeline/Pipeline.cpp)

Define the actual graph in C++: `snode.c` (no prerequisites) and `mqttsuite` (prerequisite `snode.c`), both built by the DistributionPackages workflow, all published to the `SNodeC/Packages` feed, with each package's targets taken from the `matrix` of `ci/packages.json` in its source repository (`SNodeC/snode.c`, `SNodeC/mqttsuite`), read at the HEAD of that repository's default branch (fetch first; never a feature branch, never a pinned commit). Do not invent targets — list them explicitly in C++ with their `id`s. Family mapping: `distribution` `debian` → `Debian`, `ubuntu` → `Ubuntu`, `raspberrypios` → `RaspberryPiOS`, `fedora` → `Fedora`, `rocky` → `RockyLinux`; `openwrt` with `build.format` `ipk` → `OpenWrtOpkg`, `apk` → `OpenWrtApk`; any other value → stop and report. The dependency graph is defined **only** in `Pipeline.cpp`: `depends_on` and every other field of `ci/packages.json` are ignored by core-ci (Phase 4 removes `depends_on` from the source repositories). The feed publisher workflow (section 7.2) lives in `SNodeC/DistributionPackages` and pushes to `SNodeC/Packages`. AISuite is **not** added (it joins only after its first release).

### 3.4 Tag format and version ordering (core/Version.h)

- A tag is accepted as a version only if it matches exactly `v?MAJOR.MINOR.PATCH(-PRERELEASE)?(+BUILD)?`, with every part as defined by Semantic Versioning 2.0.0 (numeric identifiers without leading zeros; pre-release and build identifiers `[0-9A-Za-z-]`, dot-separated, non-empty). An optional single lowercase `v` prefix is allowed and is not part of the version. Anything else (e.g. `V1.0.0`, `1.0`, `release-1.0.0`, `v01.0.0`) is not a version.
- `version` is the tag without the `v` prefix (e.g. tag `v2.0.1` → version `2.0.1`); `tag` keeps the original string for checkout.
- Ordering is Semantic Versioning 2.0.0 precedence: MAJOR, MINOR, PATCH numerically; a pre-release sorts before the same version without one; pre-release identifiers compared left to right (numeric < alphanumeric, numeric compared numerically, alphanumeric compared in ASCII order, fewer identifiers < more); build metadata is ignored for precedence.
- Two tags of equal precedence (e.g. `v1.0.0` and `1.0.0`, or `1.0.0+a` and `1.0.0+b`) are equal: the one observed later is ignored by rule 4.5.1.

---

## 4. Engine (core/Engine.h) — the heart. Implement exactly this.

### 4.1 Stored state (minimal; everything else is derived)

Per package:
- `released`: optional `{version, tag}` — highest accepted tag seen.

Per unit `(P,T)`:
- `published`: optional `{version, revision, inputs}` where `inputs` is a sorted map `prerequisite → {version, revision}` — exactly what is in the feed.
- `inflight`: optional `{buildId, version, tag, revision, inputs, phase, attempt, retryAt, deadline, runId, artifact}` with `phase ∈ {Dispatching, Building, AwaitingPublish, PublishDispatching, Publishing}`; `attempt` counts dispatch attempts of the current phase; `retryAt` is set only while waiting to retry a dispatch.
- `failure`: optional `{version, inputs, reason}`.
- `lastRevision[version]`: highest revision ever allocated for this version.

Per feed: FIFO queue of units in `AwaitingPublish`, and at most one unit in `PublishDispatching/Publishing`.

### 4.2 Derived predicates (recomputed after every event, per target in topological order)

- `desiredInputs(P,T)` = for each prerequisite Q: `published(Q,T).{version,revision}`; **undefined** unless every prerequisite `(Q,T)` has `published` (no prerequisites → empty map). If undefined, `satisfied` is false.
- `satisfied(P,T)` ⇔ `published` exists ∧ `published.version == released(P).version` ∧ `published.inputs == desiredInputs(P,T)`.
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
| `Tick` | — (only `at`) |
| `Resumed` | — (only `at`; rule 12) |

### 4.4 Commands (engine output; not persisted — they are reproduced by replay)

- `DispatchBuild{buildId, attempt, package, sourceRepo, tag, version, revision, target, inputs, workflow}`
- `DispatchPublish{buildId, attempt, package, version, revision, target, feed, buildRepo, runId, artifact, workflow}`

### 4.5 Rules

1. **TagObserved:** parse the tag as specified in section 3.4; ignore if not a version, if prerelease and not accepted, or if `≤ released`. Otherwise set `released`.
2. **After every accepted event**, in this order: (a) every unit in `AwaitingPublish` whose `inflight.version ≠ released.version` or `inflight.inputs ≠ desiredInputs` is superseded: remove it from the feed queue, clear `inflight`; (b) evaluate all targets in topological order; for every `ready` unit (deterministic order: target id, then topo order) allocate `revision = ++lastRevision[released.version]`, set `buildId = "<package>.<target>.<version>.r<revision>"`, set `inflight{phase=Dispatching, attempt=1}`, emit `DispatchBuild`; (c) for every feed with no publication in progress, pop the queue head, set its phase to `PublishDispatching` with `attempt=1`, emit `DispatchPublish`.
3. **DispatchAccepted:** `Dispatching → Building` (deadline = `engineTime` + buildTimeout) / `PublishDispatching → Publishing` (deadline = `engineTime` + publishTimeout).
4. **DispatchRejected:** HTTP 401/403/404/422 → failure (rule 7) immediately. Otherwise, if `attempt == maxDispatchAttempts` (default 8) → failure. Otherwise `retryAt = engineTime + min(30 s · 2^(attempt−1), 30 min)`. On the first accepted `Tick` with `engineTime ≥ retryAt`: `attempt += 1`, clear `retryAt`, emit the same command again (same `buildId`, new `attempt`). A re-emission does not re-check readiness: it resends a dispatch already decided by rule 2. If the unit's inputs change meanwhile, its build is superseded by rule 5; if only an upstream unit changes, the build is consistent with the feed, may publish, and is rebuilt by rule 2 once that change is published. `httpStatus = 0` (transport error) is not permanent.
5. **BuildSucceeded:** if `inflight.version == released.version` ∧ `inflight.inputs == desiredInputs` → phase `AwaitingPublish`, append to the feed queue. Otherwise the build is **superseded**: clear `inflight` (the unit becomes ready again in step 2). Superseded artifacts are never published.
6. **PublishSucceeded:** `published = {inflight.version, inflight.revision, inflight.inputs}`, clear `inflight`, clear `failure`. (Even if superseded meanwhile, record the truth of what is in the feed; the unit then becomes unsatisfied and is rebuilt.)
7. **BuildFailed / PublishFailed / deadline exceeded on Tick:** `failure = {inflight.version, inflight.inputs, reason}`, clear `inflight`, remove from feed queue/slot. Dependents stay blocked (the unit is not settled). A new tag or new prerequisite publication changes the desired inputs and therefore automatically allows a retry; otherwise only `RetryRequested` clears `failure`.
8. **Acceptance** is defined only by the table in section 4.7. Ignored events are not persisted and change nothing, so duplicates are idempotent. The engine exposes a pure, `const` `classify(event) → Accept | Ignore(reason)` implementing that table (needed by section 5).
9. `BuildStarted` and `BuildSucceeded` record `runId`; `BuildSucceeded` also records `artifact` (both used by `DispatchPublish`). A build callback accepted in `Dispatching` (or a publish callback in `PublishDispatching`) proves the run exists: first clear `retryAt` and enter `Building` (`Publishing`) with deadline = `engineTime` + timeout, then apply the event.
10. Optional global `maxConcurrentBuilds` (default unlimited); a unit occupies a slot while its phase is `Dispatching` (also while waiting for `retryAt`) or `Building`; when no slot is free, ready units wait.
11. The engine exposes `nextDeadline()` (earliest pending timeout or retry-backoff time, or none). `app/` only enqueues a `Tick` when the current UTC time ≥ `nextDeadline()`, so every `Tick` changes state. Before the first deadline exists no `Tick` is needed: every deadline is created by an event that itself carries `at`.
12. **Resumed** (enqueued once by `app/` after replay, section 5.4): re-emit the command, with unchanged `buildId` and `attempt`, for every unit in `Dispatching`/`PublishDispatching` without `retryAt` (its dispatch result was lost); units waiting for `retryAt` keep their backoff. Like rule 4, this re-emission does not re-check readiness.

### 4.6 Status (core/Status.h)

Per unit, derive one of: `NoRelease`, `Settled`, `Waiting(on <unit>)`, `Ready`, `Dispatching`, `Building(run <id>)`, `AwaitingPublish(position n on <feed>)`, `Publishing`, `Failed(<reason>)`, `Blocked(by <failed unit>)`. Render as a plain-text table and as Graphviz DOT (nodes coloured by status).

### 4.7 Event acceptance (classify)

| Event | Accepted only if (otherwise ignored) |
|---|---|
| `TagObserved` | package known; tag is a version (3.4); not a rejected prerelease; version > `released` |
| `DispatchAccepted`, `DispatchRejected` | `buildId` = a unit's `inflight.buildId`; phase `Dispatching` (kind `build`) or `PublishDispatching` (kind `publish`); `attempt` = `inflight.attempt`; no `retryAt` |
| `BuildStarted` | `buildId` matches; phase `Dispatching` or `Building`; no `runId` recorded |
| `BuildSucceeded`, `BuildFailed` | `buildId` matches; phase `Dispatching` or `Building`; no `runId` recorded or the same `runId` |
| `PublishSucceeded`, `PublishFailed` | `buildId` matches; phase `PublishDispatching` or `Publishing` |
| `RetryRequested` | unit exists; `failure` set; no `inflight` |
| `Tick` | `nextDeadline()` exists and `max(engineTime, at) ≥ nextDeadline()` |
| `Resumed` | always |

A workflow re-run started in the GitHub UI is not a retry; once the unit has left the phase its callbacks are ignored. Use `RetryRequested`.

---

## 5. Persistence — MariaDB via SNode.C's MariaDB client

Persistence is **event sourcing in MariaDB**: an append-only event table is the single source of truth; engine state is rebuilt by replaying it. The graph itself is never stored in the database — it is C++ code.

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
```

`coreci_schema` holds exactly one row, `version = 1`. In `open`: if `coreci_schema` has no row and `coreci_events` is empty, insert `version = 1`; no row while events exist, more than one row, or a version other than 1 → `Corrupt`. Schema migrations are out of scope.

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
- During replay, a stored event that the engine classifies as `Ignore` (possible only after the C++ pipeline changed, e.g. a removed package or target) is skipped with a warning naming its `seq`.
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
    virtual void append(const Event&, std::function<void()> onCommitted, std::function<void(const std::string&)> onError) = 0;  // onError: commit outcome unknown (5.3 step 4)
    virtual void loadAll(std::function<void(std::uint64_t seq, const Event&)> onEvent, std::function<void()> onDone,
                         std::function<void(const StoreError&)> onError) = 0;
};
```

`loadAll` delivers events in ascending `seq` order with the row's actual `seq` (gaps are allowed). `InMemoryEventStore` (pure, for tests) and `MariaDBEventStore` (SNode.C) implement it. The event-queue logic of 5.3 is written once against this interface and tested with `InMemoryEventStore`, including injected `append` failures.

---

## 6. External interfaces (app/ and adapters/github/)

### 6.1 Inbound HTTP (SNode.C express)

| Method & path | Auth | Purpose |
|---|---|---|
| `POST /api/v1/github/webhook` | `X-Hub-Signature-256` (HMAC-SHA256 over raw body, webhook secret, constant-time compare) | GitHub org webhook. Handle `ping` (200) and `push` where `ref` starts with `refs/tags/`, `created == true`, `deleted == false` → `TagObserved` for every package whose `sourceRepo` matches `repository.full_name`. Deduplicate by `X-GitHub-Delivery` (keep last 1000 ids). Everything else: `202 ignored`. |
| `POST /api/v1/builds/<buildId>/report` | `X-CoreCI-Timestamp` + `X-CoreCI-Signature-256: sha256=<hex HMAC(secret, timestamp + "." + body)>`; reject if clock skew > 300 s | Workflow callbacks. Body `application/x-www-form-urlencoded`: `phase` (`build`/`publish`), `status` (`started`/`succeeded`/`failed`), `run_id`, `run_attempt`, `artifact`, `feed_commit`, `reason`. Maps to the events in 4.3. Responds `200 accepted` or `200 ignored: <reason>`. |
| `GET /api/v1/status` | none | Text table (`text/plain`). |
| `GET /api/v1/graph.dot` | none | DOT (`text/vnd.graphviz`). |
| `POST /api/v1/units/<package>/<target>/retry` | `Authorization: Bearer <admin token>` | `RetryRequested`. |
| `POST /api/v1/reconcile` | admin token | Trigger tag reconciliation now. |

Every inbound request that carries an event goes through the event queue of 5.3 and is answered only after its event row is committed (or with `200 ignored` / `503`).

Use SNode.C's express API and TLS server. **Do not guess SNode.C APIs**: find the express router, HTTP client and timer facilities in the SNode.C repository (master HEAD) and its examples/apps, and cite the source files you based your code on in the PR description.

### 6.2 Outbound GitHub REST (adapters/github/)

- Dispatch: `POST /repos/{owner}/{repo}/actions/workflows/{workflowFile}/dispatches` with body `{"ref": "<branch>", "inputs": {...}}` (all values strings), headers `Accept: application/vnd.github+json`, `X-GitHub-Api-Version: 2022-11-28`, `Authorization: Bearer <token>`. `204` → `DispatchAccepted`; other status → `DispatchRejected`; transport error → `DispatchRejected` with `httpStatus = 0`. Both carry the command's `buildId`, `kind` and `attempt`.
- TLS: outbound connections use SNode.C's TLS HTTP client (`src/web/http/tls/in/Client.h`), set SNI to the host, and verify the server certificate against the system CA store (`setCaCertDirUseDefault`) or, if configured, the `githubCaCert` file. `setCaCertAcceptUnknown` is forbidden. The API base URL is `githubApiUrl` (default `https://api.github.com`). HMAC-SHA256 and its constant-time comparison use OpenSSL (`CRYPTO_memcmp`), already an SNode.C dependency.
- Token: fine-grained PAT from an environment variable, behind an interface `TokenProvider` so a GitHub App provider can be added later (do not implement the App now).
- **Tag reconciliation** (webhooks do not deliver tag events reliably, e.g. when several tags are pushed at once): on startup, every `reconcileInterval` (default 15 min) and on `/api/v1/reconcile`, list tags of every `sourceRepo` (`GET /repos/{owner}/{repo}/tags`, follow `Link` pagination) and emit `TagObserved{origin=reconcile}` for the highest accepted version. The engine ignores it if it is not newer.
- An SNode.C timer fires every 30 s; it enqueues a `Tick` only if `now ≥ engine.nextDeadline()` (see 4.5 rule 11).

### 6.3 Configuration

Non-secret settings through SNode.C's configuration (command line and configuration file; the command line wins), secrets only through the environment: listen address/port, TLS cert/key, `callbackBaseUrl`, MariaDB host/port/socket/user/database, `githubApiUrl`, `githubCaCert` (optional CA file, used by the smoke test with a self-signed fake GitHub), timeouts, `maxDispatchAttempts`, `maxConcurrentBuilds`, `reconcileInterval`. Secrets only via environment: `CORECI_GITHUB_TOKEN`, `CORECI_WEBHOOK_SECRET`, `CORECI_CALLBACK_SECRET`, `CORECI_ADMIN_TOKEN`, `CORECI_DB_PASSWORD`. Never log secrets.

### 6.4 Operational contract

**Configuration.** Non-secret options use SNode.C's configuration (command line overrides config file); invalid or missing required settings → log the option name (never a value) and exit non-zero before connecting anywhere. Server: SNode.C TLS express instance `coreci`, default port 8443, certificate and key required. `callbackBaseUrl` required, `https://`. `githubApiUrl` default `https://api.github.com`, `https://` only. `githubCaCert` optional. `dbHost` `localhost`, `dbPort` 0, `dbSocket` empty = the client library's default socket `MYSQL_UNIX_ADDR` (core-ci substitutes it, because SNode.C passes an empty string rather than `nullptr` to `mysql_real_connect`); `dbUser`, `dbName` required. `buildTimeout` 21600 s and `publishTimeout` 3600 s (each 60–86400); `maxDispatchAttempts` 8 (1–20); `maxConcurrentBuilds` 0 = unlimited (0–1000); `reconcileInterval` 900 s (60–86400). `CORECI_GITHUB_TOKEN`, `CORECI_WEBHOOK_SECRET`, `CORECI_CALLBACK_SECRET`, `CORECI_ADMIN_TOKEN` required and non-empty; `CORECI_DB_PASSWORD` optional (empty = passwordless, e.g. unix_socket).

**HTTP responses.** Authenticate first (HMAC over the raw body, or bearer token), then parse. Missing/wrong signature or token, or timestamp skew > 300 s → `401 unauthorized` (no detail). Missing `X-GitHub-Event`/`X-GitHub-Delivery`, wrong content type, malformed JSON or form, duplicate form keys, malformed percent escapes, missing required field → `400 invalid: <reason>`. Size limits are enforced while reading, through SNode.C's server parser limits (`ConfigHttpParser`, whose defaults are unlimited): body ≤ 25 MiB, header block ≤ 64 KiB, ≤ 100 header fields, start line ≤ 8 KiB. Then, **before** authentication, a body over 64 KiB on any route except the webhook → `413`. Unknown route `404`, wrong method `405`. core-ci parses outbound status codes with `std::from_chars` (anything but exactly three digits → transport error, `httpStatus = 0`) and never throws on them. Known upstream defect: SNode.C's `src/web/http/client/ResponseParser.cpp` calls `std::stoi` on the status code before core-ci sees the response, so a non-numeric status line terminates the process; it is reachable only from the hostname-verified GitHub endpoint, must be fixed in SNode.C as a separate change, and is not tested in core-ci. Status and DOT during recovery → `503`.

**Callbacks.** `build`+`started` → `BuildStarted` (needs `run_id`, `run_attempt`); `build`+`succeeded` → `BuildSucceeded` (`run_id`, `artifact`); `build`+`failed` → `BuildFailed` (`run_id`, `reason`); `publish`+`succeeded` → `PublishSucceeded` (`run_id`, `feed_commit`); `publish`+`failed` → `PublishFailed` (`run_id`, `reason`); `publish`+`started` → `200 ignored: no event`; anything else → `400`. Fields an event does not use are ignored; an empty value counts as absent. `buildId` is the percent-decoded path segment.

**Webhook.** `sourceSha` = payload `after` (for an annotated tag the tag object); informational only — workflows always check out `tag`. A delivery that maps to several packages enqueues one `TagObserved` per package in package-name order; the response is `503` if any result is unavailable, otherwise `200` with one line per package (`<package>: accepted` or `<package>: ignored: <reason>`). The delivery id is remembered only if no result was unavailable; in memory, last 1000 ids, oldest evicted, empty after restart.

**Reconciliation.** Per package the highest accepted version over all pages; among equal precedence the lexicographically smallest tag string. A failed, malformed or off-origin page aborts that repository for this round (warning, no event); other repositories continue. Startup reconciliation starts after the listener is up and does not delay it. `POST /api/v1/reconcile` answers `202 started` or `202 already running` immediately.

**Outbound HTTP and TLS.** One request per connection; 30 s from connect to complete response; redirects are not followed (3xx: dispatch → `DispatchRejected` with that status, not permanent; reconciliation → abort that repository). `Link` URLs must have the scheme, host and port of `githubApiUrl`. SNode.C's TLS client sets SNI but does not verify the hostname, so in the client's `onConnected` — called after the handshake and before the HTTP context is attached (`src/core/socket/stream/tls/SocketConnector.hpp`) — require `SSL_get_verify_result(ssl) == X509_V_OK` and `X509_check_host(peerCertificate, host) == 1`; otherwise close before any request byte is sent (transport error, `httpStatus = 0`).

**Recovery integration.** Authorized additions: `EventQueue::recover(reason)` (pending → unavailable, discard engine, rebuild) and a `MariaDBEventStore` connection-lost callback that `app/` wires to it. While the queue is not ready no `Tick` is generated and dispatch results are dropped with a log line; `Resumed` re-emits the dispatch after the rebuild.

**Single writer.** Exactly one core-ci per event database, enforced: `open` runs `SELECT GET_LOCK(CONCAT('coreci:', DATABASE()), 0)` after `SET autocommit = 1`; result `0` → `Unavailable` (another instance holds the writer lock), retried like any outage. The lock is per session and re-acquired on every reconnect.

**App library and smoke test.** `app/` is a library taking a `Pipeline`; the `core-ci` executable links it with `pipeline/Pipeline.cpp`, `tests/core-ci-smoke` with the S1 test pipeline. The smoke script uses `CORECI_TEST_DB_*` (may drop and recreate the two tables there; exit 77 if unset) and shows: verified TLS dispatch to the fake GitHub; a wrong-host certificate rejected with no request reaching the fake; the S1 diamond driven by signed `curl` callbacks; a restart midway with correct `Resumed` dispatches; a second instance on the same database refused by the writer lock.

---

## 7. GitHub workflow contract (workflows/, used in phase 4)

### 7.1 `coreci-build.yml` (lives in the recipe repository, e.g. DistributionPackages)

`on: workflow_dispatch` with exactly these inputs (≤ 10): `build_id`, `package`, `source_repo`, `tag`, `version`, `revision`, `target`, `prerequisites` (format `name=version-revision,name=version-revision`), `callback_url`.

Jobs:
1. `report-start` → `coreci-report` with `phase=build status=started`.
2. `build` → checks out `source_repo` at `tag`, installs prerequisites **from the feed** at exactly the versions in `prerequisites` (fail if not available), builds **unsigned** packages for `target` with the given `revision` (Debian revision, RPM `Release`, OpenWrt `PKG_RELEASE`), uploads one artifact named `coreci-<build_id>`.
3. `report-end` (`if: always()`) → `status=succeeded` with `artifact`, or `status=failed` with `reason` (including `cancelled`).

No signing, no feed access, no downstream triggers.

### 7.2 `coreci-publish.yml` (lives in `SNodeC/DistributionPackages`, publishes to `SNodeC/Packages`)

Inputs: `build_id`, `package`, `version`, `revision`, `target`, `build_repo`, `build_run_id`, `artifact`, `callback_url`.

Steps: download the artifact from the other repository's run (`actions/download-artifact@v4` with `repository`, `run-id`, `github-token`) → sign packages → check out the feed repository's default branch → replace only this package's files for this target → regenerate and sign the indices for this target's repository → keep the existing README badge update → **validate the feed** in a container of the target distribution (`apt-get update` + `apt-get install --dry-run`, `dnf makecache` + `dnf install --assumeno`, `opkg update`/`apk update` as applicable) → create the commit (keep the existing orphan-root-commit scheme of SNodeC/Packages) → push with `--force-with-lease=<branch>:<sha fetched at checkout>` → report `phase=publish status=succeeded feed_commit=<sha>`; on any failure report `status=failed`.

Feed validity requirements per family:
- Debian / Raspberry Pi OS: `dists/<suite>/{Release,InRelease,Release.gpg}`, `Packages` + `Packages.gz` per component/architecture, `pool/`; `InRelease` signed with the repository key.
- Fedora: packages signed with `rpmsign`; `createrepo_c` metadata; `repodata/repomd.xml.asc` detached signature.
- OpenWrt (opkg): `Packages`, `Packages.gz`, `Packages.sig` (usign). OpenWrt (apk-based releases): signed apk index.

Signing keys are secrets of a protected GitHub environment in the publisher repository; they never leave GitHub.

### 7.3 `actions/coreci-report/action.yml`

Composite action: builds the form body, computes `timestamp` and `HMAC-SHA256(secret, timestamp + "." + body)` with `openssl dgst -sha256 -hmac`, sends it with `curl --fail --retry 5 --retry-all-errors`. Secret from `CORECI_CALLBACK_SECRET` (organization secret).

---

## 8. Tests (CTest) — acceptance criteria

### 8.0 Test infrastructure

- Tests are run **only with CTest** (`enable_testing()`, `add_test()`, `ctest --output-on-failure`). **No test framework**: no GoogleTest, no Catch2, no doctest, no Boost.Test, nothing fetched with `FetchContent`.
- Every test is a small, self-contained executable in `tests/` with its own `main()`. It returns `0` on success and a non-zero exit code on failure, after printing `file:line: expected … got …` for each failed check.
- The only shared helper is `tests/Check.h` (written in this repository, ≤ 60 lines): `CHECK(cond)` (records a failure, continues), `CHECK_EQ(a, b)` (prints both values), `REQUIRE(cond)` (prints and returns non-zero immediately), and a final `return checkResult();`.
- One CTest test per scenario, named after it (`S01_Diamond`, `S02_PublishSerialization`, …, `I_RandomizedInvariants`, `A_WebhookHmac`, …). Each test gets a CTest `LABELS` property: `core`, `persistence`, `adapters` or `integration`.
- A test that cannot run in the current environment (e.g. the MariaDB integration test without connection variables) prints the reason and exits with code `77`; it is registered with `set_tests_properties(<name> PROPERTIES SKIP_RETURN_CODE 77)` so CTest reports it as **Skipped**, never as passed.
- The randomized test uses a fixed default seed, prints the seed it uses, and accepts a seed and run count through environment variables (`CORECI_TEST_SEED`, `CORECI_TEST_RUNS`) so a failure is reproducible.
- `core` and `persistence` tests must build and pass without SNode.C installed (configure option `-DCORECI_WITH_SNODEC=OFF` builds only those).

Test pipeline: packages `a` (no prereqs), `b → a`, `c → a`, `d → b, c` (diamond), `e → d`. Targets `t1` (feed `f1`), `t2` (feed `f2`); `e` only on `t1`. Tests assert the **exact** command list returned for each event. Every event has an explicit `at`. **Setup**, unless a scenario says otherwise: observe tag `v1.0.0` for `b`, `c`, `d`, `e` (no commands), then the scenario's events.

- **S1 Diamond:** tag `a v1.0.0` → exactly `DispatchBuild(a,t1)`, `DispatchBuild(a,t2)`. Build+publish `a@t1` → `DispatchBuild(b,t1)`, `DispatchBuild(c,t1)`, nothing for `d`. Publish `b@t1` → no command. Publish `c@t1` → exactly one `DispatchBuild(d,t1)` whose inputs contain the new `b` and `c`. `t2` progresses independently.
- **S2 Publish serialization:** `b@t1` and `c@t1` succeed back to back → one `DispatchPublish` on `f1`; the second only after `PublishSucceeded` of the first.
- **S3 Superseding:** `a@t1` building `1.0.0`; tag `1.1.0` arrives → no command; `BuildSucceeded` of `1.0.0` → no publish, `DispatchBuild(a,t1,1.1.0)`. `b@t1` waiting in `AwaitingPublish` behind `c@t1`; tag `b v1.1.0` → `b@t1` leaves the queue, is never published, `DispatchBuild(b,t1,1.1.0)`.
- **S4 Failure & retry:** `BuildFailed(b,t1)` → `c@t1` continues and publishes; `d@t1`, `e@t1` show `Blocked(by b@t1)`; `RetryRequested(b,t1)` → `DispatchBuild(b,t1)` with a new revision; `d@t1` is then built exactly once.
- **S5 Idempotency:** duplicate callbacks and callbacks with unknown/stale buildIds produce no commands and no state change (including `engineTime`); `BuildSucceeded` with a different `runId` and `DispatchRejected` for an old `attempt` are ignored.
- **S6 Tags:** older tag, equal tag, non-semver tag, prerelease (default policy) → ignored. `Version` unit cases: every example in section 3.4; the Semantic Versioning 2.0.0 precedence chain `1.0.0-alpha < 1.0.0-alpha.1 < 1.0.0-alpha.beta < 1.0.0-beta < 1.0.0-beta.2 < 1.0.0-beta.11 < 1.0.0-rc.1 < 1.0.0`; `v1.0.0` == `1.0.0` == `1.0.0+build`; leading zeros rejected.
- **S7 Rebuild on dependency change:** with everything settled, tag `a v1.0.1` → `b` is rebuilt with the **same** version and revision + 1.
- **S8 Tag on a waiting unit:** tag `d v2.0.0` while `b@t1` is building → `d` dispatched once, after `b` and `c` are settled, with version `2.0.0`.
- **S9 Validation:** cycle (message shows the path), prerequisite missing a target, unknown references, duplicate names → `build()` fails.
- **S10 Timeouts and dispatch retries:** all events with explicit `at` values. Deadline = `at` of `DispatchAccepted` + timeout; a `Tick` one second before the deadline changes nothing, one at the deadline → `Failed`. `DispatchRejected` 5xx → backoff schedule exactly as in 4.5, measured from the rejection's `at`; 404 → immediate failure. An event whose `at` is earlier than `engineTime` does not move time backwards (deadlines unchanged). Eighth rejection → `Failed`. `Resumed` during backoff emits nothing for that unit. Re-emission without readiness check: `a@t1` published, `DispatchBuild(b,t1)` rejected (5xx), tag `a v1.1.0`, retry `Tick` → `DispatchBuild(b,t1)` re-emitted with the same `buildId`; its `BuildSucceeded` → `AwaitingPublish` (inputs still match the published `a`); once `a 1.1.0` is published, `b@t1` is rebuilt with revision + 1.
- **S11 Replay:** after every step of S1–S8, take the events stored in `InMemoryEventStore`, map them through `EventRow` and back, replay into a fresh engine → identical status text; `Resumed` re-emits only units in `Dispatching`/`PublishDispatching` without `retryAt`.
- **S12 Write-ahead:** with `InMemoryEventStore` failing `append` on demand: the event is not applied, no command is executed, the caller gets `503`; the next event is processed only after the previous `append` completed; ignored and invalid events are never appended. Recovery: `append` failing before storing and `append` failing after storing (ambiguous outcome) both lead to rebuild; afterwards the engine state equals a fresh replay of the store, events during recovery get `503`, a retried duplicate gets `200 ignored`, and `Resumed` re-emits the lost commands.
- **S13 EventRow:** round-trip `Event → EventRow → Event` for every event type, including `NULL` handling, maximum lengths and characters that require escaping (`'`, `\`, `"`, NUL rejected, UTF-8); `normalize` cases for every field rule of 5.2 (character vs. byte counting, invalid UTF-8, NUL, 4096-byte truncation at a character boundary, 40-hex SHAs); every fatal decoding case of 5.2 reports the row's `seq`.
- **MariaDB integration test (phase 3):** against a real MariaDB server (CI service container; connection from `CORECI_TEST_DB_HOST`, `CORECI_TEST_DB_PORT`, `CORECI_TEST_DB_USER`, `CORECI_TEST_DB_PASSWORD`, `CORECI_TEST_DB_NAME`; the test drops and recreates the two tables in that database only): create schema, append the S1 event sequence, then, in the same test process, destroy queue, engine, store and SNode.C MariaDB client and create fresh ones with a new connection (this is the restart), verify identical status and correct `Resumed` commands; a row with an embedded NUL inserted directly via SQL, and an events table without a schema-version row, are each reported as `Corrupt` (the former naming its `seq`). If the environment variables are not set the test exits with `77` and CTest reports it as **Skipped** — never as passed.
- **Randomized invariant test** (seeded, ≥ 10 000 runs over random event interleavings on the test pipeline): (I1) a new `buildId` is never allocated while any prerequisite of the unit is unsettled (re-emissions by rules 4 and 12 resend an existing `buildId` and are exempt); (I2) at most one inflight per unit; (I3) at most one publication per feed; (I4) when all builds succeed, the system quiesces with every unit settled against the latest tags; (I5) replay equivalence.
- Adapter tests: webhook HMAC verification (valid, invalid, missing), push-tag filtering, callback HMAC + skew rejection, form decoding, dispatch request body/headers built correctly (against a fake HTTP endpoint); the callback route accepts a `buildId` containing `.` and `+` (tag with build metadata).
- **S14 EventSql:** for every event type the `INSERT` contains string values only as `_utf8mb4 X'…'`, numbers only as decimal literals, absent values only as `NULL`; values containing `'`, `\`, `"`, `;`, `--`, `/*` and multi-byte UTF-8 appear only hex-encoded; empty strings appear as `_utf8mb4 X''`; the only non-literal expression in an `INSERT` is `UTC_TIMESTAMP(3)`; the `SELECT` lists every column except `recorded_at` in a fixed order, string columns as `HEX(col) AS col`, `ORDER BY seq`.

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
4. **Phase 4:** `workflows/` templates; then, in DistributionPackages and the publisher repository, split the existing pipeline into `coreci-build.yml` / `coreci-publish.yml` per section 7 and remove all direct chaining between SNode.C and MQTTSuite builds.

### Report format for every phase
- Files changed (paths).
- Exact configure/build commands and the complete `ctest --output-on-failure` output (passed/failed/skipped counts).
- Any deviation from this specification, with the reason. Unlisted deviations are defects.
- Open questions.

### Forbidden (each one is a rejected PR)
- Any test framework (GoogleTest, Catch2, doctest, Boost.Test, …) or `FetchContent`; tests run only through CTest.
- JSON anywhere outside `adapters/github/`; any JSON representation of the graph or state; JSON columns or JSON text in the database.
- File-based persistence of state; any database access other than SNode.C's MariaDB client; SQL built from unescaped values.
- Applying an event or executing a command before its row is committed.
- Threads, mutexes, sleeps, busy polling.
- I/O, clocks or randomness inside `core/`.
- Workflows triggering other workflows or repositories.
- GitHub `concurrency:` groups used for feed serialization.
- Pinning repositories to commits or Actions to SHAs; assuming `main` as default branch.
- Stubs, TODOs or placeholder implementations reported as done.
- Changing engine semantics in later phases without asking.
- Working in any directory other than `/home/voc/projects/snodec/core-ci` (except reading the SNode.C sources, reading DistributionPackages in phases 3b and 4, and changing the DistributionPackages/publisher repositories in phase 4).
- Reusing, copying or consulting code from the pre-greenfield implementation; deleting history, force-pushing, discarding uncommitted work, or renaming the default branch.
