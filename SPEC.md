# core-ci — wave controller specification

This is the master contract. No compatibility with earlier histories is required:
preserve existing history; the additive schema-5-to-6 upgrade below is the only database compatibility exception. Section 7 is fixed owner-supplied CONTRACT W.

## 0. Goal

A single-threaded C++20 daemon loads tag-bound declarations, maintains a typed
package×target dependency graph, plans one wave, and dispatches one GitHub workflow.
Per-target chains run independently inside that workflow and publish once. core-ci
never compiles, packages, signs, downloads artifacts or writes feeds.

## 1. Constraints

The engine is pure: no I/O, clock or randomness. Each accepted event carries UTC
seconds at; engine time advances monotonically. Identical history/options produce
identical state/plans/pending commands. SNode.C owns networking, TLS, timers and asynchronous
MariaDB access. No threads, mutexes, blocking sleeps or Actions run/job polling.

Typed Pipeline and event history are authoritative. No JSON persistence, graph
blobs or JSON SQL. JSON libraries only in adapters/github and adapters/dashboard.
Public status and plan JSON are handwritten in app, never stored or read back.
At most one active wave globally. No per-unit dispatch, Kind, DispatchPublish,
feed queues/locks, per-unit deadlines or maxConcurrentBuilds remain.

Actions use major tags, never SHA pins. No production access, deployment or tags.
Build C++20 with -Wall -Wextra -Wpedantic -Werror. Core/persistence remain buildable
without SNode.C. Keep committed shared formatting settings and explicit targets.

## 2. Ownership and workspace

core owns versions, graph, planner and engine. persistence owns typed event rows,
SQL and the write-ahead queue. adapters/github owns declarations, App auth,
dispatch and HMAC; adapters/mariadb implements the store; adapters/dashboard
projects events; app owns routes, timers, configuration and dashboard assets.
tests contains the test-only in-memory store and disposable loopback fixtures.

Canonical checkout /home/voc/projects/snodec/core-ci only. Temporary builds:
 /tmp/codex/build/core-ci/wave-model (including compiler temporaries).
Persistent results: build/codex/wave-model, removable after review. No additional
checkout. Never modify chore/owner-pending-core-ci, AGENTS.md, .local-credentials/,
secrets/ or .qtcreator/. Existing explicit formatting targets adapt the canonical
modules to this layout; builds use committed copies only.

## 3. Graph and declarations

Unit = package×target. Package: name, canonical sourceRepo/default branch,
targets, prerequisites, revisionBase. Target: opaque catalog id. No Feed or
per-package build/publisher workflow types. One configured workflow repository,
wave.yml on main; feed branch belongs to the wave configuration.

Validate duplicate packages, source repository identities, target ids within a
package, prerequisites and catalog ids; unknown references; cycles with cycle
path; and prerequisites lacking the dependent's target. Deterministic per-target
topological order, ties by package name. Identifiers 1–64 characters A–Z a–z 0–9
. _ -. Repository owner 1–39 A–Z a–z 0–9 -, name 1–100 A–Z a–z 0–9 . _ - except
. and ... Branches obey git branch rules (max 255). Relative paths max 255, nonempty
slash-separated A–Z a–z 0–9 . _ - segments, excluding . and ... Repository identity
compares ASCII case-insensitively; preserve canonical spelling; equality includes it.

Registry/schema 1 and catalog follow W1/W3, read at the same resolved registry
default-branch HEAD commit. Source/schema 1 declarations follow W2 and are read at
the selected tag's commit SHA. Validate required fields/types, supported families,
paths and ids; reject duplicate JSON keys, files over 1 MiB or HTTP errors.
Any failed load preserves the complete previous graph/releases.

Resolve repository spelling/default branches via GitHub; paginate all tags.
Highest stable release wins. Sources without accepted release bootstrap their
default-branch declaration only to display Idle units without a release, never
to build. For unchanged source identity, never omit/regress a recorded release
(including dormant packages) or replace it with an equal-version tag alias.
GraphChanged atomically includes its selected TagObserved records.
W2 has no prerelease opt-in; Version still parses/orders full SemVer.

Tags exactly v?MAJOR.MINOR.PATCH(-PRERELEASE)?(+BUILD)?, SemVer 2.0.0, max 100
characters, no leading numeric zeroes. Version drops optional lowercase v only.
Ordering ignores build metadata; equal precedence never replaces an observed
release; selection ties use lexicographically smallest tag.

