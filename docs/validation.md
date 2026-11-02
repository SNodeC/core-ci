# Implementation checkpoint — 2026-10-31

Local implementation checks pass, including both database ownership modes.
Full authorized sandbox acceptance passed across core → app and side → join on
all five targets: 20 verified publications and 40 unique successful GitHub runs.
Earlier failed/waiting selections remain intact. Production readiness beyond this
sandbox is not established; native confirmation required explicit reconciliation.
See [sandbox-acceptance.md](sandbox-acceptance.md) for exact live evidence.

The owner-supplied selection/proof rules are implemented: explicit older tags
preserve their source while prerequisites select highest SemVer once; publication
proof compares execution-specific committed and public HTTPS receipt bytes,
without inspecting current shared indexes.

## Ownership and identity

- Checkout: `/home/voc/projects/snodec/core-ci-new`.
- Branch: `feat/graph-controller`; no commit, remote, default-branch commit or PR.
- All task files remain uncommitted. The reference checkout was not changed.
  External interoperability contracts were extracted first. The owner later
  explicitly permitted reading its main.cpp for SNode.C usage patterns only;
  controller behavior was not taken as a design input. Other controller source,
  tests, UI, SQL, history, proposals, reports and logs remain excluded.
- Build: `/tmp/codex/build/core-ci/controller`, CMake Debug with tests enabled.
- GNU C++ 16.2.0, C++20, `-Wall -Wextra -Wpedantic`; SNode.C 2.1.0,
  OpenSSL 3.6.5, nlohmann_json 3.11.3, zlib 1.3.2.
- Tests: MariaDB 11.8.9, Python 3.14.10, Playwright 1.64.1 and Chromium
  156.0.8078.4, en-US locale and Europe/Vienna browser timezone.
- [source.sha256](source.sha256) records production C++, UI, tests, build wiring
  and canonical formatting/analysis configurations.
- Source manifest SHA-256:
  `b9a9948e15b4231c9b03cfa99291f75d1ceadea193a384a5a0d3dfe65e1eb191`.
- Built core-ci SHA-256:
  `37484576f4c06ffa5f0212e137b829e18413eec00f58a5e16b4eab1a4905707c`.
- Installed native MariaDB client library SHA-256:
  `1f14362cc944fb170dfa296e7a43b0e5f3c9ff51981e689aa441476e22d94fbd`.
- Installed native logger library SHA-256:
  `d6fa0ccf8513942cc4c5c91c5700c1af163e0c5c32c0422aad5df7f6c0dcca3b`.

## Production accounting

Physical counts include comments, blanks, utilities and bootstrap after canonical
formatting. No other controller-owned production C++ exists. The private static
build target uses these same counted sources, not an external custom library/SDK.

| File | Physical C++ lines |
| --- | ---: |
| src/Auth.cpp | 137 |
| src/Auth.h | 17 |
| src/Controller.h | 94 |
| src/Evidence.cpp | 346 |
| src/Execution.cpp | 392 |
| src/GitHub.cpp | 310 |
| src/Graph.cpp | 422 |
| src/Http.cpp | 130 |
| src/Managed.cpp | 161 |
| src/Managed.h | 24 |
| src/Store.cpp | 107 |
| src/main.cpp | 237 |
| **Total** | **2,377 / 2,500** |

There are 123 lines remaining. This continuation removes nine production C++ lines
by deleting an invented assignment bound and duplicate retry construction.
Complete receipts remain durable; only unused index inventories are omitted from
new dependency build inputs. C++, authored CMake and
UI production lines are at most 160 characters. All eight canonical tooling
copies match `~/.codex/formatting/` byte-for-byte.

Production changes are fresh C++, 454 lines in three presentation-only UI assets and build
wiring. JavaScript has no graph selection, readiness or scheduling authority.
No retention, editor, portal, worker/workflow changes, controller package client,
ready queue, timer, feed lock, admission quota or runner reservation was added.

## Tests and documentation

| Test C++ file | Physical lines |
| --- | ---: |
| tests/auth.cpp | 93 |
| tests/artifact.cpp | 38 |
| tests/fixtures.h | 238 |
| tests/graph.cpp | 181 |
| tests/https.cpp | 35 |
| tests/store.cpp | 188 |
| **Test C++ total** | **773** |

