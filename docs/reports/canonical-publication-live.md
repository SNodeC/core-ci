# Canonical publication live acceptance

The VOC-Experiments sandbox runs main `4f8370b97e1423d40bebc771017ea349f646b78e`
on `core.vchrist.at`, using the validated Release controller. The fresh `demo-core v2.1.7`
chain completed all 20 publications automatically. Every original build/publication workflow
and all five real client-acceptance workflows succeeded on attempt one.

## Deployment and scope

- Controller PID `226972`, zero restarts at final check; binary SHA256
  `9f19d395ca63ea126d1f686ecc6c2716e019613a523d18d5a934f855d1034203`.
- All 60 previous execution records/assignments restored exactly before triggering work.
  A protected consistent MariaDB/service/binary backup is retained at
  `/var/backups/core-ci/canonical-20261028T191742Z`.
- Management stays on wildcard 8080 and signed webhook/public status on wildcard TLS 8443.
  Ports 80/443, existing keys, private configuration and signing identities are unchanged.
- Existing 20 framework runtime libraries match the validated build byte-for-byte. One
  missing matching `libsnodec-http-server-express-legacy-in.so.2` was added to the private
  application runtime; no existing framework library or SNode.C source was changed.
- Trusted worker tooling: `4bbf551c597c681bf26e93b4c946936f1302ef3d`. New build/publication
  aliases retain the old execution wrappers. The status wrapper was repinned to the corrected
  renderer so later refreshes retain the fixed browsing links. Client scripts were deployed
  as `a0b1edd623bbd06c9046e80a8aa91f68e1a3a455`.
- Owner explicitly approved removal of exactly 41 incompatible top-level sandbox APKs and
  `openwrt/25.12/x86_64/packages.adb`. Normal non-force feed commit
  `de0ff952361ec2e42c345dc251128f692b437a2f` removed exactly those 42 blobs; all 1,089 other blobs matched.
  Git history was retained. No broader cleanup, migration, database reset or key change occurred.

## Live proof

- All 20 targets published; all 40 assigned GitHub build/publication runs are successful
  original attempts. No manual retry, reconciliation or advancement was used.
- Every new package path matches the canonical distribution layout. All 20 downloaded package
  hashes match their receipts. DEB uses `debian/pool/trixie`, RPM uses
  `fedora/43/x86_64/Packages`, and OpenWrt uses `openwrt/SUITE/x86_64`.
- All 40 Packages anchors across four Markdown and four HTML pages were checked: the eight
  distinct resolved URLs returned HTTP 200. Direct download URLs and GitHub browsing URLs
  remain separate.
- All 60 previous execution assignments/outcomes/evidence and 772 retained immutable
  package/key/receipt/export blobs match their pre-session identities, excluding the approved
  removals. The retired uncertain r169 attempt was not retried or otherwise changed.
- GitHub Pages reached generation 2774, matching the controller. UI graph,
  SSE and public JSON were observed throughout. A browser connector was unavailable, so this
  session makes no visual browser-rendering claim.

### Real package clients

Each job rejected unrelated keys, refreshed the signed repository, installed the exact new
core package and checked its runtime output. These are live GitHub client checks, separate
from the 40 graph-owned build/publication workflows.

| Target | Result |
| --- | --- |
| debian-trixie-amd64 | [PASS](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40755566459) |
| debian-trixie-arm64 | [PASS](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40755570339) |
| fedora-43-x86_64 | [PASS](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40755573814) |
| openwrt-24.10-x86_64 | [PASS](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40755577884) |
| openwrt-25.12-x86_64 | [PASS](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40755581919) |

## Timings and limits

Tag trigger: `2026-10-28T19:18:24.519544Z`. Last publication first observed by SSE:
`2026-10-28T19:40:07.226737+00:00`. Elapsed: **1302.707 seconds
(21 min 42.7 s)**. Pages was independently observed complete at
`2026-10-28T19:40:46.630167+00:00`, 39.403 seconds later.
The earlier progress update's approximately 21 min 47 s used the slower periodic graph sample.

All 15 dependent assignments appeared in the same graph generation/SSE frame as their final
prerequisite's publication. This shows no intervening observed graph state; it does **not**
mean zero CPU time. No function-level tracing was performed in this session, and exact internal
per-function controller processing time is not established. Root selection predates observer
startup, so its exact delivery-to-commit latency is not claimed.

Final idle loopback measurements (10 measured requests after one warmup per endpoint):
management graph median 43.319 ms / maximum 50.546 ms
for 852,433 bytes; public projection median
15.244 ms / maximum 16.695 ms
for 117,459 bytes. These include event-loop/HTTP/serialization time,
not isolated CPU cost or the graph traversal critical path.

GitHub jobs, including runner setup: summed build duration 1445 s;
serialized privileged publisher duration 766 s;
summed secret-free confirmation duration 442 s. Validation-complete to
publisher-start wait has median 106.5 s and maximum
260 s; this remote interval includes feed-lock/runner queueing.
All 20 privileged publisher jobs were mutually exclusive. These sums are not additive elapsed
wall time because independent builds, validation and confirmation overlap.

SDK phase markers from all eight OpenWrt publishers confirm separate metadata, archive
verification and extraction phases. They measure these logged boundaries only; standard tool
installation, configuration and signing remain separate work inside publisher jobs. No 10–20x
speedup or controlled cross-session speed factor is claimed.

### Per-target GitHub durations

Times below are seconds. "Wait" is validation completion to privileged publisher start;
"Publish" includes the privileged job's runner-side setup. It is not controller processing time.

