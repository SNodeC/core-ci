# Claude review of the OpenWrt 24.10 prerequisite 404

The owner requested this independent detailed review on 2026-10-31. Two
read-only Claude Code calls used model `claude-opus-5-5`, as reported by the
CLI. Tools, customizations and MCP access were disabled for both calls. The
first received a curated packet of current fresh-controller source, contracts,
frozen executions/receipts, the failure traceback and public GitHub job/workflow
records. The second received the first report, the exact pinned receipt-check
implementation and recorded native job evidence, and corrected its initial
controller-only recommendation. Claude performed an offline evidence review;
it did not independently execute GitHub requests, tests or live changes.

Raw requests, CLI results and supplied public contract extracts are retained in
`/tmp/codex/build/core-ci/claude-publication-review-20261031`.
The source/build identities remain those in [the run report](core-v2.1.17-run.md).
No controller, worker, workflow, test or runtime changes were made for this
review. No retry, reconciliation, restart or new build was triggered.

## Integration notes

The decisive findings were checked against the supplied source and job records:
release currently follows the matched publisher job and committed receipt;
OpenWrt confirmation completes later; native confirmation is skipped. The race
explanation is strongly supported, with the failing URL and exact public edge
activation still unknown. Receipt-read/authorization timestamps are observations,
not separately measured durable-commit completion timestamps.

The report below is Claude's review, including proposals rather than approved
changes. Two qualifications apply to its alternatives:

- A controller-side HTTPS check does **not inherently require** a new timer or
  feed webhook: the existing relevant authenticated publication notifications
  and explicit reconciliation can trigger observations. Whether they provide
  sufficient subsequent opportunities requires tracing, not an assumption.
- Neither a receipt observation nor the end of the Pages action establishes
  simultaneous package availability at every CDN edge. No such guarantee was
  measured or established here. The report's subsecond polling-time inference
  is also limited: elapsed step time does not reveal the number or duration of
  failed requests.

The proposed workflow condition and controller gate require implementation,
focused validation and authorization for the changed publication authority and
sandbox workflow/pin mutation. This document authorizes none of those actions.
A generic gate must cover native and OpenWrt; treating skipped native confirmation
as success would retain the same availability gap.

## Claude's revised report (verbatim)

# Independent review (revised): OpenWrt 24.10 demo-join prerequisite 404

*This revision uses only the first report and the evidence you supplied. Tools were not used and nothing has been newly verified. Code references from the first report are carried over as previously read.*

## 0. Corrections to the first report

| First report | Correction |
|---|---|
| "85–90% confidence" | Removed. There is no basis for a number. The cause is strongly supported, not proven. |
| "I make no claim about native Confirm behaviour" | Native Confirm is **skipped** in every supplied native run (see §4). That changes the recommendation. |
| "Run success effectively means Confirm success" | True only for OpenWrt. For native, run success occurs with Confirm skipped. |
| Option A: "require Confirm success", with no workflow or pin change | Unsafe on its own. With the current blob, it would leave every native publication stuck in `publishing`. |
| Confirm timeout "15-minute" | The script deadline is 600 s with 10 s sleeps. The job timeout of 15 min is an outer bound. |
| "Confirm polling semantics unknown" | Now known: it checks the receipt only (§4). |
| Option C "is a retry, which the owner has excluded" | Overstated. The ban concerns automatic execution, mutation and dispatch replay. A read-only GET retry is a different thing (§5). |
| Core `.ipk` loss needs "a non-atomic swap" | Too narrow. An external writer, a force-push, an older deployment or controller, or a Pages rollback could also remove files. Retention never being selected by *this* controller does not exclude those. |
| "The consequence of a Confirm failure needs explicit owner approval" | Not needed. Leaving the attempt in `publishing` with uncertainty is the already-approved uncertainty-preservation behaviour. What needs authorization is the **release-authority change** and the **workflow/pin edits**. |
| "The race is latent in every release" | Too categorical. For OpenWrt it is latent under the current contract. For native, no job checks public availability at all. |

## 1. Verdict and findings

**Proven from the supplied records:**
- Each of the three prerequisites was recorded `published` before its own Confirm job completed:
  - core: 18:11:07 vs 18:11:22
  - side: 18:15:22 vs 18:15:50
  - app: 18:15:37 vs 18:16:50
- The join's 404 at 18:16:42.341 happened while the Pages deploy step for `5fe31ca` was running (18:15:56–18:16:45).
- The 404 was an `HTTPError` from the Pages origin, raised on a package download (`native.py:35`), not on an export download.

**Strongly supported, not proven:**
- The cause is Pages not yet serving the app commit's objects at the runner's edge.
- The failing URL was not logged. There are no edge timestamps or response headers.