These test-only sources are gated by BUILD_TESTING. Python initializes isolated
fixtures/processes and generates archive inputs; a browser script checks the UI.
Those Python/JavaScript test drivers add 555 lines, separate from test C++.
No production injection hooks exist. Documentation is AGENTS.md, README.md,
WORK_LOG.md, docs/contracts.md, this report, live acceptance and the source hash manifest.
Canonical tooling copies are separate development configuration.

Commands on this source revision:

```sh
cmake --build /tmp/codex/build/core-ci/controller -j2
cmake --build /tmp/codex/build/core-ci/controller --target format-check
CI_BROWSER_MODULES=/tmp/codex/build/core-ci/browser/node_modules \
PLAYWRIGHT_BROWSERS_PATH=/tmp/codex/build/core-ci/browser/browsers \
ctest --test-dir /tmp/codex/build/core-ci/controller --output-on-failure
```

Build and canonical format-check passed without warnings. All six focused CTest entries passed.
After live-discovered corrections, the build and format check passed again.
Authentication/TLS/graph/persistence checks passed after the proof authentication
change; the native startup/browser check passed after socket reuse and cleanup
consolidation; graph/receipt, artifact and persistence checks passed after the
filename correction. The final native startup/browser test passed again with an occupied-port failure case and socket-reuse-aware availability probe. No passing result is claimed for an unexecuted test round.
No old or unrelated suite ran.

| Check | Result and meaningful coverage |
| --- | --- |
| github-authentication-contract | PASS: official HMAC vector, changed body/key/signature rejection, independent RS256 JWT verification/claims, canonical JSON/Git/SHA identities |
| graph-and-receipt-contract | PASS: highest SemVer/older request, independent roots/joins, exact frozen inputs/reuse, duplicate source notification after newer prerequisite, no replay, completed-run/later-job facts, terminal publication, timeout uncertainty, definite-failure retry, retirement/late facts, receipt bindings independent of mutable indexes, publisher-selected safe filenames and rejection of mismatched/unsafe paths |
| artifact-bytes-contract | PASS: stored/deflated/ZIP64/nested archives and summary; reject unrelated attempt/source/dependencies/version/job authority, changed hash/environment, unsafe paths, duplicate/extra entries, links and truncation |
| mariadb-commit-and-restart-contract | PASS: real isolated InnoDB, independent SQL observer sees committed authority before effects, concurrent duplicate requests dispatch roots once, parent proof/child authority visible together, rejected writes dispatch nothing, real committed UPDATE acknowledgement discarded by proxy with no dispatch or restart replay; one explicit native connection |
| native-https-contract | PASS: actual native TLS client with generated CA, certificate/hostname verification, authenticated POST/exact response, no redirect follow, HTTP 500 and wrong-host rejection |
| native-startup-http-contract | PASS: DB/schema initial failure releases resources without listening/pending startup; early managed-server exit and managed schema failure release the child without listening; managed success owns and reaps its server; required-listener bind failure releases startup resources without starting the webhook; unmanaged server remains alive; real 8080 graph, TLS/HMAC webhook and listener isolation; populated Chromium UI, multiple expansion, prerequisite expansion/scroll, local dates, heading/State sizing, retirement and within-state SSE progress |

`node --check ui/app.js` passed. The optional analyze target uses canonical
.clang-tidy; it was not run.

## Acceptance limits and final checks

Documentation changes in this continuation are the extracted renewed/input contracts,
complete sandbox acceptance report, this accounting, README status, UI-read
permission in AGENTS.md and the work log; the source manifest was regenerated.
They are separate from production code and tests.

All 20 renewed sandbox executions are verified published, including all five joins.
The initial standard-tooling trial remains four published, three definite failures
and three waiting selections. Its old expired certificate was not rolled back or
bypassed; the new explicit selection used the existing renewed workflow pair with
its same-key renewed public certificate. No worker/workflow source was edited.

The RPM join's first dispatch was definitely rejected with HTTP 422. Its assignment
and history remain immutable. An explicit retry reused the exact source, target,
workflow identities and terminal prerequisite attempts, excluding only their unused
index inventories from dispatch inputs. The new assignment was accepted, built,
published and independently proven. Full receipts remain in MariaDB.

Final relevant checks after the C++ reduction:

- Build and canonical format-check: PASS, no warnings.
- `mariadb-commit-and-restart-contract`: PASS, including >60,000-byte durable
  receipts with small exact build inputs, independent commit visibility and no replay.
