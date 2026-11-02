# Publication confirmation correction and interrupted v2.1.18 acceptance

The subsequent fresh [v2.1.19 acceptance](core-v2.1.19-run.md) completed 20/20
Published. The interrupted v2.1.18 account below remains historical evidence.

Status of v2.1.18 on 2026-10-31: correction applied and running locally. Full automatic
end-to-end acceptance is NOT complete. A local process interruption stopped
observation/graph progression while already dispatched GitHub jobs continued.

## Corrected authority

Published now requires both the matched Publish job and exactly one Confirm
public receipt job completed successfully in the same authenticated pinned run
and attempt. Only then is the committed execution receipt read. The workflow
owns existing HTTPS SHA-256 confirmation; the controller does not duplicate it.

Publication and newly ready exact dependent assignments use the existing one
MariaDB graph change; dispatch follows successful commit. Missing, skipped,
failed or ambiguous confirmation preserves Publishing and its assignment.
Historical Published outcomes remain terminal. No polling/replay/new protocol.

## Source changes and accounting

- src/Execution.cpp: reshape existing success predicate to include the unique
  successful confirmation; remove single-use report helper and retain identical
  logging at its sole callback. Net +1 physical C++ line.
- src/Graph.cpp: merge preserves timestamp fields only when previously present,
  avoiding synthetic null fields on duplicate otherwise unchanged run evidence.
  Focused idempotence tests exposed this defect; assertions were retained. +2.
- Controller production total: 2328 -> 2331, net +3, within approved +7 ceiling
  and absolute 2500. No UI changes, additional state or architecture.
- Tests: new tests/publication.cpp 266 lines, tests/publication.py 81 lines.
  Build wiring: CMakeLists.txt +8 lines. No production injection hooks.
- Documentation: AGENTS.md, docs/contracts.md, WORK_LOG.md, this report and
  regenerated docs/source.sha256. Reviewable controller/workflow patches remain
  under docs/patches; final helper/merge changes are described above.

| Production file | Physical C++ lines |
| --- | ---: |
| src/Auth.cpp | 137 |
| src/Auth.h | 17 |
| src/Controller.h | 94 |
| src/Evidence.cpp | 272 |
| src/Execution.cpp | 374 |
| src/GitHub.cpp | 307 |
| src/Graph.cpp | 437 |
| src/Http.cpp | 130 |
| src/Managed.cpp | 161 |
| src/Managed.h | 24 |
| src/Store.cpp | 103 |
| src/main.cpp | 275 |
| Total | 2331 |

All controller-owned production C++ is in these files; lines include comments
and blanks. Relevant canonical formatting and 160-character checks passed.

## Focused checks

| Check | Result | Elapsed |
| --- | --- | ---: |
| publication-confirmation-contract | Passed | 2.61s |
| graph-and-receipt-contract | Passed | 0.06s |
| mariadb-commit-and-restart-contract | Passed | 1.02s |
| native-startup-http-contract | Passed | 105.07s |
| native-sse-backpressure-contract | Passed | 19.31s |

The new contract test uses actual Controller.observe/Store/Graph and isolated real
MariaDB. GitHub public-boundary doubles exist only in the test binary. It covers
queued/running/missing/skipped/failed/duplicate confirmation, wrong run/job/actor/
workflow/attempt identities, missing/mismatched receipt, exact dependent inputs,
duplicate/out-of-order evidence, rejected commit, lost commit acknowledgement and
restart without replay. An independent SQL connection checks committed authority
before the dispatch callback. No unrelated suite was run.

Initial test failures were corrected, including the real duplicate-merge defect;
no assertions were weakened. CTest's LastTestsFailed.log retains an earlier
failure from a prior invocation; the final publication invocation passed and
CTestCostData records its successful 2.60834s execution. Subsequent invocations
covered runtime/SSE separately, replacing LastTest.log.

Workflow YAML parsing and structural before/after comparison passed: only the
confirm condition changed. Existing needs, steps, inputs, proof and feed-writing
concurrency are unchanged. Confirmation remains outside that concurrency group.

## Activated sandbox identity

Existing file: VOC-Experiments/ci-coordinator,
.github/workflows/core-ci-publish-renewed.yml.

Condition:

```yaml
if: >-
  !cancelled() &&
  needs.publish.result == 'success' &&
  fromJSON(inputs.assignment).kind != 'retention'
```

