# core-ci — state and publication specification

This is the master contract. Section 7 is the owner-approved Contract X v2 and is authoritative; sections that still describe waves are superseded where they conflict with it.

## 0. Goal

A single-threaded C++20 daemon loads repository-owned declarations, maintains a typed
package×target dependency graph, plans one wave, and dispatches target chains.
Finished targets publish early through a separate publication workflow. core-ci
never compiles, packages, signs, downloads artifacts or writes feeds.

## 1. Constraints

Committed state is authoritative. Display history is append-only and never read
to rebuild state. No runtime event store, replay, legacy replay transitions,
compatibility path or feature flag remains. Every state change is one transaction.
Outward actions are projections of stored state whenever core-ci is live, after
start, reconnect and timers; they are not consumed transition results.

SNode.C owns networking, TLS, timers and asynchronous MariaDB access. No threads,
mutexes, blocking sleeps or Actions run/job polling. Core/persistence remain
buildable without SNode.C. Build C++20 with -Wall -Wextra -Wpedantic -Werror.
Actions use major tags. No production access, deployment or tags.

## 2. Ownership and workspace

Thin main starts the application. config owns application options in the coreci
section; SNode.C owns WebApp instance options. API, webhook and dashboard routers
mount on the WebApp. WaveController owns planning/build scheduling;
PublicationController owns target batching and publication scheduling. Existing
GitHub, dashboard and MariaDB adapters retain their transport responsibilities.
The state store owns transaction boundaries and recovery. There is one authority
for every fact. Only the separate coreci-migrate tool contains old replay semantics;
it is never linked into the daemon.

Canonical source: /home/voc/projects/snodec/core-ci.
Persistent agent build: /home/voc/projects/snodec/build/codex/core-ci/state-model.
Manual build: /home/voc/projects/snodec/build/qtcreator/core-ci/<configuration>.
Temporary builds: /tmp/codex/build/core-ci/state-model.
Temporary artifacts: /tmp/codex/artifacts/core-ci/state-model.
No additional checkout. Never modify chore/owner-pending-core-ci, AGENTS.md,
.local-credentials/, secrets/ or .qtcreator/. Committed formatting and analysis
settings match the shared baseline; use explicit development targets.

## 3. Graph and declarations

Unit = package×target. Package: name, canonical sourceRepo/default branch,
targets, prerequisites, revisionBase. Target: opaque catalog id. No Feed or
per-package build/publisher workflow types. One configured workflow repository,
wave.yml and publish.yml on main; feed branch belongs to the wave configuration.

Validate duplicate packages, source repository identities, target ids within a
package, prerequisites and catalog ids; unknown references; cycles with cycle
path; and prerequisites lacking the dependent's target. Deterministic per-target
topological order, ties by package name. Identifiers 1–64 characters A–Z a–z 0–9
. _ -. Repository owner 1–39 A–Z a–z 0–9 -, name 1–100 A–Z a–z 0–9 . _ - except
. and ... Branches obey git branch rules (max 255). Relative paths max 255, nonempty
slash-separated A–Z a–z 0–9 . _ - segments, excluding . and ... Repository identity
compares ASCII case-insensitively; preserve canonical spelling; equality includes it.

Registry/schema 1 and catalog follow W1/W3, read at the same resolved registry
default-branch HEAD commit. Source/schema 2 declarations follow W2 and are read at
the source default-branch tip (ci_commit); resolve the ci/ tree hash from that tip. Validate required fields/types, supported families,
paths and ids; reject duplicate JSON keys, files over 1 MiB or HTTP errors.
Any failed load preserves the complete previous graph/releases.

Resolve repository spelling/default branches via GitHub; paginate all tags.
Highest stable release wins. Sources without accepted release bootstrap their
default-branch declaration only to display Idle units without a release, never
to build. For unchanged source identity, never omit/regress a recorded release
(including dormant packages) or replace it with an equal-version tag alias.
A graph replacement atomically includes its selected tags.
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

## 4. State model and controllers

### 4.1 Stored facts

- units: package, target; published version/revision/inputs/feed_commit/time and
  source/run links; current status Idle|Planned|Building|Built|Publishing|Published|
  Failed|Blocked; last reason, allocation order, build/publication timings,
  next revision per version, requested retry/rebuild and automatic retry bookkeeping.
- waves: id, feed_branch, frozen typed plan, owner_run_id, run_attempt, dispatch
  attempt, phase, absolute deadline, start-timeout/retry times, timestamps, conclusion.
