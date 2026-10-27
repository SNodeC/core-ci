# Controller parallelism review dialogue

Read-only discussion between Codex and the installed Claude CLI, 2026-10-27.
The current conclusions are in [the joint review](controller-parallelism-review-agreement.md).
This transcript preserves proposals and objections verbatim. Earlier statements that
were withdrawn are superseded by the final reply and joint conclusions; they are not
implementation instructions, owner approval or verified performance claims.

Claude used safe plan mode with Read/Grep/Glob only, no command/write tools or MCP.
All three follow-up invocations exited successfully with empty stderr. Neither reviewer
changed production code, existing tests or deployment during this discussion.

## Follow-up 1: Codex to Claude

````text
The owner asks us to continue discussing the review until both reviewers agree on every
aspect of the corrected diagnosis/design. This is read-only review/design, not permission
to implement. Read AGENTS.md and preserve the working tree and all private directories.
Use only current source/tests and review documents; no credentials, private config, SSH,
GitHub mutations, builds/tests or file edits. Return text; the primary agent records it.

Read your original report:
docs/proposals/controller-parallelism-review.md
Read my detailed independent validation:
docs/proposals/controller-parallelism-review-validation.md
Read the local reproduced conflict outcomes:
/tmp/codex/build/core-ci/parallelism-validation/result.json

For each C1–C11, findings F1–F7, hypotheses H1–H3 and checkpoint dispositions:
- Agree, disagree, or qualify; cite current source where it matters.
- Correct your earlier statement where warranted; do not agree merely to finish.
- Identify any error or overstatement in my validation as well.
- Produce a corrected concrete design that honors the owner's strict requirement:
  overlap every independent operation; serialize only actual shared mutations and exact
  prerequisite relationships. C++ local reaction should be measured in milliseconds,
  independently of remote network/job time. No generic engine/ready queue/controller threads,
  timers, registry/SDK clients or custom callback authentication.
- Do not quietly change owner policy. In particular distinguish an uncertain graph node,
  an executing writer, a completed writer of unknown outcome, and admission of independent
  new writers. Explain what is safe/proven and which semantics need owner decision.
- Specify a minimal coherent controller/workflow boundary, exact confirmation data handoff,
  status progress/coalescing semantics and lost delivery/restart behavior. Challenge whether
  a third confirm job is even necessary: existing controller FeedProof already independently
  checks committed receipt plus public HTTPS; can redundant worker HTTPS polling simply be
  removed while retaining node-published proof? Assess normal-time liveness if controller's
  HTTPS check runs before Pages visibility and no further event arrives. No new timer or
  invented automatic retry policy. Do not trade latency for stranded published nodes.
- Include every relevant caller and asynchronous continuation, source read serialization,
  same-origin HTTP FIFO vs deferred per-origin refactor, publication hash ordering, source
  selection consistency guard, unnecessary SSE/config reads and stale commit conflicts.
- Explain whether a minimal first coherent checkpoint meets strict parallelism or is only
  partial; avoid optional caches/new jobs/logging as defaults. Exact estimates remain
  provisional until the corrected design is settled, and growth requires approval.
- Existing sandbox deployment authority is already granted. Do not add a blanket live
  activation approval gate; separate genuinely new policy/code-growth questions.

Evidence qualifications to preserve:
- Local real-MariaDB public GraphStore probe reproduced generation conflict leaving a
  second node waiting and stale built retry rejected for evidence regression. Re-derivation
  passed. It is not a full Pipeline live reproduction; live inspected logs had zero matches.
- Actual Pages build_type=workflow; 10-build/hour branch-based limit is inapplicable here.
- queue:max documented up to100pending; FIFO by lock-wait arrival, not dispatch order.
- Publication environment branch policy currently permits only main.
- Eight completed publications preceded core IPK's privileged publisher start.

End with: agreed items; remaining substantive disagreements; remaining owner decisions;
a proposed jointly reviewable specification. Avoid declaring consensus where a concrete
handoff, race, evidence guarantee or liveness condition remains unspecified.

````

## Follow-up 1: Claude to Codex

````text
# Second-round review: corrected diagnosis and joint design

I only read files: the current source, workflows, tests and the three review artifacts. Nothing was edited, built or run. Plan mode was on, but no write tool was available, so the plan file was not created; everything is in this text for you to record. GitHub/Pages facts that only your live queries established (build_type=workflow, the environment branch policy, the r128 sequence) are accepted as your evidence; I did not re-verify them.

## 1. C1–C11

| Item | Position | Notes and corrections to my report |
|---|---|---|
| C1 | **Agree** | My opening ("deletion… an uncertain writer is still never released") hid a policy change. The uncertain *node* stays preserved. The graph-level *admission* exclusion per feed is what gets removed. An owner text says "Retain … per-feed writer exclusion" (`docs/proposals/webhook-request-reduction-task.md:147`), so this is an owner decision (§5). |
| C2 | **Agree; I withdraw state-only triggering and the latch** | Two further facts make a facts-diff filter unnecessary. The status job reads *live* facts when it runs, and `generation` is only a minimum (`status_update.py:27-40`). GitHub's workflow-level status group already keeps at most one pending run (`core-ci-status.yml:11-13`). You missed a real defect, though; see N1. |
| C3 | **Agree** | `publish.py:198` sends every non-`prepare` phase into private-key loading. The exact handoff is in §6.6. |
| C4 | **Agree** | CP2 is partial. The job-level lock still covers apt/pip setup, the SDK, signing and rendering (`core-ci-publish.yml:85-129`). |
| C5 | **Agree** | I withdraw "order no longer matters" and the "four × 5 min" illustration; I accept your eight-publication sequence. Residual ordering is the HTTP FIFO (seconds) plus arrival at the GitHub lock (set by validation duration). That is not a reason for a priority policy. |
| C6 | **Agree** | I drop the "execution POST precedes status POST" test. The invariant to test is different: no execution dispatch waits on any status request. Qualification: status and publication approval both read `/commits/<publicationRef>` in the worker repo with identical permissions, so `Pipeline::read` coalesces them (`Pipeline.cpp:309-325`). Your cold/warm inversion applies to *build* approvals in other repos. |
| C7 | **Agree; I withdraw the "new publication ref" suggestion** | It would need an environment-policy change. Activation is on `main`, between sessions, under the existing sandbox authority. Waiting or built selections pinned to the old blob would fail their proof (`Pipeline.cpp:347-349`). Old failed selections stay undispatchable under the existing pins. That is a fact to acknowledge, not a new gate. |
| C8 | **Agree** | Estimates are provisional (§8). |
| C9 | **Agree** | Correction to F6: there are two FIFOs. `github-http` has a fixed peer (`GitHub.cpp:87`). `feed-http` re-targets its host per request (`HttpClient.cpp:141-147`). |
| C10 | **Agree** | FeedProof on its normal path makes 6 GitHub GETs plus 1 HTTPS read. A dispatch costs a coalesced ref GET, then the POST, then an inspect only when a `workflow_run_id` comes back. Cold tokens and proofs cost extra. |
| C11 | **Agree on all bullets** | My refutation criterion ("logs never show 503 → F4 theoretical") was wrong. F4 actually fires every time two graph writes overlap (N2). |

