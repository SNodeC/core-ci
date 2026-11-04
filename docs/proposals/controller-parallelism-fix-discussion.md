# Controller parallelism implementation: fix discussion

Read-only discussion with the installed Claude CLI on 2026-10-27. Source remains
`63ab7b2` on `fix/controller-parallelism`. This record is not implementation,
commit/push/merge/install authority, runtime proof or an owner waiver.

All proposal/implementation comparisons concern the current approved scope. The first two
rounds used Read/Grep/Glob only; the final acknowledgment uses no tools. No code, existing
test, workflow, configuration or deployment was changed. Private directories were excluded.

The current dispositions are recorded in [the fix agreement](controller-parallelism-fix-agreement.md).
Earlier positions in this dialogue are superseded where corrected below.

## Round 1: Codex to Claude

````text
The owner requests that we compare every finding in your implementation review with the approved proposal/current implementation, and discuss until we agree on fixes or explicit evidence-gap dispositions. This is DESIGN/REVIEW ONLY: no production/test/workflow/config edits, commands, live GitHub work, commits, deployment or secret access. Read/Grep/Glob only. Return text; I record it. Do not inspect .git, secrets/, .local-credentials/, .qtcreator/, build/, checkouts/, credentials, keys/private files, personal settings/logs. Restrict source searches to explicit src/, tests/, ui/, CMakeLists.txt, deployment/worker source/workflows/templates, the named review/proposal documents, and framework public implementation when necessary.

Exact source remains 63ab7b2 on pushed fix/controller-parallelism. No source/test/worker changes since your review. You can read your completed review at /tmp/codex/build/core-ci/controller-parallelism/claude-implementation-review.md and the independent code validation at /tmp/codex/build/core-ci/parallelism-findings/code-validation.md. Probe results/scripts are in that same findings directory; they do not claim live evidence. Read docs/proposals/controller-parallelism-design.md and controller-parallelism-review-agreement.md to check all invariants. The owner excluded investigating deployment documentation in the last turn; do not reopen a doc audit. M1 can receive its already identified minimal correction as a disposition.

Independent findings/corrections and my proposed fixes:

M1: Accept the already identified wording correction only: first-install instructions must not overwrite retained O workflow paths during upgrade; N tooling commit excludes O wrappers, N wrappers rendered into new aliases in a separate commit. No additional documentation investigation or new rollout architecture.

H1: CONFIRMED by installed-library probe: github-http --disabled=true yielded callback count=0. Framework ClientFlowController.cpp observeConnectEventReceiver only tracks pointers, never terminates on empty; SocketClient final error termination excludes DISABLED. Proposed minimal fix: status callback handles State::DISABLED by recording definite unsubmitted error and calling finish(operation), using existing membership/pump guards. After client.connect returns, the existing missing-membership branch terminates its returned flow. Do NOT finish every non-OK error: intermediate multi-address establishment must still work. No new flags/cancel manager. Disabled fixed/dynamic named clients should be exercised through public boundaries. Shutdown connector cancellation without status is NOT by itself another normal-operation failure; proposal expressly permits undelivered callbacks across process shutdown and forbids callbacks after Pipeline lifetime.

L1: CONFIRMED with checked_push/token/identity mocked: push lacking --feed reaches checked_push with Path.cwd(). Existing installed workflow always passes arguments. Phase validation should precede assignment processing/network/key handling: require feed for prepare/push, require build-result/artifacts for prepare; confirm requires its public hash/revision as now. Push does NOT consume build-result/artifacts, so I disagree with requiring those unused inputs for push. Keep compatibility with callers already passing them, no new argument framework.

L2: Agree std::exchange(operation->callback, nullptr), replacing move at deliver, no new delivered flag. Standard func.wrap.func.con#6 guarantees only valid/unspecified moved source. Current libstdc++16 does empty it, so no observed duplicate runtime claim.

L3: Agree set the existing definitely-unsubmitted shutdown error before finish for queued, unstarted work. submitted=false remains correct. No shutdown callback guarantee or new lifecycle state.

L4 requires precise agreement, not a blanket rewrite:
(a) Body/status differences exist, but ui/app.js displays work-operation bodies and does not use assignment_preserved or no-op execution IDs to authorize actions. No broken consumer found. Nevertheless proposal A explicitly says preserve caller error classification and no-op/report behavior. Recommend restoring identifiers/assignment-preservation context where applicable and preserving the actual prior caller-specific preparation-error classification, without an error taxonomy/new generic mechanism or eager graph mutation. Please trace original/current call sites and recommend the smallest concrete interface/callback change; distinguish genuinely required externally visible preservation from hypothetical stale paths.
(b) A duplicate while DB mutation uncertainty is latched does get internal 503 + unchanged graph SSE. BUT GitHub already got HTTP202 at Pipeline::event:212 before observe; its processing callback :138 logs 503. Your external-503 wording is inaccurate. Do not add a second eager bindRun path, cache/flag or permit producers after uncertainty just to optimize this rare state. I recommend retaining conservative unavailable-authority behavior and explicitly accepting this exceptional SSE repetition; no GitHub evidence/status-dispatch/SQL request is generated. Does any invariant require a smaller correct change instead?
(c) Built-with-capacity now no blockers is correct publication eligibility; beginBuild is called only for waiting. No change.

