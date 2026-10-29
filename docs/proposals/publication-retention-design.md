# Publication retention: approved design and local implementation

The owner approved the policy, mechanisms and initial bounds. Implementation continues within the revised owner-approved
bounds below. Canonical checkout:
`/home/voc/projects/snodec/core-ci`, branch `feat/publication-retention`, based on main
`3cc14fd9dfaba872dacb82e57d80f70c628c50fc`. The approved implementation branch was created
from main; all four unrelated dirty document hashes were verified unchanged across that
transition. No new source checkout, reset, stash or rebase. No open PR or new commit.

## Invariant and the smallest complete direction

A publication may stop supplying new build inputs only in a committed graph transaction that
also checks existing consumers. Remote deletion follows that commit. Publication outcome,
input eligibility and an uncertain remote deletion must never be confused.

Keep three highest successfully verified source versions per repository and physical feed target,
and the highest successful rebuild revision within each. Failed/unverified work never displaces
a working version. Group by repository ID and distribution/suite/architecture/format, not runner,
mutable target name, image or SDK identity. Reuse `Version::compare`; equal SemVer precedence
(including ignored build metadata) shares a version bucket, with highest successful revision
winning. Source identities themselves remain unchanged. Stable/prerelease sharing is approved.

Worker-only deletion is unsafe. Waiting for another publication is also insufficient: after the
last build, excess files would never be removed. The smallest complete proposal therefore needs
one event-triggered cleanup operation, using the existing publication workflow and transport.
The owner approved this named architectural addition; it is not an extension of status authority.
No timer, new HTTP endpoint, authentication scheme, application lease, ready queue or scheduler.

## Source trace and reductions

| Existing boundary | Finding / proposed use |
| --- | --- |
| `Graph.cpp:59 assignment` | Retried dependencies come from the first assignment; inspect exact attempt IDs, not a prerequisite's newest receipt. |
| `Graph.cpp:173 select`, `:279 blockers`, `:359 retry` | Add one shared eligibility predicate; replace the assumption that every historical published input remains usable. |
| `Graph.cpp:318 advance` | Keep readiness order, prerequisites and dispatch capacity unchanged. No runner reservation. |
| `Graph.cpp:405 published`, `:435 failed` | Verified publication and consumer termination can change retention roots/protection. |
| `GraphStore.cpp:27 change` / `Database.cpp:write` | Existing lazy FIFO is the atomic boundary for protection, expiry and cleanup authority. No second transaction queue. |
| `Pipeline.cpp:13 change`, `:100 dispatch`, `:213 accept`, `:463 inspect` | Reuse approval, dispatch-after-commit, binding, facts and bounded reconciliation. Do not duplicate an HTTP or webhook validator. |
| `Pipeline.cpp:311 complete` / `FeedProof.cpp:verify` | Proof only runs for active publishing work. Preserve this code unless a focused repeated-root test proves a defect. |
| `GraphStore.cpp:59 view`, `:104 publicView`, `main.cpp onChanged` | Existing graph/SSE authority; expose expiry and cleanup uncertainty as metadata, without changing package execution states. |
| `native.py:20 prepare_dependencies` | Exact current-feed package/export URLs; there is no history fallback to preserve. |
| `publish.py:101 prepare_commit`, `:129 checked_push` | Replace package commit creation and push mode; preserve expected-path/hash checks and status fast-forward behavior. |
| `sign_native.py:109 retained_receipt` | Keep normal publication replay checks. Expiry must not become a replay/resurrection mechanism. |
| `sign_native.py:252 apt_indexes`, `:307 rpm_indexes` | Reuse signing/index functions for cleanup. DEB pool and Release files are shared across architectures. |
| `sign_openwrt.py:update` | Extract its signing context/index block for reuse, deleting the inline duplicate in the same change. |
| `feed_status.py:render`, `status_update.py:prepare` | Feed pages enumerate retained receipts; public progress facts are read-only derivative data, never deletion authority. |
| `PipelineConfig.cpp:deployment`, worker `trusted_deployment` | Immutable build pins stay unchanged. New cleanup commands use separately captured current approved tooling/deployment pins. |

No FeedProof compatibility layer; no second SemVer implementation; no stored protected set or
cached keep-list; no cleanup daemon; no new workflow file/configuration options. Existing package
version encoding, signatures, SDK checks and source/tag/dependency selection remain authoritative.