## 2. F1–F7, H1–H3, checkpoints

| Item | Final disposition |
|---|---|
| F1 | Confirmed, with your exception. A definite pre-dispatch failure (`rejectedDispatch`) or a verifiably skipped push (`Pipeline.cpp:789-794`) releases the slot without FeedProof. |
| F2 | Confirmed. |
| F3 | Mechanism confirmed. Impact corrected as in C5. |
| F4 | Confirmed and **stronger than either report said**. The guard captures `generation` when a write is queued (`GraphStore.cpp:206`). `Database` runs writes FIFO, so any graph write queued while another is still pending *always* fails once that earlier write commits. It is rare live only because commits take milliseconds and are spaced out by network reads. Concurrent HTTP replies would make overlaps common, so the F4 fix is a prerequisite for concurrency. Your trigger correction (built, selection and retry also call advance) is accepted. |
| F5 | Ordering and the unconditional `view()` (`main.cpp:163`) are confirmed. "Near-continuous status dispatch" was wrong: the controller has at most one refresh cycle in flight and GitHub keeps one pending run. The real problems are N1 and the status job's lock scope (N4). |
| F6 | Confirmed, with the C9/C10 qualifications. |
| F7 | Confirmed, minus my sentence "only coupled through F1". Targets are also coupled through F4, F5/F6, the GitHub feed lock and runner capacity. |
| H1 | Withdrawn: build_type=workflow. Pages deploys coalesce (`pages.yml:10-12`: one running, one pending). A status push can delay a publication's visibility by at most about one deploy. Measure it. |
| H2 | Your correction is accepted: documented behaviour, at most 100 pending, FIFO by arrival at the lock wait. N3 turns this into a required admission bound. |
| H3 | Not measured. Under the strict requirement the FIFO counts as a defect whatever its size, because one dispatch waits on unrelated remote I/O. |
| CP0 | Dropped as a production default. Use existing GitHub timestamps plus harness timing. Any log line needs separate approval. |
| CP1/CP2 | Merged into checkpoint A (§7). |
| CP3 | Replaced by the N1 fix. The latch is not a default. |
| CP4 | Becomes checkpoint C: designed, measured, separately approved. |
| Alt B | Your criticism holds for B as I wrote it. A precise, enforceable variant (P2) is in §5. |

## 3. Errors or gaps in your validation

1. **F4 is framed as a race of unproven frequency.** It is deterministic for overlapping writes, and HTTP concurrency (which strict parallelism needs) raises the overlap rate. "Definite conflict needs current-state re-evaluation" is also not the best fix. Deriving each change when it actually runs (§6.2) removes the conflict class instead of retrying it.
2. **The status-refresh starvation in `refreshStatus` was missed** (N1).
3. **Alternative B was dismissed** as lacking a mechanism. It can be enforced as an admission rule over persisted evidence, with stated semantics for writers already admitted (§5).
4. **Parallel `readSources` was listed as an omission without noting it gains nothing** while `github-http` remains a FIFO. It belongs with the HTTP change.
5. **The strict map says status "fetching and preparation" can overlap.** Only the facts fetch and dependency install are independent. `prepare` renders pages from the current feed checkout, so it belongs in the critical section.
6. **The `queue: max` bound was framed as a test-load caveat.** It is a runtime hazard once admission is unbounded (N3).

## 4. New findings from this pass

- **N1: status refresh can starve while generating a continuous stream of GETs.** `Pipeline.cpp:396-403` drops the dispatch if the generation moved during the approval GET, then re-issues the GET. If commits arrive faster than the GET's queue time plus round trip, it never POSTs. Each loop also puts another GET into the shared FIFO. The pre-POST drop protects nothing, because the job reads live facts and `generation` is a minimum.
- **N2: F4 is deterministic for overlapping writes** (above). A concrete loss path: `built` is derived while an evidence write for the same node is queued. The guard then fails, the same-node retry is rejected as "evidence regressed" (your probe), and the node sits at `building` with a finished run. No further event arrives.
- **N3: unbounded admission strands nodes once more than 100 jobs are pending.** GitHub cancels the excess pending jobs. A job cancelled before it starts has no `skipped` push step, so `beforePush` is false (`Pipeline.cpp:789-790`) and FeedProof fails. The node stays `publishing` as uncertain even though no push happened.
- **N4: the status job holds the feed lock for checkout, venv and pip install** (`core-ci-status.yml:19-45`). Only feed checkout, prepare and push need it. The status job's locked time measures as long as a small publication's.
- **N5: unrelated graph commits break enrollment saves.** Saves fail on any graph generation change: `Enrollment.cpp:399`, and `enroll` uses the generation from when editing started (`:455-459`). During a busy session this can happen *after* the GitHub PUT has committed. Atomic namespace enforcement already exists (`GraphStore.cpp:18-23`). This is an admin path; its scope is your call.
- **N6: a burst of selections or enrollment saves re-reads the repository list.** `enrollment->list` runs on every commit when SSE receivers exist (`main.cpp:167-173`), but the list only changes on `enroll` (the `repositories` row is written only there).
- **N7: removing the worker HTTPS wait without another event strands normal publications** (§6.7).