T1: Real gaps must be separated from defects. Corrections: successful rollback IS exercised: stale restored GraphStore guard fails ROW_COUNT and Database::writeFailed performs client.rollback; test verifies !uncertain and unchanged persisted state; namespace conflict provides another rollback. Failed rollback/START still lack targeted failure injection. Pipeline already has two concurrent terminal reports behind real MariaDB graph_revision lock (pipeline.py:748-765); forced end-to-end built+advance overlap still missing. Existing status delay test exercises same-state commits during async work, but not sustained approval churn.
Additional probes now confirm max-in-flight1 => 12 correlated callbacks/one scoped token/peak1; max2 peak2; 0/17 rejected. Publication0/33 range-rejected; 1/8/32 passed range validation then probe stops for missing DB configuration (not startup proof).
Proposed focused follow-up coverage, not full suite: commit disabled/capacity1/range regressions into existing transport harness; malformed phase arguments; explicit callback-enqueued work and dynamic origin drain; controlled connect/TLS/response shutdown lifetime; delayed artifact result plus another evidence/advance under DB lock; sustained generation commits during approval then latest status dispatched; targeted START and failed-rollback probes with existing real-DB proxy. For multi-address establish/orphan callback: identify a real supported trigger or source-proof limitation; do not introduce production injection machinery or framework changes for an unreachable defensive branch. Please assess smallest meaningful set and identify genuine remaining acceptance gates. No claim all gaps are live-only or accepted by owner.

I1: Unchanged read release-before-callback ordering protects startup teardown. No broken consumer, no fix. Agreed?

Other review hypotheses: shared group strings match in current worker YAML; mixed O workflow-level/N job-level exclusion still requires sandbox proof before capacity>1 during mixed rollout. Status churn and response-to-close occupancy need live timing; not a justification for default latch/debounce/timer. Retained selection guard can reject at higher commit rate; this was explicitly deferred. Locked signing/setup remains approved partial scope. No change to these policies.

For EACH M1,H1,T1,L1,L2,L3,L4(a/b/c),I1 and each hypothesis: AGREE / DISAGREE / QUALIFY; cite proposal and source when disagreement matters. Give concrete corrections, what is deleted/replaced, test boundary, and residual limitations. Resolve the policy/behavior preservation question in L4 explicitly. Do not claim consensus while anything substantive remains open or infer new owner approvals. Do not write implementation or run tools other than read/search.

````

## Round 1: Claude to Codex

I couldn't write a plan file or call ExitPlanMode because only Read, Grep and Glob were available, so here is the text for you to record. No files were edited and nothing was run.

**Summary.** I agree with M1, L2, L3, L4(c), I1 and every hypothesis. I agree with H1, L1, L4(a), L4(b) and T1 with qualifications. Two of your points need correcting, and so do several of my own review claims. Open items remain (end of this reply), so there is no consensus yet.

---

## M1: AGREE
Use the minimal wording correction only:
- The general install paragraph applies to a first install only.
- The upgrade's N tooling commit must leave O's deployed publisher and status workflow paths byte-identical.
- Rendered N templates go to the new alias paths in a separate commit.

No further documentation investigation and no new rollout design. My line numbers came from the uncommitted working tree, so the correction goes into those edited files.

## H1: AGREE with the fix, QUALIFY one part of your mechanism
**Confirmed path.**
- `SocketConnector.hpp:244-255` reports DISABLED and then destroys the connector.
- The wrapper at `SocketClient.h:235-280` passes DISABLED on unchanged. It never terminates the flow, because `:277` covers only ERROR and FATAL.
- Your probe (callback count 0) matches this.

**Your mechanism is wrong on timing.** `FlowController::startFlow` (`FlowController.hpp:172-177`) defers the connect to `atNextTick`. So the DISABLED status arrives on a later tick, after `client.connect` has returned. By then the operation is still a member, and `operation->flow` and its termination hook are already installed (`HttpClient.cpp:167-173`).

The missing-membership branch at `:177-178` is therefore not what cleans up. The cleanup is `finish(operation)` called from the status callback:
- It erases the operation, delivers once and pumps.
- The flow is never terminated. Its later hook would be a no-op anyway, because `finish` checks membership (`:199-200`).
- The framework closures are released when the connector is destroyed.

None of this changes your fix; it only changes the reasoning for why it is complete.

**Correction (in `HttpClient.cpp:159-166` only).**
- Add a DISABLED branch to the status callback: set the error to "HTTPS client instance is disabled; request was not submitted" (`submitted` stays false), then call `finish(operation)`.
- Leave ERROR and FATAL as they are now: record the error and finish when the flow terminates. Intermediate address failures keep working.
- Optionally drop "or is disabled" from the remaining error text.
- Nothing is deleted. No flags or cancel manager.
- Reentrancy is safe: this runs outside `pump`, and both `finish`→`pump` and a callback that enqueues work are covered by the `pumping` guard.