Load at startup after listening, each reconcile-interval, known source tag push,
registry default-branch push, reload/reconcile. Source default-branch pushes do
nothing. Concurrent triggers coalesce to one more load; sources load independently.
Unchanged reload never rebuilds published units. Removed units retire from the
active graph. Active wave plans remain frozen and callbacks resolve against them.

## 4. Engine

### 4.1 Stored state

Per package: released {version, tag, sourceSha}. Unit state: Idle, Planned,
Building, Built, Published, Failed, Blocked. Idle includes no-release units.
Per unit: publication {version, revision, inputs, feedCommit, at, runId}, failure
{version, revision, inputs, reason, at}, retryAfter, autoRetried version+inputs,
requested retry/rebuild, lastRevision by version, allocation order, build start/end
and wave run/attempt. No independent workflow/job state cache.

Wave {id "w<seq>", immutable plan, feed_branch, phase Dispatching | Running |
Finished, run_id, run_attempt, attempt, retryAt, deadline}; retain dispatch,
start/end, conclusion. seq is the committed allocating event sequence. Retain
plans in replayed engine history for public GET, without persisted JSON. The
latest wave is the dashboard wave. Active = Dispatching or Running.

### 4.2 Complete events

All carry at. New records also carry optional dispatchDeadline: the absolute deadline
computed before append from logical event time plus the configured wave-deadline.
Only wave allocation and rejection-backoff redispatch consume this metadata. Replay
uses the stored value, never current configuration. Exactly these 16 types exist:

| Event | Additional fields |
|---|---|
| TagObserved | package, tag, sourceSha, origin webhook/reconcile |
| GraphChanged | pipeline, selected TagObserved records |
| WaveDispatchAccepted | waveId, attempt |
| WaveDispatchRejected | waveId, attempt, httpStatus, message |
| WaveStarted | waveId, runId, runAttempt |
| WaveFinished | waveId, runId, conclusion |
| UnitBuildStarted | waveId, buildId |
| UnitBuildSucceeded | waveId, buildId, artifact |
| UnitBuildFailed | waveId, buildId, reason |
| UnitPublishSucceeded | waveId, buildId, feedCommit |
| UnitPublishFailed | waveId, buildId, reason |
| RetryRequested | package, target |
| RebuildRequested | package, target |
| Tick | — |
| Resumed | — |
| WaveExpired | waveId |

No PublishStarted event: acknowledge signed publish-start without a state change.
Every unit wire callback requires a positive run_id. Admit it only when it matches
that wave's accepted WaveStarted owner, including after its deadline, then persist the existing typed
unit event bound to wave id/build_id; existing rows are never rewritten.
The first accepted WaveStarted binds the positive owner run id/attempt; later
WaveStarted from another run is acknowledged and ignored. WaveFinished must
match the owner. The existing Wave.run_id is the only owner authority.
Duplicate/stale/wrong-wave callbacks do nothing, including engine time.
Unit results may precede their build-start receipt, but require the owner
WaveStarted; they cannot reverse a terminal result. Unit callbacks without an
owner or from another run are acknowledged and ignored without changing time.
Callback receipt proves dispatch; retain the deadline recorded by the allocating event.

### 4.3 Trigger and plan

New highest release, GraphChanged adding units or changing build inputs, manual
retry/rebuild, or due automatic retry marks dirty and sets quiet-until = engine
time + wave-delay. Changes during an active wave coalesce into the next.
After the quiet period, with no active wave, plan/dispatch once; empty plan clears
dirty without a workflow. The only outward action is DispatchWave {waveId, attempt,
workflow, feedBranch}; URLs derive from wave id. Pending actions are a pure projection
of committed engine state, not a consumed return value from an event transition.

Per target traverse all units in topological order. Select each prerequisite's
version-revision from this plan. Build if current version is unpublished, inputs
differ from publication, retry/rebuild requested, or any same-target prerequisite
builds. Otherwise reuse published version-revision. Missing releases prevent that
chain from building. Unchanged failed units wait for retry with their dependent
chain; independent units remain eligible. Omit targets with no build units.
Reuse has empty build_id and no callbacks. Wave counts include reuse as Published.

