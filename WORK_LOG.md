# Work log

## 2026-10-25 — reset and design checkpoint

- Verified the exact canonical root, Git metadata, current branch and dirty-file inventory.
  Remote API confirms main as default and no open PR. Local main and previous task HEAD
  were 9a7472f. SSH remote inspection failed on local SSH configuration; GitHub connector
  reads succeeded. No credentials or SSH settings were changed.
- Owner explicitly authorized discarding existing application changes. Created
  feat/persistent-package-graph from main and removed prior application/design/build
  contents. Preserved .git/, .qtcreator/ and .local-credentials/ without reading their
  contents. No sibling checkout, global instruction or external credential was changed.
- Read only repository engineering instructions and metadata from the old project;
  did not read or reuse its source, tests, protocols or design.
- Studied SNode.C database/HTTP demos, MariaDB client/connection internals, SNodeC lifecycle,
  and MQTTSuite mqttstore configuration, asynchronous storage and HTTP client ownership.
  Inspected installed snodec CMake discovery and the canonical formatting manifest.
- Owner reiterated explicit startup MariaDBClient::connect(). Later commands establish
  a connection lazily only when its framework connection object is absent; uncertain
  mutations are not replayed. Startup success gates listener creation.
- Prepared a fresh design and production/test estimates. Production implementation
  awaits the explicit net-growth approval required by global engineering instructions.
- Asked owner about already-tagged enrollment and coherent diamond change waves.
  These policies remain unsettled; no execution policy is silently selected.
- No build, database, job, feed, key provisioning, install or live acceptance performed.

## 2026-10-25 — owner-requested independent review

- Asked the installed Claude CLI to review the complete design against a detailed
  condensation of the owner specification, with tools and session persistence disabled.
- Saved the full review in docs/claude-design-review.md. Verdict: approve with required
  revisions, not as written. Review is advisory; external API claims remain unverified.
- Accepted that the diamond safety requirement is mandatory, not an optional policy.
  Retry-edge continuity, feed identity isolation, publication serialization and trusted
  assignment/run binding need explicit resolution before implementation.
- Did not implement reviewer suggestions, alter deployment settings or activate anything.

## 2026-10-25 — complete proposal refinement

- Replaced the initial proposal with an original-scope crosswalk and explicit disposition
  of every Claude finding. Kept the independent review unchanged as earlier evidence.
- Owner chose immediate highest-SemVer enrollment work, supersession of older waiting
  automatic work with history, and inclusion of prereleases. These supersede the earlier
  unanswered-policy entries; diamond safety is mandatory, not another optional policy.
- Defined stable logical executions/attempts, exact prerequisite edges, atomic diamond
  selection, serialized async DB ownership and uncertain-COMMIT/dispatch reconciliation.
- Specified tag-preserving image digest resolution, workflow/App actor/artifact checks,
  isolated hooks, suite/SDK-separated feeds and one publisher per repository/branch.
- Checked current official GitHub docs: dispatch now documents a run ID response; optional
  larger concurrency queues exist, but do not replace persisted publication authority.
- Proposed a shared prerelease codec; 20 adjacent comparisons passed using local dpkg and
  20 using local RPM. This is a design experiment, not package/live acceptance. IPK/APK
  and complete ordering/bounds proof remain required. No source/test implementation added.
- Verified MariaDB source readiness notification and SNode.C descriptor receiver interfaces;
  actual managed-process readiness/shutdown integration remains a checkpoint-1 proof gate.
- Added explicit setup/configuration/secret inventory and file-input provisioning examples.
  Cross-repository private artifact reading is an explicit conditional credential choice,
  never assumed worker-token access or reuse of the controller/feed-writer private key.
- Revised estimate to 7,000-10,500 production / 2,500-4,000 test lines; moved a real signed
  DEB+apt+dependent path to checkpoint 2. Growth approval and external activation are separate.
- Asked remaining prerequisite-closure, runtime-dependency and older-retry promotion policy
  questions. No response is inferred. No production deployment or acceptance was performed.
- Final reduction pass removed a proposed redundant current-publisher pointer: the
  feed row lock plus authoritative publishing-attempt predicate provides serialization.
- Security-boundary review made publisher SDK trust explicit: a source-provided hash
  is integrity evidence, not permission to run its tooling with signing keys. Exact SDK/tool
  approval belongs in reviewed worker trust configuration, without a controller release list.
- Verification: git diff --check passed; local Markdown links resolve; all original A-S
  sections and Claude D1-D7 findings have explicit coverage. Only documentation changed.

## 2026-10-25 — additional owner policy decisions

- Owner requires a visible error when selected prerequisite work is missing; no automatic
  upward prerequisite execution is created. Existing selected waiting/failed executions
  remain dependency blockers, and atomic creation of the affected diamond stays intact.