**Shutdown: AGREE it is not a normal-operation defect.**
- The proposal (`design.md:270-277`) explicitly allows cancellation without a status during shutdown. The same applies to `SocketClient.h:196`, which creates no connector unless the state is RUNNING or INITIALIZED.
- `:287-288` ("required") is a configuration path, not reachable at runtime.
- I withdraw the "connector cancelled without status" trigger from my review.

**Test boundary.**
- Fixed client (required): add a `disabled` case to `tests/github_transport.py`. Assert exactly one callback, status 0, nothing seen at the server (no token request), and process exit 0.
- Dynamic `feed-http`: the DISABLED branch does not depend on `fixedPeer`. If `tests/pipeline.py` can rerun its existing publication-verification step with `feed-http --disabled=true`, assert 202 with `assignment_preserved`, the node still publishing, and a second reconcile proceeding (proving `completionReads` was released). Otherwise record it as source-proven on the shared branch.

**Residual.** The design row says "synchronous disabled connect", but the installed framework delivers DISABLED asynchronously. The test should pin the actual behaviour.

## L1: AGREE with your narrowing
- Push uses only `feed` (`publish.py:256-258`), so I withdraw my request to require `--build-result` and `--artifacts` for push.
- Correction: immediately after `parse_args` (`:183`), before `assignment()`, identity, keys or network:
  - require `--feed` for prepare and push;
  - require `--build-result` and `--artifacts` for prepare.
- Confirm keeps its current revision check.
- Callers that pass extra arguments stay compatible.
- Test boundary: malformed prepare and push invocations in `tests/worker_provenance.py`, failing before any mocked identity, token or push call.

## L2: AGREE
- Replace `std::move(operation->callback)` with `std::exchange(operation->callback, nullptr)` at `HttpClient.cpp:193`. `<utility>` is already included.
- This is exactly the proposal's "an empty callback means delivery already occurred, so no new delivered flag" (`design.md:233-234`).
- No new test: the duplicate is not observable on libstdc++. The existing tests remain the single-delivery boundary.

## L3: AGREE
- At `HttpClient.cpp:222-223`, set "HTTPS request was not submitted during shutdown" (the same text as `:130`) before `finish`.
- `submitted=false` is unchanged. No lifecycle state, no shutdown guarantee.
- Covered only if the shutdown test (T1) is added; otherwise it is a source-proven text fix.

## L4(a): QUALIFY, with the call-site trace
Tracing the base code in `review.diff` against the current code shows where the original classification was actually visible:

| Site | Base preparation failure | Current | Action |
|---|---|---|---|
| `commitSelection`, `retry` | 409 `{error}` | 409 `{error, uncertain:false}` | keep (status preserved) |
| `advance` | 422 | 422 | keep |
| `rejectedDispatch` ×2 (`:442`, `:465`) | 409 `{error}`, outer try | 409 | keep |
| `record` (`:636`) | 409 `{error, assignment_preserved}` via the `observe` apply catch (`:601-602`) and the stale retry (base `:476`) | 409 without the flag | **restore** the flag |
| `buildArtifacts` failed/built (`:752`, `:776`) | 503 `{error, execution, assignment_preserved}` via the catch at `:788-789` | 409 | **restore** 503 shape |
| `publicationResult` published (`:814`) | 409 `{error, assignment_preserved}` via the catch at `:827-828` | 409 without the flag | **restore** the flag |
| `complete` failed (`:664`), beforePush failed (`:802`) | Not reachable: `complete` checks attempt and state synchronously just before (`:659-661`) | 409 | keep the default; there is no prior classification to preserve |

**Where this is visible externally.**
- These responses never reach an HTTP client as a status code. Webhooks were already acknowledged with 202 (`:168`, `:212`) and only logged (`:139-141`).
- `reconcile` keeps only the bodies in `reconciled` (`:899-900`).
- So the externally visible part is the body fields in the reconcile response, which the UI displays, and the log lines.
- The obligation comes from the proposal (`design.md:77-81`, `:124`: "caller receives original error classification"), not from a broken consumer.

**Smallest change.**
- Give `Pipeline::change` a third parameter: the preparation-failure response, defaulting to `{409, {}}`. On `preparationFailed`, copy that body, add `error`, and use its status.
- The database-failure branch keeps 503 `{error, uncertain}`.
- `buildArtifacts` passes `{503, {execution: node.id, assignment_preserved: true}}`; the published site passes `{409, {assignment_preserved: true}}`.
- `record`'s own preparation branch returns 409 `{error, assignment_preserved: true}`.
- This replaces the two `preparationFailed ? 409 : 503` ternaries in `change` and `record`. No taxonomy and no eager derivation.
- I rejected sniffing the status inside caller callbacks as too fragile.

**Execution ID: I withdraw my claim.**
- `record`'s no-op already returns the execution ID (`:646`, the same as base `:464`).
- Base `change()` had no no-op path. A comparable duplicate terminal result went through the stale retry and returned `advance()`'s body, which has no execution ID either.
- No change needed.

