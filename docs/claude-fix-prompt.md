# Task: Verify and correct workflow review findings F1–F4, then triage F5–F14 (core-ci, feat/persistent-package-graph)

## Status of this prompt

This prompt is advisory. It is not owner approval, and it does not claim that any finding is fully verified. Where it conflicts with `AGENTS.md`, the global engineering instructions or an explicit owner decision, those win.

## Context and authority

Work in the canonical checkout `/home/voc/projects/snodec/core-ci`.

**Branch and starting point**
- The reviewed implementation starting point is `ec29760`.
- The current task branch `feat/persistent-package-graph` contains later documentation commits on top of `ec29760`. Preserve them.
- Confirm that the working tree is clean. Confirm with `git log ec29760..HEAD` that the later commits are documentation-only.
- Do not switch, detach, reset or rebase to `ec29760` or any other hash. Do not create another checkout. Do not recover discarded implementation.
- This review prompt may be committed next, as its own documentation commit, under whatever commit authorization exists.

**Directories and private files**
- Temporary checks go in `/tmp/codex/build/core-ci/<task>`.
- Persistent builds go in `build/codex/<configuration>`.
- Preserve `.qtcreator/` and `.local-credentials/`. Do not inspect or commit their contents or any secret.

**Current verified state (do not regress)**
- The public sandbox feed is `https://voc-experiments.github.io/packages/`.
- It has 29 retained receipt rows and 20 published v2.1.0 graph nodes.
- Real package-client diamond checks passed on 5 targets.
- Public TLS for the approved webhook awaits valid certificate-chain and private-key paths.
- The new renderer's integration with the publisher still needs real build proof.

**Input.** The input is `docs/claude-workflow-review.md`. It is an advisory review produced without working tools. Its line numbers, classifications and proposed fixes are unverified. A read-only inspection confirmed only the source structures behind F1–F4:
- latest-attempt run/job endpoints combined with attempt-1 assertions;
- render and token minting inside the push step;
- selection reading every enrolled repository;
- the assignment-size `require` throwing inside the advance loop.

No failure scenario has been reproduced live. No proposed fix is pre-approved.

## Binding rules

`AGENTS.md` and the global engineering instructions remain binding.

**Approval and scope**
- Trace the affected behavior and state the invariant before any production change.
- Reduce or reshape existing structure first.
- Local `AGENTS.md` requires explicit owner approval for net production growth from fixes, and for any architectural or behavior extension. Present a design and estimate, then stop and ask for that specific item.
- The original implementation approval (7000–10500 production lines, 2500–4000 test lines) and the status-renderer approval do not exempt these fixes.
- Do not infer that any review recommendation is approved.
- One coherent change per commit. Preserve unrelated changes.

**Code rules**
- One return at the end of each function.
- Braces for every compound block.
- No avoidable `break` or `continue`.
- Changed production lines are at most 160 characters.
- Use the committed canonical formatting and `format-check`.

**Runtime rules**
- No controller threads and no blocking work in event handlers.
- Study the SNode.C `src/apps` and MQTTSuite lifecycle and ownership patterns before touching framework-facing code.
- Use installed public SNode.C package targets only.

**Records**
- Record significant decisions and verification in `WORK_LOG.md`.

**Existing owner policies (preserve unchanged)**
- Enrollment immediately selects the highest numeric SemVer, including prereleases.
- Supersede older waiting automatic selections only.
- A missing prerequisite execution is an explicit work error. There are no automatic prerequisite builds and no upward autobuild.
- Explicit older source requests are supported.
- A successful retry may become the newest rebuild of that source version.
- Runtime dependency ranges remain source-compatible; consumed build artifacts are exact.
- Roots choose the latest official SDK; dependents reuse the pinned prerequisite SDK.
- No retry of any uncertain mutation.
- The controller stays slim: no controller registry or SDK clients. The shared HTTP-client refactor stays deferred.

**Preserve throughout**
- Exact source and input identities.
- Signed publication.
- Existing tags, trusted keys and previously published package bytes.
- Recorded uncertainty.
- Persisted graph history.
- All currently successful behavior.

**Out of scope**
- A new orchestrator, queue, worker-registry client in the controller, auth system, timer or retry policy, catalogue or engine.
- New version codecs or comparison algorithms.
- Blanket automatic retries.

## Authorization and stages

The owner has already authorized, within this session's scope:
- sandbox deployment, live builds and publication in VOC-Experiments;
- TLS activation;
- the status renderer.

Do not create a new blanket deployment-approval blocker. For each concrete change, decide whether it falls within that authorized scope. Examples are redeploying changed worker tooling or the workflow blob, and updating the matching approved blob/tooling pins in the sandbox controller configuration.