- wave_targets: wave, target, status, artifact.
- publications: id, feed_branch, frozen typed plan, owner_run_id, run_attempt,
  dispatch attempt, phase, absolute deadline, start-timeout/retry times, timestamps.
- tags: package → highest observed stable release tag/commit, including dormant
  packages. Source identity determines whether a recorded release can be reused.
- log: append-only display history. Never read for startup, recovery or decisions.

Typed state includes the current graph, scheduling clock/debounce and monotonically
allocated ids. Schema.sql is the exact DDL authority; child tables for typed plans,
inputs, revisions and graph membership are allowed, with one authoritative copy.
No persisted event stream or JSON state/graph blobs.

### 4.2 Planning, revisions and cascade

New highest release, changed graph/build inputs, manual retry/rebuild or due
automatic retry marks dirty and sets quiet-until = logical time + wave-delay.
Logical time advances monotonically from supplied UTC seconds. Changes during an
active wave coalesce into the next. After quiet-until, with no active wave,
allocate one wave; an empty plan clears dirty without dispatch.

Per target traverse in deterministic topological order. Select prerequisite
version-revisions from this plan. Build if the current version is unpublished,
inputs differ from publication, retry/rebuild is requested, or a same-target
prerequisite builds; otherwise reuse the published version-revision. Missing
releases prevent that chain from building. Unchanged failed chains wait for retry;
independent units remain eligible. Omit targets without a build. Reuse has empty
build_id and no callbacks, and counts as Published.

For each build allocate max(last revision for version, revision_base)+1, including
retry/rebuild. Freeze source repo, tag, source commit, ci_commit, ci/ tree hash, version, revision
and resolved dependency versions/revisions. CI tree changes make a unit eligible;
ordinary default-branch commits with an unchanged ci/ tree do not. Reset allocation timings, assign Planned and consume the request.
Never decrease any revision, including across migration and restart.

Retry on Failed schedules it and its Blocked dependents. Rebuild on Published
schedules it and its dependent cascade. Automatic retry has the same effect once
per failed version+inputs after retry-delay (zero disables), except permanent
dispatch rejection. New version/inputs make an old failure eligible again.
Blocked units have no independent retry timer.

### 4.3 Ownership, target publication and completion

First valid started callback binds the positive run id/attempt immutably, separately
for each wave/publication. Other runs and callbacks without an owner are acknowledged
and ignored without changing state or time. The frozen plan admits build ids.
Duplicate/stale callbacks cannot reverse terminal results or regress publication
facts. Results may precede build-start receipt. Owner callbacks remain valid after
a deadline. An older allocation may update its publication fact but never overwrite
a newer publication or replace newer allocation state, timings or failure.

Build started → Building; succeeded → Built; failed → Failed with reason/timing.
A target finished callback records its artifact and target completion atomically.
An empty artifact means there are no successfully built packages to publish.
Skipped units with transitive failed same-target prerequisites become Blocked;
other missing build results become Failed. Preserve independent successes.

When no publication is active for a feed, batch all finished targets with nonempty
artifacts and unpublished built units into one frozen publication, one item per
target with its successful units. Mark those units Publishing. Targets finishing
during publication wait for the next publication. Do not wait for wave completion.
Publication callbacks require the publication owner. Success records Published
with frozen version/revision/inputs, feed_commit/time/run; clear failure/request.
Already-published success is idempotent. Failure records the reason. A failed
publication unit is retried through a new publication using the same artifact
while available, retaining version/revision; failure=artifact-unavailable requires a rebuild.
Reason is human display text only. Unknown or missing failure is treated as other.
No artifact download or Actions polling is introduced into core-ci.

Wave-finished records the owner's conclusion; the wave remains active until every
planned target is published, failed or blocked. Deadline expiry also permits this
completion check; late owner callbacks still apply. No next wave starts before
this hold ends. Publication-finished finalizes missing publication outcomes once;
late owner successes after deadlines remain valid.

### 4.4 Dispatch, deadlines and recovery

WaveController and PublicationController expose pending dispatches from committed
state. Repeated reads do not consume them. Startup and reconnect immediately drive
outstanding work, including a crash between commit and dispatch. The app tracks
only in-flight HTTP request lifetimes to avoid overlapping the same id/attempt.
Unknown result-commit outcomes trigger state reload before further decisions.