**Test.** Optional. A real race exists (two runs claiming one attempt, which conflicts in `bindRun` at the head of the queue), but it can only be forced with the held-lock mechanism in `pipeline.py`. Otherwise this is a source-reviewed restoration.

## L4(b): AGREE to keep the current behaviour, with your wording correction
- I accept the correction: GitHub has already received 202 by then. The 503 is internal and logged.
- No invariant calls for a smaller change. The proposal says queued writers must not invoke producers while the uncertainty latch is set (`design.md:130`). An earlier no-op pre-check would bring back eager derivation, which the proposal replaces rather than supplements (`:62`).
- The SSE on a latched rejection follows the existing rule "SSE reports uncertainty on failed mutations" (`:162-163`), because the base also notified on latched rejections.

**One more visible difference to record and accept.** `advance` with nothing to do while latched now returns 503 `{uncertain:true}`. Base returned 200 `{assigned:0}`. This affects `/reconcile` responses, both the UI POST and the startup reconcile. It is conservative and more truthful. I recommend accepting the repeated SSE and this 503 explicitly. No GitHub, dispatch or SQL traffic is generated.

## L4(c): AGREE
No change.

## I1: AGREE
- The order is unchanged from the base. It is required for startup teardown (`design.md:105-108`).
- No consumer depends on the read callback running first. No fix.

## T1: QUALIFY
**Corrections I accept.**
- A successful rollback is exercised (`GraphStoreTest.cpp:173-183` and the namespace conflict).
- `pipeline.py:748-765` covers concurrent reports behind the lock.

**A correction to my own review.** A request enqueued from a callback is already covered:
- The token callback in `GitHub::authorize` (`:287-291`) calls `send`→`http.request` from inside `HttpClient` delivery, while the token operation still holds its slot.
- The parallel test (12 waiters) and the capacity-1 probe (12 callbacks, peak 1) exercise exactly this.
- No new test needed.

**Proposed acceptance gates (focused, not the full suite).**
1. `github_transport.py`: the disabled case (H1), max-in-flight 1 (12 requests, 1 token, peak 1) and rejection of 0/17. These probes already exist.
2. `lifecycle.py`: publication-limit 0/33 rejected. `pipeline.py`: one phase at `--publication-limit 1` showing the second publisher blocked by the capacity blocker. This is a gate because limit 1 is the mixed-rollout setting.
3. `worker_provenance.py`: malformed phase arguments (L1).
4. `pipeline.py`: commits keep arriving while an approval is delayed. Assert the POST for the captured generation, then a POST for the latest generation, and never the same generation twice. This is design row `:560` and is the N1 motivation.
5. Shutdown during a stalled TLS handshake and a stalled response (`pipeline.py` restart harness):
   - assert a clean exit, no crash signal, and no resend after restart;
   - the design explicitly requires this test (`:277`);
   - residual: proving no use-after-free needs a sanitizer build, and holding a connect phase open depends on the environment.

**Recommended as source-proven limitations, needing owner disposition.**
- **Dynamic origin drain with active connections:** unreachable. `feed-http` is used only by `FeedProof`, which requires the configured origin (`FeedProof.cpp:36`). The single first switch (at `active==0`) is already exercised by `pipeline.py`.
- **Orphan onConnect:** unreachable given the verified adjacency of the status call and `onConnect` (`SocketConnector.hpp:218-222`). No injection machinery.
- **Multi-address establishment:** this has a real trigger: the multi-address Pages and dual-stack hosts.
  - Cheapest test: `github_transport.py` with `remote --host localhost` and the server bound only to the second `getaddrinfo` result. Certificate verification uses the fixed peer name, so it is unaffected.
  - Report an explicit skip if `localhost` resolves to a single address.
  - I recommend this as a gate if the harness supports it.
- **START failure / failed rollback:** the diff shows only the shared `finish` ordering changed, and that ordering is exercised by other paths; the classification triples are unchanged from the base. Design row `:559` lists them, so a waiver needs the owner. Extending the existing proxy is optional.
- **Delayed artifact plus advance under the lock:** not a gate. Ordering is covered at the design-listed GraphStore boundary.

## Hypotheses: AGREE with all
- Group strings match.
- The mixed O/N lock behaviour needs sandbox proof before capacity above 1 while O workflows can run.
- Status churn and slot occupancy need live timing; no latch, debounce or timer.
- Retained selection-guard rejections were explicitly deferred.
- Signing and setup under the lock is the approved partial scope.

## Open before consensus
1. Your confirmation of the H1 timing correction.
2. Agreement on the L4(a) third parameter and the per-site table, including keeping the default for `complete` and beforePush.
3. Owner disposition for the source-proven T1 items and the gate list. I have not assumed any owner approval.
4. The sandbox evidence listed in my review is still outstanding.

## Round 2: Codex to Claude

````text
Continue our read-only fix agreement discussion. Same restrictions as round 1: Read/Grep/Glob only, no source/test/workflow/config edits or commands, no secrets/private dirs/settings/logs or deployment. Exact implementation remains 63ab7b2. You can read the prior request/response and code-validation.md in /tmp/codex/build/core-ci/parallelism-findings/. Read only named review/design/source paths, not a broad repository scan. The owner asks us to resolve all fix disagreements; do not invent owner waivers.

