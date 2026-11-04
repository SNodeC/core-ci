# Task: Implement the agreed controller-parallelism corrections on `fix/controller-parallelism`

## 0. Authority, precedence and scope

You are the implementing agent for one coherent correction set on an existing task branch. This is not a new architectural effort.

**Precedence:**

1. `AGENTS.md`, applicable global instructions and explicit user instructions.
2. `docs/proposals/controller-parallelism-fix-agreement.md`, the reviewer agreement and correction authority.
3. The approved proposal and review documents. These are cross-checks only.

Current code is the factual authority. Where a document and the code disagree on a fact, the code wins. Report the discrepancy and do not edit code to match the prose.

**This prompt grants no authority.** It does not grant implementation authority, growth, commit, push, merge, install, restart or activation. Act only under authorization that actually exists. That authorization includes:

- **Existing owner approval** of the four original decisions.
- **A cumulative growth bound** of **+310 net production / +790 net test lines** relative to `4257cb0fc67412da8696afdba307fc2e85ff64aa`. Current usage is +200 / +282. Inside these bounds no further growth approval is needed, and no other blanket approval exists. Re-estimate and report before you exceed either bound or add a mechanism the agreement does not contain.
- **Stage-specific authorizations** for commit, push, merge, install, restart and activation, each checked at its own stage. The existing VOC-Experiments sandbox authority remains valid within its stated scope. Do not invent blockers that it already resolves, and do not assume more than it grants.

**Absolute exclusions:**

- No production SNodeC workflow, credential, key, tag, feed or deployed state may change.
- No SNode.C framework change. Use only installed public SNode.C interfaces.
- No per-origin refactor and no unagreed architectural extension.
- **Production code** gets none of the following:
  - a new queue, cache, flag, timer, scheduler or controller thread;
  - a retry protocol, custom reporting or auth protocol;
  - a hidden lifecycle;
  - a test-injection hook.
- **Test code** may use proxies, counters, threads, clocks and similar controls where a focused test needs them. It must still use public fixtures and boundaries.
- Keep these unchanged:
  - standard HMAC webhooks and the artifact-list boundary;
  - exact assignments, signatures and join semantics;
  - immutable O/N pins;
  - one graph and the existing Database FIFO;
  - the current state machine, the bounded transport and independent publication proof.

Source state: `63ab7b207f55afb5a71c7b2f54d76fa05ee7ebef`. No fixes have been implemented or deployed since that commit.

## 1. Workspace and state

### Checkpoint values to verify

These values are not automatic blockers.

| Item | Expected value |
| --- | --- |
| Checkout | `/home/voc/projects/snodec/core-ci` |
| Branch | `fix/controller-parallelism` |
| `HEAD` and `origin/fix/controller-parallelism` | `63ab7b207f55afb5a71c7b2f54d76fa05ee7ebef` |
| Reviewed base | `4257cb0fc67412da8696afdba307fc2e85ff64aa` |
| Known working tree | Modified: `README.md`, `WORK_LOG.md`, `deployment/sandbox/README.md`, `docs/deployment/README.md`. Untracked: `docs/improvements/`, the task review files under `docs/proposals/`, `secrets/`. |

**What you may do without new permission:**

- Read-only remote verification and reversible metadata lookup, such as `git ls-remote` or a fetch that only updates remote-tracking refs.

**How to handle differences:**

- Added task documents and harmless unrelated working-tree drift are not blockers. Record them and continue.
- Resolve routine source facts independently.
- Stop and coordinate only in these cases:
  - a genuine ownership conflict on the branch or source, such as foreign commits on the task branch or concurrent edits to the files you must change;
  - missing required authority.

**Forbidden git operations:** reset, switch, rebase or stash, and creating another checkout or worktree. Continue on the branch that you actually own.

### Preserving existing content