- Owner permits successful explicit retries of older inputs to become the newest rebuild
  of the same source version. Retry reserves a new revision; prior identities stay immutable.
- Owner requested an explanation of runtime dependency constraints. No answer was inferred;
  this choice concerns installed package compatibility, not exact graph build inputs.
- Clarified that build-only graph edges need not become runtime package dependencies.

## 2026-10-25 — implementation continuation

- Owner selected compatible source-declared runtime dependency ranges and instructed
  continuation after the refined proposal/estimate. Builds remain pinned to exact artifacts.
- Began the approved implementation checkpoints in the canonical feature branch. Copied
  canonical source/CMake/analysis configurations byte-for-byte and wired explicit targets.
- Installed MariaDB API has async commands/transactions but no prepared/escaping interface;
  future text values will use SQL-mode-independent hex literals through one DB owner.
- Implementing the database lifecycle first: explicit initial connect, named HTTP instance,
  native child-authenticated MariaDB readiness and framework-owned descriptor observation.
  No application startup timer, polling/retry loop, second event loop or thread is introduced.
- initialConnection is consumed once so later framework reconnection cannot register routes
  or a listener again. Managed watchers retain their resource owner through unregistering.
- This is ongoing implementation, not a completed controller or a live package-pipeline claim.
- Lifecycle checkpoint verified using installed SNode.C 2.1.0 interfaces, GCC 16.2 Debug,
  and real isolated MariaDB under /tmp/codex/build/core-ci/foundation. All six relevant
  tests executed and passed: help, managed readiness/shutdown, unmanaged session-loss
  reconnection, managed initial DB failure, unmanaged initial DB failure, and child startup
  failure. The help assertion was corrected to the framework's named-instance help surface.
- Explicit format-check and diff whitespace checks passed. All eight canonical tooling
  files match both source bytes and recorded SHA256. Source lines remain at most 160 chars.
- Checkpoint adds 560 production lines and 174 test lines; canonical copied tooling
  is 754 lines separately. No previous production mechanism is retained or removed in this
  fresh checkpoint. Temporary builds stay at the permitted task location for ongoing work.
- No GitHub enrollment/build/publication or client package installation has been tested.
- Accounting correction: the canonical byte copies contain pre-existing EOF blank lines
  reported by staged diff --check. They are preserved unchanged as required; task-owned
  source/changes pass whitespace checks. Format-check and SHA256/byte-copy checks passed.

## 2026-10-25 — configuration and enrollment checkpoint

- Owner confirmed signing keys remain GitHub Actions secrets. The controller App PEM
  is separate and referenced by a protected external SNode.C configuration file.
- Read the new controller App/installation settings through Chrome; verified the supplied
  private file's public fingerprint against GitHub and restricted its permissions to 0600.
  Created the external GitHub config under the owner-supplied ci-test directory, without
  copying private material into the checkout. Deployment IDs remain outside application source.
- Real GitHub App JWT/token authentication and repository-scoped metadata reads succeeded
  for all five authorized sandbox source/worker repositories. No GitHub repository writes,
  workflow dispatches, signing, feed publication or public listener activation performed.
- Added configurable repository name/ID grants; token requests use repository_ids and
  metadata reads verify the configured ID. Rechecked framework/MQTTSuite configuration,
  Express, MariaDB and HTTP client patterns before implementing the boundary.
- Implemented strict definition/target/graph validation and numeric SemVer ordering,
  asynchronous serialized database operations, fresh enrollment schema/startup validation,
  local UI editing, GitHub commits/import and committed-revision feedback.
- Local enrollment fixture uses real isolated MariaDB and synthetic TLS GitHub responses.
  Covers stale edits, origin rejection, rejected/lost GitHub writes, reconciliation, restart,
  and rollback after a verified remote commit. Restart exposed missing address reuse;
  corrected through SNode.C's existing setReuseAddress(), with no startup retry mechanism.
- Uncertain database COMMIT/rollback blocks later mutations until reconciliation after
  restart. This replaces unconditional mutation progression after uncertain outcomes.
- Pipeline scheduler, worker tooling and complete live acceptance remain ongoing work.
- Added a real MariaDB wire fault test: the proxy forwards COMMIT to the server, observes
  success, then drops its acknowledgement. Verified durable server data, an uncertain
  application result, prevention of further GitHub writes, and restoration after restart.
  This is deliberate local fault injection, not live GitHub/package acceptance.
