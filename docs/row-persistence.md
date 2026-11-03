# Row persistence: Phase 1 checkpoint, 2026-11-03

**Phase 1 source implementation and isolated validation are complete.** The owner
explicitly accepted retaining the measured hydration tradeoff on 2026-11-03 and
instructed completion. This resolves the §12 performance gate: the full-read
median remains 613.487 →643.2215ms (+29.7345ms/+4.8468%). No further production
change was made to obtain acceptance. The measured local browser mutation-path
improvement remains a separate result, not a build-chain speedup claim.

Changes are reviewable and uncommitted. Migration, deployment and live acceptance
remain outside this phase and have not been performed.

## Source and allowance

Canonical checkout: `/home/voc/projects/snodec/core-ci-new`, branch `main`, HEAD
`e518068986f0bca2e4726b80692be4571f9e596f`, with uncommitted task changes.
No branch switch, staging, commit, push or merge. Debug build: GCC 16.2.0,
`/tmp/codex/build/core-ci/db-phase1-20261103/build`.

The owner first raised the total cap to 2,750 and approved Store438/Header109/
Execution400. They subsequently approved **490 formatted Store lines**. With
other allowances unchanged, their arithmetic ceiling is 2,802; the patch totals
2,794. It does not satisfy the superseded 2,750 arithmetic. AGENTS.md is protected
and has not been rewritten to reflect these subsequent owner instructions.

| Production C++ | Before | After | Delta |
|---|---:|---:|---:|
| Auth.cpp | 137 | 137 | 0 |
| Auth.h | 17 | 17 | 0 |
| Controller.h | 107 | 108 | +1 |
| Evidence.cpp | 272 | 272 | 0 |
| Execution.cpp | 389 | 394 | +5 |
| GitHub.cpp | 306 | 306 | 0 |
| Graph.cpp | 437 | 437 | 0 |
| Http.cpp | 158 | 158 | 0 |
| Managed.cpp | 161 | 161 | 0 |
| Managed.h | 24 | 24 | 0 |
| Store.cpp | 194 | 489 | +295 |
| main.cpp | 291 | 291 | 0 |
| **Total** | **2,493** | **2,794** | **+301** |

Maximum production width: 146 columns. Store is below490, header below109 and
Execution below400. No production JS/HTML/CSS changes belong to this task.
Test-only growth: C++ +242 lines, Python +183, JavaScript +2 over the protected
checkpoint, and CMake test wiring +7. No prior UI assertion was removed.
CMake changes add test targets only. The source/build hashes and reproducible
count inventory are in `identity-final.json` in the task artifact directory.

## Responsibility and exact storage

MariaDB remains the sole durable authority. A mutation must commit state, exact
assignments and newly ready dependents atomically before dispatch, then SSE.
No scheduler, cache across operations, second observer/readiness implementation,
timer, retry, replay, migration fallback or native framework change was added.

Store replaces the single ci_graph body with repository, execution, attempt,
content and graph_root. Body/refs encode direct container children only; hashes
identify expanded canonical values. All other data remains literal. There are
no in-payload reference markers or semantic aliases. Repository definitions remain
literal current assertions (4,390 copied bytes in the original corpus), so this
is not a claim of a mathematical minimum. Unknown root fields use graph_root.

DDL: all tables are InnoDB, utf8mb4/utf8mb4_bin; body/refs are NOT NULL LONGTEXT
with JSON_VALID checks. Repository name is NOT NULL LONGBLOB with native full-value
UNIQUE (MariaDB HASH index). Execution id is ascii/ascii_bin CHAR(64) PRIMARY KEY,
with unique NOT NULL BIGINT UNSIGNED ordinal. Attempt id is VARBINARY(85) PRIMARY
KEY, execution is NOT NULL ascii/ascii_bin CHAR(64) FK, ordinal is NOT NULL BIGINT
UNSIGNED, with UNIQUE(execution,ordinal). Content hash is ascii/ascii_bin CHAR(64)
PRIMARY KEY. Graph_root has TINYINT UNSIGNED PRIMARY KEY CHECK(id=1), body, no refs.
No helper columns, revision column or automatic migration exists.

One ordered native SELECT sequence reads all five tables. One materializer and
operation-local decode dictionary serve every consumer. This is O(history).
Consistency depends on the stipulated single connection and single writer.
Webhook routing changes from1+2K graph reads to1+K for K observations; resume
changes from1+2N to1+N. Explicit reconcile remains2, new selection/request2,
enrollment/retry/retire/send outcome/raw API/SSE startup each1. The fresh guarded
change read remains. Request's enrollment and final selection reads are distinct.

One paired before/after traversal plans changed rows and unchanged compact UI
patches. Moved/removed identities, removed attempts and changed immutable execution
fields are rejected. New content only is inserted; historical unchanged rows are
not rewritten. Changed encodings roundtrip before writing; outgoing authorized
assignments match their frozen expanded root hashes.

Every mutation explicitly STARTs, success-chains row statements, and enqueues
COMMIT only after all succeed. No prequeued unconditional COMMIT. Definite errors
explicitly ROLLBACK; failed rollback calls native abort() then releases Store.db.
Null initialize/read/change handles report unavailability. Only acknowledged
COMMIT dispatches, then emits nonempty SSE or the existing null resync marker.
Unknown acknowledgement may mean a full atomic commit; it never causes replay.

Bootstrap refuses legacy, partial and mixed owned schemas; unrelated tables are
ignored. Existing five-table schemas are materialized before startup succeeds.
main's reconnect callback still does nothing when controller already exists.
No reconnection or replacement client is introduced after fail-closed release.

## Lossless size and measured costs

