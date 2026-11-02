# Core-triggered run v2.1.17 — 2026-10-31

Result: **19 executions published, one definitely failed**. The whole graph
propagated automatically from one core tag. This was not a fully successful
end-to-end acceptance, and no failed execution was retried.

The only trigger was creation of `VOC-Experiments/demo-core` tag `v2.1.17` at
approved source commit `fc3acdb94779dafb879c183143c20e19b435e766`.
The tag POST started at 18:03:58.671 UTC. The last successful receipt observation
was recorded at 18:21:57 UTC: 17 min 58.3 s later. This includes external work;
it is not controller processing time. No source/workflow changes, restarts,
manual downstream selections, reconciliation, retries or debugger pauses occurred
during this run. Pagination was not changed.

## Execution and UI checks

- Exactly 20 new executions, five targets each for core, app, side and join.
- All core, app and side targets published. Four joins published; OpenWrt 24.10
  join failed while fetching a prerequisite package, before compilation.
- Exactly 39 distinct assigned GitHub runs: 38 successful and one failed.
  The failed build never received a publication assignment. No duplicate runs
  or replacement attempts were created.
- Each new execution retained one attempt. Exact prerequisite execution/attempt
  bindings and matching targets were checked. Published receipts matched the
  execution, attempt, source commit, target and recorded build result.
- The 90 previous executions retained their immutable source/target/prerequisite
  identities, assignments and attempt counts.
- All 78 applicable Build/Publisher Started/Finished fields matched GitHub job
  metadata and browser DOM timestamps. No missing job bindings or mismatches.
- Final authoritative HTTP graph equalled the SSE observer's reconstructed
  graph. Live Chrome showed 110 executions: 103 published, four failed and three
  historical waiting. New run: 19 published/one failed, with no active work.

## SSE and observed timings

The real Chromium observer received 270 SSE data frames: two initial snapshots
and 268 field patches. The second snapshot followed the initial selection patch
exceeding the 64 KiB budget: the connection closed and EventSource resynchronized
as designed. Subsequent patches totalled 941,759 bytes; the largest was 41,805
bytes. No repeated full-history broadcast occurred for ordinary updates.

| Observed interval | Samples | Minimum | Median | Maximum |
| --- | ---: | ---: | ---: | ---: |
| Last prerequisite receipt observation → dependent build authorization | 15 | 0 s | 1 s | 4 s |
| Build authorization → GitHub run creation | 20 | 3 s | 4 s | 4 s |
| GitHub run creation → build job start | 20 | 4 s | 5 s | 16 s |
| Build job finish → publication authorization | 19 | 7 s | 22 s | 35 s |
| Publisher job finish → receipt observation | 19 | 6 s | 10 s | 23 s |
| Complete SSE message received → DOM update | 270 | 22 ms | 28 ms | 121 ms |

Receipt observations and authorization timestamps precede acknowledged commit;
their difference includes time awaiting/processing the graph transaction. The
authorization-to-run interval includes commit completion, REST authentication/
dispatch and GitHub handling. Job-finish intervals also include notification
delivery, remote API/artifact/receipt retrieval and controller processing. They
cannot isolate exact controller CPU or local REST ingress-to-dispatch overhead.
GitHub queue/setup/build/publication costs are represented separately in the raw
run/job data and per-execution timing CSV. No new production instrumentation was
added. No overall speedup or elimination of writer/Pages waits is inferred.

## Failure and publication boundary

