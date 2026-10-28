# Agreed review correction prompt

This is the review record used for F1/F3/F4/S1/S2/S4. The implementing agent corrected the
prompt's mistaken "full registered suite, six checks" wording: the repository had 75 registered
tests, and only affected checks plus new regressions were run. Single-return style takes precedence
over pseudocode "return unless". CMake test registrations are reported separately from runtime code.
The owner clarified that additional lines may be proposed when needed; this correction uses 3,572 C++ lines.
F2 pagination remains a separate owner approval question, not authorization within this record.

# Correction implementation prompt: F1/F3/F4/S1/S2/S4 against `e78923e`

Both refinements are now settled, so this is the full prompt. It covers only the agreed items. The disposition table at the end records everything else, including F2 as explicitly unresolved.

~~~text
# Task: implement agreed review corrections F1, F3, F4, S1, S2, S4 on rewrite/slim-graph-controller

## 0. Authority and limits
- Source: commit e78923e on branch rewrite/slim-graph-controller. Stop and report if HEAD differs.
- The owner's current instruction authorizes implementing exactly the fixes below within these limits. Do not add a further approval gate. Do not widen scope.
- Not authorized here: commit, push, merge, deploy, live GitHub calls, live retries. These are separate stages.
- No new policy or architectural mechanism. That means no new persisted field, lifecycle state, retry loop, latch, flag, timer, owner waiver, or production test/injection hook.
- Do not use subagents. Do not touch protected or private directories.
- Preserve the four modified docs byte-for-byte: README.md, deployment/sandbox/README.md, docs/deployment/README.md, and WORK_LOG.md. WORK_LOG.md may only receive an appended entry at the end.
- Leave all untracked docs alone: docs/improvements/, docs/proposals/*, docs/reports/*.
- Before the first edit, record sha256 of the four dirty docs. Re-verify them at the end; WORK_LOG.md must have only an appended suffix.
- Do not make any timing, throughput or live claims. Fewer duplicate status requests is something to measure, not to promise.

## 1. Size contract (hard)
- Canonical C++ count: `wc -l` over src/**/*.cpp and src/**/*.h, excluding UiAssets.h.in. The baseline is expected to be 3576. Confirm it and record the exact command.
- Overall production count: the C++ count plus shipped UI assets (app.js and any other files embedded via UiAssets). Record the baseline.
- After the final step, C++ must be at most 3576 and overall production must be at most baseline.
- Order is mandatory: reductions (S2, then S1+S4) land before any growth (F1+F3).
- Recount after every step and log the numbers.
- If any growth step would exceed either limit after all reductions, stop before applying it and report the shortfall. Do not compress unrelated code to make room.

## 2. Baseline (before edits)
- Configure and build the persistent build dir build/codex/speed-release.
- Run the full registered test suite. Expect 6/6 passing; record the output.
- Run `cmake --build build/codex/speed-release --target format-check`.
- Write all evidence under /tmp/codex/build/core-ci/review-corrections/: baseline/, step-N/, final/.

## 3. Step 1: S2, synchronous repository list (reduction, about −6 C++, −1 JS)
- Change `list()` in the Enrollment component to `Json list() const` returning `{"repositories": [...]}`. Keep the element shape unchanged.
- main.cpp, at the three call sites (currently about 184-189, 212-214, 218-220; relocate by symbol): send the returned `"repositories"` value directly, with no wrapper or callback indirection.
- app.js (about :445): delete the `unavailable` event listener. First grep src/ to confirm no server path emits that event, and record the grep. If an emitter exists, stop and report.
- Invariant: the HTTP list endpoint and the SSE bootstrap/repository event payloads are unchanged in content.

## 4. Step 2: S1 + S4, status dispatch on public facts (reduction, about −17 C++)

### GraphStore (GraphStore.cpp/.h)
- Delete the before/after public-projection comparison block in the commit/notify path.
- Delete both private `view`/`publicView` overloads that take a state argument. `view()` and `publicView()` operate on the member `state` only.
- `publicView()` no longer includes `generation`. All other public within-state facts are unchanged.
- The change callback becomes `onChanged(bool repositoriesChanged)`. There is no public-change flag.