Original corpus:210 executions,211 attempts,4 repositories. Exact original graph
roundtrip is dump-byte-equal,8,311,566 bytes. All823 canonical hashes match:
208 assignments,206 manifests,206 results,203 receipts. Both receipt input forms
and distinct verification timestamps survive. The independent SQL oracle checks
expanded content hashes; adversarial tests cover unknown fields, marker-like data,
slash/tilde/numeric keys, null/absence, empty containers, array order and floats.

Actual original-corpus logical column bytes: **4,648,164**, a **44.08% reduction**,
including root metadata. Content rows:4,828. Phase0's coarse model:5,772,816 bytes
before root metadata. Fine refs total1,085,918 bytes; these are included, not extra.
Actual isolated InnoDB allocated data/index pages:7,700,480 versus8,929,280 bytes.
These are fixture layouts and insertion order, not a live disk-saving guarantee.
SQL hex encoding still doubles string payloads. Facts can grow beyond corpus row
maxima; there is no universal row bound or new quota.

Controlled Debug read comparison used identical corpus, database server, public
read harness, dependencies,64MiB buffer pool, alternating old/new runs and16 read
samples per implementation. Median full read **613.487 →643.2215ms (+29.7345ms,
+4.8468%)**. Primitive parse/dump costs remain in the raw evidence. This regression
activated the stop rule. The owner subsequently explicitly accepted retaining
this exact tradeoff; the gate is now resolved.

Same210-row browser fixture as Phase0: six successful retirement commits. Median
HTTP request-to-response1,498.34 →818.98ms; request-to-observed DOM1,556.52 →876.16ms.
Those include automation, rendering and database waits, not pure controller CPU.
The browser used only four loopback page/assets/events requests,218 SSE messages,
zero reconnects/errors,64px rows. Initial SSE376,189 bytes, largest event4,960,
unchanged from Phase0. This is not a GitHub or full build-chain speedup claim.

Per-operation Phase0 models remain sizing evidence:20-record selection46 row
writes/48 statements incl START+COMMIT; five publications plus ten ready dependents
35 row writes/37 statements; single internal observation/send change1 attempt row
plus transaction boundaries. New-code SQL audit directly verifies one attempt-only
internal write, no writes on repeats, and atomic commit-before-dispatch. The20-record
figures are models through existing Graph functions, not a new live chain run.

## Validation and limits

Final affected CTest selection passed **7/7**,182.01s: JSON diagnostics, native
startup/HTTP/browser, native SSE pressure, lossless rows, commit/restart,
publication confirmation and graph/receipt. Tests use isolated MariaDB and loopback
fixture listeners; no private configuration or GitHub mutation.

Verified: duplicate concurrent selections, exact prerequisite release after commit,
unchanged/no-write/no-SSE, internal-only silent writes, projection-failure dispatch
then resync marker, rejected and actual-lost-COMMIT acknowledgement outcomes,
restart without replay, original and adversarial raw equality, immutable guards,
mid-chain oversized packet rollback, native abort release and null-handle guards,
legacy/partial/mixed refusal and fresh/unrelated-table startup.

Actual public fail-closed fixture: legal4,096-byte retirement exceeds an8KiB fixture
packet limit with1KiB native buffer. No retirement or SSE event committed. Later
raw API/events requests return503 and retire/retry/reconcile/request return409 with
Database unavailable; static HTTP stays responsive. Already queued requests are
cancelled by native abort; no immediate503 completion guarantee is made for them.
Webhook's existing202/report-error policy is unchanged; that specific post-abort
webhook path was not separately exercised by this fixture.

Format-check passes. Canonical analyze target completed with warnings; two new
Store declaration warnings were corrected within the allowance. A targeted rerun
and its exact output are recorded in the final task log. The final touched-file
clang-tidy invocation exited0: Store has no reported warning; Execution retains an existing publicationReceipt declaration-parameter
name warning, plus three existing canonical-config MaxSize=-1 warnings. Existing
unrelated warnings are not a clean-analysis claim. Earlier failed runs remain in artifacts:
JSON/string comparison compile error, observer overload ambiguity, obsolete schema
assertions, corpus timeout caused by copying a large completion callback per row,
UI phase-state timing race, and pressure-fixture setup/separator/TIME_WAIT errors.
Each correction was tested; no production edits followed the performance gate.

Phase0 P1–P9 evidence remains applicable. Unsimulated: valid rollback failure with
a surviving session; kill precisely during COMMIT rather than lost actual OK;
mariadb_cancel failure; sanitizer instrumentation of installed native binaries.
Full atomic committed state after lost OK is verified, not partial success.
Target vchrist dependency provenance and end-to-end acceptance remain unverified.

Future import is design only: separately approved one-shot caller reuses this codec
on a copy, with independent raw/API dump, canonical hash, SQL and UI checks.
DDL commits implicitly. Separately authorized cutover stops the old writer first;
no dual writes/mirror/fallback. Drop ci_graph only after validated cutover and
separate approval. A stale binary cannot safely roll back after new authorizations.
No history deletion, GC or source-tag replay. v2.1.23 remains unselected here.

No live migration, DB setting change, deployment, restart, GitHub action or full
chain/interruption acceptance was performed. The complete system is not declared
working. Phase 1 source work is complete with the accepted measured tradeoff;
source changes remain reviewable and uncommitted.


## Subsequent deployment authorization, 2026-11-03

The owner subsequently authorized commit/push/merge, vchrist deployment and restart.
They approved a temporary75-line Store caller and2 CMake lines solely for the
current migration into a fresh destination. They explicitly require its source,
target and binary removed immediately afterward. The final controller therefore
retains2,794 lines and no permanent import capability. Future deployments start
with a fresh database. Historical no-live-action statements above describe Phase1;
activation and preservation evidence will be recorded separately after cutover.