For each build allocate max(lastRevision[version], revision_base)+1 as before,
including retry/rebuild; reuse retains publication revision. Freeze source repo,
tag, commit, version, revision, inputs and direct prerequisites. Reset build/result
timings on allocation, assign Planned, consume its request. Allocate only builds
on included targets. The plan is the sole authority for unit callbacks. Publication records refer to
that wave for its accepted owner run identity.

### 4.4 Dispatch and lifetime

Whenever the engine is live after startup, reconnect, replay or Resumed, a Dispatching
wave without an accepted or rejected dispatch for its current attempt exposes the
same pending DispatchWave. app drives this state after readiness, committed events
and its existing tick. EventQueue owns persistence only; it never delivers outward
commands. No historical HTTP effects run during replay. All initial dispatches,
manual/automatic retries, rejection retries and reload/reconcile-triggered waves
use this single state-derived authority. Repeated reads do not consume the action.
app tracks only the lifetime of an HTTP request and its result append to avoid
overlapping requests for the same wave/attempt; different attempts remain independent.
An unavailable result append releases that request so recovered state can retry.
Resends preserve wave_id and frozen inputs. First accepted owner_run_id fences
callbacks and publication; GitHub dispatch itself may create duplicate runs.

Accepted dispatch remains Dispatching until the first accepted WaveStarted,
which makes the wave Running. The allocating event records dispatch time plus
wave-deadline as an absolute deadline. A rejection-backoff redispatch records its
new absolute deadline in the event advancing the attempt. Dispatching without WaveStarted for wave-start-timeout seconds
redispatches the same frozen wave with attempt+1, including on Resumed; measure
from the most recent dispatch time. A recorded rejection retains its existing
backoff and exhaustion rules. Start-timeout redispatch does not extend the wave
deadline; expiry takes precedence. No new timer or dispatch authority is added.
Reject → same frozen wave with 30s backoff doubling to 1800s, maxDispatchAttempts.
401/403/404/422 or exhausted attempts finish wave and fail its builds. Permanent
rejections never automatically retry. Ignore dispatch results after callbacks.
No second active wave, including during rejection backoff.

Build started → Building/start; succeeded → Built/artifact/end; failed → Failed/
reason/end. Publish succeeded from Built → Published using frozen selection/inputs,
feed_commit/time/run; clear failure/request. Publish failed from Built → Failed.
Publish shares the wave run and has completion time only.

WaveFinished ends the wave. Planned builds without callbacks whose transitive
same-target prerequisite build failed become Blocked. Other Planned/Building/Built
without final publication callback become Failed ("no result"). Preserve successful
publications and independent results. Blocked units have no retry timer of their own.

RetryRequested on Failed schedules it and its Blocked dependents into the next
wave. RebuildRequested on Published schedules it and its dependent cascade.
Automatic retry has the same effect once per failed version+inputs after retryDelay
(zero disables), except permanent dispatch rejections. New version/inputs make
old failure eligible again. Requests/tags during a wave affect the following plan.

The existing timer appends WaveExpired when the recorded deadline is due, including
immediately after startup/recovery. Only its committed application ends the hold;
new Tick/Resumed records do not synthesize expiry. WaveExpired finishes the
scheduling hold with conclusion "timeout"; it does
not synthesize unit failures, Blocked outcomes or retries. Preserve unit state,
frozen selection and owner_run_id. The owner's late build/publish callbacks still
update units through their ordinary transitions; a successful publication is
Published, never "no result", and the next plan reuses its version-revision and
inputs. Non-owner callbacks remain ignored. Late callbacks do not reactivate the
expired wave or extend its hold; an owner WaveFinished may finalize remaining
unit outcomes exactly once, including a real owner conclusion of "timeout". A late
publication from a superseded allocation updates the publication fact without
replacing the newer allocation state, timings or failure; an older allocation
cannot overwrite a newer publication. Legacy synthetic no-result failures admit
the missing owner outcome; explicit terminal build/publish outcomes remain final. Resumed applies the same start-timeout rule to an unfinished
Dispatching wave; an owner-bound Running wave waits for its deadline.
An attempt with explicit rejection resumes its known backoff. nextDeadline is the
earliest applicable debounce, wave start-timeout, backoff/deadline or automatic retry time; exclude
debounce while active. Tick handles debounce/start-timeout/backoff/retry due times; no per-unit deadline exists.

