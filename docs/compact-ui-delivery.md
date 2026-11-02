# Compact management delivery

The controller sends a derived UI projection instead of the durable graph. New
connections receive repository metadata once, individual execution records, then
ready=true; successful commits send one atomic compact patch. MariaDB and the raw
/api/graph contract remain unchanged. Internal-only facts commit without UI output.
Projection failure cannot suppress a committed command: it closes streams after
commit so clients resnapshot. The browser no longer fetches the raw graph or dumps
frozen assignments/evidence. Readable attempt and retirement history remains.

Shared execution/attempt ID lookup replaces repeated loops. Store error handling
reuses checked() and native error callbacks. Readiness predicates, exact inputs,
authority-before-dispatch, history and uncertainty policy are unchanged.

## Size

All physical controller C++ lines, including comments and blanks:

| File | Before | After | Net |
| --- | ---: | ---: | ---: |
| src/Auth.cpp | 137 | 137 | +0 |
| src/Auth.h | 17 | 17 | +0 |
| src/Controller.h | 95 | 107 | +12 |
| src/Evidence.cpp | 272 | 272 | +0 |
| src/Execution.cpp | 399 | 389 | -10 |
| src/GitHub.cpp | 306 | 306 | +0 |
| src/Graph.cpp | 455 | 437 | -18 |
| src/Http.cpp | 158 | 158 | +0 |
| src/Managed.cpp | 161 | 161 | +0 |
| src/Managed.h | 24 | 24 | +0 |
| src/Store.cpp | 103 | 194 | +91 |
| src/main.cpp | 276 | 291 | +15 |
| **Total** | **2403** | **2493** | **+90** |

This leaves seven lines below the 2500 cap. Maximum C++ column is 146. app.js
is 361 lines (+7), maximum column 157; HTML/CSS are unchanged. CMake has zero net
line change: the realistic pressure test timeout increases from 60 to 180 seconds.
Tests and documentation are accounted separately from production. No generated
logic, blank/comment stripping or JavaScript scheduling authority was introduced.

## Focused checks

Debug build: build/codex/compact-ui, GCC 16.2.0, installed SNode.C 2.1.0 ABI.
Base source: 084e2fd (same production content as 21ef6b9).

- Graph/projection, native MariaDB commit/restart and publication-observation
  contracts passed. Includes exact dependent release, retained history, duplicates,
  rejected writes, dropped commit acknowledgements and no dispatch replay.
- Additional Store cases verify projection failure does not suppress committed
  dispatch/done, failed/unknown projection commits do not notify, internal facts
  commit silently and identical facts do not write/revise/notify.
- Native HTTP/TLS/HMAC/startup/heartbeat and Chromium test passed (103.98 s).
  Preserves layout, 24-hour seconds/time-over-date, order, multi-expand/navigation,
  Packages, readable history, action rules, uncertainty, and unsaved inputs.
  Both POST-before-SSE and SSE-before-POST definition refresh cases pass.
- Real pressure with 3000 execution records passed (121.63 s including setup).
  Startup contained 2709774 event bytes; largest event 1583. Twenty-four ordered
  updates were 4257–4260 bytes. A commit during initial delivery followed the
  consistent snapshot, the slow peer closed, the healthy peer continued, and
  reconnect reconstructed the current compact state. No padding field was used.
- Repository format/check target and 160-column check pass. Canonical clang-tidy
  completes; existing diagnostics remain. The new pointer-qualification diagnostic
  was fixed. The copied SSE client collection is intentional for disconnect-safe
  iteration. No framework or deployment configuration was changed for these tests.

Test corrections are recorded rather than hidden: an initial new test lacked its
local Json alias; the large pressure fixture exceeded MariaDB's default packet
allowance. Only the disposable fixture's client/server allowance was increased to
128 MiB. A later pressure invocation was interrupted before a result; the final
completed run passed. These are not live database changes.

## Isolated measurements

One Debug fixture run per version, with cached public graph data, real local
MariaDB/HTTP/SSE and Chromium; no production instrumentation or GitHub requests.
SSE event bytes include the data prefix and blank line, exclude HTTP headers/chunk
framing and any extra blank lines. The conservative sender reserve is 20 bytes.

| Scenario | Raw baseline | Compact |
| --- | ---: | ---: |
| Initial 170 executions, event bytes | 6685847 | 306583 |
| Largest initial event | 6685847 | 4960 |
| Twenty new executions, event bytes | 82155 (rejected) | 16698 |
| Step-only update | 125 | 150 |
| Timestamp-only update | 129 | 166 |

The twenty-record compact message accounts for 16718 bounded bytes, leaving
32434 bytes below the 48 KiB scenario target. The largest execution record in
that fixture was 1972 JSON bytes. The selection did not reconnect the healthy
browser; the raw baseline did. Longer ID-keyed paths account for small updates
being slightly larger; they still contain only changed display fields.

Browser navigation to Live measured 882.90 ms for 170
rows versus 1419.34 ms in the baseline. These are single samples, not
a latency guarantee. Sampled post-update rAF waits were 0.6 and
1.6 ms. Test workloads briefly overlapped; do not infer a stable speedup.

Local HTTP/commit elapsed (selection/step/timestamp) was
1845.04/1885.68/1799.17 ms. Store scheduled CPU excluding fixture
parsing/replacement was 1173.11/1234.14/1178.56 ms versus approximately
1170/1188/1171 ms before. This does not establish reduced controller processing
overhead. It includes raw graph read/compare/serialization. It is not a measurement
of production REST-to-GitHub delay, GitHub queuing/setup/build/publication, or
Pages/HTTPS waiting. No build-chain speedup or elimination of external waits is
claimed.

## Limits and activation

The compact initial snapshot is still fully buffered server-side and grows with
history. Individual events permit progressive rendering, not producer
backpressure. No public native drain callback, custom pipe/pump or history/replay
queue is added. The unchanged 64 KiB live bound can still close larger commits.
Pagination remains a later task.

The owner authorized commit, push, merge and deployment of this change to vchrist.
The service configuration, credentials, database/history, runtime prefix, workflow
pins and wildcard management binding must be preserved. Activation must verify
both binary and served UI identities and public compact SSE. Release metadata
records the actual merged source revision and binary/UI hashes.

No new GitHub build chain or clean interruption acceptance is part of this change.
Full end-to-end sandbox acceptance for this revision remains unexecuted.