Old blob: e4ca49f44c893aa9ce64e4a4b5475a1390f319cc.
Corrected blob: ce11809b876f119ccb521edd27aee66b7c39d0ac.
[Activated workflow commit](https://github.com/VOC-Experiments/ci-coordinator/commit/2af037e60189b755dc8ce9f9711f801a12720d69).

No authoritative local workflow checkout exists. Connector/Contents writes
returned definite authorization failures without mutation. Existing SSH access
then applied the reviewed workflow-only fast-forward commit using a temporary
isolated Git index. No controller commit/index/branch change, permission expansion
or additional checkout. Only the approved public deployment pin changed.
All existing assignments keep their old workflow identities.

Controller checkout: /home/voc/projects/snodec/core-ci-new, feat/graph-controller,
unborn branch/no controller commit, remote or PR. Existing untracked work retained.
Build: /tmp/codex/build/core-ci/controller, GCC 16.2.0 Debug, installed SNode.C.
Binary SHA256: 9eebe99fd8a1c4d9d7d1d62a7764ca0f3502ba1ed1b5963750c51617220e40bc.
Source-manifest SHA256: d5a06d2b11f994a83ccd85b93a143892d9701d8df5e7ab99f388966ba068622b.

## Actual acceptance and interruption

Created exactly one tag, demo-core v2.1.18, pointing to
fc3acdb94779dafb879c183143c20e19b435e766. Tag request began
19:39:42.793678 UTC and succeeded at 19:39:43.648752 UTC. Automatic propagation
selected 20 new executions across five targets: core -> app/side -> join.
All 20 froze the corrected publication workflow identity. No downstream manual
selection, retry, replay or reconciliation occurred.

All five core builds succeeded. All five actual publication workflows and their
explicit confirmation jobs succeeded, including native targets that skipped
OpenWrt tools. Confirmation job intervals (UTC):

| Core target | Confirm started | Confirm finished |
| --- | --- | --- |
| Debian amd64 | 19:41:52 | 19:42:07 |
| Debian arm64 | 19:42:33 | 19:42:57 |
| Fedora x86_64 | 19:43:04 | 19:43:50 |
| OpenWrt 24.10 | 19:44:27 | 19:44:41 |
| OpenWrt 25.12 | 19:44:45 | 19:45:10 |

First Debian core was recorded Published; its app and side were authorized in
that same committed SSE patch. Both dependent GitHub builds subsequently
succeeded. The remaining results completed during the local outage.

The foreground controller and both diagnostic observers disappeared; their tool
sessions no longer had exit statuses. Log tail alone does not establish the
cause. Do not claim a controller crash cause, or an uninterrupted acceptance.

At 19:46:07 UTC restored the SAME binary/argv/database as detached PID 412713,
managed MariaDB PID 412714. Verified no listener/server owned the stale socket
and ECONNREFUSED before removing only that socket. MariaDB performed normal crash
recovery. External MariaDB remained untouched. No DB reset/migration or history
removal. The graph compared exactly equal to the last captured graph, including
all 110 old executions and the 20 new assignments.

Unresolved new state: 1 Published, 4 Publishing, 2 Building, 13 Waiting. Restart
does not automatically replay or reconcile assignments. Six existing assigned
executions need explicit observation of their completed remote results to let
the remaining normal graph transitions proceed. Reconciliation would not rerun
those jobs but can authorize the next stage/dependent work after evidence commits.
The supplied task explicitly requires separate authorization for manual
reconciliation; none performed. Diagnostic observers stopped after evidence
capture; local controller remains running. No new tag or replacement work.

## Timing and UI limits

For the first Debian core, GitHub confirmation finished at 19:42:07 UTC. Its
completed-job webhook was delivered at 19:42:09.978 (202 response, duration 0.82s);
a completed-run delivery followed at 19:42:10.319. Which one initiated the winning
observation is not recorded. Receipt read started at 19:42:17 and finished at
19:42:18. Graph dependent authorization is timestamped 19:42:27; committed SSE
patch 38 arrived at 19:42:27.670537 and contains core Published plus app/side
Building. Accepted-dispatch facts are recorded at 19:42:36 and 19:42:37. Those timestamps
are generated when the queued database mutation executes, not at HTTP response
receipt or outgoing request start.

Thus confirmation -> dependent authorization was 20s at stored second resolution,
including notification, authentication/GitHub reads, DB/controller processing.
Receipt-read completion -> authorization was about 9s; authorization -> stored
accepted-dispatch fact was 9-10s. These are observed pipeline intervals, not isolated
CPU overhead or an exact REST-ingress-to-outbound-request-start measurement.
No new production instrumentation was added. The interrupted run cannot provide
complete-chain timing or a speedup claim.

The actual browser showed Live and changing rows before interruption, reverse
dependency order, seconds/24-hour local times with time above date. Browser core
job times matched the stored GitHub evidence. SSE patch 38 included the dependent
release in 18,116 bytes rather than another full graph snapshot. The restored
HTTP graph and initial SSE snapshot matched committed state. Subsequent browser
inspection was unavailable (debugger unattached); do not claim post-recovery DOM
latency or final live UI verification. No UI code was changed.

Raw evidence retained: /tmp/codex/build/core-ci/confirmation-chain-20261031 and
its recovery subdirectory. Pending execution identities are recorded in
recovery/pending-observation.json. No additional checkout or production SNodeC
resource change. Exact receipt confirmation proves the existing workflow's HTTPS
observation, not simultaneous package availability at every CDN edge.
