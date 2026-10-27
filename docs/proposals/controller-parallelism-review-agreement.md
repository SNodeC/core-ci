# Agreed controller parallelism review

Codex and Claude agree on the findings, constraints and preferred direction below after
three follow-up exchanges on 2026-10-27. There are no substantive reviewer disagreements
remaining. This is an agreed review and design direction, **not an implementation-complete
design, owner approval, deployed fix or measured speedup**.

Reviewed checkout: `/home/voc/projects/snodec/core-ci`, branch `deploy/core-vchrist`,
HEAD `4257cb0fc67412da8696afdba307fc2e85ff64aa`. Production source and existing tests
were unchanged. Existing deployment documentation and private directories were preserved.

Read this document together with the
[independent validation](controller-parallelism-review-validation.md).
The [full dialogue](controller-parallelism-review-discussion.md) preserves the exchanges.
The [original report](controller-parallelism-review.md) is advisory; its withdrawn statements
are superseded by these conclusions.

## Diagnosis and invariant

Independent work must not wait for unrelated remote work merely because it shares a
controller or feed. Serialize the actual shared feed update and database transactions,
and honor exact prerequisite/input relationships and resource limits. Keep one graph
authority. Commit dispatch authorization before making the HTTP mutation.

| Finding | Agreed result |
| --- | --- |
| F1: controller publication gate | Confirmed: it reserves one feed for the entire publication workflow and subsequent verification, rather than only the shared write. Removing it changes admission semantics and needs an explicit policy. |
| F2: workflow-wide feed concurrency | Confirmed: independent validation and HTTPS waiting are also serialized. Publishers and status writers still need the same exclusion for actual feed updates. |
| F3: hash-order publication selection | Confirmed. Removing one gate does not eliminate HTTP ordering, lock-arrival ordering or runner constraints. No new priority engine is proposed. |
| F4: overlapping graph writes | Conditional conflict and stale-node retry defects reproduced locally against real isolated MariaDB. If an earlier overlapping write commits and advances generation, a later eagerly captured guard is stale. No-op/failed earlier operations are exceptions. Live frequency and causal share remain unmeasured. |
| F5: derivative work | Confirmed: status/view work runs before execution continuation, and view construction is unconditional. Callback ordering alone cannot guarantee HTTP POST ordering across asynchronous proofs. |
| F6: HTTP serialization | Confirmed: GitHub API operations share one FIFO. Feed HTTPS has a separate client/FIFO. Same-origin concurrency is distinct from the explicitly deferred per-origin refactor. |
| F7: working guarantees | Eligible builds already fan out; joins require all prerequisites published; authenticated reports and immutable inputs remain necessary. Other shared resources still couple otherwise independent targets. |

The discussion also established these source-level issues:

- Status approval discards its result when generation changes and starts approval again.
  Sustained changes can starve dispatch. This possibility is confirmed from code, not
  established as the cause of a particular live delay.
- Source reads across enrolled repositories are sequential. Overlapping them requires
  overlapping transport, while retaining all selected-definition and F3 failure rules.
- Enrollment rows are read again for SSE on graph commits that did not change enrollment.
- Moving concurrency only to the publisher job still serializes that job's tool/SDK setup.
  Status jobs also hold the feed lock during independent setup.
- Selection and enrollment generation guards can reject during execution activity. Their
  replacement cannot be an enrollment-only epoch: selected definitions also reserve
  namespaces. Guard refinement remains separate work; retain current guards meanwhile.

## Preferred reduction-first direction

### Apply graph changes at the existing database boundary

Reshape the existing Database FIFO so GraphStore derives and validates a change when its
operation reaches the front, using already applied graph state. Do not add another command
queue in GraphStore. This replaces eager stale changes and the stale-node conflict retries.

Two partial orders are mandatory:

1. Successful `state.apply` and generation advancement precede the next lazy derivation.
2. A commit's execution continuation precedes its derivative `onChanged` work.

Specify release/start-next placement and audit reentrant callbacks before implementation.
Keep no-op behavior, exactly-once callback delivery, error mapping, accepted immutable facts,
attempt checks, transaction guards and dispatch-after-commit. Do not hold a transaction
across HTTP or derive/replay queued mutations after an uncertain database outcome. Explicit
selection/enrollment guards remain; they are not ordinary stale-evidence retries.

### Preserve public progress while reducing derivative work

Do not filter refreshes only by execution state: queued/running/completed evidence, job
timestamps and accepted build results can change public progress within one state.

Remove the pre-POST generation-equality rejection after successful status approval. Its
input is a minimum generation; the worker reads current facts. Retain the existing refresh
cycle and its handling of newer generations. Log approval/dispatch failures. Newer-generation
work is not permission to replay an uncertain dispatch of the same work.

Construct the full SSE view only when receivers need it. Send repository-list updates on
successful enrollment changes. Do not add a cache, epoch or outstanding-run latch by default.
Bounded transport can still make requests wait for capacity; no untested dispatch-priority
or eventual-success guarantee follows merely from callback reordering.

### Overlap independent network work within bounds

Reshape the existing HTTP FIFO to support bounded concurrent same-origin operations and
retain its existing waiting list. Trace installed SNode.C correlation, TLS identity,
ownership, cancellation and shutdown APIs before choosing the implementation or limit.