- **If it is within scope:** prepare reviewable changes, then execute the already-authorized stages.
- **If it is not within scope:** ask only for the inputs that are actually missing, or for the genuinely new code-growth or architectural approvals that `AGENTS.md` requires.

Keep these stages separate:
- Commit, push, and merge/install each require their own actual authorization. Verify that authorization; do not assume it from the others.
- Do not open a PR, merge, install or expose the UI unless that is authorized.
- External work goes to VOC-Experiments only. Never touch production SNodeC workflows, credentials, feeds or tags.

## Step 0 — Verify before changing (F1–F4)

For each finding:
1. Trace all consumers at the current code: controller, worker modules, workflows and tests.
2. Confirm or refute the defect.
3. State the invariant.
4. Classify it as one of:
   - definite defect;
   - hypothesis needing official API or live evidence;
   - owner-controlled setting or policy.

Confirm every GitHub API assumption against the official GitHub REST and Actions documentation, and cite the pages in `WORK_LOG.md`. Specifically check:
- The semantics of `GET /repos/{o}/{r}/actions/runs/{id}` (latest attempt?) versus `GET .../actions/runs/{id}/attempts/{n}`.
- The default `filter` of the jobs listing, and `.../attempts/{n}/jobs`.
- Whether job and artifact objects identify their attempt.
- How a running step learns its own attempt, for example `GITHUB_RUN_ATTEMPT`.
- `workflow_dispatch` input limits: maximum input count and total payload size. Do not rely on "about 65,535" without a source.
- Whether a run completed or cancelled before any job started reliably reports `total_count == 0`.

If the documentation is ambiguous, mark the item as a hypothesis that needs sandbox evidence. Do not encode a guess.

## Priority 1 — F1: re-run of a dispatched run

**Current behavior.** The controller and worker read latest-attempt run and job views but require attempt 1. The affected readers are:
- `Pipeline::inspect`, `buildArtifacts` and `publicationResult`;
- the `search` title matching;
- worker `github.assigned_run`, the `environment.previous` jobs listing and `publish.run_identity`.

**Keep two concerns separate.**

**(a) Reading old evidence (controller).** A UI re-run must not wedge an attempt or the feed writer. The controller must keep evaluating the immutable attempt-1 evidence:
- identity, status, conclusion, jobs and steps;
- artifact binding.

Use attempt-specific endpoints only if they are verified. Take status and conclusion from attempt 1, not from the latest view. Verify that the artifact and job bound for publication belong to attempt 1. Evidence from attempts ≥2 is never accepted as execution evidence.

**(b) Authorizing a running process (worker).** A re-run execution must never be authorized to act.
- Tie rejection to the attempt that is actually executing, not to a lookup of attempt 1. Reading attempt 1 successfully says nothing about whether the current process is a re-run.
- Every worker step that could mutate state, especially publication, must establish its own executing attempt and refuse to proceed unless it is attempt 1. This check must run before any feed mutation, token use or push.
- Confirm that `publish.run_identity` or its replacement still rejects a re-run after your change, and that no earlier step mutates the feed.

**Rules.**
- Keep the `run_attempt == 1` field check on evidence.
- Do not add re-run detection machinery beyond reading the correct attempt and checking the executing attempt.
- Preserve recorded uncertainty. Re-run evidence must never turn an uncertain outcome into a definite one.

## Priority 2 — F2: pre-push failures reported as uncertain

The controller treats only a skipped "Publish checked feed update" step as proof that no push occurred. In the push phase, the `retained_receipt` check, `render(feed)` and `writer_token` all run inside that step.

**What you must not assume.**
- A successful render in `prepare` does not guarantee that a later render, file read or I/O in `push` succeeds.
- A missing receipt, missing job or empty listing is not by itself proof that no push occurred.

**What to do.** Determine failure certainty at the actual step boundary.

1. Move all fallible deterministic work into `prepare`, so that its failure leaves the push step skipped. This covers:
   - the retained-receipt check;
   - status rendering;
   - expected-path derivation.
2. The push step should then perform only:
   - token minting;
   - `checked_push`;
   - HTTPS confirmation.

   The existing hash check verifies the already-staged bytes.
3. Do not re-render in `push` on the claim that rendering cannot fail.
4. Failures between the start of the push step and the `git push` invocation stay uncertain to the controller unless a step-level boundary proves otherwise. Examples are token minting and a missing staged file.
5. Splitting token minting or `git push` into a separate step is an architectural change to the step boundary and must not pass a token between steps. Present it only as an option, with its estimate. Do not implement it without approval.
6. Document the residual uncertainty honestly. Do not add a "declare lost" or recovery protocol.

## Priority 3 — F3: one broken repository blocks all selection

`readSources` aborts the whole request on any non-200 result from any enrolled repository.

