# Implementation review: `fix/controller-parallelism` at 63ab7b2 (base 4257cb0)

**Recommendation:** go ahead with the sandbox activation stage once the documentation conflict in finding M1 is fixed. I found no definite blocking source defect in the core mechanisms. Those are the lazy Database FIFO, the two required callback orders, request/connection correlation, publication capacity and the confirm job. One HTTP status path needs a short probe (H1).

**How I reviewed it.** I used Read/Grep/Glob only. I read AGENTS.md, the four proposal documents in the requested order, the supplied patch, the committed source for every touched consumer, and the installed SNode.C headers (`SocketClient.h`, `SocketConnector.hpp`, `tls/SocketConnector.hpp`, `State.h`). I also read the logs under `/tmp/codex/build/core-ci/controller-parallelism/`. I did not read the earlier `claude-implementation-review.md` there, so this review stays independent. I ran nothing, edited nothing, and read no private or credential material. No write tool was available, so no plan file was created.

## Requirement trace (verified, no defect)

**Database FIFO** (`src/Database.cpp:29-62, 104-181`)
- The producer runs only at the head of the FIFO, after the `mutationUncertain` check (`:109-117`). Nothing is derived after an uncertain outcome.
- Every write path ends in `finish()`, which calls the callback first and then `completed()` (`:54-62`). The entry is still in the queue while the callback runs, so work enqueued from inside the callback cannot start early.
- `drain()` with its `draining` flag and front-identity check (`:41-52`) bounds the stack for synchronous no-ops and rejections. The 4096+4096 drain test exercises this against real MariaDB.
- Reads still release before the callback (`:64-71`). That keeps the startup-teardown ownership in `main.cpp:94-104` safe.
- No write callback can destroy the wrapper: every `database.write` caller lives in GraphStore (`:166, :198`), and the restore paths use reads only.
- A callback exception latches `mutationUncertain` (`:57-60`) rather than delivering twice.

**GraphStore partial orders** (`src/GraphStore.cpp:196-295`, `enroll :160-194`)
- Apply plus generation advance (`:286-289`) happen before `completed()`, so the next producer always sees applied state.
- `callback` (`:290`) runs before `onChanged` (`:291-293`).
- No-ops send no SQL, apply nothing and trigger no notification. Preparation failures do not notify. Database failures still notify SSE.
- `enroll` keeps the explicit `expectedGeneration` guard, now evaluated at the head of the FIFO.

**Callers**
- Every `store.commit` caller now passes a derivation: `commitSelection :119`, `retry :231`, `advance :268`, `record :636`, `change :918`.
- The stale-Node retries in `change()` and `record()` are deleted.
- The selection generation guard is kept, inside the derivation (`Pipeline.cpp:121`).
- `advance` dispatches only nodes from the committed change.
- No transaction stays open across HTTP: continuations run after COMMIT.

**HTTP correlation, checked against framework source**
- Both `SocketConnector::init` (immediate success) and `connectEvent` call `finishAttempt` (status OK) and then `onConnect` synchronously, with no application code in between. So the one-slot `connecting` pointer (`HttpClient.cpp:159-166, 82-85`) is sound.
- For an intermediate address failure, the framework strips `NO_RETRY` and does not terminate the flow (`SocketClient.h:237-279`). The status callback only records the error and clears it on a later OK. Finishing happens on flow termination, and only while `connectionId == 0` (`:169-176`).
- The result is delivered on HTTP-context disconnect. The capacity slot is held until transport disconnect (`:60-67, 106-110`).
- An origin switch requires `active == 0` (`:225-227`).
- Retry and reconnect stay disabled (`configure()`).
- Token coalescing is per scope and does not depend on the old FIFO. The parallel test checks one mint for 12 requests.

**Source aggregation** (`Pipeline.cpp:69-115`)
- Results are stored per enrolled row. The first error is chosen in enrollment order, and the 422 now names the repository.
- The no-tag 404 exception, the ID and tag-object checks, `config.deployment` and the final guard are all kept.
- A synchronous callback cannot finish the aggregation early, because `remaining` is set before the loop.

**Status refresh** (`Pipeline.cpp:390-430`)
- The generation-equality drop before the POST is gone. The POST callback fires again for a newer generation.
- Approval failures are logged. No latch was added and nothing replays the same generation.