I have read your full round-1 reply. Here is my response and proposed final specification:

1. M1: agreed exactly as you list; no extra documentation audit. O wrappers stay byte-identical in N tooling commit; new rendered aliases in a separate wrapper commit; first-install scope wording corrected when implementation is authorized.

2. H1: I ACCEPT your timing correction. I read FlowController.hpp::startFlow and SocketClient::realConnect. Connect setup is deferred by the framework, so DISABLED is normally delivered after our flow handle/hook registration. I withdraw my claim that the missing-membership-after-connect branch cleans that real disabled path. finish(operation) releases the application operation and slot; connector teardown releases its framework captures; no active transport exists. We do not need a new explicit cancel/termination manager. Implement precisely the DISABLED branch/message/finish in the existing status callback; retain ERROR/FATAL flow handling and strip the inaccurate 'or is disabled' text. Source-proof plus installed callback-0 reproduction establishes the defect. No promise of callback delivery across process shutdown.

3. L1: agreed. Immediately after parse_args require feed for prepare/push, result/artifacts for prepare; leave unused push extras accepted. Public confirm hash/revision validation remains before their use. No new parser abstraction.

4. L2: agreed std::exchange replaces move, no extra flag or implementation-mirroring test. Existing HTTP+transport shutdown single-delivery assertions remain.

5. L3: agreed exact existing unsubmitted-shutdown message before finish for queued/unstarted work; do not relabel submitted requests.

6. L4(a): I ACCEPT the per-site classification table and third-parameter proposal, with these concrete constraints:
   - Existing PipelineResult is the response value; no new error type, enum, taxonomy or workflow mechanism.
   - Pipeline::change(derive, callback, preparationFailure={409,{}}). The asynchronous callback MUST capture preparationFailure by value, never retain the caller's reference/default temporary.
   - On preparationFailed copy that body, add the actual error, use its given status. SQL failures stay 503 {error,uncertain}. No eager derive or stale Node retry.
   - buildArtifacts failed/built sites provide 503 with execution ID and assignment_preserved:true; published result site provides 409 with assignment_preserved:true. Other callers retain default409.
   - record's preparation failure restores 409 assignment_preserved:true; SQL error remains503 withuncertain. No new Graph authority/state.
   - No change to change() no-op response: I accept your correction that the base had no comparable identifier guarantee, whereas record's unchanged path already retains its ID.
   - While implementing, remove truly obsolete surrounding try/catch blocks whose only formerly throwing operation was the now-lazy Graph derivation (e.g. rejectedDispatch branches). Retain catches doing real synchronous JSON/identity/current-attempt checks. This is reduction, not whole-file style rewrite.
   - Meaningful public-boundary regression if feasible: hold graph_revision lock while two authenticated run claims for the same UNBOUND assignment are accepted/queued; release and assert one binds, the other reports409 withpreservation context, no overwrite/replacement. Do not add production injection hooks for unreachable races. Other response sites may be source-reviewed restoration if no legitimate public trigger reaches their preparation failure.

7. L4(b): agreed to KEEP conservative DB-authority rejection and failed-mutation SSE. Do not add eager no-op check, new flag/cache or permit producers while latched. Correct report wording to distinguish initial202 webhook ACK from logged internal503. I accept your additional source observation that a no-work advance/reconcile under latch now surfaces503 where the prior early no-op could say200. It must retain truthful uncertain:true and no SQL/dispatch/evidence traffic. No blanket new owner-policy request is justified for refusing processing while authority is unavailable; this follows the agreed stop-before-derivation invariant. However we must explicitly record this observable difference and not claim every error surface is byte-identical. We cannot claim owner waiver of anything.

8. L4(c), I1: agree no changes.

9. T1: I accept your correction that callback-enqueued HTTP work is ALREADY covered by token completion -> send, notably capacity1/12requests. No duplicate test just for that phrase. I agree current FeedProof::verify requires selected.origin==configured.origin; config is fixed for a process. So active dynamic-origin switching cannot be driven by current product paths (historical mismatched origins are rejected before HTTP). Initial origin setup is exercised. A late orphan onConnect is likewise a defensive path excluded by framework status/onConnect adjacency. We should record these precise source limitations; no special injection machinery, no framework change, and no claim they were runtime-tested. This is not dropping supported product behavior or inventing an owner waiver. If future real origin switching becomes product behavior it requires its own coverage.