**Sound requirements.**
- Compute relevance from the definitions actually selected at their immutable revisions, not from the enrollment snapshot alone.
- A selected definition may introduce prerequisites that are absent from the snapshot. Every such prerequisite must be read and must succeed; otherwise the request fails explicitly. `traverseSources` must keep receiving every required prerequisite.
- Preserve:
  - the expected-tag-object check;
  - enrollment ID equality;
  - deployment pinning;
  - the generation check.

**Open policy question (do not invent the answer).** Suppose an enrolled repository cannot be read, so its newly selected definition is unknown. Then nothing proves it unrelated to the requested root:
- Enrollment-only reachability does not prove it unrelated.
- Classifying the error as a definite repository-local defect does not prove it unrelated either, because the unknown definition could still depend on the root.

**What to do.**

1. Validate from source what `readSources` and its callers can actually know when a read fails, and which failure kinds are distinguishable.
2. Reuse existing persisted graph evidence where that is sound: a definition already selected at an immutable revision whose dependencies are recorded.
   - Do not substitute that older evidence for a newly selected definition that could not be read.
   - A new highest tag with an unreadable definition is unknown, whatever older revisions declared.
3. If ambiguity remains, present the competing handling choices to the owner, with consequences and estimated size. Possible choices include:
   - fail the whole request, as now;
   - fail only when the unreadable repository could be a dependent;
   - exclude and report with reason, by some defined error class;
   - rely on prior immutable evidence only when the selection did not change.
4. Do not present any error-class or omission policy as an existing owner rule.
5. Implement no omission behavior until the owner decides. Never drop a repository silently.
6. While F3 is blocked, continue with independent fixes.

## Priority 4 — F4: oversized assignment aborts all advancement

The size check in `Pipeline::advance` throws for the entire batch.

**Required outcome.**
- An oversized node does not stop other nodes from being assigned or published.
- It is not silently dropped. It stays visible as blocked, with a reason, in the `advance` response and in the existing blocker/UI view.
- It does not consume the per-feed writer slot.

**Rules.**
- Check the size before committing any state change.
- A waiting→failed transition without dispatch is not currently valid. Do not invent new persisted states without approval. Prefer leaving the node in its current state with a derived blocker.
- Bound the full dispatch payload for both build and publication. Publication adds `build_result`. Use the verified official limits.
- Trimming dependency receipts changes the assignment schema, digests and worker consumers. Treat it as out of scope unless separately approved.

## Priority 5 — Triage F5–F14 (classify first, change only what is justified)

For each item, report one of: verified defect, hypothesis, owner setting, documentation-only, or rejected (with reason).

Implement only small, in-scope corrections that need no new architecture and stay within `AGENTS.md` growth rules. Anything else needs a design and estimate first.

| Item | Treatment |
|---|---|
| F5 | Check whether the host should require the build manifest's environment and evidence fields to equal the trusted `environment.json` and `contract.evidence(selected)`. State where the publisher boundary lies. |
| F6 | Check whether the workflow-blob/tooling-SHA consistency check belongs at deployment time or at selection time. Verify its cost and whether it adds network work to an event path. |
| F7 | Owner setting: observer token type and scope, and whether it moves to a main-only environment or a read-only App. Do not change credentials. |
| F8 | Owner setting: whether a ruleset restricts pushes to the feed's `main` branch to the writer App. Record it; verify only within authorized scope. |
| F9 | Hypothesis: whether a completed attempt-1 run with a verified-complete empty job list is definitive pre-push evidence. Inferring this from receipt absence contradicts current policy; owner decision only. |
| F10 | Generation-guard rollback of a definite outcome. Reproduce it before changing anything. Keep generation semantics. |
| F11 | `search` should filter runs by the controller bot actor before counting. Possibly a small definite fix; first verify that the field exists in the list payload. |
| F12 | `database.reset()` inside its own callback chain. Verify against SNode.C/MariaDBClient ownership semantics and MQTTSuite patterns, or with ASan. Do not guess. |
| F13 | Renderer items, each triaged independently (see below). |
| F14 | Correct stale or incorrect documentation against current code and evidence: secret names, receipt layout, outdated "not accepted" statements, and self-hosted runner provenance limits. |

**F13 renderer items:**
- **Version ordering.** Lexical ordering of the all-retained-versions table is not automatically a correctness defect, and it is not an owner-requested support catalogue. Triage it. If an existing comparator already applies at no meaningful cost, note that. Introduce no new version codec or algorithm.
- **README page-name collision.** Verify it independently.
- **`PACKAGES_GPG_PASSPHRASE` exposure.** Check whether it is exposed or written in phases or formats that do not need it. Verify this independently, without inspecting secret values.
- **SDK `sha256sums` trusted via TLS only.** Document or propose; do not implement.