**F1 (High): release authority excludes public availability, by contract.**
- Authority is the `Publish <label>` job's success plus the committed receipt (`src/Execution.cpp:247-268, 311-315`).
- The release happens in the same change (`src/Graph.cpp:397-406, 429-432`), with commit before dispatch (`src/Store.cpp:81-86`).
- This matches `docs/contracts.md:449-457`. The earlier contract had warned of this gap (`docs/contracts.md:252`).

**F2 (High): OpenWrt releases precede Confirm.**
- Shown for the three prerequisites above.
- Earlier successful runs may have had timing margin. That is an inference; per-execution data was not checked.

**F3 (High, new): native publications have no public check at all.**
- In the pinned blob `e4ca49f…`, `confirm` is skipped for native targets (§4).
- If native dependents fetch prerequisites from the public feed, the same race applies to them with no workflow-side signal. That is conditional; native dependency fetching was not examined here.

**F4 (Medium): the failing object is narrowed, not identified.** It was one of the core, app or side `.ipk` files (§3).

**F5 (Medium): even where Confirm runs, it proves receipt bytes only** (§4).

**F6 (Low): the dependent download is a single attempt.** This turned the boundary gap into a definite build failure.

**F7 (Low): naming can mislead.** `proof.verified_at` and `verification_started_at` record when the committed receipt was read, not any public verification.

**F8 (Low): provenance caveats.**
- The excerpt labels (`dependency-worker.py`, `download-contract.py`) differ from the traceback names (`native.py`, `contract.py`). The line numbers agree.
- The job and Pages records are current API reads, not snapshots taken at failure time.

## 2. Causal chain

The chain is unchanged from the first report:

1. Publish job success at 18:15:30.
2. Webhook matched; run authenticated against blob `e4ca49f…` and `run_attempt == 1`.
3. Committed receipt read at `5fe31ca` (18:15:36–37).
4. Same transaction: app set `published`, join authorized.
5. Commit, then dispatch (accepted 18:15:41).
6. Pages deploy 18:15:56–18:16:45.
7. Join step 7 got the 404 at 18:16:42.341.
8. App Confirm step succeeded at 18:16:47.

**New supporting detail (inference):**
- The app Confirm step took about 71 s with 10 s sleeps. So several polls did not verify before success.
- This is consistent with the receipt not being served with the expected hash at the Confirm runner's edge until roughly 18:16:3x–18:16:4x.
- It is the same commit as the app `.ipk`, but a different object, a different runner and possibly a different edge. Confirm also sends `Cache-Control: no-cache`; whether the worker does is not shown.

## 3. Which object, and alternative causes

**Download order:** core `.ipk`, core export, app `.ipk`, side `.ipk`, side export. The failure at line 35 means a package file.

**App `.ipk` is the likely candidate (inference).** It is the only object whose deployment was still in progress. Its Confirm completed about 5 s after the 404.

**Core `.ipk`:** unlikely but not excluded.
- Its receipt was confirmed public at 18:11:19.
- Removal would need an external writer, a force-push, an older deployment or controller, a Pages rollback, or non-atomic swaps. None is evidenced, and none is excluded.

**Side `.ipk`:** would be reached only after the app `.ipk` succeeded. That points away from side, assuming `5fe31ca` descends from `57060f0`. This is likely given the publish concurrency group, but not proven.

**Other causes:**
- **Wrong URL or encoding:** the later HEAD 200s on the same base and paths make a permanent bad URL much less likely. They rule out transient URL or edge issues much less strongly.
- **Same-origin redirect to a missing target, negative or stale CDN cache, transient Pages error:** possible, not excludable.

**Temporal limits:**
- The HEAD 200s at 18:18:38 show availability two minutes later, on the observer's edge, without checking bytes.
- The end of deploy-pages is not the activation instant at every edge.

**To strengthen the diagnosis (read-only, if permitted):**
- Deployment status timestamps for `github-pages` covering `5fe31ca` and its predecessor.
- Any overlapping Pages deployments between 18:11 and 18:16.
- Per-download logging, if any exists.

## 4. Confirm: exact contract and per-target behaviour

### What `wait_for_https` proves (publish.py:146-168, 202-205)

Mechanics:
- It requests `base_url/receipt_path` with `Cache-Control: no-cache`.
- Each request has a 30 s timeout. Every `OSError` (including `HTTPError` 404) counts as unverified.
- It sleeps 10 s between attempts, up to a 600 s monotonic deadline.
- On failure it stops with "reconcile before replacing this work".

Success requires all of:
- a body of at most 2 MiB;
- a final URL under `base_url/`;
- `sha256(body) == receipt-sha256` from the publish job's output.

