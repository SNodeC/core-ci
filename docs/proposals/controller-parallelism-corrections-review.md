# Independent re-review: controller-parallelism corrections (`a28f61c`)

**Branch:** `fix/controller-parallelism`, correction commit `a28f61c`, over the reviewed implementation `63ab7b2` and approved base `4257cb0`.

**What I did.** I used Read, Grep and Glob only. I read AGENTS.md first. Then I read, in this order: the fix agreement, the corrections results, the design, the review agreement, the prior implementation review and the fix discussion. After that I read `corrections.diff` and `cumulative.diff`, the current affected source and tests, the ten named evidence files, and the public SNode.C headers needed to check framework claims (`SocketConnector.hpp`, `SocketClient.h`, `FlowController.hpp`).

**What I did not do.** I ran nothing and edited nothing. I did not touch `.git`, `secrets/`, credentials, `build/` or `checkouts/`. Because I could not read `.git`, I could not independently confirm that the committed blobs are identical to the hashed build inputs (`364d21fa…`). That identity rests on your statement and `build-identity.json`.

---

## 1. Bottom line

**No confirmed production defects remain** in `a28f61c` against the fix agreement, the approved proposal or AGENTS.md. Each agreed correction is implemented as specified:
- **M1** wording corrected.
- **H1** asynchronous DISABLED now finishes once.
- **L1** phase arguments checked before any assignment or network work.
- **L2** callback cleared with `std::exchange`.
- **L3** unsubmitted shutdown message set.
- **L4a** per-site responses restored, with the failure response captured by value.

No new mechanism was added. The L4b, L4c and I1 "no change" dispositions are still correct in current source.

**Five test-quality findings, all Low, none a code defect.** Two of them make two results-record claims stronger than the tests prove:
- **TQ1:** the disabled-feed test would pass even if the disable setting were ignored.
- **TQ2:** the artifact-overlap test never reaches the `completionReads` coalescing branch.
- **TQ3:** one old capacity assertion was reshaped without the change being recorded.
- **TQ4:** the shutdown test pins an incidental error string.
- **TQ5:** the switch to a threaded HTTP fixture adds a small race window.

**Positive finding.** The binding-race test is stronger than the results record says. It does tell the lazy failure apart from an earlier `observe` rejection (§4.2).

---

## 2. Disposition of every agreed finding

