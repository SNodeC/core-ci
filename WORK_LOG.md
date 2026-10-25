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