Dispatch acceptance waits for started. After start-timeout, redispatch the same
frozen id with attempt+1. Start-timeout times and deadlines are stored absolute
times and survive configuration changes; redispatch does not extend deadlines.
Explicit rejection retains 30-second backoff doubling to 1800, up to
max-dispatch-attempts. 401/403/404/422 or exhaustion is permanent, with no automatic
retry. Ignore dispatch responses after an owner callback. Deadlines take precedence.
No per-unit deadline or independent dispatch authority.

The existing SNode.C timer handles debounce, start timeout, rejection backoff,
deadlines and automatic retry. nextDeadline excludes debounce during an active
wave. Configuration changes apply to newly allocated deadlines only.

## 5. Transactional persistence and migration

### 5.1 MariaDB state store

Use SNode.C's asynchronous MariaDBClient only, never libmariadb/libmysqlclient
directly or an ORM. Base API usage on SNode.C master source and examples and cite
the files in the implementation PR. MariaDBClient supports exec/query,
startTransactions/commit/endTransactions; no prepared statements or escaping.
Write strings as _utf8mb4 X'…', integers decimal, absent values NULL. Read strings
as HEX and validate all fields, including embedded NUL and numeric syntax.
After every connection set utf8mb4 and autocommit before other statements.
Use one connection and a database-scoped GET_LOCK single-writer lock.

For each accepted input, calculate candidate state from the committed state,
write every affected state row and its display log entry in one InnoDB transaction,
then expose the candidate and acknowledge success only after commit/endTransactions.
Never expose an uncommitted candidate or dispatch from it. Duplicate/ignored input
requires no write. An unavailable or ambiguous commit outcome hides live state,
fails pending requests with 503 and reloads state before resuming; never assume
rollback or repeat an unknown external mutation blindly. Corrupt schema/rows fail
closed with nonzero exit. Reconnect backoff uses the existing SNode.C timer, 1–60s.

Startup: connect → validate/create state schema → load state directly → drive
pending actions and due times → listen and reconcile declarations. No history
read, replay or Resumed append. No request is accepted before state is ready.
Shutdown uses the framework lifecycle; no reconnect, mutation or dispatch starts
after STOPPING. SIGTERM/SIGINT exit normally; fatal store errors remain nonzero.

### 5.2 One-shot migration

coreci-migrate is a separate executable, never linked into the daemon. Under the
same writer lock, replay an existing schema-5/6 event database once using the
pre-change semantics, including missing dispatch_deadline metadata, w964/w1339
expiry/retry behavior and allocated sequence ids. Preserve every revision,
publication fact, tag and wave history entry. Validate the complete conversion
before installing state tables, then rename old tables to legacy_* and record
the state schema version. An interrupted/ambiguous migration must fail closed
and be recoverable without discarding source history. Refuse concurrent daemon
writers and repeat migration of an already migrated database. No legacy tables
are read by the daemon. An unmigrated database is refused with a migration diagnostic.

Schema 7 state stores upgrade offline using migration/state-7-to-8.sql, under the
same database-scoped writer lock. Atomically replace only the affected tables,
retaining their schema-7 copies. Preserve revisions, frozen source commits and
resolved inputs; imported CI commits equal their historical source commits and
unknown CI trees remain empty until reconciliation. Default dispatch to paused.
Refuse repeated or concurrent conversion; an ambiguous result requires inspecting
the schema version and retained tables before any retry. The daemon accepts only
schema 8 and never performs a runtime migration.

## 6. External interfaces

### 6.1 Complete inbound route list

| Method and path | Authentication | Purpose |
|---|---|---|
| POST /api/v1/github/webhook | Raw-body HMAC-SHA256, X-Hub-Signature-256 | Push notifications: known source tag creations and registry default-branch pushes only; ping acknowledged. |
| GET /api/v1/waves/{wave_id}/plan.json | Public | Frozen W5 plan, including after finish/restart; unknown id 404, recovery 503, no-store. |
| POST /api/v1/waves/{wave_id}/report | Callback HMAC | Workflow events below. |
| GET /api/v1/publications/{publication_id}/plan.json | Public | Frozen W5 publication plan; unknown id 404, recovery 503, no-store. |
| POST /api/v1/publications/{publication_id}/report | Callback HMAC | W6 publication callbacks. |
| GET /api/v1/status.json | Public | Recorded unit status, 503 during recovery. |
| GET /api/v1/badge/{package}/{target}.svg | Public | Badge from recorded unit state, always 200, unavailable during recovery. |
| GET /, /style.css, /app.js, /events | IDP and subject allowlist | Dashboard assets and SSE. |
| POST /api/{operation} | IDP and subject allowlist | retry, rebuild, reload, reconcile, dispatch-paused. |
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
- GET /repos/{owner}/{repo}/commits/{branch} (registry and source default-branch tips)
- GET /repos/{owner}/{repo}/contents/?ref={ref}, Accept: application/vnd.github+json (ci/ tree hash)
- POST /repos/{owner}/{repo}/actions/workflows/{file}/dispatches

