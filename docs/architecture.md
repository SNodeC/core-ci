# Initial architecture — for approval

## 1. Scope and invariant

One single-tasking SNode.C process owns durable orchestration. GitHub executes assigned work; verified feed receipts record external publication. The application must not reconstruct its queue from workflow runs or feed status pages. It must not weaken the functional contract in requirements.md.

This is design, not an implementation, benchmark or deployment. Development is on jupiter. VOC-Experiments is the future integration target. The new SNodeC/core-ci repository hosts source/design only; it does not orchestrate the live SNodeC deployment during this stage.

The principal simplification is replacing workflow-driven reconstruction with explicit transactional state transitions. C++ alone provides no correctness guarantee: the state model and crash boundaries matter.

## 2. Small concrete component set

| Component | Responsibility | Excludes |
| --- | --- | --- |
| Main/configuration | SNode.C initialization, deployment config, connection/listener setup and clean shutdown | Scheduling policy |
| Http | Webhooks, worker reports, task claims, operator API and read-only live status | Uncommitted domain mutations |
| Scheduler | DAG/coverage/arm policy, revision decisions, state transitions and priority selection | Network, SQL and package parsing |
| Store | Async transactions and durable facts; restores derived indexes after restart | Independent scheduling/retry policy |
| GitHub | Scoped installation credentials, async dispatch, identity verification and targeted observation | Organization-wide occupancy scan |
| Projection | Highest-only presentation from one committed snapshot; authored branding and static export | Permission to execute work |
| Trusted publisher on GitHub | Package-byte verification, signing/indexes, feed CAS and receipts | Choosing what to build next |

Use ordinary concrete types first. No plugin platform, generic distributed workflow engine, message broker, service mesh, thread pool, separate scheduler service or event-sourcing framework is proposed. Domain types and test seams do not require a hierarchy of abstract factories.

SourceEvent, Capture, RowTask, Attempt and Publication are domain records, not separately deployed services.

## 3. SNode.C integration grounded in the checkout

Read-only trace used framework commit d795f95f4b4a82b59bed6ec13df1910d2a0511a4 on branch fix/package-orchestration. Public components must be consumed through find_package(snodec), following the installed-target pattern already used by the IDP application. Do not depend on this personal checkout path at runtime/build time.

- src/apps/jsonserver.cpp: WebApp::init, JsonMiddleware, APPLICATION(req,res), routes, listener callback, WebApp::start.
- src/apps/http/ConfigWWW.{h,cpp}: utils::SubCommand/configRoot configuration idiom.
- src/apps/http/httpclient.cpp and model/clients.h: callback-based request/response flow and TLS client family. Reuse public web::http::tls::in::Client, not example-only macros or hardcoded sample credentials.
- src/core/timer/Timer.h: SNode.C singleshot/interval timers. Restore deadlines from durable state; a timer is not authoritative state.
- src/apps/database/testmariadb.cpp and database/mariadb/MariaDBClientASyncAPI.h: async query/exec/startTransactions/commit/rollback callbacks.
- MariaDBConnectCommand and MariaDBCommitCommand use connector start/continue functions rather than blocking connect/commit on the event loop.

The same MariaDBClient also exposes synchronous calls. Those are prohibited on the application request path. Confirm all chosen calls, reconnect paths, DNS and TLS behavior during the first implementation spike; the existence of async APIs is not proof that every use is nonblocking.

Use TLS verification and bounded request/body sizes. Do not copy sample certificate paths/passwords, raw HTTP body logging or sample SQL credentials.

## 4. Single-tasking execution

The process runs one SNode.C event loop and no application threads. A route authenticates/parses a bounded message, submits a transition and returns control while async work completes. A bounded in-memory ingress queue serializes domain-changing transactions; HTTP acceptance is acknowledged only after durable commit. If storage is unavailable, do not acknowledge acceptance.

Because asynchronous callbacks can interleave, single-threaded does not mean transaction-free. Only one domain mutation transaction is in flight. Network calls never occur inside an open mutation transaction. Publication commit fencing is a separate protocol, not an open SQL transaction held across GitHub execution.

Graph work uses the changed node and reverse adjacency to re-evaluate affected dependents. Definitions/graphs are immutable by capture; derived work indexes and priority queues are disposable memory, reconstructed from committed records. Bound traversal slices and yield through the event loop for large graphs. The same task identity can be made runnable once, not appended indefinitely on repeated reports.

Startup and any initial storage reads need explicit treatment: do not accept traffic until the authoritative snapshot is restored. Reconnect never promotes an uncommitted memory candidate to authoritative state. Critical logs exclude secrets and untrusted bodies.