`feed_revision` is only format-checked and printed. It is **not** compared with the deployed revision.

**So Confirm is a receipt-only hash proof.** At one instant, from one runner and edge, the exact prepared receipt bytes were served.

It does **not** check:
- package files;
- mutable indexes (`Packages`, `Release`, repodata, signatures), which later writers overwrite;
- other edges or later instants.

Package availability follows only if all of these hold:
- (a) a deployment is an atomic whole-site artifact;
- (b) edges converge before a dependent fetches;
- (c) later deployments retain earlier files.

None of these is guaranteed by Confirm. There is **no automatic package-availability guarantee across edges.**

### Per-target behaviour in blob `e4ca49f…`

`confirm` has `needs: publish` and `if: kind != 'retention'`, with no status function. The implicit `success()` applies.

- **OpenWrt:**
  - Path: `validate` → `tools` (success) → `publish` → `confirm`.
  - Confirm runs (observed for core, side and app).
- **Native:**
  - `tools` is skipped by design.
  - `publish` runs only because its `if` uses `!cancelled()` and accepts `tools == skipped`.
  - `confirm` lacks `!cancelled()`, so the skipped ancestor makes its implicit `success()` false.
  - Evidence: debian-trixie-amd64, debian-trixie-arm64 and fedora-43-x86_64 all show `Confirm public receipt`, `completed`/`skipped`, `steps: []`, finishing in the same second as Publish.
  - The mechanism is inferred from GitHub's documented status-check semantics. The skip itself is observed.
- **Retention:** Confirm is skipped by its own condition. Retention is not selected by this controller, so its release rule is out of scope. It would need its own rule if retention is ever introduced.

Consequences:
- **Controller-only gating on Confirm success is not safe for all targets.**
  - Native attempts would never release and would stay `publishing` with uncertainty.
  - Accepting `skipped` for native would keep today's unproven boundary.
- **A run-conclusion gate is no better for native.** Under this blob, native runs conclude `success` with Confirm skipped. It would silently keep native unproven.

### Rerun note

Authentication requires `run_attempt == 1`. A failed Confirm therefore cannot be cured by a GitHub job rerun. It remains an uncertainty for explicit owner reconcile, which matches the existing uncertainty-preservation rule.

## 5. Recommended minimal route (no code changes made)

### Route R: run Confirm on every non-retention target, and gate release on the run's own Confirm

**R1. Workflow edit (needs authorization: workflow editing, new blob, controller pin update).**
- Change the `confirm` condition to: `!cancelled() && needs.publish.result == 'success' && kind != 'retention'`.
- This makes Confirm run for native and OpenWrt alike.
- Keep `confirm` **outside** the publish `concurrency` group. The existing serialization boundary, publish-only with `queue: max`, is then unchanged. Confirms of different writers run independently and do not hold the feed lock.
- `CORE_CI_TOOLING_SHA` is unchanged, because `publish.py` is untouched.
- Native Confirm has never executed. Whether `receipt-sha256` and `feed-revision` outputs are populated for deb/rpm is unverified, so the first native runs are the acceptance test.

**R2. Controller predicate (needs authorization: release-authority change to `docs/contracts.md:449-457`, plus matching updates to `AGENTS.md:78-81` and WORK_LOG).**
- In the success path (`src/Execution.cpp:311-315`), require both:
  - the matched `Publish <label>` job succeeded;
  - exactly one `Confirm public receipt` job in the same authenticated run and attempt is `completed`/`success`.
- Only then read the committed receipt. The receipt read, execution and attempt matching, same-transaction release and commit-before-dispatch stay as they are.
- Name the Confirm job explicitly rather than gating on run conclusion. This **fails closed** if the controller is ever run against the old blob: native stays `publishing` instead of releasing unproven.
- A Confirm that is `skipped` or `failure` while Publish succeeded means no release and no definite failure. The attempt stays `publishing` with uncertainty, which is already-approved behaviour. Graph's definite-failure test (`src/Graph.cpp:411-418`, keyed on the Publish step being skipped) is unchanged.
- Release is driven by the Confirm `workflow_job` or `workflow_run` completed webhook through the existing `publish_run_id` match (`src/Execution.cpp:356`). There is no polling, timer, lock, cache or replay.
- Size: a few lines of net growth. This is an unmeasured estimate and stays well under the 2,500-line limit.

**R3. Transition (operational, needs authorization).**
- Deploy the new blob pin and the controller predicate together.
- Let in-flight old-blob publications drain first, or accept that old-blob native runs will stay `publishing` with uncertainty for owner reconcile.
- Do not assume the controller can accept two pins at once. Whether it can was not established.