- **Unrelated hunks.** Preserve them in `README.md`, `WORK_LOG.md`, `deployment/sandbox/README.md` and `docs/deployment/README.md`. Never revert or reformat them, and never force them into a commit.
- **Task edits in those files.** This task appends task entries to `WORK_LOG.md` and makes the minimal M1 wording edit in `docs/deployment/README.md`. Edit only the task lines.
- **Existing task review records.** Preserve the existing `docs/proposals/controller-parallelism-*` records and `docs/improvements/`. A future explicit commit authorization may cover task-owned records or hunks.
- **Private directories.** `secrets/`, `.qtcreator/` and `.local-credentials/` are never read, listed or committed. Never inspect private configs, logs or credentials.

### Staging

- Use explicit paths only. Never use `git add -A`, `.` or directory globs.
- In files that also carry unrelated hunks, stage only task hunks non-interactively, for example with a hunk-limited patch via `git apply --cached`.
- Verify the result with `git diff --cached`.
- If task hunks cannot be separated cleanly, leave them unstaged and report it.

### Build and temporary locations

- **Existing probe evidence:** `/tmp/codex/build/core-ci/parallelism-findings/`. Reuse it and never overwrite it.
- **New focused checks:** `/tmp/codex/build/core-ci/parallelism-corrections/`. This holds proxies, namespace fixtures and logs.
- **Persistent builds:** `build/codex/<configuration>` only.
- **Reusable Debug build:** `/tmp/codex/build/core-ci/controller-parallelism/build`.
- **Caches:** point compiler caches and `TMPDIR` into the permitted task directories.
- **Build identity:** identify every reported build by `HEAD` plus a hash of the dirty diff.

## 2. Reading

1. `AGENTS.md` and any applicable global instructions. Binding rules include:
   - one graph/state authority and one event loop, with no extra production mechanism;
   - reduce obsolete code first;
   - braces always;
   - one final return per function;
   - no avoidable `break` or `continue`;
   - changed production lines no longer than 160 characters;
   - committed canonical formatting that passes the repository's format check.
2. `docs/proposals/controller-parallelism-fix-agreement.md`.
3. Cross-checks:
   - `controller-parallelism-design.md`;
   - `controller-parallelism-review-agreement.md`;
   - `controller-parallelism-implementation-review.md`;
   - `controller-parallelism-fix-discussion.md`.
4. Current code.
5. Probe evidence in `/tmp/codex/build/core-ci/parallelism-findings/`:
   - `code-validation.md`;
   - `transport-results.json`;
   - `argument-results.json`.

   These probes are synthetic, local and code-only. They are not live evidence.

**Withdrawn claims.** Never reinstate any of these:

- "synchronous disabled callback";
- a "missing no-op execution-ID guarantee";
- an "external webhook 503";
- "missing successful-rollback coverage";
- "callback-enqueued request uncovered";
- "a dual-stack localhost test is valid for AF_INET multi-address establishment".

## 3. Critical contracts

These contracts are reproduced once from the agreement. The agreement remains the authority.

### Findings and dispositions

| Finding | Agreed disposition |
| --- | --- |
| M1: O/N instructions | Qualify the already-identified first-install wording in `docs/deployment/README.md` so it cannot overwrite O. No further documentation audit. |
| H1: disabled client | The existing status callback handles DISABLED with a definite unsubmitted message and `finish(operation)`. |
| L1: phase arguments | Immediately after parsing, require phase-specific inputs (below). |
| L2: callback clearing | Replace the move with `std::exchange(callback, nullptr)`. No delivered flag. No test mirroring this line. Existing callback-count tests remain. |
| L3: shutdown text | For unstarted queued work, set the existing unsubmitted-shutdown message before `finish`. Do not relabel submitted requests. `submitted=false` already stays correct. |
| L4(a): response classification | Restore per-site responses through `preparationFailure` (below). |
| L4(b): latched no-ops/SSE | No change (below). |
| L4(c): built blockers | No change. `beginBuild` is used only for waiting work. A verified built node with free publication capacity remains publishable. |
| T1: coverage | Add focused reachable-case tests (§6). Distinguish unrun defensive checks and environment limits from covered behavior. No full suite and no production injection machinery. |
| I1: read callback order | No change. Keep release-before-read-callback order, which lets startup teardown destroy Database safely. Do not globally reorder reads to resemble writes. |