## 5. Persistence recommendation

Recommend MariaDB/InnoDB through SNode.C's existing asynchronous client, on jupiter initially. This avoids adding blocking SQLite commits or an application storage thread. The database is a storage service; the core-ci application remains single-tasking. MariaDB server availability/configuration, schema permissions, durability settings and backups must be approved before provisioning. No server was installed or contacted for this design.

Alternatives considered:

- SQLite in the event-loop process: fewer operational dependencies, but commit/fsync/lock waits can block. WAL does not make SQLite async. Not recommended under the strict execution requirement without an explicitly accepted bounded-blocking policy.
- Custom append/fsync journal: disk waits remain; also reinvents transactions, recovery and compaction. Rejected.
- Storage helper/thread: violates the chosen application model or adds another custom process. Rejected.

Proposed durable relations (names illustrative, not a committed schema):

| Relation | Owns |
| --- | --- |
| deployments | Feed/workflow identity and approved scheduling configuration |
| direct_requests | Verified source event identity, current direct slot and activation |
| captures | Immutable selected definition/source/recipe/target snapshot and reservation decision |
| tasks | Project/row/version work, root relationship, cause, binding and terminal outcome |
| attempts | Operation, intent, dispatch/run/job identity, execution state, budgets and next observation deadline |
| publications | Verified feed commit, receipt and release identities used by dependency decisions |
| publication_commits | Current serialized publisher's prepared/authorized/confirmed outcome, for the commit fence |

Do not add a second durable ready queue, generic outbox or state-event log. The persisted Attempt in dispatching/uncertain state is itself the recovery record for its pending external effect. Task reasons/history must remain inspectable; retained Attempt records provide execution history. Source dedup keys belong to direct_requests, not an unrelated receipt database.

Use unique constraints for direct source-event identity, assigned attempt intent, run/attempt binding and project/row/version/revision claim. Shared cross-row revisions are allowed; do not impose a unique(project,revision) constraint. Enforce derived semantic invariants within the transaction as well.

Choose safe parameter binding/escaping from actual connector facilities in the spike. The traced convenience API accepts SQL strings; do not concatenate request strings. If no safe public API can be used without framework changes, return that limitation before writing a custom SQL layer. Database access is least privilege, no schema/DDL permission for ordinary requests.

A deployment recovery position for App webhook deliveries is also durable metadata, not a generic event ledger. Received-but-not-applied events blocked by a publication fence remain in their owning direct-request/attempt records. No distributed DB+Git transaction or exactly-once network delivery is promised. Idempotent attempts, receipts and explicit uncertainty provide recovery.

## 6. Domain and execution state

Separate task outcome from execution facts; publication may be confirmed while cleanup still occupies a GitHub slot.

| Record | States / facts |
| --- | --- |
| Task outcome | active, published, failed, cancelled, skipped, superseded, unsatisfiable; reason/category preserved |
| Attempt operation | capture, build or publish; presentation-only export is an explicitly proposed worker role |
| Attempt execution | admitted → dispatching → queued → running → completed; dispatch-uncertain and observation-unknown retain commitments |
| Artifact result | absent or verified artifact identity attached to the relevant build attempt |
| Publication commit | prepared → authorized → confirmed; uncertain commit never means abandoned |

Pending/waiting/blocked/building/publishing are projections from task outcome, current operation and prerequisite facts, not additional persisted copies of the same status.

A logical captured source remains immutable; a fresh dependent capture is a new object. Existing numeric GitHub capture-run identities may be retained in bundle/receipt interfaces; core task/attempt identity is independently stable before dispatch. Do not mechanically replace capture IDs in retained publications.

Unregistered reservation claims and held captures are represented explicitly by capture attempts. Legacy root-node mechanics can disappear only after tests show equivalent deferral/retirement/one-shot behavior. Do not translate accidental representation into a permanent new controller.

## 7. Event transaction and effects

1. Authenticate the request or classify a targeted observation.
2. Load affected committed records; reject stale/duplicate/conflicting identity as appropriate.
3. Compute the transition using Scheduler, including graph validity and applicable budgets.
4. Commit changed records and any newly admitted Attempt atomically.
5. Apply the committed changes to derived indexes/projection.
6. Dispatch external effects or arm recovery timers outside the DB transaction.
7. Record dispatch acknowledgement transactionally. If the response was lost, reconcile the same intent before another dispatch.

A duplicate accepted event returns its existing outcome. An HTTP timeout before commit must be safe to retry. A timeout after commit must not allocate a second attempt. No event handler directly increments a budget independently from the admission/acknowledgement policy.

