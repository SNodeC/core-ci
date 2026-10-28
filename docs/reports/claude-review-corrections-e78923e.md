# Validated review corrections

The implementing agent and Claude agree on the implemented F1/F3/F4/S1/S2/S4 corrections.
F2 remains open: bounded pagination and its code growth await the owner's answer. This is
not complete resolution of every finding. No clock or preflight-commit filter was added.

## Source and authority

Canonical checkout `/home/voc/projects/snodec/core-ci`, owned branch `rewrite/slim-graph-controller`.
HEAD `e78923e78a41a20bdc4d9a55533339ee8a0d96e6`; remote main
`a11d98b7b3c401168e36a1029218631ba01d0f6e` has the same starting tree. No open PRs.
Corrections are a local working-tree patch; nothing committed, pushed, merged or deployed.
Existing unrelated documents and private directories were preserved. Only WORK_LOG receives
an appended task entry. Current deployment remains unchanged.

Invariant: one committed graph authority; unrelated evidence cannot invalidate an edit;
post-save import reads the exact committed bytes; public progress includes within-state
facts and unchanged facts do not cause derivative dispatch. Uncertainty never authorizes replay.

## Finding dispositions

| Finding | Validated result and disposition |
|---|---|
| F1 | Confirmed and fixed. Pre-PUT repositories/next_revision guard preserves namespace inputs while allowing progress. The original same-repository row comparison remains active through PUT/import. Current namespaces are validated at persistence. |
| F2 | Confirmed, not fixed. Incomplete listing over 100 cannot bind. Proposed head_sha filter withdrawn: approved actual run commit may differ after branch advancement. Clock filters also withdrawn. Owner asked about bounded pagination, up to ten pages, failing closed at 1,000 or inconsistent evidence; estimated +20–35 C++ lines. |
| F3 | Confirmed and fixed. Validate the PUT commit SHA and import exactly it, keeping branch metadata. Missing/malformed identity returns 503 with github_committed:true and no mutable-ref fallback. |
| F4 | Confirmed and fixed. UI reads content and displays the real blob identity; fixtures no longer invent github_revision. |
| S1 | Confirmed and reshaped. Pipeline compares public facts once per notification, with latest facts/generation captured after approval. GraphStore's duplicate before/after projections and filter plumbing are removed. Public endpoint generation is unchanged. |
| S2 | Confirmed and reduced. Repository list returns JSON synchronously; API and SSE payloads remain unchanged. Dead unavailable event removed. Explicit value loop retained. |
| S3 | Withdrawal agreed. Dry-run receipt validation retained: deletion changed 503/uncertainty classification and evidence recheck behavior. |
| S4 | Confirmed and fixed. Unread optional generation replaced by a bool for the same existing in-flight responsibility. |
| C | Optional control-flow/error-wording changes deferred. |
| P1/P3/P4/P5 | Operator retirement, retry, DB recovery and uncertain push policies unchanged. |
| P2 | State-only refresh withdrawn. Within-state progress, timing and links remain required. |
| E | Scaling properties confirmed statically, not measured live; no speculative optimization added. |
| Framework | Repeated-startup claim withdrawn. State equality is exact; intermediate address notifications carry NO_RETRY and do not enter the exact-OK startup branch. |

No new persisted field, queue, scheduler, timer, production hook, workflow change or worker change.
One last-dispatched facts value replaces the generation scalar; it is derivative data, not execution authority.
Failed approval can incur a new read on later notifications or explicit reconcile; no latch was added.
Database authority is checked at refresh entry and after approval. Unconfirmed status POSTs are not replayed.

## Verification

Release configuration: `build/codex/speed-release`, installed public SNode.C targets.

```sh
cmake --build build/codex/speed-release --target core-ci graph-store-test -j2
cmake --build build/codex/speed-release --target format-check
node --check ui/app.js
git diff --check
```

Twenty affected checks passed; unrelated suites were not run.
Existing checks (12): lifecycle-sse-heartbeat, enrollment-boundaries, persistence-graph,
pipeline-chain, pipeline-recovery, pipeline-overlap, pipeline-workflow, pipeline-changed,
pipeline-enrollment-race, pipeline-unconfirmed-dispatch, pipeline-unconfirmed-run,
pipeline-retirement.

