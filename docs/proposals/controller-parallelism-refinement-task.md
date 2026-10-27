# Refinement task: make the agreed parallelism direction implementation-ready

## Task and authority

Refine the agreed controller/workflow changes into a concrete, reviewable implementation
proposal. Resolve engineering questions from current source and framework interfaces;
present genuinely unresolved owner choices with a recommendation and consequences.

This is **design refinement**, not implementation authorization. Do not modify production
code, existing tests, workflows, configuration, credentials or deployed state. Do not commit,
push, merge, restart, activate or trigger work. Write the proposal and its WORK_LOG entry.
The reviewers' agreement and provisional line estimates are not owner approval of any
mechanism, policy, concurrency limit or net production growth.

Existing VOC-Experiments deployment authority remains valid for later authorized stages.
Do not invent a new blanket activation gate. Commit, push, merge and installation still
require their own actual authorization. No production SNodeC resources may be changed.

## Workspace and evidence

Work in `/home/voc/projects/snodec/core-ci`. Read AGENTS.md and applicable global instructions.
At this prompt's creation:

- Canonical remote: `git@github.com:SNodeC/core-ci.git`; default branch: `main`.
- Current branch: `deploy/core-vchrist`.
- HEAD: `4257cb0fc67412da8696afdba307fc2e85ff64aa`.
- Deployment/monitoring documentation and review files are uncommitted; preserve them.
- No open PR was returned for SNodeC/core-ci by the read-only PR listing.

Verify the actual state before starting. These are checkpoints, not instructions to reset
to a hash. Do not switch a dirty/shared checkout, reset, rebase, stash, recover discarded
implementation or create another checkout/worktree. Refinement can remain on the current
branch. Record the branch/base needed for a later implementation task rather than changing
branches now. Preserve `.qtcreator/` and `.local-credentials/` without inspecting them.

Read in this order:

1. [Joint conclusions](controller-parallelism-review-agreement.md).
2. [Independent validation](controller-parallelism-review-validation.md).
3. [Dialogue](controller-parallelism-review-discussion.md), especially the final corrections.
4. Current source and affected tests; treat cited line numbers as navigation hints.

The original report is evidence of the review, not an independent authority over the joint
conclusions. Do not reintroduce withdrawn suggestions: state-only status updates, unbounded
HTTP concurrency, a universal 99-publisher bound, an enrollment-only epoch, a second
GraphStore queue, a duplicate `pushed` flag, a default status latch, or a supposedly safe
rollout order that has not been checked.

The isolated real-MariaDB F4 probe and its result are under
`/tmp/codex/build/core-ci/parallelism-validation/`. It reproduced conditional generation
conflicts and stale result rejection. It did not establish their live frequency or causal
share of the observed delay. If temporary evidence is unavailable, report that explicitly;
do not invent or relabel proof.

Use source inspection first. If a focused local diagnostic is necessary to settle an API
or callback question, explain what it proves, keep it under
`/tmp/codex/build/core-ci/parallelism-refinement/`, and label synthetic inputs clearly.
Do not mutate the live database or GitHub to refine this design. No unrelated/full suite.
Do not inspect secrets, private configuration, personal logs or old production workflows.

## Governing invariant and preserved behavior

Overlap independent work. Serialize only actual shared mutations, exact prerequisite/input
relationships and explicitly justified resource limits. Keep one graph/state authority and
the existing small state machine. Commit dispatch authority before the GitHub mutation.

Preserve all of the following:

- GitHub-owned authoritative definitions and immutable selected sources, definitions,
  targets, tooling approvals and exact prerequisite artifact identities.
- Numeric SemVer including prereleases; enrollment selects the highest tag; explicit older
  requests; supersession only of waiting automatic selections; no automatic prerequisite
  work; successful explicit retry may become the newest rebuild of that source version.
- Source-compatible runtime dependency ranges, with exact artifacts consumed by builds.
- Roots choose the latest official SDK; dependents reuse their pinned prerequisite SDK.
- Authenticated standard run/job webhooks and the agreed artifact-list REST boundary;
  actor/repository/workflow/run/job/attempt-one binding and independent publication proof.
