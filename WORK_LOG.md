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

## 2026-10-25 — approved framework URL encoder fix

- Owner explicitly approved fixing SNode.C and pushing without creating a branch.
  Used its existing canonical checkout; preserved all unrelated receiver edits.
- Fixed unsigned-byte hexadecimal insertion and ASCII unreserved classification.
  New regression fails against the installed library; corrected encoder and existing
  HTTP query-parser tests both pass. Changed-file formatting and whitespace checks pass.
- Commit 1092fda70a60dabd7ac4cf63dc109551784482bc pushed directly to SNodeC/snode.c master.
  Existing local fix/receiver-removal-callbacks branch remains checked out. No branch,
  tag, additional checkout or installation was created. Production +4/-3; tests/build
  registration +32/-0. Build retained at /tmp/codex/build/snode.c/url-encoding for evidence.
- Installed framework remains unchanged; core-ci must consume the fixed public helper
  once the corrected framework is installed and verified.

## 2026-10-25 — shared asynchronous HTTPS transport

- Extracted the existing SNode.C TLS transport from GitHub authentication into one
  reusable HTTP client for GitHub, image resolution and later HTTPS feed evidence.
  Named instances/options are registered before init; certificate verification uses
  the framework callback before SSL creation. Automatic reconnect/retry is rejected.
- Added conservative request-submission evidence. A lost repository-operation response
  remains uncertain; failed token acquisition proves the requested operation was not sent.
  No mutation is replayed by the transport.
- Fifteen affected TLS/source/enrollment boundary tests pass, including rejected/lost
  token responses, lost operation responses and real-MariaDB enrollment faults.
  These are local fixtures, not live workflow or package-publication acceptance.
- The upstream URL encoding correction is pushed but remains uninstalled locally.
  The application retains its single correct encoder pending framework installation.
- Checkpoint accounting: production +263/-138; tests +17/-2; documentation +58/-0.
  Removed the obsolete proposed patch after its upstream commit; retained the evidence guide.

## 2026-10-25 — consume installed framework encoding correction

- Owner updated installed SNode.C. The installed public-interface probe now returns
  v1.0.0%2Bbuild. Deleted core-ci's encoder and use the framework helper for GitHub refs.
- Exact older annotated-tag selection now tests +build metadata in the raw identity
  and %2B in the GitHub request. This boundary passes. The other seven source cases
  and real-MariaDB enrollment boundary passed with the updated dependency.
- Production +4/-19; tests +4/-4; documentation updated separately. No install was
  performed by this core-ci change; it consumes the owner's installed update.

## 2026-10-25 — immutable image and SDK selection

- Added bounded OCI/Docker schema-2 index/manifest/configuration reads through the
  installed SNode.C TLS client. Exact platform selection, descriptor/hash checks and
  deployment-owned registry/token/redirect origins precede execution assignment.
  OpenWrt SDK selections retain the source URL/checksum under a separate origin grant.
- Source runner selectors and build parameters remain untouched. No local container,
  filesystem layer or SDK processing occurs. Unsupported private registry auth and
  ambiguous platforms report errors instead of guessing.
- Twelve local TLS environment cases cover hashes, sizes, configuration mismatch,
  ambiguous platforms, scoped pull authentication, origin rejection and credential-safe
  redirects. All pass, along with seven lifecycle and six GitHub transport cases.
- A redirect test exposed cached framework addresses; use its public renewal API only
  when host/port changes. A real registry check exposed cached default HTTP Host handling;
  select Host through ConfigHTTP before request construction. Client structure remains
  unchanged. Owner explicitly deferred fixed-host:port client refactoring to later work.
- Real public-registry metadata probing is separate from synthetic fixtures and package
  acceptance. No GitHub job, SDK compilation, signing, installation or feed update yet.
- Read-only real Docker registry resolution passed for debian:trixie/linux/amd64,
  verifying manifest/configuration bytes through the approved registry, token service
  and observed configuration CDN. Digest: sha256:15a71cc87173e191d5046f9c4288db4e8954b67f92e48d9587087826b0afe562.
  Temporary probe settings are under the build directory, not deployment configuration.
- Checkpoint accounting before this entry: {'production': [526, 2], 'tests': [234, 0], 'documentation': [55, 1]}. Canonical format-check and whitespace checks pass.

## 2026-10-25 — graph-driven build coordination checkpoint

- Connected UI enrollment/highest-tag policy, explicit older source requests, tag webhooks,
  graph selection, durable dispatch authority, assigned run/job/artifact inspection and
  explicit retry. Added raw-body HMAC verification and repository/installation checks;
  workflow files use the existing exact Git tree/blob reader, not a second reader.
- Preserved separate source and workflow commits. Approved workflow blob identities and
  deployment roles/public signing identities are explicit configuration, frozen per source.
  Added the previously verified controller bot ID to the protected external config only.
- Reconciliation preserves lost/inaccessible assignments, advances independent eligible
  targets and runs once after listener success. Unknown outcomes never authorize replacement
  jobs. Identical report application avoids redundant graph-generation writes.
- Found and corrected an existing guard bug: enrollment's captured expected generation
  was ignored in SQL. Real MariaDB regression now rejects stale enrollment after another
  committed graph change; generation guards remain the sole mutation authorization.
- Local end-to-end fixture uses actual HTTP routes and real MariaDB with explicitly
  synthetic GitHub/OCI servers. Proves pre-request committed state, highest prerelease,
  older +build tags/hooks, distinct workflow/source commits, HMAC rejection, lost dispatch
  response, restart, assigned-run failure, fresh retry revision, duplicate/late reports and
  assigned job/artifact metadata. It executes no GitHub job or source package hook.
- Worker/feed enrollment is rejected. The UI exposes source requests, retry, log links,
  blockers and explicit inspection. Client endpoint refactoring remains deferred at owner
  direction. No public UI, App webhook, workflow, job, signing key or feed was activated.
- Trusted build/publication tooling, verified feed receipts and required live package-manager
  installation/signature acceptance remain unfinished; do not describe this as deployment ready.
- All 37 local suites pass after the final coordinator changes. Canonical formatting,
  JavaScript syntax, whitespace and 160-column source checks pass. Live GitHub jobs and
  package publication/installation acceptance remain unperformed.
- Checkpoint accounting before this entry: {'production': [1179, 78], 'tests': [480, 98], 'documentation': [83, 2]}. No generic workflow engine or persisted ready queue added.

## 2026-10-25 — fresh native worker and signing boundary

- Confirmed existing signing/feed/observer secret names through read-only GitHub metadata;
  values were neither retrieved nor changed. Copied only public verification files from
  DistributionPackages at a recorded immutable commit. The sandbox has no protected
  publication environment yet; actual private/public matches remain a job-time check.
- Prepared fresh pinned source/reusable worker, separate validation/publication and feed
  Pages workflows. Chose sandbox packages/main and its standard project Pages URL after
  owner delegated the URL choice. Pages remains disabled and no GitHub file was written.
- Added the shared assignment/version/artifact contract, bounded archive/feed downloads,
  standard DEB/RPM metadata checks, isolated native Docker worker preparation and a working
  C reference hook for both formats. Builds import exact prerequisite packages/exports.
- Added native trusted signing/indexing, private/public correspondence checks, temporary
  keyrings, immutable package identity protection, byte-preserving replay, normal checked
  feed push and HTTPS receipt verification. Publisher code never executes source hooks.
- Found GitHub artifact names reject the colon in attempt IDs. The graph now assigns one
  safe artifact_name in immutable inputs, consumed directly by controller and workflows.
  C++/Python assignment hashes are checked across the real HTTP boundary, including Unicode.
- Real local compilation/DEB/RPM packaging, native version ordering, signatures/indexes,
  wrong-key/modified-index/modified-package rejection and identical replay pass with isolated
  generated test keys, cleaned after testing. Twelve synthetic provenance mismatch cases
  reject before artifact download. No real GitHub job or package-manager install ran.
- Five affected suites pass: real-MariaDB enrollment, graph, pipeline, native package/signing
  and synthetic worker provenance. Canonical C++/CMake formatting, Python syntax and YAML
  parse checks pass. Docker execution, cross-repository reader access, OpenWrt tooling,
  controller publication/receipt integration and full live acceptance remain unfinished.
- The fixed-origin HTTP client refactoring remains deferred at the owner's instruction.
- Checkpoint accounting before this entry: {'production': [1887, 1], 'tests': [482, 3], 'documentation': [277, 10], 'public_key_data': [29, 0]}. Growth implements the approved
  worker/signing boundary; one shared contract replaces duplicated metadata/version policy.

## 2026-10-25 — committed publication and latest SDK inputs

- Wired verified build results to committed publication authorization. Publishing attempts
  themselves serialize the feed repository/branch; no writer pointer or durable queue exists.
  State validation/restoration rejects conflicting writers. Terminal results traverse eligible
  work, including publication dispatch rejected before submission.
- Added a dedicated feed endpoint. Public GitHub reads bind the configured feed repository
  ID and receipt commit; the existing exact Git tree/blob reader now supports explicit public
  reads, replacing a separate receipt-file interpretation. Matching HTTPS bytes are required
  before published state releases dependents. Missing/inaccessible proof preserves assignments.
- Real-MariaDB/synthetic-TLS pipeline tests prove pre-dispatch committed publication state,
  restart without replacement, duplicate/late reports, changed HTTPS receipt rejection and
  recovery of a verified feed result despite a failed GitHub job. No fake record is described
  as a live publication. Local empty Git fixtures additionally prove normal checked updates,
  ignored-byte rejection and stale writer conflicts; their Git directories are cleaned up.
- Owner changed SDK policy: choose latest automatically, with no SDK SHA maintained in JSON.
  Removed exact release/SDK URL/checksum fields from OpenWrt definitions. Suite/arch remain
  row-level; build contains target/format. Deployment selects the official metadata origin.
  Read versions_list numerically, select the latest stable patch in that suite, check actual
  target/package architecture/format through profiles.json, and record resolved URL/checksum
  only as immutable execution evidence. Retries retain their recorded inputs.
- SDK changes create distinct selections/rebuild revisions and retain superseded waiting
  history. Prerequisite selection requires matching resolved SDKs; it never creates upward
  prerequisite builds. Five new TLS cases cover a new patch, absent series, architecture and
  format mismatches, and ambiguous SDK archives. No release/architecture/runner catalogue.
- Real read-only metadata resolution through installed SNode.C selected 25.12.6/x86/64
  from downloads.openwrt.org. No SDK archive was downloaded. Configured sdk-base-url only
  in the protected external deployment file; no private credential or GitHub secret changed.
- All 44 local suites passed after integration; subsequent focused graph/pipeline/Git checks
  passed after final changes. Formatting, Python syntax and YAML parsing pass. Full live
  GitHub/package-client acceptance, Docker isolation execution, OpenWrt worker/signing and
  UI workflow installation remain unfinished. Failed privileged jobs without sufficient
  pre-push/feed evidence remain uncertain rather than gaining an unsafe replacement retry.
- SDK policy documentation supersedes the original prompt's manually configured SDK pins.
  Existing multi-origin environment-client refactoring remains deferred at owner direction.
- Checkpoint accounting before this entry: {'production': [462, 138], 'tests': [238, 33], 'documentation': [105, 22]}. Growth implements the approved
  publication boundary/latest discovery; exact-file reading and SDK compatibility stay shared.


## 2026-10-25 — restore the controller/worker boundary

- Owner identified scope drift. Removed the controller registry/OCI/SDK resolver, named
  environment HTTP client and configuration, and its pre-selection callback traversal.
  The graph selects source/target/prerequisite work directly. It keeps one state authority.
- Native image resolution now uses standard Docker tools in GitHub worker code; latest
  SDK metadata discovery moved to worker tooling. No controller Docker/build/signing work
  was introduced. Physical SDK compatibility belongs at prerequisite import in the worker.
- Resolved identities are separate manifest evidence, checked by the trusted publisher and
  included in the verified publication receipt retained by the graph. Historical receipts
  remain immutable; no migration or compatibility layer was introduced.
- Removed tests for the deleted controller resolver and replaced the relevant platform,
  latest SDK, format and export-compatibility cases at the worker boundary. Pipeline tests
  assert that dispatch requires no controller-resolved environment. These remain fixtures,
  not live GitHub/Docker/SDK acceptance.
- Automatic approval review rejected a proposed additional retry-evidence artifact protocol,
  GitHub reads/permissions and controller reference field as an unapproved architectural
  extension. None of that proposal was executed. Environment pinning across retries remains
  an explicit blocking boundary for activation; no silent environment upgrade is approved.
- Verification: reduced controller builds; the initial local run passed 27/28 suites.
  The new worker SDK fixture exposed standard checksum whitespace handling; corrected it,
  then all four affected graph/pipeline/worker suites passed. Final package/signature tests,
  canonical C++/CMake formatting, Python syntax and whitespace checks passed. These are
  local checks, with real MariaDB/package tools and synthetic GitHub/worker responses.
- Working-tree checkpoint accounting before this entry: {'production': [136, 664], 'tests': [125, 324], 'documentation': [90, 68]}.
  No commit, push, installation or external activation. Retry completion remains pending
  explicit approval; this working tree must not be described as a finished correction.


## 2026-10-25 — approved GitHub retry environment evidence

- Owner explicitly approved the separate workflow evidence artifact, read-only retrieval
  on retry and prior assigned-run references in assignments. This resolves the preceding
  approval block; no controller registry/SDK functionality is reintroduced.
- GitHub records assignment-bound environment evidence before any source hook. Retried
  work reuses that identity, including across an intervening pre-build failure. Missing
  or expired evidence after a possible build fails clearly instead of changing inputs.
  An attempt proven not to reach recording/hooks permits initial resolution.
- Consolidated run provenance checks for package and environment artifacts. Both use the
  existing bounded, digest-verified download and archive checks. Only contents/read and
  actions/read are requested; the token stays in the trusted environment step.
- Controller changes carry prior assigned-run references and preserve original prerequisite
  receipts. No graph state, durable queue or controller environment client was added.
- Verification: final six affected suites passed (graph, enrollment/persistence, pipeline,
  worker environment, artifact provenance and native packages/signatures). Subsequent
  focused evidence tests also pass after the final missing-proof check. C++ build and
  canonical formatting, Python formatting/syntax, rendered workflow YAML permissions,
  maximum production line lengths and whitespace checks pass. No live GitHub/Docker or
  package-client installation acceptance is claimed.
- Combined correction accounting before this entry: production +297/-676 (net -379),
  tests +297/-325 (net -28), documentation +135/-69. Approved retry completion accounts
  for 149 net production lines within the 130-170 estimate, while the whole correction
  reduces production code. No push, installation, secret update or external activation.


## 2026-10-25 — finish workflow administration and publication failure classification

- Added an explicit workflow preview/commit UI and REST endpoint. Render the committed thin
  workflow with configured worker/tooling pins, require its configured approved blob SHA,
  read the enrolled branch through the existing immutable file reader, reject stale previews,
  and report committed revisions or GitHub failures without inventing local matrix authority.
- Split trusted publication into preparation and the sole checked feed-write step. The
  controller first checks the feed receipt; if unavailable, canonical assigned job/step
  evidence can prove failure before a feed mutation. Possible writes remain assigned and
  uncertain. No new report protocol, graph state or scheduler was introduced.
- Local HTTP/MariaDB tests prove workflow preview/create/update/no-op, conflicts/denied writes,
  pre-push explicit retry, malformed evidence rejection, and uncertainty after possible push.
  Native package/signature tests pass. These remain local fixtures, not live acceptance.


## 2026-10-25 — OpenWrt worker path and authorized sandbox secret setup

- Prepared GitHub-side SDK extraction/toolchain checks, source hook execution, selected
  prerequisite imports, IPK/usign and APKv3 signing/indexing. The controller gained no build,
  SDK or signing responsibility. Standard SDK/package tools remain authoritative.
- Owner approved roots choosing latest SDKs and dependents reusing prerequisite SDKs.
  Updated the existing worker resolver; all selected prerequisite receipts must agree before
  preparation, including joins. Retries retain prior assigned environment evidence.
- Created sandbox package-publication with a main-only branch policy. Verified the existing
  sandbox coordinator App PEM and a feed-only Contents-write token, then revoked the token.
  Provisioned PACKAGES_APP_PRIVATE_KEY via stdin and the two public feed-writer identifiers
  as environment variables. No repository secrets, signing keys or production settings changed.
- Package signing private-key files are absent from the owner-named local directory. GitHub
  secret values cannot be retrieved. Requested actual file paths; no replacement keys created.
- Local archive/package-metadata and worker-resolution fixtures are distinct from outstanding
  real SDK, GitHub pipeline, publication and package-client installation acceptance.
- Verification: four affected local worker suites pass (SDK inheritance, archive boundaries,
  native package/signature tooling and provenance). Canonical C++/CMake format checks, Python
  formatting/syntax, workflow YAML parsing, hook shell syntax and whitespace checks pass.
- Checkpoint accounting: production +616/-76 (net +540), tests +117/-0 and test-build wiring
  +5/-0. Growth implements the originally approved missing OpenWrt worker/signing path,
  reusing existing artifact, receipt and publication boundaries. No controller expansion.


## 2026-10-25 — locate and provision existing package signing keys

- Owner authorized a project-directory search before Codex-log searches. Found all three
  existing package signing files in an owner-held private directory, with directory mode
  700 and files mode 600. No Codex-log search was necessary and no private values were printed.
- Verified OpenPGP fingerprint and APK EC P-256 public-key agreement with trusted worker
  keys, plus a temporary standard usign signature/verification. OpenPGP packet inspection
  shows no passphrase protection. Temporary verification files were removed.
- Provisioned APT_SIGNING_KEY, OPENWRT_USIGN_KEY and OPENWRT_APK_KEY into the existing
  main-only sandbox package-publication environment via stdin. GitHub secret metadata
  confirms all three plus the previously provisioned feed-writer key. No new keys, rotation,
  repository-secret changes, workflow activation or publication. Production/test changes: 0.


## 2026-10-25 — collect owner-requested local sandbox credentials

- Copied existing package signing private/public keys, four sandbox App private/public
  key pairs and the existing observer token into a Git-ignored local credential directory.
  Added a local inventory of roles and source paths; no secret contents entered tracked files.
- Verified copied bytes, standard OpenSSL public-key derivation, Git exclusion and owner-only
  directory/file permissions (700/600). Originals and runtime configuration remain in place.
  Production SNodeC credentials and personal GitHub login credentials were excluded.
- No credential generation/rotation, GitHub changes, activation or publication. Production
  and test changes: 0; no application tests needed for this local file collection.


## 2026-10-25 — prepare source-owned sandbox recipes and activation review

- Read the four sandbox projects' current C sources and tag/workflow metadata. Prepared
  source-owned hooks, own-only development exports, and explicit five-row definitions.
  The controller gained no package build or discovery responsibility. Dependencies retain
  source-declared broad compatibility while assigned build artifacts remain exact.
- Local real-tool tests build all four synthetic source fixtures in DEB and RPM formats,
  checking metadata, own exports and the joined runtime result. All eight builds pass.
  The controller definition parser validates all four definitions as one graph; build,
  canonical formatting, Python formatting/syntax and whitespace checks pass.
- Recorded exact old sandbox workflows to disable, fresh worker/source/feed file placement,
  Pages setup and proposed fresh v2.0.0 tags for owner activation review. No GitHub files,
  workflows, tags or feeds changed. Public webhook/TLS deployment remains owner-supplied.
- This checkpoint is preparation within the approved implementation scope; live GitHub,
  SDK/container execution, signed feed and apt/dnf/opkg/apk installation remain outstanding.
- Accounting: source hook code +175/-0, source-owned JSON +308/-0, tests +114/-1 and
  test-build wiring +11/-1. Growth supplies original-scope source recipes and explicit
  sandbox definitions within the approved estimate, without a second implementation in
  the controller. Final hook tests pass after the package-license metadata adjustment.


## 2026-10-25 — reserve direct-test network ports

- Checked actual listeners and the active LAN route. Allocated public HTTPS TCP 443 to
  machine TCP 8443 for webhooks; the existing UI stays on loopback HTTP 8080 without router
  forwarding. Recorded the current LAN address and DHCP reservation requirement.
- No application listener, router, firewall or TLS configuration changed. Current application
  code still exposes only its local HTTP listener; dedicated SNode.C TLS webhook setup needs
  the owner's DNS hostname and certificate/key paths. No reverse proxy was installed.
- Documentation-only change; production/test lines unchanged. No runtime acceptance claimed.


## 2026-10-25 — owner-selected public webhook hostname

- Recorded gw-home.vchrist.at on public HTTPS port 443. DNS resolves, but the existing
  endpoint's office.vchrist.at certificate fails verification for the selected hostname.
  Valid TLS certificate/key paths remain required. No existing service or router changed.
- Read-only DNS/TLS checks and documentation only; production/test lines unchanged.


## 2026-10-25 — authorized sandbox workflow deployment

- Disabled twelve old sandbox workflows after verifying no queued/running jobs. Installed
  fresh worker tooling and pinned source workflows/hooks/definitions through checked commits
  on main in the five authorized repositories. Verified exact remote blob contents/modes.
- Installed the feed Pages workflow and enabled Actions-based Pages. A real Pages job passed;
  HTTPS public-key bytes match GitHub. A temporary read-only GitHub credential check passed
  for all four source artifact listings and was then removed. No package pipeline claim.
- Used the existing controller App for workflow writes because the human CLI token lacks
  workflow scope; used the logged-in browser for the feed workflow, without expanding App
  grants or OAuth scopes. Configured public worker variables and exact local controller pins.
- Existing feed keys differ from all three selected/provisioned signing identities. Preserved
  feed keys, package bytes, source tags and existing repository secrets. Owner approval or
  matching old private files are required before signing-key transition/publication. The local
  observer-token file failed authentication, but the actual GitHub reader secret passed.
- No production SNodeC changes, application push/installation, extra source checkout or secret
  disclosure. Local production/test source changes: zero; live evidence is recorded separately.


## 2026-10-25 — preserve feed trust while correcting signing configuration

- Credential-only log/path inspection located the original sandbox private keys. Standard
  GnuPG and usign signing/verification prove APT/IPK correspondence to the existing feed.
  No discarded application/orchestration implementation was recovered or reused.
- Existing APK trust is RSA-3072, incompatible with the required new EC P-256 publisher.
  Preserve keys/apk.pem and its existing feeds; name the already-provisioned P-256 public
  key core-ci-apk.pem for new SDK-specific feeds. One existing filename mapping changes;
  the overwrite-rejection guard and package layout remain intact. No new private keys.
- Copied active and legacy key files into owner-only Git-ignored credential storage, then
  provisioned matching original APT/IPK exports into package-publication through stdin.
  No signing private material entered logs, tracked files, build artifacts or MariaDB.
- Three affected local suites pass, including native real-tool signatures, provenance,
  and preservation/replay/rejection checks for the distinct APK public-key path. No
  GitHub package publication or client-installation acceptance is inferred from these checks.
- Deployed worker b45e47c415501c8afe0d3abb9c5cfb71f4e8c71c, repinned all four source
  workflows and updated protected controller/public worker settings. Added only the distinct
  EC public-key file to the feed; verified every prior Git blob/mode remained unchanged.
- Live Pages deployment passed. HTTPS retrieval confirms all four public files, including
  the preserved RSA legacy key. Removed obsolete repository-level key secrets after retaining
  verified owner-held backups; active signing/feed keys remain environment-only.
- Executable production code +1/-1 (one filename), public key data +23/-22, tests +16/-0.
  No new controller mechanism, source tags, package publication or client-installation claim.


## 2026-10-25 — first real graph-driven sandbox builds

- Started the controller on loopback HTTP 8080 with an isolated managed MariaDB under
  /tmp/codex/build/core-ci/live-sandbox. The administrator database remains untouched.
- Enrolled demo-core through the actual UI. Selected v2.0.0 at immutable commit
  7cffc1403a20664c8c07e49ae559e16b7089479e and dispatched all five target assignments.
- GitHub runs 40228867034, 40228875914 and 40228871183 built native DEB/RPM packages,
  then failed independent validation because Python could not import the trusted module.
  Set that invocation's module path explicitly; source output stays read-only and offline.
- OpenWrt runs 40228873452 and 40228869039 failed before hooks: the official SDKs
  contain absolute host-tool symlinks rejected by the archive data filter. This remains
  unresolved in this checkpoint; extraction protections have not been relaxed.
- Native package/provenance suites pass with real package/signature tools. Production
  code +1/-1, tests unchanged. No successful GitHub artifact or publication claim.


## 2026-10-25 — repair official SDK preparation

- Inspected checksum-verified official SDKs 24.10.8 and 25.12.6. All absolute symlinks
  are host/bin bindings to the SDK build machine, including missing /builder and /opt paths.
  Omit precisely those archived bindings; keep internal links and the standard data filter.
- Keep archive entry counting independent of retained members, preserving resource bounds.
  Added regression cases for valid host links, builder bindings, traversal and external links
  outside host/bin. The regression reproduced the original AbsoluteLinkError before the fix.
- Both actual archives extract successfully, omitting 25 and 20 builder bindings respectively.
  SDK target queries then exposed absent generated .config files. Run the standard SDK
  defconfig on GitHub before queries, also regenerating host command bindings for that host.
- Owner explicitly approved the narrowly scoped preparation fix. No controller architecture
  changed. Local archive, worker environment and provenance suites pass; GitHub verification
  of defconfig/build remains pending. No local package compilation or publication performed.
- Deployed worker 47ecaa83870723a7f79eb0a8a0d260b5e6cec2ef and dispatched five fresh
  graph selections for the unchanged v2.0.0 source. Native runs 40229812225 (RPM),
  40229819909 (DEB AMD64) and 40229822170 (DEB ARM64) passed, including validated artifacts.
- OpenWrt APK run 40229815999 passed extraction and reached SDK prerequisite checks,
  which rejected the workflow umask 077. Set umask 022 only in the SDK configuration child
  through the existing bounded command helper; retain restrictive parent/secret handling.
- Added an entry-count regression proving omitted links cannot bypass the archive limit.
- Both OpenWrt GitHub runs now confirm safe extraction under Python 3.12; both stopped
  at the SDK umask check, corrected in local commit 5bddcd7. Worker code/source pins
  are deployed at 929f140d890fff398772949d24e30b60a18f5969; activation waits for already
  assigned native publications to drain, preserving their immutable tooling identity.
- RPM publication run 40229989203 succeeded; core-ci verified the feed result and restored
  the node as published. Diagnostic SDK archives/extractions were removed; small checksum
  and result records remain under /tmp/codex/build/core-ci/sdk-extraction.
- Owner noticed the feed status pages do not reflect publication. Verified that
  status/demo-core.md still dates from October 23; the new publisher does not update
  those existing files. Packages, signed indexes and receipts
  are live in new feed commits. Public status-page integration remains a separate missing
  presentation feature, not evidence that the package writes failed.
- After prior native publications were verified, activated worker 929f140 and requested
  fresh unchanged-source selections. OpenWrt runs 40230575376 (IPK) and 40230566129 (APK)
  passed extraction and defconfig, including the proper-umask check, then exposed a compiler
  lookup error: TARGET_CROSS is a command prefix, not a path relative to the worker CWD.
- Resolve that prefix using the SDK's own TOOLCHAIN_DIR and keep the compiler containment
  check. Added a real-make preparation fixture checking configuration ordering, child-only
  umask, SDK compiler identity and staging path without compiling any packages. Three
  affected suites pass. This fixture is local boundary proof, not an actual SDK/live build.
- Deployed compiler lookup correction at worker 5d124fbdec05cfdfe2ac4998847c51325f6c7a98.
  Build activation need not wait for publication activation: retained the old publisher pin
  while its assigned job finished, then activated the matching new publisher pin. No
  controller scheduler/flag was added. A selection rejected for concurrent startup graph
  changes created no new work; request again against the restored stable graph.
- Both final worker SDK preparations passed on GitHub: IPK 40231441419 and APK
  40231443502. Configuration, target checks, toolchain identity and APK v3 inspection
  are now exercised against actual SDKs. The APK build then reached the source-owned
  hook and failed to locate its basename-only cross compiler. Resolve hook compiler
  and archiver commands from that SDK's TOOLCHAIN_DIR too; no worker/controller state added.
- A hook boundary regression reaches a compiler outside PATH through real make queries,
  stopping deliberately before compilation. Existing eight local DEB/RPM fixture builds
  also pass. Deploy source hook changes through new commits and a new sandbox source tag;
  preserve v2.0.0 and its recorded failing inputs.
- Final source tag v2.1.0 selects 1cfcfa17de500bb68f389bf6b8c4cbf27ddcbb57 with worker
  5d124fbdec05cfdfe2ac4998847c51325f6c7a98. Actual OpenWrt IPK run 40231880390 and APK
  run 40231884159 both passed source compilation, packaging, independent validation and
  artifact upload. All three v2.1.0 native builds also passed. OpenWrt signing/publication
  and client installation are not yet claimed. Applied actual GitHub results through
  controller reconciliation; retained every previous failed execution and immutable input.
- Changed production code +14/-9 (net +5 within owner-approved growth), tests +129/-1;
  documentation accounted separately. No controller source changes or additional checkout.
  The application remains running locally with its managed database; no application push,
  PR, installation or public UI exposure. Sandbox tooling/source deployment is live.


## 2026-10-25 — continue trusted publication and client acceptance

- Live publication stalled on feed proof because anonymous GitHub reads exhausted their
  60/hour quota. The completed publisher receipt is present; retain assigned uncertainty.
- Live scope test proved that a contents:read token restricted to the existing worker
  installation grant can read the public feed with the 5,000/hour installation quota.
  Revoked that temporary token. No feed access grant, write privilege or credential added.
- Route public reads through the existing authenticated request authority, replacing its
  anonymous forwarding branch. Production +7/-9; 16 affected local transport, source,
  enrollment and pipeline tests pass, including public-read authentication and rejection
  of writes outside the explicit repository grants. No token cache or retry mechanism.
- Added 200 lines of GitHub-only acceptance test code/workflow, without production
  runtime growth. It uses checksum-verified matching OpenWrt rootfs images, scoped public
  inputs and repository-owned public keys, with no publication environment or secrets.
  Checks reject unrelated keys, refresh/install the actual HTTPS feed, and verify the
  installed executable output. Local syntax/permission/no-secret checks pass; live
  installation proof remains pending publication.
- Remove redundant GitHub build inspection during reconciliation once its immutable
  verified result is persisted. Publication eligibility already derives from that result;
  unknown builds and assigned publication outcomes are still inspected. Production +1/-1.
- Expanded the real-MariaDB/synthetic-GitHub pipeline boundary to persist a completed
  second build behind an uncertain publisher, restart, and assert zero repeat build-status
  requests plus exactly one subsequent publisher. The previous running binary fails this
  regression by re-reading the cached build; the rebuilt binary and graph suite pass.
- Authenticated live startup reconciliation verified the formerly blocked feed receipt
  and advanced publication to the next assigned job. No replacement executions dispatched.
- Tightened the public-read boundary: only explicit feed reads may use the existing
  read-only installation token outside repository grants. Ordinary source reads and all
  writes still require enrollment grants. Consolidated SourceReader's forwarding branch;
  production +9/-11, tests +8/-7. All 16 affected boundary suites pass after rebuilding.
- Added native GitHub client acceptance alongside the OpenWrt check. The first runs
  exposed test-harness permissions needed by APT and DNF's zero exit after rejecting a
  repository; fixed the harness to permit package-manager ownership/identity operations
  and require an actual rejected installation. No production behavior or secret access added.
- Real Fedora run 40234717535 and Debian amd64 run 40234719240 pass wrong-key rejection,
  signed HTTPS refresh, exact-version package installation and installed runtime output.
  Sandbox acceptance tooling deployed at 2fedc3498c566a3ad3101f6511a18274c2365edc;
  production build/publication tooling stays pinned at 5d124fbdec05cfdfe2ac4998847c51325f6c7a98.
- Created new v2.1.0 tags on the reviewed sandbox app/side/join source commits for the
  dependency diamond. Existing version tags and failed execution inputs remain unchanged.
- OpenWrt IPK trusted publisher 40234419872 succeeded, with the committed receipt
  available over HTTPS and verified by the running controller. Dependent demo-app IPK
  work was then assigned using that exact published prerequisite.
- Imported OpenWrt rootfs client checks require boot-created /tmp and lock directories;
  added only test setup. Real opkg run 40234883261 passes wrong-key signature rejection,
  trusted usign verification, HTTPS installation of 2.1.0-r24 and runtime demo-core 60.
- Debian ARM64 real client run 40235051112 also passes wrong-key rejection, signed
  refresh, exact 2.1.0-22 installation and executable output on an ARM GitHub runner.
- Cancelled only the assigned sandbox demo-side IPK run 40234976218 to test real failure,
  join blocking and explicit retry. GitHub confirms cancellation; graph reconciliation and
  retry proof remain pending. No injected result or database record is used.
- Completed live enrollment of all four repositories and selected five join executions.
  Chrome UI confirms join work waits for both branches, displays immutable source/config
  revisions and real GitHub links. Concurrent configuration reads were correctly rejected
  when graph generations changed; a fresh read with no competing graph traversal succeeded.
- UI invariant: direct prerequisites are siblings, not a dependency chain. Replace the
  repository summary's arrow separators with commas; one line changed, no state added.
- Explicit API retry of the cancelled side IPK work created rebuild revision 41 and real
  GitHub run 40235443413, retaining the cancelled revision 34 in history. Join work remains
  waiting. Restarted the development controller during assigned build/publication work;
  all execution IDs and rebuild revisions compare equal before/after restoration.
- Regenerated embedded UI assets through the existing CMake build. Node syntax check,
  HTTP-served JavaScript and Chrome UI verify the comma-separated direct prerequisites.
  Runtime remains a temporary development build, with no application installation/push.
- Updated stale setup/readme statements to distinguish the now-active sandbox and proved
  native/IPK client installation from outstanding APK, diamond and webhook acceptance.
  No operational-readiness claim, new credential grant or controller mechanism was added.
- Added GitHub-only checks of the existing version codec against actual OpenWrt client
  comparators, covering numeric/alphabetic/multipart prereleases and rebuild ordering.
  Official opkg/apk command documentation and shell syntax were checked; no new codec.
- IPK run 40235763176 passes ordering comparisons plus wrong-key rejection, signed refresh,
  exact installation and runtime. A preceding new comparator run stopped because its
  client setup ran after the comparisons; moved volatile-directory setup before all commands.
- Saved live diamond evidence with demo-app amd64 published, demo-side amd64 unpublished
  and demo-join amd64 waiting without an assigned run. ARM64 join became eligible after
  both prerequisite publications. No injected graph state or synthetic GitHub result.
- Reviewed SNode.C TLS WebApp and MQTTSuite named listener/disabled-instance patterns for
  the remaining dedicated webhook listener. Production implementation awaits required
  growth approval; valid public TLS certificate/key paths remain owner inputs.
- Retry publication 40236058413 succeeded and the controller marked side IPK rebuild 41
  published. Read-only SQL proves attempts 34/failed/cancelled and 41/published both remain.
  GitHub run history contains exactly those two build attempts, with no duplicate dispatch.
- All three native join targets became eligible only after both branches' verified
  publications. Real ARM64 join build 40235752993 and RPM join build 40236060513 pass;
  amd64 join run 40236507555 is assigned. IPK/APK join work still waits for prerequisites.

## 2026-10-25 — approved dedicated TLS webhook listener

- Owner approved the remaining TLS listener. Invariant: public webhook routing must not
  expose local configuration administration. Move the sole existing HMAC handler to a
  named SNode.C TLS WebApp, disabled by default, loopback:8443, using installed public
  package targets. Register configuration before the existing single init; listen only
  after database/schema/UI startup success. No new lifecycle state, timers or schedulers.
- TLS errors are logged without releasing resources still owned by the local UI. Examined
  framework acceptor callbacks and MQTTSuite listener patterns. Initial database/schema
  failures still release startup resources and leave both configured ports closed.
- All ten affected local lifecycle, enrollment and real-MariaDB/synthetic-GitHub pipeline
  checks pass. HTTPS HMAC reports, untrusted certificate rejection, endpoint route isolation,
  help configuration/body limit and TLS bind failure are covered. Help assertions follow
  native disabled-instance/resource-policy command forms; no custom help code added.
- Production +24/-3 (net +21), tests +55/-7 (net +48), within the approved estimates;
  documentation counted separately. Formatting/build checks pass. Public activation awaits
  valid owner-supplied TLS certificate/key paths; no replacement certificate was generated.
- Real APT diamond client run 40236964119 passes signed installation of core/app/side/join
  and runtime `demo-join core 60, side 62`. Native diamond publication/installation is now
  proved on amd64; further target publications and APK client acceptance continue.
- Complete IPK diamond publication passed, including join publisher 40237059539.
  Read-only graph proof confirms all four receipts share SDK SHA-256
  ac4a0405d2eea821b06f93c14ba13ffa90ad0457648903df7dde02570027ab21.
  Real opkg diamond run 40237394589 verifies version ordering, wrong-key rejection,
  signed refresh, package/dependency installation and `demo-join core 60, side 62`.
- Added one test-only APK grammar check before the existing APK comparator assertions,
  using standard `apk version -c`. No production version codec change. Acceptance-only
  worker deployment is 190e97e5e491e45cfd38f5bee9e17b1dfeb3ca5a; build tooling stays pinned.
- Extended the existing real-MariaDB lost-connection check to assert exactly one TLS
  listener as well as one HTTP listener after lazy reconnection; the focused test passes.
  This adds one test line and no production state or connection retry behavior.
- Native diamond client installation/runtime also passes on ARM64 APT (40237555269)
  and Fedora DNF (40237553823), in addition to amd64 APT and OpenWrt opkg. All use actual
  signed HTTPS feeds and reject unrelated keys first. Controller assigned the root APK
  publisher 40237512801 after the preceding feed writers completed.
- Root APK publisher 40237512801 passed EC P-256 package/index signing and checked
  Git/HTTPS publication. Actual OpenWrt 25.12.6 apk-tools 3.0.5 client run 40237801462
  rejects an unrelated key, validates version grammar/ordering, refreshes packages.adb,
  installs exact 2.1.0-r25 and runs demo-core 60. All four formats now have actual signed
  package-manager installation/runtime proof. APK dependent builds continue via the graph.
- Corrected an obsolete synthetic-test docstring mentioning OCI; the fixture covers only
  GitHub/feed HTTPS. No controller registry or SDK implementation exists or was added.

- Final APK diamond publisher 40238830522 and actual client 40239260480 pass. The client
  rejects an unrelated key, installs all four signed packages with apk-tools 3.0.5 and
  executes `demo-join core 60, side 62`. All four APK receipts share the pinned SDK hash.
- Real persisted graph now reports all 20 v2.1.0 executions published. Explicit idle
  reconciliation assigns zero work and leaves all revisions/states/run identities unchanged.
  The bounded external acceptance observer has finished; no application scheduler was added.
- Recorded all 20 build/publication links in deployment documentation. Public webhook
  certificate/key paths and feed status renderer approval remain outstanding. No private
  material, application installation/push, or public webhook activation was performed.

## 2026-10-25 — approved receipt-derived feed status

- Owner approved feed rendering and public TLS activation. Actual certificate/key paths
  still must be supplied; approval alone does not provide usable TLS files.
- Invariant: feed presentation derives exclusively from committed publication receipts;
  it must not invent execution state or become a graph/configuration authority. Existing
  publisher had no presentation path to reshape. Added one renderer (105 production lines
  including its publisher integration), within the approved 80–120 line estimate.
- Render retained versions, package downloads and assigned build/publication/receipt links
  into HTML, Markdown and JSON. Invoke the same renderer before the publisher's existing
  checked feed commit; no new scheduler, callback or signing implementation.
- Added 67 test lines for retained versions, escaped content, real HTML links, deterministic
  replay, unchanged package bytes and rejection of unsafe paths/execution URLs.

- Both focused worker package/provenance tests pass. Black checks and diff whitespace checks
  pass. Sandbox worker tooling is 792949f2331fc7da76a70b1c5422ef55456c3ac9; all four source
  workflows and public configuration pins updated after verifying no assigned work remains.
- Initial presentation-only feed update a3a4427407583d440920af03b9342df5da1ec2e5 derives
  29 publication rows. Every package/index/key/receipt and unrelated document blob is unchanged.
  Pages run 40241209262 passed; verified HTTPS outputs and actual Chrome rendering.
- Restarted the development controller with updated pins; all persisted execution identities
  and states are unchanged. No application installation/push, new tag, key change or package
  rebuild. Future publisher integration is locally verified, not claimed as a new live build.


## 2026-10-26 — owner-requested full Claude workflow review

- Sent the installed Claude CLI all nine active deployed workflows, all 14 worker modules,
  controller code, source-owned hooks/JSON and current acceptance evidence. Confirmed worker
  modules match deployed tooling. Included live Pages/status URLs, 20 published diamond
  executions, real client proof and explicit remaining webhook/renderer acceptance gaps.
- Review used text input only: tools/MCP/browser access/session persistence disabled, no
  private key/token values supplied. No application or deployment changes authorized by review.
- Saved advisory verdict "Request changes" in docs/claude-workflow-review.md. Claude emitted
  textual tool invocations; none ran. Its search/line-verification claims are unsupported.
  Our subsequent source reads confirm the structural concerns F1–F4, not all proposed fixes
  or live scenarios. Remaining findings need triage; no implementation changes made.
- Production/test changes: zero. Documentation only; no package build, dispatch, publication,
  public webhook activation or application installation performed during review.


## 2026-10-26 — owner-requested Claude implementation prompt

- Asked Claude for a copy-ready fix prompt using its review, relevant current source,
  owner policies and live-result limits. Tools/MCP/browser access disabled; no secrets sent.
- Requested a revision to remove invented F3 omission policy, distinguish new-publication
  allowed changes from status-only refresh, preserve existing sandbox authorization, and
  keep rerun-process authorization separate from reading attempt-one evidence.
- Saved Claude's revised output verbatim in docs/claude-fix-prompt.md. It remains advisory:
  findings and proposed fixes need validation, and AGENTS approval rules remain binding.
- Prompt generation only. No production/test changes, deployment, dispatch, publication or
  application installation. Original request/outputs remain under the permitted temp root.


## 2026-10-26 — workflow review corrections, focused verification

- Working tree began clean on feat/persistent-package-graph at d300a4c; only the requested
  prompt documentation followed ec29760. No branch switch, reset or new checkout.
- F1 invariant: controller reads assigned attempt-one evidence even after a UI rerun;
  a rerun process itself must never gain authority. Use attempt-specific run/jobs endpoints
  in controller/provenance/environment readers. Reject reruns before environment discovery;
  existing native build and protected publisher guards remain. Consolidated existing run
  validation; production lines neutral. No extra state, scheduler or replay policy.
- Official references: https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run-attempt
  and https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt
  document immutable-attempt readers; ordinary jobs defaults to latest. Job identity remains
  explicitly checked, and artifacts remain bound by assigned ID/digest/job evidence.
  https://docs.github.com/en/actions/reference/workflows-and-actions/variables documents
  GITHUB_RUN_ATTEMPT. Dispatch inputs currently allow 25 properties / 65,535 characters:
  https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#workflow_dispatch
  Empty job lists are not documented proof that no push occurred; F9 remains uncertain.
- Studied SNode.C testmariadb/verysimpleserver and MQTTSuite named listeners; no lifecycle
  changes. MariaDBClient destruction detaches its separately owned connection via unmanaged();
  callbacks remain owned by the connection/command. F12 is not established by the review.
- Focused pipeline, worker-environment and worker-provenance checks pass. Updated fixtures
  expose contradictory latest attempt-two evidence and require attempt-one queries. No full
  suite run. Two initial test-fixture issues (an HTML link substitution and missing runner
  attempt in a second environment fixture) were corrected; both affected tests then passed.

- Owner approved bounded fixes: 70–110 net production lines and 180–260 test lines,
  one graph authority, no new persisted state/scheduler. Owner explicitly chose F3 blocking:
  report an unreadable selected repository, preserve the whole new selection request;
  never exclude unknown dependency definitions. Existing readSources behavior implements it.
- F2: preparation now renders status, verifies expected staged bytes and commits locally.
  The existing push step only obtains its writer token, performs the checked push and verifies
  HTTPS. No second render or new staging manifest. Preparation failure leaves push skipped;
  errors inside push still preserve uncertainty, including token/connection/file failures.
  Reduced production by seven lines. Focused provenance test proves render failure cannot
  reach writer token/commit/push and cleans temporary material; real local Git proves normal
  update, byte-identical replay, ignored-file rejection and stale-push rejection.

- F4: the graph now constructs dispatch inputs once, shared with the REST dispatcher.
  Eligibility checks conservative serialized UTF-8 byte bounds (60,000 assignment; 65,535
  entire inputs object), including publication build_result and JSON escaping. Blocked nodes
  retain their state and expose reasons through existing graph/UI and advancement responses.
  No writer slot or rebuild revision is allocated for blocked work. No persisted size flag.
- Focused graph-state and real-MariaDB/synthetic-GitHub pipeline tests pass. Cases cover a
  raw assignment below 60,000 whose escaped payload exceeds 65,535, oversized publication,
  visible HTTP blockers, and independent build/publication dispatch while blocked. The first
  HTTP fixture attempted to change an already pinned run URL and was correctly rejected;
  corrected it to assign its oversized synthetic evidence before binding. No weakened check.

- F5 verified: a source-controlled image's validator stdout could replace trusted environment
  evidence. Host now compares the returned environment and all existing assignment evidence
  before writing a manifest. Publisher remains the package metadata/signing trust boundary.
  Focused worker-environment test exercises the host wrapper with synthetic container results:
  honest evidence accepted, altered environment/source rejected. No local Docker build executed.

- F6/F11: moved source-workflow rendering/blob validation into the existing deployment
  configuration authority, shared by enrollment preview and every selection. No extra HTTP
  request. Search ignores copied titles from actors other than the pinned controller bot.
  HTTP pipeline tests prove configuration mismatch rejection and forged-title isolation.
- F10 reproduced before fixing: two independent failed-run reports overlap while the isolated
  test DB's generation row is briefly locked; one returns a definite conditional rollback.
  After that specific known rollback and an advanced generation, revalidate once against the
  current graph. No retry after uncertain outcomes; no recursion/loop/new queue. The original
  generation/state guard and immutability validation still apply. Same focused test now records
  both failures successfully. Residual second conflicts remain visible, never blindly replayed.
- Focused pipeline and graph-state tests pass. Production growth remains below the approved
  110-line cap; test growth is 259 lines, within the approved 260-line cap. No full suite run.

- F13 verified README collision: retain existing ordinary repository page paths; a repository
  literally named README gets status/README/README.md, separate from the aggregate page.
  Existing renderer HTML/JSON/retained-version test now verifies both pages remain intact.
  Lexical all-version ordering is presentation only; no new version codec or sorting policy.

- F13 least-privilege correction: publication workflow exposes the optional OpenPGP passphrase
  only for DEB/RPM preparation. Publisher writes it only for that phase/format. No secret
  values inspected or changed. Workflow YAML parses; focused provenance preparation-failure
  and temporary-material cleanup tests pass. SDK checksums currently trust the configured
  HTTPS metadata origin; signed checksum-list verification was not added or claimed.

- F7/F8/F9 deployment triage: observer secret's exact token type/scope remains unknown and
  no value was queried. Authorized feed metadata shows no rulesets and unprotected main;
  no access controls changed. Empty jobs/absent receipts remain insufficient pre-push proof.
- F14: corrected active secret names, implemented feed/receipt/export paths and obsolete
  SDK/client acceptance wording. Documented hosted-runner trust limits, HTTPS checksum trust,
  compatible client dependency resolution and static-export runtime evidence. Added per-finding
  classification and remaining gates in docs/workflow-review-fixes.md. Documentation only.
- Relevant format-check target and targeted Python formatting checks pass. Final correction
  delta is 61 net production lines, 259 test lines, below approved caps. No full suite run.
- Deployed reviewed worker d210b36e46b4d5041828bf7b32f67d5638570142 and source workflow
  pins in VOC-Experiments only. Publication blob 6582e15feea5f4d3785b4dfc0a157bc955175c65;
  source blob c6f037a87eda6991586d6157dda7b341c7d0370c. Protected public config values updated.
  Restarted development controller; previous 40 execution IDs/revisions/states unchanged.
- Created new demo-join v2.1.1 at f542f5e594edc66932519714a01a57a012fe9f69 for focused
  integrated publication. Five targets assigned revisions 42–46, consuming existing verified
  prerequisite receipts. Existing tags/keys/packages unchanged. Initial HTTP request response
  expired while asynchronous work continued; assignment inspected, never resubmitted blindly.

- Live new builds passed all five selected targets. AMD64 40296998946, ARM64 40297006247,
  RPM 40297014470, IPK 40297002536 and APK 40297010660 (demo-join Actions).
- Real ARM64 publication 40297258519 and IPK publication 40297837480 pass with the new
  preparation/rendering path; controller/HTTPS receipts verified. New ARM64 apt client
  40298188386 rejects the wrong key, installs demo-join 2.1.1-43 and runs the expected program.
- Deliberate REST-initiated GitHub reruns: source AMD64 latest attempt2 fails before environment
  resolution, attempt1 remains success. Publisher ARM64 latest attempt2 fails validation and
  privileged publication is skipped; attempt1 remains success and graph remains published.
- Live source full-rerun removed the original AMD64 environment/package artifacts (run-scoped
  listing now empty). This prevents publication despite preserved attempt1 success. No
  replacement dispatched. Requested owner policy for authoritative missing/expired artifacts
  versus inaccessible evidence, plus a test-cap increase to 280; independent rows continue.

- Owner approved missing/expired artifact failure and the bounded regression extension.
  Reuse existing graph failure transition only after verified attempt-one success and a
  complete, bounded artifact listing. Denied/incomplete listings preserve assignment.
  Production delta for this change: +6 net lines; focused HTTP regression +17 net lines.
- Relevant pipeline-boundaries passes (7.34 s), C++ build and format-check pass. No full
  suite run. Restart preserved every existing execution/state before reconciliation.
  Live reconciliation then marked AMD64 v2.1.1 failed: required artifacts missing/expired.
  No replacement dispatched; ARM64, RPM, IPK and APK remain published.
- APK 40298324889 and RPM 40298857335 publication pass. Four new HTTPS receipts and
  controller published states agree; feed now retains 33 rows. Tree audit preserves all
  299 prior immutable package/key/receipt/export blobs and unrelated paths. Only assigned
  additions, affected indexes and deterministic status views changed. Updated deployment
  evidence distinguishes these live checks from synthetic local tests.
- Final correction accounting from d300a4c: production +188/-121 (net +67), tests
  +300/-24 (net +276), within approved production and extended regression bounds.

- Owner requested full sandbox deployment. Pushed feat/persistent-package-graph through
  d7e43fc; audited every worker source blob and all four source workflow pins against the
  reviewed local sources: exact matches, no redundant GitHub deployment commits needed.
  CORE_CI_TOOLING_SHA matches and package-publication remains restricted to main.
- Running updated controller PID 1264161, binary SHA256
  73847c5885cb202c1ae6a412afcddd0804549fd58a267f6f7ef58477eb81738b.
  Real MariaDB restored 45 executions; mutations allowed, public feed status reachable.
  Local UI stays on 127.0.0.1:8080; public webhook listener is disabled pending actual
  certificate-chain/key paths. Asked owner for those paths; no certificate fabricated,
  UI exposed, port 443 changed, or production SNodeC resource activated. No full test rerun.

- Owner issued gw-home.vchrist.at certificate on OpenWrt using IONOS DNS validation.
  Secure SSH copied fullchain and private key to /home/voc/ci-test/core-ci-tls/ (0600),
  verified matching public keys and hostname. No key contents printed or committed.
- Restarted controller with framework command-line webhook TLS options, LAN
  192.168.1.239:8443. Existing 45 graph executions/states preserved; verified public
  HTTPS GET reaches listener (404 for GET on POST-only webhook route), without bypassing
  certificate verification. UI remains loopback-only and port 443 unchanged.
- GitHub App metadata reports no subscribed events; webhook configuration API returns404.
  Opened sandbox App settings in Chrome; GitHub requires owner sudo authentication.
  Event subscription/activation and real delivery remain pending that authentication.
  Certificate renewal-to-controller deployment automation remains to be configured.

- Authenticated GitHub settings confirm webhook inactive, URL empty and no subscriptions.
  No HMAC secret was configured in the deployment master settings; generated a new 32-byte
  random HMAC secret in /home/voc/ci-test/core-ci-webhook.secret (0600). Configured controller
  through --webhook-secret-file and restarted, preserving graph. No secret contents logged.
- Automatic browser approval rejected enabling a blank-secret hook. Kept delivery disabled;
  the official signed configuration PATCH returns404 until a hook exists. Prepared the URL
  and handed off secret entry to owner; do not enable an unauthenticated webhook. Chrome
  tab marked handoff to remain open. Event subscription and delivery proof still pending.

- Owner completed signed-hook setup. Verified active hook, configured secret, SSL
  verification enabled and subscriptions push/workflow_job/workflow_run. Real GitHub
  signed ping and ping redelivery return200; unsigned HTTPS POST returns401; UI path404.
- Created sandbox demo-join v2.1.2 at existing f542f5e... (no existing tag changed).
  Actual push delivery selected five targets and dispatched all five builds without a
  manual reconcile call. GitHub timed out waiting for slow processing; work continued.
  All five builds succeeded. First publisher 40311405971 assigned automatically.
- Owner explicitly approved bounded prompt-acknowledgement fix: at most20 net production
  and30 test lines, existing callbacks/graph, no queue/state/scheduler. Invariant: validate
  signature, event identity and assignment/tag shape before acknowledgement; later API
  verification still governs graph transitions. Log asynchronous processing results.
  Acknowledgement is not persistence of a new tag: crash-before-selection needs explicit
  rescan/redelivery, as accepted by owner. No automatic replay of uncertain mutations.
- Fix uses +8 net production and +23 test lines. Relevant pipeline-boundaries passes
  (9.84s), including a deliberately delayed GitHub response and prompt HTTPS acknowledgement,
  final graph transitions, invalid signature/repository rejection and asynchronous failures.
  Build/format-check pass. Restart during assigned publisher preserved all50 executions,
  states and assigned runs. Actual completed-build redelivery returns202 in2.43s; verified
  processing continues through the existing callbacks. No full suite run.
- SNode.C client traffic also reveals repeated Double disable warnings and occasional
  transport shutdown errors. These are recorded observations, not diagnosed framework
  defects, and no framework changes or deferred client refactoring were introduced.
  GitHub webhook response deadline reference:
  https://docs.github.com/en/webhooks/using-webhooks/best-practices-for-using-webhooks
- Webhook-driven v2.1.2 verification finished: all five builds and publishers succeeded;
  controller verifies all five published and public status retains38 receipts. No manual
  reconcile call drove progression. Real tag/report redeliveries and restart during assigned
  publication were included, as disclosed. Accepted notification persistence window unchanged.
- Owner approved readable labels across GitHub/UI:30–60 net production and40–70 test lines,
  no new graph authority/state/scheduler. Invariant: human labels derive from immutable
  repository/tag/target/rebuild inputs; cryptographic assignment/run/artifact checks remain.
  Existing pinned historical executions retain their expected names; no history is rewritten.
- Implemented readable labels from Graph::label: repository/tag, distribution/suite,
  architecture, package format and rebuild revision. New immutable assignments carry that
  derived label to GitHub run/job names; controller and worker verify those exact names.
  Existing assignments lack this field and retain their original pinned name verification;
  retry environment references retain the earlier label. No old assignment/history rewritten.
- UI derives display from the graph label, puts active work first, names prerequisite links
  and collapses full source/config/execution hashes under technical details. Repository
  revisions and operation responses remain available; failures still expand their details.
- Relevant graph/pipeline/environment/provenance checks pass; final pipeline8.58s. Added
  rejection of changed labels with both unchanged and recomputed assignment digest.
  Node syntax and committed format-check pass. No broad suite run. Net production growth
  is50 lines, within owner-approved60; tests remain below approved70. UI regenerated by
  final build. Deployment waits until all old assigned publishers finish (now verified).
- Readable-name live verification: demo-join v2.1.3 completed all five builds/publications
  through signed webhook notifications, without manual reconcile calls. Controller verifies
  all five published; public status contains43 retained receipts. Run links are recorded in
  deployment/sandbox/README.md. Restart preserves graph states and assigned run identities.
- Corrected prerequisite link presentation to show immutable repository/tag instead of the
  prerequisite node's latest rebuild label: that latest attempt need not be the consumed
  receipt. One production line replaced, zero net growth; no state or behavior extension.
  JavaScript syntax, browser display, build and format-check pass; no broad suite run.
- Replaced proposal/advisory documentation with current-snapshot READMEs: architecture and
  algorithms, project/worker/controller extensions, and full deployment/operations. Added
  a glossary, reading paths, contents, source references and ten Mermaid diagrams, including
  seven sequences. Removed superseded narrative documents rather than creating an attic.
- Deployment guide covers both MariaDB ownership modes, App permissions/installations,
  exact configuration/secret inventories, file-input provisioning, workflow pins, signing,
  feed/client trust, local UI/public TLS, renewal, upgrades and backup/restore. Current sandbox
  evidence and unresolved operational limits remain explicitly separate from general setup.
- Checked documentation against current controller/worker/schema/configuration and SNode.C
  database/demo and MQTTSuite lifecycle/callback patterns. Official GitHub App, secret and
  Pages documentation and Let's Encrypt challenge documentation are linked in the guide.
  Local links, fenced JSON, shell syntax and Mermaid11.12 parsing pass; the complete source
  definition example passes the actual Definition::parse validator. No package suite rerun
  for documentation; existing build/format and UI syntax/browser checks cover the one-line
  presentation correction. Documentation changes add no production/test code or dependencies;
  Mermaid validation tools are isolated under /tmp/codex/build/core-ci/documentation.
- Owner authorized merging to main. Verified clean canonical checkout, main ancestry and
  absence of conflicting worktrees/open PRs. Final source has no registered GitHub checks;
  relevant local checks and five-target live publication evidence are recorded above.
  Sandbox UI/graph remains available with all five v2.1.3 targets published. Fast-forward
  main preserves reviewed checkpoints; no new production/test code or production activation.

## 2026-10-26 — package status and management UX proposal

- Owner requested SNodeC/Packages status-page inspection and a management-UI proposal/mockup.
  Read only public status index/project pages and their presentation/data requirements. No
  production repository or feed was changed and no orchestration implementation was reused.
- Proposed trusted Jinja templates replacing hard-coded rendering, one normalized public
  context with owner-defined presentation parameters and provenance-backed extension fields.
  Full in-build/reference-page parity requires a separately approved read-only graph projection
  and serialized trusted status-update path; receipts alone cannot supply live work state.
- Created a desktop-first interactive design preview: full-width work table/detail layout and
  alternative inspector, separate repository editing, compact typography and expandable IDs.
  All preview states are illustrative; actions perform no live API or GitHub mutation.
- Verified JavaScript syntax, filter/selection/failure actions, repository navigation and
  variant switching in Chrome; checked1024px layout and320/360px contained table overflow.
  Jinja example renders recorded/absent values. Only relevant preview checks, no application
  suite rerun. Production/test changes zero; proposal/mockup are uncommitted design artifacts
  on docs/status-and-ui-proposal. Local preview at127.0.0.1:8879 is retained for owner review.

## 2026-10-26 — accepted desktop UI and SNode.C SSE

- Owner accepted the management UI and instructed implementation with SSE and removal of
  Refresh view. Combined estimate220–300 net production/70–90 test lines; actual changes
  are+221 production (404 added/183 removed) and+66 tests (68 added/2 removed), under bounds.
- Invariant: one committed graph authority supplies the browser; reconnect sends current
  views without replay state, timers, polling GitHub or rewriting unsaved drafts. Used native
  SNode.C sendHeader/sendFragment and MQTTSuite disconnect/receiver patterns. GraphStore's
  write-completion notification exposes only applied state and the existing uncertainty guard.
  Added immutable target_row to the existing view; no schema/state/scheduler extension.
- Implemented full-width work table, local filters, selected execution/prerequisite details,
  separate repository editor, compact typography, original revision guards, expandable IDs
  and explicit inspection/retry controls. Stream updates retain filters, selection and drafts.
  Write feedback distinguishes known commits, uncertain outcomes and failures; losing a POST
  response cannot claim a definite failed mutation. No automatic mutation retry.
- Relevant real-MariaDB/synthetic-GitHub enrollment and pipeline tests pass (3.57s/8.66s),
  covering two subscribers, committed graph/enrollment delivery, reconnect/restart, explicit
  retry and uncertain database outcome. Corrected an existing enrollment fixture's stale
  latest-run URL to the controller's assigned attempt-one endpoint. Final pipeline7.48s also
  verifies the SSE route remains absent on the public TLS listener. Build, format-check,
  JavaScript syntax and changed-production160-column checks pass. No full suite run.
- Real sandbox controller restarted with all55 execution states/run assignments preserved;
  all five v2.1.3 targets remain published. Chrome verified Live SSE, filtering/editor layout,
  automatic reconnection across controller restart with unsaved title/base revision/filters
  retained, and narrow-screen contained table overflow. Synthetic local browser fault checks
  cover committed-but-controller-failed, rejected/uncertain writes, post-commit view-read
  failure and lost POST response; these are not live GitHub write tests.
- Updated current architecture/deployment documentation. Current sandbox runs the development
  binary under /tmp/codex/build/core-ci/foundation with existing configuration; no system-service
  install, production SNodeC resources, package bytes, keys or workflow/feed activation changed.
  Concurrent worker/template changes belong to other work and are preserved/excluded from
  this checkpoint. Temporary UI fault servers stopped; no additional checkout created.

## 2026-10-26 — GitHub publication templates and recorded build details

- Owner accepted deployment-owned Jinja2 templates and richer public presentation/build
  details, then explicitly kept this thread on GitHub work while another thread owns the
  controller/UI/SSE. GitHub publication-only checkpoint uses the proposed220–360 net
  production/100–170 test bounds: actual338 added/86 removed (+252) production and151
  added/3 removed (+148) tests, including the template bundle. No controller changes.
- Invariant: templates project retained verified publication evidence and explicitly recorded
  build facts; they cannot authorize execution/publication or invent unavailable facts.
  Replaced hard-coded HTML/Markdown assembly with one Jinja context and deployment bundle,
  public parameters, project page names and bounded assets. Added strict missing-variable,
  escaping, path/link/output-size checks; status output cannot overwrite packages/keys/receipts.
- Source-owned outputs may report bounded compiler/test counts, coverage and public report
  links, labelled hook-reported rather than independent test attestation. The trusted validator
  rejects source-supplied job facts, then attaches timing from the already verified assigned
  GitHub job; publication rechecks its run/job/attempt binding. No extra API request. Receipts
  retain source/definition/target/rebuild and compact consumed-prerequisite identities; no
  recursive receipt copy, full graph snapshot, new graph authority, state or scheduler.
- Templates execute after temporary signing files are removed, in a bounded subprocess with
  no inherited GitHub/signing credentials. All rendering remains in prepare before the sole
  push step; push-step uncertainty and explicit retry policies are unchanged. The existing
  feed capacity check now includes rendered outputs/assets. Worker dependencies are pinned.
- Focused local checks pass: tests/worker.py (real DEB/RPM build/sign/index/replay and hostile
  artifacts/signatures), tests/worker_provenance.py (synthetic assigned GitHub evidence,
  summary/timing/context, strict rendering failures, collisions, links/traversal, clean child
  environment, pre-push failure and real local checked Git pushes), tests/openwrt.py (actual
  IPK controls and SDK archive/checksum boundaries, no SDK/APK execution). Black check on
  nine changed Python files, AST/YAML/JSON/shell syntax,160-column production and diff checks
  pass. No unrelated C++/UI suite run; CMake format-check covers unchanged C++/CMake only.
- Locally rendered five target coordinates from real public sandbox receipts; receipt bytes
  stayed unchanged. This is read-only public-data rendering, not new live pipeline acceptance.
  Missing publication timestamps and current work remain null; build duration is whole assigned
  job elapsed time. Minimal controller projection/verification timestamps remain coordinated
  follow-up work, not a claimed full SNode.C status-page data reproduction.
- Added worker/status-templates/README.md with context, configuration, hook summary and trust
  boundaries. No commit/push/workflow activation, controller restart or installation by this
  thread; shared checkout remains on the UI thread's feat/desktop-management-sse branch.
  Review patch/logs retained in /tmp/codex/build/core-ci/status-templates until integration.
  No production SNodeC resources, signing keys, existing tags or published package bytes
  changed. .qtcreator/ and .local-credentials/ preserved without inspection.
- Owner subsequently authorized commit and merge. This GitHub-only checkpoint is committed
  independently on feat/publication-status-templates based on main and merged by fast-forward;
  the shared UI branch, its HEAD and its index remain unchanged. No sandbox activation is
  part of this merge. The UI thread can integrate main when its own work is ready.

## 2026-10-26 — inline execution details and recorded build timing

- Owner approved chevrons beneath individual build rows, All work by default, local times
  with explicit timezone, elapsed build duration and Not recorded for absent evidence.
  Replace the detached selection panel; preserve the same graph, retry, prerequisites and
  SSE channels. Active work precedes completed work; completed work uses newest rebuild first.
- Trace: assigned attempt-1 package job -> existing immutable build result -> MariaDB ->
  GraphStore view/SSE -> row details. Optional timing is presentation evidence, not a changed
  run identity or state. No new requests, scheduler, persisted state or historical backfill.
  Timing is captured after successful artifact verification; older/failed records can lack it.
- Changes add 23 net production lines and 11 test lines (production +88/-65; tests +11/-0).
  Reduction removed the detached panel and repeated heading; inline expansion and optional
  timing require the small net addition. Combined desktop UI/SSE implementation remains
  within its earlier 220–300 production /70–90 test estimate (244 production,77 test net).
- Focused local pipeline boundaries pass8.72s: real isolated MariaDB, synthetic GitHub/feed,
  exact attempt-1 timing, publication inputs and restart persistence. Fixed the concurrent
  webhook test's premature assertion by waiting for both persisted graph outcomes; it must
  not use another notification's completion log as its own evidence. Build, format-check,
  JavaScript syntax,160-column changed production lines and diff whitespace checks pass.
- Chrome local fixture verified 3min25s duration/local timezone, missing timings, collapse,
  and an SSE update to4min26s with row and technical disclosure preserved. These are synthetic
  UI checks, not new live GitHub jobs. Temporary fixture stopped after verification.
- Restarted the authorized sandbox controller; all55 states/assigned runs preserved,43
  published/12 historical failures and five published v2.1.3 targets unchanged. Chrome verified
  All work, newest rows first and inline prerequisites/logs. No retry/build was triggered.
- Parallel GitHub worker/template changes and their WORK_LOG entries were preserved and
  excluded from this UI checkpoint. No workflow/feed/key/tag or production SNodeC change.
  .qtcreator/ and .local-credentials/ were not inspected. Preview/logs remain under
  /tmp/codex/build/core-ci/ui-proposal; canonical branch is feat/desktop-management-sse.

## 2026-10-26 — external execution logs preserve the management view

- Owner agreed that Build logs and Publication logs should open in separate tabs.
  Use the existing external-link boundary and noopener/noreferrer; add no navigation
  persistence or duplicate UI state. Production +1/-2 (net -1); no test-code changes.
- JavaScript syntax and focused application build pass. Chrome clicked both actual sandbox
  links and verified separate GitHub tabs, the same expanded execution/search and Live SSE.
  Restart preserved all55 execution states and assigned runs. No GitHub jobs were triggered.
  Parallel worker/template edits remain untouched; no unrelated suite was run.

## 2026-10-26 — independent log popup windows

- Owner approved multiple movable/resizable non-modal log windows. Ordinary clicks request
  a fresh _blank popup with1100x800 content dimensions, resize/scroll controls and
  noopener/noreferrer. Preserve native link behavior for Ctrl/Meta/Shift/Alt clicks.
  Browser/window-manager presentation remains browser-controlled; no saved navigation state.
- Production +6/-0; tests +0/-0. This extends the approved desktop UI within its300-line
  combined ceiling. No controller, graph, SSE, workflow or credential change.
- JavaScript syntax, changed-production160-column and diff checks pass; focused application
  build passes. Actual Chrome clicks opened two distinct type=popup windows (1100x834 outer
  bounds) simultaneously. Main-page search remained interactive, selected row stayed expanded
  and SSE remained Live. Closed only our two verification popups; existing user tabs preserved.
- Restart preserved all55 execution states and GitHub assignments. No builds/publications
  triggered. Parallel worker/template source and WORK_LOG changes excluded from this checkpoint.

## 2026-10-26 — structured enrollment and target matrix editor

- Owner approved180–260 net production/80–140 test lines, including dependency controls.
  Replaced comma-separated dependencies and JSON editing with self-excluding checkboxes,
  source-hook inputs, Add/Edit/Duplicate/Remove target actions and a read-only JSON preview.
  Existing dependency order, runner string/label-array types and free distribution/runner
  values are preserved. Native DEB/RPM image/platform and OpenWrt IPK/APK target fields cover
  the current definition contract; SDK resolution stays on GitHub. No new catalogue.
- One definition draft drives preview and controls. Row edits apply explicitly or cancel;
  unsupported imports remain visible with form editing disabled until an explicit fresh draft
  replaces them. Preserve the imported blob/revision guard. Existing server validation and
  GitHub write-feedback/uncertainty policies remain authoritative. SSE updates dependency
  choices without replacing form/row drafts; newly rendered choices respect operation locking.
- Focused enrollment-boundaries passes3.11s with real isolated MariaDB and synthetic GitHub.
  Build, format-check, JavaScript/Python syntax,160-column changed-production and diff checks
  pass. New127-line interactive browser fixture tests the public save payload, not live GitHub.
  Chrome exercised all four formats, exact runner selectors, duplicate IDs, duplicate/remove,
  canceled rows, empty-matrix rejection, rejected/successful writes and retained drafts over SSE.
  Unsupported-import replacement retained the original revision and newly read blob guard;
  that disposable fixture uses a deterministic affirmative native-confirm substitute.
- Actual sandbox controller restarted; all55 execution states/assigned runs remain unchanged.
  Chrome verified existing five-row demo configuration, correct dependency order, no self
  choice, read-only preview, target editing/cancel and Live SSE. No sandbox definition commit,
  build, publication, credential, feed, key or tag change. Production SNodeC untouched.
- Accounting: production +201/-48 (net153), tests +127/-0; deployment documentation +11/-1.
  Below the approved production ceiling. Parallel worker/template files still match15ad8d6
  byte-for-byte; preserve their WORK_LOG insertion and exclude them from this checkpoint.
  Canonical branch feat/desktop-management-sse; no extra source checkout. Verification assets
  remain under /tmp/codex/build/core-ci/ui-proposal; disposable fixture server stopped.

## 2026-10-26 — merge management UI with publication templates

- Owner authorized commit and merge, then explicitly instructed no further tests.
  Preserve the existing verified UI commits and publication-template commit15ad8d6.
  Only WORK_LOG append conflict required resolution; retain both complete records.
- No additional code change or worker activation. Existing working files already match
  the combined source tree. Merge without stashing, discarding files or creating a checkout.
  Main and the UI feature branch retain both histories. No push is included in this action.

## 2026-10-26 — controller publication verification time and summary display

- Owner approved40–70 net production/35–60 test lines for the controller/UI metadata
  integration after the two prior threads merged. Actual production38 added/3 removed
  (net35), tests49 added/1 removed (net48). Existing receipt storage, graph view and SSE
  carry the facts; no new column, state, scheduler, endpoint, client or GitHub request.
- Traced assigned publication -> committed/HTTPS receipt proof -> Graph::published ->
  GraphStore commit/restore -> existing graph/SSE -> expanded UI details. Invariant: only
  the verified published transition records controller-owned publication_verified_utc;
  source/feed claims cannot set it. Duplicate evidence compares unchanged publication
  content, retains the first timestamp, and still rejects conflicting publication evidence.
  Existing unrecorded receipts remain unrecorded. UTC time comes from the controller clock.
- Added Publication verified in local browser time beside existing build timing. Optional
  receipt.summary compiler versions, test counts, coverage and HTTPS report link appear as
  Hook-reported build summary. Missing facts show Not recorded; reported text uses existing
  textContent DOM creation and report links open separately with noopener/noreferrer.
  These facts are display evidence, never test attestation or permission to publish.
- Reused the merged UI directly; no redesign or second metadata/state authority. Added
  recording/rendering because existing fields cannot supply an unrecorded observation or
  show optional summaries. Current-work export/public status-update jobs remain separate;
  this change does not rewrite feed receipts or claim public template timestamp delivery.
- Isolated build: /tmp/codex/build/core-ci/publication-details, Debug, installed SNode.C
  packages. Build core-ci/graph-test, format-check, JavaScript/Python syntax,160-column and
  whitespace checks pass. Relevant ctest graph-state-boundaries/pipeline-boundaries pass
  (0.09s/8.62s), using real isolated MariaDB and synthetic GitHub/feed HTTPS. Added historical
  replay case also passes the focused graph boundary (0.10s). No full/package suite run.
- Chrome disposable fixture verifies recorded/missing facts, local timezone,1min42s build
  duration, literal markup in compiler text (no image/script), coverage0%, secure report
  attributes, no horizontal document overflow and expanded details retained over SSE.
  No live GitHub builds/publications claimed. Preview tab/server stopped after verification.
- Updated architecture and template-boundary documentation. Canonical feature branch:
  feat/publication-verification-details, based on local merged main d847f82. No commit,
  push, merge, sandbox restart or installation performed for this step. Temporary build/logs
  retained for review; no additional checkout. No production SNodeC resources, feed bytes,
  tags, keys or credentials changed; .qtcreator/ and .local-credentials/ left uninspected.

## 2026-10-26 — authorized live metadata deployment

- Owner requested commit, merge and live operation. Controller/UI checkpoint d2acc67 was
  committed, pushed and merged into main, preserving the merged UI history. No active sandbox
  assignments before upgrade:43 published/12 historical failed, graph generation270.
- The sandbox recipe already checks native runtime output but did not emit summaries. Added
  actual compiler --version and one passed native test (one explicitly skipped runtime test
  for cross-SDK builds) to its existing outputs.json. Package contents/build behavior unchanged.
  Combined bounded metadata work is+46 net production/+53 test lines, under70/60 approval.
  Focused sandbox_hooks.py passes eight actual local DEB/RPM package builds over synthetic
  source fixtures; this does not claim GitHub/container/client acceptance.
- Sandbox rollout uses GitHub Git trees based on the current tree and checked normal ref
  updates, preserving unrelated worker/source/feed content. Official semantics verified:
  https://docs.github.com/en/rest/git/trees#create-a-tree and
  https://docs.github.com/en/rest/git/refs#update-a-reference . No uncertain mutation is retried.
  Existing keys and tags are preserved; external activation is VOC-Experiments only.

## 2026-10-26 — early verified execution evidence

- Owner approved the isolated checkout checkouts/live-execution-evidence and revised bound
  of120–180 net production/70–110 test lines, with merge requested. Branch
  feat/live-execution-evidence is based on main58f6a72. Purpose: isolate controller/UI
  changes from the concurrent public-status thread. Remove the worktree after safe merge.
- Invariant: immutable assignments and run identities never change; verified attempt-1
  progress and job timestamps may accumulate before successful artifact/publication proof.
  Delayed observations cannot erase completion or change established facts. Reused the
  existing graph commit/SSE path and selected job for all evidence consumers; no timer,
  queue, scheduler, workflow changes or additional timing authority. Atomic evidence/failure
  persistence avoids a second competing graph write for a definite failed build.
- Added one per-attempt evidence JSON column, restored through asynchronous MariaDB.
  Existing deployments require the documented administrative ALTER before restart; no
  runtime DDL or fabricated historical timestamps. Deployment instructions and field
  contract: docs/extensions/execution-evidence.md. Runtime has not yet been upgraded.
- UI distinguishes assigned, queued, running, verification and waiting-for-publication
  activity. Early job starts appear immediately through SSE; expected timestamps are
  Pending. Publisher timing remains separate from verified publication. Existing five
  states and all exact inputs, uncertainty and retry policy remain unchanged.
- Official evidence contract checked against
  https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt
  and https://docs.github.com/en/webhooks/webhook-events-and-payloads#workflow_job .
  Reviewed SNode.C apps/main.cpp and MQTTSuite MqttModel.cpp streaming/lifetime patterns;
  existing SSE ownership/lifecycle is retained.
- Debug build in /tmp/codex/build/core-ci/live-execution-evidence uses installed public
  SNode.C packages. Build, format-check, node --check ui/app.js,160-column and whitespace
  checks pass. Focused graph-state-boundaries and pipeline-boundaries pass (0.11s/8.33s):
  real isolated MariaDB, synthetic GitHub/feed. Tests cover early/late evidence, attempt2
  rejection, restart, duplicate SSE/generation stability, failed-build timing and immutable
  identities. No full suite or live GitHub acceptance claimed.
- Disposable Chrome fixture verifies queued Pending, running start time, completion and
  duration over SSE while preserving expansion. Screenshot retained in the task build
  directory. Production:+313/-224 (net89); tests:+79/-4 (net75). Production growth is
  below the approved upper bound; consolidating job reads reduced the required addition.
- No sandbox database ALTER, controller restart, installation, push or external resource
  mutation performed in this checkpoint. Concurrent canonical changes are untouched;
  .qtcreator/ and .local-credentials/ were preserved and not inspected.

## 2026-10-26 — SSE idle heartbeat

- Owner requested the heartbeat fix after approving the bounded approach (15–25 production
  lines and20–35 test lines). Reused the approved isolated worktree
  checkouts/live-execution-evidence on fix/sse-heartbeat, based on main330b8d9, to preserve
  the other thread's uncommitted canonical changes. Cleanup follows the authorized merge.
- Invariant: an idle SSE connection receives transport traffic without graph/GitHub work,
  and its timer is cancelled on disconnect. Followed SNode.C apps/main.cpp and MQTTSuite
  mqttbroker/lib/MqttModel.cpp and mqttbridge/lib/SSEDistributor.cpp interval-timer patterns.
  Checked core/Timer.cpp handle cancellation/destruction and HTTP responseStarted():
  EventSource's Accept header enables the framework's SSE read-timeout handling.
- Each stream now sends :keep-alive plus the blank SSE terminator every30 seconds.
  Existing disconnect cleanup owns the timer handle; its callback captures a weak response.
  No event ID, UI event, database write, GitHub read, queue or scheduling policy is added.
- Debug build and format-check pass with installed public SNode.C targets in
  /tmp/codex/build/core-ci/sse-heartbeat. Focused command:
  ctest --test-dir /tmp/codex/build/core-ci/sse-heartbeat -R '^lifecycle-sse-heartbeat$'
  --output-on-failure. Passed in91.09s using real isolated MariaDB and a real HTTP stream:
  three comments arrive over the same idle connection beyond60s, graph generation remains
  unchanged, the stream closes and the controller shuts down normally. No full suite run.
- Production:+12/-1 (net11); tests including CTest wiring:+19/-2 (net17), both below
  approved upper bounds. Architecture documentation updated. This is local runtime proof,
  not browser/proxy or live GitHub acceptance. No sandbox restart or installation performed;
  no credentials, tags, keys or feed bytes touched. .qtcreator/ and .local-credentials/
  remain uninspected and unchanged.


## 2026-10-26 — public graph facts and deployment-owned Jinja status

- Owner requested implementation and continued after the 350–500 net production / 150–230
  test-line estimate. This is the combined controller, worker/workflow and template bound;
  controller code is only the public projection and asynchronous notification integration.
- Invariant: persisted graph facts remain the single execution authority. Templates own
  Available selection, layout and display policy. They receive ordered selected source tags,
  all retained publications and recorded work, including targets without a receipt. No new
  persisted execution state, scheduler, queue, registry/SDK client or version codec.
- Reused GraphStore::view for projection facts and Version::compare for source ordering.
  The public HTTPS route excludes assignments, hook paths and deployment/credential objects.
  Successful commits notify the approved trusted status workflow; failed/no-op commits do not.
  Startup and explicit reconciliation can refresh projections without replaying package work.
- Reused the renderer and restricted feed-token/checked-push helpers. Rendering has no
  inherited credentials; key files exist only during the push phase. Expected-path staging
  prevents a status update from including unrelated package/key edits. Current-generation
  fetches, stale/conflicting generation rejection and shared feed serialization preserve
  publication. Unknown dispatch/push outcomes are logged/failed without automatic retries.
- Verified GitHub concurrency semantics against the official documentation: default pending
  work is replaced, while queue:max retains up to100 pending jobs. Publication and status
  writers share that group; status notifications have a separate coalescing group.
  https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency
- Automatic approval review rejected creating feat/public-status-projection, applying the
  completed workflow-review task's no-switch instruction. No branch switch occurred. Continue
  on docs/live-status-acceptance at58f6a72 and preserve its two existing documentation edits.
- Focused checks use /tmp/codex/build/core-ci/public-status-projection, installed SNode.C
  packages and the committed canonical formatter. No additional source checkout. Local
  pipeline tests use real MariaDB with synthetic GitHub/HTTPS, not live GitHub acceptance.
  The extra UTF-8 log volume exposed a test reader slicing decoded text with byte offsets;
  corrected it to slice bytes before decoding, retaining all existing outcome assertions.
- No commit, push, merge, controller restart or external status-workflow activation yet.
  Production SNodeC resources, existing tags/keys/packages and private directories untouched.

- Final local verification: Debug core-ci/graph-store-test build and format-check pass.
  Focused enrollment-boundaries (2.96s), pipeline-boundaries (9.84s), and worker-provenance
  boundaries (1.47s) pass; the last renderer/link cases pass in1.99s. No unrelated full suite.
  Python AST syntax, YAML parsing,160-column and git diff whitespace checks pass. Canonical
  formatting files/modules remain byte-identical to the shared baseline. No actionlint binary.
- Public-boundary cases cover pre-commit exclusion, committed notification identity, UTF-8
  reports, numeric selected-source ordering, waiting/building/publishing/failed display,
  highest-selected-version Available semantics, retained history, unknown/source-bound facts,
  UTC verification time, prerequisite anchors, nested README pages, identical render replay,
  stale/conflicting/wrong-feed facts, re-run/actor refusal, render failure before push,
  staging isolation and the existing normal-push concurrent-writer rejection.
- Accounting: controller +139/-4 (net135); worker/workflow/templates +231/-49
  (net182); tests +172/-8 (net164); documentation +168/-21
  (net147), including the preserved pre-task documentation edits (+10/-3).
  Total net production 317, below the approved500 ceiling. No new live status acceptance
  or deployment is claimed; the running sandbox still uses the earlier installed revision.
- A final format-check initially found a stale generated CMake file list referencing the
  independently removed checkouts/live-execution-evidence directory. Reconfigured the same
  isolated build to rediscover current files; format-check passes. No canonical formatter or
  CMake source change, no checkout deletion or branch switch was needed.


## 2026-10-26 — align public status with merged execution evidence and SSE heartbeat

- Owner requested alignment with current main, then explicitly requested status -> main.
  Current main is d9502fe, including execution-evidence merge330b8d9 and scoped heartbeat.
  Preserved the reviewed status implementation in checkpoint8958c42 before integration.
- Merged current main into the status task branch to resolve overlap before updating main.
  The sole textual conflict was WORK_LOG; both complete records were retained. Parent SQL,
  monotonic evidence merging, pipeline job verification, UI activity and SSE heartbeat remain.
- Public projection now consumes the existing GraphStore evidence view for build/publication
  job status, IDs, start/completion times and conclusions. Current verified job links no longer
  wait for successful artifacts. Existing successful artifact job IDs enrich the same canonical
  view when early evidence is absent; no second timing collector or persisted state was added.
- Extended focused assertions for public/management evidence agreement; updated setup/context
  documentation. Owner stopped checks: no build, test or format-check was run after this merge
  and adaptation. Earlier passing checks apply only to the pre-alignment checkpoint.
- Next authorized step is the local status -> main merge. No push, installation, database ALTER,
  controller restart, workflow activation, feed/key/tag or credential mutation is included.


## 2026-10-26 — combined sandbox activation verification

- Owner resumed focused checks and authorized the VOC-Experiments rollout in this thread.
  Merged d2d743e builds; lifecycle-sse-heartbeat, enrollment, graph-state, pipeline and worker
  provenance boundaries all pass (91.85s/2.94s/0.10s/13.17s/1.93s). Formatting passes.
- Discovered the matching worker tooling7cba278 and combined controller already activated by
  the preceding deployment operation. Evidence column exists, public HTTPS facts respond,
  and no package work is active. Do not repeat schema ALTER or replace trusted keys.
- Previous status run40365097888 rendered successfully but git push exited128. Its captured
  stderr was discarded from the job log; feed main remains5516c3f. Do not claim certainty
  from that absence or rerun assigned package work. Removed one stderr redirection so Git's
  standard failure diagnostic is visible. No retry mechanism or execution state added.
- Fresh current-state status renders are derivative display updates using checked pushes;
  preserve the earlier failed run and verify all retained package/key blobs independently.


## 2026-10-26 — live public-status deployment and feed authentication investigation

- Owner authorized the merged status rollout with “OK start!”; prior commit/merge/live-test
  instructions cover these reviewable checkpoints, sandbox tooling deployment and source push.
  Canonical checkout remains /home/voc/projects/snodec/core-ci. No additional source checkout.
- Worker tooling96f34e0 and matching thin workflow pins are deployed in VOC-Experiments only.
  The running development controller is the focused-checked build of3aa775d under
  /tmp/codex/build/core-ci/public-status-projection. Restart preserved all60 executions and
  generation297 exactly. Existing evidence schema was verified, not altered again. Main includes
  the parent evidence and SSE heartbeat work. UI remains loopback; TLS/443 settings unchanged.
- Status run40367183456 prepared successfully, then Git rejected the feed writer with HTTP403.
  The one-line stderr reduction exposed this standard Git diagnostic. Earlier40365097888 lost
  that diagnostic; do not retrospectively infer its exact cause from the unchanged feed.
- Existing writer App5424800/installation174142534 successfully minted a feed-only Contents-write
  token. Official GitHub documentation supports installation-token HTTP Git authentication:
  https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/authenticating-as-a-github-app-installation
  No credential, installation permission, ruleset, key or token scope was changed.
- Read-only HTTP advertisements and no-update receive-pack checks succeeded. An empty generated
  Git fixture reproduced403 once; subsequent fresh-token dry-runs passed authentication and
  reached the expected non-fast-forward rejection. This does not establish a root cause.
  Automatic review rejected a dry-run from the application checkout over potential private
  object disclosure. Used only the empty fixture instead; it has now been removed. No private
  repository objects or credentials were sent to the feed by those diagnostics.
- Fresh original controller-dispatched status run40368835408 succeeded; Pages run40368892486
  succeeded. Public feed commit4af1d4e7d0c2eee5631d716a7c35b4270a2d64c5 contains exactly13
  changed display paths. Every path was verified over HTTPS against its committed Git blob.
  All265 retained package paths, five trusted key paths and every non-display path are unchanged,
  including signed indexes, receipts and exports. No package build was dispatched for this test.
- Identical original refresh40369085122 succeeded and retained the exact same feed commit.
  No new signatures, replacement package bytes or duplicate graph work. Browser inspection of
  status/demo-join.html confirms latestv2.1.4, all five targets, UTC first-verification times,
  verified build links and no-active-work dashes. Management graph remains byte-for-byte equal
  to the recorded pre-rollout view. Status states while active remain focused synthetic proof;
  this rollout started with all package executions terminal.
- Both successful refreshes used the same code/pins/credentials as the explicit403 failure.
  Its cause remains unresolved and could affect a future feed write. Successful later runs are
  not a permanent fix. No automatic retry, token cache, scheduler or persisted state was added.
- Existing focused local proof: Debug build and format-check; five relevant CTests passed
  (lifecycle-sse-heartbeat, enrollment, graph-state, pipeline, worker-provenance). Worker
  provenance passed again after the one-line stderr change. No unrelated full suite. Subsequent
  changes are documentation only; whitespace validation passes. Public run evidence is saved in
  /tmp/codex/build/core-ci/public-status-projection/live, not counted as synthetic tests.
- Updated sandbox README to current pins, running paths, public status andv2.1.4 evidence.
  Private directories .qtcreator/ and .local-credentials/ were preserved without inspection.
  No production SNodeC workflow, feed, credential or tag, and no existing sandbox tag was changed.

- Task accounting against merged parent main d9502fe: production +383/-54 (net329); tests +178/-8 (net170); documentation +305/-40 (net265).
  Production growth remains within the approved500-line bound; no new production change for403.


## 2026-10-26 — webhook verification latency investigation (not yet fixed)

- Owner requested a detailed investigation and correction of delayed controller/public
  progress. Later steering explicitly keeps this task on delay; the independent publisher
  rendering defect is deferred. Canonical checkout is unchanged; delay work uses
  `fix/webhook-verification-latency` from main23f881f. The local cleanup documentation
  checkpoint1a64901 remains safe on `chore/sandbox-cleanup`; it is not mixed into this task.
  No open core-ci PR was present. No production source has been changed for this task.
- The automatic root request selected20 executions (core/app/side/join, five targets).
  All five core builds succeeded. Each publication failed in preparation, before the
  checked feed-update step. The renderer sorts a mixture of absent and integer receipt
  revisions. Five failed roots and15 waiting dependents are retained; no retry, manual
  advancement or mutation replay was performed. This is not a completed graph acceptance.
- Live example: Fedora build40374669998 completed at18:06:21Z; GitHub delivered its
  completed workflow_run at18:06:22.818Z and received202 in0.76s. The sampled controller
  graph still showed assigned work at18:07:17.568Z, running at18:09:22.739Z and completed
  evidence at18:09:47.779Z. Observations are approximately25s apart and do not establish
  exact commit times. Debian amd64/arm64 show the same delayed progression. IPK completed
  reports arrived18:07:45Z; completed graph evidence was observed18:10:37Z. APK completed
  reports arrived18:10:10Z; its built result was observed18:12:43Z.
- The controller opened1,914 GitHub HTTP connections from18:04:14 through18:30Z. The
  sampled same-App token mint and completed-run read took0.414s and0.402s respectively;
  that API response had4,782 of5,000 core requests remaining. This disproves quota
  exhaustion for that sample, not every possible future request. Three local graph
  requests took0.031/0.026/0.025s. No evidence supports blaming a blocked event loop or
  GitHub webhook delivery for the multi-minute delay in these samples.
- A short read-only debugger trace captured only method, public API path, queue length
  and elapsed time; no headers, bodies or credential files. Eighteen completed requests
  were observed in11.95s at the end of the burst, mostly token minting and repeated Git
  objects for public-status approval. Queue length was1–2 after the earlier burst had
  drained; this is not a measurement of the earlier maximum backlog. The debugger was
  detached without signalling/restarting the controller. The controller remains running.
- Traced path: Pipeline::event acknowledges validated notifications, then every matching
  event invokes inspect. HttpClient serializes requests and opens a fresh TLS connection
  for each. GitHub::request mints an installation token for each API call. inspect reads
  attempt1, walks five Git objects for the workflow, then reads attempt1 jobs, even when
  that exact immutable workflow identity is already persisted. Eight unchanged reports
  therefore produce112 HTTP requests. GraphStore correctly suppresses identical commits;
  it does not suppress the reads preceding them. Every real committed graph change also
  runs workflow approval and status dispatch (14 HTTP requests per refresh). These
  derivative reads share the serial client with evidence verification. Matching an
  assigned run ID also lets unrelated helper-job notifications enter the full inspection.
- Focused synthetic reproduction added to tests/pipeline.py: real isolated MariaDB and
  controller TLS routes, synthetic GitHub, eight concurrent signed notifications,30ms
  response delay. Baseline:112 HTTP requests,40 Git-object reads,4.515s to drain; unchanged
  graph generation. Full existing pipeline-boundaries exercise also passed. This is local
  synthetic proof, not GitHub acceptance. Command:
  `python3 tests/pipeline.py --executable /tmp/codex/build/core-ci/public-status-projection/core-ci --directory /tmp/codex/build/core-ci/webhook-latency/baseline-tests`.
  Baseline binary is the running3aa775d implementation (later main changes are docs).
- Architectural invariant: notifications request fresh verification of assigned work;
  duplicates must not multiply concurrent verification of the same run or make derivative
  status rendering starve that verification. The graph remains the sole execution/state
  authority. Attempt1, actor/repository/workflow binding, signed publication, monotonic
  evidence and uncertainty must remain unchanged. Graph validation already rejects stale
  state/evidence regression; one concurrent duplicate artifact callback logged503 after
  another callback advanced the attempt. No graph regression was observed.
- Reduction/change considered first: discard irrelevant helper-job/terminal-phase
  notifications and reuse the graph's already verified immutable workflow tuple when
  repository/run/commit/blob match exactly; still fetch fresh attempt1 status/jobs.
  Those reductions alone leave concurrent duplicate chains and one approval chain per
  graph change. Proposed bounded addition (requires owner approval): transient in-flight
  notification tracking with one dirty bit per exact assigned run, and one in-flight
  status-refresh flag using the existing graph generation to catch up to the newest state.
  A notification received during verification causes one subsequent fresh read; it must
  not be dropped when it could signal completion. Obsolete status generations need not
  dispatch workflows; the latest committed generation must eventually dispatch. No timer,
  credential cache, new queue, scheduler, persisted state or HTTP-client refactor.
- Estimated combined correction:55–85 net production lines and100–160 test lines
  (including the33-line baseline probe already added). Approval is required by AGENTS.md
  before this production growth/transient tracking is implemented. Focused verification
  must prove completion arriving during an inspection is retained, wrong identities remain
  rejected, immutable evidence reuse is exact, refresh catches the newest generation,
  and the measured burst reduces to bounded reads. Live verification remains outstanding.
- Official interfaces checked:
  https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run-attempt
  documents attempt-specific evidence; the fix must continue using attempt1.
  https://docs.github.com/en/rest/git/trees#get-a-tree and
  https://docs.github.com/en/rest/git/blobs#get-a-blob document immutable Git objects and
  tree modes; the proposal reuses previously verified graph evidence, not a new contents
  shortcut that could weaken path/symlink checks.
  https://docs.github.com/en/rest/apps/webhooks#list-deliveries-for-an-app-webhook
  documents delivery timestamps/durations and cursor pagination used for this inspection.
- Evidence is under /tmp/codex/build/core-ci/webhook-latency and the full-graph-session
  timestamp directory20261026T180414Z. No production SNodeC resource, signing key, tag,
  credential or previously published package byte was changed. Private user directories
  were preserved without inspection. Current task changes are tests/documentation only,
  uncommitted and not deployed; git diff --check passes.


## 2026-10-26 — minimum-request investigation after owner correction

- Owner rejected the request amplification and asked to investigate using only genuinely
  necessary calls, normally fewer than ten per operation. This is investigation only;
  the preceding55–85-line proposal has not been implemented or approved. Its focus on
  combining REST inspections retained too much unnecessary verification work.
- Inspected two real Fedora completion deliveries through the App delivery API, recording
  field names and public identity/status values only. workflow_run includes run_attempt,
  actor, repository/head_repository IDs, event, path, head_sha, title, status and conclusion.
  workflow_job includes run_attempt, run_id, job ID/name, head_sha, status, timing and steps.
  Twelve offline identity/timing comparisons against the persisted real run40374669998
  all match. This confirms field availability in these real deliveries; it is not a
  production-path test and not proof that every possible delivery contains every field.
- GitHub's raw-body HMAC authenticates these facts as GitHub reports. Re-fetching the same
  facts by REST is not inherently stronger authentication. Recommended responsibility:
  normalize authenticated webhook facts and REST reconciliation facts into one validation
  and graph-application path. Keep run/attempt/actor/repository/workflow binding and
  monotonic graph evidence. Do not create competing webhook and REST state machines.
- Known assigned-run progress can therefore require zero evidence REST calls. Duplicate,
  late and irrelevant reports should require zero calls and no status refresh when the
  graph does not change. A job report alone cannot establish the controller actor or the
  approved workflow blob: require an already bound run or obtain those missing proofs once.
  Never trust a source hook's status or accept attempt2 evidence. Missing/out-of-order
  facts require only the absent proof, not unconditional repetition of the full chain.
- Build completion still needs the assigned artifact metadata, because the webhook does
  not provide the artifact's immutable ID/digest/size. Normally this is one REST read
  after run/job identity is known. Feed completion still requires independent committed
  receipt and public HTTPS-byte verification; workflow success alone is insufficient.
- Fresh installation tokens per API call are also unnecessary. Official GitHub docs
  specify an expiry and one-hour lifetime. Reuse must retain the exact repository and
  permission scope, stay server-memory only, use the returned expiry and never replay a
  failed/uncertain mutation. This would be an explicit authentication-boundary change,
  not an execution scheduler. No token reuse/cache has been implemented in this task.
- Current FeedProof has seven GitHub reads plus one public HTTPS receipt read. Fresh
  token minting doubles those seven GitHub calls, giving15 HTTP requests. Reusing one
  correctly scoped Contents-read token gives eight warm requests, nine with one mint.
  This count excludes first run binding and a missing-job fallback; it is not a claimed
  hard ceiling for recovery. Initial workflow proof and restart reconciliation must be
  counted separately and reduced only where the identical proof is already available.
- Public status refresh is separate work: a genuine graph change needs a dispatch;
  duplicate unchanged evidence needs none. Approval of the same immutable workflow must
  not be recomputed from Git objects for every generation. Any retained approval must be
  bound to the exact ref-resolved commit/blob and invalidated by a changed ref. Its
  implementation and line estimate remain to be designed before production changes.
- Request budgets must include token minting, fallback reads and derivative refresh work,
  rather than hiding those costs in a helper. A whole graph necessarily has multiple
  build/publication dispatches; fewer than ten is not a promise for an entire20-node graph.
  Normal progress, completion, refresh and cold/recovery cases need separate measured
  budgets in the focused harness before claiming the correction.
- Official authentication/payload references:
  https://docs.github.com/en/webhooks/using-webhooks/validating-webhook-deliveries
  https://docs.github.com/en/webhooks/webhook-events-and-payloads#workflow_run
  https://docs.github.com/en/webhooks/webhook-events-and-payloads#workflow_job
  https://docs.github.com/en/rest/actions/artifacts#list-workflow-run-artifacts
  https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-an-installation-access-token-for-a-github-app
- Safe evidence files: webhook-field-inventory.json and webhook-reuse-check.json in the
  existing temporary webhook-latency directory. No production edit, live graph mutation,
  secret inspection, controller restart, workflow change or deployment was performed.


## 2026-10-26 — detailed reduction map, accepted artifact boundary

- Owner accepted standard GitHub webhooks plus one artifact-list request after successful
  build completion, then requested continued detailed investigation. No custom callback is
  proposed and no production implementation was made in this investigation checkpoint.
- Added docs/proposals/webhook-request-reduction.md with the traced request inventory,
  shared report/reconciliation boundary, proof reuse, ordering constraints, failure certainty,
  token scope/expiry, status refresh and proposed normal/cold-case budgets. It explicitly
  distinguishes proposed changes from the currently deployed implementation.
- Further source findings: publicationResult checks FeedProof before its existing skipped-push
  classifier; an absent receipt normally costs four HTTP requests, not the15-request successful
  proof chain. Workflow preflight rereads a commit whose root tree was already returned;
  dispatch then discards that preflight proof and walks the actual workflow again, even when
  its executed commit matches. Approval readers download workflow bytes only to use their hash.
  Unrelated helper jobs match solely by assigned run ID. These are reduction opportunities.
- New ordering constraint identified: direct reports need a recorded run conclusion distinct
  from job conclusion, and a recorded terminal publisher push-step result. The existing
  evidence JSON can hold these facts without a schema change or new execution state. Both
  paths must use shared validation and the canonical merged graph, not a second state machine.
- The standard dispatch API for configured2026-03-10 returns200 with workflow_run_id/URLs;
  this is documented. That response does not contain all actor/workflow/job proof, so it
  cannot alone replace initial binding. No speculative return_run_details parameter is used.
- Existing source-selection/F3 policy is unchanged: do not omit unreadable newly selected
  repositories or silently substitute old definitions. Initial selection/recovery requests
  are not ordinary progress and must be accounted for separately.
- Stopped only the settled-session read-only comparison observer1435482, verifying its exact
  script identity. Controller1407899 remains running unchanged. Controller/public projection
  both report generation340. No graph mutation, retry, new build or deployment was made.
- Existing synthetic burst baseline remains112 requests/40 Git-object reads/4.515s; no
  passing test was needlessly repeated. No production code changed. Diff whitespace passes.


## 2026-10-26 — implementation prompt prepared

- Owner asked how to implement the correction and whether to prepare a prompt first.
  Created docs/proposals/webhook-request-reduction-task.md as a reviewable implementation
  task grounded in the investigation and accepted reporting/artifact boundary.
- Prompt requires shared report/reconciliation validation, exact proof/actor/attempt binding,
  reduction before added mechanisms, honest full request accounting, focused tests, and
  separate normal/cold/recovery budgets. It preserves uncertainty, source-selection policies,
  deployment boundaries and the deferred HTTP-client and renderer work.
- It grants no production growth or new blanket activation authority. A concrete bounded
  estimate is required before implementation under AGENTS.md. Existing sandbox stage
  authorization is retained where applicable; commit/push/merge remain distinct.
- Production remains unchanged and the prompt is uncommitted. No tests were rerun for this
  documentation-only addition. git diff --check passes.


## 2026-10-26 — new implementation branch and bounded design checkpoint

- Owner explicitly requested a new branch and the implementation task's design-first
  checkpoint. Created fix/github-request-reduction from main23f881f, preserving all prior
  investigation/test edits without committing, resetting or adding a checkout. No open PR.
  Cleanup checkpoint1a64901 remains separate. Controller1407899 was verified running core-ci;
  no restart/deployment or graph mutation occurred.
- Re-traced current Pipeline event/inspect/completion/refresh/dispatch, GitHub auth, SourceReader,
  Graph evidence merge/GraphStore commits and pipeline fixtures. Read SNode.C HTTP application
  callbacks/lifecycle and MQTTSuite HTTP client ownership. The proposed change leaves main,
  transport and database lifecycle untouched.
- Verified one further real delivery fact: workflow_run.head_commit.id equals head_sha and
  supplies tree_id (Fedora40374669998). Official workflow-run REST examples contain the same
  fields. This supports shared known-root file verification without another commit fetch.
  Only identity fields were saved in webhook-tree-proof.json; no headers, bodies or secrets.
- Added docs/proposals/github-request-reduction-design.md: one shared observation/completion
  path, two additional facts in existing evidence JSON, exact immutable proof reuse, scoped
  token lifetime/mint sharing, completion-read exclusion, latest-generation refresh and batch
  preflight sharing. Each transient item identifies the redundant mechanism it replaces.
- Design estimate: production+420/-330 (net90), tests+280/-60 (net220, includes existing33-line
  probe). Approval requested before implementation: ceilings120 net production/260 net tests
  for the exact listed scope. No approval inferred from the earlier55–85-line discussion.
- Budgets distinguish normal0/1 evidence reads,7–8-request feed verification with known root,
  cold approval/token costs, derivative dispatches and roughly14–15-request combined cold
  recovery. No false ten-request ceiling or measured full-graph claim. The old baseline
  remains112 requests/40 Git-object reads/4.515s; no passing test needlessly repeated.
- Production remains unchanged. Documentation whitespace check passes. No keys/tokens,
  private directories, source tags, production SNodeC resources or published bytes touched.

### Request reduction implementation checkpoint — approval ceiling exceeded

The owner approved the shared webhook/REST observation design with ceilings of
120 net production lines and 260 net test lines. Implementation is on
`fix/github-request-reduction`, based on `23f881f4419f243403ccaa9ef3cad890fdf8c383`.
No commit, push, merge, installation or controller restart was performed.

The initial draft reshapes Pipeline's unconditional inspection into shared
observation/application/completion functions, adds exact-scope installation-token
reuse and immutable workflow-proof reuse, coordinates derivative status refresh,
and lets the existing SourceReader walker use a known root tree and identity-only
reads. Feed proof receives the root tree already supplied by commit history.

The estimate was incorrect. A read-only canonical-format projection of the draft
counts production +421/-186, net +235. The unformatted working diff counts
+320/-170, net +150. These are draft counts, not final implementation accounting.
Production editing stopped when the overrun was identified. This exceeded the
approved ceiling and should have been detected earlier.

The draft is incomplete and unverified. In particular, remaining work includes
coalescing missing run/job evidence reads, preserving concurrent completion facts,
checking current attempts before applying asynchronous results, moving the existing
skipped-push classification ahead of feed proof, handling token-mint exceptions
without leaving waiters, and updating realistic public-boundary test fixtures.
No claim is made that this draft is ready to build, deploy or accept live reports.
The baseline burst remains 112 HTTP requests / 40 Git-object reads / 4.515 seconds;
no improved measurement has been obtained yet. Existing sandbox assignments and
failed/waiting work remain untouched.

A revised ceiling of 350 net production lines (260 net test lines unchanged) is
proposed to finish the same approved design, including the outstanding guards and
canonical formatting. This is a revised estimate, not authorization. No additional
queue, persisted state, scheduler, reporting protocol or transport refactor is
proposed. Commit/push/merge remain separate authorization stages.


## 2026-10-26 — approved request reduction implemented and locally verified

- Owner approved the revised ceiling of 350 net production lines; the test ceiling
  remains 260. Continued the same design, without a scheduler, queue, persisted state,
  reporting protocol, controller thread, timer or HTTP-client/per-origin refactor.
- Pipeline webhooks now supply authenticated run/job facts directly to the same
  observe/record/completion path used by attempt-one REST recovery. Actor, workflow,
  installation/repository, assignment, attempt and job identities remain checked.
  Unrelated helper jobs are ignored even when their run ID is assigned.
- Existing graph evidence gains run_conclusion and terminal push_step_conclusion.
  Run/job conclusions remain distinct. Generation-conflict rollback recomputes the
  evidence merge against the current graph. Unknown mutations are never replayed.
  Missing job evidence can be read once per relevant run observation/reconciliation;
  an incomplete response cannot recurse into polling. Completion-read exclusion and
  current-attempt checks protect overlapping artifact/feed verification.
- Reuse immutable approved workflow proof, coalesce simultaneous exact GET questions,
  and use SourceReader's existing regular-file walker with known root trees and an
  identity-only mode. Definitions/previews/receipts still read and verify contents.
  Changed run commits obtain new proof. FeedProof reuses the commit-list root tree.
- GitHub installation tokens remain memory-only, exact repository/permission scoped,
  with returned-expiry checks and one shared simultaneous mint. Signing happens before
  enqueuing the first waiter, so a signing exception cannot strand callbacks. The lost
  mutation-response test verifies no automatic replay. Secrets are not logged.
- Status refresh combines obsolete in-flight generations and dispatches the newest.
  Trusted skipped-push evidence is applied before feed reads. A possible push still
  needs committed receipt plus matching public HTTPS bytes. Failures remain visible;
  explicit reconciliation/retry policies are preserved.
- Local measurements: unchanged eight-report burst fell from 112 HTTP / 40 Git-object
  reads / 4.515s to 0 / 0 / 0.063s. Graph/SSE progress took 96ms with 300ms synthetic API
  latency, without evidence reads. Eight overlapping job-first reports shared one run
  read; overlapping completions shared one artifact read. Warm receipt verification
  used six GitHub reads plus one HTTPS read. These are synthetic GitHub observations
  with real isolated MariaDB, not live GitHub acceptance.
- Final synthetic session: 329 HTTP total, including 25 token mints, 40 evidence reads,
  104 source reads/edits, 84 worker approvals, eight build dispatches, three publication
  dispatches, 45 status dispatches, 18 feed GitHub reads and two feed HTTPS reads.
  Shared worker approval reads cannot be honestly attributed solely to status or
  publication. Counters include every fixture GET/POST/PUT and lost-response requests.
  This eight-assignment scenario is not comparable as equal work to the old live
  five-root failure session, and does not prove a complete 20-node live graph.
- Validation: isolated Debug core-ci build; graph-state-boundaries; all nine affected
  github-transport cases; eight source-selection cases; pipeline-boundaries; and
  enrollment-boundaries. All 20 relevant checks passed after realistic fixture updates.
  Initial combined run exposed enrollment's missing commit.tree fixture field; only
  that affected test was rerun after correction. The final Pipeline identity guard
  was rebuilt and pipeline-boundaries rerun. format-check, cmake-format --check and
  git diff --check passed. Logs and measurements: /tmp/codex/build/core-ci/webhook-latency/.
- Production +514/-238 = +276; build/test wiring +11/-2 = +9 (combined +285,
  below approved +350); tests +250/-59 = +191 (below approved +260). Documentation
  includes the preserved investigation/prompt and updated design/results, separately.
- Checkout /home/voc/projects/snodec/core-ci, branch fix/github-request-reduction,
  base/HEAD 23f881f4419f243403ccaa9ef3cad890fdf8c383. All task changes remain uncommitted.
  No push, PR, merge, installation, live restart, retry or new live build was performed.
  The previously recorded live PID1407899 was not present when checked; no replacement
  was started. No new live run URLs or Pages latency claim. Sandbox activation needs
  runtime identity/configuration revalidation. Full publication remains gated by the
  separately deferred renderer defect. Existing sandbox graph history was not mutated.
- No production SNodeC workflows/credentials/feeds/tags, keys, tokens or published bytes
  were modified. .qtcreator/ and .local-credentials/ were preserved and not inspected.
  No additional checkout was created; the permitted task build is retained for review.

## 2026-10-26 — owner requested activation and full session from core

- Owner instructed “do this from core up” after the merge/restart/full-test prerequisites
  were stated. Created tested controller checkpoint 2eba58b on fix/github-request-reduction.
- Rechecked the separate renderer gate: Jinja sorts optional receipt revision fields,
  so mixing retained receipts without that field with new integer revisions fails.
  The publication contract already names every receipt <reserved-revision>.json.
  The renderer now reads that existing immutable filename when the optional field is
  absent, without rewriting evidence or inventing a revision. Production change is
  one line replaced, net zero. Added mixed-receipt selection and unchanged-byte checks.
- Focused tests/worker_provenance.py passed in the permitted renderer-test directory,
  including expected local stale Git-push rejection. No full suite was run.
- Sandbox worker main verified at 3740d2b6af186f55aa7f291bc250c4b4adac5c27.
  No local core-ci/MariaDB process was running at activation preflight. Existing
  runtime arguments/data and credential-file references will be reused, not printed.

## 2026-10-26 — live session exposed skipped-job matching defect

- Activated f85c167 using the existing managed database and TLS settings. The earlier
  restricted-namespace process check was misleading: host PID1407899 was still running.
  Verified its executable, recorded graph generation340/all80 records, gracefully stopped
  only that controller and its owned database, and verified exact restoration with the
  replacement PID1475799. The unrelated MariaDB server was not touched.
- Deployed the one-line renderer correction to sandbox worker commit
  60ff1107f7192c42c2e3e5243984b8049bd864d8, and updated CORE_CI_TOOLING_SHA. Updated
  all four thin workflows through core-ci's existing App route to blob
  1c846209d3a10bf01ffe9eddf65858dcb8b78192. The CLI token lacked workflow scope;
  its unsuccessful workflow write was not repeated through that credential. Source tags,
  definitions, existing keys and packages were unchanged by this setup.
- Made exactly one core-up request for demo-core v2.1.0, selecting20 new graph nodes.
  Automatic publication released Debian and APK dependents. Four publications verified
  before demo-side/APK publication40395761204 failed validation: the artifact storage
  download returned HTTP503, “Egress is over the account limit.” The privileged job
  was skipped; it performed no signing/feed mutation. This matches Azure Storage's
  ServerBusy error wording, not proof of exhausted organisation billing quota:
  https://learn.microsoft.com/en-us/rest/api/storageservices/common-rest-api-error-codes
- Live attempt-one jobs show the skipped privileged job name literally as
  `Publish ${{ fromJSON(inputs.assignment).label }}`. The old matcher required the
  expanded label, so it retained the assignment rather than recording a definite
  pre-push failure. Invariant: the already approved run's definitively skipped privileged
  job must release the feed writer without interpreting another run's job as evidence.
- Consolidated the three existing name checks into matchesJob. The literal-name case
  requires publication, terminal skipped conclusion and an already bound matching run;
  observe still checks job ID, attempt one and exact workflow commit. It cannot identify
  arbitrary unbound work or authorize a running/successful job. No persisted state,
  retry policy, callback protocol or worker change. Production +13/-9 = +4; tests +5/-3
  = +2. Total remains within the approved350/260 ceilings.
- Focused pipeline-boundaries and format-check passed. No failed execution was retried.
  Deployment/restart reconciliation will classify the existing skipped publication;
  this is read-only evidence reconciliation, not replaying a feed mutation.
- Read-only SSE/feed observer records progress and lag under the permitted task live/
  directory. Three real build completions reached SSE within0.16s of GitHub delivery
  timestamps (including negative offsets because response completion timestamps can be
  later than the observed SSE commit). Public pages sometimes wait behind the active
  publisher's shared feed-writer slot. Also noted an empty Waiting-for label on roots;
  no unrelated page redesign was made. No blanket automatic retry has been added.

## 2026-10-26 — explicitly approved APK retry published

- Owner approved exactly one retry of demo-side/openwrt-25.12-x86_64. Submitted
  one existing /api/work/retry request; reserved revision102 retained the selected
  source and prerequisite identities. No blanket retry or manual advance was used.
- Build40397665070 succeeded; publication40398272126 succeeded. The controller
  verified its receipt/public bytes and recorded published at generation500. Run links:
  https://github.com/VOC-Experiments/demo-side/actions/runs/40397665070
  https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40398272126
- Restarted the controller at local main2b99897 to apply skipped-job matching.
  Existing bound run identities were restored unchanged. The earlier failed revision96
  remains history; reconciliation established its privileged job was skipped before the
  explicit retry. No uncertain feed mutation was replayed.
- At20:49UTC the session had7/20 published nodes, one publication active, four built
  nodes awaiting publication and eight waiting nodes. This is ongoing live acceptance,
  not a completed full graph. The read-only SSE/Pages observer remains running.
- Pages caught up to generation502, including the successful retry; controller508
  already had newer publication progress. Snapshot freshness still depends on the
  serialized feed writer and Pages propagation, separate from prompt controller SSE.
- Sampled20:38:54 requested notification had GitHub's generic title
  core-ci trusted publication rather than the assignment title; strict identity
  validation rejected it. Later expanded identity reports converged without weakening
  validation. A second earlier identity rejection remains unclassified.
- No additional production/test changes in this checkpoint. Source commits remain
  local (main ahead of origin/main by three); no production SNodeC resource, trusted
  key, existing source tag or prior package bytes was modified by the retry.

## 2026-10-26 — continued graph session and runner mirror delay

- Continued the same authorized session without new work requests, explicit advances
  or retries. By21:11UTC17/20 nodes were published. Remaining work: demo-join Debian
  arm64 publication, demo-app Fedora publication and its dependent demo-join Fedora.
- At21:25UTC assigned arm64 publication40402091147 was still running the standard
  signing/repository-tool installation step, started21:11:05UTC. GitHub's attempt-one
  jobs API independently agrees with controller generation657. The live browser log
  shows repeated Ign responses from http://azure.archive.ubuntu.com/ubuntu and fallback
  to https://archive.ubuntu.com/ubuntu during apt-get update. Feed preparation and push
  have not started. Do not attribute this delay to webhook processing or replace the
  assigned run. Link:
  https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40402091147/job/121850518683
- Public status generation651 matches target states but retains the older assigned
  activity for arm64 publication; the feed writer remains occupied by this live job.
- Read-only retained-file check at21:12:27UTC compared all337 pre-session immutable
  package/key/receipt/export Git blobs with the current feed tree: zero changed/missing.
- Controller GitHub transport count reached760 and remained unchanged during sampled
  mirror waiting; this includes derivative status work and restart recovery, and is
  not an exact endpoint-category HTTP count. Earlier1914 connections covered only a
  partial five-root session, so these are not equal-work benchmarks. CLI/browser
  diagnostic requests are separate from controller traffic. No new source/test edit,
  signing-key change, package replacement or production activation in this continuation.

## 2026-10-27 — session recheck:19 published, one explicit failure

- Live controller generation688 has19/20 session nodes published and one failed;
  no building/waiting/publishing node remains. Both remaining Fedora publications
  completed automatically, including demo-join run40406113963.
- demo-join Debian arm64 publication40402091147 was cancelled at21:36:17UTC on
  October26 while installing tools. Attempt-one job121850518683 confirms preparation
  and checked push were skipped. The controller correctly recorded a definite failure
  before feed update and released the writer. Cancellation cause was not established
  by this check. No new retry was submitted; previous single retry approval concerned
  demo-side APK, not this target.
- Public HTTPS status.json now reports generation688, matching all20 session nodes
  on state, activity, revision and tag. The observer stopped at its bounded deadline;
  this recheck read the actual live controller and public feed directly.
- All337 baseline immutable blobs remain unchanged. Controller log
  counts833 GitHub connections across the whole session, including restart recovery
  and derivative status dispatches. This is not an endpoint-category request count.
- No production/test code or deployment setting changed in this recheck. Full-session
  success remains unclaimed because the arm64 publication failed. Evidence is retained
  under /tmp/codex/build/core-ci/webhook-latency/live/; WORK_LOG.md is uncommitted.

## 2026-10-27 — completed session recheck

- Owner-operated retry selected demo-join Debian arm64 revision103. Build40462326071
  and publication40462396187 succeeded. Live controller generation707 records all20
  session targets published; public status.json matches generation707 with no differences
  in state/activity/revision/tag. No active sandbox workflow was found at the check.
- This session included one approved APK retry, a controller correction/restart, and the
  owner's arm64 retry after a cancelled publisher. It is not an uninterrupted first-pass
  success. Package-client installation tests were not rerun in this session.

## 2026-10-27 — remove management Origin/Host restriction

- Owner explicitly requested no management Origin/Host check; authentication is deferred.
  Invariant: management writes use the existing JSON/graph validation path regardless of
  browser address or Origin header. GitHub webhook raw-body HMAC remains independent.
- Traced shared main middleware, all management routes, UiConfig registration, browser
  feedback, enrollment/pipeline harnesses and deployment documentation. Removed the
  Origin/Host comparison and the unused UiConfig class/config option/CMake source entry.
  Retained JSON content-type/body validation, security response headers and separate
  public listener routes. No state, scheduler, listener binding or authentication added.
- Branch fix/remove-ui-origin-restriction, based on current local main2b99897. Existing
  live-session WORK_LOG entries preserved in a separate documentation commit. Owner
  authorized commit, merge, restart and a single new core-up build request. No push requested.
- Build: cmake -S . -B /tmp/codex/build/core-ci/ui-origin -DCMAKE_BUILD_TYPE=Debug
  -DBUILD_TESTING=ON; built core-ci, graph-test and graph-store-test with -j2.
  format-check and git diff --check passed. Focused enrollment-boundaries and
  pipeline-boundaries both passed (real isolated MariaDB, synthetic GitHub;10.62s).
  Enrollment proves JSON rejection, missing Origin accepted through body validation,
  successful localhost write with different Origin, and the existing commit/SSE behavior.
- Production src: +2/-40, net -38; CMake source wiring: -1; tests: +12/-8, net +4;
  human documentation: +6/-7, net -1, excluding this WORK_LOG and prior session entries.
- Initial auto-review rejection was resolved after checking live listener boundaries:
  UI127.0.0.1:8080; separate webhook192.168.1.239:8443, no management routes there.
  A subsequent configured-help check was rejected because the saved launch command
  references .local-credentials/live-sandbox/controller.conf, whose contents must not
  be inspected under existing instructions. Protected directory was not inspected or
  modified during implementation. Owner subsequently authorized runtime activation,
  including removal of only obsolete ui.origin from the configured protected file.
  Restart and the new session will be recorded after verification.

## 2026-10-27 — merged Origin removal, restarted, core-up request reused completed work

- Owner authorized commit, merge, restart and a new full build starting from core.
  Documentation checkpoint0434ca3 and fix47737f7 were committed and fast-forward
  merged to local main. No push was requested or performed.
- Removed only one obsolete ui.origin line from the protected configured runtime file,
  under the owner's subsequent activation approval. No secret value was displayed.
  Gracefully stopped idle controller1479498 and started tested binary from
  /tmp/codex/build/core-ci/ui-origin/core-ci as PID1553769. All100 execution records
  restored exactly at generation707. No listener/TLS/feed setting changed.
- Live POSTs with invalid retry JSON through both localhost and127.0.0.1 and an
  unrelated Origin returned the route's422 validation result rather than403. These
  probes created no attempt. Existing focused tests/build/format checks remain valid.
- Submitted exactly one /api/work/request for VOC-Experiments/demo-core with empty tag
  (highest SemVer). It returned200 with selected0/assigned0; graph generation707 and
  all completed work are unchanged. No full build is running. Existing Graph::select
  deliberately reuses identical input identity; explicit retry permits failed work only.
  Do not fabricate failure records or silently change selection identity to force work.
- Starting another graph now needs a genuinely new selected source, such as a new
  sandbox demo-core tag, or separately designed explicit rebuild behavior. No new tag,
  source commit, retry, workflow mutation or production behavior extension was made.
  Runtime/session records are /tmp/codex/build/core-ci/ui-origin/live/.

## 2026-10-27 — approved sandbox v2.1.1 starts full graph through webhook

- Owner explicitly approved new demo-core v2.1.1 tag at existing source commit
  1cfcfa17de500bb68f389bf6b8c4cbf27ddcbb57. Verified v2.1.0 points there and new
  tag was absent. CLI creation returned definite404; independent read confirmed absent.
  Used the existing controller App with a token restricted to demo-core contents/workflows
  writes. Creation returned201. No App permissions, secret values or existing tags changed.
- The real tag webhook selected20 new executions and automatically assigned five root
  builds at graph709, leaving15 dependents waiting. No manual work request/advance or
  retry was submitted for this tag. Actual source tag is v2.1.1; selected source commit
  remains unchanged. Source workflow checkout is separate as required by the contract.
- All five root runs are in_progress, verified through GitHub:
  DEB amd64 https://github.com/VOC-Experiments/demo-core/actions/runs/40464159579
  DEB arm64 https://github.com/VOC-Experiments/demo-core/actions/runs/40464156677
  RPM https://github.com/VOC-Experiments/demo-core/actions/runs/40464154014
  OpenWrt IPK https://github.com/VOC-Experiments/demo-core/actions/runs/40464151131
  OpenWrt APK https://github.com/VOC-Experiments/demo-core/actions/runs/40464146574
- Read-only SSE/HTTPS status observer is running from ui-origin/live/observe.py with a
  bounded three-hour observation window. New baseline records retained package/key/
  receipt/export Git blobs before any new publication. Results are ongoing; full-session
  acceptance is not yet claimed. No package-client installation proof is implied.

## 2026-10-27 — continuous GitHub/UI/Pages monitoring found uncertain feed push

- Owner requested continuous observation of GitHub CI, public status pages and local
  port8080 UI. Kept existing read-only SSE/public-JSON observer running; sampled actual
  Chrome UI without reload/control actions, GitHub assigned runs/jobs and all four HTML
  pages. Diagnostic traffic is separate from controller traffic. No source/test edit,
  new work request, retry, manual advance or configuration mutation during monitoring.
- Local UI remained Live and its active counts followed the graph through SSE. Its17
  failures belong to retained history, not this new session. Joins correctly waited for
  all prerequisites: Fedora core+app publication did not release join while side was
  unpublished; arm64 core+side publication did not release join while app was unpublished.
- Public snapshots lagged graph facts by roughly2–3minutes in sampled entries. Captured
  generations, state/activity differences and HTML text under ui-origin/live/. One
  derived negative age occurs because a newer graph fact postdates the sampled page;
  it is not negative processing latency. Status workflows were pending/cancelled as
  obsolete refreshes were replaced behind the shared feed writer.
- Presentation findings, not changed: root rows can show an empty Waiting-for label;
  building/publishing rows can list prerequisites already satisfied as Waiting-for;
  new feed bytes can become Available before controller verification supplies Published
  UTC. The latter is a transient difference between actual feed contents and verified
  graph publication, not proof of a failed signature or missing package.
- Publication40465675752 (demo-app OpenWrt APK, revision113) failed at07:13:54UTC
  inside Publish checked feed update. Authenticated attempt-one evidence records failed
  job, failed run and failed push step. Trusted job122060732110 log states:
  remote Permission to VOC-Experiments/packages.git denied to
  voc-experiments-ci-coordinator[bot]; Git returned HTTP403 and exit128. The intermittent
  authorization denial's root cause is not established. Other publications with the
  same tooling/App succeeded earlier in this session. No credential/scope changes made.
- Controller retains publishing uncertainty and the per-feed writer; missing public
  receipt (HTTP404) is not permission to retry. Existing policy treats started push-step
  failures as uncertain. No replacement publication was dispatched. UI and Pages show
  publication complete; verifying results despite recorded GitHub failure: this wording
  hides failure/uncertainty and is a separate presentation finding.
- At07:18:53UTC graph865 had6 published,1 publishing/uncertain,6 build-stage nodes
  and7 waiting. Latest assigned-job reads found no active GitHub source/publication/
  status/Pages run; build-stage nodes are built and waiting for the retained writer.
  Public status caught up to865 with zero state/activity/revision/tag differences.
  At07:20UTC all four Pages HTML table texts matched GitHub main status/*.md tables.
- All387 baseline immutable package/key/receipt/export Git blobs remain unchanged.
  Controller log counts334 GitHub connections so far in this partial session, including
  status work. This is not exact per-endpoint HTTP accounting or completed acceptance.
  Evidence: github-observations.jsonl, graph-observations.jsonl, feed-observations.jsonl,
  html-observations.jsonl, page-git-parity.jsonl, diamond-observations.jsonl and
  retained-check.json under /tmp/codex/build/core-ci/ui-origin/live/.
- Full graph session remains blocked; no automatic retry/recovery policy was invented.

## 2026-10-27 08:55UTC — full live health recheck

- Owner requested CI and everything else be checked again. GitHub source, publication,
  status and Pages listings have no active run. Last publication40465675752 remains
  failure (07:13:55UTC); last status refresh40466001160 succeeded. No retry/advance made.
- Actual local graph remains865:6 published,1 uncertain publication,6 build-stage,
  7 waiting. Database health query reports available and mutations_allowed is true.
  Chrome UI remains Live with matching counts; its17 failed count is retained history.
- Public JSON remains865 with zero state/activity/revision/tag mismatches. All four
  Pages HTML table texts equal corresponding GitHub main Markdown table texts. All387
  pre-session immutable package/key/receipt/export Git blobs remain unchanged.
- New network limitation: gw-home.vchrist.at resolves212.241.96.69, but connection
  to8443 failed from this machine (No route to host/timeout). Direct192.168.1.239:8443
  TCP also timed out. ss shows the controller still listening on192.168.1.239:8443;
  ip address inspection shows eno1 DOWN without IPv4 and Wi-Fi UP at10.20.99.86.
  The configured webhook listener address is no longer assigned to this machine.
  No firewall, listener, DNS, TLS credential or forwarding setting was changed.
  External reachability from GitHub itself was not probed by generating a new event.
- Loopback8080 and live database remain healthy. Controller PID1553769 and read-only
  observer1554586 are alive; observer is sleeping normally and recorded a fresh matching
  public snapshot at08:55:07UTC. It retains the existing bounded observation window.
- One read-only health check's automatic approval review timed out; its permitted single
  retry succeeded. No protected configuration/secret contents were inspected. Working
  source remains main4257cb0 with uncommitted monitoring records in WORK_LOG.md.

## 2026-10-27 — diagnose feed Git HTTP403 without replaying publication

- Owner asked where the403 originates and how to fix it. Traced publisher writer_token,
  shared checked_push, status_update caller, protected workflow environment, controller
  publicationResult/FeedProof, and existing worker boundary tests. Failure is GitHub HTTPS
  Git receive authorization, not OpenWrt compilation/signing or local webhook networking.
- Read-only GitHub checks: App5424800/voc-experiments-ci-coordinator and installation
  174142534 have contents:write/actions:write/metadata:read; installation not suspended.
  Environment FEED_WRITER_APP_ID/INSTALLATION_ID match. Feed repository ID1429576922,
  branch main, configured correctly. No ruleset and main has no branch protection.
  Browser installation page confirms packages is explicitly selected (six selected repos).
  No App permissions, repository selections, secrets or protection settings were changed.
- CLI installation-repositories query lacked read:user; did not expand OAuth scopes.
  Used browser read-only installation inspection instead. Located existing VOC-Experiments
  writer PEM filename under ci-test; no .local-credentials or SNodeC key was inspected.
- One read-only credential probe at09:12:37UTC used the existing sandbox writer key via
  openssl signing without displaying key contents. Minted a token restricted to1429576922
  and contents:write; returned201. Same-token repository REST read200; Git receive-pack
  advertisement GET200. Diagnostic token revoked204. No git push/update request sent.
  REST quota remaining4999 at this current probe; this does not establish quota at the
  earlier failed push. Repository role booleans were false for the installation token;
  they are not evidence overriding granted token permissions or successful Git discovery.
- Only allowlisted response status/timing/request-ID/quota fields were recorded in
  ui-origin/live/authorization-probe.json. No token/header/key values logged. This probe
  is not a successful publication or proof of write-phase success on the failed runner.
- Exact cause of the intermittent failed push remains unverified. The old job did not
  retain HTTP response/request-ID details. Do not claim a permanent permission/key fault,
  rate-limit cause, token-propagation race or a proven code fix from this evidence.
- Official references:
  https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/authenticating-as-a-github-app-installation
  (Contents permission, scoped installation tokens and Git HTTP password authentication).
  https://docs.github.com/en/rest/using-the-rest-api/troubleshooting-the-rest-api
  (403/rate-limit diagnostic headers and permission troubleshooting).
- No production/test change, mutation replay or new recovery policy. A bounded change
  to shared checked_push could retain only sanitized Git HTTP status/request ID and
  granted token scope for future failures, preserving the existing uncertainty rule.
  Such diagnostics require a concrete design/growth approval before implementation;
  they are diagnostic work, not a claimed root-cause fix or recovery of the current attempt.


## 2026-10-27 — Deploy sandbox controller on core.vchrist.at

Owner requested deployment after the laptop left its former LAN and authorized
`root@core.vchrist.at` SSH. No application production or test source changed.
Canonical checkout remains `/home/voc/projects/snodec/core-ci`; deployment records are on
`deploy/core-vchrist`, based on local main `4257cb0`. Existing uncommitted monitoring
records were preserved. No commit, push or merge performed in this deployment task.

Installed the previously focused-tested binary containing changes through `47737f7`
and its 20 matching installed SNode.C runtime libraries under `/srv/core-ci`.
The server's existing framework lacks MariaDBClient::connect(); the private runtime
avoids changing framework packages used by its existing services. Manifest and SHA-256
checks passed; `ldd -r` has no unresolved symbols. No extra source checkout was created.
Installed MariaDB server/client (13 new packages, zero upgrades), created dedicated
`core_ci` UNIX/database accounts with socket authentication and application DML grants.
Application database mode is unmanaged; systemd owns MariaDB. Controller App/HMAC
credentials only were transferred privately; no feed-writer or signing keys transferred.

Invariant: exactly one active controller owns graph mutations. Saved consistent logical
backup, verified source graph unchanged at generation 865, stopped laptop PID 1553769,
restored database and enabled/started `core-ci.service` as core_ci (PID 203617 at check).
All 120 execution records and graph JSON matched exactly before/after cutover and after
real GitHub deliveries: 74 published, 17 failed, 22 waiting, 6 building, 1 publishing.
The uncertain APK publication remains assigned; no retry or replacement dispatched.
Backup retained privately on server `/var/backups/core-ci/cutover-generation-865.sql`.
Local backup/evidence/runtime artifacts remain under
`/tmp/codex/build/core-ci/deploy-core-vchrist`; remove after a verified operational backup.
Temporary database client password file was removed.

UI binds 127.0.0.1:8080; TLS binds IPv4 0.0.0.0:8443. Existing certificate SAN covers
core.vchrist.at, passed public HTTPS verification, expires 2027-01-16. systemd LoadCredential
supplies certificate/key without widening original permissions; restart after renewal.
App webhook URL and worker CORE_CI_STATUS_URL now use core.vchrist.at:8443.
Real GitHub redelivery 3850644243670376448 returned 202 in 1.08 s; fresh workflow_run
3850644256886652928 returned 202 in 0.91 s and workflow_job 3850644258371403776 in 1.01 s.
Invalid authentication returned 401; public /api/graph returned 404. Public status generation
865 matched durable state. Service enabled/active; MariaDB and both existing IdP services
active. Their original PIDs 96899/165328 unchanged, as were ports 80/443.
No production SNodeC GitHub resources, tags, feed bytes or trusted keys were changed.
.qtcreator and .local-credentials preserved. Management access uses an SSH tunnel.

Verification: runtime hashes/linking; database exact-state comparison; live TLS and GitHub
reports; service/socket checks; git diff --check. No unrelated test suite run and no
new package build session requested. Existing feed push 403 remains unresolved; deployment
does not establish successful publication or complete current v2.1.1 graph acceptance.


## 2026-10-27 — Owner-authorized public management test listener

Owner explicitly requested wildcard management binding on core.vchrist.at for initial tests,
with management TLS/authorization deferred. Changed only installed core-ci.service CLI
`web local --host 127.0.0.1 --port 8080` to `--host 0.0.0.0`, reloaded systemd and restarted.
Verified active service, 0.0.0.0:8080 listening, public HTTP 200 at
http://core.vchrist.at:8080/ and byte-identical graph before/after restart. No application
source or test changes. Existing webhook TLS on 8443 unchanged. Updated sandbox instructions.

## 2026-10-27 — Post-deployment CI inspection

Read-only GitHub checks across demo-core/app/side/join, ci-coordinator and packages found
zero queued or in-progress runs. Remote core-ci PID 203772 is active; graph remains generation
865 with all 120 records. Six building-state executions already have successful completed
build evidence and await publication; the uncertain demo-app APK r113 publisher
40465675752 remains assigned. Startup reconciliation explicitly preserved it because the
feed receipt commit is inaccessible. Latest status refresh 40487164424 succeeded against
the new host. No retries, manual advances or new selections performed. The current graph
session is blocked, not actively building; remote migration did not resolve the feed 403.

## 2026-10-27 — Explicit owner recovery and fresh-session preparation

Owner requested collecting shortcomings, performing recovery and starting a fresh full graph
from core, with detailed monitoring for the intermittent feed HTTP 403. Added
`docs/improvements/README.md` with verified gaps, proposals, accepted limits and deployment
gates; it grants no production-code expansion. No application or test source changed.

Recovered r113 after independently reading the completed attempt-one run/jobs and raw job
log: final git push rejected by GitHub with 403; no publisher remains. Full feed tree was
untruncated and contained no receipt/export/package path for r113; receipt path had zero
commits. All three feed commits since the failure touched status projections only. Full
pre-recovery database backup is /var/backups/core-ci/before-r113-recovery.sql. With service
stopped, a generation- and assignment-conditional transaction changed only r113 to failed
with an explicit owner-recovery reason and incremented generation. Assignment, build result,
GitHub evidence and historical records were preserved. Protected recovery evidence with
job-log SHA-256 is /var/backups/core-ci/r113-recovery-evidence.json.

The first administrative transaction mistakenly added an operator_recovery evidence phase.
Startup validation correctly rejected that unknown phase before listeners or dispatch.
Removed only that added field and incremented generation; audit remains in the protected
file and failure reason. This was an operator error, not a framework defect. Service then
restored successfully at generation 867 and automatically assigned existing side APK r118
publication 40488462573. No direct GitHub rerun or uncertain mutation replay was used.

First source request was rejected because startup advancement changed graph generation
during selection (no source selection committed). Repeated explicit request for demo-core
v2.1.1 returned HTTP 200, selected=0, assigned=0: it reuses existing work. Asked owner for
new sandbox tag choice, recommending v2.1.2 at unchanged commit
1cfcfa17de500bb68f389bf6b8c4cbf27ddcbb57; no new tag or fresh-session claim before answer.
Existing resumed publication is being monitored; no recurrence/cause resolution claimed.

Resumed side APK publication 40488462573 succeeded: privileged job 122137032372
prepared at 10:29:56Z and verified feed revision a7eb3f25525eefe172cc616f4bb961824d489d03
at 10:30:19Z. No Git HTTP403 in its push. Controller marked published and automatically
assigned app/IPK r112 publication 40489014054. This is resumed work, not a fresh full graph.

Applied owner's explicit APK retry through /api/work/retry after recovery: new rebuild r124,
app build https://github.com/VOC-Experiments/demo-app/actions/runs/40489115089. Dispatch
response initially preserved assignment with identity mismatch; subsequent authenticated
webhook bound the correct attempt-one run and graph reports build running. No dispatch replay.
Cause of initial response mismatch is unverified and requires separate investigation.

Read-only observation process runs locally from task directory monitor-ci.py for at most
45 minutes, polling graph every20s and downloading each new completed assigned publisher
job log once. Results in recovery-session-monitor.jsonl and monitored-publisher-*.log;
no controller instrumentation, secrets printed, mutation retry, or new application threads.
Fresh full-session source-tag choice remains pending; v2.1.1 selection returned selected=0.

## 2026-10-27 — Resumed session monitoring completed

Owner requested continued monitoring/reporting. Graph progressed automatically to generation
1060 with all20 latest session nodes (root/dependent revisions104–123, APK retry124 replacing
failed113) published. The15 remaining waiting records are all superseded historical work.
The17 current failed records are historical; no new terminal failure in resumed work.

Read-only monitor captured14 completed publisher logs after recovery, all success and no
Git push403 diagnostic. App APK retry124: build40489115089 and publication40489521812 both
success; publisher completed10:37:15Z. Final join/DEB amd64 r119: build40492709653 and
publication40492796133 success. Final publisher prepared11:01:08Z and verified feed revision
b730035d72ac981499b5a5b3b886b12da66b0b59 at11:02:21Z; controller reached published1060
shortly afterwards. Public status refresh40493134880 succeeded11:02:58Z.

Live controller /public/status, Pages status/controller.json and status.json all reached
1060. Read all20 public receipt JSON documents and matched recorded content, accounting for
controller-added feed_revision/publication_verified_utc fields. Read all4 project HTML
pages; no stale build-assigned, publication-running or publication-queued labels remained.
Service and MariaDB active. No application source changes or unrelated tests run.

This proves completion of the resumed session with explicit APK retry and administrative
recovery, not an uninterrupted first-pass session, a fresh v2.1.2 graph, or a root-cause fix
for403. No new tags created; fresh-session tag choice still awaits owner answer. Evidence
remains in /tmp/codex/build/core-ci/deploy-core-vchrist including monitor JSONL and publisher
logs. No commit/push/merge in this monitoring task.

## 2026-10-27 — Owner-requested fresh full graph from core v2.1.2

Owner repeated explicit instruction to start a complete fresh chain after prior tag-choice
proposal. Created only sandbox demo-core refs/tags/v2.1.2 at unchanged source commit
1cfcfa17de500bb68f389bf6b8c4cbf27ddcbb57; existing tags unchanged. Initial gh credentials
lacked workflow scope (404). A contents-only temporary App token was insufficient (403).
Confirmed existing controller installation already grants workflows:write, then created
using a temporary token restricted to demo-core with contents/workflows write (201) and
revoked it (204). This403 was a tag-creation permission denial, separate from the unresolved
intermittent publication Git push403; no uncertain publication mutation was replayed.

Real tag webhook selected20 new graph executions (120→140), with five core builds and15
waiting dependents; no injected event, selection SQL or manual advance. Source build runs:
DEB amd64 40493669675 (r125), DEB arm64 40493681172 (r126), RPM 40493677166 (r127),
OpenWrt IPK 40493673401 (r128), APK 40493665208 (r129). At generation1082 amd64/RPM GitHub
builds succeeded, other three in progress; amd64 publication assigned automatically.
One separate source-owned tag workflow40493622829 is red; not an assigned graph build.
Its cause is not yet investigated and must not be confused with one of the five core jobs.

Restarted read-only45-minute monitor in task directory, retaining already-seen publisher IDs
to avoid re-downloading prior14 logs. Evidence fresh-v2.1.2-graph.json and
fresh-v2.1.2-monitor.out. No controller/worker source, global credentials, production SNodeC
resources, existing tags or signing keys changed. No full-session success claim at launch.


Fresh session observation11:14:35Z: core APK r129 build11:06:51–11:09:15 (144s),
publication validation11:11:17–11:13:11 (114s; metadata/provenance step98s), privileged
publisher starts11:13:15, installs tools20s, prepares/signs11:13:39–11:13:54 (15s),
push/HTTPS step starts11:13:54. Thus UI's12:06 local timestamp is build start, not
publisher start. Traced SDK prepare in both validate.py and sign_openwrt.py; each performs
SDK download/checksum/extraction/preparation. Feed writer exclusion covers the whole
publication workflow, including validation. Recorded cost in improvement register; no
optimization or weakening of publisher verification implemented.

Owner requires millisecond controller reaction and asked for analysis-only report of delay.
Traced Graph::publicationReady and Pipeline::advance: controller withholds every further
publication for same feed until previous state leaves publishing. GitHub workflow also
holds same feed concurrency group across validate and publish; status writers share that
resource. Thus minutes are spent on GitHub, but coarse controller scheduling contributes
to waiting. Do not misattribute all queue delay solely to GitHub infrastructure.
Measured server-local full graph GET119–160ms for140 records; not a measurement of local
eligibility computation/commit latency. Controller CPU117s over3109s wall (3.7%, RSS~82MB),
not evidence of sustained CPU saturation. Sample APK workflow finished11:14:40Z, next app
RPM run created11:14:53Z:13s end-to-end gap includes delivery, proof/HTTPS and GitHub dispatch;
no claim that this is pure CPU time or exact graph commit latency. No production code change.
Latest observation11:19:07Z fresh chain4published,1publishing,6build-stage,9waiting;
Pages1171 vs controller1180. No new failure observed.

## 2026-10-27 — Claude read-only controller-parallelism review

Owner requested Claude investigate current core-ci architecture and maximize eligible
parallel GitHub work. Ran installed Claude CLI in safe plan mode with only Read/Grep/Glob,
no MCP or command tools, no production changes. Request is
`docs/proposals/controller-parallelism-review-request.md`; returned report is
`docs/proposals/controller-parallelism-review.md`. CLI completed successfully, stderr empty.

Confirmed central finding against source: controller reserves the entire publication
workflow, including independent artifact validation and final HTTPS/controller proof, for
one feed-wide publishing node; GitHub workflow-wide concurrency repeats the broad lock.
advance selects one publisher in hash-ID map order. Independent eligible builds do already
fan out. HttpClient's one-in-flight FIFO and Connection:close are verified secondary
serialization; per-origin refactor remains deferred, not automatically approved.
onChanged builds a full graph view and initiates status refresh before execution continuation;
confirmed this even with no SSE receivers. CAS lost-update/stalled-advancement finding is
not reproduced; must test before treating every review assertion as verified.

Review recommends removing controller-wide publication exclusion, job-level feed locking,
unlocked HTTPS confirmation, reduced derivative status work, and focused conflict handling.
Important explicit decision: allowing independent new writers while an earlier publication's
outcome remains unknown changes the current conservative feed policy; no blanket approval
or implementation inferred. The report offers a conservative alternative. A smaller
critical section must still read current feed within lock, regenerate retained-package
indexes, sign, commit and checked push; never force/rebase or replay uncertainty.

Do not adopt proposed diagnostic growth/extra signing job by default. Estimates in advisory
report are not owner approval. No new persisted states, queue, scheduler or threads proposed.
Verified live packages Pages build_type=workflow, so report's branch-based build-limit
hypothesis is not an established explanation for this deployment. Report and request remain
uncommitted on deploy/core-vchrist with prior deployment docs preserved.

## 2026-10-27 — Detailed independent validation of Claude parallelism review

Owner requested all-detail validation and contradictions, then strengthened the objective
to overlap every independent operation. Full result:
`docs/proposals/controller-parallelism-review-validation.md`. No production implementation,
GitHub mutation, deployment change, commit or push during this validation.

Confirmed feed/HTTP gates and status callback ordering against current source. Corrected
recommendation contradictions: deleting feed exclusion changes uncertainty policy; state-only
status refresh loses public queued/running/completed facts; third confirm job needs exact
receipt/revision handoff and secret-free entry flow; CP2 leaves tools/SDK work locked;
callback reordering cannot guarantee network POST priority across async preflights; hash/order
still exists after removing one gate. Core IPK wait example was eight completed publications,
not roughly four five-minute jobs. New publication ref alone fails sandbox main-only
environment branch policy and does not solve mutable tooling variable/past-assignment pins.

Official concurrency docs confirmed queue:max supports100pending, cancels overflow, and
FIFO is waiting-group arrival rather than dispatch order:
https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency
Pages custom Actions exception confirmed against official docs and actual build_type=workflow:
https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits
These contradict blanket FIFO/no-cancel and branch-based ten-build-limit explanations.

Focused temporary diagnostic compiled current public GraphStore/Database/Graph plus existing
GraphFixtures using installed snodec::db-mariadb, nlohmann_json and OpenSSL. Isolated real
MariaDB, synthetic inputs; no live injected records. It reproduced both the definite CAS
failure leaving an independent eligible node waiting and stale built-Node retry rejected
for evidence regression with missing build result. Re-derivation from current graph passed.
This verifies F4's component mechanism, not occurrence or causal importance in live pipeline;
remote live journal since recovery contained zero matching conditional/evidence/503 errors.

Probe path /tmp/codex/build/core-ci/parallelism-validation; commands and outcomes recorded in
validation doc; result.json retained, fixture database stopped/removed. Initial diagnostic
compile missing Graph::select's third argument was corrected before passing execution.
No existing tests modified/weakened, no unrelated suite run. Production LOC0; repository
test LOC0. Temporary diagnostic sources and documentation only. Existing uncommitted work,
.qtcreator and .local-credentials preserved. git diff --check passed.

## 2026-10-27 — Mutual review agreement with Claude

Owner requested sending the independent validation back to Claude and continuing until
both reviewers agreed. Completed three follow-up exchanges through the installed Claude
CLI, safe plan mode with Read/Grep/Glob only, no MCP/command/write tools. Every invocation
exited successfully with empty stderr. Current reviewed source remains 4257cb0 on
deploy/core-vchrist. No production changes or existing test changes during the discussion.

Joint result: docs/proposals/controller-parallelism-review-agreement.md.
Exact outgoing requests and returned replies:
docs/proposals/controller-parallelism-review-discussion.md. Earlier advisory claims marked
superseded; the original report is preserved. Both reviewers explicitly agree on diagnosis,
constraints, preferred reduction-first direction and the enumerated open owner/engineering
gates. This is not an implementation-complete design, owner approval or measured speedup.

Claude accepted C1-C11 with corrected cold/shared approval wording. Additional agreed
findings: pre-POST generation equality can starve status dispatch; status and publication
jobs retain independent setup inside their feed locks; repeated enrollment SSE reads are
unnecessary. Conditional F4 overlap is deterministic when the earlier write advances the
generation, but live occurrence/frequency remains unmeasured. No timing bounds inferred
from Pages coalescing, GET response time or absence of live conflict errors.

Preferred F4 direction reshapes the existing Database FIFO into lazy graph derivation,
replacing eager stale transitions and retries instead of adding a GraphStore queue. Apply
and generation increment must precede the next derivation; execution continuation must
precede its derivative onChanged. Callback ordering/reentrancy audit remains an engineering
gate. Do not rederive/replay after uncertain database mutation. Keep explicit selection
and enrollment guards: selected definitions also reserve namespace claims, so an enrollment
epoch alone is insufficient. Claude withdrew that epoch, unbounded HTTP concurrency,
universal 99 admission claim and unconditional rollout-order/historical-retry assertions.

Agreed HTTPS direction retains normal-time liveness outside the writer lock through a
separately specified secret-free confirm job. Prepare computes bounded public receipt hash
and commit SHA; needs:publish success supplies the execution condition, not a duplicate
pushed flag. SHA is context; controller independently verifies committed receipt and HTTPS.
Failure/cancellation/lost delivery retains uncertainty and existing recovery. Official
job-dependency semantics checked:
https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idneeds

Remaining decisions are explicit: future-admission policy after unknown push, bounded
capacity, confirmation mechanism, privileged preparation/trust/replay handoff, old/in-flight
pin compatibility, precise checkpoint/LOC approval. Same-origin concurrency is separate
from the deferred per-origin refactor. No new timer, controller thread, scheduler, ready
queue, instrumentation/cache/latch or callback protocol adopted by this review.

No extra suite or live mutation was run for the discussion; prior isolated-MariaDB probe
evidence remains labelled local. Existing uncommitted deployment docs/private directories
preserved. Production LOC0; test LOC0. Documentation only, uncommitted/unpushed; no restart,
installation or deployment. Temporary dialogue inputs/outputs retained under
/tmp/codex/build/core-ci/parallelism-validation/ for review with the earlier probe.

## 2026-10-27 — Parallelism refinement task prompt

Owner requested a refinement prompt from the joint review conclusions. Created
docs/proposals/controller-parallelism-refinement-task.md. It requests a concrete design,
not implementation: lazy graph derivation through the existing Database FIFO, bounded
same-origin overlap, progress-preserving status/SSE work, explicit writer-admission policy,
secret-free HTTPS confirmation outside the write lock, and pinned-history rollout design.
Requires exact callbacks/handoffs, all affected consumers, coherent checkpoint/deletion
accounting, separate production/test estimates, focused tests and timed sandbox acceptance.
Withdrawn review suggestions remain excluded. Owner decisions and framework engineering
questions are separated; no code-growth or policy approval inferred from reviewer agreement.

Verified canonical root, origin SNodeC/core-ci, origin/HEAD main, current deploy/core-vchrist
at 4257cb0; read-only GitHub PR listing returned no open PRs. Existing dirty documentation
preserved. Prompt leaves the shared branch and deployment unchanged, requires refinement
to stop before implementation, and retains existing sandbox authority for later stages.
No production/test changes, new diagnostics, Claude invocation, deployment, restart,
commit or push for prompt creation. Documentation whitespace/link checks recorded locally.

## 2026-10-27 — Implementation-ready parallelism refinement

Executed the owner's design-only refinement task. Proposal:
docs/proposals/controller-parallelism-design.md. Canonical checkout remains
/home/voc/projects/snodec/core-ci, deploy/core-vchrist at 4257cb0; origin SNodeC/core-ci,
default main; read-only open-PR listing empty. Existing dirty documentation preserved.

Traced current Graph/GraphStore/Database, all commit/enroll/finish callers, source selection,
GitHub token/proof coalescing, report/artifact/feed completion, main SSE and startup cleanup,
worker publish/status/build workflows and signing/SDK consumers. Consulted installed public
SNode.C headers and relevant framework HTTP/database apps plus MQTTSuite mqttcli/mqttstore
ownership/configuration patterns. No framework source or application production edits.

Concrete A: lazy SQL/change production through the existing FIFO; applied graph before next
derivation; execution continuation before derivative notification; bounded-stack synchronous
no-op drain. Important refinement: read callbacks retain release-before-callback because
initial schema/restore failure can destroy wrappers; write callbacks use apply/continue/
notify before release. Preserve guards and distinguish transient preparation rejection from
SQL failure. No second queue or enrollment epoch. Status uses approved minimum generation;
SSE avoids receiver-free views and execution-only repository reads.

Concrete B: bounded same-origin overlap in the existing transport deque, actual connection-ID
correlation from per-flow success/onConnect ordering, callback ownership moved once, closing
transports keep their slots, origin changes drain first. Final flow termination distinguishes
multi-address connection establishment from intermediate errors. Framework connector shutdown
may omit status callbacks; no promise of result delivery across shutdown or destructor calls
into an already destroyed Pipeline. Assigned work recovers through existing restart reconcile.
Recommend per-instance max-in-flight 8 (range 1–16), not platform maximum or approved value.

Concrete C: graph-owned publication blockers/batch reservations, recommended P1 admission 8
per feed (range 1–32), current-head shared-write job, public hash/SHA handoff to a secret-free
confirm job and independent controller proof. P2 remains an explicit alternative future halt.
Residual privileged setup serialization and further split/environment options are visible,
not described as full maximal parallelism. Recommend retaining O/N exact deployed workflow
paths with separate fixed tooling pins to preserve historical retry; no global pin rewrite
or generic compatibility layer. Include rollout/rollback table and old restore limitation.

Created only a temporary object compile under
/tmp/codex/build/core-ci/parallelism-refinement/ against installed exported snodec::http-client
and snodec::net-in-stream-tls targets. Confirmed independent FlowHandle return, termination
hook, MasterRequest connection ID and per-instance configuration registration. First build
failed on ambiguous unqualified configuration members; qualifying ConfigInstance fixed it.
Final build passed, including the added flow-termination type check. No network, executable
run, database fixture, production test/suite or live mutation for this compile. Prior isolated
MariaDB conflict-probe result still available and remains component/synthetic evidence.

Official references verified for capacity, dependencies/outputs, REST concurrency and Pages:
https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency
https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax
https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api
https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits
No platform latency/order/cancellation guarantee inferred beyond those documents.

Planning central estimates: canonical production added 535 / removed 290 / net +245; tests
added 705 / removed 85 / net +620. Proposed approval bounds A/B/C total +310 production/+790 tests;
separate sandbox wrapper/script deployment bound +450 net production, with mirrored source
accounting per repo. These are estimates requiring approval, not actual changes or permission.
Further privileged setup split outside bounds. Proposed timing targets and focused boundary/
sandbox acceptance are untested; request accounting distinguishes evidence, token, dispatch,
status, worker and HTTPS work. Remaining owner choices stated with recommendations.

Actual production LOC 0; existing test LOC 0. Only proposal and this log changed for the task;
temporary compile evidence retained for review. No commit/push/merge, config change, restart,
trigger, deployment or SNodeC production resource change. Private directories not inspected.
Future checkpoint documentation estimate: added275/removed100/net+175, separate from the
current design artifact. Successful diagnostic commands and cleanup condition are in the
proposal. Final documentation whitespace/fence/link checks and git diff --check passed.

## 2026-10-27 — approved controller/workflow parallelism implementation

- Owner approved all four refined choices and implementation bounds: P1 independent publication
  admission while uncertainty remains assigned; eight HTTP operations per client and eight
  publication assignments per feed; secret-free HTTPS confirmation job; two retained approved
  tooling/workflow generations. Bounds: +310 net canonical production, +790 test lines;
  later sandbox copies/aliases have a separate +450 net production bound. This does not
  authorize further privileged setup splitting or commit/push/merge/install stages.
- Canonical checkout `/home/voc/projects/snodec/core-ci`; implementation branch
  `fix/controller-parallelism`, based on local main `4257cb0fc67412da8696afdba307fc2e85ff64aa`.
  Existing deployment/review documentation remains preserved and uncommitted. No extra checkout.
- Invariant: overlap independent transport and execution work; derive each shared graph mutation
  once against applied state at the front of the existing Database FIFO; apply commit/generation
  before the next derivation; run execution continuation before derivative status/SSE work.
  Dispatch still follows persisted authority. No second graph queue, scheduler, timer, schema,
  persisted state or controller thread was added.
- Replaced eager graph changes/stale-node continuation retries with lazy Database producers and
  GraphStore derivations. Selection/enrollment generation and namespace guards remain explicit.
  Preparation failures/no-ops do not open transactions. SQL uncertainty still closes mutation
  admission. Reentrant synchronous completion drains iteratively; query startup callbacks retain
  their resource-release ordering. All write completion paths were traced through Database.
- Verified installed SNode.C client callbacks and FlowHandle interfaces against framework source
  and HTTP/database apps; also checked MQTTSuite lifecycle/ownership patterns (references in the
  design proposal). Connection status precedes onConnect; an HTTP disconnect clears the request
  socket context before its callback. A first runtime test exposed an incorrect socket lookup
  there; corrected correlation to the retained MasterRequest identity, then reran the affected
  transport/startup/integration checks successfully. This was a local test defect/fix cycle,
  not a deployed failure. Intermediate address errors wait for actual flow termination.
- HTTP uses the existing waiting list with bounded same-origin overlap. Response callbacks are
  correlated by connection/request identity, delivered once, and transports retain capacity until
  disconnect. Origin changes wait for active transports to drain; the deferred per-origin client
  refactor was not introduced. Existing scoped token expiry/coalescing and mutation non-replay
  remain. Independent source reads aggregate once in enrollment order; an unreadable newly
  selected repository still blocks the request. Public options register before framework init:
  `github-http --max-in-flight` and `feed-http --max-in-flight` default 8, range 1–16.
- Removed status approval's obsolete generation-equality restart before POST; a generation is
  a minimum and the worker reads live facts. Newer-generation handling and uncertainty are kept.
  Full SSE graph views are constructed only for receivers; repository-list SSE reads follow
  successful enrollment changes, rather than every evidence commit. Same-state progress remains.
- Graph publication admission/validation/blockers use one capacity calculation. Default
  `pipeline --publication-limit 8`, range 1–32. Uncertain assignments consume capacity but do not
  block independent nodes below the bound or satisfy their prerequisites. Restore accepts
  already valid assignments even when the configured admission limit is lower.
- Canonical worker templates move only the feed-mutating publication job into the shared feed
  concurrency group, also used by status. Validation stays outside it. Prepare exports the
  exact staged receipt SHA-256 and local commit SHA. The push step retains checked update and
  its failure-certainty boundary. A separate secret-free confirm job, `needs: publish`, validates
  mandatory public outputs/actor/executing attempt/tooling and bounded HTTPS receipt bytes.
  It has no publication environment/signing/feed token, and does not match the publisher name.
  Controller committed-receipt/HTTPS proof remains authoritative; post-push failure is uncertain.
- New publisher/status templates have a deployment-rendered literal `@TOOLING_SHA@` marker;
  it must be replaced by the reviewed tooling commit in a subsequent wrapper commit before
  installation. The deployment guide specifies O/N aliases without overwriting old wrappers or
  their global pin. No actual aliases or hashes were fabricated/installed. Historical retry
  retains its original immutable deployment. Old-controller rollback requires assignments to
  drain to its supported shape. No generic compatibility implementation was added.
- Focused local proof (synthetic GitHub, real isolated MariaDB/TLS/local Git/package tooling):
  Debug configure/build succeeded under `/tmp/codex/build/core-ci/controller-parallelism/build`,
  compiler temporary files under the sibling `tmp/`. `format-check` and `git diff --check` pass.
  CTest selections: lifecycle help/initial-failure/schema-failure; graph-state-boundaries;
  enrollment-boundaries (includes GraphStore real-MariaDB harness); github-transport-*;
  pipeline-boundaries; worker-provenance-boundaries; worker-package-boundaries. Every selected
  boundary passed after correcting old one-writer assertions and letting preceding derivative
  status work finish before a zero-traffic measurement. No unrelated/full suite was run.
- Added public-boundary regressions for overlapping graph commits/evidence and independent
  advancement, 4096 reentrant no-ops plus 4096 invalid derivations, eight/ninth publication
  admission, lower-limit restore, O/N historical retry identity, twelve overlapping TLS requests
  with callback correlation/capacity/token coalescing, independent publication while another is
  uncertain, prepare output hashes, confirm hash/origin rejection and executing attempt rejection.
  Real local DEB/RPM signing/replay checks remain passing; these are not live GitHub acceptance.
- Latest synthetic pipeline measurement: eight handled unchanged signed notifications at 30 ms
  API fixture delay produced zero HTTP/token/status requests in 0.0672 s (old investigation
  baseline: 112 requests/40 Git-object reads/4.515 s). This zero-request property already existed
  before this parallelism change and is preserved. Graph/SSE progress took 0.0919 s with 0.300 s
  derivative API delay. Complex synthetic session totals include failures/retries/restarts and
  status: 25 token mints, 44 evidence, 104 source/edit, 81 worker approval, 8 build dispatch,
  3 publication dispatch, 58 status dispatch, 18 feed-GitHub, 2 feed-HTTPS = 343 requests.
  These are not a normal 20-node live-session budget or live latency evidence.
- Logs and request/latency accounting remain under the permitted temporary task directory.
  No live controller restart, settings/listener/port change, GitHub mutation, tag/key/feed/package
  change or new live acceptance occurred. `.qtcreator/` and `.local-credentials/` were preserved
  without inspection. Deferred renderer/403/service-management/production rollout work is untouched.
- Remaining gates: review/authorize commit and later push/merge stages; deploy reviewed N tooling
  and rendered aliases within sandbox authority, capture assignments/back up and restore before
  restart, verify mixed O/N dispatch and actual feed lock exclusion, exact public receipt and
  graph/SSE/Pages timings, retained bytes/signatures and affected real package-client checks.
  No end-to-end live speedup or completed rollout is claimed here.
- Tracked diff accounting against HEAD: production +652/-452 (net +200); tests +398/-116 (net +282). Both implementation nets are within the approved bounds.
  Documentation is separately modified and includes preserved pre-existing deployment/review work; untracked proposal/review files are preserved, not counted as production or tests.
- Owner subsequently authorized committing/pushing this task on its separate branch and sending Claude a detailed agreement-based review request. The staged documentation includes the task review/design evidence; unrelated deployment documentation and private/untracked files remain outside the commit. No merge or installation is authorized by that instruction.

## 2026-10-27 — Claude implementation review returned

- Owner requested waiting for the detailed review of pushed branch `fix/controller-parallelism`, commit `63ab7b2`. Claude completed successfully in read-only plan mode (Read/Grep/Glob; no command/write/MCP tools); stderr was empty. Its report is recorded in `docs/proposals/controller-parallelism-implementation-review.md`.
- Claude reports no disagreement with the agreed mechanisms and no definite blocking defect in the core concurrency mechanisms. It flags M1 conflicting O/N installation instructions; H1 potentially stranded disabled-client operations (needs probe); T1 missing focused/live coverage; L1 phase-required arguments, L2 moved-function callback clearing, L3 shutdown error wording, and L4 response/uncertainty behavior changes. These remain reviewer findings requiring independent validation, not accepted facts or authorization for broader changes.
- No fixes, merge, restart or activation were performed while waiting. Review report/log additions remain uncommitted; pushed implementation remains `63ab7b2`.

## 2026-10-27 — independent code validation of Claude findings

- Owner requested investigation/report only, excluding documentation investigation. Kept
  production, existing tests/workflows and deployment unchanged at `63ab7b2` on
  `fix/controller-parallelism`. Preserved unrelated changes and private/untracked directories.
- Confirmed H1 against framework implementation and a local installed-library probe: disabling
  github-http leaves the operation pending without callback (observed callback count 0).
  Connector observation does not implicitly terminate a disabled flow. No separate normal-running
  cancellation defect was demonstrated; shutdown must be treated by its actual lifecycle.
- Confirmed L1 via mocked phase boundary: missing --feed reaches checked_push with current
  directory. No network, real token/signing or Git mutation. Argument requirements should
  follow use: feed for prepare/push, build-result/artifacts for prepare, neither for confirm.
- L2 is a real std::function move portability weakness, but installed libstdc++16 explicitly
  empties the moved source, so duplicate delivery is not reproduced here. Standard reference:
  https://eel.is/c++draft/func.wrap.func.con#6 . L3 is genuinely incorrect shutdown wording;
  submitted=false and conservative dispatch handling remain intact.
- L4 response metadata/status differences are source-confirmed; no broken UI consumer or
  assignment loss found. Repeated duplicates under DB uncertainty can cause unchanged SSE
  broadcasts, without SQL/evidence HTTP/status dispatch. Corrected Claude's external-503 claim:
  authenticated delivery already received 202; processing 503 is internal/logged.
- T1: confirmed missing dedicated origin/shutdown/multi-address/sustained-approval/forced
  end-to-end built-overlap tests. Existing concurrent report/MariaDB-lock and GraphStore overlap
  coverage is meaningful. Corrected the broad rollback-gap claim: stale generation and namespace
  errors exercise successful rollback; START and failed rollback need targeted failure injection.
  Lost successful COMMIT acknowledgement is already covered by the enrollment DB proxy.
- Temporary transport probes additionally show capacity 1 => peak 1 with 12 correlated callbacks,
  capacity 2 => peak 2, and invalid HTTP limits 0/17 rejected. Publication 0/33 are range-rejected;
  1/8/32 pass range validation before the isolated CLI probe stops for missing required database
  configuration. These diagnostics are not newly committed tests or live acceptance.
- I1 read callback ordering is unchanged from base and preserves startup destruction safety;
  no affected consumer found. Mixed-level GitHub lock exclusion, live status contention and
  response-to-close occupancy remain unverified measurements. Selection guard rejection and
  partial signing/setup serialization are known retained boundaries, not new code findings.
- Full code-only assessment and diagnostic inputs/results are under
  `/tmp/codex/build/core-ci/parallelism-findings/`. No documentation fixes, production edits,
  commits, pushes, live mutations, controller restart or broad test suite were performed.

## 2026-10-27 — agreement on every implementation-review correction

- Owner asked for each finding to be compared against proposal/implementation and for continued
  Claude discussion until fix disagreements were resolved. Completed two detailed read-only
  rounds and a final acknowledgment. First two Claude invocations used only Read/Grep/Glob;
  final acknowledgment had no tools. All exited successfully with empty stderr. No source,
  existing tests, workflows, configuration, deployment, commits or pushes changed in this task.
- Recorded full requests/responses in
  `docs/proposals/controller-parallelism-fix-discussion.md` and the concrete current plan in
  `docs/proposals/controller-parallelism-fix-agreement.md`. Source remains `63ab7b2` on
  `fix/controller-parallelism`; review records are uncommitted alongside preserved unrelated work.
- Both reviewers explicitly agree on all fix directions. H1: complete DISABLED requests once as
  definitely unsubmitted, preserve intermediate address-error handling. I accepted Claude's
  correction: framework startFlow defers establishment; the real disabled callback is asynchronous,
  so my earlier post-connect cleanup explanation was wrong. No new cancel/lifecycle mechanism.
- L1: validate only actual phase inputs before work (feed for prepare/push; artifacts/result for
  prepare). L2: exchange callback with nullptr. L3: correct unsubmitted shutdown wording. L4(a):
  retain lazy derivation; use existing PipelineResult as a value-captured preparation-error
  response, restore actual caller classifications/preservation context and uncertain:false,
  remove obsolete eager-era catches. No invented no-op identifier guarantee.
- L4(b): keep conservative rejection of no-op commit paths while DB authority is uncertain,
  including internal503/reconcile503 and failed-mutation SSE. External webhook ACK stays202.
  No eager duplicate path/cache/flag. This observable difference is recorded, not a claim of
  byte-identical behavior or owner waiver. L4(c)/I1 require no changes. M1 retains its already
  identified minimal O/N wording correction only; no further deployment-documentation audit.
- Corrected coverage claims: token completion already exercises callback-enqueued HTTP work;
  successful rollback and pipeline concurrent reports are already covered. Focused reachable
  gaps have a concrete test plan, including disabled/capacity/range inputs, malformed phases,
  sustained approval progress, shutdown, and targeted START/failed-rollback proxy failures.
  START failure is definite/unlatched; failed rollback latches uncertainty and blocks producers.
  Actual wire interception must be counted; close/restart cleans the deliberately open test
  transaction and must explicitly verify no revision leaked.
- Design-listed active origin switch/orphan callbacks are not runtime-tested; recorded source
  constraints do not count as coverage or owner waivers. Multi-address test cannot rely on
  dual-stack localhost because tls::in is AF_INET. Namespace fixture/connect stall/failed-rollback
  wire text/feed-disabled/artifact-overlap/sanitizer feasibility remain verification work.
- Claude's final reply: "I have no remaining fix-direction disagreement." Live mixed O/N mutex
  proof, capacity rollout, timing targets, signature/client acceptance and any remaining owner
  disposition remain separate. No reviewer agreement expands approval or claims deployment.

## 2026-10-27 — Claude-authored correction implementation prompt

- Owner asked to send Claude the agreed correction plan and obtain the prompt to implement all
  corrections. Sent the complete agreement/final acknowledgment, actual branch/checkpoint,
  existing approvals and line headroom, private-directory restrictions and all test dispositions.
- Claude drafted the prompt without tools. I requested one revision to remove conflicting
  whole-file preservation versus task edits, redundant baseline/micro-step retesting, blanket
  stops on unrelated uncertainty, equal-length SQL injection as a requirement, shutdown callback
  guarantees and confusion between local documentation commits and remote tooling/wrapper commits.
  Both Claude invocations exited successfully with empty stderr; neither had read/write/command
  tools. No source, existing test, workflow, runtime configuration or deployment changed.
- Saved the revised prompt in `docs/proposals/controller-parallelism-correction-task.md`.
  Two editorial clarifications explicitly preserve the already agreed uncertainty-capacity rule
  and the existing path classification for cumulative line accounting; no fix direction changed.
  Rechecked the committed figures versus `4257cb0`: production652/452 (net200), tests398/116
  (net282), leaving110/508 under approved310/790. Documentation is separate.
- Draft/request/revision artifacts remain under `/tmp/codex/build/core-ci/parallelism-findings/`.
  The task prompt grants no new authority; generating it did not start implementation or any
  commit/push/merge/install/restart/activation stage. HEAD remains `63ab7b2` on
  `fix/controller-parallelism`; new prompt/log records remain uncommitted. Existing unrelated
  changes and private/untracked directories were preserved without inspection.


### Controller parallelism correction implementation and focused verification (2026-10-27)

- Continued `fix/controller-parallelism` at HEAD/origin `63ab7b207f55afb5a71c7b2f54d76fa05ee7ebef`,
  against reviewed base4257cb0. Reproduced starting production652/452 (net200) and tests398/116
  (net282). Preserved pre-existing public documentation hunks, review records and improvements;
  private directories were not inspected or modified.
- Applied the agreed M1/H1/L1/L2/L3/L4a corrections. Removed two obsolete eager-dispatch catches,
  retained synchronous identity/metadata/current-attempt validation, captured preparation responses
  by value, and preserved caller409/503 classifications plus assignment context. No new production
  mechanism, state, retry path, queue or lifecycle was added. L4b/L4c/I1 remain unchanged.
- Observable L4b behavior remains internal503/uncertain:true and failed-mutation SSE for latched
  no-ops; authenticated webhook acknowledgement stays202. This is not byte-identical no-op behavior.
- Added reachable public-boundary checks for asynchronous disabled delivery, capacity1/invalid
  capacities, queued shutdown, disabled feed proof, phase validation, competing run binding under
  a MariaDB lock, delayed approval, TCP/TLS/response shutdown and restoration, forced artifact
  overlap, actual START/ROLLBACK wire faults and IPv4 fallback in a private mount/user namespace.
  Failed rollback closes on shutdown, permissions are restored, and no leaked graph revision is asserted.
- Corrected a pre-existing GraphTest local-variable name collision that prevented that target from
  rebuilding. Earlier stale-binary graph results are not validation; verified-final-build.log and
  verified-graph.log establish successful final compilation and graph boundaries. All other selected
  targets were rebuilt; transport and pipeline results use their rebuilt binaries.
- All27 selected checks ran:26 initially passed in CTest. Unmanaged-initial-failure exceeded the
  Linux107-byte Unix socket path limit; a shorter socket filename fixed the registered test. Final
  transport16/16, SQL fault2/2, startup3/3, pipeline and rebuilt graph checks passed after fixture edits.
  Canonical format-check, diff whitespace and <=160-character changed production checks pass.
  No unrelated/full suite was run. Commands, source references, caveats and logs are recorded in
  docs/proposals/controller-parallelism-corrections-results.md.
- Final local burst:8 unchanged notifications,0 HTTP requests,0 workflow reads,0.074241379 seconds
  under synthetic0.03s API delay. Extended failure/restart fixture trace475 requests is not a normal
  graph-run budget. No live performance, runner or Pages timing is claimed. Submitted shutdown
  currently retains an existing invalid-status error rather than a literal unknown-outcome string;
  it remains submitted/status0, while queued requests report definitely unsubmitted.
- No runtime claim for active-origin switching, late orphan onConnect, additional defensive
  preparation failures, latched no-op SSE under rollback or sanitizer memory safety. These remain
  precisely separated from reachable runtime checks in the report; no production test hooks added.
- Cumulative production697/464=+233; tests917/271=+646. Relative63ab7b2: production67/34=+33,
  tests534/170=+364. Remaining approved headroom77/144; separate sandbox+450 allowance unused.
  Build-input identity:63ab7b2 plus SHA256364d21faa549290a3532b3bf1fdb3db93d7b93d8fdc1a6b00acb81ab0169fd84.
- No commit, push, merge, install, live restart, activation or GitHub mutation performed. Mixed O/N
  feed-lock proof above capacity1, aliases, live timings and owner acceptance remain open. Existing
  deployment authorization was not revoked or replaced by a blanket activation gate.
- New task evidence stays under /tmp/codex/build/core-ci/parallelism-corrections/ for review; existing
  findings are preserved. No extra checkout/worktree was created. Temporary live fixtures clean up
  on exit; the reusable build and diagnostic logs are retained for handover.

- Owner subsequently authorized committing/pushing these corrections and requesting another detailed
  Claude review. Stage only correction code/tests, the M1 wording, this implementation entry,
  the correction agreement and verification report. Unrelated documentation remains unstaged;
  merge, installation and live activation are not included in this publication step.


### Controller parallelism re-review and main publication (2026-10-27)

- Claude completed a read-only detailed review of correction commit a28f61c and the cumulative
  implementation. It reported no confirmed production defect and five low-priority test-quality
  findings, retained in docs/proposals/controller-parallelism-corrections-review.md. Those findings
  are review observations, not newly implemented corrections or live acceptance.
- Owner explicitly authorized push and merge after receiving that review. Main was verified to
  be an ancestor of the reviewed branch, allowing a fast-forward publication without modifying
  the dirty canonical checkout or creating another checkout. Preserve unrelated documentation.
- Publish this review record on fix/controller-parallelism, then fast-forward remote main to the
  same reviewed source plus documentation record. No source/test changes since the verified
  correction commit; production/test cumulative bounds remain +233/+646. No test rerun is needed
  for these documentation-only additions. Merge authority does not activate deployment.
- Installation, live restart, sandbox alias rollout and mixed-generation lock proof remain
  separate. No production SNodeC workflow, credential, key, tag, feed or deployed state changes.


### Deploy reviewed parallelism controller on core.vchrist.at (2026-10-27)

- Owner authorized deployment and startup after merging main at
  `7a38f93293666176c3387b8ff119b5c2c0ab6b94`. Installed the verified Debug executable,
  SHA-256 `5f74221cbb3265670a70177ac5d22980c22ba230c28a41f3db734f624a30ef66`, at
  `/srv/core-ci/bin/core-ci`. Its 20 installed private SNode.C runtime library hashes
  match the tested build exactly. Remote `ldd -r` and help checks succeeded; no framework
  package or existing runtime library was changed.
- Captured graph state before cutover and stopped the existing service. Saved a consistent
  database dump, previous executable and runtime manifest under the protected backup path
  `/var/backups/core-ci/parallelism-7a38f93-20261027T190555Z`. Backup contents were not
  inspected or transferred. No credentials or private configuration contents were inspected.
- Kept existing workflow/tooling pins and listener addresses. Installed a service drop-in
  `/etc/systemd/system/core-ci.service.d/90-parallelism-rollout.conf` preserving the existing
  command and adding `pipeline --publication-limit 1`, as the approved staged rollout requires
  before mixed-generation feed-lock proof. HTTP concurrency uses the reviewed implementation.
  No GitHub workflows, enrollment inputs, historical selections or package bytes were edited.
- Started `core-ci.service`: PID 210183, active/running, zero restarts at verification.
  The running executable hash matches the installed artifact. MariaDB is active and the API
  reports database available and mutations allowed. All 140 executions retain IDs, attempt
  identities, revisions, sources, state, run assignments and receipts at generation 1394:
  108 published, 17 failed and 15 waiting. No failed work was retried and no new build requested.
- Verified externally reachable management health on port 8080, trusted HTTPS public status
  on port 8443 (schema 1, generation 1394), local SSE graph bootstrap at the same generation,
  and unsigned webhook rejection with HTTP 401. Existing ports 80/443 were left untouched.
  These are live service deployment checks, not a new GitHub build or mixed O/N lock acceptance.
- Local evidence and deployment script are retained under
  `/tmp/codex/build/core-ci/deploy-parallelism/`. No additional checkout was created.
  Production/test source changes: zero; only this documentation entry was added locally.
  Existing unrelated documentation remains preserved. Publication capacity above one, new
  sandbox aliases and remaining live pipeline acceptance gates are separate follow-up work.


### Tag-driven sandbox graph and utilization policy (2026-10-27)

- Owner requested a complete fresh chain from a core tag and continuous monitoring of GitHub CI,
  public status pages and the core.vchrist.at:8080 UI. Created only demo-core v2.1.3 at the
  unchanged prior source commit 1cfcfa17de500bb68f389bf6b8c4cbf27ddcbb57 at 19:12:46.936 UTC.
  Initial CLI tag creation was definitely rejected with HTTP 404. The existing controller App
  then created it using a temporary token restricted to demo-core contents/workflows write;
  the token was revoked. No secret value was printed, stored or inspected.
- Its real webhook selected 20 new executions (140 to 160), reserving revisions 145 through 164.
  All five core builds were dispatched automatically; dependent work retains the original policy
  of waiting for verified published prerequisites. No manual selection or advance was injected.
- Owner clarified that available GitHub runner capacity must not be reserved for task classes;
  all eligible work should be submitted, with publication priority/reordering allowed. This
  preserves the original natural prerequisite ordering rather than introducing another queue.
- Removed the temporary one-publication admission throttle by changing the service override to
  --publication-limit 20 and restarting the same reviewed binary. Backup and graph snapshot:
  /var/backups/core-ci/admission20-20261027T192059Z. Existing GitHub assignments were retained;
  no uncertain mutation was retried or replaced. No controller production/test code changed.
- The deployed old publisher workflow still holds the feed group around the whole workflow;
  queue:max preserves pending publications. The merged new workflow narrows that boundary but
  is not activated by this controller-only deployment. Current selections retain their original
  immutable workflow/tooling pins. No gate for mixed-generation workflow activation is claimed.
- Corrected the explanation of Git: normal non-force pushes atomically reject stale history;
  they do not silently overwrite another publisher. Feed-lock coordination avoids conflict and
  metadata regeneration, not a failure of Git's branch protection semantics. No force push or
  automatic push-retry protocol was introduced.
- Live monitoring records controller, GitHub and Pages observations independently under
  /tmp/codex/build/core-ci/tag-chain-v2.1.3/. The UI rendered the session in an isolated headless
  browser initially; repeated DOM snapshots timed out, so continuing UI monitoring uses its
  actual live SSE stream and health API. This is not claimed as continuous visual verification.
- GitHub/controller completion comparisons have so far found no stale completed-run evidence.
  Pages was observed behind the controller (generation 1472 versus 1519, seven state differences),
  separately recorded as public projection lag. The session is still in progress, not accepted.


## Slim controller fresh implementation — source foundation

Owner instructions: start a fresh controller on a new branch, with at most
2,000 production lines including bootstrap; preserve the publication pipeline's
functionality, reuse `main.cpp` unchanged, and reuse the web UI unchanged.
The controller remains an externally driven, persisted dependency graph: apply
events, traverse eligible nodes, commit assignments, then dispatch GitHub work.
GitHub owns runner capacity and serial feed mutations. No separate scheduler.

Branch: `rewrite/slim-graph-controller`, based on `7a38f93293666176c3387b8ff119b5c2c0ab6b94`,
which matched remote `main` when checked. Existing unrelated documentation edits
were carried intact. The initial attempt to branch from the stale local `main`
was refused by Git; no files were overwritten.

Removed the previous controller implementation under `src/`, retaining
`src/main.cpp` byte-for-byte and its UI asset-generation template. The existing
UI, GitHub worker tooling/workflows, tests and build/format infrastructure remain
as inputs and behavioral contracts for reconstruction. Private directories and
keys were not opened, listed, removed or changed. No deployed controller or
database was touched. The removed implementation remains in the base commit.

This is a source foundation, not a functioning replacement. Bootstrap dependency
implementations and their build/test wiring still require reconstruction; the
checkout is intentionally not buildable at this point. No build, functional
verification, commit, push or deployment is claimed. The 2,000-line ceiling is
for the complete replacement controller, not a claim about this incomplete tree.

Owner performance boundary: the complete four-project sandbox matrix must incur
at most one second of controller-added delay, not one second per node or event.
Millisecond local reaction is the target. Measure local graph derivation, durable
assignment, ready-work dispatch and controller scheduling gaps; distinguish these
from externally awaited GitHub evidence/API responses, runner queues, execution,
feed writes and Pages propagation. External waits must not serialize independent
ready work. No minute-scale controller delay is acceptable. This is an acceptance
requirement, not a measured result or an end-to-end GitHub duration guarantee.


## Fresh controller implementation checkpoint

The owner approved raising the complete controller C++ source/header ceiling from
2,000 to 2,600 lines, including the reused bootstrap, after the remaining GitHub,
recovery and UI contracts were estimated. This authorizes the revised bound; it
does not authorize deployment or dropping publication functionality. Main, UI,
worker workflows/tooling and private files remain unchanged.

New code uses the installed SNode.C asynchronous MariaDB API and its existing
one-event-loop lifecycle. Graph changes derive at the front of the database FIFO,
write one conditional autocommitted graph-document update, apply only after
acknowledgement, then deliver the execution continuation before derivative UI
notification. The fresh schema is in sql/graph.sql; it is not a migration and has
not been applied to an existing or live database. The existing schema and all live
data remain untouched. New storage requires explicit deployment/data planning
before any future activation.

Focused local checks under /tmp/codex/build/core-ci/slim-controller/ passed:
- DatabaseBoundary.cpp: twelve queued writes plus a callback-enqueued thirteenth
  derive against the already-applied state; definite SQL, conditional and
  preparation failures leave later writes usable; no-op handling; teardown from
  the final read callback. Two independent isolated real-MariaDB fixtures.
- SlimGraphTest.cpp: four synthetic projects/five targets, root -> app/side -> join
  publication dependencies, every eligible publication admitted, no replay of
  assigned work, delayed/duplicate and attempt-two rejection, explicit missing
  prerequisite/cycle errors, oversized assignment blocker, SemVer cases. The
  generated assignment passed the unchanged Python worker contract.
- GraphPersistenceBoundary.cpp: real MariaDB stores the twenty-node graph, applies
  twelve overlapping progress changes without lost updates, suppresses unchanged
  advancement, rejects a failed derivation without partial storage, restores in a
  fresh process without replacement assignments, and exposes UI/public projections.
  Missing-schema startup failure/teardown checked independently.

These are isolated local checks, not complete controller, HTTP/SSE or GitHub
acceptance. The in-memory traversal measured 40.6841 ms in the Debug diagnostic;
this excludes MariaDB and HTTP and is not a full-controller latency claim.
The repository application build remains incomplete while adapters/bootstrap
dependencies are reconstructed. No commit, push, restart or activation performed.


## Slim controller: verification and line-budget checkpoint

Checkout /home/voc/projects/snodec/core-ci, branch rewrite/slim-graph-controller,
HEAD 7a38f93293666176c3387b8ff119b5c2c0ab6b94 plus the uncommitted rewrite.
The owner-approved maximum remains 2,600 physical C++ source/header lines,
including bootstrap. Current src/*.cpp and src/*.h total: 2,349. This is an
incomplete application; the remaining 251 lines cannot be reported as sufficient
for the missing publication pipeline. The earlier estimate was too low.

The invariant remains one durable graph: derive each mutation against the latest
applied state, commit dispatch assignments before HTTP, overlap independent ready
work, and satisfy dependency edges only from verified publication. No application
scheduler, persisted ready queue, replacement dispatch or admission reservation is
being added.

Fresh checks after the Crypto extraction and five-target fixture update:
- The slim-graph-test diagnostic passed: synthetic in-memory twenty-target traversal
  36.3675 ms, excluding all MariaDB and HTTP time.
- tests/database_boundary.py --case graph passed against graph-persistence-boundary
  in a new isolated real-MariaDB fixture at
  /tmp/codex/build/core-ci/slim-controller/graph-final. This includes restoration
  in a fresh process without replacement assignments.
- Fourteen independently executed tests/github_transport.py cases passed against
  slim-github-test: parallel, parallel-single, wrong-hostname, wrong-ca,
  token-rejected, token-lost-response, expired-token, mutation-lost-response,
  lost-response, disabled, capacity-zero, capacity-seventeen, shutdown, token-reuse.
  Synthetic concurrent request peaks were 7 with capacity 8 and 1 with capacity 1;
  neither is a live throughput result. Reports are under the permitted temporary
  slim-controller directory, including transport-results.json.
- SourceReader.cpp compiled with -Wall -Wextra -Wpedantic through the temporary
  slim-source-check object target. This is compilation only, not source-selection
  or definition-validation acceptance.
- The preceding local TLS success check passed after correcting request teardown
  correlation. SNode.C clears the request socket context before its HTTP disconnect
  hook; correlation now uses the existing weak request identity. The earlier local
  diagnostic crash did not involve a deployed controller.
- Managed MariaDB startup, initial database failure, and owned-child startup failure
  had separate passing isolated checks. They are not complete application startup
  acceptance.

Current source still lacks complete configuration, enrollment/editor integration,
workflow dispatch/binding, webhook/reconciliation application, artifact completion,
public-status dispatch and independent receipt/HTTPS publication proof. Definition
validation and graph/UI contracts also need completion. The root application is
not buildable, and the original pipeline tests do not validate this replacement.
The unchanged bootstrap still contains the obsolete publication-limit constructor
argument; it must be removed as wiring, rather than retained as a dummy setting.
No such bootstrap change has been made.

Re-estimate for the current approach, before any further production additions:
configuration 120-160 lines; enrollment/editor integration 180-250; independent
publication proof 100-150; dispatch/webhook/reconciliation/artifact/status orchestration
650-850; remaining interface declarations 70-100. These are unimplemented estimates,
not measured code. Consolidation may remove approximately 100-200 existing lines;
it does not account for the missing 1,120-1,510 lines. A realistic revised upper
bound for this approach is 3,800 total lines. It is NOT approved. Do not exceed
2,600, drop functionality, compress formatting, or move controller logic elsewhere
to conceal the count. A larger bound or a substantially different reviewed design
is required before this approach can be completed.

Whole-document persistence currently copies and rewrites the retained graph on a
change. The small synthetic fixture does not establish its scaling with accumulated
history. No complete-controller latency, SSE, Pages, GitHub execution or publication
acceptance is claimed. The new schema has only been provisioned in isolated fixtures.

Relative to HEAD, including explicit new task files: C++ production +1,192/-4,589,
net -3,397; tests +710/-4, net +706. sql/graph.sql is separate fresh-schema work.
These reductions include missing implementation and are NOT a completed simplification
claim. Preexisting documentation changes remain separate. Main, UI assets and the
original GraphFixtures.h remain byte-identical to HEAD. git diff --check passed.
Private directories were not opened, listed or changed. No commit, push, merge,
installation, restart, live database change or GitHub mutation was performed.

## Fresh controller: approved 3,500-line implementation checkpoint

Owner approval: “OK 3500 lines are approved.” The implementation remains on
`rewrite/slim-graph-controller` in `/home/voc/projects/snodec/core-ci`, based on
`7a38f93293666176c3387b8ff119b5c2c0ab6b94`. No branch switch, additional checkout,
commit, push, merge, installation, restart or GitHub mutation was performed here.
Preexisting README, deployment and work-log hunks were preserved. The new architecture
and deployment edits describe this source; existing live deployment evidence does not
validate it.

Measured controller size: **3,498 physical src/*.cpp and src/*.h lines**, including
main.cpp and blank lines. The unchanged 11-line UiAssets.h.in embedding template is
separate, as are reused HTML/CSS/JavaScript and unchanged worker tooling. No controller
logic was moved into those exclusions. main.cpp retains bootstrap/routes/lifetime,
with constructor wiring, direct GraphStore restoration, the existing database-authority
POST boundary and the derivative public-change callback adjusted for this implementation.
Graph restoration no longer forwards through Enrollment. AGENTS.md records the new bound.

Invariant: one durable graph authority; derive against acknowledged state at the front
of the Database FIFO; commit dispatch authority before its HTTP mutation. Every eligible
build/publication is admitted in rebuild-revision order without a publication quota or
reserved runners. Exact published prerequisites alone release edges. GitHub controls runner
capacity and exclusive feed-mutating jobs. No controller scheduler, polling timer, package
processing or uncertain mutation replay was added. The reused SSE heartbeat remains.

The fresh MariaDB schema uses one conditional autocommitted graph-document UPDATE. Lost
acknowledgement stops further producers until explicit restoration; failed derivation and
definite SQL rejection do not apply state. This deliberately has no migration/importer.
Whole-document copying/rewriting cost with large retained history is not measured. Do not
claim arbitrary history scaling or a live latency target from small local fixtures.

Source definitions are read at selected immutable revisions, including newly introduced
prerequisites absent from enrollment snapshots. An unreadable unknown repository blocks
and is reported. Selection/deployment/generation guards, namespace ownership including
selected history, SemVer, explicit older requests, exact retry inputs, oversized payload
blockers, immutable run binding and uncertainty remain exercised at public boundaries.
A recursive regular-file Git-tree lookup replaces repeated path walks. Its explicit bounds
are 20,000 entries and the HTTP client's 2 MB response bound; truncated/oversized repositories
are rejected. Large-tree compatibility remains an acceptance limit, not a proven property.

Webhook and attempt-one reconciliation facts share application/validation. Bound unchanged
progress does no HTTP work. Build completion coalesces its artifact-list read through durable
acceptance. Publication waits for terminal workflow and publisher evidence; skipped push/job
fails without feed reads, while possible push needs committed receipt and HTTPS proof.
The exact unevaluated publisher label is accepted only for a verified skipped job. Successful
or running jobs with that unevaluated label cannot authorize a writer.

Verification used GCC 16.2 Debug, installed public SNode.C targets and OpenSSL 3.6.5:

- `cmake -S . -B build/codex/slim-controller -DBUILD_TESTING=ON -DCORE_CI_TEST_DIRECTORY=/tmp/codex/build/core-ci/slim-controller/final-tests`
- `cmake --build build/codex/slim-controller -j4`: passed. The final fixture-header consolidation
  also rebuilt successfully (final-build-4.log). Compiler temporary/build artifacts stayed in
  the permitted build directories. Root application and registered test targets build.
- `ctest --test-dir build/codex/slim-controller --output-on-failure -j4 -R '^(lifecycle-|enrollment-boundaries$|database-|persistence-|graph-state-boundaries$|definitions-and-versions$|github-transport-|source-selection-|pipeline-)'`:
  51/51 passed in 96.73 s, including the idle SSE heartbeat. No worker/package/full suite ran.
- Following the skipped-job correction and direct restoration wiring, the affected eight
  startup cases, enrollment and eleven pipeline cases passed: 20/20 in 40.43 s.
- Fixture reuse/consolidation then passed database, persistence, graph and all eleven pipeline
  cases: 17/17 in 34.59 s. These use a distinct disposable fixture directory per invocation;
  rerunning CTest no longer fails because a previous fixture directory exists.
- `cmake --build build/codex/slim-controller --target format-check`: passed. Canonical formatting
  files match ~/.codex/formatting byte-for-byte; changed production lines are <=160 characters.
  `git diff --check` passed. Logs are in `/tmp/codex/build/core-ci/slim-controller/`.

Tests were ported to the fresh Json graph and SQL boundary rather than retaining production
compatibility shims. The canonical GraphFixtures/GraphTest/GraphStoreTest and pipeline.py
cover the current APIs. New DatabaseBoundary plus database_boundary.py covers callback ordering,
no-op/rejected derivation drain, definite failures, lost successful UPDATE acknowledgement and
server-side no-replay verification. Redundant early lifecycle diagnostic sources were moved to
the permitted temporary diagnostics directory; registered main lifecycle tests cover those
boundaries. Obsolete schema-specific START/ROLLBACK and normalized-table assertions do not
apply to the single-statement schema; they are not claimed as coverage. A database-locked
competing-bind race, delayed status-approval stress and sanitizer shutdown checks have not been
reproduced for this rewrite; ordinary concurrent facts, artifact overlap, transport shutdown,
restart and restoration checks do not stand in for those specific diagnostics.

Synthetic whole-chain evidence: all 20 nodes, 40 build/publication dispatches, all five target
families, both run/job orders, exact inputs, commit-before-dispatch, independent publishers,
one artifact lookup per successful build, source-read failures, explicit retry after missing/
expired artifacts or skipped push, inaccessible/tampered receipt uncertainty, recovery,
workflow preview/install/CAS, changed run-commit proof, and restart without replacement.
The eight unchanged notification burst produced **zero outgoing requests**, including tokens
and derivative status work, in every pipeline case.

The latest normal chain recorded **599 total outgoing requests**, including **16 token
requests, 20 artifact reads and 117 status dispatches**. Request-path counters include status
approval reads, execution dispatches, feed Git proof and public HTTPS. Totals vary with
asynchronous coalescing. Derivative status work is still substantial: this is not a claim
that request cost is minimal or that the 10–20x expected speedup is achieved. Synthetic runs
complete in approximately 10–13 s with a local mock, not real GitHub runner/Pages latency.
No live throughput, exact eligibility-to-commit latency, signing or client acceptance is
claimed. The former 1,914-connection partial session is not an equivalent benchmark.

Official API references used to settle the current evidence/dispatch contracts:

- https://docs.github.com/en/rest/actions/workflows#create-a-workflow-dispatch-event
- https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run-attempt
- https://docs.github.com/en/rest/actions/artifacts?apiVersion=2026-03-10
- https://docs.github.com/en/rest/apps/apps#create-an-installation-access-token-for-an-app
- https://docs.github.com/en/webhooks/webhook-events-and-payloads
- https://docs.github.com/en/rest/git/trees#get-a-tree
- https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#workflow_dispatch

The dispatch-input limit is 65,535 characters; this controller conservatively bounds the
serialized inputs object in bytes and the assignment separately. The ref wrapper is not
included in that official input limit. Artifact identity still uses the independent agreed
REST listing, not a custom callback. Installed SNode.C HTTP request/FlowController/SocketContext
and MariaDB command lifecycle interfaces, src/apps HTTP/database examples and MQTTSuite
ownership examples were inspected. They required no framework change.

Remaining activation gates: review the fresh schema and retained-feed rebuild seed; preserve
and resolve existing live assignments without replacement; validate real GitHub feed-lock
exclusion, signed integrated publication, client installation, UI/SSE and status/Pages timing;
measure controller-local latency and status-request cost separately. The source has not been
activated on core.vchrist.at. Its database, ports, TLS, webhooks, tags, keys and feed bytes
were untouched. UI assets are byte-identical. Private directories were neither read nor
listed, and remain preserved; /secrets/ was added to .gitignore.

Build identity and exact source/test accounting are recorded in
`/tmp/codex/build/core-ci/slim-controller/accounting.json` (HEAD plus dirty production diff
SHA-256, including explicit new source files). Temporary logs/fixtures remain there for review;
remove them after acceptance when no process uses them. Documentation accounting includes
preexisting unrelated dirty hunks and must not be treated as solely this task's additions.

Final implementation accounting relative to HEAD (including explicit new files): controller
C++ +2133/-4381, net -2248; CMake/schema +77/-111,
net -34; tests +1380/-1843, net -463.
The production diff SHA-256 is `4343de7845e9728d2b111379b6cf96d924cc18b136d0a8c644ecb6a4a2df34c3`.

### Publish the fresh controller review branch

Owner explicitly authorized committing on the current branch, pushing it and notifying
Claude. Stage only the fresh-controller implementation, its current architecture/schema
and test changes, and the fresh-controller work-log entries. Preserve unrelated monitoring,
deployment and review hunks unstaged. Request a read-only detailed review of the exact
pushed commit. This stage does not merge or activate the rewrite.


### Fresh-controller review corrections — 2026-10-28

Workspace: `/home/voc/projects/snodec/core-ci`, `rewrite/slim-graph-controller`,
HEAD `e34748a0cda0dc07cdf674974cedba440ca65a16`. The prior review checkpoint is
committed and pushed; these corrections are uncommitted. Preserve the unrelated dirty
operational/deployment hunks and existing review records. No additional checkout exists.
Owner explicitly approved a maximum of **3,510 physical controller C++ lines** after the
fully formatted agreed correction measured that size. AGENTS.md records that bound.

Claude and Codex agreed the source-level correction prompt after checking counterexamples,
framework ownership and local probes. Read-only dialogue, final agreement and implementing
prompt are retained under `/tmp/codex/build/core-ci/slim-controller/claude-*`; the correction
diff, final review, commands and results are under `/tmp/codex/build/core-ci/slim-corrections/`.
Those task directories are temporary evidence, removable after review/acceptance when unused.
No discarded implementation was recovered.

Invariant: derive selections against current graph authority, preserve newer/incomparable
immutable source inputs, and distinguish provably unsubmitted work from uncertain dispatch.
No controller runner reservation, publication admission cap, state, timer or queue was added.

- D1: selection captures existing `next_revision`, checks current enrollment/branch equality,
  and rejects newer/incomparable sources created during its reads. Job evidence and explicit
  retries do not invalidate those reads. Explicit older root versions remain supported;
  conflicting equal-version identities remain rejected. Current namespace validation and
  exact prerequisite lookup remain authoritative. Real competing source selections report
  409, without automatic retry/reselection; lossless competing-tag delivery is not claimed.
- Supersession: compare all shared transitive source inputs, not only the node's own tag or
  direct prerequisites. An explicit older chain and later automatic dependent cannot discard
  waiting work with newer inputs. Dependency removal remains supported. Lookup still chooses
  the newest-revision matching prerequisite; this correction does not introduce a new policy.
- N1: automatic root notifications supply a pushed-tag floor while still reading tags. Two
  pushes sharing a stale listing cannot silently lose the newer pushed tag. Explicit exact
  tags, backport/highest-version selection, expected tag-object checks and non-SemVer rejection
  retain their contracts.
- D3: startup reconciliation alone fails an assigned stage with no committed preflight,
  raw dispatch identity or bound run. Committed preflight remains uncertain. Ordinary operator
  reconciliation cannot cancel live preflight. Preflight commit checks both attempt and stage
  before dispatch. No mutation is replayed. Reconciliation uses the existing lazy change and
  removes its trailing no-op write.
- H1: coverage only. Null queued timestamp followed by stable running/completed timestamps
  succeeds; conflicting nonnull timestamps still reject. Real GitHub changing this timestamp
  remains unproven; no production relaxation was introduced.
- D2 retains block-and-report for unreadable/missing source definitions. H2 retains uncertainty
  for empty job/step evidence. H3's dispatch concern and H5's ownership concern were withdrawn
  after source/API checks. H4 retains definite zero-row CAS rejection; H6 retains the completion
  exception uncertainty latch. One-page cold run search, schema and status-refresh policy stay.

Reductions replace repeated bootstrap teardown, enrollment completion and failed-transition
closures; the unused Enrollment::persist argument is removed. Main's completion factory has
main lifetime, rather than capturing a local startup callback object.

Focused verification (local synthetic GitHub, real isolated MariaDB; no live acceptance):

```
cmake -S . -B build/codex/slim-controller -DCORE_CI_TEST_DIRECTORY=/tmp/codex/build/core-ci/slim-corrections/tests
cmake --build build/codex/slim-controller -j4
cmake --build build/codex/slim-controller --target format-check
ctest --test-dir build/codex/slim-controller --output-on-failure -j4 -R '^(lifecycle-|enrollment-|database-|persistence-|graph-state-|source-selection-|pipeline-)'
git diff --check
```

Build, formatting and whitespace checks pass. All **48 selected checks pass** (91.09 s),
including all 24 independent pipeline cases; 13 pipeline cases are new. Graph boundary tests
retain existing assertions and add stale-source, transitive-input and timestamp coverage.
The first restart-fixture run exposed graceful shutdown resolving blocked callbacks; the
crash tests now kill only their disposable controller at the held boundary, then restart
from its real durable database. All four corrected build/publication crash cases pass.
No test was weakened to accept controller behavior.

The final local normal twenty-node chain records **569 outgoing requests**, 16 token mints,
20 artifact reads and 106 status dispatches. Eight unchanged notifications cause **zero**
outgoing requests, including status/token work. The prior 599-request normal-chain result
is a baseline, not an equivalent live benchmark; asynchronous coalescing changes totals and
this difference is not a claimed speedup. In-memory graph traversal measured 41.1854 ms,
excluding HTTP/database/runner/Pages latency. Status cost and retained-history scaling remain
acceptance items. No 10–20x speedup or subsecond end-to-end acceptance is asserted.

N2 is a separately **confirmed installed-dependency recovery defect**, not corrected here.
A local probe with the exact e34748a binary stopped its isolated external MariaDB: status
answered in 0.0035 s beforehand, timed out at 2 s during absence and still timed out after
SQL was independently ready again. In-memory graph reads remained available. Installed
SNode.C's MariaDBConnection immediate reconnect failure leaves `connected=false`; destruction
then deletes queued commands without command-error callbacks. The controller FIFO cannot
complete its head. A process restart is required. Probe source/result:
`/tmp/codex/build/core-ci/slim-controller/corrections/database-disappearance.py` and
`corrections/n2/results.json`. Framework correction or an application workaround needs
separate scope; no framework edit, hidden recovery mechanism or automatic replay was made.

Accounting against e34748a: controller C++ +167/-155, **net +12**, total **3,510**;
CMake test registration +13/-0; tests +514/-218, **net +296**. Reused UI embedding remains
11 separate unchanged lines. Against the fresh rewrite base 7a38f93: C++ +2209/-4445,
net -2236; build/schema +90/-111, net -21; tests +1669/-1836, net -167.
Documentation accounting is separate; unrelated 682 work-log lines are not these corrections.
Build identity is HEAD plus dirty production diff SHA-256
`b6ade475e46b60a1f3b8d079de318314c03e49d2b521a7571a0b4bb273d72e2d`;
`slim-corrections/accounting.json` records the classification and counts.

No correction commit, push, merge, deployment, live restart or GitHub trigger occurred.
Production SNodeC resources, live database, ports/TLS/listeners, tags, trusted keys, feeds and
previously published bytes were untouched. Private directories remain uninspected/preserved.
Live pipeline/signing/client/UI/status acceptance and the N2 recovery gate remain outstanding.

Final read-only Claude review confirms plan adherence and reports no concrete code defects;
see `slim-corrections/claude-final.md`. It independently read the final diff, affected source,
tests and 48/48 result. Observable error-path difference: reconcile does not inspect when
its initial graph change fails; that definite error is reported. The latched-mutation route
remains rejected by existing middleware. Reviewer agreement is not live acceptance.

### Fresh-controller sandbox deployment authorization — 2026-10-28

Owner explicitly authorized committing, pushing and deploying the corrected rewrite on
core.vchrist.at, followed by real VOC-Experiments sandbox testing. This does not merge main
or authorize production SNodeC resource changes. Preserve the existing database/history,
keys, package bytes and listener settings. Current service has no assigned build/publication:
its public graph reports 127 published, 17 failed and 16 waiting executions. Capture that
checkpoint before cutover. The fresh schema uses a separate database, not a schema overwrite.


### Slim sandbox activation and live timestamp correction — 2026-10-28

Owner authorized commit, push, deployment on core.vchrist.at and concrete VOC-Experiments
acceptance. Committed/pushed 4cbd5bff0f923d60a3cb5c47a114df36e916a87b on
rewrite/slim-graph-controller. No main merge. Unrelated local hunks remain unstaged.

Installed the tested binary (SHA-256 adbcb9f2b015d8b29c68e1ef1736b93725bc9f618e618b8208867f182a34a161).
All 20 server SNode.C runtime libraries match the local tested build; ldd -r and help pass.
Backed up the previous executable, manifest, service and database under
/var/backups/core-ci/slim-4cbd5bf-20261028T090218Z. Kept the original core_ci database intact
with all 160 executions/history (127 published, 17 failed, 16 waiting; no active assignments).
Provisioned separate core_ci_slim with current GitHub enrollment definitions, generation1686
and next_revision165, above retained receipt/attempt revisions. No history importer or state
conversion. Service core-ci runs as core_ci, with existing keys and systemd TLS credentials;
8080 wildcard UI, TLS8443 and ports80/443 are unchanged. Fresh settings contain identifiers
and existing file paths only; private files/configuration contents were not inspected.

Sandbox worker tooling e7b1423cc9537070f84aea7bf7893fb590f75301 and wrappers
721537bc7d0478b564eb397c3c87069778ac8cfe deploy the reviewed publisher job lock and
secret-free confirmation. Sources pin this tooling. Source wrapper blob
30c4d3074c5031f4b904f70b106eeabd655f52dd; publisher c2b5e78b721d2e649490baa390e3ea5e7f8cdf9b;
status54220e81eda55958e2d862c7f8815156acec2b32. Existing trusted keys, tags and package bytes
were not replaced. The user CLI token lacks workflow scope, so Git SSH pushed wrapper commits
using a temporary Git index in the canonical repository, without another checkout/worktree.
Only VOC-Experiments resources changed. Public workflow variables/secrets were not changed.

Tag demo-core v2.1.4 triggered 20 nodes and five root builds. Live evidence then proved H1:
GitHub queued job started_at09:03:18 differs from actual start09:03:21/completion09:03:51.
The controller rejected later facts and displayed queued after GitHub completion. This is a
controller merge defect, not GitHub delivery delay. Earlier null-queued synthetic coverage
was insufficient. No real publication success is claimed for that initial test.

Owner explicitly requested the fix, redeployment and a fresh chain, plus Claude's detailed
review of all GitHub REST and inbound webhook paths. Correct invariant: queued placeholders
are not actual start evidence; actual running/completed start replaces only pre-running
facts; later queued facts cannot regress progress. Identity/provenance checks remain exact.
Graph::observe is the single merge authority. Its null suppression replaces duplicate
filtering in Pipeline::facts. A first-patch null lookup error was caught locally and corrected
with contains(), before deployment. Graph tests cover fresh stages, restored queued evidence,
completed-only recovery, late queued delivery and conflicts after actual start. Existing
checks are retained. The first test attempt also exhausted /tmp; removed only stopped,
disposable task database directories, keeping logs/source/JSON evidence, then reran tests.

Claude's read-only comprehensive review and final confirmation are in
/tmp/codex/build/core-ci/slim-deployment/claude-api-review.md and claude-api-confirm.md.
Final review confirms the correction and reports no remaining concrete REST/webhook defect.
Owner accepted the token recommendation: cache reuse stops300s before expiry, preserving
scopes, cold coalescing and mutation non-replay. This mitigates near-expiry queue risk, not
arbitrary queue delay. A new public transport test uses a still-valid120s token to verify
refresh before actual expiry; existing expired-token and reuse checks remain.

Final timestamp checks: 25/25 graph/pipeline checks passed; after token change, all24 pipeline
checks passed again, plus seven graph/token/parallel/mutation non-replay checks. Formatting
and build pass. Additional near-expiry/reuse results are recorded in token-near-expiry.log.
Source is3,508 C++ lines within the approved3,510; no new state, queue, timer or replay was added.
Official references checked: GitHub REST workflow-jobs (attempt-specific jobs), workflow-runs
(attempt-one run), workflows (dispatch), artifacts (complete listing), and installation-token
generation (one-hour expiry and scoped grants):
https://docs.github.com/en/rest/actions/workflow-jobs
https://docs.github.com/en/rest/actions/workflow-runs
https://docs.github.com/en/rest/actions/workflows
https://docs.github.com/en/rest/actions/artifacts
https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-an-installation-access-token-for-a-github-app

Live evidence, scripts, manifests and request/progress snapshots remain under
/tmp/codex/build/core-ci/slim-deployment/. N2 is still an unresolved installed-framework
reconnection defect; no framework code changed. Live signed publication, fresh-chain
completion and client acceptance remain unclaimed until actually observed.


### Approved compact prerequisite payload and live continuation — 2026-10-28

The live fresh v2.1.5 chain reached18 published targets while Fedora join was visibly blocked
by its input bound. Real diagnostic using the authoritative Graph.cpp assignment/inputs on a
read-only sandbox state measured assignment82,701B / inputs87,705B. Prerequisite RPM index
listings alone accounted for65,382B. This is a transport-bound defect, not a dependency-delay
inference. The existing65,535B dispatch bound and60,000B worker assignment bound stay.

Owner explicitly approved the bounded payload fix before implementation. Take a value copy
of each first-attempt prerequisite receipt and omit only indexes. Full stored and signed
receipts, exact package/export hashes, environment/SDK compatibility, identities and all
verification stay unchanged. Existing assignments and retries keep original bytes. Hooks'
receipt copies now omit indexes; docs/extensions/README.md states that contract. No worker
production change, callback protocol, persistence state or workflow update was introduced.
Two C++ lines bring the controller to exactly3,510. The same real Fedora data now measures
assignment17,286B / inputs19,164B. Baseline/after diagnostics remain in input-metrics*.jsonl
under /tmp/codex/build/core-ci/slim-deployment; the live database was not edited by the probe.

Claude traced all worker consumers and source hooks, agreed the design and reviewed the
implementation in detail. It found no concrete defects; claude-payload-review.md and
claude-payload-confirm.md record its source/consumer analysis and limitations. This is local
review, not live acceptance. The reviewer noted possible retained RPM repodata accumulation;
that is a separate unverified worker-side hypothesis, not changed in this correction.

Seven focused checks pass against the final production source: graph-state-boundaries,
pipeline-chain, pipeline-recovery, pipeline-large-indexes, worker-package-boundaries,
worker-environment-boundaries and worker-provenance-boundaries. The graph test proves
large-index joins fit; exact consumed identities and full stored receipts remain; assignments
are not replayed/recomputed; retries reuse their original dependencies. The new full20-node
MariaDB/syntheticGitHub case retains100 index entries in each stored RPM receipt while
omitting them from dispatched prerequisites. Worker projection proof accounts for the
correctly changed assignment digest and preserves every other publication-receipt field.

The first test run had test-only indentation and expected-digest errors. Fixed those tests;
the four affected failures pass on rerun. Graph/environment/provenance already passed against
the identical production source and are not repeated solely for a consolidated log. All
relevant checks ran independently. Logs: payload-tests.log and payload-tests-final.log.
Build, canonical formatting and git diff --check pass. Controller delta +3/-1 = net+2;
CMake registration +1; tests +60/-1 = net+59; extension documentation +3.

The corrected timestamp deployment ae2f3ac is running, binary SHA-256
93c825981c4c5378ed15f8138388735c3e76de830956e0a83ebd894ef530599c.
The fresh chain was triggered only by demo-core v2.1.5 at378ff698bf66dc1b4d1b46c1b72dc12704fe3cf1,
at09:19:29Z. No manual advancement/retry was used for its18 successful publications.
Native join client acceptance passed on amd64 (40666026402) and arm64 (40666021823);
APK client40666809235 also passed. Remaining target/client results stay unclaimed.
Public UI assets match source byte-for-byte; invalid HMAC is rejected; live SSE is captured.
Loopback endpoint medians under load: graph19.664ms, database health1.748ms, public status
18.078ms. These are endpoint timings, not dispatch latency or a speedup claim.


### Slim controller final sandbox verification — 2026-10-28

Deployed eab0b087135e8275752e48ba2fd8772c55e82e92 to core.vchrist.at under the
owner's commit/push/deployment/test authorization. Binary SHA-256:
ee4cfa91431876f967a3fb423df884b2e104e340af9d3784cfbba779bdc60f51.
Service PID221515 is active with NRestarts0. The approved payload change resumed the
same waiting Fedora join revision202; it did not retry successful executions.

All20 fresh revisions185–204 are now verified published. All5 join client workflows
passed:40666026402,40666021823,40668066980,40667246183,40666809235.
Controller/public/Pages reached generation2100; public and Pages JSON are identical.
HTTPS bytes for20 receipts,4 project pages and controller JSON match committed blobs.
Feed comparison found no removed files and no changes to535 retained package,
receipt/export/key blobs. Existing-path changes are affected indexes/status only.
No new403 or Pipeline error was observed after the payload deployment.

Full run links and limits are in docs/proposals/slim-controller-sandbox-acceptance.md.
This includes the intervening approved restart, the earlier uncertain APK assignment
(not replayed), superseded waiting history and unresolved installed-framework N2.
The original database, old tags, keys, private directories and unrelated edits remain
preserved. No production SNodeC integration changed. No main merge or speedup claim.
Final controller size is3,510 lines. This handover adds documentation only; no further
build or repeat tests were needed after the already tested/deployed source.


### Compact work UI proposal — 2026-10-28

Design-only checkpoint on rewrite/slim-graph-controller at7c5e68d. Read current UI,
CMake embedding and existing UI fixture; preserved existing unrelated edits. Owner asked
to discuss all layout/timestamp findings with Claude before implementation. Claude's
read-only review and follow-up agreed the proposal in docs/proposals/compact-work-ui.md:
merge repo/tag/rebuild and target/arch/format; grouped build/publisher start+finish,
separate verification time; intentional date/time lines; scoped nonwrapping labels and
compact details timing table. Explicit format remains because OpenWrt has IPK and APK.
Full timestamp zones remain accessible; narrow-panel sizing needs actual browser proof.

Review corrected provisional width claims and the fact that superseded waiting rows
remain in history. Existing missing-time semantics stay unchanged; no new controller
state, network request, workflow or recovery behavior. UI source/tests/deployment unchanged.
No tests or live performance claims for this design-only checkpoint. Proposed UI net
+40–60 production lines, fixture +30–50, C++0; needs explicit growth approval before
implementation. Documentation only; no commit/push/restart. Review files are retained in
/tmp/codex/build/core-ci/ui-compact-proposal/. Stuck publication investigation remains deferred.


### Compact work UI implementation — 2026-10-28

Owner approved +40–60 UI production / +30–50 fixture lines and zero C++ growth.
Implemented on rewrite/slim-graph-controller from7c5e68d without a branch change.
Invariant: present existing graph identities and evidence compactly, preserving one
state authority, exact timestamps, missing-evidence semantics and existing actions.
Merged repository/tag/rebuild and distribution/series/architecture/format. Grouped
Build and Publisher job start/finish columns; kept verification separate. Details use
a timing table and scoped nonwrapping labels. No controller/network/workflow changes.
Owner requested the grouped-header gap removed: reduced header padding to3px.
Owner then explicitly requested all timezone labels removed: local browser date/time
is used in the overview, details and full accessible descriptions/tooltips. Machine
ISO datetime values remain exact. No zone note, selector, timer or additional API.

Final task delta: UI85 added /28 removed = net+57; fixture29 added /1 removed = net+28;
C++0, controller remains3,510 lines. Fixture growth is below the estimated range.
No source moved out of src. Proposal and this append are documentation, not controller
budget. Existing unrelated README/deployment/WORK_LOG hunks remain untouched.

Focused final verification passes:
- node --check ui/app.js; python3 -m py_compile tests/ui_enrollment.py.
- Existing tests/ui_enrollment.py HTTP/SSE fixture with the new /test/layout cases.
- Installed Chrome via temporary CDP scripts check.py/check-us.py/interaction.py:
  UK/US locales,1440/1280/1152/720/390CSSpx; grouped headers, timestamp lines,
  no label wraps, no narrow-panel overflow after scrolling, IPK/APK, safe long
  compiler text, filtering, failed-only retry, busy controls and SSE open-state retention.
  No extra API/mutation requests. Initial narrow US-time overflow was fixed before
  final checks.1152/720 widths approximate zoom layout space; actual browser zoom
  controls were not exercised. This is local synthetic proof, not live acceptance.
- cmake --build build/codex/slim-controller --target core-ci format-check -j2.
  Reconfigured embedded assets and built final source. Formatting target covers its
  committed C++/CMake baseline; JS syntax/browser checks are separate. git diff --check.

Build identity: HEAD7c5e68dfd9230696968ad8905da2e7b3f0c8870c plus UI/fixture diff
SHA256 bfb6f728bc246026185afa2c2e88cab9204662b8eea2333e4f163c5368b56d1c.
Evidence/screenshots retained under /tmp/codex/build/core-ci/ui-compact/; no new
repository browser framework. No full backend suite, commit, push, installation,
restart, live retry or GitHub mutation. Stuck-publication investigation remains deferred.


### Compact UI deployed on core.vchrist.at — 2026-10-28

Owner explicitly authorized deployment and restart. Deployed the tested local UI build
from HEAD7c5e68d plus UI/fixture diff SHA256
bfb6f728bc246026185afa2c2e88cab9204662b8eea2333e4f163c5368b56d1c.
Binary SHA256 bb1d52035163124d7959a6e739e55a4e64e762166f1804bb581471b30529700f.
The runtime manifest records both base commit and dirty diff; no commit was invented.
Verified transfer checksum, backed up prior binary/manifest under
/var/backups/core-ci/ui-compact-20261028T102725Z, atomically replaced the binary and
restarted the existing service. Release: /srv/core-ci/releases/ui-compact-bfb6f728/.
No configuration, credentials, libraries, listener settings, database or GitHub changes.

Service active, PID222024, NRestarts0. Served HTML, JS and CSS match the tested files
byte-for-byte. Complete graph before/after restart is identical: generation2100,
40 executions. The earlier uncertain r169 assignment remains unchanged, not replayed.
Public TLS status and SSE bootstrap both report generation2100/40 executions.
Journal has no Pipeline failure in the inspected interval; framework TLS/socket
shutdown warnings were observed after successful HTTP checks, not investigated here.

Deployment snapshots/checks remain in /tmp/codex/build/core-ci/ui-compact/.
No further package build, commit, push or merge. Local task changes remain uncommitted;
unrelated working-tree edits/private directories preserved. Stuck-publication
investigation remains outside this UI deployment.


### Centered local 24-hour timestamps — 2026-10-28

Owner requested centered two-line timestamps and 24-hour display, citing Austria.
Centered the date/time spans and selected Intl hourCycle h23 for visible/local full
timestamp descriptions. Local timezone conversion is unchanged; no timezone labels.
No net production/test growth; UI remains net+57, fixture net+28, C++0.
Node syntax and Austrian-time diagnostic pass (midnight00:05:06, afternoon14:05:06).
Chrome checks all5 widths in US locale: two centered lines, HH:mm:ss without AM/PM,
no narrow-panel overflow. core-ci and format-check build pass; diff check passes.

Continued the authorized UI deployment: binary SHA256
eb33eb941c8fbd107883eff0d2efb8f555c46b37de21069125b50d395e0358bf;
HEAD7c5e68d plus UI/fixture diff SHA256
2b7f33dcd043538dcd7612f4fea724084a5898beba278897fe5307571b458504.
Backup /var/backups/core-ci/ui-centered-20261028T103104Z; release
/srv/core-ci/releases/ui-compact-2b7f33dc/. Service active PID222160, NRestarts0.
Live assets match source; entire graph unchanged at generation2100. No retry or
GitHub/configuration/database changes; task remains uncommitted. Evidence stays
under /tmp/codex/build/core-ci/ui-compact/.


### Centered timing group headings — 2026-10-28

Owner requested the Build and Publisher job headings centered above their Started/
Finished columns, and explicitly authorized commit and deployment. Added one scoped
CSS rule for colgroup headers. Chrome measured both text centers within0.008px of
the midpoint of their two-column group. This changes no timestamp/state behavior.
Build core-ci and format-check pass; git diff --check passes. Total UI delta is
86 added /28 removed = net+58; fixture29/1 = net+28; C++0. All UI work stays inside
the approved bounds. Commit includes task UI, fixture, proposal and task WORK_LOG
entries only; unrelated hunks remain unstaged. Deployment/restart verification follows.


### Align timing headers and values to their columns — 2026-10-28

Owner pointed out timestamp values still did not align with Started/Finished headings.
Centered timing leaf headers, verified heading, Pending values and detail timing values
as well as the group titles. Chrome measured every synthetic timestamp/placeholder
text center against its column heading: maximum delta0.008px. Both group titles remain
centered over two columns. core-ci/format-check build and diff checks pass. UI is now
net+59 relative to7c5e68d; tests net+28; C++0. No behavior/data change. The preceding
group-heading binary was transferred but not activated; deploy this complete correction.


### Timing-column alignment live verification — 2026-10-28

Deployed committed ef19a129cae76523ad8514f226b5569148d1c3c1, binary SHA256
3b9ac12ad5fd0e2063af1b7f14732442ae089c5527b3584c1f1455db3989652a.
Backup /var/backups/core-ci/ui-columns-20261028T103621Z; release
/srv/core-ci/releases/ef19a12/. Service active PID222354, NRestarts0.
Served UI assets match source; graph is byte-equivalent as JSON before/after,
generation2100. Read-only Chrome verification on the live8080 UI measured all5
column headings and all timestamp/Pending text centers: at most0.008px difference.
Both Build/Publisher job titles remain centered over their two columns. Live checks
and screenshot are retained under /tmp/codex/build/core-ci/ui-compact/live-*.
No GitHub mutations, retries, config/database changes, push or merge. Unrelated
hunks remain preserved; this append records deployment under existing commit authority.


### Unconfirmed r169 publication investigation — 2026-10-28

Owner requested investigation/proposal only and discussion with Claude until agreement.
Canonical checkout remains rewrite/slim-graph-controller at032cb34; preserved the four
unrelated dirty files and private directories. No production/test edits, GitHub mutations,
retry, deployment, restart, commit or push. Proposed correction and separate policy choice
are in docs/proposals/unconfirmed-publication-recovery.md.

Live graph plus read-only live MariaDB query confirm r169 publishing, build40661265395
successful09:04:49UTC, committed publication preflight, no publication dispatch ID/run.
Journal09:19:51.070UTC records uncertain dispatch, .069 unconfirmed status dispatch.
Nearby broken-pipe/read-descriptor logs cannot be correlated to requests; HTTPstatus,
submitted and error were discarded. Exact HTTPcause,403 and token-expiry claims unproven.
Complete date-filtered publisher listing24/24 has no r169; unfiltered159total/100page
includes all24today. No receipt-path commit history. No currently discoverable publisher
run; absence is not proof never accepted. Late/deleted evidence remains a policy risk.
Newer coreAPK r189 publication40663767448 is successful; fresh20chain already complete.

Claude and this agent exchanged source/evidence reviews and corrections to agreement.
Withdrawn: proven transport loss/nojobevercreated, uniqueness from one match on an
incomplete page, preflightprovesPOSTsubmission, unnecessary worker-atomicity changes,
and claims proposedtests were written. False-send submission flag is defensive and
unreproduced, not the demonstratedcause; defer it and unrelated status-zero wording.
Core proposal: preserve safe dispatch diagnostics, distinguish search/jobs read outcomes,
return inspected/unresolved details, truthful derived activity. No new authority, state,
queue, timer, cache or replay. Estimate+8–12C++/+60–100tests/UI0, needs approval beyond
current3510bound (proposedupper3522). No invented offsetting reductions.
Separate retirement policy: exact latest unbound publishing attempt, atomic guards,
explicit operator acknowledgement, existingfailedstate with honest unknown-outcome reason,
retained assignment/history, no automaticretry, no SQLediting. Late facts retained without
automatic completion of retirednode. Estimate+15–25C++/+8–15UI/+40–60tests; combinedC++
upper3547 if both approved. Retirement/futureexplicitretry needs explicit owner policy
approval; reviewers cannot relax no-replacement-of-unknown-work rule. Recommend r169
retirement without retry because newer chain already published; currentpolicy alternative
is retainassignment and await genuineevidence. No recovery action was taken.

Existing focused local checks (syntheticGitHub, isolated realMariaDB) bothpass:
python3 tests/pipeline.py --executable "$PWD/build/codex/slim-controller/core-ci"
 --directory /tmp/codex/build/core-ci/stuck-publication/tests/preflight-known
 --case preflight-publish-known
and same command --directory /tmp/codex/build/core-ci/stuck-publication/tests/cold
 --case cold. They prove current preservation/nonreplay and evidence-based recovery,
not a proposedfix. Coldfixture reports611HTTP/16token/0unchanged-burstrequests, not live
performance acceptance. No newtests, fullsuite or fixvalidation claimed. Stopped task
datadirs removed; logs/JSON/reviews retained under /tmp/codex/build/core-ci/stuck-publication/.
Production/test line delta0; documentation-only proposal/log. git diff --check passes.

Official GitHub API references checked:
https://docs.github.com/en/rest/actions/workflows#create-a-workflow-dispatch-event
https://docs.github.com/en/rest/actions/workflow-runs#list-workflow-runs-for-a-repository
https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run-attempt
https://docs.github.com/en/rest/actions/workflow-runs#delete-a-workflow-run
API2026-03-10 dispatch200includesrunID/URLs; documented listing/pagination/deletion is
not an exactly-once recovery guarantee. Current framework Request.cpp false-send/source
trace is separate from installed-binary runtime proof. No framework change proposed.


### Unconfirmed-dispatch correction and explicit retirement — 2026-10-28

Owner instructed obtaining Claude's implementation prompt and applying it. Saved prompt
and status in docs/proposals/unconfirmed-publication-implementation.md. Draft formatting
exceeded the initial estimate; consolidated repeated inspection response builders and
counter/result handling, measured the full correction, then requested revised bounds.
Owner explicitly approved3572C++/15UI/3test-registration/180testlines. AGENTS.md records
the current bound and acknowledged-retirement policy. No implicit authority from reviewers.

Implemented current proposal on rewrite/slim-graph-controller, base032cb34:
- Safe status/submitted/transport diagnostics for uncertain execution/statusPOSTs,
  with execution/stage or generation. No bodies/headers/tokens logged; no replay.
- Complete-list/shape/uniqueness validation before unbound-run recovery; incomplete
  pages never bind even one candidate. Known-ID/webhook validation unchanged.
- Shared local inspect outcome callback replaces duplicated response constructors.
  TransientJSON{inspected,unresolved} replaces pendingcounter, not a secondqueue.
  Read completion explicitly does not claim asynchronous graph commitment.
- Graph-derived run-unconfirmed activity and one graph::retirable predicate shared
  by mutationguard and view capability. No new persistedstate/flag/schema.
- Explicitretire action checks exactlatestattempt, publishing, no publicationrun,
  dispatchID or evidence; acknowledged via typedboolean at the HTTPboundary. Uses
  existingfailedstate with honest unknown-outcome reason; keeps assignment/history.
  Two HTTProute registrations share identicalparse/responsepolicy, separate domain
  actions. No automaticretry/cancellation; latefacts retained without completion.
- UI action uses backendcapability, mutationauthority/busychecks and explicitconfirm
  explaining unknownoutcome, nocancellation, operatorchoice and laternewrevisionretry.
Deferred false-send/status-zero and optionaldefinite4xxdiagnostics; no framework/workerchanges.

Line accounting versus032cb34, including current working tree:
C++96added/34removed/net+62 =3572physicalsrc cpp/h lines.
UI12/0/net+12; CMake3/0/net+3. Totalproduction111/34/net+77.
Tests139added/8removed/net+131. Documentation trackedseparately; unrelatedhunks preserved.
Source identity HEAD032cb34 plus SHA256(src/UI/tests/CMake diff)
3059f7c530f61a9c9c7d154723e70fc18fabcf556cee45f5ac14495bc298e2ca.
Persistent buildbuild/codex/slim-controller; newtest/evidence root
/tmp/codex/build/core-ci/unconfirmed-dispatch-fix/. No othercheckout/worktree.

Verification:
cmake --build build/codex/slim-controller --target core-ci graph-test format-check -j2
and graph-store-test build pass. format applied to task C++/CMake/test sources.
node --check ui/app.js and python3 -m py_compile tests/pipeline.py tests/ui_enrollment.py
pass; git diff --check passes, changedproductionlines<=160, singlefinalreturns/braces.
Final command (9/9pass,12.51s):
ctest --test-dir build/codex/slim-controller --output-on-failure -j3
 -R '^(pipeline-(unconfirmed-dispatch|unconfirmed-run|retirement|preflight-publish-known|preflight-publish-absent|cold|recovery)|graph-state-boundaries|persistence-graph)$'
CMake testroot now /tmp/codex/build/core-ci/unconfirmed-dispatch-fix/t to keep Unix
socket names bounded. Initial preflight-absent failure was testMariaDB108charpath>107,
not a controllerfailure; shorterpathrerun and finalregisteredtestpass. Initial GraphTest
buildfailed from duplicate testvariable ready; renamed it and graphbuild/testpass.
No failing check suppressed another; no unrelated fullsuite. Logs preserve failures.

Newpipelinecases cover lostPOSTresponse without/withcreatedrun, per-execution/status
logfields, incomplete0/1/malformed/ambiguous/unreadablesearch, jobsreaderror, no retry
or replacement afterrestart, explicitretirement ack/stale/history/latefacts/noFeedProof,
then newrevisionretry only when explicitlyrequested. Domainchecks reject recorded
run/dispatch/evidence and buildingstate. Existingcold/recovery and graph/store tests kept.
All are local syntheticGitHub plus real isolatedMariaDB, not liveacceptance.

Browserdriver ui-check.py and ui-results.json under the evidence root pass installedChrome
publicHTTP/SSE fixture: absentpartial/failed, shownretirable, cancelkeepspublishing,
unknown/no-remote-cancel/operator-decision/newrevisionconfirm, held-request busycontrols
acrossSSE, exactretirementPOSTchecked outside serverthread, and failed-rowretryvisibility.
The driver is temporary evidence; only the extendedexistingfixture is repositorycode.
Claude found no productionbug; tightened a test selecting the exactexecutionlog rather
than acceptingstatuslogfields. Finalreview reports no concrete defect/disagreement.

No commit/push/deploy/restart/live retirement/retry. Read-only livegraph stillcontains
r169 at publishing; no production SNodeC resources, keys, tags, credentials, feedbytes or
private directories touched. Existing unrelatedREADME/deployment/WORK_LOG changes remain.

### Receiver iteration simplification and authorized integration — 2026-10-28

Owner authorized the receiver fix, commit, push and merge of the completed work.
Traced sendFragment through SocketWriter and descriptor teardown: outgoing bytes are
queued; even synchronous ENOBUFS schedules disable cleanup, with disconnect notification
later in releaseDisabledEvents. Enrollment::list invokes its callback synchronously.
Both SSE receiver loops therefore use eventReceivers directly; no list snapshot/copy
is needed. No framework change, asynchronous capture or new lifecycle mechanism.
C++ total is now 3571 physical lines (net+61 versus032cb34), UI net+12, CMake net+3;
combined production net+76, tests net+131. The earlier3572 measurement predates this
one-line reduction. Build core-ci and format-check pass after this final change.
Commit includes only task source/tests/AGENTS/proposal records and task WORK_LOG entries;
unrelated existing documentation hunks remain unstaged. No deployment or live retirement.

Final receiver-change verification: all nine selected graph/store/pipeline checks pass
(12.81s), including cold/recovery, unchanged-burst coverage and retirement cases.

### Merged recovery fix deployed and r169 retired — 2026-10-28

Owner authorized deployment on core.vchrist.at and resolution of the stalled publication.
Deployed main merge d88ae51d5d6cfc3466c1143a1123789d7d8f5559 (source bytes identical
to task commit a568290), binary SHA256
ec611609081184cbbb431820720261a654ac43c328ddd12e7dcba105a7fe0ae9.
All20 runtime library hashes matched; none replaced. Release /srv/core-ci/releases/d88ae51.
Stopped existing service, made protected consistent core_ci_slim dump and previous-binary/
manifest backup at /var/backups/core-ci/unconfirmed-20261028T113924Z, installed binary
and manifest, restarted existing service. Active PID223081/NRestarts0. No configuration,
keys, libraries, ports, production SNodeC resources or package/feed bytes changed.
Restoration preserved all40 execution assignments, outcomes and evidence, generation2100.

Explicit reconciliation inspected1 unbound publication and disclosed incomplete listing;
this cannot establish whether GitHub accepted the old dispatch. Under owner recovery
authority, POST /api/work/retire acknowledged unknown outcome for exact r169 attempt.
Stored failure: operator abandoned unconfirmed dispatch; remote outcome unknown.
Independent administrative MariaDB read confirms generation2101, statefailed, oneattempt,
null publicationrun and retained preflight. No remote cancellation, replay or retry.
Published replacement r189 v2.1.5 remains intact. All39 other execution views are unchanged,
including15 pre-existing waiting rows;24 published,1 retiredfailed,0building/0publishing.
Live served UI includes action and SSE bootstrap reports retired state.

Evidence /tmp/codex/build/core-ci/unconfirmed-deploy/ holds before/restored/final graph,
reconciliation/retirement responses, manifest and acceptance JSON. Initial acceptance
script incorrectly asserted no historical waiting rows; corrected check proves all39
other rows unchanged and no active building/publishing rows. This is verification-only
correction, not an application failure or reason to advance historical work.
Original transport cause remains unknown. No new full build requested/triggered.
Deployment record remains uncommitted with preserved unrelated documentation changes.

### Fresh v2.1.6 full-chain timing acceptance — 2026-10-28

Owner authorized a fresh core tag and detailed monitoring across GitHub workflows, status
pages and the core.vchrist.at:8080 management UI. Created demo-core v2.1.6 at unchanged
main378ff698bf66dc1b4d1b46c1b72dc12704fe3cf1; preserved all existing tags. Trigger UTC
11:46:43.649288; all20publications first observed12:13:26.058382, elapsed1602.409094s.
All20builds and20publication workflows succeeded on attempt one. No manual retry,
reconciliation or advancement. All15dependent readiness transitions assigned work in
the same committed SSE frame that verified the final prerequisite. All20HTML rows
matched receipt package versions, publication minutes and empty In-build cells. Pages
reached final2442at12:14:34.926307, approximately68.868s after local completion with
30-second sampling uncertainty. Live browser recorded20published atgeneration2442.

Detailed current-session report: docs/reports/v2.1.6-live-timing.md. Evidence/observer
scripts: /tmp/codex/build/core-ci/full-chain-v216/, also server-side for the temporary
tracer. Scoped64entry/return probes on32functions/callbacks, no captured arguments,
bodies or secrets; dedicated trace instance and PID filter. Initial append-open tracefs
setup failed before triggering; corrected to non-truncating write-only file descriptors.
Final trace has0unmatchedreturns/stackdiscontinuities and0per-CPUbuffer overruns.
Removed all probes and instance; all observers stopped, temporary Chrome profile removed.
Optional final screenshot failed after finalDOM/graph capture when CDP closed; earlier
images and finalDOM evidence retained. No application failure inferred from that artifact.

Measured Debug deployment, not Release or an uninstrumented comparison. Graph selection
14.475ms; traversal301calls/625.686ms total, median1.740ms/max8.067ms. Fullgraph/SQL
preparation median114.182ms and application/continuations median185.967ms. FIFO wait
max1131.970ms. Nonoverlapping traced synchronous wall total123.144s includes observer
work/descheduling/profiler overhead, not CPU-only or added critical-path latency.
Thus functional pipeline passes, but negligible aggregate controller overhead is not
established.23successful statusjobs consumed331jobseconds;163other statusruns cancelled.
20publisher jobs consumed1082jobseconds without overlapping each other. Controller
HTTPenqueue and journalconnection counts both826, including derivative work/tokens/proof.
Observer REST calls are separately recorded (periodic421, completeness80, ad-hoc extra).
No Pipeline diagnostic errors or403reported in controller logs. No10–20xclaim without
a comparable baseline. Report separates CPU-code wall spans, async waits, runner work,
feed serialization, status snapshots, Pages and CDN observations.

Deployed controller remains d88ae51, PID223081/NRestarts0; no code/config/keys/library/
production SNodeC resource or test change. Production0/test0; report/log documentation
only, uncommitted with unrelated existing documentation preserved. Historical r169 remains
operator-retired; no new retry. Next performance work requires its own coherent scope.

### Joint speed correction review with Claude — 2026-10-28

Owner requested analysis and agreement with Claude, setting aside the preceding suggestion
to combine GitHub steps. No implementation authority inferred. Current branch
rewrite/slim-graph-controller HEAD a568290; merged/deployed main d88ae51, matching sources.
Read current graph/store/database/status/worker code, public terminal job logs and exact
workflow blobs at721537bc; nine worker Python files matched immutable e7b1423 tooling.
No discarded architecture recovered; screenshot reminders were not analysed.

Three Claude review rounds and final confirmation in session
3962a698-0c29-4fd0-a2b7-ed7330675520 reached agreement; full prompts/responses and filtered
public log evidence under /tmp/codex/build/core-ci/speed-review/. Agreement and concrete
proposal: docs/proposals/controller-speedup-agreement.md. All163 cancelled status runs
had saved jobs=[] listings. OpenWrt prepare total528s vs26s native; unlogged gaps cannot
isolate SDK archive download from metadata/API/extraction. Withdrawn hard1%/10–20s
controller ceiling and any claim331status seconds add directly to the critical path.

Agreed first changes, not implemented: Release baseline; eliminate copy-and-erase hidden
attempt fields; dump SSE once per broadcast; reserve SQL hex output. Reuse the EXISTING
validation artifact as packages/ plus official SDK archive sibling, with full independent
metadata/hash/compiler/staging/format/key/receipt checks. No second artifact/cache/latch.
Four flushed SDK phase markers allow measured attribution. Native and push/confirm do
not require the SDK; missing OpenWrt prepare argument fails before network/key handling,
content checks before extraction/execution. Seven-day retention remains; added SDK payload
about2.03GiB per comparable full session, with upload/download and temporary disk costs.

Old workflow bytes preserved; new aliases pre-rendered/provisioned and hashes verified
before config switch. Status remains on existing wrapper/tooling. Exact deployment guard
requires new core selection before below-core selections; do not relax it or rewriteold
assignments. No broad history layer. Later GitHub writes/config/restart retain their own
authority. Potential bounds needing approval: C++aimneutral/maxnet+5 (cap3576 ifallused),
Pythonnet+15–25, newphysicalYAML300–330, tests+65–110. Existing C++cap3572 remains binding.
No newstatuspolicy, confirmationcadence, tool-installchange or trustreduction in this first
checkpoint. No10–20xspeedup promised; net improvement must be measured.

Only proposal/log documentation changed; production0/test0. No builds/tests/GitHubwrites/
livechanges during this review; only public read-only metadata/log/SDKHEAD reads.
Unrelated dirty documentation and private directories preserved.

## 2026-10-28 — Agreed performance correction, local implementation

Owner instruction: obtain Claude's implementation prompt and use it to improve the agreed
controller/SDK path. Applied the quoted bounds (+5 C++, +25 Python, +330 YAML, +110 tests)
and disclosed artifact cost; no commit, push, deployment, restart or live trigger performed.
Canonical checkout remains `/home/voc/projects/snodec/core-ci`, owned branch
`rewrite/slim-graph-controller`, HEAD `a568290a96ad27b04736d4c2de38d6ad2a116d86`.
Fetched remote main read-only: `d88ae51d5d6cfc3466c1143a1123789d7d8f5559`; its starting
production files match. Remote default is main; no open PR returned. Unrelated dirty docs
were preserved. Private directories were not inspected or changed.

Invariant: one authoritative graph/Database FIFO and unchanged exact assignments, publication
proof, callback order and uncertainty. Reduce copying rather than adding another authority:
GraphStore skips copying already-hidden fields; SSE dumps once per broadcast; SQL encoding
reserves its full output and appends the suffix in place. Release build replaces Debug only
in the local comparison, not the running deployment. C++ totals 3,576 lines; AGENTS records
the accepted bound. No scheduling, status policy, polling, cache or state was added.

SDK handoff uses one existing validation bundle: packages/ plus an OpenWrt-only sdk.archive.
The single SDK preparer supports its existing download callers and a supplied regular-file
archive, independently verifies official metadata/full SHA, then retains every extraction,
target, compiler/staging and package/signature check. Four flushed messages expose phase
boundaries. Missing OpenWrt prepare input fails before identity requests/key files. Native
ignores the argument; push/confirm do not require it.

Claude supplied the prompt and reviewed implementation. Joint review corrected nested bundle
parent creation at its wrapper owner and the final SQL-buffer copy. The canonical unrendered
publication template is `deployment/source/core-ci-publish-bundle.yml.in`; existing renderer
now accepts an optional template, sharing its exact pin replacement/blob calculation. This
replaces a /tmp-only draft and ad-hoc publication rendering. Existing pinned publication,
status, build, source and Pages workflow files stay byte-identical. Real new pins/source
aliases require the later tooling commit. No guessed commit or GitHub write occurred.
The new static test initially used an undefined unqualified `run`; corrected to
`contract.output([sys.executable, ...])`, with rendered blob identity asserted and the final
test passing. No production workaround was needed.

Build: `build/codex/speed-release`, Ninja Release/GCC 16.2.0/installed public SNode.C 2.1.0.
Temporary fixtures/caches/evidence: `/tmp/codex/build/core-ci/speed-correction/`.
All 21 linked SNode.C library identities recorded; no dependencies changed.
Baseline Release binary: d11ad94713df7f13f956e3f04256e0351215da8b1c787433ea21bb0c0bd3a469.
Final Release binary: 9f19d395ca63ea126d1f686ecc6c2716e019613a523d18d5a934f855d1034203.
Final production/test diff plus new template hash:
1031ab11af0242cbd8e4befcc2ae4227445b8dc98bbdefea53e32f3bd9913df4.

Local real-MariaDB/synthetic twenty-target graph, fifty timed GETs after warmup:
Debug / unchanged Release / corrected Release graph medians 12.196 / 4.777 / 4.329 ms;
public-status medians 7.358 / 2.538 / 2.014 ms. Exact byte parity for management graph,
public status and initial graph/repository SSE across all three binaries. These are local
HTTP timings, not live controller critical-path measurements or end-to-end CI speedup.

Verification: initial nine selected checks passed; unchanged production C++ baseline saved
before edits (worker subprocesses used the working tree, not an immutable worker baseline).
Thirteen focused checks passed: graph-state-boundaries, persistence-graph, pipeline-chain,
pipeline-large-indexes, lifecycle-sse-heartbeat, enrollment-boundaries, database-writes,
database-failures, database-lost-ack, openwrt-archive-boundaries, worker-package-boundaries,
worker-provenance-boundaries, worker-environment-boundaries. After final SQL changes the
seven affected checks passed again; final renderer test passed after its correction.
Actual commands, logs and limits: `docs/reports/controller-speedup-local.md` and task evidence.
Canonical format-check and diff whitespace checks pass; changed production lines <=160.
Existing enrollment tests cover two receivers, initial events, repository/graph changes and
reconnects. SDK tests include missing/invalid/type/size/hash input, no second archive curl,
unchanged input bytes and identical facts. Final synthetic chain: 796 outgoing requests,
including 16 token mints, 20 artifact reads and 175 derivative status dispatches. Duplicate
unchanged burst remains zero requests. Status churn is not changed or claimed improved.

Accounting vs starting HEAD: controller 16 added/11 removed = +5; Python including renderer
41/21 = +20; new YAML 180/0 = +180; tests 99/3 = +96. Documentation separate. Bounds respected.
Additional artifact contents approximately 2.03 GiB per comparable session, seven-day retention,
roughly twice that upload/download traffic plus publisher disk copy; net live gain unmeasured.

Remaining stages: commit/push/rollout authority, actual tooling pins and aliases/common feed
exclusion verification, configuration/restart, explicit new core selection before dependents,
and sandbox SDK/controller/status/Pages comparison. Existing sandbox authority is preserved;
no blanket activation blocker added. No assumption another performance fix must follow.
Synthetic fixtures stopped; retain evidence for review/comparison, then clean up. Persistent
Release build retained. Running core.vchrist.at service and settled assignments unchanged.

Claude's final read-only confirmation is saved as speed-correction/claude-confirmed.json:
“The last concrete defect is fixed, and I have no remaining engineering disagreement.”
No owner acceptance or live performance acceptance is inferred from that review.

## 2026-10-28 — Performance correction commit and merge authorization

Owner explicitly requested commit, push and merge. Rechecked origin/main d88ae51 and the
reviewed production/test diff hash 1031ab11af0242cbd8e4befcc2ae4227445b8dc98bbdefea53e32f3bd9913df4
against the validated Release binary. Both are unchanged. Reused the passing focused checks
without repeating them. Stage only this correction's source, tests, template, report, AGENTS
cap and appended task log; unrelated documentation/log hunks remain local. No deployment,
restart, live tag or VOC-Experiments workflow activation is part of this stage.


## Canonical new-publication paths and browsing links — 2026-10-28

- Owner authorized implementation using Claude's agreed prompt, preserving the merged
  package-production speedup. Source: canonical checkout, `rewrite/slim-graph-controller`,
  HEAD `877946851aadd05667f6515ae8bb1c6c2493c205`; remote main verified as
  `5758671349e8898bd1ba4a11dbd8b0228daeb805` with no open PR. Existing dirty documentation
  remains preserved; no checkout/branch switch, commit, push or deployment was performed.
- Invariant: new package writes, native/OpenWrt indexes, client repository roots and
  Packages browsing links agree on distribution-owned paths. Native pools/indexes use
  `DISTRIBUTION/pool/SUITE`, `DISTRIBUTION/dists/SUITE`, and
  `DISTRIBUTION/SUITE/ARCH/{Packages,repodata}`; OpenWrt uses `openwrt/SUITE/ARCH`.
- Package filenames follow DEB/IPK underscore and RPM/APK hyphen conventions without
  changing package version metadata. Markdown browsing uses relative repository paths;
  HTML browsing uses escaped GitHub tree URLs. Direct download URLs remain unchanged.
- SDK archive handoff and official metadata/checksum verification remain intact. The
  SDK preparation module, publisher entry point, bundle workflow, workflow renderer and
  all C++ sources are unchanged from HEAD. The synthetic OpenWrt fixture asserts supplied
  archive forwarding and separate receipt SDK identities sharing the canonical directory.
- No history migration, deletion, inventory, compatibility adapter, key change, database
  reset, feed replacement or build.json addition. Existing receipt/export/signing and
  replay checks remain authoritative. Live activation and client installation are not run.
- Validation source identity: HEAD above plus SHA256 of the task production/test diff
  `851ed92568a676df8d5194f3fbb90062240f08dbedaa59a5c383d651461e3925`.
  CMake configured `/tmp/codex/build/core-ci/canonical-paths/build` with fixture directory
  `/tmp/codex/build/core-ci/canonical-paths/fixtures`; TMPDIR stayed under this task root.
- Focused commands: `ctest --test-dir /tmp/codex/build/core-ci/canonical-paths/build
  --output-on-failure -j3 -R '^(worker-package-boundaries|openwrt-archive-boundaries|worker-provenance-boundaries)$'`;
  only failed checks were rerun after correcting new fixture assumptions about RPM metadata
  compression and README selection/log identity. Final results: native package boundary
  PASS (2.08 s), OpenWrt boundary PASS (0.97 s), provenance boundary PASS (2.05 s).
  Existing speedup assertions in these checks were preserved and executed. OpenWrt signing
  tools are synthetic; these results are not real SDK/signature/client or live acceptance.
- `cmake --build /tmp/codex/build/core-ci/canonical-paths/build --target format-check`
  passed. Python AST parsing, changed production line length <=160, and `git diff --check`
  passed. No unrelated/full suite or live actions were performed.
- Task accounting: production (worker Python/Jinja) +18/-18/net 0; tests +126/-3/net +123;
  deployment documentation +9/-9/net 0, plus this appended work-log entry. No C++ growth.
  Temporary fixtures/build evidence remain under the task root for review and may be
  removed after handover; no extra source checkout was created.
- Remaining stages: commit/push/merge and tooling rollout are not performed. Before any
  live activation, report concrete occupied-directory signing/index conflicts without
  modifying historical files. Real client installation and Packages-link HTTP checks are
  live acceptance work, not established by these local checks.


## Canonical publication correction — deep review and merge preparation

- Owner authorized commit, push and merge, and required a deep Claude review. Source
  correction committed/pushed as `3683f840d92856361ee04177f69b52bad2287ff3` on
  `rewrite/slim-graph-controller`; PR https://github.com/SNodeC/core-ci/pull/3.
- Claude reviewed the exact source in session `44ba3c49-8184-4b83-9689-fea34f4c5f12`: no
  concrete source defects or merge blockers; agreed paths, receipts/replay/signatures and
  SDK archive reuse remain intact. Full read-only review/request retained in
  `/tmp/codex/build/core-ci/publication-layout-review/committed-review.{md,json}`.
- Independently resolved the branch-escaping note: public GitHub HEAD returned HTTP200 for
  both `/tree/rewrite%2Fslim-graph-controller` and `/tree/rewrite/slim-graph-controller`.
  Confirmed canonical layout/client documentation is in the source commit; unrelated dirty
  documentation hunks remain excluded. Missing receipt identity is outside the current
  contract, not a renderer fallback requirement. No production/test correction was needed.
- Existing focused tests validate the exact committed production/test bytes. GitHub reports
  no configured PR checks and a clean merge. No full suite was run. Local synthetic
  OpenWrt proof is not live SDK/signature/client proof; second-architecture/native client
  acceptance and concrete pre-activation content conflicts remain live-stage concerns.
- This review record changes documentation only. No deployment, new builds, historical
  cleanup, feed/key/config changes or uncertainty recovery was performed.


## Canonical publication live activation check — 2026-10-28

- Owner authorized going live and a new full sandbox graph run after main merge
  `4f8370b97e1423d40bebc771017ea349f646b78e`. Current checkout remains
  `rewrite/slim-graph-controller` at review-record commit `8cdbf32`; source tree matches
  merged main. Unrelated dirty documentation remains preserved.
- Before activation, captured live graph and public feed tree under
  `/tmp/codex/build/core-ci/canonical-live/`. Graph generation2442 retains60 executions:
  44published,15waiting,1failed, with zero building/publishing assignments.
  `/api/graph` is healthy; `/api/health` is not a registered endpoint (HTTP404).
- Required read-only activation check found41 top-level APK files already in
  `openwrt/25.12/x86_64/`. Current public key `core-ci-apk.pem` is EC P-256; separate
  `apk.pem` is RSA3072. Public keys only were downloaded. No private key/config was read.
- Downloaded official SDK25.12.6 (285832189bytes), verified its SHA256
  `1864152619092e781fa36a401960c8e9ae42d6a364c44ee5f0b197ca5f3c0fcc` against both official
  `sha256sums` and retained SDK evidence. Extracted only its packaged host APK tool/libraries.
  Tool identifies as apk-tools3.0.5; initial standalone wrapper could not run without its
  loader/libraries, then verification completed with the official packaged host dependencies.
- Sample `openwrt/25.12/x86_64/demo-app-1.0.0-r10.apk` fails `apk --keys-dir <current-key>
  verify` with exit1/UNTRUSTEDsignature. Verification with the separate old public key also
  fails; signing provenance/validity of those older bytes is not established.
  Crucially, the actual `mkndx --output <temporary-index> <sample>` operation with the
  current key exits99: UNTRUSTEDsignature;1errors,notcreatingindex. This is a concrete
  activation conflict, not a blanket prohibition on existing directories or a live run failure.
- Current publisher enumerates all top-level APKs in the corrected canonical directory.
  A new run would therefore encounter this incompatible existing input. Evidence and exact
  outputs are in `apk-compatibility.json`. Only one sample was verified; no claim that all41
  files have been independently checked. No signature bypass, re-key or filtering added.
- Honored owner instruction "No historic clean up!": no deletion, relocation, database reset,
  workflow/config mutation, deployment, restart or new tag/build was performed. Activation
  and the new full run remain pending an owner decision for the conflicting sandbox bytes.
  Application production/test changes0; this work-log append only. Official SDK/evidence
  retained under the task root for review and cleanup after resolution.


## Canonical publication deployed and v2.1.7 fully accepted — 2026-10-28

Owner approved removal of exactly 41 old canonical sandbox APKs and their index, then
continuation of the authorized deployment/full build run. Feed commit `de0ff952361ec2e42c345dc251128f692b437a2f`
removed exactly those42files; all1089otherblobs matched. Initial post-update verification
compared changed directory-tree hashes as though immutable; corrected blob-only comparison
proved the exact approved mutation. No Git mutation was repeated. Keys/receipts/Git history
were preserved; no broader cleanup, re-key, reset, signature bypass or migration occurred.

Deployed main4f8370b as validated Release binary `9f19d395ca63ea126d1f686ecc6c2716e019613a523d18d5a934f855d1034203`
on core.vchrist.at, PID226972/NRestarts0. Existing20runtimehashes unchanged; one
matching missing framework module added privately. Protected consistent MariaDB/service/binary
backup `/var/backups/core-ci/canonical-20261028T191742Z`. All60executionrecords restored exactly atgeneration2442.
Listeners remain wildcard8080/8443; private configuration, credentials, ports80/443 untouched.
Worker tooling4bbf551, canonical source/publication aliases and corrected status pin verified.
Old execution wrapper bytes retained. Client scripts updated to reviewed canonical roots.

Created only demo-corev2.1.7 at27932aec43653be5f8dc08a03a7dfd1ea4efb9fb, UTC19:18:24.519544.
All20fresh targets r225–244 published automatically; all40original build/publication runs and
five real APTamd64/arm64, DNF, opkg, APK client workflows succeeded. No retries, replacement
dispatch, reconciliation or manual advancement. All20package HTTPS hashes match receipts,
all40rendered Packages anchors resolve (8distinct URLs HTTP200). All60previous assignments/
evidence and772retained immutable feedblobs unchanged after the approved removal.

SSE first observed all20published at2026-10-28T19:40:07.226737+00:00, elapsed
1302.707s. Pages reachedgeneration2774 at2026-10-28T19:40:46.630167+00:00. All15dependent
assignments appeared in the same observed graphframe as final prerequisite publication.
No function-level tracing or zero-CPU-time claim. Final loopback graph median43.319ms
(852433bytes), publicstatus15.244ms(117459bytes),10samples each afterwarmup. Publisherjobs
were mutuallyexclusive, total766jobseconds; remote post-validation feed-job wait median106.5s,
max260s. These are remote queue/setup intervals, not controller processing time. All8OpenWrt
publishers emitted4SDKphase markers. Browser unavailable; UI monitored viaAPI/SSE, not visual
rendering. Full proof/timings/runlinks: docs/reports/canonical-publication-live.md.

Evidence/temporary officialSDK under /tmp/codex/build/core-ci/canonical-live/, observer stopped
afterPagescaughtup. No application production/test changes; local documentation only, preserved
unrelated hunks/private directories. No additional checkout, frameworksource edit or production
SNodeC workflow/credential/tag/feed change. Deployment records remain uncommitted.


## Additional live UI review run — 2026-10-28

Owner requested another full run after missing the completed session. Confirmed idle graph
generation2774 with no building/publishing assignments, then created only demo-corev2.1.8
at unchanged main27932aec43653be5f8dc08a03a7dfd1ea4efb9fb, UTC19:51:20.675237.
Existing tags preserved; limited diagnostic controller token revoked after the operation.
Trigger evidence: /tmp/codex/build/core-ci/webui-review-run/v2.1.8-trigger.json.
No deployment, configuration, feed cleanup, retry, manual advancement or code change.
This is a newly started run for live UI review, not completed acceptance.


## Timing-pair horizontal spacing — 2026-10-28

Owner screenshot shows unequal Started/Finished spacing between Build and Publisher job.
The table lets automatic layout independently size the timestamp columns. Set the four
job timestamp cells to the same7emwidth through their existing work-time class; preserve
centered two-line local timestamps, group headings and the independent verification column.
UI CSS only, one existing line reshaped, production +1/-1/net0; no C++ or test changes.
Static selector check covers all four normal execution timestamp cells; changed line <=160
and git diff--check pass. Browser unavailable in this session, so rendered layout remains
unverified. Not committed/deployed; no active run, service or assignment was modified.


## Stable work-table columns during live state updates — 2026-10-28

Owner additionally observed that changing state labels resize the State column and move
timing columns. Replace the local timing-cell width tweak and obsolete state min-width
with one table column policy: fixed layout, explicit State/Target/timestamp/verification
columns, remaining width for Repository, and equal widths for all four job timestamps.
Existing colgroup semantics, centered headings/date/time, two-line timestamps, full state
text and horizontal scrolling remain. Minimum width keeps dates/headings readable on narrow
containers; expanded details retain their existing container-relative layout. No truncation
or new JavaScript/state/rendering mechanism. C++0, tests0; combined UI HTML/CSS net0lines.
Owner authorized commit, push, merge and deployment. Release core-ci build, format-check,
column-group/width assertions and git diff --check pass. Browser unavailable; rendered layout
remains unverified. Restart will preserve durable assignments; no workflow or pipeline change.


## Work-table layout deployment — 2026-10-28

Task commit a0e6ec503509a87451077baafa045af23032e162 pushed on
rewrite/slim-graph-controller; PR #4 merged as c06a7f7a37b81a81292f37c09c0bd0f79c164a81.
Release binary 26caa741b3fee7040c9d446e43ebc4ad7dc32d00e85f2f92508d97e2a6e9a8c4
deployed to core.vchrist.at, PID227598. Same source tree as merge; runtime21 libraries
verified unchanged. Service arguments/workflow pins unchanged. Database backup:
/var/backups/core-ci/ui-columns-20261028T201028Z. Durable generation3100 restored exactly;
all100 executions, attempts, assignments and existing run IDs preserved, including2 active
assignments. Fresh v2.1.8 at verification:18 published,1 publishing,1 building. No retry/tag.
Served HTML/CSS/JS match local assets through loopback and public8080; TLS public status
responds. Browser unavailable, so rendered visual verification remains outstanding.
Production UI +4/-4/net0; C++0; tests0; committed documentation+14 lines.
Deployment record appended locally, preserving unrelated working-tree documentation.


## Restore compact timing pairs — 2026-10-28

Owner rejected the fixed percentage table layout. Governing presentation invariant: State
width stays independent of its current label; timing pairs stay compact, with more whitespace
between Build and Publisher job than between Started and Finished. Restore automatic table
layout and original colgroups, fix only State to260px, and apply symmetric20px outer/6px
inner timing-cell padding to both headers and body. Centered local two-line timestamps and
group headings remain. No JS/C++ or controller behavior change. UI+6/-6/net0; tests0.
Static checks and git diff --check pass, changed lines <=160. Browser reports no available
apps/browsers, so no rendered visual claim. Continue the previously authorized commit/push/merge/deployment stages for this correction.


## Compact timing correction deployment — 2026-10-28

Correction d1b50a3 committed/pushed on rewrite/slim-graph-controller, PR#5 merged as
0188d73a0fff92f5d411cb0445c82252bba7f7d8. Release build source tree equals merge tree.
Deployed binary c36cdef521af25406cb95b73e58039fda0a0818e10ef08d2a1026955f2f26cb3
on core.vchrist.at; service PID227769. Backup
/var/backups/core-ci/ui-columns-20261028T201657Z. Generation3116 and100 executions
restored without changes to attempts, assignments or existing run IDs. No active assignments
at restart; v2.1.8 has20 published targets. All3 served UI assets match local source over
loopback and public8080; existing TLS public status responds. No config/pin/library changes.
Browser unavailable; rendered appearance remains for owner review. Deployment record local.


## Explicit compact timing-column layout — 2026-10-28

Owner accepted one explicit layout: equal compact Started/Finished pairs, a larger gap
between Build and Publisher job, centered headings/timestamps, stable State, remaining
space for Repository. Replace automatic sizing with fixed pixel colgroups: disclosure36,
Target156, State260, four timing columns96each, verification148; Repository alone expands.
Minimum table width1224 preserves240px for Repository and horizontal scrolling on smaller
screens. Existing symmetric outer20/inner6px padding gives82px timestamp-center spacing
inside both pairs and110px between pairs, with group headings over the pair midpoint.
Source assertions cover1224/1400/1712px layouts, colgroup semantics4/2/2/1 and unchanged
JS/C++ consumers. These are source/calculated checks, not browser-rendered proof. Browser
reports no enabled apps/browsers. UI+4/-4/net0; tests0; changed lines<=160 and diff check pass.
Continue existing owner authorization for commit/push/merge/deployment of this UI fix.


## Explicit timing layout deployment — 2026-10-28

Task commit ebe63db5245311d88afc9d8fc566e0c36e14ac47 pushed on
rewrite/slim-graph-controller; PR#6 merged as7dfc6931aa314a203aef4b5130d985014011f8fc.
Release source tree matches merge tree. Binary
d53a6c16e85f39caace1aa45f69628119942e25f2cc41c908071ed334ea00d36
deployed on core.vchrist.at; service PID227971. Backup
/var/backups/core-ci/ui-columns-20261028T202803Z. Generation3116 and100 executions
restored unchanged, including attempts, assignments and existing run IDs; no active work
at restart, v2.1.8 remains20 published. All3 served assets match source over loopback
and public8080; TLS public status responds; service command and runtime libraries unchanged.
Source geometry checks and Release build passed; browser unavailable, so no rendered visual
verification claimed. UI production+4/-4/net0, tests0, committed documentation+14lines.
This deployment entry remains local alongside preserved unrelated documentation.


## Fresh Work-page presentation design — 2026-10-28

Owner requested a design from scratch, retaining all information, after rejecting the
existing layout. Read current summary/detail rendering, data/SSE and existing UI fixture.
Reviewed official GitLab Pajamas/GOV.UK/Carbon table guidance and Buildkite overview/detail
patterns. Proposal and standalone synthetic HTML preview:
docs/proposals/work-table-redesign/README.md and preview.html. Native seven-column table;
each job has one identical inner Started/Finished grid, avoiding competing spanning widths.
Bold package identity, restrained version/owner metadata, status text plus colour, quieter
dates and stronger tabular times. All current evidence, prerequisites, logs, operator
actions, missing-data distinctions and technical facts are mapped in the inventory.
Rendered/visually inspected explicit SVG mockups at1440/1280 and expanded details; PNGs
are drawings, not screenshots of real HTML. Six-row preview search/filter/expansion script
passes node --check and contains no remote calls; browser runtime verification is pending.
No production/test/controller/deployment changes, commit or restart. Existing unrelated
working-tree changes preserved. Proposed implementation estimate+20–40 net UI lines,
+15–30 fixture lines, zero C++/SQL/workflow/API; explicit growth approval remains required.


## Implement fresh Work-page presentation — 2026-10-28

Owner's cpmd instruction authorizes implementation within the proposed UI/fixture bounds,
then commit, push, merge and deployment. Replaced nine-column spanning layout with seven
native columns and one shared job-pair grid in each job cell/header. Removed obsolete
positional padding/width rules. Repository is a row header with bold basename/version and
owner/rebuild metadata. State/known activity share the existing state formatter; no new
execution state or data authority. All detail facts, conditional retirement/retry, logs,
dependency navigation and SSE/selection behavior retained. Timestamp data/locale functions
unchanged; presentation gives quieter dates and stronger centered times. Work-specific
styles preserve narrow-window scrolling and existing light/dark variables.
Production UI+57/-24/net33, fixture+16/-0/net16; no C++/SQL/API/workflow changes.
Final Release core-ci build and format-check passed; node --check ui/app.js and Python AST
parse passed; changed production lines<=160 and git diff --check pass. Existing public UI
fixture serves actual assets and10 synthetic examples (all five states, unconfirmed,
superseded, long identity), including graph and repository SSE updates. Native header/column
checks pass. These HTTP/source checks are not browser rendering/interaction coverage.
Evidence: /tmp/codex/build/core-ci/work-table-redesign/fixture-proof.json. Browser access
still reports no enabled apps/browsers; requested enablement while continuing independent
work. Authorized deployment will be reported with visual verification explicitly pending.


## Redesign merge authorization review — 2026-10-28

Commit3ab30cb66b25be2e80fd64eeecf5986a8f349f97 pushed on rewrite/slim-graph-controller;
PR#7 open and mergeable with only task-owned files. Automatic approval review rejected
the exact-head REST merge: it judged cpmd insufficient explicit authorization for merging
the redesign into the default branch. No workaround attempted; merge and deployment not
performed. Owner must explicitly confirm merge/deployment. Existing vchrist release remains
unchanged. Local synthetic fixture stopped; Release binary and evidence retained under
build/codex/speed-release and /tmp/codex/build/core-ci/work-table-redesign.


## Redesign deployment after explicit cpmd clarification — 2026-10-28

Owner explicitly clarified cpmd as commit/push/merge/deploy. Merge retry accepted; PR#7
merged as876139d48a79ad996891677f1ff6952e07d4762a, source commit3ab30cb. Source tree
equals merged tree. Release binary
5b7a706e8b50e77fa9884776202e2c41c44d8077637779779abc0e5798f0d757
deployed on core.vchrist.at, service PID228323. Backup
/var/backups/core-ci/ui-columns-20261028T205754Z. Generation3116 and100 executions
restored unchanged, preserving attempts, assignments and run identities. No active work at
restart; v2.1.8 retains20 published targets. Existing service arguments,21 runtime libraries
and workflow pins unchanged. HTML/CSS/JS match source over loopback and public8080; TLS
public status responds. No new tag, build, retry or pipeline mutation. Final UI production
+57/-24/net33 and fixture+16/-0/net16; zero C++/SQL/API/workflow change. Build, format,
syntax, source semantics and public fixture/SSE checks pass; actual browser rendering,
interaction, zoom/locales and dark appearance remain unverified because browser access
is unavailable. Original synthetic fixture required SIGKILL after normal termination
remained ineffective; its port34591 is verified closed. No production process affected.
Deployment record remains local, preserving unrelated documentation changes.


## Independent execution expansion and compact columns — 2026-10-28

Owner requires arbitrary simultaneous open builds and explicitly authorizes cpmd after the
fix. Invariant: each execution owns its disclosure independently; opening, closing, SSE
refresh or prerequisite navigation must not close unrelated executions. Replace the single
executionId with one Set of expanded IDs, prune only IDs absent from the authoritative
graph, and snapshot each visible open technical disclosure by its panel ID before redraw.
No second authority, persistence, network mechanism or backend change. Filtering retains
row expansion choices. Technical disclosure state is retained for visible rows across SSE;
closing/hiding a row does not create a new persistent technical-state cache.
Owner additionally requests narrower Package/State and equal heading typography. Package
208px, State128px, table1048px with1050px wrapper; no unused width assigned to Package.
Publication verified uses the same strong element/shared text colour/12px/700 rule as Build
and Publisher job; its156px column accommodates that face. All timestamps/target info kept.
Production UI+19/-19/net0, tests0 repository lines; C++/SQL/API/workflow changes0.
Focused actual-app synthetic DOM probe (jsdom installed only under permitted temporary
checks) reproduces old exclusive expansion failure, then passes24 simultaneous disclosures,
independent close, per-row technical state across standard graph events, additive dependency
navigation, filter round-trip, pruning removed IDs, header styles and column widths, with0
fetch requests. It is DOM/logic coverage, not browser geometry. Probe/logs under
/tmp/codex/build/core-ci/multi-expansion/. node --check and final Release build pass;
changed production lines<=160 and git diff --check pass. Existing C++ unchanged, earlier
format-check remains applicable. No live graph mutation/new build/tag/retry. Browser visual
verification remains unavailable; cpmd stages will verify exact served assets and durable state.


## Independent expansion deployment — 2026-10-28

Source49f90ee6cabb64e837cf26e97b2f72c6bab7f2fc pushed on rewrite/slim-graph-controller;
PR#8 merged asf98a3901b1fb82b4ae86c247e5faa2416ae7f876. Source/merge trees identical.
Release binary46bc1b313088f054f4cacc0877534592e25d012bcc5664c1edba88ae80ffcf60
deployed on core.vchrist.at, PID228594; backup
/var/backups/core-ci/ui-columns-20261028T211305Z. Generation3116 and100 execution
records restored unchanged, preserving attempt/assignment/run identities; no active work
at restart. Fresh v2.1.8 remains20 published. Same service command/workflow pins/runtime
libraries and listener settings. HTML/CSS/JS match source through loopback/public8080;
TLS public status responds. Synthetic DOM additional checks confirm common heading colour
and font family as well as12px/700.24 simultaneous disclosures, independent close, SSE
retention, dependency navigation, filters and removed-ID pruning pass with0 fetch requests.
Browser visual geometry remains unverified (no enabled browser). UI+19/-19/net0, tests0
repository lines, C++0. Diagnostics and jsdom dependency remain only under
/tmp/codex/build/core-ci/multi-expansion for reproducibility; no repository dependency added.
No new tag/build/retry. Deployment record local alongside preserved unrelated documentation.


## Prerequisite navigation scroll — 2026-10-28

Owner reports that prerequisite navigation opens the execution but does not move the view.
Invariant: explicit prerequisite navigation reveals and scrolls to that exact execution's
summary, preserving other open rows; SSE and ordinary disclosure changes must not scroll.
Trace: dependency button -> showExecution -> clear filters/work tab -> renderWork. Added
one native scrollIntoView after synchronous render, locating the summary via its existing
panel ID. inline nearest preserves horizontal position where possible; missing graph targets
are safely ignored. No new callback, state, timer, API or network mechanism.
UI production+1/-0/net1, within approved redesign20–40 net-line bound (cumulative34).
Repository tests0, C++/SQL/workflows/API0. Actual-app synthetic DOM probe reproduces old
failure, then passes correct summary targeting after opening and after filtered already-open
navigation, no scrolling from SSE/disclosure, missing-target handling and all24 simultaneous
expansion checks. Native scrolling is observed through its DOM boundary; physical browser
viewport movement remains unverified because browser access is unavailable. Probe/evidence:
/tmp/codex/build/core-ci/prerequisite-navigation/{probe.cjs,before.log,after.log}.
Node syntax, changed-line<=160 and diff checks pass. Continuing owner cpmd authorization
for these Work-UI fixes; preserve unrelated working-tree documentation and deployment state.


## Prerequisite scroll deployment — 2026-10-28

Sourceea38ff6abcadf93118c1b0d12024a37604f5a3a4 pushed on rewrite/slim-graph-controller;
PR#9 merged asdd09c702ae5e3d9447af7a3c396aadc7cbbd7f2a. Release source matches merge
tree; deployed binary68a093f3292ffbe0a3c2f838ec99644cc289e2f8765fe971da2e1e1bca1933af
on core.vchrist.at, PID228868. Backup
/var/backups/core-ci/ui-columns-20261028T212021Z. Durable generation3116 and100
executions unchanged across restart, with attempts/assignments/run identities retained;
no active work at restart; v2.1.8 remains20 published. Same listener/service arguments,
workflow pins and runtime libraries. All3 served assets match source over loopback and
public8080; TLS public status responds. UI+1/-0/net1, repository tests0, C++0. Final Release
build/syntax/diff and targeted actual-app DOM probe pass; physical viewport motion remains
unverified with no available browser. No tag/new build/retry. Deployment record local.


## Claude current-source architecture review requested — 2026-10-28

Owner explicitly requested a deep Claude review of unnecessary constructs/architecture
against the small externally triggered graph/state-machine contract. LOC allowance is
accepted and not a defect by itself. Deployed dd09c70/sourceea38ff6 checkpoint and exact
service/binary/graph restoration facts supplied. Claude Opus5.5 started with only
Read/Grep/Glob, safe/restricted modes, no mutation/live-access tools and no delegation.
Prompt prohibits private directories/configs/logs/keys and discarded source/history;
current code/AGENTS are factual authority. Coverage includes C++ control/data/lifetimes,
worker/standard GitHub trust boundaries and UI/presentation as relevant. Require concrete
file/line evidence, all callers, deletion-first remedies, preserved invariants and clear
separation of defects, real simplifications, optional preferences and policy decisions.
Prompt/stream under /tmp/codex/build/core-ci/current-architecture-review/. Review is in
progress, not yet a result. Concurrent follow-up is one cosmetic chevron CSS-selector
change, with no C++/worker/JS difference from the supplied architecture checkpoint.


## Chevron hover styling — 2026-10-28

Owner requests no underline on disclosure triangles. Narrow the existing text-link hover
selector with :not(.chevron), preserving prerequisite-name underlines without adding a
competing override. Icon hover uses ordinary button affordance. CSS+1/-1/net0; C++/JS/tests0.
Final Release build, changed-line<=160 and diff check pass. No new behavior mechanism.
Keep this cosmetic source delta separate from Claude's dd09c70 architecture checkpoint;
controller/worker/application logic is unchanged. Continue existing UI cpmd authorization.


## Chevron hover deployment — 2026-10-28

CSS-only sourcee78923e pushed; PR#10 merged asa11d98b7b3c401168e36a1029218631ba01d0f6e.
Release binary3c0aafd9449fc791b85b26fb67be60bfbed108712021ad7f8b307b79f44e4139
deployed on core.vchrist.at, PID229265. Backup
/var/backups/core-ci/ui-columns-20261028T213047Z. Durable generation3116 and100 execution
records unchanged across restart; existing attempt/assignment/run IDs preserved. Latest
root-triggered v2.1.8 session remains20 published. Service command, libraries, pins and
listeners unchanged. All served UI assets match source through loopback/public8080; TLS
public status responds. CSS+1/-1/net0, C++/JS/tests0. No new build/tag/retry. Browser hover
remains unverified because no browser is enabled. Claude architecture checkpointdd09c70
differs only by this selector; exact delta recorded for review follow-up under
/tmp/codex/build/core-ci/current-architecture-review/current-deployment-update.json.


## 2026-10-28 — Claude architecture review and corrected status contract

Actual Claude CLI completed a read-only detailed source review at ea38ff6 / merge dd09c70.
Full findings and follow-up are recorded in docs/reports/claude-architecture-review-dd09c70.md.
Final source e78923e and deployed merge a11d98b differ only by the chevron hover CSS selector
from that review checkpoint; Claude confirmed no findings depend on that cosmetic delta,
using the supplied diff without a second source audit or live inspection.
Claude withdrew state-only public refresh: required within-state progress, timing and links
must remain visible. Suppression of unchanged public facts remains a candidate to validate.
Overall verdict: problem-aligned, no broad rewrite warranted. Four candidate defects and
four simplification findings still require independent code validation before changes.
No production/test changes, performance measurements or live CI actions occurred for this
review discussion. Report and this log entry remain local; no review fixes are authorized
or implemented by this record. Existing unrelated changes were preserved.


## 2026-10-28 — Validated Claude review corrections

Canonical checkout /home/voc/projects/snodec/core-ci, owned rewrite/slim-graph-controller,
HEAD e78923e; starting origin/main a11d98b has the same tree. No open PRs.
Validated each review finding against current source and discussed concrete counterexamples
with actual read-only Claude CLI. Agreement, prompt and final review are recorded in
docs/reports/claude-review-corrections-e78923e.md and
docs/proposals/review-corrections-agreed-prompt.md.
Implemented F1/F3/F4/S1/S2/S4: enrollment guards tolerate independent progress while
preserving namespace and same-row races; post-PUT import uses exact SHA; real UI fields;
public-facts status comparison in one consumer, latest capture after approval, uncertainty
gate and no replay; synchronous list and existing bool in-flight responsibility.
Claude withdrew unsafe preflight-SHA recovery filtering, receipt dry-run deletion and
repeated-startup claim. State-only refresh remains withdrawn; within-state progress retained.
F2 over-100 recovery remains unimplemented pending the owner's separate bounded-pagination
choice and growth approval. No new retirement/retry/DB recovery policy or persisted field.
C++ 3576 ->3572, runtime source/UI net -5; eight CMake test registrations separately counted.
Twenty affected checks passed (12 existing, eight new); Release build, canonical format-check,
JS syntax, AST and diff checks passed. Actual app.js synthetic jsdom probe passed, not browser.
Initial fixture-byte/reimport/immutable-URL assumptions and seeded generation were corrected;
a rejected JSON iterator conversion was reverted after the existing boundary test caught it.
/tmp disk-full startup failures were rerun after cleaning only stopped task-owned synthetic
datadirs, preserving logs/traces. All final regressions passed. Synthetic comparable workflow
HTTP counts: baseline818/178statusPOSTs ->660/124; event-order dependent, no live speedup claim.
Unchanged eight-report burst retains zero outgoing requests.
Claude's final source review found no remaining production defect within implemented scope;
its failed-approval coverage gap is now covered. Negative timing assertions are bounded.
Nothing committed/pushed/merged/deployed, no live CI mutation or acceptance run. Existing
three other dirty docs remained byte-identical; WORK_LOG appended only; private dirs untouched.


## 2026-10-28 — Review correction promotion authorized

Owner requested cpmd: commit, push, merge and deployment of the reviewed correction set.
The separate F2 pagination question remains unanswered and outside this promotion.
Verified final production diff matches the tested identity; all20affected checks and
formatting passed. Only task source/tests, review records and task log hunks are staged.
Deployment will use the existing private runtime and unchanged service/listener settings,
with durable graph backup/restoration and asset checks. No fresh build/tag is requested.


## 2026-10-28 — Review corrections merged, deployed and fresh chain triggered

Authorized cpmd completed: source1250f43c57dee10006cffe68083dac21257b09d5, PR11
https://github.com/SNodeC/core-ci/pull/11, main8f14486e2ac6d4f8af0cbe16442c9f949f88abf7.
Feature HEAD and main have identical trees. Only task source/tests/review records and
selected task WORK_LOG hunks were committed; unrelated dirty docs/private dirs preserved.
Local Release binary deployed to core.vchrist.at; SHA256
26947775b025fe5a1bc220a7293bac37537332cfdb4c3740158b6221a5464d46.
Service active PID230625, NRestarts0. All21 existing runtime hashes and service argv hash
unchanged. Protected consistent backup /var/backups/core-ci/review-corrections-20261028T223752Z.
All100 executions, attempts, assignments and run identities survived restart; full idle
graph stayed generation3116, 84published/15waiting/1failed. UI assets verified both locally
on host and externally, public8443status verified; ports/listeners/TLS unchanged.
Pagination remains excluded and pending specific approval. No new live correction policy.

Owner then requested a fresh full build from core. Created only VOC-Experiments/demo-core
refs/tags/v2.1.9 at current main27932aec43653be5f8dc08a03a7dfd1ea4efb9fb, preserving old tags.
Tag webhook created20new executions (r265–284) automatically, five core builds and15
dependent targets. GitHub first core workflows created22:40:04Z; allfive observedrunning.
GitHub status/demo-core.md already showsv2.1.9. No manual advancement/retry was used.
One external repositories read timed out; direct on-host API returned1.49ms and healthy
graph/status, so no controller-delay conclusion from that network observation.
Evidence under /tmp/codex/build/core-ci/review-corrections/deployment; this is launch proof,
not completed20target publication acceptance. Runtime monitoring snapshot follows.
{"controller_commit": "8f14486e2ac6d4f8af0cbe16442c9f949f88abf7", "core_commit": "27932aec43653be5f8dc08a03a7dfd1ea4efb9fb", "core_run_states": {"completed": 3, "in_progress": 2}, "core_tag": "v2.1.9", "fresh_states": {"building": 2, "publishing": 3, "waiting": 15}, "fresh_targets": 20, "generation": 3163, "github_status_blob": "a5941235267aea72d5092b33ac0cbf126ed4dfbc", "github_status_v219": true, "observed_utc": "2026-10-28T22:41:50.676096+00:00"}


## 2026-10-29 — v2.1.9 full-chain completion checked

Read-only check at06:13Z: all20fresh targets r265–284 are published; no failed or
unfinished target in this session. Controller generation3458, mutations_allowedtrue,
service active. Independent GitHub listing of the deployed canonical publication workflow
ID399803555 confirms20/20fresh publication runs success (no unfinished/unsuccessful runs).
Final target r283 publication verified2026-10-28T23:09:40Z (Oct29 00:09:40 Europe/Vienna),
29m36s after first core workflow creation22:40:04Z. This is end-to-end elapsed time, not
controller processing latency. Final status generation3458 run40785345279 succeeded,
updated23:10:10Z. Allfour GitHub status pages inspected; root showsv2.1.9 and dependent
pages retain their own selected source versions with new rebuild revisions. External
8080UI returns200; TLS publicstatus returns3458 and20/20published fresh rows.
No mutation, retry, reconciliation, code edit or new package-client test performed.


## 2026-10-29 — v2.1.9 timing attribution

Read-only reconstruction recorded in docs/reports/v2.1.9-timing.md; raw evidence under
/tmp/codex/build/core-ci/review-corrections/timing-v219/. Critical dependency path is
OpenWrt24.10 core r268 ->side r278 ->join r283. From firstGitHubcorecreation to final
verification29m36s:29m01s GitHubworkflowwindows,34s handoff/verificationgaps,1s rootfanout.
34s includeswebhookdelivery, GitHubREST/network, artifact/receiptproof, DB anddispatch,
not pureC++CPUtime. ThisrunhadnoCPUspantrace; oldv216probe not reused ascurrentproof.
Largestworkflow-internalgap sideIPK:22:51:34Z validationcomplete to23:02:14Z publisherstart,
10m40s; timestampsalonecannotseparate feedgroupqueue fromrunnerqueue. No codemutation
or retries performed. Initial selection before firstGitHubworkflow is outside measuredinterval.


## 2026-10-29 — publication wait and target editor investigation

Read-only investigation on rewrite/slim-graph-controller at1250f43 (deployed tree8f14486).
Report: docs/reports/publication-wait-investigation.md. Reconstructed all640s of sideIPK
r278 wait:509s blocking sideAPK r279,99s other publishers,12s status,20s handoff gaps.
APK publisher held feed group605s; download/extraction563s for285845912-byte validated
artifact including retained SDK. Push2s; signed preparation20s. Digest matched. Ordinary
log cannot distinguish slow stream/disk from debug-only Action retries; dependency has
inactivity timeout, not total deadline. Other OpenWrt downloads3–8s. No claim of640s
controller delay or proven organization-wide runner saturation. Exact whole-job mutex
confirmed in deployed canonical/status workflows. Independent preparation under this
mutex is the coupling; a cross-job handoff still needs transfer, so no handwaved prep-job fix.
Preserve current SDK reuse; recommend bounded tool-dependency/trust refinement before
implementation. Explained all target dialog fields, actual JSON, Apply draft semantics,
runner AND-label behavior, SDK inheritance and separate UI/server length/pattern mismatch.
Evidence /tmp/codex/build/core-ci/publication-wait-investigation/. No code/test/workflow
changes, tests, build, commit, push, deploy, retry or live mutation. Existing dirty docs preserved.


## 2026-10-29 — publication wait correction agreed with Claude; approval checkpoint

Owner requested discussion through agreement, then an implementation prompt and fix.
Claude session2c479bf5-90fc-4bae-ae99-726bdcdc3958 independently checked source/evidence.
Four rounds resolved status-sample overclaims, unsupported timing estimates, native empty-job
overhead, duplicate bulk handoffs, package-parser/executable trust, wrapper/runtime closure,
job conditions, both supported YAML contracts, helper signature and focused-test scope.
Agreed design and corrected prompt: docs/proposals/publication-wait-fix.md. Phase0 mandatory
owner approval is pending for one unprivileged OpenWrt-only tools job and a small verified
executable handoff, replacing whole-SDK artifact and publisherfullSDKsetup. Estimate+193net;
proposed productioncap+230net/260added, YAML+120net/Python+110net, tests+180net, C++0.
New mechanism/netgrowth not implemented under general fix instruction without quantified approval.
Both public SDKsha256verified; readelf closure2152850bytesIPK/4082672bytesAPK. Local minimal
IPKephemeralkey sign/verify+mkhash andAPKversion3.0.5 checks pass; not package/client/GitHub
acceptance. SDKtoolwrappers needhiddenbinaries+loader+runas+libs; not just three commandfiles.
Pinned Actiondocs/source verified hiddenfiles/modes/singleartifactlayout/jobif and artifact
runtimecredential; no live or local credentialvalues inspected. No package/source executed
in tools-design proof, only officialSDKtool commands above; no productionkeys. Tradeoff
bulktransfers4->3, noneunderfeedlock, extraOpenWrtjobhop andofficialdownloadoutside; no
speedupclaim. Evidence /tmp/codex/build/core-ci/publication-wait-fix/. No source/test/workflow
edits, builds, commits, pushes, deploys, tags, retries or live mutations. Unrelated dirtydocs preserved.


## 2026-10-29 — Claude review of OpenWrt build-worker proposal

Owner requested review only. Sent proposal to Claude session2c479bf5-90fc-4bae-ae99-726bdcdc3958;
independently checked findings and discussed smallestchange/counts through agreement. Report:
docs/reports/openwrt-worker-proposal-review.md. Exactdevelopmentexportreuse alreadyexists;
no integratedSDKpublishedbybuildjobs, but perjobdownload/imagecopy/privatecopy remain.
Agreed minimalcandidate removesSDKimagelayer/contexttransfer usingread-only/sdk mount for
bothcontainers and existingCORE_CI_APK argument; keepshookprivatecopy/pristinevalidator,
permissions/cleanup/nativebehavior. AllSDKsetup/imageconstruction is NOT eliminated.
In-memorycandidate ASTparsepasses:15added/15removed/net0production including12movedlines;
newoperationlinesmax100characters. Claude withdrewformatting-basedgrowthestimate. Tests
estimate45–60lines; noneeditedorrun. C++/controller/workflows0changes. No Dockeroperation,
commit/push/deploy/tag/feed/configurationmutation. Runtimeproof/speedupunverified. Pending
publicationtoolsjob/growthproposal separateandunapproved. Evidence under
/tmp/codex/build/core-ci/openwrt-worker-review/. Unrelateddirtydocs/privatepaths preserved.


## 2026-10-29 — OpenWrt build preparation and publication tool handoff

Owner approved both changes and the publication proposal's implementation bounds, then
instructed continuation. Canonical checkout: /home/voc/projects/snodec/core-ci; owned branch
rewrite/slim-graph-controller; HEAD 1250f43c57dee10006cffe68083dac21257b09d5. Remote main
8f14486e2ac6d4f8af0cbe16442c9f949f88abf7 has the same source tree. No open PR was returned.
Unrelated dirty documentation/review records are preserved. Private directories were not inspected.

Invariant: source artifacts supply package facts, never executable publication authority.
The pristine SDK is read-only in both build containers; the hook retains its private writable
copy. SDK bytes are excluded from the image/context. Native image/mount behavior is unchanged.
An unprivileged OpenWrt tools job independently prepares the official SDK outside the feed
lock, extracts bounded wrapper/ELF-library files and binds them to the exact assignment and
complete SDK environment. The publisher checks the trusted job-output manifest hash, every
file, identity and bounds before chmod/execution. The full SDK artifact and publisher SDK
preparation are removed. Signing/indexing/feed mutation stay inside the shared lock; the
push-step name, uncertainty interpretation, confirmation and receipts stay unchanged.

Production accounting against HEAD (added / removed / net):
- deployment/source/core-ci-publish-bundle.yml.in: 56 / 5 / +51.
- deployment/worker/.github/workflows/core-ci-publish.yml: 62 / 5 / +57.
- deployment/worker/core_ci/native.py: 15 / 15 / 0 (includes moved shared mounts).
- deployment/worker/core_ci/publish.py: 4 / 4 / 0.
- deployment/worker/core_ci/sdk.py: 96 / 1 / +95.
- deployment/worker/core_ci/sign_openwrt.py: 4 / 4 / 0.
- deployment/worker/core_ci/validate.py: 6 / 1 / +5.
Total 243 / 35 / +208; YAML +108, Python +100, C++ zero. Approved bounds:
added260/net230, YAMLnet120/Pythonnet110. Remaining: 17 added / 22 net.
Tests: openwrt.py 125 / 6 / +119; worker_environment.py 28 / 4 / +24;
worker_provenance.py 45 / 8 / +37. Total198 / 18 / +180, exactly the approved test bound.
Task deployment documentation adds seven lines; this log is append-only.

Validation configuration: build/codex/openwrt-worker, Debug, BUILD_TESTING=ON;
fixtures/TMPDIR/CCACHE_DIR under /tmp/codex/build/core-ci/publication-wait-fix/implementation/.
Build identity: HEAD above plus production/test diff SHA256
 b11a917f4e31e05cdb0a135d74ea3dc39ad17afe7980d182195c03a4088b65b4.
Baseline: ctest --test-dir build/codex/openwrt-worker --output-on-failure -j4
-R '^(worker-package|openwrt-archive|worker-provenance|worker-environment)-boundaries$':
4/4 passed before edits (1.94s). Same selection passed after production changes (1.85s).
Final expanded OpenWrt/provenance/environment checks: 3/3 passed (1.95s); package check
passed on the same unchanged production source (1.69s). These are test runtimes, not pipeline
speed measurements. cmake --build build/codex/openwrt-worker --target format-check passed;
Python AST, both YAML parses, git diff --check and changed production line limit passed.
A new minimal validator fixture initially lacked target; corrected and rerun successfully.

Official archive SHA256 checks passed. Production extractor/verifier on retained official SDK
inspection files produced IPK8files/2152850bytes and APK6files/4082672bytes. No SDK executable
was run. Fresh full extraction hit a full /tmp tmpfs; its partial directory was deleted,
freeing581MiB. A test attempt blocked by the same capacity issue was rerun successfully.
Fresh full-extraction proof remains unverified; existing extraction fixture checks pass.
Evidence: implementation/official-tools-results.json and Claude read-only reviews under the
same temporary task directory. Claude found no blocking production defect; its four test gaps
were addressed (validator outputs, exact job condition, file/symlink/APK cases, native mounts).

Bulk SDK transfers decrease from four to three, all outside the feed lock; an additional
serial OpenWrt job and official download remain outside it. Small artifacts and pip remain
inside the lock. No speedup or live acceptance is claimed. Real GitHub conditions/artifact
roundtrip, SDK container execution, IPK/APK signing/indexing and before/after timing still need
an authorized sandbox run. No commit, push, merge, deployment, restart or live mutation performed.

Claude follow-up confirmed all four gaps closed and no remaining concrete defect. The tools-job
condition assertion was then scoped to that job; its focused provenance check passed (2.20s).
Final review records: implementation/claude-review.json and implementation/claude-confirm.json.

Owner subsequently authorized commit, push and merge. Deployment/activation remains separate.


## 2026-10-29 — Retention options agreed with Claude

Owner requested discussion through agreement on retention options; no implementation requested.
Canonical checkout /home/voc/projects/snodec/core-ci, branch rewrite/slim-graph-controller,
HEAD8461764ff2131ef13e44ce187d1c84b1550f5d27 (merged main3cc14fd). Three read-only Claude rounds,
session2c479bf5-90fc-4bae-ae99-726bdcdc3958; final response confirms no remaining disagreement.
Evidence: /tmp/codex/build/core-ci/retention-review/round1.json, round2.json, round3.json.
Independently traced Graph.cpp assignment/select/blockers/retry, GraphStore.publicView,
FeedProof.verify, native.prepare_dependencies, environment.previous/main, contract receipts,
sign_native.retained_receipt, publish capacity check and feed_status receipt enumeration.

Agreed recommendation, not owner approval: configurable N successful source versions per
project and target, latest-only N1 valid, N3 suggested; keep the highest successful rebuild
revision per retained version, not two rebuilds and not the last completion timestamp.
Failed newer work does not displace a successful publication. Count versions independently
per target; simplest suggested stable/prerelease handling shares numeric SemVer ordering.
Protect the exact reference closure needed by active/uncertain work and by whatever retry
policy the owner chooses. This can exceed N and can retain multiple rebuilds. It is a union
of reachable references, not roots multiplied by graph depth. It is no universal package-count
bound and does not guarantee arbitrary external runtime dependencies or repair bad declarations.
No new general dependency resolver is proposed.

Current retries reuse first-attempt exact dependencies; workers fetch current public feed paths.
Published-only graph eligibility and receipt-based public links do not know package expiry.
Consequently a worker-only cleanup under the feed lock is insufficient: selection/retry can
create references after a protected-set snapshot. Before deletion, the controller must atomically
check protection and withdraw new-reference eligibility in its authoritative graph. Preserve
historical published outcomes and immutable receipts. Exact representation, availability display,
worker handoff and uncertain outcomes require refinement and growth/mechanism approval; no new
flag, schema, protocol, queue, timer or C++ addition is approved by this discussion. Uncertain
remote deletion is reported, never blindly replayed; withdrawn eligibility is not proof of deletion.

Owner policy options: (1) N including latest-only; (2) finite retry eligibility/explicit expiry,
or indefinite protected inputs accepting capacity refusals; (3) separately whether total Git
history storage must be bounded. Indefinite protection plus the existing hard900MiB site check
can preserve the bound by refusing new publications, at the cost of forward progress.
Reading exact bytes from immutable Git history is a possible separately designed worker path,
not an implemented fallback; it could preserve retry inputs while bounding the current feed,
but not Git storage. Normal deletion leaves old Git objects; compaction or different binary
storage is a separate owner decision, not authorized migration/history rewriting.

No production/test/configuration/deployment changes, tests, commits, pushes or live mutations.
Unrelated dirty documents and private directories preserved; this entry is append-only.


## 2026-10-29 — Orphan feed snapshots agreed with Claude

Owner requested read-only discussion through agreement after inspecting SNodeC CI. No
implementation or growth approval follows. Canonical branch rewrite/slim-graph-controller,
HEAD8461764ff2131ef13e44ce187d1c84b1550f5d27; current production/tests remain unchanged.
Verified SNodeC/DistributionPackages ci/publish/push.sh creates commit-tree without a parent
and pushes with an explicit expected-head lease. Status updates in ci/publish/status.sh add
one parent. Live SNodeC/Packages history sample: HEAD6c1007acd52173a8efb329e573caa8ea5c4bec53
has parentd496378a4d41adf80617b5a1058275de4dcf223e, which has no parents. Independent cleanup
there uses30days; that policy/retry loop is not adopted. No SNodeC resource was modified.

Two Claude rounds, session2c479bf5-90fc-4bae-ae99-726bdcdc3958, reached agreement. Evidence:
/tmp/codex/build/core-ci/retention-review/orphan-round1.json and orphan-round2.json.
Direction: retain GitHub hosting, package publication creates a parentless current-feed
snapshot, explicit lease against checkout HEAD captured before root creation, existing shared
feed lock, no mutation replay. Status commits may append parents until the next publication.
Root creation only removes ancestry: actual expired files must also leave the current tree;
other references retaining discarded history need checking. GitHub controls physical GC timing.
Owner chose3 successful versions/project/target, one highest successful rebuild, retries may
expire, no payload archive or history-recovery fallback. Active/uncertain inputs remain protected.

Source validation corrected Claude's claim that FeedProof necessarily breaks: Pipeline.complete
calls it only for active publishing work, coalesces per attempt/stage, and reconciliation excludes
published outcomes. FeedProof can observe an identical receipt in a newer root for late first
verification. feed_revision is recorded observation context, not a worker dependency download
address; confirm validates receipt HTTPS bytes, not commit reachability. No compatibility or
re-verification bypass is justified. Focused repeated-root/late-proof tests remain required.
Lease errors retain current push uncertainty; no new definite-failure inference is approved.

Retention candidates must use already-verified successes, never prospective publications.
Protect unverified receipts and exact active/uncertain inputs, withdraw new-reference eligibility
atomically before remote deletion. Temporary publication overlap, representation, deletion
handoff, replay/status rendering, bounds and LOC estimates require design refinement before
implementation. Suggested bounded feed receipts/status metadata follows retained versions;
receipt deletion is not yet owner-approved. MariaDB audit retention is separately scoped.
No production/test edits, tests, commits, pushes, activation, deployment or feed mutation.
Unrelated dirty documents/private directories preserved. This entry is append-only.


## 2026-10-29 — Claude retention task prompt

Owner requested Claude's prompt only. Requested a self-contained task based on the agreed
three-version/one-rebuild/expiring-retry/no-archive policy and orphan feed snapshots.
Reviewed the first draft; Claude corrected source wording, protection of superseded history,
Git lease terminology and the lack of a finite cleanup deadline without a completion trigger.
Final prompt: docs/proposals/publication-retention-task.md. Original responses under
/tmp/codex/build/core-ci/retention-review/implementation-prompt.json and
implementation-prompt-final.json. Same Claude session as the preceding agreement.
The prompt requires a concrete source-based design, per-file estimates and explicit architecture/
growth approval before production edits. No implementation, test, commit, push, activation,
restart, deployment or feed/history mutation performed. Existing dirty files preserved.


## 2026-10-29 — Retention design and line-estimate checkpoint

Owner invoked the phase-gated task and emphasized reducing code before any additions.
Verified canonical checkout/remote/default main and both exact heads: owned branch
rewrite/slim-graph-controller HEAD8461764ff2131ef13e44ce187d1c84b1550f5d27, merged main
3cc14fd9dfaba872dacb82e57d80f70c628c50fc. No open PR, no source drift; existing dirty docs
preserved. Did not switch branches, create a checkout, inspect private directories or mutate GitHub.
Counted src/*.cpp and src/*.h:3572 physical lines, current cap3576, headroom4.
All five requested prior Claude evidence files are present and read. Read actual Graph,
GraphStore, Database FIFO, Pipeline approval/dispatch/binding/completion/reconciliation,
FeedProof, Version, worker input/signing/index/SDK/projection code, both workflow templates,
status workflow, UI retry presentation and relevant graph/store/pipeline/worker/UI fixtures.

Proposal: docs/proposals/publication-retention-design.md. One authoritative eligibility
annotation plus explicit durable cleanup mutation evidence in the existing graph. No second
queue/engine/timer/transport/auth callback. Reuse existing publisher in cleanup-only mode;
verified publication or released protection triggers it, including the final target with no
future build. Package publication and cleanup use root snapshots and captured-old-head leases;
status stays parented. Pure next-publication cleanup is rejected as incomplete. Reuse shared
validation, signing contexts and index functions, extracting OpenWrt index code instead of
copying it. No FeedProof compatibility layer or new SemVer/dependency resolver proposed.

Source constraints: exact frozen retry inputs, current-feed downloads, shared DEB architecture
pool/Release, superseded waiting exclusion, protected unverified receipts, and per-object package
versus export needs all affect expiry. New logical expiry is not proof of physical deletion.
Normal ranking is3 versions/highest successful revision; runtime prerequisite/protected extras
can exceed that count. Unknown POST/push remains visible and is not automatically replayed.
No lower-version/failed candidate evicts a verified third publication. No archive/history recovery.

Forecast (not a patch or granted budget): C++159added/62removed/net97, projected3669lines;
Python206/92/net114; YAML48/8/net40; UI/templates7/3/net4; CMake4/0/net4; production424/165/net259.
Test forecast283/13/net270. Requested ceilings equal the forecast without contingency: C++net97/total3669,
Pythonnet114, YAMLnet40, UI/templatesnet4, CMakenet4, totalproductionnet259, testsnet270.
The extra operation is required to finish cleanup without a subsequent publication. Reductions
alone cannot supply eligibility withdrawal or safe remote-deletion evidence, which do not exist.
No implementation will start under the existing four-line headroom.

Local diagnostic only: empty synthetic Git repos proved root ancestry replacement, orphan alone
retaining old files, explicit deletion removing them, stale lease rejection, status parenting,
and unchanged receipt path history moving to a newer root. Evidence:
/tmp/codex/build/core-ci/retention/git-snapshot-results.json. Removed only these new temporary
Git fixtures afterwards. No production FeedProof/MariaDB retention or real GitHub test, physical
GC assertion, build, full suite, signing test, deploy, tag or feed operation. git diff --check passes.
Production and test edits zero; design document and this append-only work-log entry only.

STOP at Phase2: owner decisions are bounded receipt/status policy (including export availability),
shared stable/prerelease SemVer ordering, and the explicit expiry/cleanup mechanisms and line
bounds. Per-file estimates and rollout/uncertainty/test gates are in the proposal. MariaDB audit
purging is separate scope. No commit, push, merge, restart, activation or implementation.


## 2026-10-29 — Retention implementation starts; revised C++ bound checkpoint

Owner approved the retention design's three decisions and initial quantified bounds: net259
production/net270tests, C++total3669 (+97), Python114/YAML40/UI4/CMake4. No commit/push/
merge/deployment authority. Approved feature branch feat/publication-retention was created
from current main3cc14fd9dfaba872dacb82e57d80f70c628c50fc; all four unrelated dirty documents
retained identical SHA256 hashes across the transition. No source checkout/reset/stash/rebase.
Current HEAD remains main's merge commit. No open PR. Canonical formatting files/modules
match byte-for-byte. Read SNode.C HTTP client/event-loop and MQTTSuite application patterns;
no framework API/lifecycle change is planned. No private directory contents inspected.

Four affected worker baseline checks passed (1.92s). Reduced shared Git helper first:
publish.prepare_commit now supports explicit approved deletions and a parentless snapshot;
returns the captured old head. checked_push supports an explicit SHA lease with no retry;
status callers keep normal parented commits/fast-forward push. The ordinary main publication
path is not activated to orphan mode yet. Production21added/23removed/net-2 Python lines;
new tests25added/net25. Focused worker-provenance-boundaries passed (2.43s), including real
isolated Git root/tree/deletion/status/stale-lease checks. No C++ production edit or live change.

My initial controller estimate was too low. Source-based derivation draft canonical formatting
produced147physical lines before guards/helpers/shared GitHub binding/public views. It is a
partial uncompiled design draft with explicit remaining cases, not tested application source
or a claim that no shorter algorithm exists. Evidence under
/tmp/codex/build/core-ci/retention/controller-derivation-{draft,formatted}.cpp.
Full graph references, exact frozen attempts, active/runtime/export distinctions, bounded
commands and duplicate/uncertain-operation ownership cannot be dropped to meet a line count.
I stopped before applying C++ changes, as required by the approved bound.

Revised per-file estimate: Graph.cpp158/16/net142; Graph.h3/0/net3;
Pipeline.cpp75/42/net33; Pipeline.h3/1/net2; GraphStore.cpp13/3/net10.
C++252added/62removed/net190, total3762 (+93 beyond approved3669). Other estimates unchanged:
Python114/YAML40/UI4/CMake4, totalproductionnet352 instead of259; testsnet270 unchanged.
No additional policy/mechanism requested. Approval needed only for revised growth; continue
reductions first, never delete verification or move controller policy to workers to evade it.
Design checkpoint updated with current authority and exact partial implementation status.
Source remains canonical on the task branch, uncommitted. Unrelated docs preserved. No
commit, push, merge, install, restart, tag, live orphan push or activation performed.

### Retention revised implementation bounds approved

The owner approved C++ net +190 (3,762 physical lines), total production net +352,
and test net +270. The Python +114/YAML +40/UI +4/CMake +4 estimates remain unchanged.
Continue on feat/publication-retention at base 3cc14fd9; no commit, push or live activation.

### Retention implementation checkpoint — remaining C++ integration bound

Canonical checkout /home/voc/projects/snodec/core-ci, feat/publication-retention,
HEAD 3cc14fd9dfaba872dacb82e57d80f70c628c50fc; no commit or remote mutation.
Source/test diff SHA256: 7bd4266fcea5647e835bd082eab45ac3b3242a4fe2a3e851e17ed379c16984de.

Current implementation: shared exact-input eligibility, three-version/highest-revision
retention derivation, package-only transitive closure, explicit bounded cleanup commands,
per-scope outstanding-operation exclusion and atomic expiry annotations. The derivation is
not yet connected to Pipeline changes. Existing duplicate preflight/dispatch callback handling
and a redundant per-node copy were removed. Package preparation now creates orphan snapshots
and passes its captured expected head to the checked push; status defaults remain parented.
No controller expiry or cleanup is activated or deployed.

Measured task source changes against HEAD: C++ 213 added/28 removed/net185,
Python26/26/net0, YAML4/0/net4; production243/54/net189. Tests73/3/net70.
Physical C++3,757, five below the approved3,762. No CMake/UI changes.
The revised C++ estimate is still insufficient: shared cleanup lookup/enumeration about16,
Pipeline dispatch/completion/reconciliation reuse20, view/projection10 and remaining guards7
need about53 net lines beyond this source. Request total C++3,810/net238 and total production
net400, increasing each approved ceiling by48. Other category estimates and test cap270 stay
unchanged. No new mechanism or policy is requested. Stop before adding that integration.
Remaining worker deletion/index reuse, cleanup workflow mode, views, persistence/protection/
batching/restart and late FeedProof tests are incomplete; this is not acceptance.

Validation at this exact source: core-ci and graph-test built, format-check passed; no changed
production line exceeds160 characters; git diff --check passed. Commands:
- TMPDIR=/tmp/codex/build/core-ci/retention cmake --build build/codex/slim-controller --target core-ci format-check -j2
- TMPDIR=/tmp/codex/build/core-ci/retention cmake --build build/codex/slim-controller --target graph-test -j2
- PYTHONDONTWRITEBYTECODE=1 ctest --test-dir build/codex/slim-controller --output-on-failure -R '^(graph-state|worker-provenance)-boundaries$':2/2 passed,2.42s.
- PYTHONDONTWRITEBYTECODE=1 TMPDIR=/tmp/codex/build/core-ci/retention ctest --test-dir build/codex/slim-controller --output-on-failure -R '^pipeline-(chain|overlap|preflight-publish-known)$':3/3 passed,28.83s.
The provenance fixture first failed because its prepare_commit mock did not return the new
captured-head output; the fixture now supplies that head and checks the added public output.
This is local/synthetic GitHub evidence with isolated real MariaDB, not live throughput.
The existing unchanged burst assertions remain intact and passed with zero outgoing requests.
The reused build initially retained its older fixture directory; reconfigured
CORE_CI_TEST_DIRECTORY=/tmp/codex/build/core-ci/retention/fixtures before final checks.
Build logs and temporary diagnostics remain under the task directory. No additional source
checkout. Unrelated document hunks/review files and private directories remain untouched.
Commit, push, merge, install, restart, activation and live acceptance remain unperformed.

## 2026-10-29 — Retention local implementation and final approved-bound validation

The owner approved the revised 3,810 physical C++ / +400 total production / +270 test bounds.
Canonical checkout /home/voc/projects/snodec/core-ci, branch feat/publication-retention,
HEAD/base3cc14fd9dfaba872dacb82e57d80f70c628c50fc. Remote main/HEAD still matches;8461764 is
its merged ancestor. Source/test dirty SHA2567645ab2de7dec0c625c3567ed51e571484ac57bdebd383376abd748ecc089c63
includes the explicit new retention.py bytes. Nothing staged, committed, pushed or deployed.

Implemented atomic exact-input expiry and cleanup authority in the existing graph/Database FIFO,
three successful SemVer roots/highest revision, active/uncertain/frozen retry protection and runtime
package closure without permanent audit-only exports. Shared physical coordinates now match actual
OpenWrt publication roots. Bounded per-object cleanup batches use the existing approved publication
workflow, HMAC/run/job/attempt binding and reconciliation path. No timer, queue, controller thread,
new endpoint or replay. Failed/uncertain cleanup keeps its scope assigned/paused. The last publication
or consumer release triggers cleanup without a future build. Unchanged reports remain no-ops.
Worker verifies all granted bytes before local deletion, shares native/OpenWrt signing/index paths,
creates parentless snapshots and pushes with the exact previously captured old-head lease. Status
commits remain parented;900MiB guard, normal package validation and FeedProof stay unchanged.
Views show input expiry and cleanup evidence; package rows omit removed components. History stays.

Measured production639added/306removed/net333; tests287/17/net270. Physical C++3,809 from3,572.
Headroom:67 production overall, one C++ line, zero test lines. AGENTS.md records the newly approved cap.
Per-file/category counts and documentation accounting are in docs/reports/publication-retention-local-validation.md.
Unrelated co-owned document hunks and existing review records preserved; private directories untouched.

Final build: TMPDIR=/tmp/codex/build/core-ci/retention cmake --build build/codex/slim-controller
--target core-ci graph-test graph-store-test format-check -j2: passed. Final graph-state/worker-package/
openwrt-archive/worker-provenance boundaries4/4passed6.79s. Isolated real-MariaDB selected cases9/9passed:
persistence-graph, persistence-graph-missing, pipeline-chain/overlap/recovery/preflight-publish-known/
status-delayed-approval/retention and worker-environment-boundaries. Final phase-loop edit then reran
pipeline-chain10.74s and pipeline-retention3.50s, both passed. Full commands, identities and failure
corrections are in the report. Node syntax/Python parsing/format/changed160-column/diff checks passed.
No unrelated full suite. Browser inventory empty; iab unavailable, so no browser acceptance claim.

Synthetic chain514requests includes17tokens/86statusPOSTs/40executionPOSTs/20artifact lists;
retention224requests includes11tokens/32statusPOSTs/14executionPOSTs/4artifact lists. All fixture
outgoing requests counted; GitHub worker/Git/Pages traffic does not run here. Existing unchanged burst
and cleanup terminal duplicates produce zero outgoing requests. Status cost remains material; no new
latch/debounce/cache hides it. Local synthetic timings are not live throughput or a before/after speedup.

Activation gap: historical retries preserve O tooling/workflow pins, whose publisher makes parented
commits. Draining assigned work alone cannot enforce orphan roots for future such retries. No pin
rewrite, blanket old-generation expiry or compatibility mechanism implemented. Resolve that concrete
upgrade disposition before activation, plus canonical old-path compatibility and actual feed lease
permissions. No live feed inspection/mutation, installed-client check or GC timing proof here. Unknown
cleanup can pause deletion indefinitely; protected extras may exceed3. Old-controller rollback cannot
ignore expiry. Source work is not live acceptance or complete activation readiness. No stage approval
was inferred. Task probes/logs/JSON remain under /tmp/codex/build/core-ci/retention; stopped own fixture
DB data removed safely for tmpfs space. Persistent build remains under build/codex/slim-controller.

## 2026-10-29 — Retention promotion authorized

Owner requested cpmd and a fresh full build chain from core: commit, push, merge, deploy
and launch in VOC-Experiments. This authorizes those stages for the retention change.
Before promotion, main remains3cc14fd and the final production/test diff identity matches
local validation. Live controller generation3458 is idle:104published/15waiting/1failed.
No active build/publisher is running; the one failed demo-corev2.1.4 is historical.
Historical immutable publisher pins remain an explicit activation decision, pending owner
response; source promotion and release preparation continue independently.

## 2026-10-29 — Retention source merged; concrete activation failure found

Owner cpmd authorized source commit98efef368992ac2668e55bb7f4218a0e1dbedec4, pushed
feat/publication-retention and merged PR13 https://github.com/SNodeC/core-ci/pull/13 as
2b4482729dd239b9aef0f17f55e48d8a550577c9. Source and merge trees match exactly.
Staged only explicit task paths and selected WORK_LOG/deployment-guide hunks; unrelated
document content remains local. Release build under build/codex/speed-release and canonical
format-check passed; production/test source unchanged from the locally validated commit.

Read-only live preflight: generation3458,120executions,104published/15waiting/1failed,
no active assignments. Feed has no rulesets. Inspected only execution/public deployment
facts and public Git trees, not credential/configuration values.44 verified recorded package
paths still exist outside the canonical layout, including apt/debian/trixie/pool/... and
nested openwrt/.../ipk/<sdk>/... paths. Existing failed demo-corev2.1.4 also retains older
immutable publisher pins. Owner retry-policy clarification remains pending.

A temporary diagnostic links the existing Graph domain library and calls graph::retain on
an offline snapshot of actual execution facts. It derives20commands/182objects, zero missing
objects, and all20commands include one or more of the44unsupported historical paths. This
is offline derivation, not live graph mutation or package deletion. The worker's current
canonical-directory guard would reject those commands. New activation therefore requires
a concrete historical-path/index correction; no guard was weakened, no pin rewritten and
no cleanup error was knowingly dispatched. The running service, feed and tags are unchanged.
Deployment and requested full chain are NOT performed. Release/probe hashes and exact
preflight evidence are under /tmp/codex/build/core-ci/retention/promotion/. This post-merge
record remains local; no live acceptance is claimed.

## 2026-10-29 — Same-key sandbox signing validity renewal

Owner explicitly approved extending the existing APT/RPM key by one year. Public fingerprint
A4B307A9CFD583089547481DE58C165F4340EE99 is unchanged; new expiry 2027-10-29T19:52:02+00:00.
Renewal job https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40943855027
verified the fingerprint and sealed the renewed secret directly to GitHub environment secret
encryption. Only GitHub-encrypted secret data/public metadata left the job. The encrypted API
value was applied to the existing package-publication APT_SIGNING_KEY; no new credential or
permission scope. No signing-key plaintext inspected or placed in source/artifacts/logs.
Public worker key metadata is updated; no compatibility code, C++/Python/YAML/controller
logic change. Native maintenance failures remain visible, before push, pending explicit recovery.

### Live publication queue diagnosis — 2026-10-29

Read-only GitHub run/job evidence for fresh core v2.1.10 Debian amd64 r318:
build finished 20:01:47Z; publication run 40945362987 created 20:01:56Z;
artifact validation finished 20:02:13Z; skipped OpenWrt tools prerequisite settled
20:02:14Z; feed-writing job started 20:05:56Z and finished 20:06:23Z;
the checked push itself ran 20:06:18–20:06:20Z. The 9-second build-end to
run-creation interval includes webhook delivery, controller evidence/proof/dispatch
and GitHub creation; it is not a measurement of local C++ processing alone.

The fresh chain was launched before the approved native cleanup recovery backlog
had drained. Cleanup r314 ran 20:03:32–20:04:09Z, r315 20:04:13–20:04:39Z,
r316 20:04:44–20:05:06Z and r317 20:05:10–20:05:30Z. Status run 40945247595
then occupied the feed-writing boundary 20:05:35–20:05:52Z, before r318 started.
Actual sandbox publication/status workflow blobs both use the same feed concurrency
group with queue:max and cancel-in-progress:false. Thus this sample's dominant delay
was the shared feed queue behind maintenance, followed by a status write; it does
not establish a multi-minute controller dispatch delay. Launching this timing run
before maintenance finished contaminated the baseline. All 21 maintenance commands
subsequently reached verified completion. No source, workflow or deployed-state
mutation was made for this diagnosis.

### Push-to-verification timing defect — 2026-10-29

Additional read-only evidence distinguishes publication queue time from verification:
APK r322 checked push ended 20:07:48Z; confirmation job ran 20:07:53–20:08:09Z;
controller verified at 20:08:14Z (26 seconds after push, five seconds after job end).
IPK r321 checked push ended 20:08:25Z; confirmation ran 20:08:31–20:08:56Z;
controller verified at 20:09:04Z (39 seconds after push, eight seconds after job end).
Those five/eight seconds include report delivery and remote proof, not solely local C++.

Native r318/r319/r320 pushed successfully at 20:06:20Z/20:06:49Z/20:07:27Z.
Their confirmation jobs were all skipped. The live workflow and source template have
confirm depending on publish with an ordinary kind-only condition, without an explicit
status-check function; the intentionally skipped native tools ancestor propagates the
skip down the dependency chain. GitHub's official jobs.needs documentation describes
this propagation. Publisher itself explicitly handles this case; confirm does not.
Controller first proof failures are timestamped 20:06:29.176Z/20:06:58.542Z/
20:07:36.183Z, reporting public HTTPS receipt bytes did not match Git. Completion
releases its in-flight read but retries only when evidence changed or reconciliation
occurs. No later confirmation job means no normal later terminal wake-up here.
At 20:12:46Z all three public receipt bodies matched current Git bytes exactly while
all three graph states remained publishing. Thus this is a real workflow/controller
integration defect, not simply continuing Pages propagation. Minimum observed post-push
pending intervals were 6m26s/5m57s/5m19s. No reconciliation or workflow mutation was
performed during diagnosis. Proposed correction: explicit !cancelled() status function
plus needs.publish.result == 'success' and the existing non-retention condition on
confirm; preserve refusal to run after unsuccessful publication and avoid controller
polling. Local tests/live activation of that correction have not been performed.

### Native publication confirmation correction — 2026-10-29

Invariant: every successful package push must reach secret-free HTTPS confirmation,
including native runs whose OpenWrt tools ancestor is intentionally skipped. Both
workflow source copies now use an explicit !cancelled() status function, require
needs.publish.result == success, and retain the non-retention condition. No polling,
new state, replay, signing changes or controller code. Production 2 added/2 removed;
test 1 added/1 removed. The existing workflow-boundary assertion now requires this
condition; worker-provenance-boundaries passed (2.41 seconds), format-check passed.
This local check does not execute GitHub's scheduler; real native confirmation is
required after rollout. Owner explicitly authorized commit/push/merge/deploy, current
publication recovery and a fresh core chain. Unrelated dirty documentation preserved.

### Distinguish workflow progress from publisher job progress — 2026-10-29

The graph presentation previously fell back from missing job status to run status,
so publishing:in_progress could describe artifact validation or a queued writer.
GraphStore::view now displays the known job status, or explicitly says awaiting job
status and labels the available workflow observation. This single projection feeds
UI, SSE and public progress; no new UI authority, persisted state or timestamp was
introduced. Actual timestamps remain job evidence. Production 1 added/2 removed;
tests 2 added/2 removed; C++ total 3,808 physical lines, below 3,810. Existing pipeline
fixtures now assert the workflow/job distinction and the revised unconfirmed text.

Final source passed pipeline-chain (7.96s), pipeline-unconfirmed-dispatch (1.78s),
pipeline-queued-job (8.24s), build and format-check. Initial fixture runs were blocked
by full /tmp, then Unix socket paths >107 characters; test-only scratch databases
created by this attempt were removed and fixture root shortened within the authorized
retention directory. Failed logs retained. No live database changes for testing.

Source confirmation correction is merged as PR15, main ef0646511a77f18ba645a2795cf4b44ed1ec19d7.
Sandbox alias .github/workflows/core-ci-publish-confirm.yml was deployed in worker
commit 0ae931e273495f277a61846d83f1a4d2cbc3da10 with blob
310fa5cd7ea1477fb0156dbc62ba027d8d7aae68; previous aliases/pins remain immutable.
Vchrist restarted at 20:16:01Z with 140 existing assignments preserved and unchanged
listeners/TLS. Fresh core tag v2.1.11 was created at 20:16:29Z. Native Fedora r349
confirmation ran 20:22:16–20:23:42Z and succeeded; controller verified at 20:23:48Z.
This is live confirmation-path proof, not a complete fresh-chain acceptance claim.

Activity-label correction merged as PR16, main 326353a4cf046e0322b285555ba99c73420b7a4a.
Installed binary SHA256 c5adce9b705faf22510f0c1ebe7b3bb51cc13640f900016fe818fd0a269bc487,
built from e48384f8ad04bb15c150153482c1c447b9e79f5b, matches that merge's source.
Vchrist activation at 20:25:27Z preserved 160 existing execution assignments; service
PID255665, listener/TLS unchanged. Live /api/graph contains the corrected workflow/job
wording. Backup: /var/backups/core-ci/job-status-label-20261029T202524Z.
All five v2.1.11 core publications subsequently verified; dependents are progressing.

Two dependent native arm64 attempts r353/r358 failed before publication dispatch,
with the generic approved-workflow-unavailable error around this deployment restart.
The actual workflow blob still matches approval. The stored generic error does not
establish the underlying cause; do not label the restart hypothesis as proven.
Under the owner's current-CI recovery authorization, explicitly retried each once
via /api/work/retry after checking failed/retryable state and absent publication run.
Both returned200; original attempts remain in history. No uncertain publication was
replayed. Fresh chain currently 5 published/4 publishing/6 building/5 waiting; this
is continuing work, not complete full-chain acceptance. Source production net -1,
tests net0 for the label correction; unrelated dirty documentation preserved.

### Unusual CI timing observed during continued monitoring — 2026-10-29

Read-only graph and GitHub job evidence at 20:31–20:35Z: new-chain app amd64 r352
finished artifact validation 20:25:43Z, but its feed-writing job ran only
20:30:11–20:30:35Z (4m28s queue wait, 24-second job). Confirmation ran
20:30:39–20:31:04Z; controller verified at 20:31:10Z. Six retention writer jobs
wholly inside that queue interval occupied 116 seconds, excluding other writers,
status work and startup gaps. This is not a local C++ CPU-time measurement.

The live graph contained nineteen retention assignments issued since revision347,
including three with empty objects arrays: r368, r378 and r380. r368 and r378 still
ran feed-writing jobs for26 and25 seconds respectively; r380 was queued at first
observation. Graph::retain creates a remote retention command when either deletion
objects or eligibility withdrawals exist; worker retention.update regenerates/signs
indexes even when there are no objects. This is observed avoidable remote work,
not a claim that retention policy/protection can simply be removed. No source or
policy changes made for this monitoring finding. Maintenance and the preceding
chain overlap this run, so these totals are not a clean steady-state benchmark.

OpenWrt sample setup remains material outside the lock: side APK validation took
99 seconds and tool extraction35 seconds; side IPK validation35 seconds and tool
extraction94 seconds. These are workflow job durations, not controller processing.
Native app arm64 retry r385 finished validation20:27:42Z, its skipped tools
prerequisite settled20:27:43Z, and publisher started20:34:09Z (6m26s later).
At20:34:54Z current chain9 published/8 publishing/3 waiting, maintenance47
published/4 publishing; no new failed attempt. Both controller public projection
and GitHub Pages project/status pages respond200. No current multi-minute post-job
verification stall was found in these samples; the old skipped-confirmation defect
is corrected in the new native runs. Detailed JSON snapshots and request-boundary
job evidence remain under retention/confirmation-fix in the authorized temp root.

### Full-chain monitoring result — 2026-10-29

Fresh v2.1.11 chain: all20 current target executions reached verified publication.
Tag creation20:16:29.478Z; last verification20:47:42Z (31m12.522s elapsed).
This includes one controller restart, two explicit arm64 retries, the preceding
chain's unfinished work, and automatic retention; it is not a clean steady-state
performance benchmark. Original failed preflight attempts remain in history.

Final join APK validation/tools completed20:40:15Z; publisher became pending
20:40:16Z and started20:45:51Z (5m35s). Job ended20:46:06Z; confirmation ran
20:46:09–20:47:13Z; controller verified20:47:19Z. Final IPK became pending
20:41:11Z and started20:46:42Z (5m31s); job ended20:47:05Z; confirmation ran
20:47:08–20:47:36Z; controller verified20:47:42Z. Both final post-confirmation
intervals were6 seconds, including GitHub delivery and independent remote proof;
local C++ CPU/database durations were not separately measured. No new minute-scale
post-confirmation graph stall was observed. New native confirmations succeeded.

At20:48:11Z all four GitHub Pages project pages returned200 and contained every
one of the20 current package metadata versions (none missing). Pages Last-Modified
20:47:52Z. Controller generation4820. Seven automatic retention assignments were
still active/queued; package chain completion does not mean all CI maintenance is
idle. Result and per-target timing JSON are saved under
/tmp/codex/build/core-ci/retention/confirmation-fix/package-chain-completed.json.

Priority timing findings: unnecessary remote cleanup for empty objects; export-only
cleanup still regenerating/signing package indexes; numerous small maintenance jobs
competing with package publishers in the same feed-write boundary. Final worker
inspection confirms retention.update regenerates indexes unconditionally after its
removal loop. These are implementation-generated remote-work costs, not just an
unexplained GitHub queue. No retention policy, production code, workflow or deployed
configuration was changed during this monitoring-only stage. Client installation,
physical GitHub garbage collection and a clean timing run remain separate proof.

### Claude timing investigation requested — 2026-10-30

Owner requested Claude independently investigate confirmed timing problems. Resumed
retention review session2c479bf5-90fc-4bae-ae99-726bdcdc3958 with current source/main
326353a4cf046e0322b285555ba99c73420b7a4a and explicit measured run evidence.
Requested independent validation and source tracing of empty cleanup workflows,
export/receipt-only index/SDK/signing overhead, small retention jobs competing with
publishers, derivative status work, and backlog versus steady-state costs. Requested
reduction-first corrections/estimates/tests while preserving policy, exact inputs,
expiry/history, uncertainty, signing, orphan roots and within-state public progress.
Claude is limited to Read/Grep/Glob in plan mode, with no command execution, file
edits, live operations or subagent delegation. Prompt saved under
/tmp/codex/build/core-ci/retention/timing-review/request.md; report destination
claude-investigation.json in that directory. Investigation started; no conclusion
or implementation approval claimed before its result.

Claude timing investigation completed successfully (read-only session20 turns,
226.464s). Verbatim result saved as retention/timing-review/claude-investigation.md
beside its JSON. It confirms empty remote commands, unconditional index/signing
work for metadata-only deletions and recurring cleanup waves competing with package
publication. It additionally flags failed retention attempts remaining busy/claimed
without normal retry/retirement support. Proposed fixes include graph-only expiry,
bundling metadata deletion with package removal, and feed-idle cleanup admission;
estimates C++3820–3824 for the full proposal. These remain reviewer proposals,
not owner approval or independently validated fixes. In particular metadata deferral
has no wall-clock bound if no future package removal occurs, and ignoring failed
commands could automatically redispatch on the next state change; those implications
must be resolved before adopting the recommendations. No implementation or live
mutation was performed for this review request.

### Agreed reduction prompt after independent validation — 2026-10-30

Validated Claude's timing findings against current Graph::retain/eligible/select/
retry, Pipeline::change/fail/dispatch/complete, GraphStore views and worker/workflow
callers. Confirmed unnecessary empty remote cleanup, metadata-only full index/signing/
SDK preparation, and failed cleanup's missing ordinary operator recovery.
Disagreed with and obtained Claude's explicit withdrawal of metadata deferral until
future package removal (potentially indefinite), excluding failed cleanup from busy/
claims (would immediately redispatch through fail -> change -> retain), the whole-feed
idle gate (unapproved scheduling/starvation policy), and unverified FIFO/steady-state
claims. Discussion rounds2/3 and final prompt/corrections are saved in
/tmp/codex/build/core-ci/retention/timing-review/.

Agreed scope: move existing expiry loop so empty physical deletion is graph-only;
retain current nonempty independent deletion; derive actual package presence within
existing worker path validation; skip SDK/native apt/index/sign work only for
metadata-only deletion, using workflow path hints with independent worker enforcement.
No new scheduler, admission policy, automatic retry, schema/assignment field, route,
controller flag or persisted state. Existing secret acquisition and actual package
signing/tool proof remain. Failed-cleanup recovery remains separately scoped.
Official GitHub expression and skipped-ancestor semantics checked; local syntax/
contract checks cannot prove live job scheduling. Source-proven status workflow
already builds its template venv without native package tooling on ubuntu24.04.

Claude explicitly agreed and produced implementation-prompt.md, including corrected
authority wording preserving actual VOC sandbox authorizations. Estimated production
+15 net maximum proposed, tests+85 maximum proposed, C++neutral3808/3810. These new
bounds are proposals requiring owner approval; no production edits have been made.
Existing unrelated dirty documentation/private directories preserved. Final prompt
is to be presented to owner; no implementation, commit, push, merge or activation
performed during this review-and-prompt task.

### Pre-edit checkpoint for retention work reduction — 2026-10-30

Verified owned feat/publication-retention HEAD and remote main both326353a4cf046e0322b285555ba99c73420b7a4a;
no open PR returned. Four known dirty documentation files preserved; no production edit.
Re-read AGENTS.md/current affected source. C++3808, cap3810; workflow copies identical.
Constructed the concrete planned transformations in memory only, including clang-format
for the C++ candidate and readable YAML <=160 columns. Planned production counts:
Graph.cpp4+/4- (0); retention.py12+/9- (+3); workflow template10+/4- (+6);
worker workflow copy10+/4- (+6). Total36+/21-, net+15; C++remains3808.
No publish.py change or new controller mechanism is needed. Test estimate net+70,
proposed bound+85; docs minimal. Source plan accounting saved to
/tmp/codex/build/core-ci/retention-reduction/pre-edit-plan.json. Explicit user prompt
requires stopping before positive production growth until bounds are approved.

### Expanded retention reduction review checkpoint — 2026-10-30

Verified feat/publication-retention at326353a4cf046e0322b285555ba99c73420b7a4a;
C++3808/3810; production/test sources unchanged. Owner required all earlier
findings considered, so the prior +15-only proposal is insufficient. Claude and
Codex agreed after three expanded review rounds: graph-only empty expiry; exact
metadata deletion without SDK/native tooling/index/signing; no duplicate IPK
validator SDK preparation; explicit failed-retention retry through the existing
API, dispatched only after its committed transition. Original pins, objects,
claims, history and unknown assignments remain; no automatic replacement.
Carrier cleanup, indefinite deferral, directory regrouping, new persisted fields
and shorter confirmation polling were rejected. A/B/E defects and the IPK
duplication have proposed fixes; necessary protected-input release waves, actual
serialized feed mutations, progress refresh and Pages/receipt proof remain.
The measured116seconds across six cleanup jobs is mixed/unclassified occupancy,
not a measured irreducible package-removal cost. Six seconds after confirmation
is aggregate delivery/proof elapsed, not a measured controller CPU time or bound.

Claude supplied the agreed implementation prompt under
/tmp/codex/build/core-ci/retention/timing-review/all-findings-implementation-prompt.md.
Review evidence: all-findings-review, all-findings-round2, all-findings-round3
JSON/Markdown in that directory. Preliminary production growth+29..33, tests
about+127; proposed ceilings+35 production/+150 tests/C++3830 are UNAPPROVED.
Exact formatted per-file accounting is still required before requesting approval
and editing production. Recovery is API-only; the existing UI does not show
retention rows. Existing assignments/retries retain their old workflow/tooling
pins. No new runtime, scheduling or live performance proof was produced.
No production/test edit, commit, push, merge, deployment, activation or live
mutation was performed. Existing unrelated dirty work/private directories preserved.

### Exact draft checkpoint: retention work reduction — 2026-10-30

Verified feat/publication-retention HEAD326353a4cf046e0322b285555ba99c73420b7a4a;
C++3808/3810, publication workflow copies identical, existing dirty work preserved.
Production/test candidates exist only as temporary draft data; no source was changed.
A/B/E: graph-only expiry, metadata-only removal without setup/index/signing, and
explicit failed-cleanup retry are proposed fixes. F removes duplicate IPK SDK
preparation, keeping APK parser and independent tool verification. C/D retain
necessary protection-release waves and actual shared writes; G retains Pages and
receipt proof; H preserves within-state progress while removing empty-job churn.
The116seconds measured across six cleanup jobs is mixed occupancy, not a measured
irreducible package cost. Six seconds after confirmation is aggregate delivery/proof.
No new fields, scheduler, automatic retries, batching or confirmation polling.
API-only recovery preserves pins, claims and history; unknown attempts stay assigned.
Exact formatted draft accounting is in retention-reduction/checkpoint/accounting.json.
Exact accounting is recorded separately; C++3829 and revised growth require approval.
Claude confirmed that retry labels must include revision to prevent old-run binding.
Local YAML/contract checks cannot prove scheduling; sandbox predicate outcomes and
clean/continuous chain timing, signing/clients and request counts remain acceptance gates.
No commit, push, merge, deployment, activation or live mutation was performed.
The agreed unchanged-label clause is unsafe in accept()/inspect() while a dispatch
response is pending or lost. Proposed exception: only retry label gains its revision;
no new assignment field. Claude agreed in checkpoint/label-review.json. Tests draft
held/lost responses and delayed old reports; these tests have not run. Owner must
approve this exception, explicit retention retry, and the revised quantified bounds.

### Retention work reduction implemented and locally verified — 2026-10-30

Owner approved explicit retention retry, revision-specific labels and bounds of
+40 production/+190 tests/C++3830, then authorized commit/push/merge/deploy and a
fresh VOC-Experiments graph, subject to Claude reviewing the diff before applying.
Claude reviewed every hunk; two fixture defects and one missing assertion were
corrected before its explicit agreement to reviewed-revision.patch09fc76aa.
Applied the reviewed source; one runtime fixture exceeded argv size. Claude agreed
to targeted JSON_REMOVE plus an exact seeding assertion; the rerun passed.
Single graph/FIFO and commit-before-dispatch remain; unknown outcomes are not retried.
Empty expiry is graph-only; metadata-only deletion skips SDK/native/index/signing
work. Failed cleanup can be explicitly retried via the existing API with exact
objects/pins/history and a unique label. IPK validation omits duplicate SDK setup;
APK parsing and independent publication-tool verification remain. Actual shared
writes, protection-release waves, public progress and Pages proof remain necessary.
Release build core-ci/graph-test/graph-store-test and format-check passed. Each
focused ctest target ran separately: graph-state-boundaries0.10s, persistence-graph
1.32s, pipeline-retention5.01s, openwrt-archive-boundaries1.21s,
worker-package-boundaries2.36s, worker-provenance-boundaries1.98s, pipeline-chain9.18s.
Real isolated MariaDB/synthetic GitHub; unchanged eight-report burst zero HTTP;
full synthetic chain635 HTTP including16 token/20 artifact requests, not live throughput.
Production69+/31- (net38); tests184+/8- (net176); C++3829/3830.
Exact source/build hashes, reviews and fixture evidence: /tmp/codex/build/core-ci/retention-reduction/.
No live scheduling, timing, clients or publication acceptance claimed by these local checks.
Unrelated dirty hunks/private directories preserved; promotion and activation follow.

### Retention reduction promoted and fresh graph launched — 2026-10-30

Owner-authorized CPMD completed: source8339fefa38ecd38cfe24bd7456b1489ddd4c0f62,
PR17 https://github.com/SNodeC/core-ci/pull/17, main902d7a7efb654204349bb4fb036501f466d680aa.
Canonical checkout remains on owned feat/publication-retention with identical source tree;
no switch/reset/rebase/stash, unrelated hunks excluded from commit.
Sandbox toolingc850cf3bf37eab9a8e034a88c68d22a2f72b47d2; new publisher alias
core-ci-publish-reduction.yml commitb9f08f09296098724868602837b8d998369c2641,
blob6c1dc8f7edb6c5b3d0adb26a062f3c39aa9fae21. Source reduction aliases added
to all four sandbox repositories; all pre-existing blobs/aliases retained unchanged.
Vchrist deployed at10:35:58Z, PID266230, binary
1f6ee18f6fa2e70b81f3be06e5c312cbcd5f8ff7a0eb9af4ecf60218190d9160.
All21 installed SNode.C libraries matched local build. Protected backup
/var/backups/core-ci/retention-reduction-20261030T103555Z. All160 executions and
72 retention records/attempts/assignments/run identities survived; generation4859
unchanged. Listener/port/TLS settings unchanged; UI/public-status HTTP200 and SSE work.
Fresh demo-corev2.1.12 tag created10:37:23Z atfc3acdb94779dafb879c183143c20e19b435e766;
old tags unchanged. Webhook automatically created20 new executions; five core runs
created10:37:36Z. By10:39:05Z allfive builds completed successfully and entered
publication, with15 dependent executions waiting and no fresh failure. New publisher
runs41042205480/41042206689/41042207359/41042277581/41042294411 created.
Pages status/demo-core.html showsv2.1.12; 8080 graph/SSE current.
Claude received source/deployment/test handover after its detailed pre-apply review.
No full graph completion, live predicate matrix, client-install proof, request totals
or local CPU/DB latency/speedup is claimed yet. Actual serialized writes and Pages
propagation remain; no failed/uncertain historical mutation was replayed.
Evidence/reviews/build identity and live snapshots under
/tmp/codex/build/core-ci/retention-reduction/. Promotion record remains local alongside
the preserved pre-existing dirty documentation; source changes are committed/merged/deployed.

### Read-only root-cause investigation of publication start delay — 2026-10-30

Owner requested deep investigation only and permitted independent Claude review.
Source remains8339fefa/main902d7a7; deployed identity/pins verified from prior activation
and actual GitHub workflow blobs. No source, workflow, settings, assignments, tags,
service or feed mutations were made during this investigation.
Dashboard/GitHub/receipt agree for demo-app Debianarm64r417: buildcompleted10:41:01Z;
publicationworkflow41042549697created10:41:07; validationcompleted10:41:28;
publishjob124018766778created10:41:29, actuallystarted10:45:21, finished10:45:41;
publicationverified10:46:03. The4m20 start gap is real; the workflow was already
running6seconds afterbuildcompletion. No inference of exact C++CPU/DB latency.
Confirmed cause: job-level core-ci-feed-VOC-Experiments/packages-main exclusivity
covers checkout/artifacttransfer/apt/pip/setup and preparation/push/teardown across
package publishers, physical cleanup and status. Eight earlier same-group jobs ran
one at a time during validation-end-to-publisher-start233seconds: five package
publishers132s, two cleanupjobs50s, one statusjob12s; between reported jobintervals39s.
The194s job intervals include apt67s and venv/pip32s versus32s feedprepare/push.
Other setup/checkouts/download/poststeps39s;24s withinjobtime not covered by named
steps. No attribution of all39s handoff gaps to idle time, runner shortage or CPU.
Jobcreation order matches observed start order, but historical group-entry times
were not sampled. Current GitHub concurrency API directly confirms same group
activeowner/pendingwriters and ahead-of-job blockers. Actual deployed YAML and
public FEED_REPOSITORY/FEED_BRANCH match; environment has branch policy only.
Inspected60 canceledstatusruns in criticalinterval: zero jobs, zero write occupancy.
Read-only stored cleanup authorities prove r431/432/433 remove actual corev2.1.9
packages plus metadata using newtoolingc850 and workflow6c1; they are not empty or
metadata-only commands. Thus prior reductions do not remove these queued jobs.
Independent Claude review agrees on primarycause and measurement limits. Raw
API snapshots and computedintervals: retention-reduction/live/deep-delay/ in/tmp.
Separate confirmed finding: metadata-onlyr436 run41043079306 failed checkedpush
10:48:26Z with HTTP403. This occurred after r417 started and did not cause its
4m20 wait. Controller retains its uncertain publishing assignment; no retry/replay
was attempted. Cause of HTTP403 itself is not established by this investigation.

### Publication delay: reviewed candidate and measurement checkpoint (2026-10-30)

Read-only discussion with Claude confirms whole feed-job exclusion as the queue cause;
necessary writes, setup and handoff remain distinct costs. No complete speedup is proven.
Candidate: one dependency-only image pinned by digest for all feed writers, with application
code still owned by the selected tooling checkout. No backward compatibility code, adapters,
fallbacks, floating tags or automatic image upgrades. This is an unapproved new mechanism.
Claude agrees to staged cold-probe rejection, local bare-Git lease checks, explicit prototype
and growth approval before edits/build/push, and conservative APT timing attribution.
Reject the candidate if provisioning erases its savings; no four-minute or speedup promise.
Prompt: /tmp/codex/build/core-ci/retention-reduction/live/deep-delay/publication-delay-fix-prompt.txt.
Source: feat/publication-retention at 8339fefa38ecd38cfe24bd7456b1489ddd4c0f62.
Only this log entry and temporary review evidence were written; production and tests unchanged.
Remaining gates: exact design/accounting, owner approval of mechanism/bounds/prototype,
cold provisioning measurements, production-stage authority and real acceptance. The separate
HTTP 403 uncertainty remains unresolved and was neither retried nor reclassified.

### Publication dependency image prototype (2026-10-30)

Owner approved prototype source/build/push and measurement, with no production feed activation.
Worker prototype commit f328f5f303755f84b97c43ea0b113754e1c6f5bb; image build run 41055283027 passed.
Image digest: sha256:eed7eb9fd4cc3f79d98aa73bb66fbde4d68409653ab04b8ee7a9359f0f858041.
GitHub created the image private; visibility and organization policy were not changed. Cold probe not dispatched.
Prototype source net +42, temporary probe draft 19 lines; C++ unchanged at 3,829. Existing worker-provenance passed (1.93 s).
Image pull timing and signing/container acceptance remain unverified; no compatibility path or mutation replay was added.

### Publication image measurement decision (2026-10-30)

Owner approved private-image probe with its job token and packages:read; visibility and organization policy stayed unchanged.
Probe run 41057946885 (worker 4f81e3925df2bb128dfe3f37add94b33b6c02280) passed both smoke jobs.
Image initialization 8 s versus baseline setup/render 4 s; total jobs 11 s versus 7 s, one cold sample.
Pull/extraction alone took about 6.78 s, excluding about 0.43 s registry login. Candidate rejected; no broader matrix or activation.
Temporary probe removed after measurement; source prototype retained uncommitted for review, production net +38 after cleanup (peak +62).
C++ remains 3,829/3,830; tests unchanged; this task appended 18 documentation lines total. Evidence: /tmp/codex/build/core-ci/publication-image/.
Publication delay remains unresolved; no signing/client acceptance or new live throughput claimed, and r436 uncertainty was not replayed.

### Live-code review and setup reduction — 2026-10-30

Owner requested correction under the global principles and a review of actual live code with Claude.
Read-only SSH confirmed PID266230 and deployed binary SHA256 1f6ee18f6fa2e70b81f3be06e5c312cbcd5f8ff7a0eb9af4ecf60218190d9160.
Live publisher blob6c1dc8f7edb6c5b3d0adb26a062f3c39aa9fae21 uses worker c850cf3bf37eab9a8e034a88c68d22a2f72b47d2.
Reviewed deployed worker modules match canonical code. The older status pin has identical executed helpers and all seven
status-template entries; Claude withdrew the suggested deployment-drift defect. No needless status re-pin performed.
Three Claude exchanges covered deployed source, candidate code, final applied diff and six new workflow-contract guards.
Final patch removes validate/tools apt setup, restricts publisher installation to createrepo-c for RPM package work,
and uses the existing host pip to populate the same isolated renderer venv without bootstrapping another pip copy.
Production YAML +12/-20/net-8; tests +6/-0. C++ unchanged at3829. No new job, image, helper, schema or controller mechanism.
Rejected local image prototype files and its requirements hunk removed, with exact evidence retained under publication-boundary/.
Preserved unrelated dirty docs and private directories. No local commit/push/merge or live activation performed.
Local API diagnostic proves clean-environment rendering from the pip-less venv; its timings are not hosted-runner proof.
worker-provenance-boundaries2.15s, worker-package-boundaries2.84s, openwrt-archive-boundaries1.20s passed independently.
YAML syntax, identical publication copies, changed production line lengths, format-check and git diff --check passed.
Build/source identity: HEAD8339fefa38ecd38cfe24bd7456b1489ddd4c0f62; task diff SHA256
3533894b63843525afe495ff00b4a7943c48f54dd7716dbc98a1546c713509b1. Source and tests unchanged after those checks.
Claude found no concrete defect in the final patch and agrees it is only a partial setup reduction. Remaining whole-job
writer queuing, hosted tool/pip behavior and actual performance require further evidence; complete-delay acceptance is not claimed.
Claude withdrew current-tree presence/absence as a safe generic recovery algorithm for uncertain cleanup. Git2.56 source
plus r436's public error strongly locate that command's403 at ref discovery before update transmission, subject to binary
provenance limits. This does not explain the authorization denial or authorize a general parser, state change or replay.
Bound uncertain cleanup recovery remains an explicit policy/authority issue; r436 and its claims were left unchanged.
Evidence and all three reviews: /tmp/codex/build/core-ci/publication-boundary/.

### Lean publication setup rollout and measured chain — 2026-10-30

Owner authorized commit, push, merge, deployment and one fresh monitored sandbox rebuild.
Committed the exact reviewed patch as e2c0045f22827563a729ca24955cd9532f1631db; PR18 merged as
f98669b71f2313371ebd27f0bd2127183fed5a41. Only task code/test paths and the preceding task log entry were staged.
VOC worker commit226151893442ed47a718ad6b67c1b80ce074bdfc adds immutable lean-setup publisher/status aliases.
Application tooling remains c850cf3bf37eab9a8e034a88c68d22a2f72b47d2; existing workflow aliases and assigned pins unchanged.
Vchrist restarted at18:20:47UTC with only publication/status workflow pin arguments changed; PID268419.
Binary SHA256 remains1f6ee18f6fa2e70b81f3be06e5c312cbcd5f8ff7a0eb9af4ecf60218190d9160; no C++ rebuild/replacement.
Restart preserved existing assignments and generation5414, including uncertain retentionr436; no replay or replacement.
Hosted status preflight41106146433 passed, including host pip populating the isolated pip-less venv and a checked status push.
Fresh demo-core tagv2.1.13 created at18:21:40UTC, commitfc3acdb94779dafb879c183143c20e19b435e766; existing tags preserved.
Twenty selected executions are revisions455–474. Read-only observer captures SSE, graph, jobs/logs, feed queue and Pages.
Early results confirm remaining serial queue delays; setup reduction is not complete-delay acceptance.
Evidence: /tmp/codex/build/core-ci/publication-boundary/live/. Final timing and run outcomes follow below.

### Trigger-driven controller replacement — 2026-10-30

Owner authorized a fresh controller implementation, maximum 2,500 physical C++ source/header lines including bootstrap.
Canonical checkout /home/voc/projects/snodec/core-ci, owned branch feat/publication-retention, HEAD e2c0045f22827563a729ca24955cd9532f1631db.
Current main f98669b71f2313371ebd27f0bd2127183fed5a41 contains the reviewed setup reduction. No open PR.
Starting C++ count: 3,829, excluding the 11-line UiAssets.h.in template; the physical C++ limit is a reduction of 1,329 lines.
Invariant: authenticated event -> one committed graph transition -> GitHub dispatch; verified publication alone releases exact dependents.
No new engine, scheduler, controller package processing or compatibility implementation. Preserve keys, configuration, unrelated dirty docs and UI.
The running sandbox graph and its assigned uncertainty are independent of local replacement work; no replay or reset authorized or performed.
Production replacement and end-to-end acceptance are not yet complete. Temporary checks: /tmp/codex/build/core-ci/trigger-graph-controller/.
Source review confirmed advance already traverses ready executions in revision order and commits them before dispatch; the measured minute waits are worker/Pages boundaries.
Claude reviewed current source read-only: unnecessary full re-derivation on evidence/preflight writes and repeated view calculations are concrete findings;
its estimate is not a proof that 2,500 lines is impossible. No functional deletion or C++ replacement has been applied.
Asked owner whether this reset removes controller retention and GitHub definition/workflow editing; preserving existing behavior forbids silently removing them.
Baseline graph-state-boundaries passed0.10s and pipeline-chain passed9.37s; these validate existing source, not a replacement.
The monitored v2.1.13 chain settled19 published/1 failed; r473 artifact validation returnedHTTP502 before publication. No retry or replacement performed.
New retention attempts remain publishing without push-step evidence; existing unknownr436 unchanged. Local observers stopped after terminal execution outcomes.


### Owner-authorized feature removal — 2026-10-30

Owner answered "remove" for controller-managed retention and GitHub definition/workflow editing.
Removed graph cleanup derivation, cleanup retry/completion, worker retention implementation and workflow predicates;
removed write APIs, editor controls and embedded workflow rendering. GitHub committed definitions remain authoritative;
read-only enrollment, exact assignments, normal retry/retirement, signing and independent publication proof remain.
Readiness-producing facts traverse and commit the graph before dispatch; evidence/preflight writes only persist their facts.
Existing expired-input facts still reject unavailable inputs. Old cleanup records remain in the database document but are
not exposed, observed or reconciled by this implementation; unknown outcomes remain unresolved and are never replayed.
No automatic package-storage reclamation remains. Storage management is external to this reduced controller.
Claude reviewed the applied source/diff and found no remaining production defect; this is not approval of completion.
C++ physical count 3,494: +73/-408/net -335 against e2c0045f22827563a729ca24955cd9532f1631db.
Other production +58/-395/net -337; tests +53/-789/net -736. Combined production net -672.
Per-file accounting: /tmp/codex/build/core-ci/trigger-graph-controller/removal-accounting.json.
The 2,500-line replacement objective is NOT achieved: 994 C++ lines remain above the owner limit.
This removal does not establish an end-to-end delay improvement or a completed fresh implementation.
Build identity: HEAD e2c0045f22827563a729ca24955cd9532f1631db, source/test dirty diff SHA256
4b4eef0f2cb48204b8bbfa37405dcdeaa7b7dd40be2ce88c66813d3553dfc120.
Built with cmake --build build/codex/speed-release --target core-ci graph-test graph-store-test -j4.
Final build/format command: cmake --build build/codex/speed-release --target core-ci format-check -j4.
Ran each independently: ctest --test-dir build/codex/speed-release -R '^<name>$' --output-on-failure.
Passing names: graph-state-boundaries (0.07s), pipeline-chain (9.24s), pipeline-overlap (8.65s),
pipeline-cold (9.13s), persistence-graph (1.84s), enrollment-boundaries (1.85s),
worker-provenance-boundaries (1.99s), worker-package-boundaries (2.07s), openwrt-archive-boundaries (0.99s).
These are isolated real-MariaDB/local synthetic-GitHub checks, not live acceptance or throughput measurements.
Earlier fixture startup failures were /tmp ENOSPC and MariaDB path permissions; cold proof application failed with
"graph_state is full" before space was reclaimed. The affected checks subsequently passed independently.
Removed-feature tests were deleted; remaining build/publication and safety tests were retained.
Browser check against the synthetic UI fixture verified read-only definition rendering and only a refresh POST;
no GitHub editing controls or console errors. Fixture and browser tab stopped/closed after checking.
YAML parsing, identical publication copies, Python/JavaScript syntax, changed production line lengths <=160,
canonical format-check and git diff --check passed. No unrelated/full suite was run.
Preserved unrelated dirty documentation, review records and private directories. Only targeted current-feature docs
were changed; whole dirty-document numstats include previous work and are not charged to this task.
Freed inactive generated test database directories after their processes stopped; preserved logs and evidence.
Also removed 33 inactive temporary object files (215,043,568 bytes) from the old controller-parallelism build;
sources, linked binaries and probe evidence remain. No additional checkout was created.
Changes remain local on feat/publication-retention, uncommitted and undeployed. Running sandbox state is unchanged.
