# Slim controller sandbox acceptance

Verified on 2026-10-28 in VOC-Experiments. Controller source is
`eab0b087135e8275752e48ba2fd8772c55e82e92` on `rewrite/slim-graph-controller`.
The installed binary SHA-256 is
`ee4cfa91431876f967a3fb423df884b2e104e340af9d3784cfbba779bdc60f51`.
The service on core.vchrist.at is active, PID 221515, with no automatic restarts.
The controller contains exactly 3,510 physical C++ source/header lines.

A tag at demo-core v2.1.5 selected 20 executions. All 20 are verified published;
all five join package-client acceptance runs succeeded. Dependencies were satisfied
by verified publications. The Fedora join required the approved payload correction
and controller restart before its existing waiting revision could proceed. This is
real integrated acceptance, but not an uninterrupted performance benchmark.

## Build and publication evidence

| Project | Target | Revision | Build | Publication |
| --- | --- | ---: | --- | --- |
| demo-core | debian-trixie-amd64 | 185 | [Run](https://github.com/VOC-Experiments/demo-core/actions/runs/40663397151) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40663488469) |
| demo-core | debian-trixie-arm64 | 186 | [Run](https://github.com/VOC-Experiments/demo-core/actions/runs/40663396866) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40663491642) |
| demo-core | fedora-43-x86_64 | 187 | [Run](https://github.com/VOC-Experiments/demo-core/actions/runs/40663396976) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40663494253) |
| demo-core | openwrt-24.10-x86_64 | 188 | [Run](https://github.com/VOC-Experiments/demo-core/actions/runs/40663396485) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40663901873) |
| demo-core | openwrt-25.12-x86_64 | 189 | [Run](https://github.com/VOC-Experiments/demo-core/actions/runs/40663396372) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40663767448) |
| demo-app | debian-trixie-amd64 | 190 | [Run](https://github.com/VOC-Experiments/demo-app/actions/runs/40664009411) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40664106492) |
| demo-app | debian-trixie-arm64 | 191 | [Run](https://github.com/VOC-Experiments/demo-app/actions/runs/40664110665) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40664226605) |
| demo-app | fedora-43-x86_64 | 192 | [Run](https://github.com/VOC-Experiments/demo-app/actions/runs/40664185243) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40664332931) |
| demo-app | openwrt-24.10-x86_64 | 193 | [Run](https://github.com/VOC-Experiments/demo-app/actions/runs/40665115058) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40665347273) |
| demo-app | openwrt-25.12-x86_64 | 194 | [Run](https://github.com/VOC-Experiments/demo-app/actions/runs/40664463137) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40664797992) |
| demo-side | debian-trixie-amd64 | 195 | [Run](https://github.com/VOC-Experiments/demo-side/actions/runs/40664008948) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40664111953) |
| demo-side | debian-trixie-arm64 | 196 | [Run](https://github.com/VOC-Experiments/demo-side/actions/runs/40664110008) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40664206556) |
| demo-side | fedora-43-x86_64 | 197 | [Run](https://github.com/VOC-Experiments/demo-side/actions/runs/40664184903) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40664313605) |
| demo-side | openwrt-24.10-x86_64 | 198 | [Run](https://github.com/VOC-Experiments/demo-side/actions/runs/40665113923) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40665392148) |
| demo-side | openwrt-25.12-x86_64 | 199 | [Run](https://github.com/VOC-Experiments/demo-side/actions/runs/40664463156) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40664749669) |
| demo-join | debian-trixie-amd64 | 200 | [Run](https://github.com/VOC-Experiments/demo-join/actions/runs/40665153859) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40665264556) |
| demo-join | debian-trixie-arm64 | 201 | [Run](https://github.com/VOC-Experiments/demo-join/actions/runs/40665213877) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40665311548) |
| demo-join | fedora-43-x86_64 | 202 | [Run](https://github.com/VOC-Experiments/demo-join/actions/runs/40667241335) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40667357812) |
| demo-join | openwrt-24.10-x86_64 | 203 | [Run](https://github.com/VOC-Experiments/demo-join/actions/runs/40666173688) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40666513359) |
| demo-join | openwrt-25.12-x86_64 | 204 | [Run](https://github.com/VOC-Experiments/demo-join/actions/runs/40665689393) | [Run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40666046953) |

## Client installation checks

The existing trusted client workflows checked signed feed installation, requested
top-level package version, wrong-key rejection and `demo-join core 60, side 62`.
The output proves consumption of build-time exports; it does not independently prove
exact runtime dependency selection or all live modified-package/index rejection cases.

- [Client run 40668066980](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40668066980) — success.
- [Client run 40666026402](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40666026402) — success.
- [Client run 40666021823](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40666021823) — success.
- [Client run 40667246183](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40667246183) — success.
- [Client run 40666809235](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40666809235) — success.

## Status and preservation

The local graph, public projection and Pages projection reached generation 2100.
Public and Pages JSON are equal. The 20 new receipt files, four project HTML pages
and controller JSON served over public HTTPS match their committed Git blob hashes.
[Status refresh run](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40667564269)
succeeded. The HTTP UI assets match source; SSE updates were observed. Graphical
browser inspection was unavailable in this environment.

Compared with the pre-test feed tree, no files were removed and all 535 retained
package/receipt/export/key blobs are unchanged. Changes to existing paths are confined
to affected indexes and deterministic status projections. Existing tags were preserved;
new test tags were added. Production SNodeC workflows, feeds, keys and credentials
were not changed. Private directories and unrelated local edits were preserved.

## Remaining limits

One earlier v2.1.4 APK publication retains an uncertain assignment without a bound
run. It was neither replayed nor replaced. Fifteen superseded waiting executions
remain as history. They are separate from the 20 successful fresh executions.
The original database remains preserved; this deployment uses the fresh core_ci_slim
database. The installed framework's independently reproduced database-reconnection
callback defect remains unresolved; no SNode.C code changed.

No new 403 was observed in this verification. Its previous intermittent cause remains
unproven. Token reuse has a 300-second expiry margin, without mutation retries.
No 10–20x speedup or subsecond end-to-end latency is claimed. Endpoint medians measured
under load (graph 19.664 ms, health 1.748 ms, public status 18.078 ms) are not webhook-to-
dispatch timings. Detailed local evidence is retained under
`/tmp/codex/build/core-ci/slim-deployment/`. No merge to main was performed.