- Join eligibility only after every selected prerequisite is verified published.
- Visible failures, explicit retry, package rebuild identities, retained history and bytes.
- Assignment/uncertainty after inaccessible evidence or a possible mutation; no automatic
  mutation replay, replacement run or inference of failure from a missing receipt/job list.
- Signing and all package/SDK/image work on GitHub. No controller package processing.
- Existing SSE and public progress, including evidence changes within one execution state.
- Current SNode.C lifecycle, managed/unmanaged MariaDB ownership and one event loop.

No controller threads, polling timer, new persisted execution state/schema, ready queue,
generic engine, custom reporting/authentication protocol, registry/SDK client, unrelated UI
redesign or SNode.C change. The per-origin HTTP-client refactor remains deferred.

## Required refinement

### 1. Trace responsibilities and classify every wait

Trace source selection through graph derivation, persistence, dispatch, webhook application,
artifact completion, FeedProof, dependent advancement, status refresh and SSE. Include
all callers of GraphStore commit/enroll and Database completion, not just the failing path.

For each wait, name the inputs, owner, lifetime, current serialization and whether it is:
required dependency, shared mutation, resource-capacity wait or removable coupling. Cover
independent source reads, API proof chains, build dispatches, artifact reads, publication
verification, publisher/status setup and feed updates. Preserve F3: an unreadable newly
selected repository with unknown relevance blocks the request and is reported.

Produce a dependency/concurrency map and event timelines. Distinguish local C++ work,
database wait, transport wait, remote API response, runner queue/setup and Pages propagation.
Local millisecond reaction is the target, not an end-to-end millisecond guarantee.

### 2. Specify lazy graph derivation in the existing Database FIFO

Prefer reshaping the existing queue over adding a GraphStore command queue. Specify actual
interfaces and callback ownership for producing a GraphChange/SQL only when its database
operation reaches the front, against already applied graph state.

Required partial orders:

1. Successful state application and generation advancement precede the next lazy derivation.
2. That commit's execution continuation precedes its derivative `onChanged` work.

Choose and justify the full release/start-next order. Audit every `completed()` path and
reentrant enqueue, including no-op, derive/validation exception, transaction-start failure,
statement failure, rollback, commit success, uncertain commit, shutdown and callback lifetime.
Show exactly-once callback delivery and no transaction held across HTTP. Stop before deriving
queued mutations when database mutation outcome is uncertain.

Convert all affected transition callers coherently. Closures retain accepted immutable event
or result facts and attempt IDs, then consult current graph state. Do not retry stale Nodes.
Preserve no-op/report behavior, error mappings, terminal monotonicity and dispatch-after-commit.
Identify obsolete eager construction/retry paths to delete in the same checkpoint.

Retain explicit selection/enrollment guards. An enrollment-only epoch is unsound because
selected definitions also reserve namespaces. Do not silently broaden this task to fix admin
save contention or narrow consistency guards; document those remaining limitations.

### 3. Specify bounded same-origin HTTP concurrency

Study relevant SNode.C implementation, multiple `src/apps` examples and MQTTSuite usage.
Verify installed public client APIs for connection/request correlation, TLS identity,
callback lifetime, cancellation, shutdown and errors. Cite concrete interfaces/patterns;
do not assume a connection-ID design is supported before tracing it.

Reshape the current FIFO into bounded concurrent operations using its existing waiting list.
Explain how each response/error is matched exactly once and how a submitted request remains
uncertain without being resent. Retain token scope/expiry and existing coalesced token/proof/
artifact reads under overlap. Do not create per-origin pools or change dynamic host routing
as a side effect. State what happens if an origin change is requested with work in flight.

Propose a concurrency limit and configuration, with evidence and tradeoffs; distinguish an
engineering recommendation from owner approval. Do not assume a 20-node graph bounds every
deployment or that a platform maximum is an appropriate normal operating limit.

