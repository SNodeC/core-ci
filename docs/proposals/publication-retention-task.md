TASK: core-ci build retention with orphan-root feed snapshots
Phase-gated. Design, estimate and owner-approval checkpoint BEFORE any production change.

0. CONTEXT AND AUTHORITY
Repository: /home/voc/projects/snodec/core-ci (canonical checkout). GitHub: SNodeC/core-ci.
Expected state: branch rewrite/slim-graph-controller, HEAD 8461764ff2131ef13e44ce187d1c84b1550f5d27.
  This HEAD was merged INTO main as 3cc14fd9dfaba872dacb82e57d80f70c628c50fc. Verify both facts.
Expected owned dirty files, which must be preserved: README.md, WORK_LOG.md, deployment/sandbox/README.md,
  docs/deployment/README.md. There may be other untracked docs.

Read and obey AGENTS.md completely, including:
- the C++ cap of 3,576 physical lines under src/ (count the current total yourself);
- reductions first: delete obsolete paths before adding anything;
- one graph/state authority; no persisted ready queue and no generic workflow engine;
- never replay uncertain mutations; commit dispatch authority before calling GitHub;
- braces, a single final return and no avoidable break/continue (Python too);
- changed production lines of at most 160 characters;
- persistent builds under build/codex/<configuration>;
  temporary checks under /tmp/codex/build/core-ci/retention/.
Also read the final WORK_LOG.md entries and the evidence files
  /tmp/codex/build/core-ci/retention-review/{round1,round2,round3,orphan-round1,orphan-round2}.json.

Normal Git metadata commands and refs (status, log, rev-parse, diff, show, ls-remote, etc.) are permitted.
Never inspect private credential or configuration values, and never inspect secrets/, .qtcreator/ or
  .local-credentials/.
Never run reset, switch, rebase, stash, destructive clean, or an additional source checkout.
Never discard or overwrite dirty work.

Authority: this task grants no production change, commit, push, merge, tag, release, install, restart, deploy,
  workflow dispatch, push of an orphan root to a real feed, activation, change to SNodeC production resources, or
  history rewrite.
Honor any authority that the actual session explicitly grants later, for exactly the stage granted, without
  asking again for authority already given.
Owner approval for new architecture, mechanisms or line growth does NOT exist yet. See the Phase 2 checkpoint.

Branch policy: new independent work belongs on a feature branch created from current main, with coordination of
  the owned dirty checkout. If creating that branch would disturb dirty work or branch ownership, stay on the
  current branch, limit yourself to design documents until a transition is authorized, report the blocker and ask
  the owner. Do not move dirty files.

1. OWNER-APPROVED POLICY (this is policy, not implementation authority)
- Normal policy per project and target: retain the three highest successful source versions. A successful version
  is a verified publication, ordered by SemVer.
  - The recommended default is one numeric ordering shared by stable and prerelease versions; this default is not
    approved.
  - Transient protected extras may exist beyond the three. A failed or unverified replacement never displaces a
    working third version.
- Per retained version: retain the one highest successful rebuild REVISION. That is the highest revision number,
  not the most recently completed run.
- Historical retries may expire. Expiry must be visible. Never automatically build an ancestor, substitute a
  replacement or rebind a retry.
- No archive. No recovery from Git history. GitHub hosting stays. No hosting migration.
- Immutable assignments are never rewritten. Joins are satisfied only by verified publications.
- At each package publication, create a parentless root snapshot (git commit-tree with no -p) of the complete
  retained tree. Push it with git --force-with-lease=refs/heads/<branch>:<expected old head>, where the expected
  old head is captured from the checkout BEFORE the root is created. This runs under the existing shared
  feed-write concurrency group.
  - This agreed Git lease is permitted design.
  - What is excluded: new application-level leases, distributed locks, queues, timers, caches, schedulers and
    callback protocols, unless they are explicitly approved.
- Status updates stay ordinary commits with a parent.
- Do NOT import SNodeC DistributionPackages' 30-day cleanup, its inventories, or its 30-iteration conflict and
  retry loop. The only SNodeC evidence is a sampled main branch: a parentless package commit plus one status commit
  with a parent (verified 2026-10-29). Nothing about core-ci adoption is proven by it.

