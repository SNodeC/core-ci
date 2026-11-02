# Incremental SSE delivery — 2026-10-31

The source validation below preceded live activation. The owner subsequently
authorized the local restart recorded at the end of this report. No GitHub
workflow change, manual reconciliation or new build trigger was performed.

## Behavior and authority

MariaDB remains the sole durable graph authority. Store::change computes RFC
6902 changes from its transaction's before/after graph values. Empty changes
skip UPDATE, revision increment, dispatch and notification. Nonempty changes
notify only after successful write acknowledgement. No dispatch or notification
follows a rejected or uncertain write. Published/readiness behavior is unchanged.

Each SSE connection receives the full committed graph initially. Registration,
initial send and subsequent commit callbacks execute in one event loop, so a
change cannot interleave halfway through subscription setup. Afterward messages
contain only changed fields, not full execution/history replacements. The browser
applies add/remove/replace patches to presentation data, including escaped JSON
Pointer paths and array edits. All existing details and evidence remain available.

The per-peer map replaces the old response vector. Its only additional value is
the cumulative queued-byte position immediately after the initial response.
Unsent initial bytes are subtracted from pending output for allowance purposes.
New output, including heartbeat comments and conservative HTTP chunk framing,
must fit within 65,536 bytes beyond that initial remainder. Otherwise close the
peer immediately. A temporary copy of the small peer map keeps iteration valid
if disconnect cleanup removes a peer. EventSource reconnects to a fresh snapshot.
No custom pipe/source, extra timer, pending-event queue, graph cache or replay.

The bound covers application socket output: one remaining initial framed
snapshot plus 64 KiB, shrinking to 64 KiB. It does not bound OS TCP buffers,
browser memory or all transaction JSON allocations. A single oversized patch
also causes reconnect, even for a healthy peer. Actual changed graph transactions
still serialize/persist the full MariaDB graph; this change does not establish
zero controller overhead or solve every possible graph-scaling cost.

## Validation

- Build: GCC 16.2.0, Debug, installed SNode.C from
  `/usr/local/lib/cmake/snodec`; no framework modifications.
- `mariadb-commit-and-restart-contract`: passed, 1.03 s. Nineteen duplicate
  selections produced no additional revision or notification. Applying each
  patch reconstructed independently visible committed state. Rejected and lost
  write acknowledgements emitted neither dispatch nor notifications. Restart
  retained assignments without replay.
- `graph-and-receipt-contract`: passed, 0.06 s; graph/state behavior unchanged.
- `native-sse-backpressure-contract`: passed, 19.71 s. Real isolated MariaDB,
  actual REST retirement mutations and two TCP readers. Initial snapshot
  4,331,011 bytes; 24 ordered update lines of 4,237–4,240 bytes. One mutation
  occurred before the healthy reader consumed its initial snapshot; snapshot
  and update still reconstructed the expected order. The blocked peer closed;
  healthy delivery continued and reconnect recovered all 24 committed changes.
  The 24-mutation/delivery/reconnect scenario took 13.95 s, including full-graph
  MariaDB operations. This is not an isolated controller-overhead measurement.
- `native-startup-http-contract`: passed, 102.99 s. Existing initial failure,
  response-lifetime, TLS/HMAC, listener and managed/unmanaged ownership assertions
  remained. The same idle SSE connection survived 90 s with three heartbeats
  and no graph mutation. Real Chromium checked populated/changing UI, actual
  retirement patch delivery, within-state progress/timestamps, details, ordering,
  local times, links, expansion/navigation and reconnect.
- The controlled browser SSE fixture delivered an 869-byte patch; its progress
  and timestamp changes were observed within 4 ms of the test sender's write.
  The complete update/reconnect scenario took 3,132 ms. This browser measurement
  excludes MariaDB processing and GitHub, and is not production latency.
- clang-format dry-run, cmake-format check and JavaScript syntax checks passed.
  All production C++ lines are at most 160 characters.