Pair transport overlap with concurrent independent source reads. Aggregate results once,
with deterministic failure reporting and no late callback corrupting a completed selection.
Keep tag-object, enrollment ID, required-definition, deployment-pin and generation checks.
Do not parallelize sequential proof requests whose inputs genuinely depend on prior results.

### 4. Reduce derivative status/SSE work without losing progress

Specify removal of the pre-POST generation-equality restart after successful approval.
The dispatched generation is a minimum and the worker reads live facts. Keep newer-generation
handling; distinguish fresh derivative work from replay of an uncertain POST. Define approval
failure and dispatch-uncertainty behavior honestly, including the no-new-commit case.

Do not use five-state-only filtering: queued/running/completed observations, timing, links
and build-result acceptance can alter public progress within a state. Build the full SSE
view only for receivers; emit repository-list changes after successful enrollment changes.
Trace all consumers so neither reconnect/bootstrap nor enrollment feedback becomes stale.

Do not add an outstanding-run latch, cache or new timer by default. Explain existing GitHub
coalescing and its limits. Measure/estimate request cost instead of promising zero-cost public
refreshes. Callback reordering is not proof that one HTTP POST always wins a race.

### 5. Define publication admission separately from feed-write exclusion

Identify every graph/restore/validation/advance check enforcing the current one-publisher
gate, and the tests that encode it. Specify coherent replacement so a later implementation
does not delete one check while another silently restores the same serialization.

Present these unresolved owner choices without selecting one as already approved:

- **Write-boundary exclusion:** independent work may be admitted while another node awaits
  publication proof; the actual feed-mutating jobs remain mutually exclusive.
- **Partial future-admission halt:** an observed unknown push outcome stops new admissions;
  already admitted writers may continue under the same feed lock. Specify behavior for
  missing/delayed job evidence. This is weaker than the current full exclusion.

Distinguish active writer, uncertain dispatch, completed job with unknown push outcome and
landed-but-unverified publication. No choice may release the uncertain node, satisfy its
dependents or replay its mutation without evidence.

Specify a bounded admission recommendation using all participating writers/status jobs and
platform queue capacity. State assumptions about external/manual writers and missing evidence.
Do not hard-code 99 as a proven universal rule. Queue overflow/cancellation and an empty job
listing remain subject to current uncertainty policy. Do not add a scheduler to manage this.

### 6. Specify the smallest coherent GitHub workflow change

Move the shared feed concurrency boundary from the whole publication workflow to its
feed-mutating job. Keep publishers and status updates in the same actual write group.
Read current head inside that boundary; preserve retained packages/unrelated paths; regenerate
affected metadata, sign/verify, commit and checked-push together. No stale prepared commits
followed by force-push or rebase.

Independent artifact validation runs outside that boundary. Identify remaining independent
setup inside locked jobs. Further signing/tool preparation splits require concrete authority,
compatibility, safe handoff, retained-receipt and identical-replay/no-resigning semantics.
Do not default to extra privileged jobs, executable tool artifacts or caches. Show measured
or explicitly provisional startup/transfer/setup tradeoffs. Mark partial checkpoints as partial.

Refine the preferred unprivileged HTTPS-confirmation job:

- Prepare derives bounded expected receipt hash and local commit SHA before push starts.
- The named push step remains token minting plus checked push; preserve its failure-certainty
  boundary and controller interpretation of skipped versus possibly executed push.
- Mandatory bounded public outputs cross jobs. No token/private key crosses the handoff.
- `needs: publish` supplies the successful prerequisite condition. The approved workflow
  must have no ignored push errors, conditional push skips or `always()` bypass; do not add
  duplicate `pushed` state. Validate outputs before use.
- Confirmation uses pinned trusted tooling, validates assignment/actor/executing attempt,
  avoids publisher identity/private-key branches, has no signing/feed-write secrets, and
  checks approved HTTPS bytes against the exact hash within existing bounds/origin rules.
- The commit SHA is context, not a substitute for controller committed-receipt proof.
  Confirm must not match the publisher job-name predicate. Trace run versus job conclusions
  and the ordinary terminal run event that wakes existing controller completion logic.