The sole legacy rule is for events without dispatchDeadline: allocation and
backoff redispatch use 23400 seconds, never the current setting. Their historical
Tick/Resumed expiry still performs the original no-result/Blocked/automatic-retry
transitions, preserving every existing revision allocation and wave state. No
event is rewritten or removed; revision allocation must never decrease on replay.
New waves use the new configured duration. A production-shaped legacy replay
fixture including w964/w1339 timeouts and retries must match the pre-change
baseline for every revision and existing wave field under changed configuration.

## 5. Persistence — MariaDB via SNode.C's MariaDB client

Persistence is **event sourcing in MariaDB**: an append-only event table is the single source of truth; engine state is rebuilt by replaying it. The graph is stored only as part of `GraphChanged` events, in typed tables (5.2) — never as JSON.

### 5.1 Database API

Use the MariaDB client that SNode.C already provides. **Do not use libmariadb/libmysqlclient directly, do not add an ORM or another database library, and do not guess the API**: find SNode.C's MariaDB client in the SNode.C repository (master HEAD) and its examples/apps, and cite the source files you based your code on in the PR description. The client is asynchronous and runs in the SNode.C event loop; use it that way (callbacks), never block.

Verified facts (SNode.C master, `src/database/mariadb/MariaDBClientASyncAPI.h`, `MariaDBClient.h`, `commands/async/MariaDBFetchRowCommand.cpp`): the client offers asynchronous `exec`, `query`, `startTransactions`/`commit`/`rollback`, but **no prepared statements and no escaping function**. Therefore every string value is written as a hexadecimal literal with charset introducer, `_utf8mb4 X'…'` (the bytes of the normalized UTF-8 value; the empty string as `_utf8mb4 X''`), every integer as a decimal literal and every absent value as `NULL` — no other form, so SQL injection is impossible by construction. The SQL text is produced by the pure `persistence/EventSql.h/.cpp` (S14). After every (re)connect, `open` executes `SET NAMES utf8mb4` and `SET autocommit = 1` before any other statement. An `INSERT` runs via `exec` in autocommit mode; the store calls `onCommitted` only after the subsequent `SELECT LAST_INSERT_ID()` finishes. Failure between those statements is an unknown commit outcome. `query` delivers rows through `onQuery(MYSQL_ROW)` and ends with a final `nullptr` row. SNode.C reconnects lazily on the next command after a lost connection and reports state through `onStateChanged`. The only non-literal expression allowed in an `INSERT` is `UTC_TIMESTAMP(3)` for `recorded_at` (audit only: never read back, never decoded). Reads select every column except `recorded_at`, string columns as `HEX(col) AS col` and numeric columns as decimal text; `adapters/mariadb` decodes the hex purely (invalid hex or non-decimal number → `Corrupt`) before `EventRow` validation, so an embedded NUL is detected. A `nullptr` field is SQL `NULL`; `HEX('')` is an empty, non-null field.

### 5.2 Schema (adapters/mariadb/Schema.sql, created by core-ci at startup with `CREATE TABLE IF NOT EXISTS`, InnoDB, utf8mb4)

Fresh schema 6 uses coreci_events with typed nullable columns: package, target,
tag, source_sha, origin, wave_id, build_id, http_status, message, run_id,
run_attempt, attempt, artifact, feed_commit, reason, conclusion, event_at, dispatch_deadline.
No kind column. Graph tables contain packages (source identity/revision_base),
target memberships, prerequisites and selected release rows. No feed table or
per-package workflow columns. Schema.sql is the exact DDL authority.


`coreci_schema` holds exactly one row, `version = 6`. In `open`, under the writer lock, upgrade version 5 only by idempotently adding nullable BIGINT dispatch_deadline and then recording version 6. Existing event rows, identities and graph rows remain unchanged; their deadline metadata is NULL. If the schema table has no row and all coreci tables are empty, insert version 6. No row with existing data, multiple rows, or any other version is Corrupt. Every event uses only its own columns plus the optional common deadline metadata; the others are NULL.