There are no Actions run or job lookups. No static bearer credential is accepted or configured. Repository calls use installation tokens; token mint uses the signed JWT. All use X-GitHub-Api-Version: 2022-11-28 and User-Agent core-ci.

Dispatch JSON is W4 on main: wave.yml uses wave_id; publish.yml uses publication_id.
Both include plan_url, callback_url and feed_branch. 204 records accepted dispatch;
other statuses record rejection (transport failure 0). core-ci never sends plan_json.

Outbound TLS verifies the CA chain and hostname before sending request bytes, sets SNI, and uses system CA trust or github-ca-cert. One request per connection, 30-second deadline, no redirects. Pagination accepts only links on github-api-url's exact scheme/host/port and rejects loops. One one-second engine timer applies a transactional timer update only when nextDeadline is due. There is no Actions run/job polling timer; the dashboard timer performs no GitHub I/O. Configured declaration reconciliation (3.3) belongs to workflow management.

### 6.3 Configuration

All configuration uses SNode.C configuration files and command-line options, with
no environment variables. Application options live in an own "coreci" section.
Dashboard server instance "coreci" and optional redirect instance "coreci-redirect"
use their WebApp instance host, port, TLS certificate and key options. No application
option duplicates or programmatic overrides of these instance settings.

Application keys: registry, target-catalog, workflow-repository, feed-branch,
github-app-id, github-installation-id, github-app-key-file, callback-base-url,
github-api-url, github-ca-cert, webhook-secret-file, callback-secret-file,
idp-base-url, idp-client-id, idp-public-key-path, dashboard-users,
db-host, db-port, db-socket, db-user, db-name, db-password-file,
wave-delay, wave-deadline, publication-deadline, start-timeout, retry-delay,
max-dispatch-attempts, reconcile-interval, dispatch-paused.

Secret values are read from operator-owned files; database password uses FILE,
including passwordless socket authentication when omitted. Never print secrets.
All GitHub App keys, callback URL, HMAC files, db-user/db-name, IDP client/public
key and dashboard-users remain required. Positive decimal App/installation ids.
Defaults retain registry SNodeC/Packages:ci/registry.json, catalog ci/targets.json,
workflow repository SNodeC/Packages, feed-branch feed (only feed/feed-staging),
github-api-url https://api.github.com (HTTPS only), db-host localhost, db-port 0,
db-socket empty uses MYSQL_UNIX_ADDR; wave-delay 120 (0–86400), wave-deadline 23400
(60–86400), publication-deadline 23400 (60–86400), start-timeout 900 (1–86400),
retry-delay 600 (0–86400, zero disables), max-dispatch-attempts 8 (1–20),
reconcile-interval 900 (60–86400). Invalid settings fail before connecting and
log only the option name. Package examples use actual SNode.C config syntax.
Deployment notes document the translation from old options and one-shot migration;
service unit changes only as required by configuration. No service activation.

dispatch-paused accepts true or false. Omission preserves the persisted value
(default false for a fresh store). An explicit setting is committed before dispatch.
While paused, allocate and display the pending frozen plan but dispatch neither
waves nor publications. Pending deadlines start on resume; already running waves
and their frozen plans remain unchanged. The authenticated dashboard toggle commits
the same persisted state and is effective across restart.

### 6.4 Callback admission and recovery

Callbacks remain application/x-www-form-urlencoded authenticated with
X-CoreCI-Timestamp and X-CoreCI-Signature-256:
sha256=HMAC(secret,timestamp+"."+rawBody). W6 is the complete protocol.
Authenticate before parsing; decode ids once. Wave reports accept build and target
facts only; publication reports accept publish facts only. Frozen plans and their
respective owner_run_id determine admission, including after deadline.
Non-owner/duplicate/stale callbacks return 200 ignored. Invalid input returns 400;
accepted changes answer 200 only after transaction commit; unavailable store 503.
Webhooks retain the last 1000 successful delivery ids in memory; failed declaration
loads are retryable. No new production access is required.