For genuinely reachable failure/ordering gaps, do NOT ask the owner to waive tests. Keep a focused implementation verification plan within the already approved test budget:
   A. Commit disabled-client, capacity1 and invalid max0/17 into existing transport harness. Fixed client callbackonce/status0/submittedfalse/server seesno token. Exercise dynamic feed disabled with the existing publication/reconcile fixture if possible and show preserved assignment + completionReads released; otherwise record shared-branch source proof honestly.
   B. Publication-limit invalid0/33 parser test; pipeline limit1 case verifies second built node visibly capacity-blocked and admitted once prior definite terminal result frees capacity. Default8 and lower-limit restore tests remain.
   C. Malformed prepare/push arguments rejected before assignment/identity/secrets/network, in worker_provenance.py.
   D. Controlled generation commits during delayed approval: demonstrate a POST proceeds while commits continue, and latest minimum is eventually sent after settling, with no duplicate unchanged-generation POST. No assertion that callback order dictates GitHub job arrival/Pages timing.
   E. Shutdown while TLS/response stalled: clean exit/no crash and restart restoration without replacement mutation. Existing general connect-failure/timeouts plus source lifecycle tracing remain; a connect-in-progress shutdown case should be exercised if the local fixture can actually hold it. No fake guarantee that every shutdown callback fires. An instrumented focused build may strengthen memory evidence if needed, never claim noninstrumented exit proves absence of UAF.
   F. Multi-address establish: your localhost idea is promising BUT getaddrinfo selection might return IPv4 only for the concrete `tls::in` address type; verify the actual resolver/family before assuming dual-stack. Use a real controllable supported address list/second-address listener if available; report explicit environment limitation rather than invent evidence or change DNS/global hosts/framework. No catalogue/refactor.
   G. START failure and rollback failure: instead of owner-waiver, target them using the existing DatabaseProxy in enrollment.py/GraphStore boundary. A controlled proxy can cause the real backend to execute an invalid query for the targeted START/ROLLBACK, relay its real error and verify no application/producer after uncertainty. Connection teardown rolls back the deliberately unclosed test transaction. Or use a smaller existing public-boundary technique you recommend. No synthetic records claimed live, no new production mechanism. Successful rollback/COMMIT loss coverage stays.
   H. Delayed artifact + another advance: GraphStore boundary already covers accepted facts/order; existing pipeline concurrent-report test stays. Add a forced public overlap if the artifact fixture can control the real callback ordering cheaply, but do not force an unreachable bad-result race just to mirror internal code. No blanket claim end-to-end forced coverage until measured.

10. All prior hypotheses remain as your accepted list: mixed workflow/job feed mutex needs sandbox proof before mixed capacity>1; live status churn/response-close occupancy measured separately; selection guards deliberately retained; privileged signing/setup split remains out of scope. Those are acceptance gates/limits, not pre-approved architecture changes. No actual activation in this discussion.

Please respond with:
- Any remaining substantive disagreement with items1-10; exact correction/reason.
- In particular check L4 third-parameter lifetime, exceptional no-work503 semantics, T1 source-unreachable dispositions versus owner waivers, and the smallest feasible failure-proxy/multi-address test boundaries.
- If you agree, explicitly state whether all FIX DIRECTION disagreements are resolved. Keep unresolved runtime evidence/test feasibility/owner authority separate; no claim deployment or full acceptance.
- No new production mechanism may be added merely to cover a test branch. We will record the agreed correction plan before implementing.

````

## Round 2: Claude to Codex

I couldn't write the plan file or call ExitPlanMode: only Read, Grep and Glob are available in this session, which also matches your restrictions. Nothing was edited or run. Below is the round-2 text for you to record.

I checked these files for this round: `src/Pipeline.cpp`, `src/Pipeline.h`, `src/Database.cpp`, `src/GraphStore.cpp`, `tests/enrollment.py`, `docs/proposals/controller-parallelism-design.md:555-565`, and snode.c's `src/net/in/SocketAddress.cpp`.

## Items 1–10

**Items 1–5 and 8: agree, no corrections.**

**Item 6, L4(a): agree, with three clarifications.**

1. **Lifetime.**
   - Capturing `[this, callback, preparationFailure]` by value is correct and matches the existing pattern: `derive` and `callback` are already copied (`Pipeline.cpp:920,923`).
   - `PipelineResult{409, {}}` gives a **null** body (`Pipeline.h:13`). Use `body["error"] = …`, which turns null into an object. Don't use calls that need an object.
2. **The `uncertain` field.** Today every caller on the default path returns `{error, uncertain:false}` (`:925`, `:644`).
   - If the third parameter replaces the body, those callers quietly lose `uncertain`.
   - Recommendation: on preparation failure, also set `uncertain` to `result.uncertain`. That value is always false here, because `Database.cpp:115` sets `{…, false, true}`. Adding it to the restored shapes is harmless.
   - Either choice works, but decide it explicitly; it doesn't block the fix direction.
3. **The regression test needs something that tells the paths apart.** The `observe` catches at `:602` and `:624` already return 409 with `assignment_preserved:true`.
   - A test that only checks "409 + preserved" would pass even if a synchronous `observe` rejection fired instead of the `bindRun` preparation failure.
   - So the test must also assert that the error text is `bindRun`'s conflict message, that the first `build_run` ID is unchanged, and that no second dispatch happened.
   - Webhook outcomes are only ACKed with 202 and then logged, so the assertion has to read the logged internal result plus `/api/graph`.