**GraphChanged storage.** A `GraphChanged` is one `coreci_events` row (`event_type = 'GraphChanged'`, only `event_at` and optional `dispatch_deadline` set) plus its graph rows and selected tags in coreci_graph_releases(event_seq, package, tag, source_sha, origin), all non-null and keyed by (event_seq, package), written atomically on one connection as `startTransactions` (autocommit off) → the event `INSERT` → the graph `INSERT`s → `commit` → `endTransactions` (autocommit on again), as implemented in SNode.C's `MariaDBClientASyncAPI.cpp` (`MariaDBAutoCommitCommand`, `MariaDBCommitCommand`). The graph rows use `LAST_INSERT_ID()` as `event_seq`, the second allowed non-literal expression. `onCommitted` is called only after `endTransactions` succeeded. On any error in this sequence the store aborts the connection instead of issuing `rollback` (the server discards uncommitted work and releases the writer lock) and reports an unknown commit outcome (5.3 step 4); a connection is never reused with autocommit off. `normalize` checks a `GraphChanged`'s strings against these columns. The graph tables use `utf8mb4_bin`, so identifiers compare exactly as in C++. `loadAll` reads the graph tables ordered by primary key and attaches them to their `GraphChanged` rows; a `GraphChanged` without graph rows is the empty pipeline; graph rows without their `GraphChanged` row, or rows that fail section 3, are `Corrupt`. Other schema migrations are out of scope.

Every event field from 4.3 maps to exactly one typed column; unused columns are `NULL`. The mapping `Event ↔ EventRow` lives in `persistence/EventRow.cpp`, is pure and is round-trip tested for every event type.

**Field rules** (`persistence/EventRow`, pure): `normalize(event) → Event | Invalid(reason)`. Identifier fields (`package`, `target`, `tag`, `wave_id`, `build_id`, `artifact`, `conclusion`) must be non-empty valid UTF-8 without NUL and fit their column, counted in characters as MariaDB `utf8mb4` does. `source_sha` and `feed_commit` are exactly 40 lowercase hexadecimal characters. Numbers must fit their column (`run_attempt`, `attempt` ≥ 1; `http_status` 0–999). Free text (`message`, `reason`) is never rejected: invalid UTF-8 → U+FFFD, NUL removed, truncated to 4096 bytes at a character boundary. Every field listed for an event in 4.2 is required; `message`/`reason` may be empty. **Decoding** a row with an unknown `event_type`, an invalid `origin`, a missing required field, a non-NULL field the event type does not use, or a value violating these rules is fatal: `loadAll` reports it through `onError` naming the row's `seq`, and core-ci refuses to start.

### 5.3 Write-ahead ordering (critical)

All inbound events (webhook, workflow callback, dispatch result, tick, dashboard operation) go through **one FIFO event queue** and are processed strictly one at a time:

0. Stamp `event.at` with the current UTC time (seconds) when the event enters the queue. This is the only place where core-ci reads the clock for events. Then `normalize(event)` (5.2); if invalid → answer `400 invalid: <reason>`, done (never retried, never classified, never stored).
1. `engine.classify(event)`; if `Ignore` → answer the HTTP request (`200 ignored: <reason>`), done.
2. `INSERT` the event row. **Wait for the commit callback.** No other event is processed meanwhile.
3. Only after a successful append acknowledgement: `engine.handle(event)` updates state → answer the originating HTTP request (`200 accepted`). app derives pending outward actions from the live committed state; no one-shot command is handed off by this transition.
4. If `append` reports an error, the commit outcome is **unknown** (the row may or may not be stored): do not apply the event, answer `503`, and enter **recovery**: answer every further event `503`, discard the engine, reconnect and re-read the store, then rebuild exactly as at startup (5.4: `loadAll`, replay, `Resumed`). Never assume success or failure or blindly repeat the original append. The re-read history deterministically establishes whether its effects exist. If stored, the sender's retry is a duplicate and is answered `200 ignored`. A live unresolved Dispatching attempt is resent immediately from state, including when the lost acknowledgement belonged to a Resumed that advanced the attempt. A recorded acceptance waits for WaveStarted/start timeout; a recorded rejection retains its known backoff. An owner-bound Running wave waits for its deadline. Duplicate runs are fenced by the first accepted WaveStarted owner and the publisher ownership check.

Consequence: the in-memory state only ever contains committed events, so a crash at any point loses nothing that was acknowledged.

### 5.4 Startup and outages