The first runtime attempt lacked PLAYWRIGHT_BROWSERS_PATH. The next exposed a
test reader that did not skip a valid CRLF SSE separator; that parser was fixed.
No production checks were weakened. Pressure testing is now its own CTest entry,
so it can run without repeating the independent 90-second idle scenario.

Chromium fixture screenshot: `/tmp/codex/build/core-ci/runtime/populated-ui.png`.
It is not the live 8080 page. No new GitHub queue/setup/build/publication timing,
HTTPS publication timing, full-chain acceptance or overall speedup was measured.

## Accounting and identities

| Production C++ file | Physical lines |
| --- | ---: |
| src/Auth.cpp | 137 |
| src/Auth.h | 17 |
| src/Controller.h | 94 |
| src/Evidence.cpp | 272 |
| src/Execution.cpp | 373 |
| src/GitHub.cpp | 307 |
| src/Graph.cpp | 435 |
| src/Http.cpp | 130 |
| src/Managed.cpp | 161 |
| src/Managed.h | 24 |
| src/Store.cpp | 103 |
| src/main.cpp | 275 |
| **Total** | **2,328** |

Production: C++ +27 (Store +6, main +21); UI JavaScript +19, now 349 lines.
No other production source changes. This is below the approved growth estimate
and 172 C++ lines below the hard cap. Test sources: store.cpp +16, runtime.py +4,
ui.cjs +75, new sse.py 75; total +170 physical test lines. CMake adds one focused
test entry (+8 lines). Documentation changes: AGENTS.md, README.md, WORK_LOG.md,
this report and the source hash manifest; GitHub wire contracts are unchanged.

Physical test C++ counts: tests/artifact.cpp 38; tests/auth.cpp 93;
tests/graph.cpp 249; tests/https.cpp 35; tests/store.cpp 209;
tests/fixtures.h 237. Total test C++: 861, excluded from the production cap.

Checkout: `/home/voc/projects/snodec/core-ci-new`, branch `feat/graph-controller`.
The branch has no commits and no remote; no commit, push, PR or deployment.
Build: `/tmp/codex/build/core-ci/controller/core-ci`.
Binary SHA-256:
`ece3700fc29ef2c04741a8c26a992edc65f4d82d71340ba31d53e72e34a04c10`.
Source manifest `docs/source.sha256` SHA-256:
`87cf0bf9dea2285f0239e7077d99d747039c04360921d7398f2a9c3ac510d5d7`.
Reports are excluded from that manifest to avoid self-reference.
Task diagnostics and the review patch are under
`/tmp/codex/build/core-ci/sse-fix-20261031`; retain until review, then removable.
Isolated test databases/keys are removed by their test drivers. User screenshots,
unrelated work and the live controller's resources are preserved.

## Subsequently authorized local restart

The owner explicitly requested "Restart!". The old controller PID 262184 received
SIGTERM and exited with its owned MariaDB child. The same approved startup
arguments, database directory/socket, public deployment inputs and secret paths
started the validated binary as PID 346835, with owned MariaDB PID 346836.
Existing files/history were retained; no socket removal or database reset.
Both listeners are active: management 127.0.0.1:8080, TLS webhook 0.0.0.0:8443.

The complete 90-execution graph before/after restart compared equal, including
selections, attempts, assignments and facts. Initial SSE data matched the HTTP
graph, with a 3,508,733-byte data line. A live graph HTTP read took 0.456 s; that
single observation is not a performance guarantee. The running executable's
SHA-256 matches the validated binary above. Both existing Chrome 8080 tabs were
reloaded with the updated UI; the page populated and showed Live, 84 published,
3 historical waiting and 3 failed executions. No work was replayed or selected.
Dynamic patch/backpressure behavior was verified in the isolated tests above;
no fresh GitHub chain was triggered to remeasure live transition latency.
Restart evidence: `/tmp/codex/build/core-ci/sse-restart-20261031`.