## Graph data and callback handoffs

Two new facts are irreducible; neither currently exists:

1. An attempt's `expired_by` operation ID withdraws future build-input eligibility. Its historical
   published state, receipt, timing and immutable assignment remain untouched. It does not say
   bytes have been deleted.
2. An immutable cleanup command with its run/evidence is recorded in the same graph document.
   It contains its ID, exact deployment pins, target, verified SDK environment when needed,
   bounded path/size/hash removals and receipt identities. Reuse immutable receipt references
   for evidence; do not copy complete build results or store a second keep/protection inventory. Record GitHub identity/progress through
   the existing validation/application path. No separate package execution state or SQL table.

This is explicit durable mutation evidence, not a ready queue: new commands are returned from
that one graph change and dispatched by its success continuation. Nothing dequeues or schedules
commands. Restart reconciliation inspects them; it never resubmits an uncertain operation.
A DB failure/uncertainty prevents its dispatch. Async callbacks re-read the current command by
ID/hash before applying facts, rather than storing a captured stale graph node.

After a verified publication, terminal consumer result, explicit retirement or selection that
supersedes waiting work, derive retention from the current graph inside the existing FIFO.
Unchanged progress/duplicate reports create no commands and no extra requests. In the same
transaction: check protections, annotate newly expired attempts and record bounded cleanup
commands. Only then call GitHub. Ordinary ready builds advance in that transaction and may
start while cleanup queues at GitHub; they do not wait for housekeeping.

Every reference created before this transaction participates in its protection check. Later
selection/retry/assignment must use the shared eligibility predicate and cannot resurrect it.
If retry wins the FIFO order, it protects its exact inputs; if expiry wins, retry returns an
explicit expired-input error. Existing SDK environment artifact expiry remains a separate,
visible retry limitation; package retention does not promise indefinite SDK reproducibility.

Protection is based on work that can still consume bytes, including waiting work with unsatisfied
but potentially satisfiable prerequisites, assigned/unbound builds, built work and publishing/
uncertain work. Superseded waiting history does not pin bytes. Failed retryable history can expire.
Explicitly retired unconfirmed publications need special care: preserve their unknown remote
outcome and receipt identity; do not turn their failed display state into proof that no push ran.

The keep calculation distinguishes two uses:

- Three-version roots and live build consumers retain required packages and development exports.
- Recorded prerequisite closure of retained packages conservatively retains package bytes for
  installation. Traverse exact original assignment references, including old attempt IDs.
  An audit-only reference does not retain development exports. No general dependency resolver.

Old closure-only packages may remain installable while ineligible as new build inputs; their
exports can expire. De-duplicate paths and never remove bytes another retained owner needs.
Never remove mutable shared indexes or keys merely because an old receipt mentions them.
An explicit older request still obeys exact prerequisite availability; no automatic ancestor work.
A new lower-version publication outside the highest-three window can expire after verification;
no unapproved age grace period or special permanent pin is added.

## Completion trigger, batching and uncertainty

A cleanup-only invocation is an explicit mode of the existing trusted publication workflow.
Its own command ID/run name distinguishes it from the original Publish run. It is not a new
attempt, replacement publication or rewritten build assignment. Generalize the existing
binding/evidence path only for this concrete operation; do not build a generic workflow engine.

Native cleanup skips source-artifact validation. OpenWrt cleanup independently prepares the
same bounded signing/index tools from the command's verified environment, using existing SDK
code. It never trusts executable source artifacts. Share deployment/run validation instead of
fabricating a build assignment or weakening normal package validation. Retain privileged keys
only in the existing feed-locked publisher job. Skip normal package receipt confirmation for
cleanup; authenticated workflow evidence reports its push outcome without changing publication
proof. A completed cleanup is not a claim about GitHub physical GC or immediate Pages visibility.

The first cleanup is dispatched after the last publication too; it needs no subsequent build.
Releasing the last consumer similarly triggers recomputation. With healthy delivery/DB/GitHub,
cleanup reaches the desired set after queued jobs execute. There is no absolute time promise
for GitHub scheduling, delivery or failures. An outstanding/uncertain cleanup is visible.

