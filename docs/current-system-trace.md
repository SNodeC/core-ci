# Read-only baseline and replacement trace

Trace date: 2026-10-24, host jupiter. No existing checkout was edited for this design. Local paths identify evidence, not application build/runtime dependencies.

## Baselines

| Checkout | Branch / HEAD | State and role |
| --- | --- | --- |
| `/home/voc/projects/snodec/snode.c` | fix/package-orchestration @ d795f95f4b4a82b59bed6ec13df1910d2a0511a4 | Clean at inspection; framework examples and public APIs, not modified |
| `/tmp/codex/checkouts/VOC-Experiments-ci-coordinator/canonical-ci-live-validation` | main @ d7b4c31c9fdae384e25b48323a0a5e557864e7e4 | Existing local workflow/admission/render/docs fixes preserved |
| `/home/voc/projects/snodec/core-ci` | new main | Authorized sibling repository; initial documentation only |

The sandbox remote's default branch was main, last pushed at 2026-10-24T11:16:21Z when checked. Local sandbox modifications are not claimed deployed. No current remote claim about generic-code equality is made after those local changes.

The existing DP/source task worktrees remain separate. This design does not merge their changes or alter live source tags. GitHub SNodeC/core-ci was absent at the name check; repository creation is explicitly requested by the user.

## Current production/config physical sizes

Measured from the sandbox working tree, not HEAD alone:

| File | Physical lines | Bytes |
| --- | ---: | ---: |
| ci/dispatch.py | 799 | 55,234 |
| ci/repository.py | 835 | 46,875 |
| ci/publish/publication.py | 662 | 45,448 |

These are complete mixed-responsibility file sizes. Do not call all 2,296 lines deletable orchestration. They include worker/publisher behavior that survives. The line estimates below count mechanisms once, not whole files.

## Authority → transformations → persistence → consumers

| Concern | Current source location | Current persistence/consumer | Proposed disposition |
| --- | --- | --- | --- |
| Direct intake | dispatch.py:intake:70; repository.py:configure:286 | verified workflow input and direct requests | Move acceptance/validation/activation to server; capture worker verifies pinned input |
| Highest graph | repository.py:requested_projects:154, validate_projects:139 | requests → tick/root selection | One Graph policy in Scheduler, validated in local transaction |
| Capture allocation | publication.py:reserve:329, shared_revision:313; repository.py:capture_inputs:465 | feed counters/allocations/root source link | Local Capture/reservation transaction; keep input meaning and row claim tests |
| Immutable bundle | repository.py:prepare:474, dispatch.py:baseline:123 | bundle/context/profiles/checksums and artifact | Keep trusted GitHub capture preparation; change registration RPC and remove feed scheduling writes |
| Generation registration | publication.py:record_run:373 | feed runs/request eligibility | Transactional capture/register handler, no source-worker feed-write token |
| Work/arm policy | repository.py:work_index:231, cascade_blockers:249, binding:764 | per-row nodes plus feed release manifests | One C++ Scheduler policy; disposable memory indexes; shared publication authorization |
| Reconciliation | dispatch.py:observe:349, coverage:429, tick:461, apply_report:701 | feed node transactions → API calls → projection | Replace with durable transitions, known-run reports/reconciliation and local timers |
| Current capacity | dispatch.py:capacity:197 | org metadata, five run-status scans per repo, per-run jobs | Delete in new mode; configured own-worker budget, explicit policy change |
| Concurrent scheduling writes | dispatch.py:change:155 / transaction callers | status.sh/push.sh base-value CAS | Local transaction; preserve feed CAS for package/publication writer |
| Highest display | publication.py:projection:245, render:97 | status pages, inventory, README blocks | Single committed-state Projection with equivalent locally corrected behavior |
| Package publication | publication.py:publish:392; repository.py:publication_needed:768 | build.json, signed indexes, package files, receipts | Keep byte verification/signing/feed writer; replace only scheduling-state consultation |
| Authored refresh | publication.py:copy_site:569 and README block preservation | site/install/key docs vs generated blocks | Keep, with one deployment-authored header authority |
| Wake/retry | dispatch.py:main:729 and control-packages.yml | workflow_run/report/manual/schedule | Replace control workflow with continuously running server; keep worker reports |

Current selected function spans: tick 238 lines, observe 78, change 40, capacity 38, intake 34, apply_report 19, coverage 12; render 146, projection 38, reserve 42, shared_revision 14, record_run 17. These are measured physical spans, not a proposed C++ implementation LOC or deletion total.

## Workflow boundary trace

- orchestrate-packages.yml currently validates source notification, writes reserve to feed, prepares capture bundle, uploads it, then registers generation through `status.sh`. Per-project capture concurrency serializes aggregate runs. In the new design only worker execution/bundle integrity remain; core owns reserve/register and child coverage does not wait on aggregate completion.
- build-packages.yml is reused by each source wrapper. The existing local change adds best-effort started reporting before the source hook; completion reporting remains separately credentialed because jobs can outlive an initial token.
- sign-and-publish-packages.yml validates actor, restores capture-pinned trusted tooling, overlays current site/install/keys, verifies artifact, publishes and pushes. Keep trusted tooling/keys/bytes isolation. Replace scheduling ledger access with task/permit API and server policy; do not execute scripts from artifact uploads.
- control-packages.yml currently serializes separate control runs, observes state/capacity and renders. Remove that workflow as an authority in the refactored sandbox after quiescence. No GitHub cron is necessary to advance local durable deadlines.

## Local API cost finding retained as evidence, not a future benchmark

Old capacity scan: 2 + 5R + A API calls before additional pagination; R is visible repo count and A the distinct active runs inspected for jobs. A synthetic unchanged-function probe with 43 repos measured 217 calls idle and 260 with one active run/repo. A 5,000/hour shared user quota can be exhausted by 23 idle-cost scans. Full-wave totals previously discussed were timing-dependent estimates, not live caller-attributed measurements.

Do not use this to claim conditional caching was implemented. The server proposal removes the organization-wide observation responsibility entirely. Its targeted API budget still requires measurement.

## Framework evidence

`src/apps/jsonserver.cpp` shows middleware/routes/lifecycle; `src/apps/http/model/clients.h` shows async HTTP request callbacks; `src/core/timer/Timer.h` exposes interval/singleshot timers. `database/mariadb/MariaDBConnection.cpp:85` sets MYSQL_OPT_NONBLOCK and installs SNode.C read/write/exception receivers. Async connect/commit use connector start/continue functions. `database/mariadb/CMakeLists.txt:95` exposes snodec::db-mariadb. express::Request::body retains raw webhook bytes, so HMAC can run before JsonMiddleware without a framework patch. The current convenience DB API accepts SQL strings: safe parameterization remains a specific spike, not assumed solved.

Installed `libmariadb` pkg-config version on jupiter was 3.4.10. A mariadb CLI is present. This does not establish that a database server is installed, running, configured or approved. No DB connection, credential read, installation or schema creation was attempted.

## Architectural fault not hidden by the rewrite

The existing feed CAS jointly orders scheduling changes and package changes. A local DB plus remote feed cannot inherit that ordering automatically. The final publication fence is a proposed replacement, not deleted complexity. Its safety/availability trade-off is explicitly D3 in architecture.md.
