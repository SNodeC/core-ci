# Validation of the controller parallelism review

Follow-up discussion reached agreement on these corrections and the explicit remaining
design gates. See the [joint conclusions](controller-parallelism-review-agreement.md) and
[verbatim dialogue](controller-parallelism-review-discussion.md). Neither document claims
implementation approval or verified performance improvements.

The central diagnosis is correct: core-ci holds a feed-wide publication gate far longer
than the shared feed mutation requires. The proposed implementation is not ready to apply
as written. It contains policy contradictions, incomplete job handoffs and claims that
need narrower wording or evidence.

This validates [Claude's report](controller-parallelism-review.md), against the canonical
checkout on `deploy/core-vchrist`, HEAD `4257cb0`. Production source was not changed.
Uncommitted deployment and monitoring documentation was preserved. The owner's stricter
criterion applies: overlap every independent operation; serialize only real shared
mutations or required dependency relationships.

## Evidence and limits

- Read the graph, persistence, HTTP client, source selection, report handling, dispatch,
  status projection, worker scripts, workflows and relevant tests.
- Checked official GitHub concurrency and Pages documentation, linked below.
- Queried the sandbox publication environment and Pages settings read-only.
- Compared the recorded live execution timestamps with the review's timing claims.
- Built a temporary diagnostic against the current GraphStore/Database/Graph sources and
  installed public SNode.C package targets. Ran it against an isolated real MariaDB fixture.
  This is local component-boundary proof with synthetic graph inputs, not live GitHub proof.
- The diagnostic, CMake files, logs and result JSON are under
  `/tmp/codex/build/core-ci/parallelism-validation/`. The isolated database was stopped and
  removed. No deployed database was modified.
- Proposed changes do not exist yet. Their safety, exact line counts and speedup cannot
  be declared tested merely because their direction is reasonable.

## Contradictions and incomplete recommendations

### C1. “Uncertainty protection is unchanged” contradicts the proposed policy

The opening and Q5 say an uncertain writer remains protected. CP1 removes the graph's
feed exclusion, and Q5 explicitly allows unrelated writers to proceed while an earlier
publication remains uncertain. That preserves the uncertain **node**, but removes its
current feed-wide exclusion. Those are different guarantees.

The report itself eventually acknowledges the owner decision; its opening must not present
this as a purely mechanical deletion with unchanged policy. GitHub job concurrency can
protect actual job execution, while a graph node continues awaiting proof. Whether that
is the desired replacement for the current conservative exclusion must be stated explicitly.
The failed execution's assignment, inputs and dependent blockers must remain intact.

Sources: `Graph.cpp:485–505, 613–615`; report F1, CP1 and Q5.

### C2. State-only status refresh would recreate stale progress

CP3 proposes skipping evidence-only commits. A node remains `building` while its GitHub
run changes from queued to running to completed, and while its build result becomes ready
for publication. Those changes alter public activity, timing and links without changing
that state. The same applies within `publishing`.

`GraphStore::view()` derives activity from evidence; `publicView()` exports that activity
and job information. `project.md.j2` displays it. Refresh must be based on meaningful public
facts, not only the five execution states. Unchanged/irrelevant facts may be coalesced;
visible progress must not disappear until the next state transition.

Sources: `GraphStore.cpp:288–338, 350–389`; `project.md.j2:26`; report CP3.

### C3. The unlocked confirmation job lacks an exact-data handoff

`wait_for_https(selected, feed)` reads expected receipt bytes from the publisher's local
feed checkout. A separate job has a new filesystem. It does not inherit that checkout,
the exact pushed revision, or the prepared receipt merely because it has `needs: publish`.

The proposal must define a trusted handoff of the exact receipt/revision and how a
confirmation job obtains them without trusting a later mutable branch head. Current
`publish.py` also routes every non-prepare phase into private-key loading; merely adding
`confirm` to argparse would try to require the writer private key. That conflicts with the
proposed unprivileged, secret-free confirmation job unless control flow is reshaped.

Keeping the publisher job name preserves matching, but does not solve these inputs,
attempt checks, failure handling or cleanup. No private key or token may cross the handoff.

Sources: `publish.py:146–165, 169–250`; publication workflow `needs` and artifact steps;
report CP2, Q5 and its +20–40-line estimate.

### C4. CP2 is not the smallest critical section described in Q3

A job-level GitHub concurrency lock covers the entire job. Moving the lock to the existing
`publish` job leaves its runner setup, tool installation, SDK preparation, package signing,
rendering and feed work serialized. CP2 parallelizes validation and removes HTTPS waiting
from the lock, which is worthwhile, but does not parallelize everything Q3 lists.

CP4 addresses some of that remaining setup, at the cost of more jobs and handoffs. Treat
CP2 as a partial step, not proof that the owner's stricter parallelism goal is met. Splitting
jobs can itself add runner startup, installation and artifact-transfer cost. Measure it.

Sources: `core-ci-publish.yml:79–145`; `sign_openwrt.py:24–29`; report Q3, CP2 and CP4.

### C5. Removing F1 does not make ordering irrelevant

All ready publications could then be dispatched, but the current HTTP FIFO still orders
their requests. A job-level feed lock still orders actual writes by arrival at that lock.
Validation duration and runner availability influence arrival order. There is still a
resource-constrained critical path.

This does not justify adding a priority engine. First remove unnecessary serialization
and measure whether ordering still materially delays prerequisites. The report's claim
that ordering simply stops mattering is too strong.

The illustrative timing is also inaccurate: eight publications had completed before core
IPK r128's privileged job started at 11:29:16Z: r125,127,129,132,137,134,135,139. The report's
“roughly four … about 5 min each” is not the recorded sequence.

Sources: `Pipeline.cpp:264–268`; `HttpClient.cpp:130–171`; live timestamps; report F3.

### C6. Moving onChanged after a callback does not guarantee dispatch priority

It removes synchronous view work from the front of the continuation. That is a real
improvement. But a terminal-transition callback starts an asynchronous advance transaction;
onChanged can then enqueue a status read before that transaction completes. Dispatch also
has asynchronous approval steps. A cached status approval can finish before an uncached
execution approval even if the latter began first.

Therefore callback reordering alone does not prove the report's test expectation that
execution POST always precedes status POST. The complete asynchronous path must be tested,
including cold proofs and a terminal transition followed by another graph commit.

Sources: `GraphStore.cpp:273–280`; `Pipeline.cpp:259–304, 366–474, 898–921`;
`main.cpp:158–174`; report F5 and CP1.

### C7. A new publication ref alone does not solve activation

The sandbox `package-publication` environment currently permits only the `main` branch.
A new branch/tag ref would not automatically gain publication-environment access.
Also, `trusted_deployment()` requires the assignment tooling SHA, the configured
CORE_CI_TOOLING_SHA and checked-out tooling commit to agree. The workflow checks out that
mutable deployment variable, rather than a ref-specific automatically selected setting.

Thus a new workflow ref needs a complete pin/configuration/environment plan. Changing global
tooling variables can also invalidate historical assignments/retries. Activating between
sessions avoids in-flight work, but does not by itself resolve later retries of older
immutable selections. Do not rewrite those selections or broaden secret access silently.

Sources: `validate.py:12–43`; publisher workflow `CORE_CI_TOOLING_SHA`/checkout;
`Pipeline.cpp:430–465`; live environment API; report activation paragraph.

### C8. The small “deletion fix” is not the full proposed change

The opening emphasizes deleting code, but the plan adds confirmation-job handoff/control
flow, conflict re-derivation, status policy and potentially outstanding-run tracking, plus
40–70 diagnostic lines. Its own aggregate estimate is +65–165 production lines, before
optional CP4. The arithmetic is consistent; the implementation estimate is not validated.

Re-deriving generic `change(Node)` operations requires tracing its different callers and
preserving each result/failure fact. Moving confirmation needs actual inputs and output
handling. The listed affected consumers omit important parts of those changes. No estimate
here is an approval or a sufficiently detailed implementation budget.

### C9. All GitHub API calls share a FIFO; all controller HTTP does not

The GitHub wrapper is one FIFO and sends Connection: close. FeedProof has a separate
`feed-http` client for public HTTPS. The controller has distinct server listeners as well.
F6 is correct for GitHub API work, but must not be read as one connection for all traffic.
The same-origin API FIFO and the deferred dynamic/per-origin refactor are also different
questions; do not automatically bundle the latter into this change.

### C10. “Three round trips per dispatch” is a case, not an invariant

A warm approval, dispatch POST and immediate attempt-one inspect can cost three requests.
Cold authorization/proof costs more. Concurrent identical preflight reads coalesce. A
204 dispatch does not trigger that immediate inspect; a valid run webhook can supply the
binding. The necessary invariant is verified assignment binding, not a mandatory extra
REST inspect on every successful dispatch.

FeedProof's normal path here is six GitHub GETs (repository, path commit, three receipt-path
trees, blob) plus one HTTPS receipt read; cold authorization is additional. State those
budgets explicitly instead of treating all as one unqualified round-trip count.

