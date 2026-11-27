# Implementation plan for Contract X v2

Contract: SPEC.md §7 (authoritative). This plan says how it is implemented, qualified
and rolled out. Gates are marked GATE; everything else runs without stopping.

## 0. Prepare (read-only)

- Entry check: every touched canonical checkout is clean and on its updated default
  branch. Later, the task's own feature branches may stay checked out while their PRs
  are open. Work of another task found in a checkout is a gate.
- snode.c, if it is not clean on master: read-only only (`git show master:<path>`,
  `git log`). Prepare its changes as a patch under `/tmp/codex/artifacts/snode.c-x/`
  without touching the checkout. Before the merges in step 5 → GATE A ("snode.c
  free"); then apply the patch on updated master, PR, merge.
- vchrist, read-only inventory only (via the SSH/sudo route of the earlier ops
  threads; if it fails → GATE): installed core-ci version, database schema,
  configuration, service arguments, heads of `feed` and `feed-staging`, highest
  allocated revision per package×version×target in the old database. No backup, no
  refs and no other writes in this step; the final backup happens in §5a.

## 1. SPEC

§7 is already Contract X v2. Rewrite §0–§6 and §8 to match it (persistence, routes,
configuration, service arguments, acceptance). Remove wave, chain and migration text.

## 2. Packages (one PR, merged only in step 5)

- `build.yml`, `publish.yml`, `restore.yml` (one job each) and tools per X6–X9;
  delete `wave.yml`, `chain.yml`, the multi-job publication and all wave logic.
- APT layout per distribution; snapshot refs; staging fault input (X8).
- Catalog with image/SDK digests and pinned OpenWrt feed revisions.
- Tests: selectors; request validation; installation from `feed_commit` with
  verification; claim 409 / rerun / idempotent 200 for builds and publications;
  OpenWrt closure in one SDK; quarantine with partial publication; receipts and
  acknowledgement; atomic push with snapshot refs; `pre-x` protection; retention
  closure over runtime dependencies plus by-hash generations; namespace collision
  check; `fault` rejected outside feed-staging; restore with lease; grep test against
  project names in `tools/`, `.github/`, `ci/targets.json`.
- Ceiling: tools ≤ 1,000 lines of Python (YAML not counted).

## 3. core-ci (one PR)

- FIRST in the report: deletion/replacement budget against 5,000 lines (existing
  counting rule in `tests/slim_source.py` PLUS the daemon DDL; baseline today 5,850 +
  219 = 6,069, so a net cut of at least 1,069 lines is required). Not achievable →
  GATE with the numbers.
- Implement X2–X11. Reuse unchanged: GitHub App auth, HMAC, IDP OAuth/MFA login,
  MariaDB client, SNode.C configuration, webhooks. New schema from scratch; delete the
  one-shot migration.
- Dashboard: pass N; reservations (queued/running/publication); queues with item
  states and conditions with reasons; package×target matrix (version-revision,
  attempt, bindings, feed_commit, run links); timings (GitHub queue, setup, staging,
  build, upload, publication); actions Retry (identical inputs only), Admit tag,
  Reconcile, Pause.
- Acceptance tests:
  - a turn fills all free reservations; fairness across queues; arrivals after the
    cutoff go to the next turn;
  - a newer version in progress → waiting although an older publication is usable;
    newer version failed → highest published one bound;
  - closure conflict vs waiting during a refresh; cycle;
  - all slots occupied by long builds: the first to finish hands its slot to the
    publication, no build takes it, the batching delay runs inside it;
  - the last build finishes while a publication with a frozen plan runs: on that
    publication's completion a new publication reservation is claimed for it; same
    at startup; batching deadline survives a restart;
  - a reservation is released only when GitHub reports the run completed;
  - Retry uses the identical frozen request; changed inputs after a failure → new
    revision, no Retry;
  - identity changes (ci/, target environment, contract, dependency); no rebuild on an
    ordinary commit; refresh only when the binding vector changes;
  - uncertain commit → reload; lost dispatch / lost 200 → idempotent claim;
    rerun → 409; finished before started; duplicates;
  - publication: ambiguous run found via run-name; crash after push → receipts;
    quarantine; backoff limit; artifact expiry; acknowledgement;
  - snapshots kept while an attempt is active or the item is retry-eligible, deleted
    afterwards, `pre-x` never;
  - Retry: `inputs` byte-identical, attempt +1; a run ending without a report is
    concluded from the GitHub API after the callback deadline;
  - immutability: same original digest → already published; different → conflict;
    RPM served digest differs from the original and is recorded separately;
  - `qualification_fault` refused unless feed_branch is feed-staging, consumed once;
  - all three deadlines; restart in every state; pause.

## 4. Registry repositories (one PR each, no tags)

- `releases` range; complete direct build and runtime requirements (incl. PCRE2);
  exact runtime dependencies from `DEPENDENCIES_JSON`.
- snode.c: `Build/InstallDev` (headers, linker files, CMake package).
- mqttsuite: recipe consumes `EXTRA_DEPENDS`; PCRE2 runtime dependency.
- Every OpenWrt recipe builds from `$SOURCE_DIR`.
- Qualify clean consumer builds and fresh installs on IPK and APK locally.

## 5. Rollout

### a) Quiesce the old system (0.2.1 has no Pause)

1. While the old daemon runs: its status endpoint shows no active wave or
   publication. If not: wait (do not cancel) and check again.
2. Stop and disable the service.
3. After the stop: `gh run list -R SNodeC/Packages --status in_progress` and
   `--status queued` are empty for the wave and publish workflows. If not: wait until
   they are (a dispatch may have raced the stop); do not cancel.
4. Final backup, now that no old writer is active, under
   `/var/backups/coreci/pre-x-<timestamp>/`: database dump (verified by restoring it
   into a scratch database), the installed .deb and its configuration.
5. Record the final heads and pin them by ref creation (authorized operation: create
   a new ref pointing at an existing commit; nothing else):
   `refs/snapshots/feed/pre-x` → current `feed` head (protected per X6),
   `refs/rollback/packages-pre-x` → current Packages `main` head.
6. Only now merge anything that removes `wave.yml`.

### b) Install (staging)

Merge the PRs (after GATE A if it applies), tag core-ci v1.0.0, build the bootstrap
.deb (trixie container, its own hook), install it, new database,
`feed_branch = feed-staging`, `github_slots = 20`, start with Pause ON.

**GATE B:** owner logs in with MFA; wait for "Live confirmed".

### c) Staging qualification

Reset feed-staging (new layout, keys only; record its head as
`refs/snapshots/feed-staging/pre-x`), then two runs:

1. **Cold run** (timing baseline, no interference): unpause and let the full matrix
   reach a terminal disposition. Its timings and runner-hours are the baseline for
   GATE C criterion 5.
2. **Fault run**: start it with T6 (a ci/-only commit in core-ci's own `ci/`), which
   re-enrolls core-ci on all its targets. Set `qualification_fault = after_push` for
   T4 beforehand. T1, T2 and T5 are executed while that run's builds are in
   processing; T3 on a finished run; T7 after the run.

Authorized staging test operations (feed-staging only), each with its evidence:

| Test | Operation | Evidence |
|---|---|---|
| T1 restart | `systemctl restart` while ≥ 5 items are processing and a publication reservation exists | same attempts continue; no duplicate attempt; run ids unchanged; dashboard consistent |
| T2 duplicate dispatch | dispatch `build.yml` manually with a running request's id and attempt | second run ends with 409 log line, no work, no artifact |
| T3 manual rerun | `gh run rerun` of a finished build run | run_attempt 2 gets 409, no work |
| T4 interruption after push | `qualification_fault = after_push` → the next publication gets `fault=after_push` | push present at feed-staging head; job failed before callbacks; core-ci recovers all items from receipts; no duplicate publication; the option is consumed |
| T5 isolated retry | `gh run cancel` of one running build | item failed with reason "cancelled" (from the final report or, if missing, from the GitHub conclusion after the callback deadline); Retry → attempt +1, same revision, `inputs` byte-identical (diff), snapshot still present; other items unaffected |
| T6 ci-only revision | a test-only commit in core-ci's own `ci/` (comment line in its hook) on its default branch | core-ci items get revision +1 with the same source commit; no tag; no other repository rebuilt except declared consumers |
| T7 restore | `restore.yml` on feed-staging to `refs/snapshots/feed-staging/pre-x`, then back to the latest publication snapshot | branch head equals each snapshot; lease respected |

**GATE C criteria** (all must hold; report each with evidence):

1. Every item has a terminal disposition. Accepted: published; enrolled with
   condition unsupported (declared by the repository's `releases` range); and, only
   as named exceptions, failed or enrolled with condition blocked. Rejected: enrolled
   without condition (runnable), waiting, processing, built.
2. Every exception is listed per item with reason, responsible repository and the
   resulting coverage gap. The owner must accept each one explicitly in the GATE C
   answer.
3. Fresh install from feed-staging of every published package×target: apt (per
   distribution), dnf, opkg, apk.
4. Every riscv64 job finished within 6 h including upload.
5. Timings and runner-hours measured and compared with the estimate (4½–7 h,
   40–53 runner-hours).
6. T1–T7 passed.
7. No namespace collision; ledger = database; snapshots consistent.

The report contains the new APT sources lines for users and the rollback procedure.
Wait for "cut over" (with any accepted exceptions).

### d) Production

1. Pause; new database; revision floor = max(old database dump, production ledger) + 1
   per package×version×target; `feed_branch = feed`; unpause; full run.
2. The old APT layout (`apt/dists/…`) stays FROZEN on the branch for 30 days with a
   notice file pointing to the new sources lines; afterwards core-ci removes it through
   a normal retirement.
3. Update the landing pages/docs with the new sources lines.
4. Once core-ci 1.0.0 is in the feed: `apt install --reinstall` on vchrist, restart,
   verify the version (owner logs in with MFA again; not a gate).

### Rollback (only on the owner's explicit instruction)

The old daemon 0.2.1 cannot be a rollback target: it reads `ci/packages.json` from
each repository's newest tag, and every newest tag already carries schema 2, which
0.2.1 rejects. Rollback therefore protects users, not the old scheduler; recovery
of the CI itself is fix-forward.

1. Pause and stop core-ci 1.0.0.
2. `restore.yml` on `feed` to `refs/snapshots/feed/pre-x` (users see exactly the
   pre-cutover feed again).
3. Fix forward on staging; cut over again only through GATE C.

The old database dump, .deb and configuration from §5a are kept for forensics only.
Before GATE C, T7 proves that `restore.yml` restores a pinned snapshot exactly.

## 6. Issue #76

Result, measured timings, accepted exceptions, open items. Close issues made obsolete
by the wave model.
