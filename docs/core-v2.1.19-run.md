# Fresh core v2.1.19 full-chain acceptance — passed

Created one core source tag and observed the entire automatic graph complete.
All 20 executions are Published: core, app, side and join across Debian amd64,
Debian arm64, Fedora, OpenWrt 24.10 and OpenWrt 25.12. Both OpenWrt joins built
successfully; no repeat of the previous prerequisite-download failure.

No manual downstream selection, retry, reconciliation, webhook redelivery or
additional tag was used. No restart occurred during this run. Existing interrupted
v2.1.18 assignments remain unresolved as the owner explicitly accepted for now.
All 130 prior executions compare exactly equal before/after this fresh run.

## Measured run

- Tag: VOC-Experiments/demo-core v2.1.19.
- Source: fc3acdb94779dafb879c183143c20e19b435e766, unchanged approved source.
- Tag POST started 19:50:45.445577 UTC; success 19:50:46.383997 UTC.
- Final Published SSE received 20:10:36.505627 UTC.
- End-to-end elapsed: 1191.060s, or 19m 51.060s. This includes GitHub work,
  feed-writing order, Pages/HTTPS confirmation, controller processing and delivery.
- Austrian local interval: 20:50:45–21:10:36 on 2026-10-31.

| Target | Four executions Published | Final join receipt recorded (UTC) | Tag POST → final join SSE |
| --- | ---: | --- | ---: |
| debian-trixie-amd64 | 4/4 | 2026-10-31T20:02:32Z | 708.472s |
| debian-trixie-arm64 | 4/4 | 2026-10-31T20:00:08Z | 567.438s |
| fedora-43-x86_64 | 4/4 | 2026-10-31T20:01:43Z | 659.447s |
| openwrt-24.10-x86_64 | 4/4 | 2026-10-31T20:06:45Z | 961.478s |
| openwrt-25.12-x86_64 | 4/4 | 2026-10-31T20:10:35Z | 1191.060s |

## Exact evidence and graph release

All 20 build workflows, 20 publication workflows and 20 associated Pages workflows
completed successfully in the independently captured GitHub metadata. Each
publication has exactly one successful completed Confirm public receipt job and
a successful matched Publish job in its authenticated pinned run/attempt. Native
confirmation actually ran after the intentionally skipped OpenWrt tools job.

Every published fact was observed after its confirmation completed. Every one
of the 15 non-root executions first became Building in the SAME committed SSE
patch as its last required prerequisite's publication. All its exact dependency
execution/attempt identities match the frozen graph. No early release, duplicate
attempt, automatic replay or unexpected new execution was observed. All 20 new
selections froze corrected publication workflow blob
ce11809b876f119ccb521edd27aee66b7c39d0ac and commit
2af037e60189b755dc8ce9f9711f801a12720d69.

The final HTTP graph equals the reconstructed SSE graph. Final browser check at
20:11:16.194 UTC found all newest 20 rows Published and 120 timestamps. A common
read-only digest over row ID/state and all six UTC timestamp attributes matched
committed job/receipt facts exactly (1335453258). All local displayed times include
seconds, use 24 hours and place time above date. Status was Live · GitHub job
events. No browser reload was used to obtain final state.

The page still includes the preserved older incomplete/failed work. Its total
counts are 2 Building, 4 Publishing, 16 Waiting, 4 Failed, 124 Published over 150
rows. Those active/failed historical counts are NOT failures of this new run.

## Separate timing boundaries

All stored/GitHub timestamps have second resolution. Intervals below are direct
observations; medians/ranges describe these 20 executions, not guarantees.

| Interval | Minimum | Median | Maximum |
| --- | ---: | ---: | ---: |
| Build authorization → GitHub run created | 3s | 3s | 4s |
| GitHub build run created → job started | 4s | 4s | 7s |
| GitHub build job, including setup/worker/cleanup | 28s | 39s | 169s |
| Build job finished → publication authorization | 7s | 21.5s | 47s |
| Publication authorization → GitHub run created | 2s | 3.5s | 8s |
| Publication run created → Publish job started (validation/tools/feed order/queue) | 24s | 71s | 167s |
| GitHub Publish job | 13s | 23.5s | 37s |
| Publish finished → Confirm job started | 2s | 3s | 10s |
| Confirm job (setup, Pages/HTTPS proof and cleanup) | 7s | 34.5s | 75s |
| Confirm finished → controller receipt read started | 3s | 7.5s | 46s |
| Controller committed receipt read through GitHub API | 0s | 2s | 7s |
| Receipt read finished → committed Published SSE received | 1.210s | 3.547s | 21.541s |