- **Startup order:** connect to MariaDB → ensure schema → `SELECT … FROM coreci_events ORDER BY seq` → replay every row through the same engine without executing outward actions → enqueue one `Resumed` event through the normal write-ahead queue → mark the engine live and drive its pending actions → **only then** start the HTTP listener and timers. No inbound event may be accepted before replay has finished. The start timeout protects against a workflow silently dropped by GitHub; it is never the recovery mechanism for lost local command delivery.
- During replay, a stored event that the engine classifies as `Ignore` (possible only if engine semantics changed between core-ci versions) is skipped with a warning naming its `seq`.
- **Store errors** from `open`/`loadAll` have a kind (5.5): `Corrupt` (a row fails decoding per 5.2, or an unknown schema version) → log it and exit with a non-zero status, at startup and at runtime. `Unavailable` (any database or connection error) → stay in recovery (every event `503`, no ticks); `app/` retries the rebuild with an SNode.C timer, backoff 1 s doubling to at most 60 s, at startup and at runtime. A successful rebuild always starts from a fresh engine and replay (5.3 step 4).
- **Shutdown:** the application's framework lifecycle is authoritative. Once termination begins, the queue exposes no live engine and starts no reconnect, replay, event append (including Resumed), or outward action. Late callbacks cannot revive it. Pending database operations close through the framework's existing cancellation/destruction path; an already-issued append can have an unknown durable outcome, resolved from the store on the next live start. Receiver teardown is not a connection-loss notification. A normal SIGTERM stop exits 0; fatal application/store errors retain a non-zero exit status.
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
| GET /api/v1/waves/{wave_id}/plan.json | Public | Frozen W5 plan, including after finish/restart; unknown id 404, recovery 503, no-store. |
| POST /api/v1/waves/{wave_id}/report | Callback HMAC | Workflow events below. |
| GET /api/v1/status.json | Public | Recorded unit status, 503 during recovery. |
| GET /api/v1/badge/{package}/{target}.svg | Public | Badge from recorded unit state, always 200, unavailable during recovery. |
| GET /, /style.css, /app.js, /events | IDP and subject allowlist | Dashboard assets and SSE. |
| POST /api/{operation} | IDP and subject allowlist | retry, rebuild, reload, reconcile. |
| GET /auth/callback | IDP authorization-code validation | Complete login; protected access checks subject. |

No other routes are supported. Removed API paths return 404, including /api/v1/builds/{buildId}/report, text status, DOT, unit retry/rebuild, reload and reconcile under /api/v1. There is no admin bearer token. Optional plain HTTP listener redirects all requests with 308 to the same path on the configured HTTPS origin.

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

Dispatch JSON is W4 on main with string inputs wave_id, plan_url, callback_url,
feed_branch. 204 emits WaveDispatchAccepted; other statuses emit WaveDispatchRejected
(transport failure 0). core-ci never sends plan_json. No presentation dispatch.

Outbound TLS verifies the CA chain and hostname before sending request bytes, sets SNI, and uses system CA trust or github-ca-cert. One request per connection, 30-second deadline, no redirects. Pagination accepts only links on github-api-url's exact scheme/host/port and rejects loops. One one-second engine timer enqueues Tick only when nextDeadline is due. There is no Actions run/job polling timer; the dashboard timer performs no GitHub I/O. Configured declaration reconciliation (3.3) belongs to workflow management.

### 6.3 Configuration

Non-secret options use SNode.C configuration, command line overriding configuration file. Keys:
registry, target-catalog, workflow-repository, github-app-id, github-installation-id,
github-app-key-file, callback-base-url, github-api-url, github-ca-cert, db-host,
db-port, db-socket, db-user, db-name, wave-delay, wave-deadline, wave-start-timeout, feed-branch, retry-delay,
max-dispatch-attempts, reconcile-interval, listen-address,
listen-port, tls-cert, tls-key, idp-base-url, idp-client-id, idp-public-key-path,
dashboard-users, dashboard-redirect-port.

All three GitHub App keys,
callback-base-url, db-user, db-name, TLS files, idp-client-id, idp-public-key-path and
dashboard-users are required. App and installation ids are positive decimal ids.
Defaults: github-api-url https://api.github.com (HTTPS only); db-host localhost,
db-port 0, db-socket empty uses MYSQL_UNIX_ADDR; registry SNodeC/Packages:ci/registry.json, target-catalog ci/targets.json,
workflow-repository SNodeC/Packages, wave-delay 120 (0–86400), wave-deadline 23400
(60–86400), wave-start-timeout 900 (1–86400), feed-branch feed (only feed/feed-staging); retry-delay 600 (0–86400; zero disables), max-dispatch-attempts
8 (1–20), reconcile-interval 900 (60–86400),
listen-address 0.0.0.0, listen-port 8443, dashboard-redirect-port 0/off.
Missing or invalid settings fail before connecting and log only the option name.
Required secret environment variables: CORECI_WEBHOOK_SECRET and CORECI_CALLBACK_SECRET.
Optional CORECI_DB_PASSWORD (empty permits passwordless socket authentication).
No other production CORECI environment variables remain.