### main.cpp
- `publicStatus` (the public endpoint builder) adds `generation`, alongside the schema/feed fields it already adds. The public endpoint body must be key-for-key and value-for-value identical to baseline for the same state.
- The onChanged handler calls `pipeline.refreshStatus()` on every notification, including notifications while the store is uncertain. The gate lives inside refreshStatus.
- The endpoint 503 under uncertainty is unchanged.

### Pipeline (Pipeline.cpp/.h)
- `statusSent` becomes `Json`, default null, so the first refresh after startup dispatches.
- `refreshing` becomes `bool` (S4).
- Remove `<optional>` if it is now unused.

### refreshStatus flow (exact)
1. Entry: return unless `!statusWorkflow.empty() && store.mutationsAllowed() && !refreshing && store.publicView() != statusSent`.
2. Set `refreshing = true` and run the existing approval preflight unchanged.
3. If approval fails: set `refreshing = false`, keep the existing failure log, do NOT touch `statusSent`, and do NOT call refreshStatus. A later commit notification or explicit reconcile retries while facts differ. This residual failure-mode read cost is accepted. Do not add anything to suppress it.
4. If approval succeeds but `!store.mutationsAllowed()`: send no POST, set `refreshing = false`, leave `statusSent` untouched, and log one clear line such as "status dispatch skipped: graph authority uncertain". If an existing log line already says exactly this, reuse it.
5. Otherwise, in the same synchronous step, immediately before the POST: set `statusSent = store.publicView()` and read the committed generation from the store. Send the POST carrying that generation.
6. `statusSent` stays assigned whatever the POST outcome. An uncertain POST is never replayed.
7. In the POST completion, whatever its outcome: set `refreshing = false`, then call `refreshStatus()`. With unchanged facts this is a no-op.

### Invariants
- No status POST is sent while mutation authority is uncertain, checked both at entry and after approval. This is stricter than baseline.
- Preflight commits and dispatch-ID commits do not cause an extra status dispatch.
- The worker contract is unchanged: status_update.py still receives `generation` and publishes live facts at or above it.

## 5. Step 3: F1 + F3, enrollment guards and exact post-PUT commit (growth, about +9..11 C++)

### Enrollment::save, pre-PUT guard
- At save start, capture `repositories` (the whole object) and `next_revision`. Also capture the `enrolled` row for this repository.
- The start-time `validateSources` (about :77, synchronous 422) is unchanged.
- At the pre-PUT check (about :79), replace the `generation != ...` comparison with `repositories != startRepositories || next_revision != startNextRevision`.
- Keep the existing pre-PUT 409 response body and status exactly as at baseline.
- Retained guard lifetime: these two snapshots are held by value in the save closure only up to the PUT dispatch decision. They are not used after the PUT.
- Do NOT rerun `validateSources` inside SourceReader/file callbacks. Exceptions there are swallowed after `finish()` clears the callback, which would leave the HTTP request unanswered.
- Pipeline's existing selection guards are unchanged. A conservative 409 on concurrent enrollment, selection or retry remains; a 409 on job progress no longer happens.

### Enrollment::persist
- Replace the generation parameter with `const Json& expected`.
- Require `state.repositories.value(name, Json::object()) == expected`.
- Keep the existing ID check and the commit-time `validateSources`.
- On mismatch while the store is certain: 409 `{"error": "repository enrollment changed during GitHub read"}`, plus any fields the caller path already adds.
- When the store is uncertain, use the existing uncertain 503 contract, unchanged.
- `Json::object()` is the only "absent row" marker. Real rows are never empty.

### Enrollment::refresh (import)
- New signature: `refresh(input, callback, const std::string& commit = "", const Json& expected = nullptr)`.
- Null `expected` means capture now: `expected = repositories.value(name, Json::object())` at refresh start.
- Routes call `refresh(body, cb)` only. No JSON field can set `commit` or `expected`.
- If `commit` is non-empty, read `/commits/<commit>` instead of the branch.
- The stored row keeps `branch` from input. `revision` is the SHA returned by the verified lookup.
- Retained guard lifetime: `expected` is held by value through the GitHub read closures and compared in `persist`. No member state.