Do not introduce per-origin pools, controller threads, timers, automatic mutation retries
or unbounded fan-out. Source/tag/definition reads, independent proofs, dispatches, artifact
reads and publication proofs may overlap only when their inputs permit it. Preserve exact
IDs, approval blobs, attempts, scope, uncertainty and every required definition read.

### Restrict feed exclusion to the actual shared update

Move the publication lock to the feed-mutating job, with the same group used by status
writers. Current-head checkout, retained-package additions, affected indexes/signatures,
rendering dependent on that checkout, commit and checked push remain serialized.

Independent validation can run before the lock. Further removal of privileged setup and
per-package preparation from that job needs an exact trust/handoff/replay design and
measurement. An extra Sign job, tool cache or artifact handoff is not a default decision.

Keep normal checked fast-forward updates; never force-push, rebase stale prepared updates,
replace published bytes or unnecessarily re-sign an identical replay.

### Keep HTTPS confirmation outside the feed lock

Simply deleting the worker wait is insufficient: one-shot controller verification can run
before Pages becomes visible and receive no later assigned event.

The preferred option for further specification is an unprivileged confirmation job:

- Prepare derives the bounded expected receipt hash and local commit SHA before the push
  step. Only public values cross jobs; no key or token does.
- Keep the named push step limited to token minting and checked push.
- Confirmation requires successful `publish`, mandatory valid outputs and an approved
  workflow in which successful publication necessarily includes successful checked push.
  Do not add a duplicate `pushed` flag, `always()`, ignored errors or conditional push skips.
  GitHub's job dependency supplies the success condition.
  [GitHub job dependencies](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idneeds).
- Pinned trusted tooling validates assignment, actor and executing attempt before checking
  approved HTTPS bytes within existing bounds/origin rules. It needs no signing or feed-write
  secrets and must not enter private-key loading or publisher run-identity code.
- The hash is the exact public-byte expectation. The commit SHA is context; this job does
  not replace independent controller committed-receipt and HTTPS proof.
- Its completion produces the ordinary assigned run-completion notification. It must not
  match the publisher job-name predicate. Lost delivery, missing outputs, cancellation or
  post-push failure retain uncertainty and existing explicit reconciliation/restart recovery.

This moves HTTPS waiting out of the lock but adds another runner start outside it. Measure
the tradeoff. A feed Pages-completion webhook is an alternative requiring a new grant/path;
neither option is owner-approved by this review. No controller polling timer is proposed.

## Open owner decisions and engineering gates

These are agreed open issues, not disagreements between reviewers:

| Item | Required decision or proof |
| --- | --- |
| Uncertain publication admission | Choose write-boundary exclusion only, or a partial halt of future admission after an unknown push. Already admitted writers still proceed under GitHub exclusion in the latter option; it does not restore the old full halt. An unverified node and its dependent blockers remain intact in either case. |
| Platform capacity | Define bounded writer admission and HTTP concurrency under stated assumptions. The proposed 99-publisher figure was withdrawn as a universally proven rule. Queue overflow/cancellation does not prove no push. |
| Confirmation mechanism | Specify/approve the confirmation job or an alternative event source. Do not trade normal-time liveness for fewer jobs. |
| Lazy database operation | Complete the callback/lifetime/error/no-op audit and tests for both partial orders without adding a second queue. |
| Framework transport | Verify correlation, cancellation, TLS and lifetime against installed SNode.C and application patterns; choose/configure the concurrency bound. |
| Guard refinement | Trace exact selection/enrollment input dependencies before narrowing generation guards. Enrollment-save contention is separate scope. |
| Privileged preparation | Specify package/tool authority, compatibility, exact handoffs, retained receipts and identical replay before splitting signing/setup. |
| Trust upgrade and rollout | Preserve old/in-flight immutable selections and explicit retry semantics when tooling/workflow pins change. Main-only publication-environment access remains. Neither rollout order nor a new workflow ref alone is proven sufficient. |
| Code approval | Produce a concrete checkpoint and estimate; obtain required approval before net production growth or architectural extensions. Existing sandbox activation authority remains valid. |

## Verification boundaries

The local F4 probe is real-MariaDB component proof with synthetic graph inputs. It is not
live GitHub reproduction or a measured explanation of the observed delay. No proposed fix
has been implemented or tested during the dialogue.

Existing Pipeline/graph/worker tests remain. Add relevant behavior tests for lazy overlapping
commits, exactly-once callbacks, unchanged reports, admission/restore, out-of-order completion,
status progress under sustained changes, bounded HTTP correlation, confirmation inputs,
attempt rejection, uncertainty, restart and compatibility. Then verify intended overlap,
feed exclusion, signatures/client installation and progress timing in the authorized sandbox.

Time local CPU, database wait, transport wait, remote API work, runner queue/setup, feed
mutation, controller verification and Pages visibility separately. Millisecond local reaction
is the target; no end-to-end millisecond promise or speedup estimate is verified.

The branch-based Pages ten-builds/hour explanation is inapplicable to this custom Actions
deployment. That does not bound deployment/CDN delay.
[GitHub Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits).
Concurrency queue capacity and arrival ordering are platform constraints, not guarantees
against every cancellation or delay.
[GitHub concurrency](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency).

No production source/test changes, commits, pushes, activation, restarts or publication were
performed for this dialogue. No private directory or secret contents were inspected.
