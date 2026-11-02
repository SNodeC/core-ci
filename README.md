# Graph CI controller

Fresh implementation in `/home/voc/projects/snodec/core-ci-new` on
`feat/graph-controller`. The authorized VOC-Experiments controller runs on
`core.vchrist.at`. The owner authorized replacing that deployment with this
reviewed revision. Existing production workflows, feeds and credentials are unchanged.

Focused checks pass, including managed/unmanaged startup, commit uncertainty,
publication confirmation, bounded incremental SSE and the populated management
interface. The core v2.1.19 source trigger automatically completed core → app/side
→ join across all five targets: 20/20 Published, with 20 successful build workflows,
20 successful publication workflows and 20 successful Pages workflows. Native
confirmation ran after intentionally skipped OpenWrt tools; no manual retry or
reconciliation was used. See [the full acceptance report](docs/core-v2.1.19-run.md)
and [publication correction and checks](docs/publication-confirmation-acceptance.md).
Production deployment remains unverified.

After the management listener starts successfully, startup performs one observation
pass over the latest assigned Building/Publishing attempts. Existing GitHub runs
and receipts can recover missed execution notifications through the normal state
machine; newly eligible stages/dependents dispatch only after a successful commit.
Unbound, running or uncertain assignments are preserved without dispatch replay.
Explicit reconciliation remains available. There is no polling or recovery of a
source-tag event that was missed before any selection existed.

Observation GETs now have a bounded, request-local no-response reissue policy,
with durable attempt/stage uncertainty and safe diagnostics. This is best-effort,
without polling, and does not fix an identified outage cause. POSTs and received
responses are never retried. See [policy, clearance and limits](docs/observation-uncertainty.md).
Activation is recorded separately in WORK_LOG.md; clean interruption acceptance
for this change remains unexecuted.

The invariant is one MariaDB graph authority. Selections freeze source,
committed definition, target, tooling/workflow pins and prerequisite executions.
An attempt freezes its exact prerequisite publication receipts before building.
Only a successfully acknowledged atomic graph update authorizes dispatch.
Unknown commit/send outcomes preserve their original assignment without replay.
Verified publication and newly ready dependents commit together, then every
authorized command dispatches immediately in graph creation order.

The small implementation has three responsibilities: graph/state/persistence,
GitHub authentication and evidence/proof, and native bootstrap/REST/UI delivery.
There is no scheduler, persisted ready queue, admission policy, controller timer,
feed lock, worker execution, portal or definition/workflow editor.

## Build and focused checks

Dependencies are installed SNode.C public CMake components, C++20, OpenSSL 3,
nlohmann_json, zlib and MariaDB server/client utilities. Tests additionally use
Python 3 and OpenSSL's CLI. No controller SDK or external custom logic is used;
all controller-owned C++ is in `src/` and counted toward 2,500 physical lines.

```sh
cmake -S . -B build/codex/debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/codex/debug -j2
ctest --test-dir build/codex/debug --output-on-failure
cmake --build build/codex/debug --target format-check
```

Temporary checks use `/tmp/codex/build/core-ci/<task>`. The focused CTest entries
exercise authentication, generic graph/receipt rules, real MariaDB commit and
restart boundaries, actual TLS transport, and native startup/HTTP interfaces.
They create and remove their own databases and generated keys; they never read
private configuration or connect to a configured external database. The startup
tests require free loopback ports 18080/18443. All controller tasks run in SNode.C's
single event loop; there are no controller threads. Tests continue independently
when a different test fails.

For the optional real Chromium UI check, install Playwright and its Chromium
runtime under the permitted temporary build location, then set
`CI_BROWSER_MODULES` to that installation's `node_modules` and
`PLAYWRIGHT_BROWSERS_PATH` to its browser directory before running
`native-startup-http-contract`. It checks populated and changing authoritative
graph facts, multiple expansion, prerequisite navigation, local dates and real
retirement through REST/SSE. The screenshot is a fixture, not live execution.
The separate `native-sse-backpressure-contract` uses a blocked TCP reader and a
healthy reader against isolated MariaDB; it does not repeat the 90-second idle test.

`format`, `format-check` and optional `analyze` are explicit development targets.
Canonical formatter/analysis files are repository-local byte copies. Analysis
uses this checkout's `.clang-tidy` and exported compile database. Point clangd
at the chosen build directory; source files are never rewritten during a normal
compile. There are no generated production sources.

## Deployment inputs

External interfaces are specified precisely in [docs/contracts.md](docs/contracts.md).
No credentials, public identity values or approved pins are provided as defaults.
An operator must supply a public deployment JSON object with these fields:

| Field | Required value |
| --- | --- |
| owner | Authorized GitHub owner boundary |
| repositories | Object mapping granted owner/repository names to stable numeric IDs |
| controller_app_bot_id | Approved numeric GitHub App bot ID |
| worker | repository, repository_id, approved 40-hex tooling_sha |
| source_workflow | approved path and 40-hex blob_sha |
| publication_workflow | approved path, 40-hex blob_sha and dispatch ref |
| feed | public repository, repository_id, branch and HTTPS base_url |
| signing | approved gpg_fingerprint, usign_key_sha256, apk_key_sha256 |

These are deployment inputs, not a replacement for committed source definitions.
Enrollment supplies each source repository's definition/workflow branch. Each
selection resolves its workflow commit, verifies the approved blob and freezes
that identity. An unexpected branch move remains unresolved; it does not relax
evidence binding or authorize replay. Source tags resolve using `refs/tags/`.
SemVer precedence selects prerequisites once; equal-precedence distinct highest
tags or ambiguous matching targets are rejected rather than silently chosen.
Assignments must fit the existing worker's 60,000-byte input bound.

Runtime root options are `--deployment-file`, `--app-id`, `--installation-id`,
`--app-key-file`, `--webhook-secret-file`, `--database`, `--database-user`,
`--database-socket`, optional `--database-password-file`, `--managed-datadir`
and `--ui-root`. Supplying `--managed-datadir` starts the configured MariaDB server
executable with that already initialized directory, no defaults and networking
disabled. A SNode.C read receiver observes its standard readiness notification;
only then does the controller connect once. Another SNode.C read receiver observes
process exit. Framework shutdown signals and reaps that owned process without a
blocking wait. No controller polling, timer, thread or retry is involved.
The database socket must be absent before managed startup. Unmanaged mode
performs no server lifecycle action.
Protected input files must be regular, owned by the effective
user, non-symlinks, mode 0600 or stricter, and at most 64 KiB. Keys are loaded
only by the running application from explicit paths. A webhook secret has at
least 32 bytes. Database/server initialization, migration, reset and production
configuration changes are not performed.

Management binds to loopback port 8080 with the existing trusted local access
requirement. `webhook` is a separate named TLS instance, disabled until explicitly
configured with its certificate/key and enabled through native SNode.C options.
Use native `--help` with an explicit config file to inspect listener options.
Do not expose management externally without the separately agreed access setup.

## Public management and evidence boundaries

`GET /api/graph` and `GET /events` expose durable facts, with SSE updates for
within-state evidence as well as domain transitions. Management JSON POSTs are:

| Path | JSON fields |
| --- | --- |
| /api/enroll, /api/refresh | repository, ref |
| /api/request | repository, tag (empty means highest SemVer) |
| /api/retry | execution; only a definite failure permits a new attempt |
| /api/reconcile | execution; observation only, never dispatch replay |
| /api/retire | execution, acknowledgement; only unbound publication |

Each `/events` connection receives a complete graph object first. Subsequent
`data:` messages contain RFC 6902 arrays with only changed fields (`add`,
`remove`, `replace`), derived from the same before/after transaction values.
An unchanged operation performs no UPDATE, revision increment or broadcast.
Notifications and GitHub commands follow successful commit acknowledgement;
uncertain or rejected writes emit neither. Initial subscription and commit
callbacks run in the same event loop, preserving snapshot/update order.

SSE uses direct sends, without a custom pipe/source or delivery queue. Each peer
may buffer its remaining initial framed snapshot plus 64 KiB of updates and
heartbeats, including conservative HTTP framing allowance. The initial allowance
shrinks with SNode.C's cumulative sent-byte counter. An event that exceeds the
remaining budget closes that peer immediately; even a healthy peer reconnects
for a fresh snapshot if a single patch is too large. Existing 30-second heartbeat
comments and EventSource reconnect behavior remain. This bounds application
output buffering, not browser or operating-system TCP buffers. Changed graph
transactions still persist the complete authoritative graph in MariaDB.

`POST /github/webhook` exists only on the TLS instance and authenticates the exact
raw body with GitHub's standard HMAC. Repository/installation IDs and REST-read
run/actor/workflow/job/artifact identities constrain evidence. Source notifications
reuse already committed selections; duplicate notifications cannot select newer
prerequisites or replay dispatch. Explicit new requests may select current inputs
while preserving earlier frozen work.

Build results require the assigned successful run/job and exact artifact bytes
and manifest. Publication requires the immutable execution receipt from a trusted
committed feed revision to equal its independently served public HTTPS bytes,
with exact attempt/source/result/publication/package bindings. A later shared
index mutation is irrelevant. No current-index membership or hash is a readiness
condition. Unavailable/mismatched proof preserves the publication assignment.

No full-system or speedup claim follows from local tests. GitHub queue/setup,
build/publication and public HTTPS/Pages availability were observed in the
authorized sandbox acceptance. External writer/Pages waiting remains present;
no overall speedup has been established.