[OpenWrt 24.10 join run](https://github.com/VOC-Experiments/demo-join/actions/runs/41228526086)
failed at the `Build selected packages in an isolated container` step. Its log
records `HTTP Error 404: Not Found` at 18:16:42.343 UTC inside
`prepare_dependencies → download_feed`, fetching a prerequisite package.
The log does not identify which exact requested package URL returned 404.

The app prerequisite's publisher finished at 18:15:30 UTC; its receipt was
observed at 18:15:37. The join was released at 18:15:37 and its job started at
18:15:44. That app receipt binds feed commit
`5fe31caf03a954df91292bab182bb0958a5902c4`.
[The Pages deployment of that commit](https://github.com/VOC-Experiments/packages/actions/runs/41228509611)
was still running when the download failed: its deploy-pages step ended at
18:16:45, the job at 18:16:46 and the workflow at 18:16:47 UTC.

Subsequent read-only HEAD requests to all three selected core/app/side package
URLs returned HTTP 200. These facts support a public-feed availability race;
they do not retrospectively identify the specific failed URL or exact instant
each public edge became ready. The current publication boundary trusts the
assigned publisher job and committed receipt before Pages availability. That
gap remains unresolved. The independent OpenWrt 25.12 chain continued and
published successfully. No proof wait or workflow change was added during the run.

## Exact failed-download investigation

The owner's follow-up requested identification of the actual failed package.
The complete GitHub workflow log archive (including system and individual step
logs), the job log, the artifact list and pinned worker download contract were
inspected. The actual assignment printed by the runner exactly equals the
controller's frozen assignment. Its sequential request order is:

1. `demo-core_2.1.17-r1_x86_64.ipk`.
2. Core's execution-specific `development.tar.gz`.
3. `demo-app_2.1.0-r6_x86_64.ipk`.
4. `demo-side_2.1.0-r5_x86_64.ipk`.
5. Side's execution-specific `development.tar.gz`.

The traceback points to the package request, not the export request. Therefore
core is proven to have been requested first, and the failed package is one of
core, app or side. No individual request/success is logged. Python's exception
contains `req.full_url` at runtime, but the uncaught traceback prints only
`HTTP Error 404: Not Found`; it does not serialize that value or loop variables.
The only saved run artifact is the environment record uploaded before the
downloads. The package artifact upload was skipped after failure. There is no
retained prerequisite directory or request trace in the available evidence.

Consequently the exact package that returned 404 cannot be recovered from these
records. First-in-order does not prove first-request failure, and elapsed time
does not establish how many requests succeeded. App remains an inference, not
an identified failed URL. Identifying a subsequent failing request requires
recording its URL at the download/error boundary; this investigation changed no
worker or workflow and triggered no new execution. The original exception and
its locals cannot be recovered by reproducing the request later.

The full log archive, archive listing and exact ordered URLs are retained under
`/tmp/codex/build/core-ci/prerequisite-404-20261031`.

## Which prerequisite was absent from the last deployed revision?

A further read-only check inspected the GitHub Pages deployment status history
and the exact feed Git trees. At the download failure, 18:16:42 UTC:

| Package | Pages deployment reported successful (UTC) |
| --- | --- |
| core `2.1.17-r1` | 18:11:17 |
| side `2.1.0-r5` | 18:15:45 |
| app `2.1.0-r6` | 18:16:47 (after failure) |

The last deployment reported successful before failure targeted
`57060f0184273988676864047605758832c5d1ee` (side's feed revision). Its complete
Git tree contains the exact core and side IPK paths and does not contain
`openwrt/24.10/x86_64/demo-app_2.1.0-r6_x86_64.ipk`.
The next deployment targeted app's revision
`5fe31caf03a954df91292bab182bb0958a5902c4`, whose tree contains all three exact
package files. Its deployment was `in_progress` from 18:15:51 and was reported
`success` at 18:16:47.

Thus app is identified as the package absent from the last successfully deployed
revision, not merely guessed from publish-job timing. This still does not
recover the failed HTTP request URL or establish the precise instant every
public CDN edge switched revisions. Deployment status success is distinct from
the deploy-pages step completion (18:16:45).

Read-only deployment IDs: core 7462411163, side 7462460943, app 7462467606.
Saved deployment lists/statuses and complete, non-truncated feed trees are in
`/tmp/codex/build/core-ci/prerequisite-404-20261031`.

## Identity and evidence

Controller PID 346835; binary SHA-256
`ece3700fc29ef2c04741a8c26a992edc65f4d82d71340ba31d53e72e34a04c10`.
GCC 16.2.0 Debug, installed SNode.C; source manifest SHA-256
`87cf0bf9dea2285f0239e7077d99d747039c04360921d7398f2a9c3ac510d5d7`.
Checkout `/home/voc/projects/snodec/core-ci-new`, branch `feat/graph-controller`,
no commits/remote/PR. Production C++ remains 2,328 physical lines; per-file counts
are in `sse-delivery-check.md`. Production/test source changes for this run: zero.
Documentation changes: this report and WORK_LOG.md. No production SNodeC resource
was touched and no commit, push, installation or production deployment occurred.

Read-only observers stopped after final verification; controller remains live.
Raw evidence, baseline/final graphs, logs, screenshot and `timings.csv` are under
`/tmp/codex/build/core-ci/sse-chain-20261031`. These are retained for review; no
additional checkout exists. The browser-close record is the observer's deliberate
shutdown, not an SSE failure. No browser script or GitHub observer errors occurred
during capture. Publication proof here follows the owner's revised publisher/
receipt rule; independent complete public HTTPS proof was not implemented or
claimed by this run.