## 5. Uncertainty semantics (owner policy, not changed quietly)

| Category | Graph evidence | Can it still mutate the feed? | Protection under every option |
|---|---|---|---|
| Uncertain graph node | `publishing`, not proven | depends on the rows below | Assignment and inputs preserved, dependents blocked, never auto-released |
| Executing writer, or dispatch of unknown outcome | `publishing`, publish job not `completed` (or no run bound) | yes, but only inside GitHub's job-level feed group | Mutual exclusion through the group plus a checked fast-forward push |
| Completed writer of unknown outcome | publish job `completed`, push step not `success` or `skipped` (e.g. 403, lost runner) | no (job ended) | Later writers check out the current head, so a landed push is included |
| Landed but not yet proven | push step `success`, FeedProof pending or failed | no | Same |

**Proven safe under both options:** commits never interleave or race on the feed, and nothing force-pushes or rebases. No package identity ever gets different bytes. A join still needs every prerequisite verified.

**Owner decision: admission of a new independent writer on the same feed.**
- **P1 (write-boundary exclusion only).** Admission ignores other nodes. This matches the strict requirement literally.
- **P2 (admission halt on an unknown push).** `publicationReady` returns false while any node on the same feed is `publishing` with publish-job `job_status=completed` and `push_step_conclusion` not in {`success`, `skipped`}. All of that is persisted evidence, so it survives a restart and never waits on network time. Writers already admitted proceed under the GitHub lock. Only `published`, a verifiably skipped `failed`, or administrative recovery releases the halt. The step split (§6.6) is what makes "landed, not yet visible" distinguishable from "push unknown". Today both show as a failed step.

**Applies to both options (a derived platform bound, not a new policy; owner-visible):** admit a publication only while fewer than 99 nodes on the same feed are `publishing` without completed publish-job evidence. That leaves 100 pending minus one status job. The alternative would be treating a cancelled, never-started job as a definite no-push, which changes failure-certainty policy. I don't recommend it.

## 6. Corrected design

### 6.1 Boundary

- **The controller** owns admission, dispatch, evidence, the authoritative FeedProof and status triggering.
- **The workflows** own feed mutation exclusion: one job-level group per feed, shared by publishers and status jobs, plus a fast-forward push.
- **The confirm job** is only an event source. Its result is never trusted as evidence.

### 6.2 Graph commit sequencing (fixes F4, N2 and the stale `change` retry)

- `GraphStore::commit` takes a derive closure (`→ GraphChange`) instead of a finished change.
- GraphStore keeps at most one graph write outstanding. The next closure is evaluated only after the previous write has been applied and `generation` incremented. Validation and the guard therefore always use exact state. `enroll` joins the same sequence.
- A closure that throws returns a definite (not uncertain) error. Callers keep their current 409/422 mapping, so "late result; attempt preserved" semantics are unchanged.
- The DB generation guard stays, against external writers and restore staleness.
- `change()`'s retry (`Pipeline.cpp:914-918`) and `record()`'s retry (`:637-644`) are deleted.
- Callers converted to closures:
  - `commitSelection` (select from applied state)
  - `retry`
  - `advance` (the scan runs inside the closure; dispatch uses the nodes it returns)
  - `record` (`bindRun`)
  - `change` (rejectedDispatch ×2, failed ×3, `built`, `published`)
- This is a transient in-memory sequence inside the single authority. It is neither a ready queue nor persisted, and it adds no latency: writes already serialize on the one connection.
- **Restart:** pending closures are lost. Their facts came from acknowledged events, and the existing startup `reconcile()` re-inspects every building/publishing node and then advances (`Pipeline.cpp:866-899`). A lost tag-push selection is not recovered; that is existing behaviour.

### 6.3 Admission

- Delete the feed checks in `publicationReady`, `validate` and `restore` (`Graph.cpp:489-496, 580, 613-616, 159, 164-165`). Keep `publicationReady` inside `beginPublication`.
- `advance` admits every ready publication in one change (remove `publicationSelected`).
- Add the 99 bound, plus P2 if chosen.

### 6.4 Status, SSE and configuration reads

- **`refreshStatus`:** always POST with the captured generation once approval succeeds. After the POST returns, re-fire if the generation moved. Keep "uncertain POST → no resubmit; next commit re-fires".
  - **Guarantee:** for the newest committed generation G, either a status run reading facts ≥ G is dispatched, or the last dispatch was uncertain and logged. Progress that changes evidence but not state is preserved.
- **`GraphStore`:** call `callback` before `onChanged`. Build `view()` only when there are receivers. Send the `repositories` SSE event only after a successful `enroll`.
- **Selection guard:** replace the global generation check (`Pipeline.cpp:46,112`) with an in-memory enrollment epoch, incremented on successful `enroll` (the only enrollment mutation). It is checked inside the selection closure. The consistency invariant (rows and deployment pins unchanged during the read) is preserved.
- **N5:** the same epoch for the enrollment save guard, if you include it.

### 6.5 HTTP and source reads (checkpoint B)

- **Same-origin concurrency in `HttpClient`.** Each operation gets its own connection, correlated by connection ID instead of `requests.front()`. This is allowed only while the host and port equal the client's current fixed target. `github-http` is fixed. FeedProof already requires the configured feed origin (`FeedProof.cpp:36`), so in practice `feed-http` targets one host too. A host switch still waits for the FIFO to drain.
- **No dynamic per-origin pools.** That deferred refactor stays deferred.
- **No cap of our own.** Natural concurrency is roughly nodes × a small constant, and GitHub documents a limit of about 100 concurrent requests. Add a cap only if you ask for one.
- **`readSources`:** issue all repository selections concurrently, aggregate them, and answer once. Keep the "first non-404 error fails the request" behaviour (F3 policy), the ID and tag-object checks, and the epoch guard.
- **Status API budget:** with concurrency, a busy session could approach about 4,000 status requests per hour. Measure it. Coalescing to GitHub's run cadence (an in-memory latch cleared by the status run's own webhook, reconcile or restart, never by a timer) is an owner decision, not a default.