### M1 commit rule versus the local wording edit

The rule "the N tooling commit leaves O wrappers byte-identical; render N only into new aliases in a separate wrapper commit" governs **trusted worker deployment**. That is what the qualified wording must say. It does **not** require the local M1 documentation correction to be its own commit.

### H1 timing and ownership

- The installed `FlowController::startFlow` defers establishment to its next tick. DISABLED is therefore delivered **asynchronously**, normally after `client.connect` has returned and the application has installed its flow handle and hook.
- The earlier explanation, that the post-connect missing-membership branch cleans this disabled path, is withdrawn.
- `finish()` removes the operation, delivers its callback and releases capacity. Connector teardown releases framework captures. There is no active transport.
- Keep intermediate ERROR/FATAL establishment handling. Intermediate per-candidate address errors must not finish the operation, and you must not finish on every non-OK status.
- The existing membership and pump guards keep delivery once-only and reentrant-safe.
- Do not add a cancellation manager, destructor callback, lifecycle change, or a promise that every disabled flow emits termination.
- Cancellation without status during process shutdown is not a demonstrated running-service failure. The proposal allows callbacks to go undelivered across teardown and forbids calling Pipeline after its lifetime.

### L1 phase arguments

| Phase | Requirement |
| --- | --- |
| `prepare` | Requires the feed and the build-result/artifacts |
| `push` | Requires the feed. Do not require unused push inputs. |
| Confirmation | Needs neither a feed checkout nor artifacts |

Validate before assignment processing, identity, key handling and any network access. An empty feed must no longer resolve to the current directory.

### L4(a) response preservation

Reshape only the existing private helper:

```cpp
void change(const std::function<Node(const Graph&)>& derive,
            const Callback& callback,
            const PipelineResult& preparationFailure = {409, {}});
```

The asynchronous completion captures `preparationFailure` **by value**. On a preparation failure it:

1. copies the body;
2. sets `error`, which turns a null default body into an object;
3. sets `uncertain = result.uncertain` (false);
4. uses the captured status.

SQL failures keep `503` with `error` and `uncertain`. No new error taxonomy and no eager graph calculation.

| Site | Preparation response |
| --- | --- |
| `record` / `bindRun` | 409, actual error, `assignment_preserved:true`, `uncertain:false` |
| `buildArtifacts` missing/expired-result failure and built-result acceptance | 503, actual error, execution ID, `assignment_preserved:true`, `uncertain:false` |
| `publicationResult` published-receipt application | 409, actual error, `assignment_preserved:true`, `uncertain:false` |
| `rejectedDispatch` branches, complete-failed, before-push-failed | Existing default 409. No invented former classification. |
| `commitSelection` / `retry` / `advance` | Keep 409 / 409 / 422 |

**Execution ID:** the unchanged `record` response already includes its execution ID. Base `change()` had no no-op ID guarantee, so no identifier restoration is needed.

**Catches:**

- Remove the two obsolete dispatch `try`/`catch` blocks around the formerly eager `rejectedDispatch` derivation.
- Retain the catches that still validate JSON, identity, artifact metadata or current attempt state. Name each retained catch and what it guards.

### L4(b) latched no-ops

- Mutation uncertainty blocks producers before derivation.
- Latched no-ops keep internal `503` / `uncertain:true` and failed-mutation SSE, with no SQL, dispatch or evidence traffic. These include an unchanged report and a no-work advance.
- No eager duplicate path or cache.
- Record this as an observable difference. It is not byte-identical behavior and not an owner waiver.
- The external authenticated webhook ACK stays `202`.

### Retained policies