Sources: `GitHub.cpp:207–295`; `Pipeline.cpp:309–382, 463–490`;
`FeedProof.cpp:29–113`; `SourceReader.cpp:190–285, 342`.

### C11. Several certainty and causality claims are too strong

- No matching conflict error in a live log does not refute the independently reproduced
  race; it means that observation did not demonstrate it. F4 is not a proven cause of the
  measured live delay.
- Status pushes can trigger Pages jobs, but a Git-identical replay makes no commit. A push
  does not imply a new deployment in every case. Pages contention is plausible and must be
  measured rather than asserted as the cause of every HTTPS delay.
- The controller's 119–160ms full-GET result includes view construction, serialization and
  HTTP handling. It is not a strict general upper bound for onChanged, nor an eligibility
  or commit-latency measurement.
- “Independent targets are only coupled through F1” contradicts the report's own F4–F6.
- Summing all lock times and adding per-tier build/validation times is not an exact critical
  path calculation: those intervals can overlap. Use measured dependency/resource timing.

## Findings F1–F7: disposition

| Finding | Validation result |
| --- | --- |
| F1, controller feed gate | Confirmed. It is per `(repository_id, branch)`; effectively graph-wide in this single-feed deployment, not a universal graph invariant. A definite pre-dispatch/pre-push failure can release it without FeedProof, so “only after all proof returns” needs that exception. |
| F2, workflow-wide lock | Confirmed. Independent validation and setup occupy the lock. The true feed-update section must read the current feed and retain all prior packages. |
| F3, hash ordering | Confirmed mechanism and observed long wait. Numerical example and “order no longer matters” corrected above. No priority policy invented. |
| F4, overlapping commits | Conflict/stale-node mechanism reproduced locally. Live frequency and contribution to delay remain unproven. The report overstates the available advancement triggers: successful build-result acceptance, source selection and retry also call advance. |
| F5, derivative work precedes continuation | Confirmed ordering and unconditional view construction. Actual overhead and Pages causal contribution are not measured. Callback-order-only fix is incomplete. |
| F6, GitHub FIFO | Confirmed, with separate feed-HTTPS client and warm/cold/coalesced-budget qualifications. |
| F7, existing parallelism/invariants | Eligible builds fan out; joins require all published prerequisites; acknowledgements precede asynchronous processing; scopes/proofs coalesce. These claims are supported, with F4 and shared-resource limitations. |

