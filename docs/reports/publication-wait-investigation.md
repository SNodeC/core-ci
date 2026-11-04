# Publication wait investigation and target editor reference

## Finding

The 640-second wait for demo-side OpenWrt 24.10 IPK r278 was predominantly contention
on the shared GitHub feed-job concurrency group. Another publisher, demo-side OpenWrt
25.12 APK r279, occupied that group for 605 seconds. Its artifact download/extraction
step took 563 seconds, before signing or pushing. A completed, digest-verified artifact
was eventually produced. This is an observed workflow bottleneck, not evidence of a
640-second controller event-processing delay.

The architectural invariant is that independent preparation should overlap; only work
requiring a coherent current feed head and its checked mutation needs serialization.
The current job-level boundary also serializes unrelated network transfers and setup.

Scope: read-only investigation of the completed v2.1.9 session. No code, tests, workflow,
configuration, deployment, tags, assignments or feed contents changed. No new live run.

## Source and evidence identities

- Local branch: rewrite/slim-graph-controller; HEAD 1250f43c57dee10006cffe68083dac21257b09d5.
- Deployed controller merge: 8f14486e2ac6d4f8af0cbe16442c9f949f88abf7; same source tree.
- Observed coordinator workflow revision: a0b1edd623bbd06c9046e80a8aa91f68e1a3a455.
- Canonical publication workflow blob: 83c060c9b1bf60cf707730d9160f6eba190f796f.
- Status workflow blob: 98911b7209c7fb54a2acd120eb3636dca68b92a5.
- Trusted worker checkout: 4bbf551c597c681bf26e93b4c946936f1302ef3d.
- Authoritative local publication template: deployment/source/core-ci-publish-bundle.yml.in.
- Evidence: /tmp/codex/build/core-ci/publication-wait-investigation/.

The evidence directory contains the complete 160-run coordinator listing for the session
window, job lists for 38 successful runs and three canceled status samples, exact deployed
workflow YAML, interval analysis, all publisher download durations and a sanitized download
log. No signed download URLs or secret values are retained in the sanitized log.
The earlier critical-path graph evidence remains under review-corrections/timing-v219/.

## Exact queue accounting

All times below are UTC on 2026-10-28. Job timestamps have one-second resolution.

[Blocked IPK workflow](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40782970934):
validation finished at 22:51:34; publisher started at 23:02:14 and finished at 23:03:08.
The gap is 10m40s; the publisher itself ran 54s.

| Feed-group occupant | Execution interval | Overlap with the 640s wait |
| --- | --- | ---: |
| demo-side APK r279 | 22:49:58–23:00:03 | 509s |
| demo-join DEB amd64 r280 | 23:00:06–23:00:34 | 28s |
| Status generation 3354 | 23:00:38–23:00:50 | 12s |
| demo-join Fedora r282 | 23:00:54–23:01:17 | 23s |
| demo-app IPK r273 | 23:01:22–23:02:10 | 48s |
| Gaps between these jobs and r278 | Five gaps of 3–5s | 20s |
| Total | | 640s |

The feed group was occupied for 620/640 seconds. During 472 seconds of this wait the
blocking APK job was specifically downloading/extracting its artifact. The remaining
37 seconds of overlap with that job covered other preparation and its mutation.
Three other publishers account for 99 seconds; status accounts for only 12 seconds.
The 20 seconds of handoff gaps cannot be assigned exactly to runner establishment or
GitHub scheduling from these timestamps. They are not measured controller CPU time.

Both publication and status jobs use the same repository-scoped concurrency group:
`core-ci-feed-${{ vars.FEED_REPOSITORY }}-${{ vars.FEED_BRANCH }}` with
`cancel-in-progress: false` and `queue: max`.
[GitHub documents](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency)
one active job for a group. FIFO is based on entering its concurrency wait, not workflow
dispatch or job creation; creation timestamps alone cannot establish a queue-order defect.

The configured package-publication environment has branch protection but no wait timer
or reviewer-approval rule. The observed jobs across participating source/coordinator runs
peaked at three active jobs during this wait. This is not an organization-wide inventory:
Pages and unrelated repositories were not exhaustively counted. The explicit feed-group
occupancy already explains almost all of the wait without assuming global runner exhaustion.

## Blocking APK job: what took ten minutes

[APK workflow](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40782620457/job/123148119541):

| Work | Duration |
| --- | ---: |
| Artifact download/extraction, 22:50:03–22:59:26 | 563s |
| Install signing/repository/template tools | 12s |
| Prepare signed feed update | 20s |
| Mint writer token and checked push | 2s |
| Runner setup, checkouts, cleanup and step gaps | 8s |
| Entire locked job | 605s |

Artifact 12857428234 is 285,845,912 bytes (272.6 MiB), uploaded successfully in the
validation job at 22:48:01. The original build artifact recorded in the public job inputs
was only 7,422 bytes. The validated artifact includes the retained official SDK archive;
its large size is not the size of the demo package.

