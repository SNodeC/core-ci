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
