# core-ci — state and publication specification

Section 7 defines Contract X v2. IMPLEMENTATION-X.md defines its implementation,
qualification, and rollout.

## 0. Goal

core-ci is a single-threaded C++20 daemon. It admits repository releases, resolves
their declared dependencies, allocates revisions, and dispatches builds and
publications through GitHub Actions. The daemon reconciles workflow runs and feed
receipts. The Packages workflows build, package, sign, and update feeds.

## 1. Constraints

Committed state is authoritative. A scheduler decision and every fact it creates
are committed together before they are used for dispatch or acknowledged. Display
history is append-only. Scheduling and recovery read the current committed state.

SNode.C owns networking, TLS, timers, and asynchronous MariaDB access. The
scheduler and persistence interfaces build without SNode.C. Build C++20 with
-Wall -Wextra -Wpedantic -Werror. Observe Actions runs asynchronously as specified
in X10. Apply the concurrency rules in X4 and X11. Scheduling starts with the
Contract X schema on a fresh database and the revision floor from the rollout.

## 2. Ownership and workspace

Repositories own project-specific declarations, hooks, recipes, and package
metadata. The target catalog owns target environments. The Packages workflows
execute frozen requests and plans. core-ci owns scheduling decisions, frozen
inputs, claims, deadlines, and recovery. The state store owns transaction
boundaries. GitHub App, HMAC, IDP, MariaDB, and SNode.C adapters retain their
transport responsibilities.

Canonical source: /home/voc/projects/snodec/core-ci.
Persistent agent build: /home/voc/projects/snodec/build/codex/core-ci/contract-x-v2.
Manual build: /home/voc/projects/snodec/build/qtcreator/core-ci/<configuration>.
Temporary builds: /tmp/codex/build/core-ci/contract-x-v2.
Temporary artifacts: /tmp/codex/artifacts/core-ci/contract-x-v2.
Use the existing canonical checkout and explicit formatting and analysis targets.
Preserve AGENTS.md, chore/owner-pending-core-ci, and local credentials.

## 3. Declarations and admission

Read the schema-1 registry and its target catalog from the same resolved
default-branch commit. Resolve canonical repository spelling and default branch
with GitHub. Read the schema-2 declaration at that repository's default-branch
tip and resolve its ci/ tree hash from the same commit. Validate uniqueness of
repository, package, and target identities, supported families, required fields
and types, relative paths, and references. Reject duplicate JSON keys, files over
1 MiB, and failed HTTP reads. Failed loads preserve committed inputs.

Process registry and target changes and registered source tag or default-branch
pushes. Re-read source declarations on changed ci/ tree hashes. Reconcile at
startup, at the configured interval, and on request. Paginate tag listings and
compare stable versions numerically. Accept stable tag names vMAJOR.MINOR.PATCH
without leading zeroes. Initial discovery, subsequent admission, explicit
historical admission, and retag conflicts follow X2. Missing targets, version
ranges, cycles, and closure conflicts determine item conditions under X3 and X4.

## 4. Scheduler and durable state

### 4.1 Stored facts

Persist the pass, releases and admission order, desired and frozen item
identities, condition and reason, revision high-water marks, and pause. Each
attempt retains its immutable request or publication plan, ownership tuple,
run state and conclusion, report and timing facts, and absolute deadlines.
Persist artifact provenance, inventory and expiry, publication receipts and
acknowledgements, pending retirements, and snapshot references required for
retention and rollback. The daemon schema defines the exact stored representation.

### 4.2 Picks, runs, and publication

Open each pass with a committed cutoff. Apply X2 and X4 to fill all free
reservations in readiness and admission order, giving the next free reservation
to eligible publication work when no publication is unreconciled. Resolve
transitive bindings and freeze the request and its current feed snapshot.
Commit the pick, attempt, and pending dispatch in one transaction.

Observe completion through GitHub, independently of callbacks. Release the
reservation at observed completion and finalize a build when a valid finished
report and observed completion are both committed. When its report is missing,
apply the callback grace deadline and record the conclusion as specified in X7.
Hold further publications until the previous one is reconciled. Freeze the
publication plan after the persisted batching deadline. Receipts at the feed
head supply missing publication outcomes and remain until acknowledged by a
later plan.