### 6.4 Callbacks, write-ahead and recovery

Callbacks are application/x-www-form-urlencoded, authenticated with
X-CoreCI-Timestamp and X-CoreCI-Signature-256: sha256=HMAC(secret,timestamp+"."+rawBody).

W6 is the complete callback protocol. Empty required values are absent. Decode
wave_id once. Publish-start callbacks are authenticated and validated, then
acknowledged without a persisted event; publish completion supplies its timing.
Unit wire callbacks require run_id and bind build_id within the frozen wave.
The authenticated callback admission boundary checks the run id against the
accepted wave owner before queuing the existing typed unit event. Publish-start
callbacks use the same ownership check after deadline expiry as well. Non-owner callbacks receive200 ignored.
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
Retain publication finish times and assignment order from recorded events.
Drop GitHub job queued/cancelled states, job cache, job log links and unsupported metadata
(proof, verified_at, uncertainty, publish_assigned, publish_run_bound, retire action, step).

Assets use no-cache. SSE sends a snapshot on connect and full model updates at most once per
second on committed events or successful declaration loads; no updates during recovery.
Keep alive every 30 seconds, at most 64 streams (excess 503). Until declarations are loaded,
ready is false and units are empty. A one-second SSE timer may coalesce events and check sessions;
it never requests GitHub data. Dependencies sort descending by level and allocation order.

Add a wave panel with latest id, phase, run link, started/finished and counts by
unit state. Public status includes wave state.

Supported POST /api/{operation}: retry (Failed unit), rebuild (Published unit), reload
(declarations and tags), reconcile (same reconciliation path). Unit operations take
{"package":"...","target":"..."} and answer after commit; reload/reconcile answer after loading.
Every other operation answers "Not supported by core-ci". The Refresh control sends reload.

IDP integration stays with snodec-auth 2: authorization code + PKCE S256/state/nonce,
openid profile email, MFA ACR urn:snodec:acr:mfa, RS256 public-key verification and
introspection on each protected request. Only validated subjects in dashboard-users are admitted;
email is display-only. Cookie coreci_access_token is HttpOnly/Secure/SameSite=Lax, path /,
lifetime 12 hours. Re-introspect open streams at least every 60 seconds and close invalid sessions. Stream maintenance uses monotonic elapsed time; the existing one-second tick starts the session check once 59 seconds have elapsed, leaving a tick for the 60-second boundary. Keep-alives also use monotonic elapsed time.

### 6.6 Public status

status.json is generated from recorded unit state by the existing handwritten app/ writer,
without a JSON library. It contains the event sequence, packages and unit selections, published,
selection and failure records, plus the current wave. It is public alongside badges. No presentation dispatch,
status workflow, generation/dirty bookkeeping for a presentation writer, registry presentation
field or status_url field exists.


---

## 7. GitHub side — wave contract

W1 Repositories. SNodeC/Packages, default branch main, holds .github/workflows/wave.yml,
   ci/registry.json {"schema":1,"repositories":["SNodeC/snode.c", …]} and the target catalog
   ci/targets.json. The production feed is branch `feed`, the staging feed branch
   `feed-staging` of SNodeC/Packages.
W2 Source declaration, read at the tag: ci/packages.json {"schema":1, "package", "title",
   "depends_on":[packages], "build":{"deb":path,"rpm":path,"openwrt":path?},
   "build_dependencies":{"deb":[…],"rpm":[…],"openwrt":[…]}, "targets":[catalog ids],
   "revision_base":N?}.
W3 Catalog entry: {"id","family":"deb"|"rpm"|"ipk"|"apk","distribution","release",
   "codename","arch","image","runner","platform","emulated":bool,"disttag"?,"sdk"?}.
   Fields other than id, family, distribution, release, codename and arch are opaque to core-ci; sdk is an object used only by wave.yml.
W4 Dispatch: core-ci → workflow_dispatch of wave.yml on main with inputs wave_id, plan_url,
   callback_url, feed_branch ("feed" | "feed-staging"). wave.yml additionally accepts
   plan_json (base64) for manual runs; empty callback_url = no callbacks.