New independent checks (8): pipeline-enrollment-progress, pipeline-save-progress,
pipeline-same-row-put-race, pipeline-exact-post-put-sha, pipeline-status-delayed-approval,
pipeline-unchanged-private-commits, pipeline-status-uncertain,
pipeline-status-approval-recovery.

Each check was invoked with `ctest --test-dir build/codex/speed-release -R '^NAME$' --output-on-failure`,
or the corresponding anchored alternation for selected groups. New regressions were run serially
against the final fixture version and individually logged under the task's `final/` directory.
They use real isolated MariaDB and synthetic TLS GitHub, including a test-side lost-ACK wire proxy.

Actual ui/app.js response-contract probe passed using existing temporary jsdom, not a browser:
`node /tmp/codex/build/core-ci/review-corrections/ui-probe/probe.cjs`.
It verifies committed workflow content, error/committed feedback and unsupported definition blob display.
No new project browser tooling was installed. Actual browser layout/interaction was not tested here.

Preserved checks include zero outgoing requests for an unchanged eight-report burst,
immutable workflow proof, possible push uncertainty, HTTPS receipt mismatch, restart without
replacement, and graph/SSE/repository bootstrap. Negative no-request assertions are bounded
observations using 0.15-second settling windows, not formal proof of all future executions.
Receipt dry-run classification was also verified by source inspection; no new runtime claim
for every invalid-assignment receipt shape is made.

Failures encountered and resolved:
- Two new fixture failures: exact pretty-printed JSON bytes were not retained; a second
  save after a committed conflict omitted the required explicit reimport. Corrected fixture,
  retained all assertions, reran successfully.
- Existing enrollment fixture returned no immutable commit URL: initial error was
  `GitHub source evidence is unavailable` with `github_committed:true`. Added the real endpoint shape.
- Nine checks initially failed before fixture startup because `/tmp` was full:
  `preallocating 100663296 bytes ... failed with error 28`. Removed only stopped task-owned
  synthetic database files, retained logs/traces, and reran the affected checks successfully.
- Attempted direct JSON-object-iterator array construction threw
  `[json.exception.invalid_iterator.209] cannot use offsets with object iterators` in the existing
  empty-list API check. Restored the explicit loop. Final production patch matches Claude's review.
- New approval recovery test initially assumed generation zero; the fixture starts at one.
  It now asserts the captured generation remains unchanged and passed.

## Measurements and accounting

One comparable local synthetic workflow trace: baseline 818 total outgoing HTTP requests /
178 status POSTs; corrected 660 total / 124 status POSTs. Tokens, approval reads, dispatch,
artifact/feed proof and public HTTPS are included in outgoing totals. Counts depend on event
interleaving; this is not a live throughput or latency speedup. Deterministic private-commit
regression observes startup/build/progress refreshes only, and duplicate burst remains zero.
No live performance measurement or deployment acceptance is claimed.

C++: 3,576 → 3,572 physical source/header lines. Runtime C++/UI code: net −5 lines.
CMake adds eight test-only registrations, accounted separately; tests contain no production hooks.
If all CMake text is counted together with production files, that combined textual net is +3,
entirely from independent test registration rather than runtime code. No controller growth is required.

| File | Added | Removed | Net |
|---|---:|---:|---:|
| CMakeLists.txt | 8 | 0 | 8 |
| src/Enrollment.cpp | 35 | 19 | 16 |
| src/Enrollment.h | 3 | 3 | 0 |
| src/GraphStore.cpp | 3 | 14 | -11 |
| src/GraphStore.h | 1 | 3 | -2 |
| src/Pipeline.cpp | 10 | 9 | 1 |
| src/Pipeline.h | 2 | 3 | -1 |
| src/main.cpp | 9 | 16 | -7 |
| tests/GraphStoreTest.cpp | 2 | 2 | 0 |
| tests/enrollment.py | 1 | 1 | 0 |
| tests/pipeline.py | 216 | 17 | 199 |
| tests/ui_enrollment.py | 5 | 2 | 3 |
| ui/app.js | 2 | 3 | -1 |

Build identity:

```json
{
  "head": "e78923e78a41a20bdc4d9a55533339ee8a0d96e6",
  "production_diff_sha256": "012b6ac590ae7f94d8e1c3108745441b5514f7de1cffea50573aa464530f12eb",
  "source_ui_diff_sha256": "89f32a5ea3bebd4aa5d7a3d7835af29014fac9e90cc2dc8114de89a8ae842eff",
  "cpp_lines": 3572
}
```

## Claude final source review

I found no production defects in the final patch. Two gaps are test coverage only, and none of the agreed boundaries changed. I reviewed the source by reading it. I did not build or run anything, so the test results quoted below are the ones you reported, not ones I reproduced.

## Production code, item by item

**S1: public status dispatch** (`src/Pipeline.cpp:586-627`)
- **Gate after approval:** authority is checked on entry and again after the workflow blob is approved (`:597`). `Database::uncertain` never resets (`src/Database.cpp:85,114,119`), so once authority is lost, every later `refreshStatus` is a no-op. That includes the call from the POST callback. The `status-uncertain` case covers the approval read finishing after the latch.
- **At most one POST:** `refreshing` stays true from entry until either the preflight fails or the POST callback runs. Every path that clears it also ends that cycle.
- **No lost progress within a state:** `statusSent` is taken after approval (`:601`), so it already includes any commits made during the preflight read. Commits made while the POST is in flight are picked up by the unconditional `refreshStatus()` in the callback (`:619`), which compares against the newest public view. I traced the sequence C1 → C2 (during preflight) → C3 (during POST) and it dispatches exactly twice.
- **No replay:** `statusSent` is set before the POST, so an unconfirmed outcome never re-sends the same view. A later POST only happens when the public view is different.
- **Failed preflight:** this clears only the flag and does not touch `statusSent`. The next commit pays for one more read, nothing is latched, and the latest state is not dispatched until another commit or `/api/work/reconcile`. That matches the agreed behaviour.
- **Generation:** `generation` is read together with `statusSent`. The worker's `facts.generation >= minimum` check (`deployment/worker/core_ci/status_update.py:34-40`) still works because `main.cpp:134` adds `generation` back into `/public/status`.

**F1: save and refresh guards** (`src/Enrollment.cpp`)
- **The `next_revision` guard is sufficient.** `validateSources` uses executions only through `source.repository`, `source.package` and `target_row` (`src/Graph.cpp:167-169`), and none of those change after creation. New execution keys are only added at `Graph.cpp:265`, and that path always calls `waiting()`, which bumps `next_revision` (`:40`). The guard also trips on retries (`:362`), so it errs on the conservative side. Progress updates leave it alone, which is the intent.
- **Same-row CAS across the read and the PUT:** `enrolled` is captured from the same snapshot as the guard and passed to `persist` as `expected` (`:110`). The comparison runs inside `apply`, which `Database::write` evaluates in queue order against the current state. A conflict hits `require`, so `code == 0` and the result is 409 with `github_committed:true`.
- **Same-row semantics:** comparison is by value. A no-op refresh writes no SQL and returns 200. If a row changes and then changes back (A→B→A), it is accepted, which is correct because the state is identical. If two concurrent refreshes read identical content and the first one changed the row, the second is still rejected. That is conservative, not wrong.
- **Captures outlive their callbacks:** `[=]` copies the `commit` and `expected` reference parameters into the closures, so nothing dangles. `Enrollment` and `Pipeline` live until process exit, because `release()` only runs before the listener is accepting requests.

**F3: exact SHA after the PUT.** Only a 40-hex string `commit.sha` leads to a read. `SourceReader::file` (`src/SourceReader.cpp:158-160`) requires the returned commit to equal the SHA-shaped ref, so the lookup is exact and immutable. If the SHA is missing or malformed, the save returns 503 with `github_committed:true` and makes no fallback read. The `exact-post-put-sha` case covers all five malformed forms.

**F4: UI fields.** `content` and `blob_sha` match what `workflow()` and the refresh error body actually return. No references to `github_revision`, `.current` or `unavailable` are left in `src/`, `ui/` or `tests/`.