A changed desired identity enrolls a new revision under X5. Retry preserves the
frozen inputs and revision and advances the attempt. On confirmed artifact
expiry with unchanged desired identity, automatically rebuild the same revision
using those inputs. Protect the frozen snapshot while its attempt is active or
its item is built or retry-eligible. Enforce the expected-head, rollback-point,
and pre-x protections of X6 when freezing snapshot deletions.

### 4.3 Recovery and pause

After restart, drive pending dispatches, locate ambiguous runs by run-name, and
resume deadline processing and reconciliation. Establish the outcome of a known
run before replacing it. Observe every incomplete run at the poll interval and
at deadlines. Queue, execution, and callback grace deadlines are separate from
publication batching. The scheduler continues recovery and observation while
paused and stops initiating new builds and publications.

## 5. Transactional persistence

Use SNode.C's asynchronous MariaDBClient. After every connection set utf8mb4
and autocommit. Use a database-scoped GET_LOCK for the single writer. Encode
SQL strings as _utf8mb4 hex literals, write integers in decimal and absent
values as NULL, and validate decoded database fields. The daemon creates and
validates the Contract X schema and fails closed on an unexpected schema or
corrupt state. The rollout provides the next allowable revision per
package, version, and target from the old database and feed ledger.

Compute candidate state from the committed state and write the transition in
one InnoDB transaction. Expose its result and dispatch pending actions after
commit. Duplicate inputs that leave state unchanged require no write. On
unavailable storage or an uncertain commit, return 503 for affected requests
and reload before deciding or dispatching again. Use bounded asynchronous
reconnect backoff. Stop starting work on framework shutdown.

## 6. External interfaces

### 6.1 Inbound routes

| Method and path | Authentication | Purpose |
|---|---|---|
| POST /api/v1/github/webhook | GitHub raw-body HMAC-SHA256 | Ping, registered source tag and default-branch pushes, registry default-branch pushes. |
| GET /api/v1/requests/{id}/attempts/{attempt} | Public | Immutable X7 request for the specified attempt. |
| GET /api/v1/publications/{id}/attempts/{attempt} | Public | Immutable X8 plan for the specified attempt. |
| POST /api/v1/requests/{id}/claim, /api/v1/publications/{id}/claim | Callback HMAC | X9 claim. A successful response signs the owner identity and the document digest. |
| POST /api/v1/requests/{id}/report, /api/v1/publications/{id}/report | Callback HMAC | X9 reports from the claim owner. |
| GET /api/v1/status.json | Public | Committed scheduler and item status. |
| GET /api/v1/badge/{package}/{target}.svg | Public | Badge from committed package and target status. |
| GET /, /style.css, /app.js, /events | IDP and subject allowlist | Dashboard assets and SSE. |
| POST /api/retry, /api/admit-tag, /api/reconcile, /api/dispatch-paused | IDP and subject allowlist | Dashboard actions. |
| GET /auth/callback | IDP authorization-code validation | Complete login. |

Use SNode.C express routing, HTTP, TLS, and timers. Answer 405 for an unsupported
method on a known route and 404 for an unknown route or request. Use no-store for
dynamic public responses. Status, requests, and plans return 503 during
recovery. The optional plain HTTP listener redirects with 308 to the configured
HTTPS origin. Preserve parser limits of 25 MiB for webhook bodies, 64 KiB for
other bodies, 64 KiB and 100 fields for headers, and 8 KiB for the start line.

### 6.2 GitHub API

Preserve GitHub App JWT (RS256) authentication and the in-memory installation
token cache. Coalesce token mints and refresh and retry once after a repository
request returns 401. Verify outbound TLS host names and CA chains. Preserve
request timeouts and same-origin, loop-free pagination.

Use the repository, commit, contents, and tags APIs for declaration and release
discovery. Dispatch build.yml on main with request_id, attempt, and feed_branch.
Dispatch publish.yml on main with publication_id, attempt, feed_branch, and
the X8 fault input. Request return_run_details for each and record the
returned run id. Use Actions run listing to locate ambiguous
dispatches by exact run-name, run details to observe known runs, and cancellation
where X10 requires it. Read the configured feed head and its ledger for
publication reconciliation.

