# Live VOC-Experiments acceptance — 2026-10-31

The full authorized sandbox chain is proved across all five targets:
**demo-core → demo-app and demo-side → demo-join**. All 20 executions in the
renewed selection are verified published. The independent audit found 40 unique
successful GitHub runs and exact matching committed/public HTTPS receipt bytes
for all 20. This is sandbox acceptance, not a production deployment claim.

## Current runtime and ownership

- Checkout `/home/voc/projects/snodec/core-ci-new`, branch `feat/graph-controller`; no commit, remote or PR.
- Running binary `/tmp/codex/build/core-ci/controller/core-ci`, SHA-256 `37484576f4c06ffa5f0212e137b829e18413eec00f58a5e16b4eab1a4905707c`; PID `107024`.
- Management [http://localhost:8080/](http://localhost:8080/) on Jupiter; native TLS webhook on `0.0.0.0:8443`.
- Existing forwarding remains WAN IPv4 TCP 8443 → `192.168.1.239:8443`; router configuration was not changed.
- Sandbox App `5467318`, installation `175134675`, bot `347832523`.
- App webhook destination is `https://gw-home.vchrist.at:8443/github/webhook`, with certificate verification enabled.
- A real authenticated GitHub redelivery returned 202; an unsigned request returned 401.
- The owner authorized live sandbox runs and reuse of existing credential files. No secret/key bytes were printed or committed, and no key was rotated.
- Managed isolated MariaDB authority remains under `/tmp/codex/build/core-ci/acceptance/`; no existing database was reset or migrated.
- No production SNodeC resource, workflow source, tag, signing trust, installed service, commit, push or merge was changed.

## Exact selected inputs

One explicit request selected `demo-join`, recursively resolving each repository's
highest SemVer once and reusing each exact node within the diamond. Definitions
and targets came from committed GitHub data. No source tag was created.

| Repository | Selected tag | Source commit | Definition Git blob |
| --- | --- | --- | --- |
| VOC-Experiments/demo-app | v2.1.0 | `e4069844a0b4235451a31e97b92818e991c6b4a6` | `4d7744361faa1724e08c5a218d3baab822d92e5e` |
| VOC-Experiments/demo-core | v2.1.13 | `fc3acdb94779dafb879c183143c20e19b435e766` | `faaf4c6cd0d3a424c4301ab9124897bcfeed7393` |
| VOC-Experiments/demo-join | v2.1.4 | `96db7574f3289f44e2e575bbf9e36f84922566d2` | `d852b8ce5ea39502611640281dba07d740e8cf7f` |
| VOC-Experiments/demo-side | v2.1.0 | `6b8f46a8311c0c968dc8fcbccf33f199b6e3dff9` | `31250e8c3140b3465b46eb2a3afcbdde6fb37e4f` |

Existing renewed workflows supply tooling pin
`230e594f78ccf51108cba5db910ea41f87f59876`, source-workflow blob
`d413f2396dd9c05bc4890f4e59c556dacf3b9570`, and publication-workflow blob
`e4ca49f44c893aa9ce64e4a4b5475a1390f319cc`. Every execution additionally freezes
its exact workflow commit. The public certificate at that tooling pin exactly
matches the deployed feed certificate. Its fingerprint is unchanged from the
initial trial; the old pinned certificate had expired. No workflow was edited.

## Completed runs

Each row below is verified published by independent exact HTTPS receipt proof.

| Package | Target | Attempt revision | Build run | Publication run |
| --- | --- | ---: | --- | --- |
| demo-core | debian-trixie-amd64 | 2 | [41201508816](https://github.com/VOC-Experiments/demo-core/actions/runs/41201508816) | [41201558963](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201558963) |
| demo-app | debian-trixie-amd64 | 2 | [41201726454](https://github.com/VOC-Experiments/demo-app/actions/runs/41201726454) | [41201779323](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201779323) |
| demo-side | debian-trixie-amd64 | 1 | [41201725912](https://github.com/VOC-Experiments/demo-side/actions/runs/41201725912) | [41201786339](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201786339) |
| demo-join | debian-trixie-amd64 | 1 | [41201999892](https://github.com/VOC-Experiments/demo-join/actions/runs/41201999892) | [41202050330](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41202050330) |
| demo-core | debian-trixie-arm64 | 2 | [41201508976](https://github.com/VOC-Experiments/demo-core/actions/runs/41201508976) | [41201564594](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201564594) |
| demo-app | debian-trixie-arm64 | 2 | [41201730946](https://github.com/VOC-Experiments/demo-app/actions/runs/41201730946) | [41201791475](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201791475) |
| demo-side | debian-trixie-arm64 | 1 | [41201730634](https://github.com/VOC-Experiments/demo-side/actions/runs/41201730634) | [41201793703](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201793703) |
| demo-join | debian-trixie-arm64 | 1 | [41202009911](https://github.com/VOC-Experiments/demo-join/actions/runs/41202009911) | [41202061237](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41202061237) |
| demo-core | fedora-43-x86_64 | 2 | [41201509128](https://github.com/VOC-Experiments/demo-core/actions/runs/41201509128) | [41201558720](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201558720) |
| demo-app | fedora-43-x86_64 | 2 | [41201736823](https://github.com/VOC-Experiments/demo-app/actions/runs/41201736823) | [41201820255](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201820255) |
| demo-side | fedora-43-x86_64 | 1 | [41201736654](https://github.com/VOC-Experiments/demo-side/actions/runs/41201736654) | [41201801318](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201801318) |
| demo-join | fedora-43-x86_64 | 2 | [41202744187](https://github.com/VOC-Experiments/demo-join/actions/runs/41202744187) | [41202848166](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41202848166) |
| demo-core | openwrt-24.10-x86_64 | 2 | [41201509085](https://github.com/VOC-Experiments/demo-core/actions/runs/41201509085) | [41201605178](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201605178) |
| demo-app | openwrt-24.10-x86_64 | 2 | [41201816149](https://github.com/VOC-Experiments/demo-app/actions/runs/41201816149) | [41201947354](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201947354) |
| demo-side | openwrt-24.10-x86_64 | 1 | [41201816182](https://github.com/VOC-Experiments/demo-side/actions/runs/41201816182) | [41201971407](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201971407) |
| demo-join | openwrt-24.10-x86_64 | 1 | [41202212287](https://github.com/VOC-Experiments/demo-join/actions/runs/41202212287) | [41202303293](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41202303293) |
| demo-core | openwrt-25.12-x86_64 | 2 | [41201509019](https://github.com/VOC-Experiments/demo-core/actions/runs/41201509019) | [41201649937](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201649937) |
| demo-app | openwrt-25.12-x86_64 | 2 | [41201878662](https://github.com/VOC-Experiments/demo-app/actions/runs/41201878662) | [41201965556](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201965556) |
| demo-side | openwrt-25.12-x86_64 | 1 | [41201878759](https://github.com/VOC-Experiments/demo-side/actions/runs/41201878759) | [41201986142](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41201986142) |
| demo-join | openwrt-25.12-x86_64 | 1 | [41202280718](https://github.com/VOC-Experiments/demo-join/actions/runs/41202280718) | [41202387753](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/41202387753) |

The RPM join's original dispatch was definitely rejected with HTTP 422. An
explicit retry created a new attempt, preserving its source, target, workflow
pins and exact prerequisite attempts. Its assigned input size fell from 62,053
to 16,582 characters by omitting only unused shared-index inventories. Complete
original receipts and the rejected assignment remain immutable in MariaDB.
The retry was accepted, built, published and independently proven. The original
GitHub 422 response body was not persisted, so its exact server-side validation
message is unavailable; size is the supported inference from the large rejected
payload and accepted smaller retry, not a recovered error message. No unknown
publication was replayed or replaced.

## Authority, proof and history

- All five roots dispatched without a prerequisite barrier; app and side proceeded independently for each compatible target.
- All five joins waited for their three exact prerequisite executions. Publication proof and newly ready work were derived in the same graph transaction; dispatch followed commit.
- The observer recorded all 15 initial downstream releases. Proof-verification and authority timestamps can differ by up to two seconds because verification precedes the authoritative update; these are not isolated processing timings.
- The real-MariaDB boundary test independently reads committed proof and dispatch authority before an effect is sent, including large receipts and duplicate/unknown-commit cases.
- Native restarts preserved complete graph snapshots byte-for-byte. Restart did not redispatch assigned work. The independent run audit found one matching run per accepted assignment and no duplicate effects.
- Every one of the 20 receipts was re-read at its originally proven Git commit and over public HTTPS after all publications. Git blob identity, exact bytes and recorded SHA-256 matched.
- For every join, its three prerequisite receipts record changed hashes for overlapping index paths: 15 comparisons. Earlier receipt proof still passed after those later changes. No current index was fetched or inspected as publication proof.

The initial standard-workflow trial remains intact: four verified OpenWrt outcomes,
three native definite failures before feed mutation, and their three waiting app
selections. Those native publications used an expired public certificate and
stopped at the existing byte-equality key guard. The original selected identities
were not rebound to renewed tooling. In total the durable graph has 30 executions,
31 attempts, 24 published current outcomes, three current failures and three
waiting historical selections. The final independent audit checked all 55 committed commands: 54 unique exact
App runs and zero matching runs for the definite HTTP 422 rejection. Twenty-four
publications are verified. No duplicate effect was found.

## Timing boundaries

No clean baseline or isolated controller CPU/database benchmark was recorded.
This acceptance includes diagnosis, code changes, restarts and explicit operator
reconciliation; its elapsed duration cannot establish an overall speedup.

| Segment | Recorded evidence and interpretation |
| --- | --- |
| Controller selection/transition | Join selection response 7.857 s including GitHub discovery. Readiness and commit-before-dispatch are verified; processing time is not isolated. Typical recorded authority-to-send acknowledgement is about 2–3 s including App authentication and network/API time. |
| GitHub queue/setup | RPM core build run creation → assigned job start 5 s; final RPM join 4 s. GitHub Set up job step 1 s / 0 s. These are separate from environment/package tool setup. |
| GitHub build | RPM core environment resolution 3 s and package-container step 17 s; final RPM join 3 s and 65 s. Complete build jobs 27 s / 75 s. |
| GitHub publication | RPM core validation job 19 s, publisher job 29 s, prepare 3 s and push 2 s. Final RPM join: 17 s, 21 s, 2 s and 2 s respectively. Existing GitHub feed-boundary serialization remains. |
| HTTPS/Pages proof | Final RPM feed deployment run [41202905831](https://github.com/VOC-Experiments/packages/actions/runs/41202905831) ran 12:50:50–12:51:10 UTC. Controller recorded proof at 12:53:03 UTC after explicit reconciliation. That includes operator delay and is not an exact public-availability or Pages-propagation measurement. |
| UI delivery | Real Chromium checked all 30 executions and the copied layout, plus changing within-state SSE fixtures. No browser-delivery latency benchmark was made. |

## Port 8080 interface

The owner explicitly requested the prior UI layout. Its Work/Repositories tabs,
compact table, grouped build/publisher time columns, repository list and read-only
definition view were copied as presentation and adapted to the fresh graph/API.
The graph and readiness authority remain entirely in C++/MariaDB. UI search,
filtering, display ordering and expanded rows are presentation only.

Real Chromium passed: 30 rows, 24 published outcomes, five published joins, four
dynamic repositories, filters/tabs, read-only definitions, simultaneous expansion,
prerequisite expansion/scroll and zero page errors. The isolated real-MariaDB
browser fixture additionally passed within-state progress and real acknowledged
retirement. Dates/times are browser-local with locale-appropriate hour format;
compact displayed times omit seconds, retained in tooltips. Headings and date/time
alignment match the copied layout; State width stays 128 px and chevrons are not underlined.

The UI receives GitHub job events, not continuous individual-step telemetry.
Step text is marked last reported. A completed publisher awaiting proof is shown
as Awaiting public receipt proof. No polling timer, invented progress or new
worker callback protocol was introduced.

## Limits and handover

Native jobs in the existing renewed workflow skip their confirmation job. This
acceptance used explicit reconciliation after HTTPS availability, including one
completion notification missed during a diagnostic restart. It proves the entire
sandbox chain with the supported reconciliation operation; it does not prove
unattended native confirmation. Actual tag creation/push, arbitrary repositories
and production deployment remain untested. External writer/Pages waits remain.

Final production C++ is 2,377 physical lines; per-file counts and source/build
identity are in [validation.md](validation.md). This continuation reduces C++ by
nine lines; test C++ grows 28, test drivers 16, and the explicitly requested UI
copy adds 231 presentation lines. No controller logic moved into UI or workflows.
The final relevant graph/persistence and native/browser checks passed. A temporary
test-driver SyntaxError during isolated-port adaptation was corrected before the
passing runs. The final native/browser check took 4.05 s; that is not a pipeline
performance result.

The foreground controller, webhook and MariaDB remain active on Jupiter. Preserve
its database/history until the instance is explicitly retired. Disposable isolated
test fixtures were removed; the completed SSE observer and automated browsers
were stopped. No additional checkout exists.

Evidence is under `/tmp/codex/build/core-ci/acceptance/`: final graph, renewed
selection, original/retry assignments, before/after restart snapshots, independent
run/receipt and all-command audits, release observations, Pages/run/job timings and copied-layout
screenshots. SSE observations have reconnect/idle gaps and are not a complete
performance trace or another graph authority. The runtime directory contains
credential paths, never credential contents.
# Later monitored rerun

This report describes the earlier sandbox acceptance. The separately authorized
v2.1.14 run, its recovered completion, timing defects and fixes are recorded in
[monitored-rerun.md](monitored-rerun.md).