**Capacity** (`Graph.cpp:492-519, 205-234, 628-631`; `Pipeline.cpp:280-289`)
- One calculation, `publicationBlocker`, is shared by advance, `beginPublication`, `validate` and `blockers`. Batch reservation is counted per feed.
- Restore accepts more existing assignments than a lowered limit.
- Uncertain publishing nodes keep their slots, and joins still require `published`.

**Workflow** (`core-ci-publish.yml`)
- The lock is on `publish` only, using the same group name as the status job.
- The feed checkout, prepare, sign, render, commit and push stay inside the lock. Validate runs outside it.
- `confirm` has `needs: publish`, no environment, no secrets, `contents: read`, no `always()` and no conditional push.
- Its name does not match `matchesJob` (`Pipeline.cpp:244-251`).
- Outputs are written in prepare, before the push (`publish.py:274-277`). The hash is taken over bytes `prepare_commit` has already checked against the staged index.
- `confirm` returns before `run_identity`, key handling or token minting (`:191-194`). The HTTPS read is bounded to 2 MiB+1, the origin prefix is checked and the hash must match exactly.
- `trusted_deployment` still rejects attempt 2.
- The controller's `beforePush` and FeedProof logic is unchanged, so a skipped push is still a definite failure and a possible push stays uncertain.

**Enrollment fixture route** (`tests/enrollment.py:126`)
- This change is explained, not a regression. The enrollment test runs `graph-store-test`, and its new `overlap()` seeds a second building node with no bound run. Startup reconcile then correctly searches for its run.

**Line budget**
- `line-accounting.json` shows production 652/452 (net +200 of the +310 bound) and tests 398/116 (net +282 of +790).
- `format-check` passed and the build logs show no warnings.

## Findings, ranked by severity

### M1. Medium, definite documentation and deployability defect: the install instructions conflict with keeping the O generation
- **Location:** `docs/deployment/README.md:227-234` versus `:574-577`. These line numbers are from the working tree, which has uncommitted edits to this file.
- **Trigger:** the general install step says to commit the contents of `deployment/worker`, including `.github/workflows/`, at the repository root. The upgrade step 3 says "Commit reviewed new worker tooling first". Followed literally, the N tooling commit replaces O's `core-ci-publish.yml` on `main` with the new template, unresolved `@TOOLING_SHA@` marker included.
- **Violated invariant:** historical exact retry. Old assignments approve `core-ci-publish.yml` at `publicationRef` against the O blob (`PipelineConfig.cpp:151`), so their dispatch fails approval. That is the rollout the design itself prohibits.
- **Smallest fix:** in step 3, state that the N tooling commit must leave O's deployed publisher and status workflow paths byte-identical, and that rendered N templates go only to the new alias paths. Qualify the general install paragraph as first-install only.

### H1. Medium if confirmed, needs a probe: a connector that ends with a non-error status can strand an operation
- **Location:** `HttpClient.cpp:159-176`.
- **Trigger:** a disabled `github-http` or `feed-http` instance, or a connector cancelled without status. In that case `SocketConnector::init` calls `onStatus({}, STATE_DISABLED)` and then `destruct()`. `SocketClient.h:277-279` calls `terminateFlow()` only for ERROR or FATAL while RUNNING. Whether `observeConnectEventReceiver` terminates the flow on destruct is implemented in a .cpp file I could not inspect.
- **Effect:** the operation stays `started` forever. It holds a slot, its callback is never invoked, and `refreshing` or `completionReads` stay latched. The old code finished on any non-OK status, and the current error text still says "or is disabled".
- **Fix:** probe it with a disabled instance. Or, more simply, call `finish(operation)` in the status callback when `state == DISABLED`. The existing `std::find` and `terminateFlow` path already handles synchronous removal.

### T1. Medium, acceptance gap in tests
The design's own verification table (`controller-parallelism-design.md:559-565`) lists checks that are not implemented:
- **HTTP:** origin drain and switch on `feed-http`, multi-address establishment, a late orphan `onConnect`, capacity 1, a request enqueued from a callback, and shutdown during connect/TLS/response.
- **Options:** neither `--max-in-flight` nor `--publication-limit` appears in any test, so default and range parsing are unexercised. `--max-in-flight 1` is the natural rollback setting.
- **Status (N1):** no test sustains commits while an approval is in flight. The `pipeline.py:477-481` addition is a polling wait, not that test.
- **Pipeline boundary:** the interleaving of evidence, `built` and `advance` is proven only at the GraphStore level (`overlap()`), not end to end.
- **Lazy producer failure paths:** START failure and rollback have no direct test. The uncertain-commit path is covered only through enrollment's lost COMMIT.