2. SOURCE FACTS TO RE-VERIFY (line numbers come from earlier review and may have drifted)
- Graph.cpp
  - assignment(): from the second attempt on, dependencies are taken frozen from attempts.front().
    The first attempt embeds the prerequisites' latest receipts, without "indexes", at dispatch time.
  - select(): fixes prerequisite execution IDs at selection time. For a dependency outside the affected set it takes
    the newest-revision, unsuperseded execution of the currently selected source with matching coordinates.
    Prerequisites always have identical target coordinates.
  - Supersession applies only to automatic nodes that are still waiting.
  - blockers(): among other checks, which include supersession and the size bounds of the dispatch payload
    (assignment <= 60000, total <= 65535), it requires each prerequisite's current attempt to be "published".
  - retry(): requires failed and not superseded.
  - published(): rejects a changed receipt, which includes feed_revision.
  - next_revision is a single monotonic counter.
- Pipeline.cpp
  - complete(): calls feeds.verify only while the attempt is in state "publishing", coalesced per attempt and stage
    through completionReads.
  - reconcile(): selects only publishing attempts, and building attempts that have no build_result.
  - Publication certainty:
    - a skipped push step or skipped publication job yields a verified failure;
    - a successful, valid publication proof records the attempt as published;
    - a push that may have happened without a valid publication proof stays uncertain.
- FeedProof.cpp: finds the newest branch commit that touches receipts/<id>/<rev>.json, reads it at that commit,
  compares with the HTTPS bytes and records feed_revision.
  This is NOT proven broken by root snapshots. A newer root holding the identical receipt supports a late FIRST
  verification. feed_revision is an observation context, not a byte address for workers.
  Confirm only checks the receipt hash over HTTPS and formats/prints the revision.
  Do NOT change FeedProof, and do not add compatibility or bypass logic, unless a focused test with repeated roots
  and a late proof demonstrates a real defect.
- Worker
  - publish.py: prepare_commit (expected-path staging), checked_push (fast-forward only, no force),
    the 900 MiB current-tree check before push, the confirm phase.
  - sign_native.py: retained_receipt (fails on missing or changed bytes), add_new (identity must not exist),
    publish_export, apt_indexes (the deb pool/<suite> is shared across architectures), rpm_indexes.
  - sign_openwrt.py: per-directory IPK/APK indexes.
  - feed_status.py: lists every receipt.
  - status_update.py: prepare and push.
  - native.prepare_dependencies / contract.download_feed: exact current-feed HTTPS path, size and hash.
  - Both publication workflow templates; the status workflow.
- Deleting worker files alone would break published-only selection, frozen retry dependencies and current-feed
  downloads.

3. PHASE 1 — READ-ONLY INVESTIGATION
Verify the repository state and every fact in section 2. Count the current C++ lines.
Identify obsolete code that could be deleted (reductions first).
Resolve the following FROM SOURCE as concrete proposals, not as owner questions:
 a. Eligibility invariant. Atomically, in one graph change committed before any remote deletion, check protection
    and withdraw eligibility for new references.
    Derive "protected" by tracing CURRENT eligibility in the source. Protected is work that can still legitimately
    proceed and consume the artifact:
      - genuinely eligible waiting work;
      - unbound, building, built or publishing work;
      - uncertain work.
    Do NOT treat superseded waiting nodes, or failed historical records whose retries have explicitly expired, as
    protected; otherwise protection would defeat the owner's expiry policy forever.
    Never withdraw anything from an unverified or uncertain publication, nor its receipt.
    A prospective, unverified publication never counts as a successful version and never displaces a working one.
    No new application-level queue, lease, lock, timer, cache, callback protocol or scheduler.
 b. Exact receipt closure. Define precisely what the retained publications and the protected work actually need,
    from the recorded receipts and assignments: which packages and which development exports are build inputs, and
    which transitive references exist only as audit metadata.
    Conservatively protect the recorded build relationships. Do NOT retain exports forever merely because a receipt
    mentions them for audit purposes.
    No package resolver for the four formats and no guarantee about arbitrary external runtime dependencies.
 c. Invocation timing and temporary overlap.
    One OPTIONAL candidate, not mandated: candidates are computed in the state change that records a newly verified
    publication; deletion is carried out by the next package publication for the same target.
    Analyze this honestly. If no further publication arrives for the target, deletion has no finite time bound.
    Do NOT claim a bounded overlap unless a source-supported completion trigger exists.
    No new timer, scheduler or protocol by default. If a deadline-free design remains, expose that as an
    engineering limitation; do not declare the design complete.
 d. Transport of the deletion set to the worker. It must stay within dispatch input bounds.
    Address compatibility with assignments already issued and with pinned definitions/workflow blobs: what happens
    to attempts dispatched before the upgrade, and how the input reaches both workflow templates.
 e. Representation of withdrawn eligibility, and how select, blockers, assignment and retry honour it.
    Expiry is visible, there are no dispatches that would 404, and there is no rebind.
    Withdrawn eligibility does NOT prove the bytes were removed.
 f. Unknown deletion or lease push outcome: report it, never replay it blindly, keep the assigned uncertainty and
    the existing path for restoring it.
    Whether a proven lease rejection may be classified as a definite refusal requires evidence; do not change the
    certainty classification silently.
 g. Lease capture: an immutable expected old head from the checkout, recorded before the root is created and
    carried across the prepare and push steps (for example via existing Git refs or files in the job).
    Never take the new head at push time.
 h. Per-format deletion and index regeneration:
    - deb: shared architecture pool; delete only files of the own architecture; regenerate only the affected index.
    - rpm: createrepo_c.
    - ipk: ipkg-make-index plus usign.
    - apk: mkndx plus signature.
    Preserve signatures, public links, provenance, the replay check and the checked-push guards.
    The SDK and signing boundaries of the worker stay unchanged.
    Delete payloads and exports from the current tree only. Do not create branches, tags, backups or archives.
    Report other refs; promise neither GitHub GC timing nor physical purge.
 i. Effect on the REST and public views: counts, status generation, status projection.
 j. Receipts and status metadata. Recommend keeping them bounded together with the retained versions, while always
    protecting unverified receipts and the graph's published outcomes.
    The exact deletion policy is a GENUINE OWNER CHOICE: record it at the checkpoint.
    MariaDB history retention is separately scoped; do not invent an archive policy for it.
 k. The existing 900 MiB current-tree check stays unchanged. No new, smaller limit. Refuse before any remote feed
    mutation; local scratch changes are acceptable.

