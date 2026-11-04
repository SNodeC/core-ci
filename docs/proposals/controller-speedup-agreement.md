# Agreed first performance correction

Status: design reviewed jointly with Claude; no implementation, commit, deployment or
new build session is authorized by this document. The owner asked for investigation and
agreement. The preceding suggestion about combining GitHub steps was set aside.

## What is actually slow

The live v2.1.6 chain completed all 20 builds and publications in 26 minutes 42.409 seconds.
Its graph selection took 14.475 ms; 301 readiness traversals totalled 625.686 ms. Every one
of the 15 dependent readiness transitions assigned the child in the same committed frame
that verified its last prerequisite. A new scheduling algorithm is not the first fix.

There are two distinct costs:

1. **Controller envelope.** The deployed binary is Debug, despite the deployment guide
   specifying Release. Full graph/SQL preparation had a 114.182 ms median and the inclusive
   application/continuation path a 185.967 ms median. Copying large hidden attempt fields,
   building intermediate views and serializing the same SSE payload repeatedly are
   unnecessary work. The 123.144-second union of instrumented synchronous intervals
   includes observer work, profiler overhead and descheduling. It is not CPU-only time,
   added critical-path latency or proof of a particular percentage of the whole run.
2. **Privileged job occupancy.** OpenWrt preparation repeats full official SDK acquisition,
   extraction and identity checks in validation and then inside the feed writer job.
   Eight OpenWrt prepare steps totalled 528 seconds, versus 26 seconds for all 12 native
   prepare steps. The 20 whole publisher jobs totalled 1082 seconds; 23 successful status
   jobs totalled another 331 seconds in the common write group. Those occupancies cannot
   simply be added to compute their effect on the critical path.

The long unlogged portion of OpenWrt prepare contains run-identity API work, official
metadata requests, SDK archive acquisition and extraction. Public logs isolate `defconfig`
and later work but do not prove that the archive download alone caused the variability.
Removing its duplicate acquisition is the selected small candidate; its net benefit must
be measured, including the new artifact transfer.

All 163 cancelled status workflows have saved terminal job listings with `jobs: []`.
They produced dispatch/API/webhook overhead, not demonstrated runner or write-lock time.
The extended generation-2364 status stall was upstream: the refresh job waited for the feed
writer group. Pages deployment and caching remain additional boundaries, not excluded causes
in general. No hard 1% or 10–20-second ceiling on controller contribution is established.

See [the live timing report](../reports/v2.1.6-live-timing.md) for measurements and limits.

## Agreed controller correction

Measure an unchanged optimized build first, then apply the small reductions independently:

- Build the deployable binary as Release using the documented configuration and matching
  dependencies. Do not represent a build-type change as a measured speedup before comparison.
- In `GraphStore::view`, copy only the fields exposed today. Stop copying assignment,
  build-result, preflight and dispatch data merely to erase them afterward. Preserve the
  complete current management/public JSON contract, including receipts and summaries.
- Serialize each SSE event body once per broadcast, rather than once per receiver. Keep
  initial subscription and repository-list events correct; retain no cross-event cache.
- Reserve the output capacity in `Database::text` before its existing hex encoding.

Exact before/after `/api/graph`, `/public/status` and SSE payload parity is required through
existing public fixtures. Keep one graph, the existing Database FIFO, current uncertainty
and callback semantics, and all source/dependency/publication checks. Do not add a projection
cache, new scheduler, new persisted state, alternate SQL engine or framework changes.

## Agreed worker correction: one existing bundle

Use the already existing validation-to-publication artifact, not a second SDK artifact,
second artifact ID, cache or reporting protocol. Its layout becomes:

```text
validated/
  packages/       # the existing strictly verified manifest/package/export files
  sdk.archive     # OpenWrt only; the validator's independently verified official SDK
```