**try/catch cleanup:**
- **Removable:** the blocks at `:441-449` and `:461-473`. After the change, only `std::string` building or `bad_alloc` is left inside them; `derive` exceptions are already caught at `Database.cpp:114`.
- **Keep:** `:601-602`, `:623-624`, `:788-793` and `:827-828`, because they wrap real `require` or current-attempt checks.
- At the built site, the wrapper callback (`:780-786`) passes non-200 responses through unchanged, so the 503 restoration works there.

**Item 7, no-work 503: agree. The source confirms the mechanism, but the recorded scope may need to be wider than `advance`.**
- `Database.cpp:109-110,118-119` checks the latch before `prepare()`. So `derive` never runs, the result is `uncertain:true, preparationFailed:false`, the caller returns 503 `{error, uncertain:true}`, and `GraphStore.cpp:291` sends the SSE. There is no SQL or dispatch traffic.
- This applies to every `store.commit` caller while latched, not just `advance`. That includes `record`'s unchanged path, which returns 200 with the execution ID (`:646`) when not latched.
- When you write the record, check `review.diff` for whether base returned 200 on that path while latched. If it did, word the difference as "no-op commit paths, notably reconcile/advance and duplicate reports". Don't limit it to `advance`.

**Item 9, T1: agree on substance (no injection, no framework change), with one wording correction.**
- Design row 561 explicitly lists "multi-address establishment and late orphan callback", "origin drain" and "synchronous disabled connect" as **required checks**.
- Calling the dynamic-origin and orphan-onConnect cases source-unreachable is a fair factual disposition, not a waiver. But the record must say "design-listed check not run; reason: no product path reaches it; not runtime-tested". It must not count them as covered.
- "Synchronous disabled connect" is wrong for the installed framework, which delivers DISABLED asynchronously. Record that as a design-text correction.
- This belongs on the owner/acceptance list, not the fix-direction list.

**Plan items A–E and H: agree.**

**Plan item G, test boundary: feasible as a test-only change, with two corrections.**
- **How:** the proxy already parses client COM_QUERY packets (`enrollment.py:59-61`). Add a one-shot rewrite of a matching `b"\x03START TRANSACTION"` or `b"\x03ROLLBACK"` packet into invalid SQL, recomputing the 3-byte length and keeping the sequence byte. Assert the match counter is 1, so the test fails loudly if `client.rollback`'s wire text isn't `ROLLBACK`. COMMIT matching already proves the pattern only for COMMIT.
- **START failure is not uncertainty.** `Database.cpp:126-127` returns `uncertain:false` with no latch.
  - Assert: 503 with `uncertain` false, `github_committed:true`, revision unchanged, `mutations_allowed` still true, and the next save succeeds.
  - "No producer after uncertainty" doesn't apply here. No transaction was opened, so nothing needs tearing down.
- **ROLLBACK failure:**
  - Trigger it with the existing real statement failure, the REVOKE INSERT step (`:257-262`). That already reaches `writeFailed`, then rollback, because the error code isn't server-gone.
  - Inject on the ROLLBACK. The result is `mutationUncertain` (`Database.cpp:176-178`): 503 with `uncertain`, `mutations_allowed:false`, and no further GitHub write (same check as `:282-284`).
- **Caveat:** an injected syntax error doesn't end the transaction. That connection keeps an open transaction and its row locks, which a realistic rollback failure on a lost connection wouldn't. So:
  - don't assert on application reads between the injection and the restart;
  - check from the separate root connection, which does non-locking reads;
  - then stop the controller (the disconnect rolls back), restore GRANT, restart, and assert no revision leaked.
- This exercises the shared `Database::write` path that GraphStore also uses. That's enough, because the outcome classifications are defined in `Database.cpp`.

**Plan item F, multi-address: I withdraw my localhost idea.**
- `net::in::SocketAddress` is `AF_INET` (`SocketAddress.cpp:56-57`), and its resolver hint is `ai_family = AF_INET` (`:113`). So `::1` is never a candidate, and localhost normally resolves to a single address.
- The only non-global option I see:
  - run the controller in an unprivileged user and mount namespace;
  - bind-mount a private hosts file mapping a test name to `127.0.0.2` (nothing listening, so refused) and `127.0.0.1` (the listener);
  - read the order from `getaddrinfo` inside that same namespace, because RFC 6724 sorting may reorder the addresses.
- That is harness environment, not product machinery. But it is optional and depends on the host. If user namespaces aren't available, record it as an environment limitation.

**Item 10: agree.**

## Status

**All fix-direction disagreements are resolved**, provided the clarifications above are recorded:
- the `uncertain`-field decision;
- the regression-test check on `bindRun`'s message;
- START is not latched;
- the wider no-op-503 wording.

None of them changes a fix's direction.

**Still open, kept separate:**
- **Runtime and test feasibility:**
  - holding a connect phase open (E);
  - the namespace multi-address test (F);
  - the ROLLBACK wire text (G);
  - feed-http with `--disabled=true` in the publication fixture (A);
  - a cheap forced artifact overlap (H);
  - instrumented memory evidence (E).
- **Owner authority:**
  - the design-row-561 checks that weren't run, and the "synchronous disabled" text correction;
  - acknowledging the recorded latched-503 difference;
  - the performance thresholds;
  - the mixed O/N capacity gate and the other item-10 gates.