### 6.5 SSE dashboard

Preserve the existing layout and information about packages, targets, versions, dependencies,
attempts and workflow phases. Derive all phase information solely from committed state:
for build and publish, expose start, end, duration, state and run link
https://github.com/{workflowRepo}/actions/runs/{run_id}[/attempts/{run_attempt}].
Unknown values stay absent; dispatch acceptance must not invent a workflow start.
Retain publication finish times and assignment order from stored state.
Drop GitHub job queued/cancelled states, job cache, job log links and unsupported metadata
(proof, verified_at, uncertainty, publish_assigned, publish_run_bound, retire action, step).

Assets use no-cache. SSE sends a snapshot on connect and full model updates at most once per
second on committed state changes or successful declaration loads; no updates during recovery.
Keep alive every 30 seconds, at most 64 streams (excess 503). Until declarations are loaded,
ready is false and units are empty. A one-second SSE timer may coalesce events and check sessions;
it never requests GitHub data. Dependencies sort descending by level and allocation order.

Add a wave panel with latest id, phase, run link, started/finished and counts by
unit state, with overall unit totals beside the latest wave/retry counts. Show each
unit's source commit, CI commit and dependency versions, plus blocked reasons and
the pending plan while dispatch is paused. Add a publication panel with id, run link, items and phase, and
per-target publication state. Public status includes wave and publication state.

Supported POST /api/{operation}: retry (Failed unit), rebuild (Published unit), reload
(declarations and tags), reconcile (same reconciliation path), dispatch-paused
({"paused":bool}). Retry/rebuild first reload declarations so corrected CI files are
used for the unchanged tag. Unit operations take
{"package":"...","target":"..."} and answer after commit; reload/reconcile answer after loading.
Every other operation answers "Not supported by core-ci". The Refresh control sends reload.

IDP integration stays with snodec-auth 2: authorization code + PKCE S256/state/nonce,
openid profile email, MFA ACR urn:snodec:acr:mfa, RS256 public-key verification and
introspection on each protected request. Only validated subjects in dashboard-users are admitted;
email is display-only. Cookie coreci_access_token is HttpOnly/Secure/SameSite=Lax, path /,
lifetime 12 hours. Re-introspect open streams at least every 60 seconds and close invalid sessions. Stream maintenance uses monotonic elapsed time; the existing one-second tick starts the session check once 59 seconds have elapsed, leaving a tick for the 60-second boundary. Keep-alives also use monotonic elapsed time.

### 6.6 Public status

status.json is generated from recorded unit state by the existing handwritten app/ writer,
without a JSON library. It contains the state sequence, packages and unit selections, published,
selection and failure records, plus the current wave. It is public alongside badges. No presentation dispatch,
status workflow, generation/dirty bookkeeping for a presentation writer, registry presentation
field or status_url field exists.


---

## 7. Contract X v2 — scheduling and execution (authoritative)

Contract X v2 replaces CONTRACT W v2 completely. Where §0–§6 or §8 still describe
waves, chains, the one-shot migration or W-era routes, they are superseded and are
rewritten to match this section during the X implementation (see IMPLEMENTATION-X.md).

X1 Roles.
 - Repositories (Packages ci/registry.json) own ci/packages.json and everything
   project-specific: toolchain activation, build, test, OpenWrt staging
   (Build/InstallDev), package metadata.
 - The catalog ci/targets.json owns the target environment: image (digest),
   distribution repository setup (e.g. Rocky CRB/EPEL, Raspberry Pi archive),
   host_packages, OpenWrt SDK (URL, digest, pinned feed revisions).
 - SNodeC/Packages owns the generic workflows build.yml, publish.yml and restore.yml.
 - core-ci owns admission, scheduling, binding, revisions, dispatch, retry, recovery.
 - Central code (Packages, core-ci) contains no project-specific knowledge.