4. PHASE 2 — MANDATORY CHECKPOINT (STOP HERE)
Deliver a short concrete design containing:
- solutions to 3a–3k, with source references;
- the files and functions to change, and the deletions;
- any new mechanism, named explicitly. Prefer a small, problem-aligned state change to any engine or framework;
- the current C++ count and the projected count against the 3,576 cap. If the cap would be exceeded, say so and
  request a new bound;
- production/test/docs lines added, removed and net, per file, listed separately for C++, Python and YAML;
- residual risks, engineering limitations (including any unbounded deletion timing) and the remaining open
  questions.
Owner decisions to list, and only these:
  (1) the receipt and status metadata retention policy;
  (2) confirmation of the shared stable/prerelease ordering default;
  (3) approval of the named mechanisms and line bounds.
Do NOT turn engineering or API questions into owner decisions.
Do NOT call the design implementation-ready while any open question remains.
Record the checkpoint in WORK_LOG.md only if the session authority allows documentation edits.
Otherwise return it as text. Then STOP and wait for explicit owner approval.

5. PHASE 3 — ONLY AFTER EXPLICIT APPROVAL, WITHIN THE APPROVED BOUNDS
Implement exactly the approved design. If any bound would be exceeded, or any unapproved mechanism needed,
STOP and re-estimate.
Focused tests only, run against the existing public boundaries. Use an isolated real MariaDB where the
persistence boundary matters. Clearly label synthetic GitHub fixtures. No full suite.
- Orphan roots: no parent; the full retained tree is present; deletions only of explicitly expected paths.
- Lease: the expected old head is captured before the root and carried through prepare and push.
  A lease conflict and an uncertain outcome are not replayed.
- Late proof: the same receipt under a newer root yields a valid first verification. Independently, HTTPS/Pages lag.
- Policy: three versions; highest revision; a failed or unverified candidate never displaces a working version;
  per-target independence; prerelease ordering.
- Protection: genuinely eligible waiting, unbound, active and uncertain work stays protected.
  Superseded waiting nodes and expired failed history are not protected.
  Race between retry and withdrawal; visible expiry; no dispatch that would 404.
- Bytes: shared bytes and package components; export links; the shared deb pool; restoring uncertainty.
- Unchanged: native/OpenWrt signing, the replay check, the 900 MiB check, status commits with a parent.
Verify that existing behavior is preserved where it is not intentionally changed.
Check actual permissions (git force-with-lease rules for the feed-writer App) only as a read-only activation step in
a VOC-Experiments stage, when that stage is authorized.
Commit, push and every later stage only as the actual session authorizes.

6. FINAL REPORT
- Production, test and documentation changes listed separately, with line counts against the approved bounds.
- Exact test commands and their results.
- Local proof distinguished from pending live acceptance.
- Anything unverified, and any engineering limitation still open.
- No commit, push, deploy or activation unless the actual session granted it.
Stop after reporting.