### 6.6 Workflows (checkpoint A)

- **`core-ci-publish.yml`:**
  - Move `concurrency` (with `queue: max`) from workflow level to the `publish` job. Keep its name `Publish <label>` and the step name "Publish checked feed update", so `matchesJob` and push-step evidence are unchanged.
  - The push step becomes token plus push only. After a successful push it writes `receipt-sha256` (sha256 of the committed receipt bytes in the job's own checkout) and `feed-revision` to `$GITHUB_OUTPUT`. These are non-secret, and nothing is read from the mutable branch head.
  - Add a new job `confirm`, named e.g. "Confirm public receipt" (it must not match `Publish …`):
    - `needs: publish`, no environment, no secrets, `contents: read`
    - pinned tooling checkout
    - `publish.py --phase confirm` checks the assignment and `trusted_deployment`, and returns before any secret handling or `run_identity`
    - polls `base_url/receipts/<execution>/<revision>.json` until the sha256 matches, using the existing 600 s / 10 s bounds
- **`core-ci-status.yml`:** unchanged in this checkpoint. Shrinking N4 needs a job split, so it goes with checkpoint C.
- **Controller code for confirm: none.** `complete()` already waits for both run conclusion and publish-job completion (`Pipeline.cpp:668`). A confirm failure leaves the run in `failure` with a successful push step, so FeedProof runs and the node stays `publishing` if HTTPS is not visible. That is the same as today, minus the lock time.

### 6.7 Is the confirm job necessary? (N7)

Yes, as an event source, given the current grants.

- **Why removal strands nodes:** FeedProof is one-shot. On an HTTPS mismatch it returns 202 with no retry (`Pipeline.cpp:796-809`, `FeedProof.cpp:93-106`). Timeline in a normal run:
  - Push, then the run finishes about 2 s later.
  - The webhook arrives a few seconds after that, and FeedProof reaches HTTPS within about 5–15 s.
  - Pages needs runner start, upload and deploy, so it typically becomes visible 40–90 s after the push.
  - So in normal operation the controller would check before visibility, and no further assigned event arrives: a stranded node until manual reconcile. Latency would have been traded for stranded published nodes, which you ruled out.
- **Alternative E2: the feed's Pages `workflow_run completed` webhook.** It would re-verify every `publishing` node whose run has completed. It needs the controller App granted on the feed repository plus about 20–40 lines of controller code. That is a new grant, so it is an owner decision. Lost delivery behaves the same as E1: recovered only by reconcile.
- **Recommendation:** E1 (the confirm job).

### 6.8 Deployment order

Either order is safe. A new controller with old workflows still serializes in GitHub. Old controller with new workflows means one publication at a time. Activate both on `main` while the graph has no waiting, built or publishing nodes.

## 7. Checkpoints and strictness

- **A (coherent, partial):** §6.2–6.4, §6.6 and the admission rules.
  - Removes the controller gate, validation and HTTPS waiting in the lock, F4/N2 loss, N1 starvation and the pre-continuation view work.
  - Not strict yet: local dispatch still waits behind unrelated GitHub requests, and the lock still covers privileged setup.
- **B (strict local reaction):** §6.5.
  - After A and B, every step's latency is local milliseconds plus only its own prerequisite I/O: token when cold, ref read, proof, artifact list, FeedProof chain.
- **C (strict GitHub-side):** split into an unlocked privileged `Sign` job and a locked `Publish` job (feed checkout → index → sign → render → commit → push), with hash-checked tool handoff. Shrink the status job's lock (N4) the same way.
  - Requires measured stage times and separate approval. It carries runner and artifact costs.

## 8. Provisional estimates (growth needs approval)

| | Production | Tests |
|---|---|---|
| A, C++ | net +20 to +70 (gate −15, retries −15, sequencing +35–60, epoch/SSE/status +10–20, bound/P2 +5–15) | +150–300 |
| A, YAML/Python | +25–45 | +20–40 |
| B | +50–100 | +60–120 |
| C | +100–200 | +80–150 |

## 9. Tests and acceptance (requirements, not results)

**Public-boundary tests:**
- **Graph:** several `publishing` nodes per feed; restore accepts them; the bound; P2 if chosen; joins blocked by an uncertain prerequisite.
- **`pipeline.py`:**
  - N ready publications produce N POSTs, each after its own durable commit (replacing the `:176` "exactly one" assertion).
  - An evidence webhook interleaved with `built`/`published`/advance still yields dependents with no further event.
  - Status requests that are delayed or failing never delay an execution POST.
  - A status refresh is dispatched under sustained commits (N1).
  - Restart with three `publishing` nodes.
  - With B: concurrent requests are correlated correctly, and shutdown cancels them.
- **Worker:** the lock is on `publish` only; `confirm` has no secrets or environment and checks the sha256; still no force push.

**Live acceptance (sandbox, existing authority):**
- validation jobs overlap; locked publish and status jobs never overlap
- no publish job cancelled within the bound
- every join's dispatch comes after its last prerequisite's verification time
- feed history is linear
- client signature tests pass
- receipt → POST latency stays in milliseconds in harness timing

## 10. Summary

**Agreed:**
- C1–C11 as qualified above, F1–F3 and F5–F7 corrected, the H1 withdrawal, the H2/H3 qualifications.
- Uncertain nodes are never released and only verified publication satisfies an edge.
- The publisher lock moves to job level and HTTPS waiting leaves it.
- State-only status triggering and the latch are dropped.
- No new ref, no CP0 logging by default, CP4 gated by measurement.

**Remaining substantive disagreements (for you to answer):**
1. The F4 remedy: derive at execution time (mine) versus re-derive on conflict per caller (yours).
2. Whether same-origin HTTP concurrency belongs to the deferred refactor or to strict parallelism.
3. Whether you accept N1, N3 and N7 as stated, and P2 as an enforceable form of Alternative B.

**Owner decisions:**
1. Admission policy: P1 or P2.
2. The 99 admission bound, versus changing what a cancelled publish job proves.
3. Approving checkpoint B now, given the earlier deferral of the per-origin refactor.
4. E1 (confirm job) or E2 (Pages webhook with a new feed grant).
5. Approval for checkpoint C after measurement, including the SDK-duplication options.
6. Whether the N5 enrollment-save fix is in scope.
7. The status-coalescing latch, only if measured API use requires it.
8. Acknowledging the effect of pinned old blobs on historical failed selections at activation.

**Proposed jointly reviewable specification:** §5–§7 as written. It is not yet consensus. These points are still open:
- your acceptance of the closure semantics in §6.2 (error mapping, `record`'s early check for unchanged evidence)
- connection-ID correlation in §6.5, which needs confirming against the SNode.C client API before an estimate is fixed
- the exact P2 evidence predicate for runs whose publish-job webhook was lost (it currently counts them as not completed, i.e. conservatively)

````

## Follow-up 2: Codex to Claude

````text
Continue the read-only dialogue. Read your response in
/tmp/codex/build/core-ci/parallelism-validation/claude-response-1.md and the current
source where needed. No edits, commands, credentials or external mutations. Return a
concise response (about 1,500 words), addressing the numbered points below individually.
Do not re-read/restate every earlier report. We need honest technical consensus, not
agreement by repetition. An agreed unresolved design gate is not a completed design.

My response to your second round:

1. AGREED: F1/F2 are real redundant long-lived gates. C1-C11 qualifications stand,
including your useful C6 qualification: the same worker/ref/scope lookup can coalesce;
cold build approvals remain a valid inversion example. Same-origin concurrency and the
deferred per-origin refactor are distinct; that was already my position, not disagreement.
Parallel readSources needs transport overlap to gain network latency; I agree they belong
together. Status render against current feed stays locked, independent facts fetch/setup
can overlap; this was exactly the boundary in my map.

2. F4: accept deriving graph transitions immediately before their serialized persistence
operation rather than retrying stale Nodes. That is preferable IF we reshape the existing
Database operation FIFO into a lazy-operation boundary so state.apply and generation
advance happen before the next derivation. Avoid adding a second GraphStore command queue
on top of the Database queue. Compare that reduction-first variant against a new queue;
state what it replaces. Preserve one authority, definite/uncertain distinction and error
mappings, attempt identity, immutable accepted facts, no-op behavior, callback ownership,
exactly-once callbacks, and commit-before-dispatch. Do not re-evaluate an uncertain mutation.
Your 'every overlap always fails' needs conditions: earlier write must succeed and advance
generation; no-op writes or earlier failures do not establish it. Live overlap frequency
is still unmeasured. My validation explicitly says local mechanism proven, live frequency
unproven; deterministic conditional behavior does not contradict that.

3. N1 accepted as source-proven starvation possibility, not a reproduced live cause.
Removing the pre-POST generation equality check is coherent: minimum generation and live
facts permit it. Preserve progress facts without state-only filtering or a new default
latch. Your eventual dispatch guarantee needs failure exceptions: approval can fail
definitely (not just uncertain dispatch). Newer-generation derivative work is distinct
from retrying the same uncertain POST. Do not promise no execution waits on status I/O
until transport/resource scheduling proves that; bounded capacity can still queue work.

4. N3 accepted as platform-capacity risk, not yet exact proposed bound. I reject treating
hard-coded 99 as a proven derived rule: count only accounts for controlled latest attempts,
while status or other same-group writers and delayed/missing evidence complicate capacity.
It is conservative for this controlled sandbox but needs assumptions and owner-visible
admission policy, not hidden automatic approval. Queue overflow is one possible cause of
cancellation; absence of push evidence remains uncertainty. Do not infer an empty job list
proves no push. P2 is enforceable ONLY as a partial future-admission halt: already admitted
writers continue, unlike full old exclusion. Calling it 'Alternative B precisely enforced'
must state that weaker semantics. Lost job completion leaves unknown, not proof of safe
completion. Owner policy P1/P2 remains open.

5. N4/N6 accepted source-level waste; N5 needs separate scope/consistency trace. I do NOT
accept replacing global selection/enrollment guards with only an enrollment epoch. The
namespace claims table also changes from GraphStore::commit(selection) through
reserveNamespace, not only enroll. Concurrent source selections may change assumptions
other than enrollment; prove semantic input dependencies before narrowing the guard.
Do not add an epoch/cache by default. Keep existing guard until a separately specified
replacement is proved; deferred guard refinement must be visible as a limitation.

6. N7 accepted: remove worker HTTPS wait outright and normal one-shot FeedProof can strand
work. Keeping polling on GitHub outside the feed lock is coherent. Your proposed hash+SHA
job outputs are a simpler exact handoff than copying the full feed/receipt artifact, and
I accept that direction. Specify: expected receipt hash and actual local committed SHA
are derived in prepare, before push starts, with size/path/assignment bounds. The push
step remains token minting + checked push, preserving failure certainty; make only public
outputs, no key/token transfer. Confirm uses pinned trusted tooling, validates executing
attempt/actor/assignment through trusted_deployment, polls approved HTTPS within current
bounds and redirects/origin constraints, matches exact receipt hash, no signing/feed-write
secrets or run_identity call. Outputs from a failed/cancelled push must not cause confirmation
of a success we did not observe. Controller still independently verifies committed receipt
and HTTPS before published; confirm merely ensures a later assigned run completion event.
Unexpected post-push failure/missing output/lost delivery stays uncertain, recovered only
by existing reconcile/restart. All timings like 'Pages typically40-90s' are unmeasured in
this round; they are illustrative, not proved bounds. Is this fully agreed?

7. H1: reject 'delay at most about one deploy'. Coalescing does not bound wall-clock time:
arrivals/cancellation, runners, deploy failure and CDN propagation matter. State simply
branch-based10/h limit inapplicable, Pages latency needs measurement. C5's FIFO 'seconds'
also unmeasured. 'Every step after A+B local milliseconds plus own prerequisite I/O' is a
goal, not proved under resource bounds. Local CPU/DB/transport wait/remote I/O/worker queue/
Pages must be timed separately.

8. HTTP: reject unbounded concurrency as default. Enrolled graph size is unbounded, and
readSources + reconcile + callbacks can exceed a platform limit regardless of typical
20-node use. Reshape the existing FIFO into bounded concurrent operations, preserving
remaining waitlist rather than a new scheduler. Exact bound/configuration and framework
correlation/lifetime design need source verification and approval; do not invent a safe
number or claim SNode.C API feasibility before tracing it. No per-origin pool, no retries,
no thread/timer. Shutdown/error/transport cancellation must preserve uncertain submissions.
Does this resolve the supposed disagreement without smuggling deferred refactor?

9. Activation: accept main-only constraint and old failed selections currently rejected by
changed trusted pins. Reject treating permanent historical retry breakage as resolved just
by 'acknowledge it'. Explicit retry/immutable history are product requirements. This is a
deployment/trust-upgrade design gate: choose owner-approved retention/allowlist or explicit
new selection/rebuild semantics later, not silent rewrite or broad trust. Neither an
automatic controller/workflow deployment order nor new environment ref can be declared
safe until compatibility for in-flight/historical work and changed job evidence are checked.
Existing sandbox deployment approval stays valid; no new blanket activation blocker.

10. Strictness: A/B are partial checkpoints, C is still a design gate. A Sign job outside
lock and handoff needs exact package/tool authority, trust, compatibility, retained receipt
and identical replay/no-resigning semantics. Platform job-level locks necessarily include
some runner start; don't promise literal zero overhead or maximal parallelism from an
unspecified C. Record which waits are required vs remaining avoidable serialization.
Do not default to extra jobs/caches/logging. Estimates remain provisional, not approval.

Please reply agree/disagree/qualify for 1-10 with reasons. Identify any actual technical
disagreements left; list owner decisions and engineering gates separately. If these
corrections are accepted, we can agree completely on the diagnosis, principles and open
gates without pretending an implementation-complete design or tests exist.

````

## Follow-up 2: Claude to Codex

````text
# Third-round reply on parallelism validation

I only read files: my earlier response, `Database.cpp`, `GraphStore.cpp`, `Pipeline.cpp:30-129` and `Enrollment.cpp:385-465`. I edited nothing and ran nothing. No write tool was available, so no plan file was created.

## 1. Agree

Nothing to add. When I listed this as an open disagreement, I misread your position. That was my error.

## 2. F4 via the Database FIFO: agree, with one required ordering fix

**Ordering problem.** As the code stands, deriving at operation start would still see stale state. In `Database::writeNext`, the commit-success path calls `completed()` before `write->callback` (`Database.cpp:131-133`). `completed()` starts the next queued operation synchronously (`:36-41`). `GraphStore`'s `state.apply(change)` and `++generation` only run inside that callback (`GraphStore.cpp:272-276`). So the next operation would start before the previous write is reduced. The reduction-first variant has to reorder this:

- **Success:** run the store's apply hook (`state.apply`, `++generation`), then `completed()`, then `onChanged`, then the caller's callback.
- **Failure:** the order doesn't matter, because nothing is applied. Every `completed()` site (`:90, :99, :132, :137, :145, :150, :155`) still needs checking for exactly-once callback delivery.

**Lazy boundary.**
- `Database::write` gets a variant whose statements come from a producer. The producer runs when the operation reaches the head of the queue, and only after the `mutationUncertain` check (`:89-91`). So nothing is re-derived after an uncertain mutation.
- `GraphStore::commit` passes a producer that does the following against the applied state:
  1. derive the `GraphChange`
  2. `validate` it
  3. run the no-op test
  4. build the statements, with the guard on the current `generation`
- **Results:**
  - A derive or validate exception is a definite error, so callers keep their 409/422 mappings.
  - A no-op calls `completed()` and then the callback with success, without opening a transaction. That keeps the current behaviour at `:200-204`.
  - Commit-before-dispatch holds, because dispatch only happens in the caller's callback.
  - Accepted facts and attempt IDs are fixed inside the closure. The closure re-reads node state, not event facts.

**What it replaces:**
- eager statement construction in `commit` (`:191-271`)
- the generation captured at enqueue (`:206`), which is the F4 root cause
- the retries in `change()` and `record()`
- my proposed separate GraphStore write sequence, which I withdraw

There is no second queue: the Database FIFO already orders reads and writes on one connection. `enroll` can stay eager because its guard uses an explicit `expectedGeneration` (see point 5).

**Existing quirk, kept as is.** After an uncertain mutation, writes still queued are reported `uncertain=true` even though they never ran (`:91`). Changing that would be a separate decision.

**Your condition on "every overlap fails" is correct.** It fails only when the earlier write commits and advances the generation. A no-op or a failed earlier write does not cause it. "Mechanism proven, live frequency unmeasured" is accurate.

## 3. N1: agree

Corrected guarantee: for the newest committed generation G, exactly one of these holds:
- a status run reading facts ≥ G is dispatched, or
- the approval failed definitely, or
- the last dispatch was uncertain.

Both failure cases are logged. The next commit fires a new refresh. That refresh is new work for a newer generation, not a retry of the uncertain POST. I withdraw "no execution dispatch waits on status I/O". The testable claim is narrower: no execution dispatch waits for a status refresh to complete. Under bounded HTTP capacity (point 8), both can still queue for transport.

## 4. N3: agree

"Derived rule" overstated it. 99 is a conservative sandbox assumption. It depends on:
- GitHub's documented limit of 100 pending jobs per group
- at most one pending status job
- no other writer in the group (no manual dispatches, no leftover old runs)
- runs whose publish-job webhook was lost counting as not completed

That last assumption only costs availability, not safety. The bound must be owner-visible admission policy, not hidden auto-approval.

- An empty job list does not prove no push happened. A cancelled job stays uncertain.
- If P2 is chosen, it should be described as a partial halt on future admission. Writers already admitted keep running. That is weaker than old Alternative B's full exclusion. A lost job-completion event leaves the outcome unknown.

## 5. N5/N6 and the selection guard: agree; I withdraw the enrollment epoch

The source supports you, and the problem is worse than you stated:
- `GraphStore::commit` writes `namespace_claims` through `reserveNamespace` on selection (`GraphStore.cpp:215`).
- `Enrollment::checkClaims` reads that table before the GitHub PUT, then checks the generation (`Enrollment.cpp:393-400`).

So the save path really does depend on selection commits, and an epoch bumped only by enrollment would allow a stale claim check. The DB insert is still atomic (`GraphStore.cpp:18-23`). But a 409 *after* the GitHub PUT has committed is exactly the bad outcome. The same unproven-assumption risk applies to the selection guard (`Pipeline.cpp:46,112`).

- Both global guards stay. Refining them is a deferred engineering gate and should be recorded as a known limitation.
- N6 stands as stated: `list` joins on `configuration_revision`, which only `enroll` writes (`Enrollment.cpp:57`, `GraphStore.cpp:169-173`). Fixing it needs no cache or epoch. Only emit the `repositories` SSE event after `enroll`.

## 6. Confirm job: agree, with three details

1. **A pushed flag.** Hash and SHA are now computed in prepare, so they exist before the push. The push step must therefore also write a `pushed=true` output, and only after its checked push has succeeded. `confirm` runs only when that flag is `true`. Then a failed or cancelled push can never lead to confirmation of a success we didn't observe. The push step keeps token minting plus the checked push. Outputs are public values only, with bounded size and format.
2. **Run conclusion.** The run's conclusion now includes `confirm`. A confirm timeout or failure makes the run `failure` while the push step shows `success`. FeedProof then runs and the node stays `publishing` if HTTPS isn't visible, which matches today's behaviour. `matchesJob` must not match "Confirm …".
3. **Latency.** Confirm takes nothing out of the locked time, but its runner start adds to the time until `published`. That is unmeasured.

Everything else is as you specified:
- confirm uses pinned trusted tooling, validates attempt/actor/assignment through `trusted_deployment`, never calls `run_identity`, and holds no signing or feed-write secrets
- it polls approved HTTPS only, within the current bounds and redirect rules, and matches the exact receipt hash
- the controller still verifies independently
- lost or missing outputs stay uncertain, recovered only by reconcile or restart

The 40–90 s Pages figure is illustrative only. With these details included, I agree fully.

## 7. Timing: agree

"At most about one deploy" is withdrawn. The branch-based limit of 10 deploys per hour doesn't apply, and Pages latency needs measuring. The "seconds" figure for the FIFO is withdrawn too. "Local milliseconds plus own prerequisite I/O" is a goal, not a measured result. These should be timed separately:
- local CPU
- DB wait
- transport wait
- remote I/O
- runner queue
- Pages

## 8. HTTP: agree; this settles the disagreement

My "no cap" position is withdrawn. Graph size is unbounded, so total request volume is too. The plan becomes: reshape the existing FIFO into at most N concurrent operations, with the rest waiting in the existing list in order.

- No per-origin pools, retries, threads or timers.
- Shutdown, errors and cancellation must leave submitted requests uncertain, never re-sent.
- I haven't traced SNode.C's client connection, correlation or lifetime API. Feasibility and the value of N are open engineering and owner gates.

This doesn't sneak the deferred refactor back in, because the pools and their routing stay out.

## 9. Activation: agree; I withdraw two claims

- "Acknowledge it" is withdrawn. Explicit retry and immutable history are product requirements. Breaking retries for history pinned to old trust is a design gate for the trust upgrade, with these options:
  - owner-approved retention or allowlist
  - explicit new-selection or rebuild semantics
  - never silently rewriting history or broadening trust
- "Either order is safe" (§6.8) is withdrawn. Old controllers with new workflows, and the reverse, both depend on how the changed job evidence (the `confirm` job, job-level concurrency) is interpreted. I haven't checked either combination.

The existing sandbox deployment approval still stands.

## 10. Strictness: agree

- A and B are partial checkpoints. C is a design gate.
- The Sign/Publish split needs:
  - exact package and tool authority
  - trust and compatibility with the existing evidence
  - retained receipts
  - identical replay without re-signing
- A job-level lock always includes the runner start and checkout of the locked job. Required waits are the feed-mutation critical section and runner start; anything outside those is avoidable serialization.
- No extra jobs, caches or logging by default beyond what you've accepted. Estimates stay provisional.

## Remaining technical disagreements

None of substance. The only correction is the one in point 2: `Database` has to run the reduction hook before `completed()`. That's a precondition of your approach, not an objection to it.

## Owner decisions

1. Admission: P1, or P2 described as a partial halt.
2. The admission bound and its stated assumptions, or a policy for cancelled publish jobs.
3. E1 (the agreed confirm job) or E2 (Pages webhook, which needs a new grant).
4. The HTTP concurrency bound N and how it's configured.
5. Trust-upgrade semantics for historical and in-flight selections.
6. Approving checkpoint C after measurement.
7. Whether N5 is in scope.
8. A status-coalescing latch, only if measured API use requires it.

## Engineering gates

1. Specify the lazy `Database` write: success-path order, an audit of every `completed()` site, and how no-ops and definite errors are handled.
2. Trace SNode.C client correlation, lifetime and cancellation for bounded concurrency.
3. Trace input dependencies before narrowing the selection or enrollment guards (deferred, recorded as a limitation).
4. Check activation compatibility for controller/workflow combinations and the changed job evidence.
5. Run timing separately for each component in point 7.
6. Specify checkpoint C's handoff (authority, trust, replay).
7. Write and run the public-boundary and live acceptance tests. None exist yet.

If you accept these corrections, we agree on the diagnosis, the principles and the open gates. That still isn't an implementation-complete design.

````

## Follow-up 3: Codex to Claude

````text
Final narrow read-only clarification. Your third-round reply is
/tmp/codex/build/core-ci/parallelism-validation/claude-response-2.md.
I accept its corrections on conditional overlap, lazy DB production, no-op/uncertain
semantics, N1, bounded transport, retained guards, activation gates and measured strictness.
No substantive disagreement on those principles remains. Two details before recording
consensus; please reply within about600words, no broad re-investigation.

1. You propose success: apply, completed, onChanged, caller callback. That puts derivative
onChanged before the execution continuation again (F5). Our agreed required partial order
must instead be: apply+generation BEFORE any next lazy derivation; caller execution
continuation BEFORE its derivative onChanged. Release/start-next placement and reentrant
no-op/error callbacks must be specified/tested without introducing a second queue, invoking
callbacks twice, holding a DB transaction across HTTP, or replaying uncertain work. We do
not need to settle an untested total callback order in this review; keep it an engineering
gate with those two partial-order requirements. Agree?

2. I reject default addition of pushed=true as duplicate state when GitHub's needs/result
already represents the successful publisher job. Official docs verified now:
https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idneeds
Documents prerequisite jobs must complete successfully; failure/skip propagates unless
overridden. A confirm job needs publish (or explicit needs.publish.result == 'success'),
with no always/continue-on-error or conditional skipping of push. The approved workflow
must make publish success imply successful named checked-push step, and confirm validates
mandatory bounded receipt-hash/SHA outputs. Thus no extra pushed flag is necessary. A
post-push failure/cancel remains uncertain exactly as before; controller can still verify
independently at run completion. If future workflow conditions invalidate this implication,
revisit at that change rather than duplicating state now. Agree, or show a concrete current
counterexample using our restricted approved workflow?

Also qualify your last wording: 'confirm takes nothing out of locked time' presumably means
new runner startup is outside the lock; relocating the existing HTTPS wait DOES remove
that wait from the lock. No required/avoidable runner-start claim beyond platform lock
granularity until measured. Existing Pipeline tests exist; proposed behavior tests are
not implemented yet (rather than 'none exist' unqualified).

If agreed, explicitly confirm there are no substantive reviewer disagreements, and that
this is consensus on findings, design constraints, preferred reduction-first direction
and clearly enumerated open owner/engineering gates, not implementation-complete design,
owner approval or verified speedup. I will record your replies verbatim plus the joint
conclusions, with all overstatements marked superseded. No production edits/deployment.

````

## Follow-up 3: Claude to Codex

````text
I agree on both points, with the wording corrections you asked for. I only read files: my previous reply, `core-ci-publish.yml` and `publish.py`. I edited nothing and ran nothing.

## 1. Callback order: agree

You're right. My proposed order "apply, `completed`, `onChanged`, caller callback" put the derived `onChanged` before the caller's execution continuation, which brings F5 back. I withdraw that total order. The engineering gate keeps only two required partial orders:

- **(a)** On success, `state.apply` and `++generation` happen before the next queued lazy producer runs.
- **(b)** On the same commit, the caller's execution continuation runs before its derived `onChanged`.

The gate must also specify and test the following, without a second queue, double callback delivery, a DB transaction held across HTTP, or replaying uncertain work:

- where release and start-next go
- reentrant no-op and definite-error callbacks
- exactly-once delivery at every `completed()` site

We don't need to settle an untested total order now.

## 2. `pushed=true`: agree, withdrawn

I found no counterexample in the current restricted workflow:

- The `publish` job (`core-ci-publish.yml:78-144`) has no step-level `if:`, no `continue-on-error`, and no `always()`. Every step's failure fails the job.
- Push phase (`publish.py:245-250`): there's no exit-0 path that skips `checked_push`. Push runs unconditionally, then `wait_for_https`, and `run` raises on a non-zero git exit.
- **Edge case I checked, not a counterexample:** if prepare stages no change (`publish.py:107-109`), no commit is made. The push is then an up-to-date no-op and succeeds. The receipt bytes really are on the branch at that point, and `confirm` checks the exact receipt hash and SHA. So "success" still means the right content is published.

So `confirm` with `needs: publish`, no `always()` or `continue-on-error`, and no conditional skip of the push step is enough. Mandatory, bounded, format-validated receipt-hash and SHA outputs are still required. A failure or cancellation after the push stays uncertain, and the controller still verifies independently. If a future workflow change adds conditions that break the implication "publish succeeded ⇒ the checked-push step succeeded", revisit it then. I agree not to duplicate state now.

## Wording corrections

- **"Confirm takes nothing out of locked time" is replaced by:** the new `confirm` runner's startup happens outside the lock. Moving the existing HTTPS wait (`wait_for_https`, currently inside the locked push step) into `confirm` *does* take that wait out of the locked time. The cost is extra unmeasured latency until `published`.
- **Runner start:** I withdraw my "required waits are … runner start" claim beyond this: a job-level lock includes the locked job's runner start and checkout, given the platform's lock granularity. I make no claim about whether that's required or avoidable until it's measured.
- **Tests:** "None exist yet" is replaced by: existing Pipeline tests exist; the proposed behaviour and acceptance tests are not implemented yet.

## Consensus statement

There are no substantive disagreements left between us. We agree on:

- the findings, including F4 and F5, with conditional overlap, mechanism proven, live frequency unmeasured
- the design constraints: lazy DB production, no-op and uncertain semantics, N1, bounded transport, retained global guards, activation gates, measured strictness, and the two partial orders above
- the preferred reduction-first direction
- the open owner decisions and engineering gates listed in my previous reply, as amended here

This is **not** an implementation-complete design. It's not owner approval of any option or bound, and not a verified speedup; every timing figure stays provisional until measured. Overstatements in my earlier replies, including the total callback order, `pushed=true`, "takes nothing out of locked time", the runner-start claim and the unqualified "none exist", should be marked superseded. You can record this reply verbatim.

````

## Codex final acceptance

I accept the final corrections. There are no substantive reviewer disagreements left
on the diagnosis, design constraints, preferred reduction-first direction and explicitly
open owner/engineering gates. Required callback partial orders and the existing
successful-prerequisite-job condition replace the superseded total order and pushed flag.
The receipt hash supplies the confirmation job's exact public-byte expectation; the
commit SHA is bounded context, not a substitute for the controller's independent
committed-receipt and HTTPS verification. Implementation details, approvals, compatibility
and performance proof remain open as listed in the joint review.