Define precisely which existing counters represent accepted dispatches versus admissions. Preserve the existing three-dispatch bound rather than incrementing both on an uncertain response and on its later observed acceptance.

## 8. DAG policy and compact data structures

Use immutable per-capture project definitions, adjacency/reverse-adjacency, a deterministic topological order, and keyed indexes for highest applicable work and row claims. Detect cycles before accepting graph changes.

Readiness is a derived predicate, not a persisted boolean. Current roots come from highest verified feed receipts, not timestamps. No activation/publication clock-domain comparison.

Evaluate each required arm for publication presence, release identity and current admitted binding/coverage. Use the full pinned graph. Retain direct presence-only admission and lineage-free dependent gating. A failed current-baseline arm closes an already captured child actionably; a root-level wait need not allocate join state.

Priority selection orders publish, dependent/overlap/capture, direct, then stable acceptance order and task identity. A stack is not the scheduler. Per-version row claims prevent older activity from suppressing the highest display or newer work.

One C++ domain policy computes graph/binding/coverage decisions. The publisher requests that policy's final authorization rather than porting another arm-gate implementation into Python. Package internal-control validation remains publisher-owned and separate from scheduling policy.

## 9. Own-worker capacity — explicit policy change

Recommend one configured total budget for this deployment's allocated/committed GitHub jobs and a publisher sublimit of one. Include capture, build, publisher and any status-export jobs; do not silently budget each class independently above the total. Leave the desired deployment headroom when configuring the budget. GitHub queues behind unrelated organization work.

Ready publication gets first access to a free slot; dependent work gets the remaining usable slots before direct work. A publisher backlog does not globally block builds.

Queued/uncertain attempts retain a commitment. Release an occupied job based on verified job completion, not merely artifact availability or a report sent before cleanup. Capture's completed job can release its slot even if an aggregate workflow continues; the registered child may proceed. A published receipt does not pretend publisher cleanup is already completed.

This intentionally replaces the old full-org capacity guarantee. It stops reading unrelated/private repository counts; therefore the observer PAT is unnecessary for the new deployment. That removal is a design decision for approval, not a statement that the old deployment can discard its observer now.

## 10. Publication is the difficult cross-system boundary

The old feed CAS includes scheduling state and package state in one Git snapshot. Moving scheduling state to a database removes that implicit atomic boundary. Merely asking core-ci for permission and later pushing is unsafe: a conflicting scheduling event can arrive between permission and push.

Proposed preservation: one durable publication-commit fence, owned by core-ci and the assigned trusted publisher. The publisher constructs the candidate against an exact feed commit, presents trusted manifest/receipt/baseline facts, and requests final authorization. Core-ci evaluates the same C++ gate and durably records the authorization before replying. Feed CAS still rejects an unexpected parent commit.

From authorization until the external commit is confirmed or proved not committed, do not apply scheduling transitions that would invalidate that authorization. Accept conflicting events durably, but defer their domain effect behind this fence. Nonconflicting observations and builds proceed. No open SQL transaction and no global pause of all worker admissions.

For the first correct implementation, a coarse publication-decision fence can defer policy-changing events globally for this short final commit interval, while serving reports and already-runnable nonconflicting work. It must not cover downloading/signing/index generation, which happen before final authorization. Measure interval and event latency. Per-row fine-grained locks are not the initial design.

A lost push/report is not released by timeout alone. A known publisher run that may still push keeps the fence; reconcile its execution and the feed receipt. Only proof of commit or inability to commit releases it. This can reduce availability during uncertainty; it preserves safety. Explain the operator path. Do not introduce renewable time leases without fencing the external Git writer.

Open choice D3: approve this narrow serialization/fence, or explicitly relax the requirement so authorization is the linearization point and later accepted scheduler changes cannot revoke an admitted publication. The latter is simpler but changes current semantics. Do not silently select it. Receipt confirmation/replay must make duplicate authorizations harmless.

## 11. HTTP and report trust

Source tag webhook: verify raw-body HMAC/delivery identity/repository allowlist, durably accept, then resolve exact tag and inspect pinned source definition asynchronously. A signature authenticates GitHub delivery; semantic tag eligibility and source policy still need validation. Activation occurs only after those checks. Source webhooks replace the Notify-App workflow relay in the refactored sandbox; they do not authorize reading inactive products before their own event.

Workers: proposed GitHub OIDC against an explicit core-ci audience. Verify issuer, signature, validity, repository ID/owner ID, assigned run/attempt, allowed workflow revision and reusable workflow identity where present. task_id in a body is not a signed OIDC claim. Verify the assigned intent against targeted GitHub run facts before binding an unknown run. Do not assume job ID or artifact identity is an OIDC claim; verify those separately. Reject confused-deputy use by another allowed workflow/task.