**Release-authority statement for the new contract:** "matched pinned run, `Publish <label>` success and that run's `Confirm public receipt` success, plus the committed receipt; Confirm is a receipt-hash observation, not a package-availability guarantee."

**Independent work:** each attempt waits only on its own prerequisites' Confirm events. Unrelated chains, for example 25.12 alongside 24.10, are unaffected.

### Conditional alternatives

**C1. Workflow edit not authorized.** Gate OpenWrt on Confirm success, and keep native on today's boundary.
- This is a target-family rule using the frozen assignment target.
- It is still a release-authority change for OpenWrt.
- The native race stays an explicitly documented, owner-accepted gap.

**C2. Stronger Confirm (tooling change, `CORE_CI_TOOLING_SHA` pin update).**
- Extend `wait_for_https` to also fetch the receipt-listed immutable package files and check their hashes.
- Do not check mutable indexes; later writers change them legitimately.
- This narrows the gap but still observes only one edge.

**C3. Bounded read-only GET retry in the dependent download (tooling/workflow pin change).**
- This is not the dispatch, execution or mutation replay the owner banned. Confirm itself already does read-only polling.
- It still needs agreed semantics:
  - which statuses qualify (404/5xx versus others);
  - a bound;
  - hash verification of the bytes;
  - how exhaustion is classified.
- It is secondary hardening against edge skew. It is **not** a substitute for a correct release boundary.

**Not recommended: a controller-side HTTPS proof.**
- It needs either a timer (forbidden) or feed-repository webhooks, which expand authorization and `deployment.repositories` (`src/Execution.cpp:333-335`).
- It duplicates Confirm.

## 6. Tests and evidence needed

These are controller fixtures with no live resources.

**Gating:**
- Publish success with Confirm queued or in progress → no receipt read, no state change, no commands.
- Confirm success → published, and dependents released in the same transaction; dispatch only after a successful UPDATE.
- Confirm `skipped` with Publish success, for both native and OpenWrt → stays `publishing`; no release, no definite failure.
- Confirm `failure` → same as skipped.

**Old-blob fixture:**
- A native run in the shape of the supplied records (Confirm skipped) is never released.

**Rejections:**
- Two Confirm jobs, or a Confirm from another run or attempt → rejected.
- Empty commits after Confirm → "Publisher receipt unavailable".
- Receipt execution or attempt mismatch → graph unchanged.

**Ordering:**
- Confirm event before `workflow_run` completed, and duplicate deliveries → exactly one release and one dispatch. This relies on `Store::change` re-reading (`src/Store.cpp:69-75`) and on single-connection serialization, which should be tested.

**Isolation:**
- Later writer B confirms before A's Confirm → only B's dependents are released.
- An independent chain is released while another chain's prerequisite awaits Confirm.

**Restart:**
- A persisted `publishing` attempt is released exactly once by the next relevant webhook or an explicit reconcile.
- There is no startup reprocessing, no replay, and no re-evaluation of already-published history.

**Live acceptance (separate authorization):**
- The first native runs on the new blob show Confirm actually running and succeeding.
- For every release, `build_authorized` is at or after each prerequisite's Confirm `completed_at`.
- Report Pages deployment status times alongside. Do not claim edge consistency was verified.

## 7. Plain-language summary

The join build was started as soon as the demo-app package had been committed to the feed's Git branch. GitHub Pages was still deploying that commit. The build asked for a package a few seconds before the deploy finished and got "not found"; two minutes later all three files were reachable. That this is the cause is strongly supported but not proven, because the failing URL and the edge timing were not recorded.

The publisher workflow has a "Confirm public receipt" job that waits until the exact receipt is served over HTTPS. The controller ignores it, by current contract. For Debian and Fedora, that job never runs at all: a workflow condition skips it.

The minimal coherent fix has two parts:
1. Let Confirm run for every target. This is a workflow edit and pin update, and needs authorization.
2. Have the controller release dependents only after that run's Confirm succeeds. This changes release authority and needs authorization.

Confirm proves only that the receipt was visible once, from one place. It narrows the race; it does not guarantee every package is visible everywhere. A bounded download retry in the dependent build could be added later as extra hardening, if its rules are agreed.

| UTC | Event |
|---|---|
| 18:15:28 | App feed commit `5fe31ca` pushed |
| 18:15:30 | App publish job succeeds |
| 18:15:36 | App Confirm starts polling (10 s interval) |
| 18:15:37 | Controller reads the committed receipt and releases the join |
| 18:15:56 | Pages deploy step starts |
| 18:16:42.34 | Join gets 404 on a prerequisite `.ipk` |
| 18:16:45 | Pages deploy step ends |
| 18:16:47 | App Confirm verifies the receipt hash |
| 18:18:38 | HEAD returns 200 for all three `.ipk` URLs |