X2 Admission and turns.
 - Release = (repository, stable tag vMAJOR.MINOR.PATCH, peeled commit). "Newest" =
   numeric SemVer. Deduplicated by repository + tag; a retag (same tag, different
   commit) is reported as a conflict and not admitted.
 - First discovery of a repository (bootstrap, registry addition) admits its highest
   stable tag. Reconcile recovers missed newer tags. Historical tags only via
   "Admit tag".
 - Every admitted release creates one item per declared target. A tag outside the
   declaration's "releases" range → item condition unsupported, with reason.
 - Pass counter N. Every admission, declaration change, retry and committed outcome
   gets eligibility N+1, regardless of how many arrive.
 - A scheduler turn (triggered by events, timers, startup, unpause) opens pass N+1 if
   entries with eligibility N+1 exist, commits that cutoff, and fills ALL free
   reservations; it considers only eligibility <= its pass. Arrivals after the cutoff
   go to the next turn.
 - One queue per repository (admission order, tie-break SemVer). Fill order: the
   oldest ready item across all queues; dependency order comes from readiness.
 - A default-branch push with a changed ci/ tree hash re-reads the declaration.

X3 Declaration, schema 2, read at the default-branch tip (ci_commit):
   {"schema":2, "package", "title", "releases":range,
    "depends_on":[{"package","version":range}],
    "build":{"deb","rpm","openwrt"?},
    "build_dependencies":{"deb":[…],"rpm":[…],"openwrt":[…],
                          "<family>@<distribution>-<release>":[…]},
    "targets":[catalog ids]}
 - Range ">=3.0,<4": comma = AND; operators >=, >, <=, <, ==; abbreviated endpoints
   are normalized (3 → 3.0.0).
 - Selector lists are added to the family list of matching targets.

X4 Items, reservations, selection.
 - Item = release × target. States: enrolled, processing (build job reserved:
   queued, running or terminating), built, published, failed. Conditions of enrolled
   items, each with a reason: waiting; blocked (range unsatisfiable, target missing,
   prerequisite failed, closure conflict, cycle); unsupported.
 - Required option "github_slots" in the coreci configuration section (default 20).
   Reservations = items in processing + the publication reservation <= github_slots.
 - Publication reservation: when an item becomes built and no publication reservation
   exists, the reservation its build just released passes directly to the
   publication. The batching delay (default 2 min) runs inside that reservation; no
   build can take it. It is released when the publication run has finished.
 - A pick is synchronous in memory. Pick, frozen request, attempt and pending
   dispatch are then committed in ONE transaction; only then is the request
   dispatched. If the outcome of a commit is uncertain: reload from the database
   before the next decision.
 - An item belongs to its pick until published or failed.
 - Selection per depends_on, for the same target: if a newer eligible version in
   range is in progress (enrolled, processing, built; not failed, blocked or
   unsupported) → waiting, EVEN IF an older publication would be usable (deliberate:
   no double build; covered by acceptance tests). Otherwise bind the highest retained,
   published version in range with its highest published revision.
 - Closure: merge the frozen transitive bindings of every selected output; exactly one
   output identity per package. A mismatch caused by a pending refresh → waiting;
   otherwise → blocked "closure conflict".
 - Bound output = (package, version, revision, identity, source commit, ci_commit,
   its frozen bindings, inventory).
 - Inventory = per split binary package: name, native version, architecture, sha256,
   runtime dependencies (read from the package metadata by the publisher).

X5 Identity, revisions, retry.
 - build identity = hash(source commit, ci tree hash, target environment digest
   (catalog entry incl. image/SDK digest and pinned feed revisions), workflow contract
   version, identities of the bound outputs).
 - The desired identity is kept separately from the frozen identity of an attempt; it
   is recomputed after committed input changes; duplicates are coalesced.
 - Retry = next attempt of the same revision with the IDENTICAL frozen request (same
   bindings, same feed_commit). Retry is offered only while the desired identity
   equals the frozen one.
 - Differing desired identity → the next revision is enrolled, whether the previous
   attempt failed or was published. A running attempt is never overwritten.
 - Refresh: the newest published version of each consumer per target is re-enrolled
   only when its resolved binding vector changes.
 - Revision high-water mark per package×version×target is persisted independently of
   retention. At cutover it is set above every revision ever allocated.