The trusted wrapper supplies `--output .../validated/packages` to the unchanged validator.
After successful validation, an OpenWrt-only step copies its verified
`validation-sdk/sdk.archive` to the bundle's sibling `sdk.archive`. Assignment values are
not interpolated into shell commands. The source-derived files cannot supply that sibling.

The existing upload action uploads the explicit `validated/` directory with compression
level 0. It keeps the current seven-day retention, output ID, assignment-derived name and
same-run `needs.validate.outputs.artifact-id` download. Native bundles contain only
`packages/`. Package manifest validation continues to inspect only `packages/`, with its
exact-files, provenance and metadata rules unchanged.

The publisher reads packages from that subdirectory and receives a constant SDK archive
path via `--sdk-archive`. Presence is required only for OpenWrt prepare, after assignment
parsing and before run-identity network requests or key handling. Native preparation ignores
it. Push and confirmation do not require it.

`sdk.prepare(target, environment, destination, archive=None)` remains the single preparation
implementation. Its existing build/validation callers still download normally. For supplied
archives it rejects non-regular files, symlinks and sizes above the existing 1 GiB SDK bound,
then copies to its new controlled destination. It independently obtains official release
metadata and checks the complete archive SHA before extraction or execution. Full extraction
bounds, `defconfig`, target/version/architecture, compiler, staging, apk-tools, key and receipt
checks remain. Content checks occur inside SDK preparation, before extraction/execution;
that is after the existing run-identity and key-file setup, not before every network call.
Original source artifact identities, signed package identities and receipts are preserved.

Four flushed progress lines mark metadata start, metadata verified, archive verified and
extraction completed. These distinguish acquisition-plus-hashing from extraction and later
checks. They log no credentials. Ordinary buffered prints would not provide reliable phase
timestamps. Validation measures the original download arm; publication measures the supplied
archive arm in the same authorized comparison session.

### Cost that needs explicit acceptance

The two SDK archives used in the measured run have official HEAD lengths of 258,916,431
and 285,832,189 bytes. Four projects across those two targets add approximately 2.03 GiB of
artifact contents per comparable 20-target session, retained for seven days. Upload and
subsequent download together transfer about twice that amount through GitHub artifact
storage. The publisher also temporarily holds an extra archive copy on disk. These are
additional costs even though the second official archive download is removed. No storage
pricing or runtime saving is asserted here.

## Safe activation without breaking old assignments

No implementation or activation is performed in this review. In a later authorized stage:

1. Commit the new trusted tooling while leaving existing workflow wrappers byte-identical.
2. Render new source aliases offline using `deployment/render-source-workflow.py`. This
   uses the same template and replacements as `PipelineConfig::sourceContent` and reports
   the Git blob SHA. The deployed binary must embed that same template revision.
3. Install the new publication alias and the four source aliases on their respective main
   branches. Verify exact bytes and blobs before selecting them in controller configuration.
   Preserve the existing paths/blobs so historical explicit retries retain their immutable
   authority. No selected source or stored assignment is rewritten.
4. Update the five relevant existing configuration values: tooling SHA, source workflow path
   and blob, publication workflow path and blob. Status stays on its existing approved
   wrapper and tooling; no status alias is needed for this change.
5. Select the new demo-core root explicitly before allowing new below-core selections.
   `Graph.cpp` requires an exact deployment match for an existing prerequisite. Until the
   new root selection exists, a below-core selection cannot reuse an old-deployment core.
   Coordinate tag creation for that narrow transition; do not weaken the identity rule.
6. Verify old explicit retries still use old paths and that both publisher generations and
   status use the same actual feed write group. The controller's graph remains authoritative.

GitHub's concurrency boundary applies to the whole configured job. Preparation cannot simply
be moved to an earlier step in that same locked job and become concurrent. Keep the shared
write group and normal checked push. A failed possibly executed push does not establish
failure certainty; preserve assignments and uncertainty until the required proof exists.
[GitHub concurrency documentation](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency).