| Finding | Agreed correction | Current source | Test evidence | Disposition |
|---|---|---|---|---|
| **M1** | Qualify first-install wording. The N tooling commit leaves O wrappers byte-identical. N goes only into new aliases, in a separate commit. | Diff hunk: README step "For a first installation…" and step 3 "leaving O's deployed publisher and status workflow paths byte-identical. Exclude their replacement templates…". | Wording check only, as instructed. | **Closed.** I checked this through the diff hunk only. Whether the surrounding upgrade section is all inside the committed blob or partly in the dirty tree needs a `git show` I was not allowed to run. |
| **H1** | DISABLED gives a definite unsubmitted message and `finish(operation)`. ERROR/FATAL establishment handling stays. | `src/HttpClient.cpp:163-165`. ERROR/FATAL still only record the error (`:166-168`). Termination finishes only while `connectionId==0` (`:172-176`). | `github-transport-disabled` passed: one callback, only after `request()` returned (`returned` flag), the DISABLED message, no server traffic. | **Closed. Exact local evidence** (§3.1). |
| **L1** | Before assignment, identity or network: prepare/push need `--feed`; prepare needs `--build-result` and `--artifacts`. Push must not require unused inputs. | `deployment/worker/core_ci/publish.py:184-188`, before `assignment()` at `:190`. Workflow callers pass these arguments (`core-ci-publish.yml:128-148`). | `worker_provenance.py`: five malformed invocations; mocked `assignment`, `run_identity`, `writer_token` and `checked_push` are never called. | **Closed. Exact local evidence.** |
| **L2** | `std::exchange(callback, nullptr)`. No new flag, no mirror test. | `src/HttpClient.cpp:196`. | Existing callback-count tests only, as agreed. | **Closed. Source disposition.** |
| **L3** | Queued unstarted work gets the existing unsubmitted-shutdown message. Submitted work is not relabelled. | `src/HttpClient.cpp:225-227`. Submitted operations are untouched. | `github-transport-shutdown`: 11 queued calls get the exact unsubmitted text with `submitted=false`; 1 submitted call has `status 0` and `submitted=true`. | **Closed. Exact local evidence** (see TQ4 on the pinned text). |
| **L4a** | `change(derive, callback, preparationFailure={409,{}})`, captured by value. The listed sites are restored and the obsolete dispatch catches removed. | `Pipeline.h:90-92`. `Pipeline.cpp:921-926` captures `preparationFailure` by value and uses `body["error"]` (a null body becomes an object). Sites: record `:635-636`; missing artifacts `:746-751`; built acceptance `:771-782` (wrapper passes non-200 through at `:778-779`); published `:810-815`. Defaults kept for rejectedDispatch `:441-445,460-465`, complete-failed `:658-663` and before-push `:798-803`. commitSelection, retry and advance are unchanged (409/409/422). | Binding race: 409 with `assignment_preserved:true` and `uncertain:false`. Artifact 503 checks the execution ID and `assignment_preserved`. | **Closed.** The remaining prep-failure sites were source-reviewed only (they were not run). |
| **L4b** | Keep the internal 503 with `uncertain:true` and failed-mutation SSE. Do not add an eager bypass. | `Database.cpp:109-110` latches before `prepare()`. `GraphStore.cpp:291` notifies SSE on non-preparation errors. `Pipeline.cpp:212` acknowledges 202 before `observe`. | Not exercised end-to-end, as the record says. | **Closed by source.** One wording point below. |
| **L4c** | No change. | `Graph.cpp:211-216` treats built nodes as publication work. `Pipeline.cpp:274-277` calls `beginBuild` only for `waiting` nodes. | Pipeline at limit 1 shows a visible "capacity" blocker. | **Closed.** |
| **T1** | Focused reachable tests. Keep unrun defensive checks separate. | — | See §4. | **Mostly closed.** TQ1–TQ5 are follow-ups. |
| **I1** | No change. | `Database.cpp:64-71`: release, then callback. Writes use `finish` (callback, then `completed`, `:54-62`). | Lifecycle initial-failure checks passed. | **Closed.** |

**L4b wording point (not a defect).** "Without SQL, dispatch or evidence traffic" is exact for bound unchanged reports and for a no-work `advance`/`reconcile`. A `reconcile` that has assigned nodes still makes its attempt-one inspection GET (`Pipeline.cpp:485-495`) before `record` reaches the latch. That is old read-only behaviour and does not break the stop-before-derivation invariant. The record just should not imply every latched reconcile is free of traffic.

---

## 3. Architecture re-check (whole affected design, not only changed lines)

### 3.1 Transport lifecycle (H1, L2, L3)

**DISABLED is asynchronous, and the code handles that.**
- `FlowController::startFlow` schedules work with `atNextTick` (`FlowController.hpp:172-177`).
- `SocketConnector::init` reports `STATE_DISABLED` and then destroys the connector (`SocketConnector.hpp:244-255`).
- The `SocketClient` wrapper ends the flow only on a final ERROR or FATAL while RUNNING (`SocketClient.h:277-279`).
- So DISABLED reaches the status lambda after `connect()` has returned and the hook is installed (`HttpClient.cpp:170-176`). The new branch is the only thing that completes the operation.

**Once-only delivery and safe re-entry hold.**
- `finish` checks membership (`:202-203`), so a later termination hook does nothing.
- `deliver` clears the callback before calling it (`:196`).
- The DISABLED status never runs inside `pump`, because it arrives on a later tick. A callback that enqueues new work re-enters `request→pump` with `pumping==false`, which is safe. The next connect is deferred again.
- When the disabled connector is destroyed, its captures are released. The flow's own hook holds only a weak reference, so there is no ownership cycle.

**Lost/unsubmitted versus unknown classification is unchanged where it matters.**
- No caller branches on error text. Grep finds only string literals in `HttpClient.cpp`.
- Callers decide on `status` and `submitted`. Examples: `Pipeline.cpp:456`; GitHub token failure → `submitted=false` at `GitHub.cpp:243`.
- Consequence of the shutdown "invalid status" text on a submitted request: `status 0` and `submitted=true` still drive dispatch to "assigned; reconcile" (`Pipeline.cpp:469-474`). So uncertainty is preserved. Only the diagnostic text is misleading.
- The same applies to the older ERROR text "connection failed; request outcome may be unknown" (`:167`). It comes with `submitted=false` and is from before this change. Fixing its wording is optional.

