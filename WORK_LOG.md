# core-ci wave model — implementation record

Canonical checkout /home/voc/projects/snodec/core-ci, origin
git@github.com:SNodeC/core-ci.git, default branch main. Step 0 passed: clean main,
pulled, HEAD 649d0f8e011fca160113397db091f91f16fe43e6. No open PR existed.
chore/owner-pending-core-ci stays at d17dfaada853d480f4fe823ae47df72cb80badf2.

SPEC PR #71: https://github.com/SNodeC/core-ci/pull/71, "spec: wave model",
merge dd5f45ca97574fdc6c8bd45825dc1a7408f50ea8.
Implementation branch refactor/wave-model is based on that merge.
Section 7 was compared byte-for-byte with the owner's CONTRACT W.

The invariant is one active wave with one frozen plan and one workflow dispatch.
Unit callbacks resolve their selected source/revision/inputs from that plan.
The event history remains authoritative, including restart and dashboard display.
Per-unit dispatch, workflow ownership, publication serialization, feed types/rows,
Kind, per-unit deadlines and the old callback route are removed completely.
No compatibility, production access, deployment, system installation or tags.

## Accounting

Physical lines include blanks/comments, SQL and templates. app excludes its
dashboard assets; tests source includes C++/headers/Python/JavaScript, and the
all-tests figure also includes JSON fixtures/documentation. Baseline 649d0f8.

| Area | Baseline | After | Delta |
|---|---:|---:|---:|
| core/ | 1291 | 1139 | -152 |
| persistence/ | 749 | 736 | -13 |
| adapters/ | 1746 | 1712 | -34 |
| app/ | 1115 | 1129 | +14 |
| app/dashboard/ | 496 | 505 | +9 |
| Production total | 5397 | 5221 | -176 |
| Test source | 5875 | 3322 | -2553 |
| All test files | 9119 | 5725 | -3394 |
| CTest cases | 38 | 27 | -11 |
| Event types | 14 | 15 | +1 |
| Commands | 2 | 1 | -1 |
| Route shapes, excluding optional redirect | 10 | 11 | +1 |
| Nonsecret config keys | 29 | 29 | 0 |
| Production CORECI environment keys | 3 | 3 | 0 |
| Persistence tables | 7 | 6 | -1 |

The 15 old numbered per-unit engine rules are deleted. Their replacement is the
wave trigger/planner and dispatch/outcome policies in SPEC 4.3–4.4, governed by the
10 fixed clauses W1–W10; the new specification does not number individual rules.
This avoids implying the differently grouped rules have a comparable numeric count.

app/assets grow for the public plan, wave panel and feed commit display. The
complete architecture and production code shrink; no new parallel implementation
or compatibility path remains. The removed scenario files tested per-unit
dispatch/feed queues. Their replacement exercises wave behavior, including replay
after every event and 10000 seeded randomized scenarios. Authentication, transport,
write-ahead, row/SQL, MariaDB, SSE and HTTP boundary coverage remains.

Config removed: build-timeout, publish-timeout, max-concurrent-builds.
Added: wave-delay (120), wave-deadline (23400), feed-branch (feed/feed-staging).
Registry/catalog/workflow defaults point to SNodeC/Packages. One dispatch uses
wave_id, plan_url, callback_url, feed_branch; workflow ref main.
Routes replace POST /api/v1/builds/{buildId}/report with public GET
/api/v1/waves/{wave_id}/plan.json and HMAC POST /api/v1/waves/{wave_id}/report.
Other public/dashboard/webhook routes and optional redirect remain.

## Decisions

- Decision: follow the owner's explicit full-task/SPEC authorization because it
  supersedes the older repository instructions requiring hash scripts and a
  stop after each phase. AGENTS.md is unchanged.
- Decision: freeze active plans across declaration changes because callbacks must
  keep resolving the dispatched source, revision and dependency inputs.
- Decision: mark only transitive same-target dependents of explicit build failures
  Blocked because independent chains and targets must continue.
- Decision: acknowledge signed publish-start callbacks without an event because W6
  accepts them and the owner's complete event list excludes UnitPublishStarted.
- Decision: use fresh schema 5 because typed event/graph layouts change and old
  histories are explicitly unsupported.
- Decision: preserve a dispatched wave through restart until its original deadline
  because blindly repeating an ambiguous dispatch could start a second run.
  Only explicit rejection resumes dispatch backoff.
- Decision: accept stable release tags only because W2 removes the former
  accept_prereleases opt-in. Full SemVer parsing/ordering remains in Version.
