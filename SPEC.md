# core-ci — state and publication specification

This is the master contract. Section 7 is fixed owner-supplied CONTRACT W v2.

## 0. Goal

A single-threaded C++20 daemon loads tag-bound declarations, maintains a typed
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
default-branch HEAD commit. Source/schema 1 declarations follow W2 and are read at
the selected tag's commit SHA. Validate required fields/types, supported families,
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
retry/rebuild. Freeze source repo, tag, commit, version, revision, inputs and direct
prerequisites. Reset allocation timings, assign Planned and consume the request.
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
while available, retaining version/revision; if unavailable it requires a rebuild.
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
max-dispatch-attempts, reconcile-interval.

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
unit state. Add a publication panel with id, run link, items and phase, and
per-target publication state. Public status includes wave and publication state.

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
without a JSON library. It contains the state sequence, packages and unit selections, published,
selection and failure records, plus the current wave. It is public alongside badges. No presentation dispatch,
status workflow, generation/dirty bookkeeping for a presentation writer, registry presentation
field or status_url field exists.


---

## 7. GitHub side — wave and publication contract

W1 Repositories. SNodeC/Packages, default branch main, holds .github/workflows/wave.yml,
   .github/workflows/publish.yml, ci/registry.json {"schema":1,"repositories":[…]} and the target
   catalog ci/targets.json. Production feed: branch `feed`; staging feed: branch `feed-staging`.
W2 Source declaration, read at the tag: ci/packages.json {"schema":1, "package", "title",
   "depends_on":[packages], "build":{"deb":path,"rpm":path,"openwrt":path?},
   "build_dependencies":{"deb":[…],"rpm":[…],"openwrt":[…]}, "targets":[catalog ids],
   "revision_base":N?}.
W3 Catalog entry: {"id","family":"deb"|"rpm"|"ipk"|"apk","distribution","release","codename",
   "arch","image","runner","platform","emulated":bool,"disttag"?,"sdk"?}. Fields other than id,
   family, distribution, release, codename and arch are opaque to core-ci.
W4 Dispatch. Wave: workflow_dispatch of wave.yml on main with inputs wave_id, plan_url,
   callback_url, feed_branch. Publication: workflow_dispatch of publish.yml on main with inputs
   publication_id, plan_url, callback_url, feed_branch. Both accept plan_json (base64) for manual
   runs; empty callback_url = no callbacks.
W5 Plans (GET plan_url, public, JSON).
   Wave plan: {"wave", "feed_branch", "owner_run_id":id|null, "targets":[{"id","units":[{"package",
   "repo","tag","commit","version","revision","build_id","action":"build"|"reuse"}]}]}; units per
   target in topological order; "reuse" = install the published version-revision from the feed;
   only targets with ≥1 "build" unit.
   Publication plan: {"publication", "feed_branch", "owner_run_id":id|null, "items":[{"wave",
   "wave_run_id","target","artifact","units":[{"build_id","package","version","revision"}]}]};
   all units of one target are in one item.
   owner_run_id is null until the first accepted "started" callback of that wave/publication, then
   that run_id, fixed afterwards.
W6 Callbacks, signed with the existing core-ci callback HMAC wire format.
   Wave run → POST https://<core-ci>/api/v1/waves/{wave_id}/report: scope=wave status=started
   (run_id, run_attempt) | finished (run_id, conclusion); scope=unit phase=build
   status=started|succeeded|failed (run_id, build_id, reason when failed); scope=target
   status=finished (run_id, target, artifact — empty if no unit of the target built successfully).
   Publication run → POST https://<core-ci>/api/v1/publications/{publication_id}/report:
   scope=publication status=started (run_id, run_attempt) | finished (run_id, conclusion);
   scope=unit phase=publish status=succeeded (run_id, build_id, feed_commit, reason
   "already published" when applicable) | failed (run_id, build_id, reason).
W7 wave.yml: job plan → matrix job chain (one job per planned target, fail-fast false; units in plan
   order; a unit whose same-target prerequisite failed is skipped without callback; at the end the
   job uploads artifact coreci-<wave>-<target> with the successfully built packages and a sha256
   manifest, then sends scope=target finished) → job report (always): scope=wave finished. wave.yml
   holds no signing keys and never writes a feed.
W8 publish.yml (environment publish, the only holder of signing keys): re-fetch plan_url
   immediately before pushing; publish only if owner_run_id equals GITHUB_RUN_ID, otherwise
   succeed without push and without callbacks (fetch/validation errors fail closed). Download the
   listed artifacts from their wave runs (same repository, GITHUB_TOKEN), verify the manifests,
   place packages, rebuild the indices of touched suites, apply retention, ONE orphan-root commit,
   push with lease; callbacks per unit, then scope=publication finished.
W9 Versions: deb <version>-<revision>~<codename>; rpm <version>-<revision>.<disttag>;
   ipk/apk <version>-r<revision>.
W10 Feed: apt/ (dists/<codename>/main/binary-<arch>/, pool/main/; Release with Origin
   SNodeC/Packages, Label SNode.C, Suite = Codename = <codename>, Components main, Architectures,
   Date, Acquire-By-Hash yes, SHA256 only, InRelease + Release.gpg), rpm/<distribution>/<release>/
   <arch>/ (signed packages, repodata, repomd.xml.asc), openwrt/<release>/<arch>/ (ipk: Packages +
   Packages.sig usign; apk: signed packages.adb), keys/. Ledger publications.json. Immutability:
   if the ledger already holds the same package, target, version and revision, no file is placed
   or changed and the unit is reported published with reason "already published". Retention:
   newest 2 publications per package×target.
W11 Concurrency. core-ci runs at most one wave at a time and at most one publication run per feed
   at a time. Whenever no publication is active and at least one target has reported
   scope=target finished with a non-empty artifact and is not yet published, core-ci dispatches
   one publication containing all such targets. Targets finishing while a publication runs go into
   the next one. A wave is finished when its run reported scope=wave finished (or its deadline
   expired) and every planned target is published, failed or blocked; the next wave starts only
   after that. wave.yml uses concurrency group "wave-<feed_branch>", publish.yml
   "publish-<feed_branch>", both without cancellation.

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
allocation; unavailable artifact rebuild; wave completion waits for all targets;
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
be below the baseline. Record reversible choices as "Decision: … because …".
Merge SPEC first, implementation only after qualification; delete task branches,
leave canonical main pulled and clean. No tag, installation or deployment.