**Dynamic origin rules are unchanged and still sound.**
- `pump` launches a different origin only when `active==0` (`:229-231`).
- The remote address is resolved in the deferred `init`, and no switch can happen while a started operation is a member. So the deferred resolution reads the intended origin.
- The Host header and SNI are written to shared config per connect, but always with the same origin for concurrent operations.

**Process teardown matches the design.**
- A connector cancelled without a status (STOPPING and SYN_SENT) leaves no callback. The design allows exactly this (`design.md:270-277`).
- The SYN_SENT and TLS-stall shutdowns exited with code 241 and restarted without a replacement. No universal callback guarantee is claimed.

### 3.2 Lazy Database FIFO and GraphStore (untouched by `a28f61c`)

I re-verified the following:
- The latch is checked before the producer runs (`Database.cpp:109`). Producer exceptions are definite preparation failures (`:114-115`).
- START failure is definite and does not latch (`:126-127`).
- A failed rollback latches (`:176-178`). A failed COMMIT latches (`:161-163`).
- `finish` calls the callback before `completed()`. A callback exception latches (`:54-62`).
- The drain loop uses a front-identity check (`:41-52`).
- GraphStore applies the change and advances the generation before the callback, and the callback runs before `onChanged` (`GraphStore.cpp:284-293`).
- An unchanged node produces no SQL (`:203-207`).

The removed dispatch catches (`corrections.diff:486-519`) were obsolete. `change()` either queues work or derives synchronously inside `Database::write`'s own `try` (`:112-116`), so nothing a derivation throws could reach them.

### 3.3 Shared-write exclusion and confirmation

These are unchanged since `63ab7b2`:
- The job-level `core-ci-feed-…` group on `publish` (`core-ci-publish.yml:77-78`) matches the status job's (`core-ci-status.yml:19-20`).
- `confirm` (`needs: publish`) returns before identity or key handling (`publish.py:196-199`). It validates the hash format (`:150`) and the revision format (`:197`) before using them.

L1 does not affect the confirm path.

### 3.4 Immutable O/N and capacity policy

- Historical retry keeps the old deployment pins (`GraphTest.cpp:202-221`).
- Restore under a lower limit and the default of 8 are unchanged.
- At limit 1, a definite rejection frees the slot at the Graph boundary (`GraphTest.cpp:198-201`).

---

## 4. Review of the new tests

### 4.1 Tests that prove what they claim
- **Disabled HttpClient** (`GitHubTest.cpp`, `github_transport.py`). Discriminating: the `returned` flag pins asynchronous delivery, the exact error substring is checked, and `seen==[]`.
- **Capacity 1, 0 and 17.** Peak 1, one token, 12 correlated results. The out-of-range values are rejected with no traffic.
- **Multi-address.** It actually ran; it was not skipped (`final-transport.log:23`). It reads the resolver order inside the namespace and binds the listener to the second candidate. The assertion on "using next socketaddress" proves the intermediate ERROR→OK path within one flow. The order is read with Python's `getaddrinfo` in the same namespace. That matching the framework's order is inferred, and the fallback diagnostic corroborates it.
- **START/ROLLBACK proxy** (`enrollment.py`).
  - The proxy rewrites exactly one packet, recomputes the 3-byte length and keeps the sequence byte. Interception is counted, and the logs show `'START TRANSACTION'` and `'ROLLBACK'`.
  - START: `mutations_allowed` stays true and the next write succeeds.
  - ROLLBACK: the latch is set and no further GitHub write happens.
  - With the injected transaction still open, the only reads are the in-memory `view` and root non-locking reads. Then stop, restore the GRANT, restart.
  - After restart it asserts both the configuration revision and `graph_revision.generation` are unchanged.
- **Delayed approval** (`pipeline.py:887-913`). Discriminating against the old behaviour: the captured generation must appear in `status_refreshes`, the latest generation is reached, and no generation is posted twice.
- **Shutdown during TLS, SYN_SENT and response stalls** (`pipeline.py:915-981`). SYN_SENT is checked through `/proc/net/tcp`. Each exits with 241. Restart keeps run 108 with nine dispatches.
- **L1 malformed arguments** and **publication limits 0/33** (`lifecycle.py`).