### Enrollment::save, post-PUT
- Extract the SHA without throwing: check `body.is_object()`, then `body["commit"].is_object()`, then `sha` is a string.
- Validate the SHA against `^[0-9a-f]{40}$` using `std::regex_match` (add `<regex>`). If an equivalent existing hex/length idiom is shorter, you may use it.
- Valid SHA: call `refresh(input, cb, sha, enrolled)`, where `enrolled` is the row captured at save start.
- Same-row change during the PUT: 409 with `github_committed: true`. Never silently overwrite.
- Other-row changes during the PUT are caught by the commit-time `validateSources`, as today.
- Missing or malformed PUT SHA: respond 503 `{"error": "<...> committed without a commit identity; import again", "github_committed": true}`. Do not read the branch and do not persist.
- After the PUT, every error path keeps `github_committed: true` via the existing mechanism. This includes verified-lookup failure and GitHub lag on the new commit.
- No coalescing: the GET key includes the path suffix, so `/commits/<sha>` must not join an in-flight `/commits/<branch>` read. Assert this in tests, not in code.
- After this step, Enrollment must not reference `generation` at all. Grep to confirm.
- Accepted residual: the window between passing validation and the PUT being in flight is the same as at baseline.

## 6. Step 4: F4, UI uses real workflow fields (about 0 net JS)
- app.js (about :544): read `.content` instead of `.current`.
- app.js (about :520-521): remove the `github_revision` usage. Show the real `blob_sha` (the actual save guard) with the existing "Start new definition" hint.
- `selected.revision` stays the enrolled revision, matching save's check (about Enrollment.cpp:80).

## 7. Tests (source vs tests)
Production edits are limited to steps 1-4. Everything else goes in tests.
- Use the existing real MariaDB and synthetic GitHub fixtures.
- Test-side threads, events, gates or delays inside the synthetic GitHub fixture are allowed. Production code gets no hooks.
- Register each new case as an independent registered test, not folded into an existing one, so pass/fail is visible per case.

### Updates to existing tests
- GraphStoreTest, about :159: use a `[&](bool repositories)` callback and assert `!repositories && callbacks == notifications && graph->graph().at("generation") == notifications`.
- GraphStoreTest: keep the 13/0 count at about :54.
- pipeline.py, about :576: also assert `preview["content"]` equals the committed workflow text.
- ui_enrollment.py, about :162-163: remove `github_revision`.
- ui_enrollment.py: add a `/api/repositories/workflow` reply with the real body shape `{repository, head, blob_sha, proposed_blob_sha, proposed, content}`.
- ui_enrollment.py: add error replies carrying the real `error` and `github_committed` fields.

### New registered regression cases
Each case blocks the relevant synthetic GitHub response with a test-side gate and drives the competing change through the public HTTP/webhook boundary.

1. enrollment-progress
   - Import (refresh) is blocked in its GitHub read while a job-progress commit lands.
   - Assert: import succeeds and the row is persisted.

2. save-progress
   - Save is blocked first in its pre-PUT read and then in its PUT, while job-progress commits land each time.
   - Assert: PUT is sent, 200, and the row has the new revision.
   - Sub-case: an enrollment change or selection during the pre-PUT read gives the baseline pre-PUT 409 with no PUT sent (check the fixture request log).

3. same-row-put-race
   - Save is blocked in its PUT while the same repository row is changed through the public boundary.
   - Assert: 409 with `github_committed: true`, and the stored row equals the concurrent change.
   - Sub-case: a different-row change during the PUT does not break save, provided sources stay valid.

4. exact-post-put-sha
   - The fixture's PUT returns SHA X. The fixture's branch GET for the same branch returns stale SHA Y and is held open by an overlapping concurrent branch read.
   - Assert: the fixture log shows `/commits/X`, the stored revision is X, and no post-PUT `/commits/<branch>` request is made by this save.
   - Sub-cases with PUT body missing `commit`, missing `sha`, and a non-hex or wrong-length sha: assert 503, `github_committed: true`, no branch read after the PUT, and the row unchanged.