### 6.3 Configuration and service

The application reads SNode.C settings in the coreci section. WebApp settings
belong to the coreci and optional coreci-redirect instances, including host,
port, certificate and key, and parser limits. The service runs
/usr/bin/core-ci with --config-file /etc/coreci/coreci.conf and the enabled
WebApp instance sections. Application settings do not come from environment
variables.

Preserve registry, target-catalog, workflow-repository, feed-branch, GitHub App,
API and CA options, the HTTPS base URL, HMAC files, IDP endpoint and subject
allowlist, database connection options, and reconcile-interval. Read secret
values from operator-owned files and log option names on configuration errors.
Validate before scheduling. Use the scope and defaults of X4, X8, and X10:
github_slots defaults to 20, publication batching to 2 minutes, polling to
5 minutes, queue deadline to 2 hours, execution deadline to 6 hours, and callback
grace to 15 minutes. Reject qualification_fault unless the feed branch is
feed-staging. Token consumption is part of the committed publication plan.
Persist the dashboard Pause action and start qualification with Pause enabled.

The Packages workflows derive core-ci URLs from the repository Actions variable
CORECI_ORIGIN and fail closed if it is unset. Set it during staging installation
as specified in IMPLEMENTATION-X.md §5b.

### 6.4 Callback admission

Preserve application/x-www-form-urlencoded callbacks and the X-CoreCI-Timestamp
and X-CoreCI-Signature-256 headers. The signature is
sha256=HMAC(secret,timestamp+"."+rawBody). Authenticate before parsing, reject
invalid authentication or timestamp skew over 300 seconds with 401, and reject
invalid input with 400. Commit accepted changes before answering 200. Sign each
successful claim response under X9, including the digest of the exact bytes
served for its attempt. Workflows verify the signature, timestamp, owner fields,
and document digest before doing work. The publisher re-fetches the plan and
verifies its digest before pushing. Apply the idempotent claim and 409 rules in
X9, accept a finished report before started, and enforce monotonic terminal
outcomes.

### 6.5 Dashboard and public status

Use the existing OAuth authorization-code flow with PKCE S256, state and nonce,
MFA ACR urn:snodec:acr:mfa, RS256 key verification, token introspection, and a
subject allowlist. Preserve the HttpOnly, Secure, SameSite=Lax session cookie.
Re-introspect streams at least every 60 seconds. Send committed snapshots via
SSE, coalescing updates to once a second with 30-second keep-alives and a limit
of 64 streams.

Show the pass, queued/running/publication reservations, repository queues with
item states and reasoned conditions, and the package-by-target matrix. Show
version and revision, attempt, bindings, feed_commit, run links, and measured
queue, setup, staging, build, upload, and publication timings. Leave unknown
timings absent. Provide Retry for identical inputs only, Admit tag, Reconcile,
and Pause. Public JSON and badges derive from the same committed state.

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
 - Item = release × target. States: enrolled, processing (an attempt exists whose
   outcome is not finalized), built, published, failed. Conditions of enrolled items,
   each with a reason: waiting; blocked (range unsatisfiable, target missing,
   prerequisite failed, closure conflict, cycle); unsupported.
 - Every attempt (build or publication) has a run state, separate from the item
   state: dispatched → claimed → completed (observed via the GitHub API, X10).
 - Required option "github_slots" in the coreci configuration section (default 20).
   Reservations = attempts (builds and publications) whose run state is not
   completed <= github_slots. A reservation is released when its run is observed
   completed; core-ci observes every known run independently of callbacks (X10).
 - Superseding an ambiguous attempt (builds and publications): a dispatched attempt
   whose run_id is unknown, for which a run-name lookup performed immediately before
   the decision finds no run, and which has no claim past the queue deadline, is
   superseded. In ONE transaction: its run state becomes superseded (counts as
   completed for reservations; any later claim for it gets 409), the next attempt is
   created, and the reservation passes directly to that next attempt. A run of the
   superseded attempt that GitHub creates later is observed like any known run, holds
   no reservation, and ends without work at its claim (409). An attempt whose run IS
   found is a known run and is never superseded; past its deadline it is cancelled
   and replaced only after its completion is observed (X10).
 - Outcome finalization of a build attempt: as soon as the run is observed completed
   AND a valid finished report has arrived (in either order) → built or failed per the
   report. Run completed without a report → callback grace (X10), then failed with
   reason "conclusion <conclusion>, no report"; retry eligibility per X5.
 - Publication reservation: whenever publication work exists (built items not
   assigned to a publication, or a due timed retirement, X6) and no publication is
   unreconciled, the publication claims the next released reservation before any
   build (on a build's release, on publication completion, on the retirement timer
   and at startup). A publication is unreconciled until its receipts or its failure
   are reconciled (X8), not merely until its run completes. The batching deadline (default 2 min after the claim) is persisted and
   runs inside that reservation; no build can take it. It is released when GitHub
   reports the publication run completed; at that moment the rule is evaluated again.
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
 - Bound output = (package, repository, version, revision, identity, source commit,
   ci_commit, its frozen bindings, inventory).
 - Inventory = per split binary package: name, native version, architecture, sha256,
   runtime dependencies (read from the package metadata by the publisher).