### 4.2 Binding race: stronger than recorded
`pipeline.py:849-885` holds the `graph_revision` write lock while two signed claims are sent, and checks after each that the node is still unbound.

The results record says the error text cannot tell the paths apart, which is true. But the test also asserts `rejected["result"]["uncertain"] is False` (`:883`). The `observe` catches at `Pipeline.cpp:594` and `:616` produce `{error, assignment_preserved}` with **no** `uncertain` key. Only the lazy preparation path at `:636` includes it. So an `observe` rejection would raise a `KeyError` and fail the test. The test therefore does reach `Graph::bindRun` (`Graph.cpp:471`) at the head of the FIFO.

### 4.3 Test-quality findings (Low; none is a production defect)

**TQ1. The disabled feed-http test cannot tell disabled from enabled.** (`pipeline.py:727-739`)
- *Trigger.* The served receipt bytes are already replaced at `:720` ("modified HTTPS receipt"). `FeedProof` maps every HTTPS failure to the same message (`FeedProof.cpp:100-101`).
- *Effect.* If `feed-http.disabled=true` were ignored, a real mismatched read would produce the identical assertion outcome. The test does prove that `completionReads` is **released**: a stuck entry would make the second reconcile return the "progress/history retained" body and fail the message check. But it does not prove the dynamic-client DISABLED branch ran. Results item 3 overstates this.
- *Evidence.* Source reading. The session total `feed_https: 3` is not enough to settle it either way.
- *Fix (test-only).* Snapshot `len(request_trace)` before the disabled phase and assert no `"/receipts/"` path appears during it. Or move the `deployed_files[...] = raw` restore (`:740`) before the disabled phase, so that an ignored disable would publish and fail `state == "publishing"`.

**TQ2. The forced artifact overlap never reaches `completionReads` coalescing.** (`pipeline.py:983-1012`)
- *Trigger.* The duplicate `report(108)` at `:996` carries the same run facts. `bindRun` returns an identical node, so GraphStore sees no change (`GraphStore.cpp:203-207`). `record` returns `{report: unchanged}` at `Pipeline.cpp:639-640` and never calls `complete()`.
- *Effect.* "One artifact lookup" holds trivially. The in-flight branch at `Pipeline.cpp:654-656` is not exercised, and no test asserts "progress/history retained". The test does show that a held artifact result keeps the node `building`, is accepted afterwards (`publishing`), and that both callbacks complete.
- *Fix.* While the gate is held, POST `/api/work/reconcile`. Its recovery path (`recovery=true`) reaches `complete()`. Assert that the reconciled entry for `current_id` is "progress/history retained", that there is still exactly one `/runs/108/artifacts` GET, and the final `publishing` state.

**TQ3. An old assertion was reshaped and the change is not recorded.** (`pipeline.py:664-676`)
- *Before.* The old assertion was `report(100)[0] == 202 and len(publications) == 2`. It proved that the build-completion continuation itself admitted a second concurrent publication on one feed.
- *Now.* At limit 1, `report(100)` is blocked (200). The second publication is admitted by startup reconciliation inside `start()`. `report(100)` is then an unchanged 200.
- *What remains covered.* P1's two concurrent publishers are still covered by `len==2` and the database count check at dispatch (`:198`).
- *What is lost.* The continuation-driven second admission at capacity above 1 is not covered there. The agreement's "definite terminal result frees capacity" is covered only in GraphTest (`:199-201`), not at the pipeline boundary.
- *Fix.* Restore the original 202 assertion at default capacity, and put the limit-1 check on another built target. Or free the limit-1 slot with a definite terminal publication result and assert that its continuation dispatches the waiting one.

**TQ4. The shutdown test pins incidental parser wording.** (`GitHubTest.cpp:47`)
- It requires exactly "HTTPS response has an invalid status" on the submitted request.
- If the parser stopped invoking the response callback on teardown, `deliver` would give the more accurate unknown-outcome text and this test would fail on an improvement.
- *Fix.* Assert `submitted && status == 0 && !error.empty()`.

