# Unconfirmed publication dispatch: investigation and proposed correction

Investigation and locally verified correction, current checkout `/home/voc/projects/snodec/core-ci`, branch
`rewrite/slim-graph-controller`, HEAD `032cb34`. The correction is implemented locally; no live retirement/retry, GitHub mutation, deployment
or restart was performed. Final verification is recorded below.

## Current execution and evidence

Execution `1a7d0b931a08306be8ce1879fefaf54d13cd1b7c9fa11f7d5ea5bf84a5c2d89a`,
attempt suffix `:169`, selects demo-core v2.1.4 / OpenWrt 25.12 / x86_64 / APK.

- [Build 40661265395](https://github.com/VOC-Experiments/demo-core/actions/runs/40661265395)
  completed successfully at 09:04:49 UTC on 2026-10-28. Artifact evidence was accepted.
- The saved attempt is `publishing`. Publication preflight records worker commit
  `721537bc7d0478b564eb397c3c87069778ac8cfe` and approved workflow blob
  `c2b5e78b721d2e649490baa390e3ea5e7f8cdf9b`. There is no publication dispatch ID,
  bound publication run, job evidence or receipt. A preflight authorizes a POST;
  it does not prove submission or acceptance of that POST.
- At 09:19:51.070 UTC the application logged “dispatch outcome is uncertain;
  assignment retained.” A status dispatch was also unconfirmed at .069.
- A nearby framework log at .067 reports a client TLS shutdown broken pipe and read
  descriptor warnings. The application did not retain the HTTP status, submitted flag
  or transport error, or correlate the operation to a connection. Those warnings do
  not prove the cause of this dispatch result. There is no evidence identifying a 403
  or token expiry as its cause.
- A complete date-filtered publisher-workflow listing returns 24/24 runs for Oct 28,
  with no r169. Neighbouring r165/r166/r168 runs were created at 09:19:52 UTC.
- The exact unfiltered lookup used by the controller returns 100 of 159 runs, including
  all 24 from Oct 28. Its page limit is a latent recovery problem, not why this r169
  was missed. No match is hidden on an older page in the independently complete day list.
- The feed receipt path has no committed history. Absence of a listed run or receipt
  is not proof the original POST was never accepted; deleted history and unobserved
  outcomes cannot be ruled out. There is no currently discoverable publisher run to cancel.
- Six nearby status runs cannot be correlated to the generic status-dispatch error:
  the log did not record its generation. Do not infer that particular request succeeded.
- The separate r167 delay followed an inaccessible artifact read at 09:19:34.418 UTC;
  it recovered later. It is distinct from r169's missing publication dispatch outcome.
- The newer core APK r189 is verified published:
  [publication 40663767448](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40663767448).
  The entire fresh 20-execution chain has completed. Retrying r169 is not required for it.

## Invariant and actual cause of the stalled controller state

Commit authority before dispatch. When submission/acceptance is uncertain, retain the
assignment; never infer definite failure or replay a mutation from missing evidence.
Only a verified publication satisfies dependent edges.

The known cause of the persistent controller state is an unconfirmed publication dispatch
with no subsequent matching evidence and no policy-authorized way to retire it. It is not
an identified runner job waiting for capacity. The original HTTP failure mechanism is
unknown because the diagnostic details were discarded. A 5xx, lost/invalid response or
unrecognised success response cannot be distinguished retrospectively.

The state trace is `graph::advance` → committed publishing assignment → workflow proof →
committed preflight → POST → uncertain-result log. `inspect()` can read a known attempt-one
run or search for an unbound run; `accept()` owns identity validation and observation;
`complete()` invokes independent receipt/HTTPS proof only after applicable terminal run/job
facts. `graph::retry` permits only failed executions. Restart reconciliation preserves a
preflight-backed assignment. That explains why clicking Inspect does not produce Retry.

## Verified code findings

| Finding | Current source | Disposition |
| --- | --- | --- |
| Dispatch diagnostics discarded | `Pipeline::dispatch`, `refreshStatus` | Correct: log status/submitted/fixed transport error and execution or generation. No headers, tokens or raw response bodies. |
| Search failures hidden | `Pipeline::inspect` | Correct: distinguish unreadable, incomplete, zero matches and ambiguity. Jobs-read errors must not look like successful inspection. |
| Reconciliation overstates success | `Pipeline::reconcile` | Correct: return inspected count and per-execution unresolved outcomes; reads finishing does not prove asynchronous graph commits completed. |
| Generic progress presentation | `GraphStore::view` | Correct: derive “publication run unconfirmed”/equivalent from absence of a verified binding. This can also describe the normal short dispatch window. No new stored flag. |
| First-page-only recovery | `Pipeline::inspect` | Report incomplete honestly; defer pagination rather than add routine requests. Known-ID and authenticated-webhook binding remain unchanged. |
| `submitted` stays true after `send()==false` | `HttpClient` | Source-level defensive classification issue; no reproduction or evidence r169 reached it. Defer from this correction. |
| Synthetic status zero described as invalid | `HttpClient` | Wording issue; not established as r169's cause. Defer. |

The current framework source's false-send branch queues no request; true means queued,
not remotely accepted. The false branch appears practically unreachable in the current
connected-hook adjacency. Do not change pessimistic submission marking without checking
synchronous callback lifetime/reentrancy. Current framework source inspection is not proof
of the exact installed binary's implementation or a runtime reproduction.

## Recommended bounded correction

1. Reshape existing error logs to retain safe diagnostics for both execution and status
   POSTs. Keep full uncertain outcomes assigned. Do not add request replay.
2. Validate the listing and its completeness before claiming a unique unbound match.
   An incomplete or inconsistent page remains incomplete even if it contains one match:
   accepting the first candidate does not prove uniqueness. Keep known-run-ID reconciliation
   and full attempt-one actor/repository/workflow validation through the existing path.
   A complete zero-match list is “no matching run,” not proof of non-submission.
3. Replace the existing reconcile pending-counter aggregation with transient result
   aggregation, reporting inspected reads and unresolved execution/reason pairs. This
   replaces an existing callback-completion mechanism; it is not another queue or authority.
   Do not label a candidate resolved before its asynchronous graph application succeeds.
4. Derive honest run-unconfirmed activity for active work without a verified run. Keep
   graph state, blockers, assignments, retries and SSE authority unchanged.

No schema, state, cache, timer, thread, worker change or automatic recovery protocol.
No extra routine GitHub requests. No blanket pagination or recovery loop.

Estimate: **+8–12 net C++ lines**, **+60–100 test lines**, **0 UI-asset lines**.
Prefer reductions during implementation; no offsets have yet been established. The current
controller is 3,510 lines, so an upper bound of **3,522** needs explicit owner approval.
These are proposed bounds, not implementation authority or completed tests.

## Separate owner decision: retire this obsolete uncertain assignment

Under the current rule, evidence-based reconciliation is the only authorized recovery.
Repeated inspection cannot manufacture a missing result. The diagnostic correction does
not itself resolve r169 or make retry safe.

Recommendation: allow explicit operator retirement of r169 **without retry**, since the
newer APK and full chain already published. This is a new policy requiring owner approval:

- Name the execution and exact current attempt, and explicitly acknowledge that the remote
  outcome is unknown. This acknowledgement also covers a later explicit retry; do not add
  acknowledgement flags or parse failure strings to invent another state machine.
- Inside the existing graph transaction, require latest-attempt identity, publishing state,
  no bound publication run, no recorded publication dispatch ID and no publication evidence.
  Reject stale or changed requests atomically. These guards restrict the action; they do
  not prove no POST occurred.
- Use existing failed state with an explicit reason such as “operator abandoned unconfirmed
  dispatch; remote outcome unknown.” Preserve the assignment and full history. This records
  an operator decision, not a definite GitHub failure, and does not cancel a remote run.
- Do not retry automatically or modify SQL by hand. If an operator later retries, it creates
  a new rebuild revision; an old unobserved run may still exist or have published. Approval
  explicitly acknowledges that residual uncertainty and relaxes the current no-replacement rule.
- Late authenticated facts can still enter the original attempt's evidence. Existing
  completion logic will not automatically publish a failed/retired attempt or satisfy its
  dependents. Do not promise to verify a late receipt automatically after retirement.

Alternative: keep r169 assigned and unresolved, with truthful presentation. That preserves
the current policy exactly and does not prevent the newer completed chain from working.

Retirement estimate, separate from the core correction: **+15–25 C++ lines**, **+8–15 UI
lines**, **+40–60 test lines**. Both together imply at most **3,547 C++ lines** unless sound
reductions offset growth. No retirement or implementation is approved by reviewer agreement.

## Verification and reviewer agreement

Two existing public-boundary checks passed against unchanged source and isolated real MariaDB
with synthetic GitHub:

```sh
python3 tests/pipeline.py --executable "$PWD/build/codex/slim-controller/core-ci" \
  --directory /tmp/codex/build/core-ci/stuck-publication/tests/preflight-known \
  --case preflight-publish-known
python3 tests/pipeline.py --executable "$PWD/build/codex/slim-controller/core-ci" \
  --directory /tmp/codex/build/core-ci/stuck-publication/tests/cold --case cold
```

They confirm preservation of preflight-backed assignments across restart, refusal of retry,
no replacement mutation and recovery when matching evidence exists. They do not validate
any proposed change. The cold synthetic full-chain check reported 611 outgoing requests,
16 token requests and zero requests for its unchanged duplicate burst; these are existing
fixture measurements, not live throughput or new performance acceptance.

Proposed tests: lost POST response with/without a created run; HTTP diagnostic fields;
complete zero-match, incomplete (including a single match), malformed, ambiguous and
inaccessible listings/jobs; per-execution inspection errors; unchanged authenticated facts;
restart without replacement. Retirement needs separate stale-attempt/identity/evidence guards,
acknowledged unknown outcome, preserved history, no automatic retry and late-evidence checks.
No new tests were written in this analysis checkpoint.

Claude and this agent reviewed the source and exchanged corrections until agreement.
Withdrawn claims include proven transport loss, proof no run ever existed, uniqueness from
one match on an incomplete page, preflight proving submission, unnecessary worker atomicity
changes and claims that proposed tests had already been written. No disagreement remains.
Review/evidence files are retained under `/tmp/codex/build/core-ci/stuck-publication/`.
No secrets, production SNodeC resources, keys, existing tags or feed bytes were changed.

## Official API references

- [Workflow dispatch API](https://docs.github.com/en/rest/actions/workflows#create-a-workflow-dispatch-event):
  API version 2026-03-10 documents a 200 response with run ID/URLs; there is no documented
  idempotency key or exactly-once recovery guarantee to rely on here.
- [List workflow runs](https://docs.github.com/en/rest/actions/workflow-runs#list-workflow-runs-for-a-repository):
  pagination and supported filters; search results have resource limits. Missing matches
  are not documented as proof a dispatch was never accepted.
- [Attempt-specific run evidence](https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run-attempt):
  continue reading immutable attempt-one execution evidence.
- [Delete a workflow run](https://docs.github.com/en/rest/actions/workflow-runs#delete-a-workflow-run):
  a run can have deleted history; do not assume from the listing alone that it never existed.

## Implemented checkpoint

The owner instructed implementation and approved revised measured bounds after canonical
formatting exposed the underestimate: 3,572 C++ lines, 15 UI lines, 3 test-registration lines
and 180 test lines. Actual source is exactly 3,572 lines (+62); UI +12, CMake +3; tests +131.
The guards, diagnostics, complete-list validation, inspection results and explicit retirement
capability above are implemented. HttpClient and worker behavior are unchanged.

All nine selected checks pass on the final source: the three new unconfirmed-dispatch,
unconfirmed-run and retirement pipeline cases; cold and recovery; preflight-publish-known
and preflight-publish-absent; graph-state-boundaries and persistence-graph. They use real
isolated MariaDB and synthetic GitHub. Initial fixture failures were a test variable-name
collision and a 108-character MariaDB socket path; both were corrected and the final checks
passed. No test was removed or weakened.

Installed Chrome passes UI retirement visibility, cancellation, acknowledgement text, busy
controls across SSE, exact POST body checked outside the server thread, and the resulting
failed-row retry action. The fixture is committed test source; the browser driver and results
are temporary evidence under `/tmp/codex/build/core-ci/unconfirmed-dispatch-fix/`, not a new
repository browser harness. Build/format-check, JS syntax, Python compilation and diff checks
pass. Claude reviewed the code and corrections and reports no remaining concrete defect or
disagreement. Optional unrelated fixes remain outside this scope.

This is local verification only. r169 remains unchanged on core.vchrist.at; no GitHub work
was triggered or replaced. The source changes are uncommitted and not installed.