Each command fits the existing 60,000-byte request / 65,535-byte total input bounds. Withdraw input eligibility atomically when a bounded command first covers an attempt; physical
removal may span several commands, in deterministic revision order. Larger sets are processed
in bounded commands, with completion deriving the next batch from the graph, never a stored
pending list. Do not issue another command for objects already assigned to an unresolved
operation; progress duplicates must not create another cleanup. Reject an indivisible oversized item explicitly; do not spill authority into an
unverified artifact or truncate a removal list. Batching adds GitHub jobs and is a disclosed cost.

A lost POST/push response is never retried or replaced automatically. Expiry stays committed;
physical removal remains unconfirmed until usable evidence arrives. Existing explicit inspection/
restart reconciliation reads the original attempt-one cleanup run. A genuinely new publication
may act on its fresh checkout; it must not blindly replay an earlier removal command. A proven
pre-push failure is displayed, not silently retried. No automatic re-enablement after failure.

## Snapshot and worker deletion

Prepare captures checkout HEAD before creating any commit and emits it through the existing
step-output channel. Package publications and cleanup create `commit-tree(write-tree)` without
a parent. Push uses the literal captured SHA with `--force-with-lease=refs/heads/BRANCH:OLD_SHA`.
No fetch/rebase/retry changes the expected head. Status uses its existing parented commit and
normal push. Lease conflicts preserve the current controller uncertainty rule for a possible push.

Reuse the existing index, expected-path staging and byte/hash checks. Add explicit approved
deleted-path handling: verify regular file, scope, identity/hash before unlink; stage deletions
alongside writes; do not skip hash checks for arbitrary missing files. Existing keys/docs and
unaffected tracked files remain in the root tree. Do not add branches, tags, archives or backups.
Keep the existing 900 MiB tree limit before remote mutation. Other refs can retain old history;
report them during later authorized activation, without deleting owner refs automatically.

DEB: remove only the specified architecture's package paths in the shared pool; regenerate its
Packages[.gz] and the shared signed Release/InRelease using all remaining architecture indexes.
RPM: regenerate current repodata and remove obsolete metadata through the repository tool's
retention settings, keeping detached signature verification. IPK/APK: reuse the current index
and signature implementations and the verified host tools. Do not re-sign retained packages.

## Presentation and upgrade

Recommend removing feed receipts/status rows when none of their downloadable payload remains,
while preserving unverified/uncertain receipts and metadata needed for retained package rows.
Expose development-export availability separately when only runtime packages remain. This exact
metadata policy is approved. Bound public feed projections to retained/live work and referenced
prerequisites; MariaDB audit/selection history and the local history view are not purged here.
Expired-input retry is visibly unavailable with a reason; published history remains published.

Activation must use the new approved controller/workflow/tooling together. There is no feature
flag that permits deploying this controller while disabling its retention derivation. Drain
already issued old-pinned work first. New cleanup commands capture current approved tooling
independently of historical pins, without changing the historical selections.

**Unresolved activation compatibility:** draining assigned work does not handle future explicit
historical retries. Those retries retain their immutable old workflow/tooling pins, and the old
publisher uses parented commits and a fast-forward push. Thus this implementation cannot promise
that every future package publication creates a root while such retries remain eligible. A new
ref or between-sessions rollout does not solve that. No silent pin rewrite, trust broadening,
blanket old-generation expiry or compatibility mechanism is implemented. This needs a concrete,
owner-approved upgrade disposition before activation. A cleanup also rejects noncanonical old
paths rather than deleting outside its approved scope; actual live path compatibility is untested.
After expiry, rolling back to a controller unaware of eligibility annotations is unsafe.

## Approved bounds and verification

The owner approved 3,810 physical C++ lines, +400 total production lines and +270 test lines
against main `3cc14fd9dfaba872dacb82e57d80f70c628c50fc`. The local patch is within these bounds.
The implementation reuses the Database FIFO, graph, run binding, completion reconciliation,
signing/index functions and existing workflow. Added durable facts are the expiry annotation
and explicit cleanup operation; neither is a second scheduler or graph authority.

See `../reports/publication-retention-local-validation.md` for measured per-file accounting,
exact commands, evidence identity and remaining limits. These checks are local, with synthetic
GitHub and isolated real MariaDB. They establish neither live acceptance nor GitHub GC timing.
Commit, push, merge, installation, restart and activation are not authorized or performed.