- Decision: leave unchanged failed chains awaiting retry while planning independent
  eligible chains because an unrelated tag must not silently retry a failure or
  serialize unrelated work.
- Decision: preserve dormant failures when source identity is unchanged because
  removing/re-adding a unit is not a retry request.
- Decision: retain frozen plans in replayed wave history because plan URLs remain
  readable for finished runs and after restart, without JSON persistence.
- Decision: count reuse units as Published in the wave panel because they install
  existing publications rather than receiving callbacks.
- Decision: refer publication records to their wave for run identity because a
  WaveStarted callback can arrive after a unit publication receipt.
- Decision: reject a recorded tag whose commit changes because the release and
  tag-bound declaration must stay paired with one source commit.
- Decision: retain shared HTTP fixtures after removing the browser suite because
  SSE, routes and smoke tests still consume them; no test requires browser tooling.
- Decision: build the unchanged tracked snodec-auth client into this task's
  temporary prefix because the system-installed client lacks APIs already used
  by the baseline. No dependency checkout/branch or untracked declaration is edited.

## Qualification

GCC 16.2.0, C++20 RelWithDebInfo, OpenSSL 3.6.5, nlohmann-json 3.11.3,
Python 3.14.10 and installed SNode.C.
The auth dependency is unchanged revision b0b1c8c702a6f80319b1799a996fac8d87f1b7a9,
built with its documented AUTH_CLIENT_ONLY option into
/tmp/codex/build/core-ci/wave-model/auth, prefix auth-prefix under that task root.
Its existing untracked ci/ directory is not a build input and remains untouched.

Controller build /tmp/codex/build/core-ci/wave-model, CORECI_WITH_SNODEC=ON,
snodec-auth_DIR at that prefix. Compiler temporaries use the task's tmp directory.
The committed formatting baseline and modules match ~/.codex/formatting bytewise.

Disposable MariaDB binds 127.0.0.1:13326, database coreci_wave, with all data,
socket, pid and logs under the same task build directory. No system service or
production database is used. CTest receives CORECI_TEST_DB_HOST=127.0.0.1,
CORECI_TEST_DB_PORT=13326, CORECI_TEST_DB_USER=root,
CORECI_TEST_DB_NAME=coreci_wave, PYTHONDONTWRITEBYTECODE=1 and task-local TMPDIR.
All HTTP fixtures use loopback TLS and generated disposable credentials.

A first focused trial passed 20/21; a full trial passed 28/29, including all
integration cases. The trials exposed the expected dormant-failure behavior and a source-identity
reset regression; both were corrected before qualification.
Final review also removed the plan's separate prerequisite list: frozen input
keys already supply that information. Qualification restarts after that reduction.
Earlier compilation exposed the baseline installed auth-client mismatch and stale
test references; these were resolved without changing dependency sources.

Final full builds and format-check pass. Final CTest run 1: 27/27 passed,
no skips, 176.87 seconds. Final run 2: 27/27 passed, no skips, 173.80 seconds.
Each run includes 10000 seeded randomized wave scenarios, C++ dashboard model/
plan checks, SSE/route/authentication HTTP checks and MariaDB integration.
No sanitizer, performance speedup, production integration or deployment result
is claimed.

## Owner-directed test removal

The owner removed browser-driven qualification from scope. Delete
tests/dashboard/browser.cjs (121 baseline lines), tests/dashboard/e2e.cjs
(79 baseline lines) and tests/dashboard/e2e.py (84 baseline lines), plus both
CTest registrations and the browser dependency configuration block. The scripts total 284 baseline lines; the removed CMake block adds 40, for
324 removed baseline lines, plus documentation changes. No Node/npm manifests or
lockfiles were present. Shared HTTP fixtures remain because S22–S24 and
M_LocalSmoke consume them; C++ W_Model remains. SPEC and test documentation no
longer require browser tooling. Full qualification is repeated on the reduced
suite after the final source reduction.

## Handover

GitHub App requirements: Actions write + Contents read on SNodeC/Packages;
Contents read + Metadata read on source repositories; webhook event push;
installation on the relevant repositories. App key and HMAC secrets remain
operator-managed. The separate Packages workflow implements W7–W10.

No extra checkout exists. Temporary build/auth/database artifacts are removed
after qualification; retained logs are under ignored
build/codex/wave-model and removable after review. Both task branches are deleted
after authorized merges; canonical main is left pulled. An untracked ci/ directory
appeared during final verification (ci/package/native.sh and ci/packages.json).
It is excluded from this task's commit and remains untouched pending ownership
coordination; a clean-worktree handover depends on its owner's resolution. Final PR/merge
identities and test results are supplied in the final report.
