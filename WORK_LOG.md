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
