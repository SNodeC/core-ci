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