**The other reductions** (synchronous `list()`, the single-flag `onChanged`, removing `generation` from `publicView()`) are behaviour-preserving apart from the agreed move of the public-view comparison into `Pipeline`. The `GraphStoreTest` change still asserts one notification per commit.

## Missing tests (not production bugs)
1. **Retry after a failed preflight** has no regression test. Smallest test: serve a wrong status-workflow blob once, commit some progress, then serve the correct blob and expect exactly one dispatch for the latest generation.
2. **The negative checks** ("no extra requests", "private commits did not dispatch") rely on `time.sleep(0.15)`. They are bounded evidence, not proof.

## Not changed, as agreed
F2 (the 100 bound), S3 dry-run and failure/uncertainty semantics, and C/P/E/framework are untouched. A definite non-2xx status POST still leaves `statusSent` set, which is the same as before.

## What this covers
The `src/` diff and how it interacts with `Database`, `GraphStore`, `Graph::validateSources`/`select` and `SourceReader`, plus the adapted fixtures. My conclusions rest on that reading and on the results you reported: 6/6 affected checks and the 7 new regressions passing, and the UI check being a jsdom probe rather than a real browser. Other existing checks are still being re-run after the `/tmp` disk-full failures, so the suite is not yet confirmed green. There is no live performance evidence.

With that scope, I agree the implementation matches the agreed prompt.

## Claude agreement closure

# Correction review: final agreement

**I agree that F1, F3, F4, S1, S2 and S4 match the agreed prompt.** The basis is my earlier reading of the source, plus your report that the final `src/` and `ui/` are byte-for-byte identical to the patch I reviewed. I have not checked that identity myself, and I have not built or run anything in this round.

## Implemented items: agreed
- **S1, S2, S4 (status dispatch):**
  - Authority is checked again after approval.
  - At most one POST is in flight.
  - `statusSent` is taken after approval, so progress made during the preflight read is not lost.
  - There is no replay of an unconfirmed POST.
  - A failed preflight clears only the flag.
  - The generation contract with the worker is preserved.
- **F1 (save and refresh guards):** the `next_revision` guard, plus same-row compare-and-set through `Database::write` queue order. A conflict returns 409 with `github_committed:true`.
- **F3 (exact SHA):** only a 40-hex `commit.sha` leads to an exact read. Anything else returns 503 with no fallback.
- **F4 (UI fields):** `content` and `blob_sha` match the server responses.
- **Reverted list constructor:** going back to the explicit loop leaves the reviewed source unchanged. Reverting was correct, since the existing API list test caught the shorter version.
- **Lifetime:** closures copy `commit` and `expected` by value. Object lifetime relies only on the existing ownership. No new lifetime guarantee is claimed or needed.

## Coverage gap: closed
`pipeline-status-approval-recovery` covers the case I raised. According to your report, the sequence is:
1. The status blob starts unapproved.
2. An explicit reconcile retries and fails the preflight.
3. The correct blob is restored.
4. The next reconcile dispatches once at the captured fixture generation, with no graph commit.

Comparing against the captured generation, instead of an assumed 0, is the right fix. The test drives the retry through `/api/work/reconcile`. A retry triggered by a commit goes through the same `refreshStatus` entry, and I have reviewed that path only by reading the code.

## Limits that remain
- **F2 (pagination) waits on the owner.** Bounded pagination has been proposed but not approved, so it is not implemented and the current 100 bound is unchanged. There are no clock or head-SHA filters.
- **Negative assertions are bounded evidence, not proof.** "No extra request" and "no dispatch" checks rely on 0.15 s settling windows.
- **Evidence status, as you reported it:**
  - 12 affected checks passed.
  - The 9 failures from the full `/tmp` disk were environmental and passed on rerun.
  - The final serial run of the 8 new cases is still in progress.
  - The UI check is a jsdom contract probe, not a real browser.
  - There is no full-suite claim and no live performance evidence.
- **Unchanged:** S3, C/P/E and the framework. No commit, push, merge, deploy or live CI action was taken.

Within this scope, the correction review is closed. F2 remains open for the owner.

The final eight-case serial run completed successfully after Claude's closure response.
Claude reviewed source and supplied analysis; the implementing agent ran the tests.