**TQ5. The threaded fixture has unsynchronised handler state.** (`pipeline.py:340`)
- `run_id = 100 + len(dispatches)` (`:167`) is computed before `dispatches.append` (`:177`). `lose_next` is checked and cleared without a lock (`:178-179`).
- With up to eight concurrent dispatch POSTs, two handlers could take the same run ID.
- No failure has been observed; this is a flakiness risk only.
- *Fix.* Put one `threading.Lock` around the dispatch-handler body.

**Minor.** Fault mode in `enrollment.py` skips the end-of-run checks for "synthetic-token" appearing in logs. This is optional to fix.

---

## 5. Evidence classification

**Exact local evidence** (final logs):
- `final-transport.log`: 16/16, with multi-address passed rather than skipped.
- `final-faults.log`: 2/2, with the intercepted command text shown.
- `final-lifecycle.log`: 3/3 after the socket rename. The 107-byte path failure is kept visibly in `final-tests.log:27`.
- `final-connect-pipeline.log`: passed after the SYN and artifact additions were made.
- `verified-graph.log` and `verified-final-build.log`: graph-test relinked and passed. Earlier stale-binary results are discarded.
- `final-format-check.log`: passed.
- `line-accounting.json`: +233/+646 against the +310/+790 limits. Headroom is 77 production and 144 test lines. The sandbox +450 allowance is unused.

**Limitation.** The logs do not embed the input hash. Linking them to `364d21fa…` and to `a28f61c` relies on the recorded sequence and your statement. The results record's "State" section (uncommitted, HEAD `63ab7b2`) describes the checkpoint before the commit.

**Source dispositions (not runtime coverage):**
- Active-origin switching (`FeedProof.cpp:36`).
- Late orphan `onConnect`.
- Latched no-op 503 plus SSE.
- Lazy preparation failures in built-result acceptance, missing artifacts and receipt application.
- `completionReads` in-flight coalescing (TQ2).
- Disabled dynamic `feed-http` as distinct from a mismatch (TQ1).

**Not run:** sanitizer or instrumented memory-safety checks. A clean exit does not show there is no use-after-free.

---

## 6. Contradictions with the agreement or proposal

There is no contradiction in production behaviour. On test or record precision:
1. **Agreement plan "a definite terminal result frees capacity."** This is met only at the Graph boundary (TQ3).
2. **Results item 3 ("disabled feed-http…").** Release is proven; the disabled path itself is not (TQ1).
3. **Results item 10 ("overlaps duplicate completion").** The duplicate never reaches completion handling (TQ2).
4. **L4b's "no evidence traffic."** This is precise only for bound unchanged reports and no-work advance (§2).
5. **Proposal's "synchronous disabled connect" label.** It is corrected, and the test pins asynchronous delivery. This agrees with the agreement.

---

## 7. Gates that remain open (live; not code defects)
- **Mixed O/N lock exclusion.** O's workflow-level group versus N's job-level group needs a real sandbox proof before any capacity above 1 while O workflows can still run. The design's staged rollout starts at `--publication-limit 1` with old pins.
- **Sandbox aliases** under the separate +450 limit.
- **Timing:** graph/DB/SSE, status churn, response-to-close slot occupancy, runner setup and Pages latency. No latch or timer is suggested to hide these.
- **Other sandbox evidence the design requires:** validation overlapping, the status and publish jobs never holding the lock together, historical O retry, signed client installation.
- **Owner acceptance**, and every commit/merge/install/restart/activation stage not yet performed.

---

## 8. Recommendation

- **Merge (code).** No confirmed production defect remains. The corrections match the agreement, keep the single graph authority, the existing FIFO, commit-before-dispatch, uncertainty preservation and independent publication proof, and stay within the approved limits. I recommend fixing TQ1 and TQ2 before merge, or at least correcting results items 3 and 10, because they are cheap test-only changes and stop two claims from overstating coverage. TQ3–TQ5 can follow. None of them is a merge blocker in code terms.
- **Deployment.** Follow the design's staged rollout: start at `--publication-limit 1` with old pins, and don't go above capacity 1 while O workflows remain until the mixed-lock sandbox proof exists. Reviewer agreement is not live acceptance and does not authorise activation; the owner decides.