## Focused F4 reproduction

The temporary program uses current public GraphStore methods, the existing graph fixture
schema and a real isolated MariaDB server. Two calls queue changes derived from one graph
generation while the first transaction is still in flight.

| Observation | Result |
| --- | --- |
| Evidence update followed by an independent beginBuild commit | Evidence succeeds; beginBuild fails the generation guard; the second node remains waiting |
| Re-evaluate beginBuild from current graph | Succeeds |
| Newer same-node evidence followed by a built transition derived before that evidence committed | Evidence succeeds; built commit fails the generation guard |
| Repeat the stale built Node, as change currently does on its one conflict retry | Rejected: execution evidence regressed or disappeared; build result remains missing |
| Re-derive built from current graph using the same accepted result facts | Succeeds |

This verifies the persistence/transition mechanism behind F4. It does not simulate real
GitHub delivery as live evidence or prove every Pipeline interleaving. In the inspected live
journal since recovery, counts for the conditional-generation error, evidence-regression
error and logged webhook status503 were all zero.

Commands, using only the permitted temporary directory:

```sh
TMPDIR=/tmp/codex/build/core-ci/parallelism-validation cmake \
  -S /tmp/codex/build/core-ci/parallelism-validation \
  -B /tmp/codex/build/core-ci/parallelism-validation/build
TMPDIR=/tmp/codex/build/core-ci/parallelism-validation cmake --build \
  /tmp/codex/build/core-ci/parallelism-validation/build -j2
TMPDIR=/tmp/codex/build/core-ci/parallelism-validation python3 \
  /tmp/codex/build/core-ci/parallelism-validation/run-probe.py
```

The probe passed all expected outcome checks. Result: `result.json` in that directory.
An initial diagnostic compile omitted Graph::select's third argument; corrected before
running. No existing test was removed, weakened or rewritten.

## GitHub hypotheses and platform claims

**H1:** The 10-builds/hour Pages explanation is inapplicable to this deployment. The live
Pages API reports build_type=workflow. GitHub explicitly excludes custom Actions publishing
from that soft build limit. This does not rule out deployment/CDN delays or other limits.
[GitHub Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits).

**H2:** queue:max is documented, not an unexplained experimental key. It retains at most100
pending jobs/runs; excess arrivals are canceled. FIFO applies to when a job/run starts
waiting for the concurrency group, not workflow dispatch order. The report's expectation
that no pending job is ever canceled needs a bounded load and controls for status workflow
cancellation, manual cancellation and platform failure. The status workflow intentionally
uses the default single-pending behavior at its outer workflow level.
[GitHub concurrency](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency).