| Revision | Project | Target | Build | Validate | Wait | Publish | Confirm | Evidence |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| 225 | demo-core | debian-trixie-amd64 | 35 | 20 | 81 | 30 | 25 | [Build](https://github.com/VOC-Experiments/demo-core/actions/runs/40753126110) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753244520) |
| 226 | demo-core | debian-trixie-arm64 | 32 | 18 | 19 | 30 | 24 | [Build](https://github.com/VOC-Experiments/demo-core/actions/runs/40753125917) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753243759) |
| 227 | demo-core | fedora-43-x86_64 | 31 | 22 | 53 | 25 | 14 | [Build](https://github.com/VOC-Experiments/demo-core/actions/runs/40753125912) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753236946) |
| 228 | demo-core | openwrt-24.10-x86_64 | 119 | 112 | 5 | 43 | 16 | [Build](https://github.com/VOC-Experiments/demo-core/actions/runs/40753125618) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753456851) |
| 229 | demo-core | openwrt-25.12-x86_64 | 158 | 108 | 148 | 64 | 5 | [Build](https://github.com/VOC-Experiments/demo-core/actions/runs/40753127033) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753588441) |
| 230 | demo-app | debian-trixie-amd64 | 31 | 17 | 207 | 28 | 44 | [Build](https://github.com/VOC-Experiments/demo-app/actions/runs/40753730687) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753861065) |
| 231 | demo-app | debian-trixie-arm64 | 37 | 22 | 82 | 26 | 25 | [Build](https://github.com/VOC-Experiments/demo-app/actions/runs/40753531982) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753734306) |
| 232 | demo-app | fedora-43-x86_64 | 38 | 16 | 99 | 26 | 26 | [Build](https://github.com/VOC-Experiments/demo-app/actions/runs/40753589186) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753783122) |
| 233 | demo-app | openwrt-24.10-x86_64 | 146 | 67 | 135 | 56 | 25 | [Build](https://github.com/VOC-Experiments/demo-app/actions/runs/40753941985) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40754339817) |
| 234 | demo-app | openwrt-25.12-x86_64 | 93 | 79 | 81 | 59 | 16 | [Build](https://github.com/VOC-Experiments/demo-app/actions/runs/40754470802) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40754725684) |
| 235 | demo-side | debian-trixie-amd64 | 34 | 17 | 260 | 30 | 55 | [Build](https://github.com/VOC-Experiments/demo-side/actions/runs/40753729705) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753862075) |
| 236 | demo-side | debian-trixie-arm64 | 33 | 14 | 67 | 29 | 15 | [Build](https://github.com/VOC-Experiments/demo-side/actions/runs/40753531123) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753711925) |
| 237 | demo-side | fedora-43-x86_64 | 45 | 23 | 114 | 30 | 24 | [Build](https://github.com/VOC-Experiments/demo-side/actions/runs/40753590665) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40753801150) |
| 238 | demo-side | openwrt-24.10-x86_64 | 124 | 35 | 137 | 49 | 15 | [Build](https://github.com/VOC-Experiments/demo-side/actions/runs/40753941995) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40754278591) |
| 239 | demo-side | openwrt-25.12-x86_64 | 166 | 40 | 145 | 46 | 15 | [Build](https://github.com/VOC-Experiments/demo-side/actions/runs/40754471287) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40754905588) |
| 240 | demo-join | debian-trixie-amd64 | 30 | 19 | 132 | 29 | 17 | [Build](https://github.com/VOC-Experiments/demo-join/actions/runs/40754799980) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40754905973) |
| 241 | demo-join | debian-trixie-arm64 | 29 | 17 | 134 | 24 | 25 | [Build](https://github.com/VOC-Experiments/demo-join/actions/runs/40754172868) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40754270952) |
| 242 | demo-join | fedora-43-x86_64 | 68 | 19 | 194 | 29 | 25 | [Build](https://github.com/VOC-Experiments/demo-join/actions/runs/40754320669) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40754512608) |
| 243 | demo-join | openwrt-24.10-x86_64 | 119 | 106 | 5 | 49 | 16 | [Build](https://github.com/VOC-Experiments/demo-join/actions/runs/40755080871) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40755406509) |
| 244 | demo-join | openwrt-25.12-x86_64 | 77 | 133 | 5 | 64 | 15 | [Build](https://github.com/VOC-Experiments/demo-join/actions/runs/40755560390) · [Publish](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40755780020) |

### OpenWrt publisher SDK phases

| Revision | Metadata | Archive verification | Extraction |
| --- | ---: | ---: | ---: |
| 228 | 0.485 s | 0.201 s | 7.141 s |
| 229 | 0.186 s | 0.249 s | 8.746 s |
| 233 | 0.787 s | 0.260 s | 10.602 s |
| 234 | 0.385 s | 0.246 s | 8.560 s |
| 238 | 0.187 s | 0.264 s | 10.873 s |
| 239 | 0.235 s | 0.249 s | 8.758 s |
| 243 | 0.291 s | 0.264 s | 11.032 s |
| 244 | 0.189 s | 0.291 s | 11.526 s |

## Evidence and workspace

Evidence, public workflow pins, cleanup proof, activation/restoration proof, run/job results,
SSE/progress/Pages observations, SDK markers and acceptance checks are retained under
`/tmp/codex/build/core-ci/canonical-live/`. Diagnostic observer HTTP/API calls are separate
from controller traffic; no exact controller request total is claimed here. The observer stopped
after Pages caught up. Official SDK artifacts can be removed after this acceptance record is reviewed.

Canonical checkout remains `/home/voc/projects/snodec/core-ci`, branch
`rewrite/slim-graph-controller`, source commit `8cdbf32` (same tree as merged main `4f8370b`).
No application production/test changes were made during deployment. This report and the
appended work log are documentation only and remain local with unrelated documentation
hunks preserved. No additional source checkout was created and no private directory was inspected.