- Missing output, cancellation, lost report, timeout and post-push failure preserve uncertainty
  and existing reconciliation/restart behavior. No new controller polling or custom callback.

Compare with a Pages-completion event only as an explicit alternative requiring a new grant/
event path. Recommend one; reviewer preference is not owner authorization. Check actual GitHub
job/output/concurrency semantics in official documentation. Do not infer Pages latency bounds
from coalescing or apply the branch-based ten-build/hour limit to custom Actions publishing.

### 7. Resolve trust-upgrade and activation compatibility

Trace the selected tooling SHA/workflow blob, mutable deployment settings and protected
main-only environment. Build a compatibility table for old/new controller, workflow and
tooling combinations, including already assigned work, waiting/built selections and explicit
retry of historical failures. Inspect source statically; do not change live settings.

Do not call permanent historical retry breakage an acceptable acknowledgement. Recommend
a bounded owner-controlled upgrade strategy that preserves immutable inputs/history and
explicit retry requirements; expose any genuine policy choice. Do not silently broaden trust,
rewrite selections or invent a general compatibility layer. A new ref or a between-sessions
rollout alone is not proof. Specify rollback limits after multiple same-feed publishers
exist; an older controller's restore validation may reject them.

Separate actual approval/input gaps from stages already authorized in VOC-Experiments.
Leave current listener addresses, TLS, UI exposure and ports untouched; never touch port 443.
Intermittent 403 diagnostics/recovery, service management and production SNodeC rollout are
outside this refinement.

## Required proposal and verification plan

Write `docs/proposals/controller-parallelism-design.md`, documenting the current proposal
rather than a history of discarded approaches. Include:

1. Governing invariant, exact affected functions/interfaces and each wait's owner.
2. Concurrency map and normal, overlapping, failed, uncertain and restart timelines.
3. Concrete callback/data handoffs, bounded transient state and exactly what each replaces.
4. Recommended smallest coherent checkpoints, dependencies and what remains serialized.
   Cover all agreed directions; do not call a partial first checkpoint the complete solution.
5. A short owner-decision table: recommendation, alternative, consequence and required approval.
   Resolve source/API questions yourself; ask the owner only for genuine policy or authority.
6. Production/test/documentation added, removed and net estimates per checkpoint. Count C++,
   worker Python and workflow YAML as production. Explain every net addition and delete the
   superseded paths. Prior advisory estimates are not the checkpoint budget or approval.
7. Focused tests through existing graph/store/pipeline/worker boundaries: overlap without
   lost facts/advance; reentrant no-op/error callbacks; unchanged reports; all eligible work
   committed before dispatch; join correctness; bounded request correlation/scopes; status
   under sustained progress; exact confirmation and attempt rejection; uncertainty/restart;
   admission limits and trust/rollout compatibility. No mirroring-only or unrelated tests.
8. Request accounting including tokens, approvals, dispatch, artifact/receipt proof, public
   HTTPS and derivative status work. Parallelism must not multiply unchanged report reads
   or duplicate completion/proof work. Keep normal/cold/recovery costs separate.
9. A sandbox-only acceptance plan: capture assignments before restart; verify restoration
   without replacement dispatch; demonstrate independent overlap and shared-write exclusion;
   compare receipt/graph/SSE/status/Pages timestamps; preserve existing bytes/keys/tags and
   verify signatures/client installation where affected. Specify gates honestly.
10. Measurable acceptance targets for local processing and capacity behavior, clearly labelled
    proposals until measured. Use the recorded baseline; do not equate a fast GET, low average
    CPU or a late aggregate log with exact eligibility/commit latency.

Use official GitHub and installed framework references for uncertain interface claims.
Distinguish source-proven behavior, prior local synthetic evidence, live observations and
untested proposals. No new live acceptance or performance success may be claimed here.

Update WORK_LOG.md with decisions, evidence, remaining gates and exact workspace/source.
Report the proposal path and outstanding owner decisions. Stop before implementation;
present the concrete design and quantified approvals required by AGENTS.md.