W5 Plan (GET plan_url, public, JSON): {"wave":id, "feed_branch", "owner_run_id":id|null, "targets":[{"id",
   "units":[{"package","repo","tag","commit","version","revision","build_id",
   "action":"build"|"reuse"}]}]}; units per target in topological order; "reuse" = install
   the published version-revision from the feed; only targets with ≥1 "build" unit.
   owner_run_id is null until the first accepted WaveStarted, then its run_id;
   it is derived from the existing wave owner and remains fixed through finish/replay.
W6 Callbacks: POST callback_url (https://<core-ci>/api/v1/waves/{wave_id}/report), signed with
   the existing core-ci callback HMAC wire format; form fields: scope=wave → status=started
   (run_id, run_attempt) | finished (run_id, conclusion); scope=unit → run_id, build_id,
   phase=build|publish, status=started|succeeded|failed, plus reason (failed), artifact (build
   succeeded), feed_commit (publish succeeded).
W7 Workflow semantics: job plan → matrix job chain (one job per planned target, fail-fast
   false; inside, units in plan order; a unit whose same-target prerequisite failed is skipped
   without callback) → job publish (always after chain): every unit built successfully in
   this run is published in ONE orphan-root commit to feed_branch with GITHUB_TOKEN, publish
   reported per unit, then scope=wave finished.
   With non-empty plan_url, the publisher re-fetches that plan immediately before
   pushing and publishes only when owner_run_id equals GITHUB_RUN_ID. A non-owner
   publisher succeeds without a push or publisher/wave-finished callbacks. An
   empty plan_url keeps the existing manual publication behavior. Fetch/validation
   errors fail closed. Owner checks use one shared predicate; all publisher
   callbacks are deferred until ownership is established. The existing serialized
   feed mutation remains unchanged; no target-early publication is introduced.
W8 Versions: deb <version>-<revision>~<codename>; rpm <version>-<revision>.<disttag>;
   ipk/apk <version>-r<revision>.
W9 Feed: apt/ (dists/<codename>/main/binary-<arch>/, pool/main/; Release with Origin
   SNodeC/Packages, Label SNode.C, Suite = Codename = <codename>, Components main,
   Architectures, Date, Acquire-By-Hash yes, SHA256 only, InRelease + Release.gpg),
   rpm/<distribution>/<release>/<arch>/ (signed packages, repodata, repomd.xml.asc),
   openwrt/<release>/<arch>/ (ipk: Packages + Packages.sig usign; apk: signed packages.adb),
   keys/ (public keys). Retention: newest 2 publications per package×target.
W10 Concurrency: core-ci runs at most one wave at a time; wave.yml uses concurrency group
   "wave-<feed_branch>" without cancellation.

## 8. Acceptance

Full build with adapters and full CTest twice green, no required skips, against
disposable MariaDB and fake GitHub/IDP HTTP fixtures. Label every test requiring
a MariaDB server `mariadb`, including all ConnectionLoss tests. Native package
builds run `ctest -LE mariadb` and do not install server-test-only dependencies;
repository CI retains the server and full suite. No test framework or
fetched dependencies. Retain HMAC/TLS/App auth, version/validation, write-ahead/
ambiguous-outcome, SQL/row/replay, SSE, badges, status, dashboard model and integration
coverage. Delete obsolete per-unit/feed tests. Browser-driven tests, dashboard
end-to-end/browser scripts, Node/npm dependencies, Playwright and reference-browser
requirements are removed completely. C++ dashboard model tests and HTTP tests of
SSE, routes, operations and authentication remain; no test launches a browser.

Cover debounce coalescing, only one wave active, changes during a wave, build vs
reuse/unchanged reuse/diamond cascades, revision increments/base, W5 plan schema,
callback transitions/idempotency, transitive Blocked dependents, independent
success, no-result failures, manual/automatic retry cascades/limit, dispatch
rejection/backoff/exhaustion, deadline followed by late owner publication and
next-plan reuse (non-owner ignored), restart waiting, frozen plan during graph
changes, dashboard unit/wave contents, SSE output and routes, old report route 404,
public plan and authenticated callbacks. Retain 10000 seeded randomized scenarios.

Report baseline/final physical production lines per directory and dashboard
assets, test lines separately, event/rule/route/config counts, PRs/merge SHAs and
qualification. Record each reversible ambiguity as "Decision: … because …".
Merge SPEC first, implementation after qualification, delete only task branches
and leave canonical main pulled and clean.