- P1 independent publication remains bounded. Approved limits: eight HTTP operations per client and eight publication assignments per feed.
- Unknown nodes keep their assignments and block their dependents. Independent work may proceed within the approved capacity limits; uncertain assignments still consume their slots.
- Before mixed O/N rollout at capacity above one, old/new workflow-level and job-level feed-group exclusion needs real sandbox proof. Matching group strings are not that proof.
- These each need separate measurements, and no latch, debounce or timer is approved to hide them:
  - status churn;
  - response-to-transport-close occupancy;
  - graph/DB/SSE timing;
  - runner setup;
  - Pages propagation.
- Selection and enrollment generation guards stay unchanged. Do not replace them with an epoch.
- Signing and setup stay within the privileged feed lock.
- Performance targets and owner acceptance remain separate from reviewer agreement.

## 4. Line accounting

**Bounds** (cumulative, relative to `4257cb0`, net = added − removed):

| | Production | Tests |
| --- | --- | --- |
| Approved limit | +310 | +790 |
| Current | +200 (652 added / 452 removed) | +282 (398 added / 116 removed) |
| Headroom | +110 | +508 |

**Procedure:**

1. Before the first edit, reproduce the current figures with `git diff --numstat 4257cb0` over the repository's production/test path classification. For reproducing these figures, classify `src/`, `deployment/worker/` and `CMakeLists.txt` as production, `tests/` as tests, and documentation separately. This includes C++, worker Python, workflow YAML and build/test registration. Keep this classification fixed.
2. If you cannot reproduce the figures, reconcile the method and report it. Do not silently adopt a different method.
3. Include working-tree changes in every measurement.
4. Re-measure after the code group and at handover.
5. Re-estimate and report before a change would exceed either headroom.

The separately approved sandbox bound of +450 net production covers later sandbox copies and aliases. It is not available for controller code, and controller code does not count against it.

## 5. Implementation

**Baseline:**

1. Discover the actual build, format-check and test target names, for example with `cmake --build <dir> --target help`, `ctest --test-dir <dir> -N` or the documented format target. Never fabricate target names.
2. You may reuse the recorded results of the 18 existing focused checks if you verify that the build identity matches the exact current source. Otherwise establish the relevant baseline once.
3. Baseline results are not validation of new code.

Apply the changes as coherent groups, with reductions first:

1. **L4(a) reduction:** remove the two obsolete catches and record which catches are retained and why.
2. **Transport:** L2 `std::exchange`, H1 DISABLED handling and L3 queued-shutdown wording.
3. **L4(a) `change()`:** reshape the helper and apply the per-site responses.
4. **L1 worker validation.**
5. **M1 wording:** the minimal edit in `docs/deployment/README.md`.
6. **Confirmation only:** confirm L4(b), L4(c) and I1 against the source without editing.
7. **Format and rules:** apply canonical formatting and run the actual format-check target. Check every changed production line against the `AGENTS.md` rules from §2.
8. **WORK_LOG:** append task-specific decisions and evidence to `WORK_LOG.md`. Documentation you change describes the current state only, with no attic or legacy narrative.

There is no build-and-retest requirement after each small edit. Build and test as described in §7.

## 6. Focused tests

**Rules:**

- Tests assert outcomes at public boundaries. They must not mirror the implementation.
- Use existing harnesses and public fixtures. Test-side controls are allowed.
- Keep every existing test.
- For each item below, either implement it or record the precise feasibility or environment limit. An unrun check never counts as coverage.

### 1. Transport: disabled client (H1)

A disabled instance delivers exactly one callback, asynchronously after `connect` returns, with:

- status 0;
- `submitted=false`;
- the definite unsubmitted message;
- no token request and no server request.

Also:

- **Capacity 1:** twelve correlated requests complete with one token and a peak of 1. This already covers callback-enqueued requests, so add no duplicate test for them.
- **Invalid capacities:** 0 and 17 are rejected.
- **Keep:** the capacity-8/reverse-response, expiry, TLS identity and mutation non-replay tests.

### 2. Queued shutdown (L3)

Check the wording when the existing shutdown path delivers the queued operation:

- Unstarted queued work reports the unsubmitted-shutdown wording with `submitted=false`.
- Submitted work keeps the unknown-outcome wording.