Build-result and confirmation observation intervals include GitHub notification
latency, authentication/API/receipt/artifact work, controller work and DB queuing.
Publication run creation to Publish start includes prior validation/tools jobs,
feed-writer ordering and runner queue; it is not pure GitHub queue time. Confirm
job duration includes setup and cleanup; it is not isolated HTTPS latency.

Receipt read completion to Published SSE delivery includes the controller's
remaining state/transaction/DB queue and SSE transport. Its maximum 21.541s is a
real observed delay; it must not be presented as instantaneous release or hidden
inside package build time. Same-transaction dependent authorization was verified,
but exact DB commit time and browser DOM delivery latency were not instrumented.

No exact REST-ingress → outgoing HTTP-request-start pair is recorded. Persisted
send.at is generated while its queued database mutation executes, not on network
send or HTTP response arrival. Therefore this acceptance cannot supply exact
controller-only overhead. Authorization → GitHub run creation (3–4s for builds)
is a useful measured boundary, including commit/dispatch/network/GitHub costs.
No production instrumentation or unsupported overall speedup claim was added.

## SSE and runtime

246 SSE messages: two initial snapshots on two connections and 244 incremental
updates, totaling 963053 incremental payload bytes, largest patch 39839 bytes.
The source-selection transaction added all 20 new rows at once; the observer
reconnected at that boundary and obtained a fresh initial snapshot. It stayed
connected for subsequent progression, with 41 heartbeat comments and no observer
error. There is no claim that the diagnostic observer measured socket pending
output during this acceptance; bounded backpressure was validated separately by
the focused real-browser/slow-reader test.

No UI or production source changed during the run. Running controller PID 412713,
owned MariaDB PID 412714; external MariaDB untouched. Local controller remains
running after diagnostic observers stop. No installation or production SNodeC
workflow/feed/tag/database/service change.

## Source, checks and accounting

Checkout /home/voc/projects/snodec/core-ci-new, feat/graph-controller, unborn
controller branch/no controller commit, remote or PR. Approved sandbox workflow
commit [2af037e](https://github.com/VOC-Experiments/ci-coordinator/commit/2af037e60189b755dc8ce9f9711f801a12720d69)
remains active; its pin was not changed again for this run.

Binary SHA256: 9eebe99fd8a1c4d9d7d1d62a7764ca0f3502ba1ed1b5963750c51617220e40bc.
Source-manifest SHA256: d5a06d2b11f994a83ccd85b93a143892d9701d8df5e7ab99f388966ba068622b.
Build /tmp/codex/build/core-ci/controller, GCC 16.2.0 Debug, installed SNode.C.
Manifest re-verification passed during acceptance. No rebuild/source change.

Controller production remains 2331 C++ physical lines, net +3 for the authorized
fix: Execution.cpp +1, Graph.cpp +2. No additional production/test/workflow edits
for this fresh run; documentation/diagnostic evidence only. Per-file counts,
focused test names/results, 347 new test lines and CMake +8 wiring are recorded
in [the implementation report](publication-confirmation-acceptance.md).

Raw evidence: /tmp/codex/build/core-ci/core-v2.1.19-20261031, including baseline,
final graph, independent GitHub runs/jobs/deliveries, reconstructed SSE history,
inspection.json, timings.json and ui-final-check.json. Temporary diagnostics are
retained for review, not installed or part of production. No additional checkout.
Existing unrelated source/images/credentials preserved.

This demonstrates the requested full sandbox chain for this exact revision and
run. Receipt confirmation is the existing workflow's authenticated HTTPS
observation; it does not prove simultaneous availability at every CDN edge.
Historical interrupted work and exact overhead instrumentation remain limitations.