X5 Identity, revisions, retry.
 - build identity = hash(source commit, ci tree hash, target environment digest
   (catalog entry incl. image/SDK digest and pinned feed revisions), workflow contract
   version, identities of the bound outputs).
 - The desired identity is kept separately from the frozen identity of an attempt; it
   is recomputed after committed input changes; duplicates are coalesced.
 - Retry = next attempt of the same revision with byte-identical frozen inputs (X7
   "inputs", including bindings and feed_commit); only the attempt envelope changes.
   An item is retry-eligible while it is failed and its desired identity equals the
   frozen one. Retry is offered only to retry-eligible items.
 - Differing desired identity → the next revision is enrolled, whether the previous
   attempt failed or was published. A running attempt is never overwritten.
 - Refresh: the newest published version of each consumer per target is re-enrolled
   only when its resolved binding vector changes.
 - Revision high-water mark per package×version×target is persisted independently of
   retention. At cutover it is set above every revision ever allocated.

X6 Namespace, snapshots, retention.
 - APT: apt/<distribution>/dists/<codename>/… with its own pool per distribution.
   RPM: rpm/<distribution>/<release>/<arch>/. OpenWrt: openwrt/<release>/<arch>/.
 - Immutability is decided on the original artifact bytes (sha256 of each split
   package as produced by the build, before signing). The ledger records this original
   digest and, separately, the sha256 of the served file (after RPM signing).
   Same native identity (name, version, architecture) in one namespace with the same
   original digest → "already published"; with a different original digest →
   immutable-conflict, rejected.
 - Snapshots: every publication commit additionally gets the ref
   refs/snapshots/<feed_branch>/<publication_id>. A pick always pins the snapshot of
   the current feed head as feed_commit. core-ci keeps a snapshot while an attempt
   referencing it is active, or its item is built (until published or failed), or
   its item is retry-eligible (X5).
 - Rollback points: the snapshots of the 3 most recent reconciled publications per
   feed branch are protected and never deleted while they are among those 3; they
   are the rollback targets besides pre-x.
 - A publication's deletion list never contains the snapshot of its own expected
   head. A snapshot is listed for deletion only if, in the committed transition that
   freezes the plan, it is not the current head and no attempt pins it. The deletion
   happens in that publication's atomic push.
 - refs/snapshots/<feed_branch>/pre-x is protected: never deleted by publications.
 - A head older than the last accepted head is accepted only per X8b.
 - Legacy content (on the first X publication into a feed that has pre-X content):
   - The legacy APT tree apt/dists/ and apt/pool/ is carried over byte-identical and
     is never indexed, bound or changed by X. 30 days after the cutover a timer makes
     "retire legacy APT" due; it is publication work (X4): it joins the next plan, and
     if no items are built, a publication containing only this retirement is
     dispatched. It removes both directories.
   - Legacy RPM and OpenWrt packages stay in their namespaces. The first publication
     that touches such a namespace imports them into the ledger as legacy entries:
     served sha256 from the file, runtime dependencies read from the package
     metadata, original digest "unknown". Indices are rebuilt over X entries and
     legacy entries together. Legacy entries are never bound and are subject to the
     normal retention (they disappear once newer X revisions are kept).
   - Publication date of a legacy entry (for the 7-day rule): the timestamp from the
     old database's publication records where one matches; otherwise the committer
     time of refs/snapshots/<feed_branch>/pre-x (an upper bound of the true date). It
     is recorded in the ledger entry at import.
 - Retention on the branch head keeps: the two highest version-revisions per
   package×target; the transitive runtime closure of everything kept (over the
   inventory's runtime dependencies); everything published within the last 7 days.
 - Retirements are decided in one committed scheduler transition and carried in the
   publication plan. A pick never binds an output that is being retired.
 - by-hash: the previous 2 generations of index files stay, with their payloads.

X7 build.yml — workflow_dispatch on main with return_run_details: true;
   run-name "build <request>/<attempt>"; core-ci persists run_id.
   Inputs: request_id, attempt, feed_branch. No URL is ever taken from an input:
   every core-ci URL is derived from the repository Actions variable CORECI_ORIGIN
   (trusted configuration, e.g. https://core.vchrist.at:8443); unset → fail closed.
 - Request JSON (GET <CORECI_ORIGIN>/api/v1/requests/{id}/attempts/{attempt},
   public, immutable per attempt):
   {"request","attempt",
    "inputs":{"package","repo","commit","ci_commit","version","revision",
              "target":<catalog entry>,"feed_commit",
              "closure":[bound outputs in topological order, transitive ones included,
                         each with repository, commit, ci_commit, bindings, inventory],
              "contract":version}}
   "inputs" is frozen at pick and byte-identical for every attempt of a revision.
 - One job: claim → catalog setup → resolved declaration list (dnf --allowerasing) →
   install the exact inventory of the complete closure from snapshot feed_commit,
   then verify every closure entry as installed at its exact version
   (dpkg-query / rpm -q / opkg / apk) → check out the source
   with submodules and ci/ at ci_commit → hook (env SOURCE_DIR, CI_DIR, OUTPUT_DIR,
   SDK_DIR, TARGET_JSON, PACKAGE_VERSION, PACKAGE_RELEASE, DEPENDENCIES_JSON; no
   callback secret, publication credentials or write tokens in the hook environment)
   → upload artifact coreci-<request>-<attempt> (retention-days 90) with sha256
   manifest and inventory → report finished (artifact id, digest, inventory).
 - On failure the job reports finished with status failed.
 - Outcome per X4 finalization. A run completed without a finished report becomes
   failed after the callback grace, for every conclusion (success, failure, cancelled,
   timed_out, startup_failure, action_required, stale, skipped, neutral), with reason
   "conclusion <conclusion>, no report"; it is retry-eligible only per X5 (desired
   identity equals the frozen one). An artifact of a run without report is never used.
 - A cancelled run that still delivers its finished report reports status failed with
   reason "cancelled".
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
   branch head, deadline) and dispatches with return_run_details. Inputs:
   publication_id, attempt, feed_branch, fault (X8 staging input). The plan is read
   from <CORECI_ORIGIN>/api/v1/publications/{id}/attempts/{attempt}; no URL from an
   input (as X7).
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
   item failed, with Retry.
 - Confirmed artifact expiry of a built item that was never published: if its desired identity still equals
   the frozen one → automatically the next attempt of the SAME revision with
   byte-identical inputs (its snapshot is still pinned, see X6), dispatched under the
   normal reservation rules; otherwise the next revision is enrolled per X5.
 - Staging fault input "fault" (none | after_push), honoured only when feed_branch is
   feed-staging: after_push ends the job with failure directly after a successful
   push, before any post-push report (the claim has already happened). core-ci sets it
   only from the configuration option "qualification_fault" = "after_push:<token>"
   (refused at startup unless feed_branch is feed-staging). Each token is used at most
   once: its consumption is committed together with the publication plan that carries
   it, so a restart never arms it again. Qualification only.

X8b restore.yml — environment publish; manual dispatch only, on the owner's explicit
   instruction (staging: as part of qualification). Inputs feed_branch, snapshot_ref,
   expected_head. Sets the feed branch to the commit of snapshot_ref as a leased
   non-fast-forward update against expected_head. No signing, no other change.
   restore.yml has run-name "restore <feed_branch> <snapshot_ref>". core-ci accepts a feed head that is older than its last accepted head (by the ledger's publication sequence) only with this evidence: a restore.yml run, observed via the GitHub API, completed with conclusion success; its run-name names exactly this feed branch and snapshot ref; the commit of that snapshot ref equals the observed head; and the run completed after core-ci's last accepted head change. Without this evidence, core-ci keeps its last accepted head and shows "feed head behind: <sha>". On acceptance, core-ci sets itself paused and records "restored to <snapshot_ref>"; only the owner unpauses. Items whose published output is missing from the restored ledger return to built while their artifact is available (the next publication republishes them); otherwise the next revision is enrolled with current inputs and a fresh snapshot pin. A revision that was ever published is never rebuilt; it is only republished from its original artifact.

X9 Callbacks (existing HMAC wire format, secret CORECI_CALLBACK_SECRET), identical for
   builds and publications; all URLs derived from CORECI_ORIGIN (X7):
 - Claim POST /api/v1/{requests|publications}/{id}/claim (run_id, run_attempt,
   attempt): the same (id, attempt, run_id, run_attempt) gets 200 idempotently after
   commit. Another owner, a stale attempt or run_attempt > 1 (manual rerun) gets 409;
   the job then ends successfully without work.
 - Claim response authentication: core-ci signs the 200 response with the same HMAC
   scheme (X-CoreCI-Timestamp, X-CoreCI-Signature-256 over "<timestamp>.<body>").
   Body: {"id","attempt","run_id","run_attempt","granted":true,"document_sha256"},
   where document_sha256 is the sha256 of the exact request JSON (builds) or plan
   JSON (publications) bytes served for that attempt.
 - The workflow does work only if: the signature verifies, the timestamp is within
   5 min, id/attempt/run_id/run_attempt equal its own, and the sha256 of the document
   it fetched equals document_sha256. Any other outcome (unsigned 200, mismatch,
   non-200 other than 409, transport error after retries) → fail closed without work.
   The publisher re-fetches the plan before pushing and checks it against
   document_sha256 again.
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
 - Run observation: core-ci polls the GitHub API for every known run that is not yet
   observed as completed (default every 5 min, and at every deadline), independently
   of callbacks. Runs with unknown run_id are looked up by run-name.
 - Separate deadlines: queue (default 2 h from dispatch to claim), execution (claim +
   6 h; upload and report must fit inside GitHub's 6 h job limit), callback grace
   (default 15 min from the observed completion of the run).
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
   publications.json with inventory (original and served digests) and receipts.
   Immutability per X6: byte-equivalent → nothing placed, reported published with
   reason "already published"; otherwise immutable-conflict.

---

## 8. Acceptance

Run the acceptance scenarios in IMPLEMENTATION-X.md §§2–4. Exercise the scheduler,
persistence, and workflows through their public boundaries as well as focused
tests. Preserve coverage of GitHub App authentication, HMAC, TLS, OAuth/MFA,
MariaDB transaction and recovery behavior, SNode.C configuration, and changing
dashboard and public status data. Run CTest with adapters and disposable MariaDB,
and verify the package test scope without the server-dependent tests.

Report production and test lines separately. Measure daemon source using the
counting rule in tests/slim_source.py plus daemon DDL (ceiling 5,000) and Packages
tools Python lines (ceiling 1,000). Report the changed routes, configuration,
and schema, the staged §7 hash, and links to commits, PRs, and checks.

Follow the ordered rollout, T1–T8, and owner gates in IMPLEMENTATION-X.md.
Record the qualification results, timings, and accepted exceptions in issue #76.