5. status-delayed-approval
   - The status-workflow approval preflight is blocked while public facts change through the public boundary.
   - On release, count `core-ci-status.yml/dispatches` in the fixture trace and assert exactly one dispatch, carrying the latest generation.
   - Further commits with unchanged public facts produce no further dispatch.
   - Uncertainty sub-case: latch mutation authority during the blocked approval, using the same public-boundary mechanism the existing unconfirmed-mutation tests use. Assert no status POST. If no such public-boundary mechanism exists, report the gap. Do not add a hook.

6. unchanged-private-commits
   - Run one build dispatch end to end.
   - Assert: preflight commits and dispatch-ID commits add no status dispatch beyond what public fact changes require.

### Existing behavior to preserve
Run these and name them in the report:
- zero-request unchanged burst
- unconfirmed mutation handling (including unconfirmed status)
- receipt-mismatch policy (S3 dry run retained, 503 `uncertain: true`)
- SSE bootstrap and repository events

### UI DOM probe (synthetic)
- Run the real src app.js under the jsdom already present in the existing temp location from the earlier probe. Do not install anything, add no package files, and add no project tooling.
- Keep the probe script in /tmp/codex/build/core-ci/review-corrections/ui-probe/, not in the repo.
- Feed it the ui_enrollment.py fixture shapes. Assert that the workflow editor shows `content`, that `blob_sha` is displayed with the hint, that no `github_revision` is referenced, and that error bodies render `error` and `github_committed`.
- Label the result "synthetic jsdom DOM probe, not a real browser".
- If the temp jsdom is gone, skip the probe, say so, and do not install it.

## 8. Final verification
- Build build/codex/speed-release.
- Run the full registered suite: the 6 baseline cases plus the new ones, all passing.
- Run the format-check target.
- Rerun the line counts against both size limits.
- Grep that Enrollment no longer references `generation` and that `<optional>` is gone from Pipeline if unused.
- Run `git diff --stat`, then confirm the dirty-doc hashes and untracked docs are untouched.
- Save all outputs to final/.
- Append a dated WORK_LOG.md entry covering: items implemented, line counts before and after, test results, the synthetic probe status, and the open items (F2, owner flags).

## 9. Report
- Per-step diff summary with C++ and production line counts.
- Test output, quoting any failure verbatim.
- Any stop condition hit, any skipped step, and why.
- Explicit statement: nothing committed, pushed, deployed or run live.

## 10. Disposition (scope record; do not implement anything marked deferred or unresolved)

| Item | Disposition |
|---|---|
| F1 | Implement: repositories+next_revision pre-PUT guard; same-row `expected` through PUT/import |
| F2 | UNRESOLVED, out of scope. head_sha/clock filters withdrawn. Bounded pagination needs a separate concrete proposal and any growth approval. Do not describe it as fixed. |
| F3 | Implement: validated immutable PUT SHA read; 503 `github_committed` on a missing or malformed SHA |
| F4 | Implement: real `content`/`blob_sha` fields, plus fixture and synthetic probe |
| S1 | Implement: latest public facts and generation at POST; uncertainty gate at entry and after approval; no replay |
| S2 | Implement: synchronous `list()`, remove dead `unavailable` listener |
| S3 | Withdrawn: keep FeedProof dry run and receipt-mismatch policy |
| S4 | Implement: `refreshing` as bool |
| C (optional refactors) | Deferred |
| P1, P3, P4, P5 | Deferred, unchanged |
| E | Deferred: no instrumentation, no live calls |
| Framework repeated-startup | Claim withdrawn, no change |
~~~

**Notes for you:**
- **Line numbers** are from `e78923e` and will shift after S2 and S1. The prompt tells the implementer to relocate them by symbol.
- **Uncertainty sub-case in test 5:** this depends on an existing public-boundary way to latch authority mid-flight. If there isn't one, the prompt has the implementer report the gap instead of adding a hook.
- **Regex choice in F3:** `<regex>` is specified. I allowed an equivalent hex/length check only if it is shorter, since the cap margin is about 12 lines.