The log begins the blob download at 22:50:03.764 and reports the matching SHA-256 digest
at 22:59:26.538. No warning, retry or timeout is visible between those entries. This
interval includes streaming, hashing, ZIP extraction and writing files, not a separately
measured network transfer. Dividing bytes by elapsed time yields roughly 0.485 MiB/s
end-to-end, not an independently measured network throughput.

Important limit: absent retry messages do NOT prove no retries. The pinned download
Action uses @actions/artifact 6.3.1 according to its lockfile. Its published implementation
logs retry messages at debug level, permits up to five stream attempts, and refreshes a
30-second inactivity timeout on incoming data. That is not a total-download deadline.
A slow but progressing stream can therefore run much longer. The ordinary job log does
not distinguish slow blob delivery, runner disk/extraction backpressure or debug-only
retries. Their causal shares remain unknown; no particular GitHub outage is established.
The exact downloaded library source was read without installing or executing it.

[Action source and lockfile](https://github.com/actions/download-artifact/tree/9000827ccba6bdab643e8b6fd33ac0654aef8333)
and [published dependency](https://registry.npmjs.org/@actions/artifact/-/artifact-6.3.1.tgz)
provide the implementation references. The artifact digest check succeeded; this is not
an observed digest, signing, expired-token, feed-403 or checked-push failure.

Comparable OpenWrt publisher artifact downloads in this same session took 3–8 seconds.
Other APK downloads took 4, 5 and 6 seconds. Thus nine minutes is an outlier, not the
normal cost of this archive. Official SDK revalidation in r279 began at 22:59:39.334,
metadata was verified at 22:59:39.869, archive hash at 22:59:40.093, and extraction
finished at 22:59:47.177. Those operations did not consume the preceding nine minutes.

## The 54-second IPK publisher

For r278, artifact download took 7s, tool installation 13s, signed feed preparation 23s,
and checked push 2s. The remaining 9s covers setup, checkouts, cleanup and step gaps.
Confirmation subsequently ran 23:03:11–23:03:26; controller publication verification
was recorded at 23:03:31. Confirmation and controller proof are outside the feed mutex.
The 54-second job is therefore also predominantly preparation, not 54 seconds of pushing.

The full session dispatched 140 status workflows: 18 succeeded and 122 were canceled.
Three canceled samples had no jobs. Successful status jobs occupied the feed group for
243s across the session; the 20 publishers occupied it for 1,239s. These are aggregate
job occupancy totals, not automatically additive savings on the critical dependency path.
Status churn merits separate reduction analysis, while preserving within-state progress.
It does not explain the particular 640-second delay: only 12s was status occupancy there.

## Correction direction and constraints

The controller cannot release a GitHub job-level concurrency slot while that same job
is downloading. Reordering steps within the existing job does not narrow its lock.
Increasing controller HTTP/publication limits likewise cannot make two jobs in this
one GitHub feed group execute simultaneously.

The existing SDK reuse change must be preserved: validation downloads/verifies the official
SDK once and retains it, and publication rechecks it. Simply deleting the retained archive
restores an official SDK download inside the locked publisher. It substitutes a network
source and does not remove the coupling. A shorter timeout merely converts this incident
into a failed publication; it is not a throughput fix.

Safe target: independent package validation and acquisition of immutable trusted tools
complete before entering feed-write exclusion. Inside exclusion, read the current feed
head, preserve retained bytes, add packages, regenerate affected indexes, sign/verify,
commit and perform the checked push. Public HTTPS confirmation remains outside. Reading
an old head outside the lock and force-pushing/rebasing prepared signed metadata is not
an equivalent design.

There is an important implementation constraint: GitHub-hosted jobs have separate fresh
machines. An added preparation job cannot hand a local SDK directory to the publisher
without another artifact transfer or an explicit shared storage/runner/tool-image mechanism.
The current validation job already demonstrates that limitation. Therefore 'add a prep
job and move the download there' alone is not a complete fix.

Two design alternatives deserve a bounded proposal before implementation:

1. Reduce publication to the exact pinned tools it consumes, rather than transporting and
   extracting a full compiler SDK. Establish tool provenance and runtime dependencies from
   the approved official SDK and preserve exact package/SDK compatibility checks. A pinned
   trusted publication image is one possible delivery mechanism, but adds image build,
   approval and distribution responsibilities and can itself pull slowly. No such mechanism
   is approved or implemented by this investigation.
2. Retain full SDK preparation but use a controlled runner model that permits preparation
   outside the write boundary without retransfer. This introduces runner/shared-state and
   lifecycle responsibilities, a materially larger change. It is not the default recommendation.

Recommendation: first refine the smallest version of alternative 1 against actual IPK/APK
   tool dependencies and trust requirements, favoring deletion of unnecessary SDK work.
Do not add generic caches, a controller queue, retries or a scheduler to conceal the wait.
No credible exact LOC estimate follows until that delivery/trust boundary is specified.
The immediate diagnosis is complete; the exact cause within the slow artifact stream is
not recoverable from the retained ordinary log. A future occurrence needs targeted Action
runner/step debug evidence and transfer progress to discriminate those possibilities,
without logging tokens or signed URLs. No rerun or debug activation was performed here.

## Target editor: complete field reference

The dialog edits one row in `ci/packages.json`'s `matrix`. The displayed values serialize as:

```json
{"id":"openwrt-25.12-x86_64","family":"openwrt","distribution":"openwrt",
 "suite":"25.12","arch":"x86_64","runner":"ubuntu-24.04",
 "build":{"format":"apk","target":"x86/64"}}
```

| Field | Meaning and consequence |
| --- | --- |
| Target ID | Unique name of this target row within the repository definition. Used for identification/display. It does not select a runner or SDK and need not equal another repository's target ID. Dependency matching uses full target coordinates, not ID alone. |
| Family | Selects the build method: `openwrt` uses the official OpenWrt SDK; `native` uses a distribution container image. Controls available formats and the matching family build hook. It is not a runner reservation or concurrency setting. |
| Distribution | Target distribution coordinate used for grouping and compatibility. Native publication roots use this value. OpenWrt publication currently uses the literal `openwrt/` root; this field should be `openwrt` for this target. It is not the runner OS. |
| Suite / feed series | Distribution release/series. `25.12` means the OpenWrt release series, not an exact SDK patch version. A fresh root chooses the highest stable official `25.12.N` release; dependents inherit their prerequisite SDK identity, and joins require consistent identities. It also identifies the publication directory. |
| Architecture | Package architecture expected in the packages and official SDK profile, here `x86_64`. It participates in dependency matching and publication paths. It does not select the CPU architecture of the hosted runner. |
| Package format | `ipk` or `apk` for OpenWrt; `deb` or `rpm` for Native Linux. It controls validation, signing and repository metadata. The OpenWrt worker verifies it against the selected official release rather than accepting an arbitrary suite/format pair. |
| Runner selector | `Single name` stores one string; `Label array` stores an array of required runner labels. This selects the JSON representation of the build job's GitHub `runs-on`. It is not a job count or an instruction to create one job per label. |
| Runner name or labels | With Single name, `ubuntu-24.04` selects the corresponding GitHub runner image. With Label array, enter each label on its own line, e.g. `self-hosted`, `linux`, `x64`; all labels must match one eligible runner. No blank labels. The current publication workflow separately fixes its jobs to ubuntu-24.04, so this target setting applies to the source build. |
| SDK target | Official OpenWrt board/subtarget, here `x86/64`, used in the SDK metadata/download location and checked against the SDK's reported board/subtarget. It is distinct from package architecture `x86_64`. This does not name an SDK file or an arbitrary URL. |
| Apply target | Updates only the browser's definition draft and closes the dialog. Does not commit GitHub changes, dispatch a build or modify an existing execution. The separate repository-definition save commits the draft; future selections capture immutable definition inputs. |
| Cancel | Closes this target dialog without applying its field edits to the draft. |

If Family is Native Linux, the OpenWrt SDK field is replaced by Distribution image
(e.g. a suitable distribution container reference) and Image platform (e.g. linux/amd64).
These select the build container and its platform, separately from the host runner. The
worker resolves the actual immutable image identity for execution evidence.

[GitHub runner documentation](https://docs.github.com/en/actions/how-tos/write-workflows/choose-where-workflows-run/choose-the-runner-for-a-job)
confirms that an array requires all labels on one runner; it does not fan out jobs.

The example publishes APK files under `openwrt/25.12/x86_64/`. The exact SDK patch/hash
remains execution/receipt evidence, not another directory component. Neither this dialog
nor its Runner selector configures the shared publication lock or HTTP capacity.

Source trace: ui/index.html:58–75; ui/app.js targetFamily/editTarget/applyTarget and editor
submit; src/SourceReader.cpp definition validation; src/Graph.cpp coordinates and dependency
matching; deployment/worker/core_ci/environment.py and sdk.py; sign_openwrt.py/sign_native.py;
deployment/worker/.github/workflows/core-ci-build.yml runs-on.

Separate observed UI validation discrepancies, not fixed here: the dialog permits 256-character
coordinates and runner labels, while the server permits 128; its coordinate pattern excludes
`+` although the server accepts it. Apply is therefore not proof the server will accept a
later save. The UI also does not establish that the named GitHub runner is available or
that the chosen official SDK release exists; those checks happen during workflow execution.

## Verification and limits

Read-only GitHub REST job/run metadata, job logs, exact workflow sources, local source and
published Action dependency code were inspected. No production/test lines changed, no tests
or builds were run, and no deployment/acceptance result is claimed. Reports and WORK_LOG
only were edited. Existing unrelated working-tree changes remain untouched.