X6 Namespace, snapshots, retention.
 - APT: apt/<distribution>/dists/<codename>/… with its own pool per distribution.
   RPM: rpm/<distribution>/<release>/<arch>/. OpenWrt: openwrt/<release>/<arch>/.
 - The same native identity (name, version, architecture) with different bytes in one
   namespace → immutable-conflict, rejected.
 - Snapshots: every publication commit additionally gets the ref
   refs/snapshots/<feed_branch>/<publication_id>. core-ci keeps a snapshot while an
   unfinished attempt references it. Released snapshots are deleted by the next
   publication (listed in its plan) in the same atomic push.
   refs/snapshots/<feed_branch>/pre-x is protected: never deleted by publications.
 - Retention on the branch head keeps: the two highest version-revisions per
   package×target; the transitive runtime closure of everything kept (over the
   inventory's runtime dependencies); everything published within the last 7 days.
 - Retirements are decided in one committed scheduler transition and carried in the
   publication plan. A pick never binds an output that is being retired.
 - by-hash: the previous 2 generations of index files stay, with their payloads.

X7 build.yml — workflow_dispatch on main with return_run_details: true;
   run-name "build <request>/<attempt>"; core-ci persists run_id.
   Inputs: request_id, attempt, request_url, callback_url, feed_branch.
 - Request JSON (GET request_url, public, immutable per attempt):
   {"request","attempt","package","repo","commit","ci_commit","version","revision",
    "target":<catalog entry>,"feed_commit",
    "closure":[bound outputs in topological order, transitive ones included, each with
               commit, ci_commit, bindings and inventory],
    "contract":version}
 - One job: claim → catalog setup → resolved declaration list (dnf --allowerasing) →
   install the exact inventory of the direct bindings from snapshot feed_commit, then
   verify what was installed (dpkg-query / rpm -q / opkg / apk) → check out the source
   with submodules and ci/ at ci_commit → hook (env SOURCE_DIR, CI_DIR, OUTPUT_DIR,
   SDK_DIR, TARGET_JSON, PACKAGE_VERSION, PACKAGE_RELEASE, DEPENDENCIES_JSON; no
   callback secret, publication credentials or write tokens in the hook environment)
   → upload artifact coreci-<request>-<attempt> (retention-days 90) with sha256
   manifest and inventory → report finished (artifact id, digest, inventory).
 - On cancellation or failure the job reports finished with status failed.
 - OpenWrt (normative path): SDK digest and feed revisions from the catalog; register
   the closure's recipes (one per package, at their commit/ci_commit) before feed
   resolution; build in topological order with the repositories' Build/InstallDev
   staging; validate runtime dependencies of the split packages; upload only the
   requested item.
 - build.yml holds no signing keys and never writes a feed.

X8 publish.yml — ONE job; environment publish; the only holder of signing keys and
   the only writer of feed branches and snapshot refs; run-name
   "publish <publication>/<attempt>".
 - core-ci persists the immutable publication plan (items, artifact ids, digests,
   retirements, snapshot refs to delete, acknowledged receipts, attempt, expected
   branch head, deadline) and dispatches with return_run_details.
 - Publisher: claim (X9) → re-fetch plan → download artifacts directly from their
   runs, verify identity, allowed contents, checksums → quarantine items with
   immutable-conflict or malformed artifacts, publish the valid rest → read runtime
   dependencies of each split package into the inventory → sign → rebuild touched
   indices (by-hash per X6) → apply retirements → write receipts into the ledger
   (publication, attempt, request, revision, identity, artifact digest, inventory) →
   stream the old tree → ONE orphan-root commit → ONE atomic push: feed branch as a
   leased non-fast-forward update against the expected head + new snapshot ref +
   deletion of released snapshots → callbacks per item → finished.
 - Receipts stay in the ledger until a later plan lists them as acknowledged; core-ci
   acknowledges a receipt once it has committed the outcome.
 - Recovery: missing callbacks are taken from the receipts at the feed head. Ambiguous
   run (dispatch committed, run_id unknown) → look up the run by run-name via the
   GitHub API; none found and no claim past the queue deadline → next attempt (the
   old one gets 409). Deadline exceeded → establish the known run's terminal state,
   compare receipts, only then a new attempt.
 - Transient failures (feed-push, signing, other): retry with backoff, at most 3 →
   item failed, with Retry. Confirmed artifact expiry → item re-enrolled for a build.
 - Staging fault input "fault" (none | after_push), honoured only when feed_branch is
   feed-staging: after_push ends the job with failure directly after a successful
   push, before any callback. Qualification only; rejected for any other branch.

X8b restore.yml — environment publish; manual dispatch only, on the owner's explicit
   instruction (staging: as part of qualification). Inputs feed_branch, snapshot_ref,
   expected_head. Sets the feed branch to the commit of snapshot_ref as a leased
   non-fast-forward update against expected_head. No signing, no other change.

X9 Callbacks (existing HMAC wire format), identical for builds and publications:
 - Claim POST /api/v1/{requests|publications}/{id}/claim (run_id, run_attempt,
   attempt): the same (id, attempt, run_id, run_attempt) gets 200 idempotently after
   commit. Another owner, a stale attempt or run_attempt > 1 (manual rerun) gets 409;
   the job then ends successfully without work.
 - Report POST /api/v1/{requests|publications}/{id}/report: started | per item
   (publications only) | finished. finished may arrive before started; duplicates are
   idempotent; terminal states are monotonic; storage unavailable → 503 and the
   workflow retries with backoff.

X10 Recovery.
 - Persisted: releases (admission order, eligibility, disposition); pass counter;
   items (state, condition, desired identity, attempts with frozen request,
   run_id/run_attempt, deadlines, outcome, artifact id/digest/inventory/expiry);
   revision high-water marks; snapshots in use; retirements; publications (plan,
   attempt, owner, expected head, deadline, receipts, acknowledgements); pause.
 - On restart: resume committed but unsent dispatches; reconcile ambiguous dispatches
   via run-name and claim.
 - Separate deadlines: queue (default 2 h), execution (claim + 6 h; upload and report
   must fit inside GitHub's 6 h job limit), callback (default 15 min after run end).
 - Before replacing a known run: check its state; cancel it or wait for its
   termination. Dispatch retries with bounded backoff.
 - Pause stops new builds and publications; reconciliation of existing work goes on.

X11 Concurrency. Reservations <= github_slots; publication reservation per X4; at most
   one publication per feed branch; publish.yml uses concurrency group
   "publish-<feed_branch>" as a safeguard only.

X12 Versions and feed. deb <version>-<revision>~<codename>;
   rpm <version>-<revision>.<disttag>; ipk/apk <version>-r<revision>. Release fields:
   Origin SNodeC/Packages, Label SNode.C, Suite = Codename = <codename>, Components
   main, Acquire-By-Hash yes, SHA256 only, InRelease + Release.gpg. Ledger
   publications.json with inventory and receipts. Immutability: same package, target,
   version and revision already in the ledger → nothing placed, reported published
   with reason "already published".

---

## 8. Acceptance

Build with adapters and run full CTest twice green, no required skips, against
disposable MariaDB and fake GitHub/IDP HTTP fixtures. Label all server-dependent
tests mariadb, including ConnectionLoss tests. Package scope ctest -LE mariadb
must pass without server-test dependencies. Keep no browser/Node/npm requirements.

Migration: production-shaped tests/data/legacy-replay-baseline.json and w964/w1339
history yield every revision, publication, tag and wave field identical to the
pre-change baseline, including under changed daemon configuration.
State: transactional updates, commit failure/unknown outcome, corruption,
shutdown, restart at every boundary including commit-before-dispatch, immediate
recovery of outstanding wave/publication actions. A source/link test proves no
event store, EventRow/EventSql/EventQueue or replay path exists in the daemon.

Publication: target-finished dispatches early; batch several ready targets;
targets arriving during publication enter the next; owner fencing; already-published
idempotency; retry failed publication from the same artifact without revision
allocation; test artifact-unavailable, immutable-conflict, feed-push, signing, other,
unknown and missing failure values, including misleading reason text; unavailable
artifact rebuild; wave completion waits for all targets;
the next wave waits. Test deadlines and late owner facts for both workflow types.

Configuration: parse an SNode.C configuration file including coreci application
section and WebApp instance options; start the daemon with no configuration
environment variables; verify required-file validation without leaking values.
Preserve HMAC/TLS/App auth, declaration validation/SemVer, graph/revision/cascade,
retry/backoff/deadline, frozen plans, status/badges, SSE, IDP authentication and
retry/rebuild/reload/reconcile behavior. Exercise populated and changing dashboard
models and public HTTP boundaries. Retain 10000 seeded randomized scenarios.

Report baseline/final physical production lines per directory and dashboard
assets, migration lines separately, test lines, routes, configuration keys,
environment variables (zero), tables, SPEC old/new SHA256, PRs/merge SHAs, checks,
owner option translation and merged CONTRACT W v2. Daemon production code must
not exceed the owner-approved ceiling of 5,850 physical lines. Report net growth
against the baseline separately from migration and test code. Record reversible choices as "Decision: … because …".
Merge SPEC first, implementation only after qualification; delete task branches,
leave canonical main pulled and clean. No tag, installation or deployment.
