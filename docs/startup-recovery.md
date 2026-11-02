# Startup reconciliation implementation and verification

Implemented 2026-11-02 in /home/voc/projects/snodec/core-ci-new on the existing
feat/graph-controller branch. Base commit 87061ab3b9c712bb8b4a65a56f0ca1148e448b25;
origin/main merge 3fce12d58868c8d11c81f1cd15bc119c67eeeb44 has the identical base
tree. This change is uncommitted and has not been pushed, merged or deployed.
The separate reference checkout and eight unrelated root images are untouched.

## Behavior and invariant

After successful management-listener startup, Controller::resume reads one
MariaDB graph snapshot. It observes only the latest assigned Building/Publishing
attempts, binding every call to the snapshot's exact execution and attempt ID.
All observations use the existing Controller::observe path, including frozen
run/job/workflow authentication, result/receipt validation, evidence merging,
state transitions and same-transaction dependent authorization.

Startup never dispatches an existing assignment again. New verified evidence may
advance a stage or release exact dependents, whose new assignments commit before
normal dispatch. Independent observations continue when another run is unbound
or unavailable. Terminal/unassigned executions are skipped. Running/uncertain
work remains assigned. Repeated restart and concurrent webhook observation cannot
duplicate authorization. Initial DB/schema/listener failure starts no recovery.

No new state, flag, thread, timer, cache, queue, scheduler, dispatch replay,
authentication/reporting protocol, graph projection or alternate readiness policy.
No source/tag discovery, dependency reselection or historic date matching.

The callback reports observation errors/results per execution; it is not an
aggregate batch-completion barrier. The listener/event loop remain available
while asynchronous observations proceed. All persisted unfinished assignments
are eligible; no age filter or automatic retirement policy was added.

## Exact production and test changes

- src/Execution.cpp: +15 lines, one snapshot pass invoking existing observation.
- src/Controller.h: +1 declaration.
- src/main.cpp: +1 startup invocation after required listener success.
- Total: +17 production C++ lines, 2331 -> 2348, below approved +20–30 estimate
  and hard 2500 limit. Canonical formatting, braces and <=160 columns preserved.
- Tests: publication.cpp +89 net, publication.py +5, runtime.py +3 = +97 net
  test lines (163 inserted, 66 removed). Existing test harness reused, no
  production injection hooks or new test framework. CMake/UI/workflows unchanged.
- README, AGENTS, contracts, work log, source manifest and this verification
  report document the new startup semantics separately from historical acceptance.

| Production C++ file | Physical lines |
| --- | ---: |
| src/Auth.cpp | 137 |
| src/Auth.h | 17 |
| src/Controller.h | 95 |
| src/Evidence.cpp | 272 |
| src/Execution.cpp | 389 |
| src/GitHub.cpp | 307 |
| src/Graph.cpp | 437 |
| src/Http.cpp | 130 |
| src/Managed.cpp | 161 |
| src/Managed.h | 24 |
| src/Store.cpp | 103 |
| src/main.cpp | 276 |
| Total | 2348 |

## Focused checks

All tests use public entry points and disposable real MariaDB where persistence
matters. GitHub API doubles exist only in the test binary; actual production
observation, validation, state machine and Store code execute.

| Check | Result | Final elapsed |
| --- | --- | ---: |
| publication-confirmation-contract | Passed | 3.52s |
| native-startup-http-contract, including real Chromium UI | Passed | 104.99s |
| native-sse-backpressure-contract | Passed | 18.43s |
| mariadb-commit-and-restart-contract | Passed | 1.11s |
| graph-and-receipt-contract | Passed | 0.06s |
| canonical format-check target | Passed | — |
| changed Python syntax / production line widths / git diff --check | Passed | — |

Publication tests retain the original 20 cases and add nine primary recovery
cases plus separate-process repeat passes:

- Missed publication confirmation commits Published and exact dependent assignment
  together; an independent SQL connection verifies authority before dispatch.
- Running and unbound publication do not release work or resend assignments.
- An unbound/error execution preceding an independently successful publisher
  cannot block the successful release; terminal history and unassigned work skip.
- Actual Controller::webhook observation racing startup reads receipt twice but
  commits only one dependent dispatch.
- Missed definite build failure becomes Failed; a running build stays Building,
  preserving original assignment and attempt.
- A separate process after successful recovery skips the Published parent and
  observes the assigned child without replay. No receipt read or dispatch repeats.
- Rejected DB update emits neither dispatch nor committed SSE.
- Lost acknowledgement after a real successful commit preserves durable
  publication/assignment, emits no dispatch/SSE from the uncertain outcome, and
  a separate-process recovery does not resend that committed child assignment.

Initial failures were test-fixture defects (build workflow blob supplied as
publication blob, reused Unix proxy path). Corrected those fixtures while keeping
assertions. Strengthened independence by moving the error before success and
using the actual authenticated webhook entry point for the race. Final recovery
suite and formatting were rerun on the final test revision and passed.

The native UI/transport fixture is deliberately populated AFTER empty startup,
so its fake GitHub assignments cannot generate real network observations.
Populated/changing UI, timestamps, SSE and ownership assertions remain intact;
managed startup ownership uses an empty graph. Automatic observation of persisted
unfinished work is tested by Controller::resume with real DB and API doubles.
The successful build-result/artifact implementation itself is unchanged; the new
recovery-specific build cases exercise running/definite-failure outcomes, while
prior sandbox acceptance exercised successful results through the shared path.

## Source/build identity and limits

Source-manifest SHA256: 443789c8bd53fb3b5f50203092dbb23480c73335556e8406510771070b2d515c.

Controller binary SHA256: 185c18b622e7db3c6c2535fc2ba44ff9131f21e08a7da38ff58b36f2af260777.

Build: /tmp/codex/build/core-ci/startup-recovery-20261102, GCC 16.2.0 Debug,
installed SNode.C/OpenSSL/MariaDB interfaces. Compiler temporaries stay under that
build's tmp/ directory. No extra checkout or installation. Build and test records
are retained there for review; disposable test databases/keys clean up normally.

The previous live sandbox binary/path/configuration was not replaced or restarted.
No live executions were reconciled, retried or triggered. No database migration,
reset, credentials, public pins, GitHub worker/workflow or production resource
changed. No new performance or end-to-end GitHub acceptance claim: deploying and
restarting this revision can resume ALL persisted unfinished assignments and may
authorize new next-stage/dependent work, so it requires separate authorization.

This recovers missed execution evidence only for work already durably selected.
A source tag whose original notification was missed before a selection existed
is not discovered. Observation errors remain unresolved until later authenticated
notifications, explicit reconciliation or a later startup; no polling/replay.