- Checkpoint accounting (source/UI/schema/config/build versus tests separately): {'docs/tooling': [117, 3], 'production': [1801, 52], 'tests': [604, 20]}; counts precede this log entry. No legacy implementation is retained.
- Relevant local suites passed across their final affected runs: seven lifecycle cases, enrollment HTTP/real-DB fault boundaries, definition/SemVer cases, and four TLS/App cases. Explicit format-check and staged whitespace review passed. Read-only live access also passed with ID-scoped configuration.


## 2026-10-25 — graph and state persistence checkpoint

- Added the ordinary graph with immutable selected sources/targets and prerequisite IDs.
  States live on attempts only; readiness is derived from all prerequisite publications.
  Atomic diamond planning, independent targets, explicit older selections, history-preserving
  automatic supersession and definitive-failure retry use the same graph.
- Execution identity uses selected inputs/prerequisite IDs, not request type. Explicitly
  claiming waiting work protects it without duplicating it; explicitly requesting a
  superseded selection retains the old record and references it from the new selection.
- Removed full-matrix copies from each target execution. Definitions are stored once per
  immutable repository revision, referenced by foreign keys; compact execution snapshots
  are checked against that definition before persistence and again on restore.
- Added asynchronous conditional transactions against a graph-generation row. Commit
  acknowledgement precedes graph mutation. Stale callbacks fail and roll back without
  authorizing dispatch. Removed the redundant unconditional database mutation API.
- Centralized enrollment persistence at the same graph-generation boundary. Namespace
  reservations survive configuration renames and are enforced for selected source revisions;
  tests prove another repository cannot claim an earlier reserved namespace.
- Added /api/graph and UI rendering of authoritative state, source/definition revisions,
  dependency links, blockers, failures and assigned GitHub log links.
- Local tests prove diamond gating, failure/retry, duplicate/late/conflicting reports,
  exact retained inputs, supersession, independent targets, real MariaDB atomic persistence,
  restart of assigned/unknown work, stale run-identity rejection and namespace rollback.
  The database driver uses synthetic source/run metadata; this is not live GitHub job evidence.
- Observed framework diagnostic during deliberate lost-COMMIT fault injection: installed
  SNode.C may log epoll_ctl MOD/EBADF while suspending a socket already closed by libmariadb.
  Traced to MariaDBConnection::checkStatus() descriptor handling before its error callback.
  State uncertainty/reconnection tests pass. No framework source was changed and no application
  timer/deferral workaround was added; retain this as an integration limitation for handover.
- Live event-to-build/publication coordination, worker tooling and full acceptance remain
  ongoing; nothing has been deployed, pushed or published by these changes.
- Namespace checks retain all historical claims and reject conflicts in the same conditional
  transaction as enrollment/selection. Preflight checks precede UI-authored GitHub writes.
  Configuration operations retain their original graph generation throughout GitHub I/O.
- Local enrollment/graph persistence and domain suites passed after the final namespace
  changes; the seven lifecycle cases and definition/version suite also passed. No simulated
  job result or local graph fixture is counted as live package-pipeline acceptance.
- Checkpoint line accounting (before this log entry): {'production': [1163, 63], 'docs': [41, 1], 'tests': [383, 0]}. Formatting, staged whitespace and 160-column source checks passed.


## 2026-10-25 — immutable GitHub source reading

- Implemented one source reader for enrollment and execution selection: paginated numeric
  SemVer selection including prereleases, explicit older tags, bounded annotated-tag peeling,
  exact commit/tree/blob reads, regular ci/packages.json validation and Git blob hash checks.
  Ambiguous highest precedence requires an exact request; no default branch fallback exists.
- Enrollment uses that reader rather than a second contents decoder. Optimistic writes
  compare file blobs and effective configuration revisions, allowing unrelated branch commits.
  Invalid committed JSON can be read/repaired without accepting an invalid effective graph.
  Confirmed write receipts must contain the expected blob identity before local acceptance.
- Consolidated repository name validation separately from package namespaces; underscores
  and canonical uppercase repository names are supported. Config grants reject duplicate IDs.
- Synthetic TLS source tests passed for pagination/prereleases, older nested annotated tags,
  moved tags, ambiguity, no version tags, symlink definitions, changed blob bytes, invalid
  definitions and invalid direct tags. Added duplicate-grant startup rejection coverage.
  Real MariaDB enrollment tests cover definition repair, unrelated commits and changed-file CAS.
- Read the public bot/feed identities for later deployment configuration; no repository,
  workflow, secret or feed write was made. Chrome became unavailable for optional UI visual
  verification; JavaScript syntax/API boundary checks remain separate from browser rendering.
- Source-event coordination and trusted worker/publication code remain ongoing work.
- Checkpoint accounting (before this entry): {'production': [546, 94], 'docs': [24, 1], 'tests': [266, 8]}. Explicit format-check, JavaScript syntax and staged whitespace checks passed.