- **Not claimed:** deployment, sandbox evidence or full acceptance. Nothing has been implemented or run.

## Final clarification: Codex to Claude

````text
Final acknowledgment from Codex of your round-2 fix discussion. No tools, edits, implementation or new investigation requested. This is only confirmation that your stated conditions were accepted. Your round-2 response follows after this note for exact context.

I accept all round-2 clarifications. Decisions:
1. Preparation failures retain uncertain=result.uncertain (false) in the restored bodies. PipelineResult preparationFailure is captured by VALUE in the asynchronous callback; body["error"] handles a null default. No new error taxonomy.
2. The held-lock run-claim regression must distinguish lazy bindRun conflict from earlier observe rejection: assert bindRun's specific message, first run ID retained, no replacement/second dispatch, and inspect logged internal outcome versus external202 separately.
3. START failure stays definite/unlatched; verify the next write succeeds. ROLLBACK failure latches uncertainty, no later producers/mutations. Match counters prove the actual wire command was intercepted. Close/restart releases the deliberately open test transaction before further DB-dependent checks. GraphStore::view's existing in-memory mutations_allowed check is safe to inspect; do not rely on DB-backed application reads while the injected transaction remains open.
4. Record the unavailable-authority response difference for NO-OP COMMIT PATHS, notably reconcile/advance and unchanged reports, not just advance. Retain the agreed stop-before-derivation invariant and truthful uncertain503. Authenticated webhook ACK remains202; processing503 is logged. Repeated SSE in that exceptional state is retained without a cache/flag or eager bypass. We have not claimed owner waiver or byte-identical responses.
5. Source-unreachable active-origin/orphan checks are recorded explicitly as design-listed checks NOT RUN, with source reason, never counted as runtime coverage. DISABLED is asynchronous in the actual framework. Reachable test/failure gaps have focused coverage planned, not waived. IPv4-only resolver means your withdrawn dual-stack-localhost test is not used. Optional isolated-namespace feasibility remains unverified; no global host/DNS/framework changes.
6. All previously agreed small fixes/dispositions remain unchanged: M1 minimal wording; H1 finish unsubmitted DISABLED; L1 validate actual phase inputs; L2 exchange callback; L3 unsubmitted shutdown wording; L4 per-site preparation response preservation, no invented no-op ID contract, no change to correct built blockers; I1 ordering unchanged. Remove obsolete eager-era catches, retain actual synchronous validation catches.
7. Runtime/test feasibility, live mixed O/N lock/capacity proof, timing thresholds and any owner acceptance remain separate. No new policy/mechanism, blanket deployment blocker, implementation or acceptance claim. Existing bounded implementation authorization is not new authorization from reviewer agreement. Current task is discussion only.

I have no remaining substantive disagreement about any fix direction. Please confirm concisely whether this satisfies your round-2 conditions and whether you have any remaining FIX disagreement (do not reopen runtime acceptance as a fix disagreement).

````

## Final acknowledgment: Claude to Codex

**Yes, this meets my round-2 conditions. I have no remaining fix-direction disagreement.** I used no tools and didn't write a plan file.

Each round-2 condition maps to one of your decisions:

- **`uncertain` field:** decided explicitly in item 1. It is `result.uncertain` (false), the preparation failure is captured by value, and `body["error"]` is safe on a null body.
- **Telling bindRun apart from observe:** covered in item 2. The test asserts the bindRun message, that the first run ID is kept and that there's no second dispatch. It checks the logged internal outcome separately from the external 202.
- **START vs ROLLBACK:** covered in item 3. START fails definitely, isn't latched, and the next write succeeds. ROLLBACK latches, and no producer or mutation follows. The match counter proves the injected wire text was actually hit.
- **Wider no-op-503 wording:** covered in item 4. It says "no-op commit paths, notably reconcile/advance and unchanged reports". The stop-before-derivation invariant is kept, and the webhook 202 and SSE behaviour are recorded as they are.
- **Row-561 checks not run, and the asynchronous DISABLED correction:** covered in item 5. They stay off the coverage count.

Two notes on item 3. Neither is a disagreement.

1. **The `GraphStore::view` claim:** I haven't re-verified this round that `view` reads `mutations_allowed` only from memory, with no DB access. If it does touch the DB, the separate root connection is the fallback I already proposed.
2. **Post-restart check:** after close/restart, keep the explicit assertion that no revision leaked. Your "further DB-dependent checks" wording implies it, but it should be a named assertion.

Runtime and test feasibility, live mixed O/N proof, timing thresholds and all owner acceptance items stay separate and open. Nothing has been implemented, run or accepted.

## Final source clarification from Codex

GraphStore::view builds its JSON from the graph in memory and calls Database::mutationsAllowed
(GraphStore.cpp:351); mutationsAllowed returns the existing boolean (Database.cpp:14–16),
without a query. It is safe for that narrow uncertainty check during the failed-rollback fixture.
DB-dependent application reads remain deferred until cleanup. The planned post-restart test
explicitly asserts no revision leaked. These notes do not change the agreed fixes.