The coverage that does exist is meaningful: reverse-order correlation with a peak of ≤ 8 out of 12, the overlap and drain test against real MariaDB with an order assertion, P1 with two publishers, restart without replacement, and the confirm boundary mocks.

### L1. Low, definite: required arguments were weakened
- **Location:** `publish.py:178-182`.
- **Problem:** `--build-result`, `--artifacts` and `--feed` now default to `""` for every phase. A prepare or push run without `--feed` would resolve to the current directory.
- **Fix:** add `require(...)` checks for these arguments when the phase is not `confirm`.

### L2. Low, definite: exactly-once delivery relies on unspecified `std::function` behaviour
- **Location:** `HttpClient.cpp:193`.
- **Problem:** delivery happens only once because a moved-from `std::function` is left empty. The standard leaves its value unspecified, although libstdc++ and libc++ do empty it.
- **Fix:** use `std::exchange(operation->callback, nullptr)`.

### L3. Low, definite: shutdown cancellation is mislabelled
- **Location:** `HttpClient.cpp:222-223`.
- **Problem:** queued operations that never started are finished during STOPPING with "request outcome is unknown". `submitted=false` is still correct, so the semantics hold, but the text contradicts the agreed wording ("not submitted during shutdown").
- **Fix:** set the error string before calling `finish`.

### L4. Low, behaviour changes to note
- Error mapping and response bodies changed:
  - A late or invalid `built` derivation used to return 503 with `assignment_preserved`; it now returns 409 without it.
  - A `bindRun` failure in `record` no longer carries `assignment_preserved`.
  - A no-op `change()` now returns `{report: unchanged}` without the execution ID.
- Unchanged reports now pass through the FIFO. While the uncertainty latch is set, each duplicate webhook gets a 503 and causes a graph SSE broadcast. The direction is conservative but the change is visible.
- `Graph::blockers` (`Graph.cpp:211-219`) now returns no blockers for a built node with free capacity. The old "attempt is building" reason is gone. This only affects presentation; `beginBuild` is only ever called for waiting nodes.

### I1. Informational: callback order between a read and later no-op writes
- **Location:** `Database.cpp:64-71`.
- Because a read releases before its callback, no-op writes queued after it can run their callbacks before the read's callback. I found no consumer that depends on that order.

## Hypotheses and gaps that need live measurement or a probe
1. **Mixed-level lock exclusion.** I assume O's workflow-level group and N's job-level group with the same name exclude each other. The existing status job already mixes levels, but this needs a sandbox probe before raising `publication-limit` above 1 while O workflows can run.
2. **Status churn.** Under sustained commits, refresh now dispatches one status run per round trip. Each status run takes the feed lock and can trigger a Pages deployment, so expect more lock contention and Pages load under P1. This needs measuring; it is not a defect.
3. **Selection guard rejections.** A higher commit rate raises the chance that the retained global selection guard rejects an automatic tag selection (409). This was knowingly deferred, but should be watched live.
4. **Slot occupancy.** Time from response to transport close reduces effective HTTP capacity. Not measured.

## Owner policy and known deferred scope (not defects)
- P1 admission, the capacities of 8, the confirm job, and O/N retention were approved.
- Signing and setup still run inside the locked publish job, and the status job's lock still covers its setup. These are the acknowledged partial boundary.
- Narrowing the guards is deferred.
- No runtime throughput or rollout claim is made, and none is supported.
- The local 0.0919 s progress figure and the zero-request burst come from existing harnesses, not new ones.

## Remaining disagreement and acceptance gaps
I have no disagreement with the agreed mechanisms as implemented. Before calling this accepted, I would want:
- M1 fixed.
- H1 probed or fixed.
- The T1 gaps closed, or explicitly accepted as live-only.
- The sandbox evidence the design requires:
  - validation overlapping across publications
  - the publish and status jobs never holding the lock at the same time
  - O/N historical retry
  - mixed-lock behaviour
  - signed client installation
  - separate timings for each stage
