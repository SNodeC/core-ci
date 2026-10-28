# Local controller and SDK handoff verification

The implementation is in the canonical checkout on `rewrite/slim-graph-controller`, based
on `a568290a96ad27b04736d4c2de38d6ad2a116d86`. Its starting production files match merged
`origin/main` (`d88ae51d5d6cfc3466c1143a1123789d7d8f5559`). At the local verification
checkpoint, no commit, push, deployment, restart, GitHub mutation or new live build had
been performed. Subsequent commit/merge stages are recorded in WORK_LOG.md.

## Implemented behavior

The graph, Database FIFO, dispatch authority, callback order, authenticated webhook and
publication uncertainty rules are unchanged. `GraphStore::view` skips copying the four
already-hidden attempt fields; every exposed field and full receipt remains. SSE serializes
once per broadcast. SQL hex encoding reserves its complete output size and appends its suffix
in place rather than constructing another large string. The deployable local build is Release.

OpenWrt publication accepts the validator's official SDK archive through the existing
validation artifact. `sdk.prepare` remains the sole preparation implementation: original
build/validation callers download, while publication copies the supplied regular file. Both
paths independently verify official metadata and the complete archive SHA before extraction.
Extraction bounds, target/compiler/staging checks, package signatures and receipt proof stay.
Four flushed phase messages identify metadata, archive verification and extraction boundaries.

The canonical publication template is
`deployment/source/core-ci-publish-bundle.yml.in`. It creates `validated/` before downloading
into `validated/packages`, copies the SDK in a separate OpenWrt-only step and uploads the
explicit parent directory with compression level zero and seven-day retention. The existing
same-run artifact ID carries the bundle to the publisher. Only prepare receives `--sdk-archive`.
Native preparation ignores it; push and confirmation do not require it.

The existing renderer accepts an optional `--template`, retaining its original default and
Git blob calculation. This supplies one rendering path for both source and publication
workflows, replacing a separate ad-hoc publication substitution. No guessed tooling SHA is
embedded. The existing publisher, status, reusable builder, source template and Pages workflow
remain byte-identical. Their hashes were recorded before and after.

The approximately 259 MB and 286 MB official SDK archives add about 2.03 GiB of artifact
contents per comparable twenty-target session, retained seven days. Upload plus download
transfers approximately twice that amount; publication temporarily holds another archive copy.
Net CI benefit requires live measurement of that cost against the removed official download.

## Local measurements

An isolated real MariaDB graph from the synthetic twenty-target `pipeline-large-indexes`
fixture supplied identical stored state for all three binaries. Each endpoint had one warmup
and fifty timed GETs. These are sequential local observations, not a throughput guarantee or
an estimate of critical-path contribution. Timings include HTTP and serialization overhead.

| Binary | Graph median / p95 (ms) | Public status median / p95 (ms) |
| --- | --- | --- |
| Deployed-identical Debug | 12.196 / 12.572 | 7.358 / 7.501 |
| Unchanged Release | 4.777 / 6.319 | 2.538 / 3.869 |
| Corrected Release | 4.329 / 4.590 | 2.014 / 2.079 |

All three returned byte-identical management graph (184,763 bytes), public status
(25,751 bytes), initial graph SSE and initial repository SSE. Existing enrollment tests
also exercised both receivers across graph/repository changes and reconnects. No UI redesign
or SSE cache was added. No end-to-end speedup is claimed.

Baseline Debug SHA256:
`ec611609081184cbbb431820720261a654ac43c328ddd12e7dcba105a7fe0ae9`.
Unchanged Release SHA256:
`d11ad94713df7f13f956e3f04256e0351215da8b1c787433ea21bb0c0bd3a469`.
Corrected Release SHA256:
`9f19d395ca63ea126d1f686ecc6c2716e019613a523d18d5a934f855d1034203`.
Production/test diff plus new template SHA256:
`1031ab11af0242cbd8e4befcc2ae4227445b8dc98bbdefea53e32f3bd9913df4`.
Compiler: GCC 16.2.0; installed public SNode.C package 2.1.0. All 21 linked SNode.C shared
library identities were recorded and held constant across this comparison.

## Focused checks

Build: `build/codex/speed-release`, configured with Ninja, `CMAKE_BUILD_TYPE=Release`,
and `CORE_CI_TEST_DIRECTORY=/tmp/codex/build/core-ci/speed-correction/fixtures`.
The unchanged production C++ baseline was saved before production edits. The initial nine
selected checks passed; their worker Python subprocesses used the working tree, so those
Python results are not claimed as an immutable worker baseline.

Thirteen relevant checks passed after implementation:

```sh
ctest --test-dir build/codex/speed-release --output-on-failure -j2 -R \
'^(graph-state-boundaries|persistence-graph|pipeline-chain|pipeline-large-indexes|lifecycle-sse-heartbeat|enrollment-boundaries|database-writes|database-failures|database-lost-ack|openwrt-archive-boundaries|worker-package-boundaries|worker-provenance-boundaries|worker-environment-boundaries)$'
```

After the final SQL append reduction, its seven affected database/store/pipeline/worker
checks passed again; after correcting the new renderer test's unqualified function call,
`worker-provenance-boundaries` passed on the final source. Format-check and `git diff --check`
passed. No full suite ran. The persistent tests cover hidden fields/full receipts, supplied
SDK/no archive curl, identical toolchain facts, marker order, unchanged supplied bytes,
wrong hash, symlink/directory/missing/sparse-oversized input, nested artifact download,
missing argument before identity/secret files, IPK/APK forwarding, native ignore behavior,
strict package membership, template wiring and rendered Git blob identity.

The final synthetic chain had 796 total outgoing requests: 16 token requests, 20 artifact
reads and 175 status dispatches included. The unchanged burst caused zero requests. This
fixture deliberately produces frequent progress commits. Status dispatch churn is unchanged
policy; request reduction for that derivative work is outside this correction. These counts
are not live measurements and no lower session request total is claimed.

## Accounting and remaining stages

Relative to starting HEAD, including the new template:

| Area | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| Controller C++ | 16 | 11 | +5 |
| Worker Python and shared renderer | 41 | 21 | +20 |
| Publication YAML template | 180 | 0 | +180 |
| Tests | 99 | 3 | +96 |

C++ totals 3,576 physical source/header lines, including main, within the accepted bound.
`AGENTS.md` records that bound; documentation is separate. Existing unrelated documentation
hunks and private directories were preserved.

The code and template are locally verified. Activation requires actual stage authorization:
commit trusted tooling, render aliases with its real commit, install aliases preserving old
pinned bytes, verify exact blobs/common feed exclusion, update the five deployment pins,
restart and explicitly select the new core root before selecting dependents. Source aliases
use the existing renderer default. No stored assignment is rewritten or uncertain mutation
replayed. A sandbox session must compare SDK phases, artifact transfer, feed-lock waiting,
controller commit/SSE timing and Pages visibility separately. No additional fix is assumed
necessary before those measurements.

Evidence and Claude prompt/review records are under
`/tmp/codex/build/core-ci/speed-correction/`. Synthetic MariaDB fixtures are stopped. Retain
those temporary files through owner review/live comparison, then remove them. The persistent
Release build remains available for the authorized rollout.