**Evidence claims.** Also reassess the review's evidence claims, and correct project documentation where it overstates them:
- Client checks assert only the top-level package version.
- `core 60, side 62` proves consumption of build-time exports, not runtime package dependency selection.
- Modified-index and modified-package rejection has been verified only locally.

## Tests (public boundaries, meaningful)

Tests must cover changed behavior and invariants through existing harnesses: `tests/pipeline.py`, `tests/worker*.py` and `tests/openwrt.py`.
- Do not write tests that merely duplicate the implementation.
- Do not add regression tests for low-impact documentation changes.

| Finding | Tests |
|---|---|
| F1 | The latest view shows attempt 2 while attempt-1 evidence is success or failure, and the attempt still resolves from attempt-1 evidence. Attempt-2 evidence is never accepted. A worker process executing as attempt ≥2 refuses before any mutation, even though an attempt-1 lookup succeeds. |
| F2 | A render failure fails `prepare` and leaves the push step skipped, so the controller reaches `failed`. A failure inside the push step stays `publishing` and uncertain. The happy-path output is byte-identical to the current output. |
| F3 | Only approved behavior is tested. Sound parts: a prerequisite introduced only by the selected definition is read and required, and a broken required prerequisite fails selection. Test any omission behavior only after the owner decides. |
| F4 | An oversized node is blocked and reported while other nodes proceed. Publication payload bounds are enforced. The feed writer slot is not consumed. |
| F5–F14 | Each accepted behavior change gets a test of its invariant. |

Label local synthetic checks clearly. They are not GitHub acceptance.

## Live sandbox verification (within the already-authorized VOC-Experiments scope)

Once a change is confirmed to be within authorized scope, deploy the changed tooling and workflow blob and update the matching sandbox controller pins.

**1. Real integrated publication.** This proves the new renderer and publisher integration with a real build. It is a new-package publication, not the earlier status-only refresh, so the expected diff differs.
- **Must stay unchanged:**
  - previously published package bytes;
  - trusted keys;
  - existing tags;
  - all unrelated feed paths.
- **May change, and only these:**
  - the assigned package additions;
  - the indexes affected by those additions;
  - the new receipt and export;
  - deterministic status projections (status page and JSON).
- **Verify:**
  - receipt and HTTPS bytes;
  - controller state `published`;
  - the status page and JSON diff against that allowed set.
- **Identical replay:** a replay of identical content must add no changes and no unnecessary signatures.

**2. UI re-runs.** Make a deliberate UI re-run of a completed build and of a completed publication.
- The controller resolves both from attempt-1 evidence.
- The re-run attempts refuse before any mutation.

**3. Failure cases.** Test pre-push failure and cancelled-pending-publication only if they can be triggered without touching production or forging evidence. Otherwise record them as not live-verified.

Never retry an uncertain mutation to "test" it.

## Outstanding owner inputs (ask only for these; continue independent work meanwhile)

- Certificate-chain and private-key paths for `gw-home.vchrist.at`. TLS activation is already approved for the webhook at `https://gw-home.vchrist.at:8443/github/webhook`. Leave port 443 untouched. Live webhook delivery and redelivery or duplicate handling remain unclaimed until then.
- F3 handling choice, if source validation leaves ambiguity.
- `PACKAGES_OBSERVER_TOKEN` type and minimum scopes (F7).
- Feed `main` ruleset (F8).
- Policy for the F9 inference.
- Approval for any net production growth or architectural change required by `AGENTS.md`, for example the F2 step split.
- Authorization for commit, push, and merge/install, wherever it is not already explicitly given.

## Completion and final report

You may complete F1, F2, F4, the sound parts of F3 and other independent fixes while owner inputs are pending. Do not claim full acceptance.

Report:
1. For each of F1–F14: classification, evidence (official documentation citations and traced code paths), invariant, the change made or deferred, and the reason.
2. Commits, each a single coherent change on `feat/persistent-package-graph` on top of the preserved documentation commits, and whether each was pushed. Include the authorization relied on for each stage.
3. Line accounting, reported separately for production, tests and documentation: added, removed and net. State whether any net growth was approved and by which decision.
4. Formatting and `format-check` results, plus build and test commands with outcomes, run from the permitted directories.
5. Local proof versus live sandbox evidence (run URLs), each clearly labelled.
6. Residual uncertainty, especially F2 push-step failures, F3 and F9.
7. Outstanding owner inputs and acceptance gates:
   - TLS paths and live webhook delivery;
   - F3 policy;
   - token scope;
   - feed ruleset;
   - real publication with the new renderer;
   - live tamper-rejection checks.
8. Confirmation that no production SNodeC resources, secrets, existing tags, trusted keys or previously published package bytes were touched, and that `.qtcreator/` and `.local-credentials/` were preserved.
9. Updated `WORK_LOG.md` entries.