- `graph-and-receipt-contract`: PASS, including dependent retry input preservation.
- `native-startup-http-contract`: PASS with real Chromium and isolated MariaDB;
  final run 4.05 seconds after the locale display adjustment. Tests use the normal
  SNode.C named-instance CLI on 18080/18443; live 8080/8443 stayed running. One
  test-driver port-edit typo caused an initial SyntaxError and was corrected.
- Live independent audit: PASS, 40 unique exact App run bindings, 20 exact
  committed/public HTTPS receipts and 15 earlier-receipt/later-index comparisons.
- Live copied UI: PASS, 30 actual rows, 24 published, four dynamic repositories,
  five published joins, search/filter, tabs, read-only definition, simultaneous
  expansion, prerequisite scrolling and zero browser page errors.

The owner explicitly requested the earlier UI template. Only its three presentation
assets were inspected/reused, adapting the fresh graph/API. UI assets grow 231
physical lines in this continuation; C++ shrinks nine. Test C++ grows 28 lines and
test drivers grow 16. The updated real-browser fixture verifies within-state SSE
progress, exact retirement, locale dates/hour format and stable grouped headings.

GitHub emits job events, not continuous individual-step progress. The UI identifies
step text as last reported and distinguishes completed publication jobs awaiting
public receipt proof. Native jobs in the existing renewed workflow skip confirmation;
explicit reconciliation was used. This acceptance does not prove unattended native
confirmation, actual tag creation/push, arbitrary repositories or production deployment.

Startup now follows the owner-identified SNode.C main pattern: configure named
instances before init, call init once, then start database work directly.
Only initial connection/schema success configures routes and starts listeners.
The owner confirmed that small nonblocking managed process ownership is allowed.
Readiness and process-exit receivers use public ReadEventReceiver lifecycle and
shutdown callbacks. There is no explicit SNodeC::stop, deferred startup, polling,
controller thread or controller timer. Persistence tests assert one explicit
connection. All controller tasks stay in the framework's single event loop;
counting its logger thread as a controller violation was an incorrect test.

The standard READY=1 notification is documented by
[systemd](https://freedesktop.org/software/systemd/man/latest/sd_notify.html).
The configured filesystem notification endpoint passed with the installed
MariaDB; the earlier isolated abstract-socket probe had failed with EACCES.
No host policy, framework or external service configuration was changed.

Automatic approval review rejected a proposed relaxation from exact workflow
commit binding to a commit-format check because it could accept an unrelated
commit. It was not applied; exact selected commit and approved blob checks remain.

## Timing and handover

| Segment | Evidence and limits |
| --- | --- |
| Controller processing | Local functional graph/DB checks pass; no isolated processing benchmark or comparison baseline |
| GitHub queue/setup/build/publication | Real run/job/step times recorded in live acceptance; allowance/queueing belong to GitHub |
| HTTPS/Pages proof | Real committed/public receipt equality verified, including receipt A after later publication B changed a shared index; external waits remain |
| UI delivery | Real local 8080 and changing SSE facts checked in Chromium, then 30 actual executions checked in the copied layout; no latency benchmark |

Local test durations establish no overall speedup. External writer/Pages waits
have not been proven eliminated.

No commit, push, PR, merge, installed system service, tag, database migration/reset
or production SNodeC resource change occurred. Authorized sandbox changes are
the App webhook URL, a foreground Jupiter development instance with an isolated
database, 54 existing GitHub workflow runs in total (40 successful renewed runs),
and 24 verified publications including the four earlier ones. One additional
committed dispatch was definitely rejected with HTTP 422 and created no run.
No workflow source or signing credential changed. Disposable local test
fixtures were removed. No additional checkout exists.

Build/browser artifacts and live acceptance evidence remain under
`/tmp/codex/build/core-ci/`. The acceptance database is now the durable authority
of the running instance and must not be deleted while it is in use. Retire that
instance explicitly before removing its runtime directory. Fixture screenshot:
`/tmp/codex/build/core-ci/runtime/populated-ui.png`; live screenshots/evidence:
`/tmp/codex/build/core-ci/acceptance/`.
# Later monitored rerun

The historical checks below precede the v2.1.14 monitored rerun. Current source
identity, fixes, counts, timing results and observation limits are recorded in
[monitored-rerun.md](monitored-rerun.md).