This is one bounded upgrade, not a general history migration or a promise that two wrapper
generations settle every future upgrade. Existing sandbox authority is not revoked; commits,
GitHub writes, configuration, restart and activation retain their stage-specific authority.

## Verification required for implementation

Use affected existing graph/store/database/pipeline/SSE/worker boundaries, not a full suite:

- Optimized before/after timing and exact graph/public/SSE JSON parity, with real isolated
  MariaDB and the existing synthetic GitHub fixtures clearly identified.
- Multi-receiver SSE consistency and correct initial/repository events.
- Both download and supplied-archive SDK paths yield identical target/toolchain/staging
  facts. The supplied path performs no archive curl, while retaining metadata verification.
- Missing OpenWrt prepare input, wrong hash, symlink, non-regular and oversized inputs fail
  at the stated boundaries. Wrong bytes never reach extraction/execution. The supplied
  archive remains unchanged. Use a sparse oversized fixture rather than allocating 1 GiB.
- Native bundles without an SDK succeed. Unexpected files inside `packages/` remain rejected;
  a sibling SDK archive does not become package or executable authority.
- Exact original artifact/assignment/receipt semantics, missing/expired evidence handling,
  join gates, explicit retries, standard webhooks and mutation non-replay remain.
- New aliases match configured hashes; old bytes remain; common feed exclusion and historical
  retry compatibility are checked. New root selection precedes dependent selection.
- A later authorized sandbox run measures SDK phases, upload/download overhead, lock waits,
  controller timings and Pages visibility separately. Require a net improvement; no 10–20×
  promise or pure critical-path subtraction follows from this design.

## Provisional implementation bounds requiring owner approval

| Checkpoint | Production estimate | Test estimate |
| --- | --- | --- |
| Optimized build | No source lines | Existing focused checks |
| Controller reductions | Aim neutral; at most +5 net C++ lines | +20–40 |
| SDK bundle/preparation change | +15–25 net Python lines | +45–70 |
| New publication/source aliases | Approximately 300–330 physical YAML lines | Covered above |

Current C++ is 3,571 against the approved 3,572 cap. A +5 result would require a 3,576 cap;
try reductions first. The new alias files count as production, even though much of their
content is preserved workflow logic. Five configuration value changes are deployment work,
not permission to read secrets. Estimates are not growth or activation authorization.

The proposed maximum is +5 C++, +25 worker Python, +330 YAML and +110 tests, with documentation
accounted separately. Source inspection or measurement that requires a different mechanism
or exceeds those bounds must return for review before implementation.

## Explicitly deferred

- A new status latch, timer, queue, polling loop, state-only public filter or status authority.
  Reducing cancelled dispatches alone does not remove the successful status jobs' lock cost.
- Publishers fetching live controller facts as a prerequisite for package publication.
- Host-tools-only publisher verification, unverified executable handoff, new SDK caches or
  registry/client mechanisms. Full publisher SDK verification stays intact.
- Faster confirmation polling: the current loop has a 600-second deadline, and shortening
  its sleep increases request exposure rather than shortening that deadline.
- Format-specific dependency installation, prepared images and other environment changes.
  Their potential value remains, but they are separate from the first SDK comparison.
- Splitting the feed group, force-push, automatic mutation retry, pruning history or weakening
  receipt/public-byte proof to make the run appear faster.

## Review record

Claude read current source, immutable deployed workflow/tooling evidence and measured job
steps. Three review rounds and a final confirmation resolved the engineering objections.
Final confirmation: session `3962a698-0c29-4fd0-a2b7-ed7330675520`, evidence at
`/tmp/codex/build/core-ci/speed-review/claude-confirmation.json`. All nine inspected worker
Python files were byte-identical to the deployed tooling pin. No code, tests, configuration,
credentials, tags, feeds or service state changed in this analysis. No tests or new live
performance measurements were run. Remaining choices are owner approval of implementation
bounds, artifact cost and the later authorized activation schedule.