Do not assert or promise delivery of every callback during process shutdown or after the owner's lifetime. The agreement excludes that guarantee.

### 3. Feed-disabled publication

Only where the existing publication fixture permits:

1. disable feed-http;
2. assert that the publishing assignment is retained;
3. assert that a second reconcile completes, which proves `completionReads` was released.

Shared-branch source reasoning is not this runtime test and must not be labelled as such.

### 4. Options, lifecycle and pipeline

- Publication limits 0 and 33 are rejected.
- Capacity 1 visibly blocks a second built node.
- A definite terminal result frees capacity.
- Keep the default-8 and restore-under-lower-limit checks.

### 5. Worker provenance (L1)

Each of these fails before any assignment, identity, secret or network access, and the test asserts it at that boundary:

- `prepare` without a feed;
- `prepare` without build-result/artifacts;
- `push` without a feed.

Confirmation succeeds without a feed or artifacts. `push` does not demand unused inputs.

### 6. Response race (L4a)

1. Hold the graph-revision lock while two authenticated run claims for one unbound assignment queue up.
2. Release the lock.
3. Assert:
   - the `bindRun` conflict message, distinguishable from the earlier rejection text;
   - the first assigned run is retained;
   - no replacement dispatch occurred.
4. Inspect separately:
   - the logged internal outcome;
   - the external webhook `202`.

Also assert the per-site bodies the public fixtures reach:

- `assignment_preserved:true`;
- `uncertain:false`;
- the execution ID on the `buildArtifacts` 503.

### 7. Delayed approval

While same-state graph evidence keeps committing:

1. the captured-minimum dispatch proceeds;
2. the latest generation follows after settling;
3. no duplicate unchanged-generation POST is sent.

Callback order is not evidence of remote job arrival or of Pages order.

### 8. Stalled TLS and response shutdown

Assert:

- clean exit with no crash;
- durable restoration;
- no replacement mutation after restart.

Exercise a held connect phase if a real local fixture can do so. A normal exit alone does not prove the absence of use-after-free. Instrumented or sanitizer evidence is separate and optional; build it under the permitted directories and label it as such.

### 9. Real MariaDB proxy (START / ROLLBACK)

**Proxy construction:**

1. Capture the actual START and ROLLBACK wire text.
2. One-shot rewrite each targeted `COM_QUERY` into invalid SQL. Recompute the 3-byte payload length and preserve the sequence ID. Equal-length replacement is allowed but not required.
3. Verify the interception count.

**START failure:** definite and unlatched; a subsequent write succeeds.

**ROLLBACK failure:** latches uncertainty and prevents further producers and mutations. The failed rollback deliberately leaves a transaction open, so:

1. Before relying on in-memory reads, verify from source that `GraphStore::view` (or the equivalent read) consults `mutations_allowed` only from memory.
2. If the read touches the DB, take independent facts over a separate root connection with non-locking reads.
3. Do not rely on DB-backed application reads while the transaction is open.

**Cleanup:**

1. close/restart so the server rolls back;
2. restore permissions;
3. make the explicit named assertion that no graph revision leaked;
4. clean up the proxy and fixtures.

### 10. Preserved coverage

Keep and run these existing checks:

- successful rollback;
- lost successful COMMIT acknowledgement;
- GraphStore evidence/built/advance ordering;
- real-MariaDB pipeline concurrent reports.

Add a forced artifact-result overlap test only if the public fixture can hold it meaningfully, with no production hook.

### 11. Multi-address (optional)

Multi-address establishment is real, reachable behavior. However, `tls::in` uses AF_INET, so a dual-stack localhost test cannot establish it.

If supported, use a private user/mount namespace with a controlled hosts file that supplies multiple IPv4 candidates. Read the actual resolver order inside the namespace instead of assuming the first candidate. No global DNS change and no framework change.

Otherwise record the precise environment limit and claim nothing.

### 12. Source dispositions only

These are **not runtime coverage** and not owner waivers. Report them as unrun:

- **Active-origin switching:** `FeedProof` requires `selected.origin==configured.origin`, and the configuration is fixed per process.
- **Late orphan `onConnect`:** excluded by status/`onConnect` adjacency in the traced path.

The initial feed-origin setup is exercised. The proposal's "synchronous disabled connect" label is wrong for the installed framework; item 1 exercises the actual asynchronous delivery.

### Feasibility items

These remain test-feasibility work until done, and are not completed checks:

- connect-stall control;
- namespace support;
- actual ROLLBACK wire text;
- the feed-disabled fixture;
- forced artifact overlap;
- sanitizer evidence.

## 7. Verification

- Build the final source and run the relevant selected checks, using the actual discovered target names:
  - the affected existing checks;
  - the new focused tests.
- Do not run unrelated or full suites.
- Repeat an already-passing check only for a reason, such as a source change affecting it.
- Register tests so that each runs and reports independently and no failure suppresses another.
- Re-run the existing zero-request duplicate-burst check and keep it unchanged. The earlier 0.0919 s figure is historical.
- Report status, Pages, runner and controller timings separately, labelled local and synthetic. Never present a local synthetic number as live throughput.

## 8. Uncertainty and stop rules

### Uncertain outcomes

When the outcome of a remote or assigned mutation is uncertain:

- never replay or replace that mutation;
- keep its dependent steps blocked until the necessary evidence exists;
- report the uncertainty;
- continue the independent fixes, tests and authorized work.

This follows P1.

### Stop or coordinate only for

- a genuine branch or source ownership conflict;
- a change that would exceed the remaining headroom, or would need an unagreed mechanism, framework change, production hook or per-origin refactor;
- code that contradicts a factual premise the work depends on (stop only the dependent work);
- any step that would touch production SNodeC workflows, credentials, keys, tags, feeds or deployed state;
- a stage that lacks actual authorization (stop at that stage only).

## 9. Commit and later stages

1. Self-review the full diff against `63ab7b2` and against `4257cb0`.
2. Commit only under an existing authorization that covers committing on this branch.
   - Stage explicit task paths and hunks only (§1).
   - The M1 wording may share a commit with the code.
3. Push, merge, install, restart and activation each need their own actual authorization, checked at that stage. Otherwise report the stage as not performed.

## 10. Handover

Deliver one report. Each item below is also a completion criterion.

1. **State:**
   - the verified starting checkpoints and any drift;
   - the final `HEAD` and working-tree status;
   - confirmation that unrelated hunks, review records and private directories are untouched (never list private contents).
2. **Per finding** (M1, H1, L1, L2, L3, L4a, L4b, L4c, T1, I1): what changed or why nothing changed, with `file:line` references. For L4a, include:
   - the per-site table as actually implemented;
   - the deleted catches;
   - the retained catches and what each guards.
3. **Line accounting:**
   - production and test added/removed/net relative to `4257cb0` and relative to `63ab7b2`;
   - remaining headroom against +310/+790;
   - the classification method;
   - confirmation that the sandbox +450 bound is untouched.
4. **Tests:**
   - exact commands and actual target names;
   - pass/fail, with output for failures;
   - which tests are new and which pre-existed;
   - build identity (`HEAD` plus dirty hash);
   - confirmation that all relevant selected tests ran.
   - Baseline checks and 0.0919 s are never presented as validation of the new code.
5. **Not run and limits:**
   - each §6 item not implemented, with the precise reason;
   - the §6.12 source dispositions, labelled as not runtime coverage;
   - any uncertain outcomes and the dependents still blocked.
6. **Measurements:** the duplicate-burst result and the separately labelled local/synthetic timings. No live claim.
7. **Observable differences:** the L4(b) latched no-op 503/SSE behavior and the unchanged external 202.
8. **Open gates:**
   - mixed O/N feed-group sandbox proof before a capacity above 1;
   - live timing and performance targets;
   - the sandbox alias work under its own +450 bound;
   - owner acceptance;
   - every commit, push, merge, install, restart or activation not performed.

   No invented owner waivers and no claim of owner acceptance.