A one-task claim can return the immutable manifest and a short-lived read credential limited to that task's capture access. Never return the controller App key, sibling read access, feed-write token or signing secrets to source hooks. Refresh task-scoped credentials through an authenticated claim if a long build outlasts them.

Core-ci's GitHub App is installed on the sandbox repositories with required dispatch/read permissions. Generate narrowly scoped installation tokens rather than putting App private keys in every source repo. Public-feed access is the current prerequisite-download assumption; supporting a private publication feed needs an explicit reader policy.

Operator API uses an established authenticated identity provider and an explicit authorization allowlist. Do not implement a new password/session system merely for this project. Initial operator authentication choice remains D5.

## 12. Status and static feed pages

Projection is pure and state-driven; the core-ci HTTP status view updates from committed transitions without waiting for GitHub cron or another build. Preserve highest-only fields, state icons, multiline immediate prerequisite links, centered dashes, real-job links, published-only Packages links, banner and inventory navigation.

The Packages repository's Markdown pages remain a static export and must continue to exist. Recommend the same trusted feed-writer workflow support a metadata-only export operation that pulls a core-generated snapshot, preserves package bytes and shares publisher serialization. Package tasks keep priority; exports coalesce to the latest committed snapshot. No signing secrets are loaded for metadata-only work. A normal package publication also includes the latest authorized export.

This proposed export operation is real implementation cost and must be approved (D4). It avoids Git operations/package processing on the controller and avoids a second independent feed writer. Static pages can lag while an export is queued; display the snapshot time and link to the live status view. Do not promise instant static GitHub-page updates. The alternative is retaining a pure versioned renderer inside the GitHub writer with server-supplied data; do not operate both renderers as policy authorities.

Current Python render behavior is the parity oracle for tests, including local VOC fixes; migrate it once to Projection or choose a single shared renderer explicitly. Do not leave C++/Python highest-only and DAG-explanation logic diverging.

## 13. Recovery and quotas

A continuously running local timer re-evaluates durable deadlines. After restart, arm overdue work immediately subject to backoff. No external wake server or GitHub cron is necessary to run the scheduler.

Use known-run webhooks/reports first, targeted reconciliation after missed reports, bounded attempts and GitHub Retry-After/reset values. Never scan every org repo. Bootstrap missing state by verifying known tasks/receipts, not by inferring absence from a recent-time window.

No organization observer PAT is proposed in the new mode. Track App installation quotas and reconcile only known work. Retain the old observer until the old sandbox/production controller is actually retired.

The API acceptance budget is in verification-and-cost.md; it must be measured before live acceptance. HTTP request counts, primary chargeable calls and credential-specific quotas are separate metrics.

## 14. Repository and development boundaries

Canonical new source/design checkout: /home/voc/projects/snodec/core-ci. No source files live inside the framework repository. Existing framework checkout and all DP/source task diffs remain untouched.

Build consumption uses installed SNode.C public components and explicit CMake package search configuration; no hardcoded dependency on a personal source directory. Future applicable canonical formatting configs are copied/integrated when C++ starts. The initial repository contains documentation only.

Use VOC-Experiments/ci-coordinator as the thin capture/publisher worker repository, the four demo sources as build workers and VOC-Experiments/packages as publication destination. Refactor old sandbox scheduling out at approved cutover. SNodeC/core-ci is code hosting, not the test workload owner. No tag or workflow is created by this design.

## 15. Approval decisions

| ID | Recommendation / reason | Not yet done |
| --- | --- | --- |
| D1 | Async MariaDB through existing SNode.C client; genuine durable nonblocking DB I/O without application threads | DB provisioning, API spike, schema |
| D2 | Configured own-worker total budget with publisher sublimit one; remove full-org observer | Capacity policy change and sandbox config |
| D3 | Narrow final publication fence preserving cross-system safety; no timeout-only release | Protocol/spike/semantic approval |
| D4 | One pure server projection plus coalesced metadata-only operation in same GitHub writer; live view is immediate, static export timestamped | Static-export behavior/worker operation |
| D5 | OIDC worker authentication, signed source webhooks, existing IdP/operator allowlist | Public jupiter ingress, credential/App/webhook changes, operator identity choice |
| D6 | Dedicated new source repository; no existing source-task diffs mixed in; sandbox quiesce/import of verified receipts before cutover | Cutover and receipt import approval |

These are concrete architectural choices for review. No implementation, host installs, token changes, worker edits or activation is claimed complete.