**H3:** A roughly one-second FIFO contribution is not measured. The existing client causes
sequential requests; its actual queue time must be separated from remote service time.

**Refutation criteria:** F1 is already demonstrated directly in source. If removing it leaves
another bottleneck, that does not refute its existence or its previous effect. Similarly,
absence of an observed F4 failure is not proof that the race cannot occur.

## Strict parallelism map and omissions in the review

| Work | May overlap | Required boundary |
| --- | --- | --- |
| Source/tag/definition reads for different enrolled repositories | Yes; readSources currently performs them serially, an omission in the report | All required selected definitions must succeed before graph acceptance; retain F3 error policy, identity/tag checks and consistency guard |
| Independent workflow approvals, dispatches, artifact reads and run proof | Yes where their inputs are independent | Each dispatch's authority must already be committed; no unverified run binding or mutation replay |
| Event evidence for different nodes and dependent eligibility calculation | Independent asynchronous work can overlap | One graph authority and atomic conditional database transitions; definite conflict needs current-state re-evaluation |
| Database transaction statements on the existing one connection | Keep serial | Transaction atomicity; do not serialize unrelated external I/O inside that boundary |
| Builds and unprivileged artifact validation | Yes across eligible nodes/targets | Exact prerequisite identities; all dependencies verified before an eligible dependent starts |
| Trusted immutable tooling acquisition and safe per-package preparation | Yes across jobs | No tool/key authority from untrusted source artifacts; preserve format-specific verification |
| Feed checkout, additions, affected indexes/signatures, commit and checked push | Serial per actual shared feed | Start from current head and preserve unrelated paths/bytes; status writers must use the same exclusion |
| Public HTTPS propagation and independent FeedProof for different executions | May overlap after writes | A node becomes published and unblocks dependents only after its own proof succeeds |
| Status fetching and preparation that does not depend on mutable feed state | Can overlap | Recheck freshness/merge against current feed in the feed critical section; no stale status/index overwrite |
| Status projection and SSE work | Avoid unnecessary work before dispatch, coalesce redundant facts | Preserve meaningful public progress and SSE liveness; no second state authority |

Additional source-level coupling omitted from the report: on every graph commit with SSE
receivers, main also queues enrollment->list although enrollment usually has not changed.
That repeats configuration reads on the same database connection. Source selection's global
generation guard can reject a selection while evidence advances; preserve the consistency
invariant and diagnose it rather than simply deleting the guard.

No unbounded fan-out, new controller threads, queue, timer, engine or automatic mutation
retry follows from “everything parallel.” Actual platform capacities and prerequisite/input
relationships still apply.

## Disposition of the proposed checkpoints

- **CP0:** Measurements are useful; broad production logging growth is not needed merely to
  establish the already-confirmed feed gate. Define a small evidence-driven measurement
  checkpoint, not automatic adoption of every log proposed.
- **CP1/CP2:** Direction supported, but require a coherent combined controller/workflow
  design addressing C1–C7. CP1 alone leaves GitHub validation serialized; CP2 alone leaves
  the controller gate. No independent stage should claim the full concurrency benefit.
- **CP3:** Reject state-only triggering as specified. Coalescing must preserve the exported
  facts users actually see. An optional outstanding-run latch is another mechanism: define
  lost delivery, restart and uncertain dispatch behavior, and what it replaces.
- **CP4:** Optional further design, not pre-approved. Moving trusted work outside the feed
  lock can help, but additional privileged jobs and handoffs need concrete trust and cost
  accounting. Do not infer a cache or signing-job redesign is approved.
- **Alternative B:** An outcome-sensitive gate cannot necessarily stop writer jobs already
  dispatched and queued in GitHub before an earlier outcome becomes uncertain. Its exact
  enforcement mechanism is missing; another owner-controlled policy, not a complete plan.
- **Alternative C:** Separate feed branches/repositories can partition conflicts, but is a
  deployment/layout/hosting and client configuration change. Graph's feed key alone does
  not implement multi-feed deployment; PipelineConfig currently supplies one feed.

## Required implementation proof after a corrected design

Use the existing focused graph/pipeline/worker harnesses. Prove all ready independent work
is dispatched, validation/preparation overlap as intended, the actual shared mutation section
never overlaps, and no pending publication is silently replaced within the agreed load.
Reproduce conflicts at the real Pipeline boundary and show completion cannot be lost without
another event. Test cold and warm approval interleavings, status progress without state
changes, restart and uncertainty semantics, exact confirmation handoff, attempt-one rejection,
and historical assignment/pin behavior. Maintain real client/signature acceptance.

These are required future checks, not claims that proposed code has passed them. No change
was deployed, committed or pushed during this validation. Production changes:0; repository
test changes:0. Documentation and temporary diagnostic evidence only.
