# Publication retention: local validation

## Source and authority

Canonical checkout: `/home/voc/projects/snodec/core-ci`. Branch: `feat/publication-retention`.
HEAD/base: `3cc14fd9dfaba872dacb82e57d80f70c628c50fc`; main and remote HEAD still resolve to this commit.
The earlier `8461764ff2131ef13e44ce187d1c84b1550f5d27` is its merged ancestor.
Production/test diff SHA256 (tracked diff plus explicit new retention.py path/bytes):
`7645ab2de7dec0c625c3567ed51e571484ac57bdebd383376abd748ecc089c63`.
No commit, push, PR, merge, installation, restart, deployment, workflow/tag/feed mutation or live acceptance.
Unrelated document hunks and existing review records remain local; private directories were not inspected.
No additional source checkout, subagent, controller thread or framework change.

## Implemented invariant

The graph withdraws future exact-input eligibility and commits cleanup authority in one existing Database FIFO write.
Only its successful continuation dispatches. Three highest verified source versions/highest successful revision remain
roots; failed or unverified candidates cannot displace them. Stable/prerelease use existing numeric SemVer ordering.
Active, uncertain and viable waiting consumers protect exact original inputs. Frozen retries are never rebound.
Runtime package closure remains; audit references alone do not retain development exports forever. Shared bytes stay.
`expired_by` is eligibility withdrawal, not proof of physical removal. Historical outcomes/receipts remain unchanged.

`Graph.cpp:121 eligible`, `:382 retain` and `:545 retry` share eligibility and exact assignment references.
`Pipeline::change` advances normal work and derives explicit cleanup operations; its authenticated report and
reconciliation paths handle both. `Pipeline.cpp:331` accepts cleanup success only with successful terminal run,
publisher job and push-step evidence. Possible push remains uncertain; a failed cleanup blocks further cleanup
for that scope. No automatic resubmission or replacement. Completion derives the next bounded batch.

Worker `retention.update` checks the entire granted path/hash/size list before local deletion, then reuses the
signing/index implementations. DEB architecture pools and shared Release metadata are respected. RPM obsolete
metadata is explicitly staged. IPK/APK use the existing verified tool handoff. Retained packages are not re-signed.
`publish.prepare_commit` captures old HEAD, checks staging and creates a root; `checked_push` uses that literal
expected head as a lease. Status defaults remain ordinary parented commits. The 900 MiB guard is unchanged.

Cleanup is an explicit mode of the existing workflow, not a build with fabricated artifacts. It triggers after the
last publication/consumer release without waiting for a future package. Unavailable GitHub or uncertain outcomes
can still postpone deletion indefinitely. There is no timer, new queue, cached keep-list or second graph.

Local graph/SSE expose expiry and cleanup evidence; retry is disabled when exact inputs expired/unavailable.
Public projection omits cleanup operations but retains highest selected source and retained/live prerequisite facts.
Feed rows omit removed package components and identify missing development exports. MariaDB history is not purged.

## Measured lines

Classification: `src/`, worker Python/YAML/Jinja, source publish template, `ui/` and `CMakeLists.txt` are production;
`tests/` is test code. Include untracked new `retention.py`. Counts include working-tree changes against the base.
Physical C++: **3,809 / 3,810**; production net **+333 / +400**; tests net **+270 / +270**.
Production headroom: 67 lines overall, but only one C++ line. Test headroom: zero.

| Category | File | Added | Removed | Net |
| --- | --- | ---: | ---: | ---: |
| production | `CMakeLists.txt` | 1 | 0 | +1 |
| production | `deployment/source/core-ci-publish-bundle.yml.in` | 13 | 4 | +9 |
| production | `deployment/worker/.github/workflows/core-ci-publish.yml` | 13 | 4 | +9 |
| production | `deployment/worker/core_ci/contract.py` | 12 | 9 | +3 |
| production | `deployment/worker/core_ci/feed_status.py` | 32 | 29 | +3 |
| production | `deployment/worker/core_ci/publish.py` | 53 | 41 | +12 |
| production | `deployment/worker/core_ci/sdk.py` | 1 | 1 | +0 |
| production | `deployment/worker/core_ci/sign_native.py` | 1 | 1 | +0 |
| production | `deployment/worker/core_ci/sign_openwrt.py` | 105 | 93 | +12 |
| production | `deployment/worker/status-templates/table.html.j2` | 1 | 0 | +1 |
| production | `src/Graph.cpp` | 219 | 32 | +187 |
| production | `src/Graph.h` | 7 | 0 | +7 |
| production | `src/GraphStore.cpp` | 91 | 62 | +29 |
| production | `src/Pipeline.cpp` | 43 | 29 | +14 |
| test | `tests/GraphTest.cpp` | 117 | 0 | +117 |
| test | `tests/openwrt.py` | 14 | 0 | +14 |
| test | `tests/pipeline.py` | 85 | 7 | +78 |
| test | `tests/ui_enrollment.py` | 2 | 2 | +0 |
| test | `tests/worker.py` | 36 | 0 | +36 |
| test | `tests/worker_provenance.py` | 33 | 8 | +25 |
| production | `ui/app.js` | 4 | 1 | +3 |
| production | `deployment/worker/core_ci/retention.py` | 43 | 0 | +43 |

Totals: production 639 added / 306 removed / +333; tests 287 added / 17 removed / +270.
Growth implements the approved expiry fact and durable cleanup command; reductions share dependency extraction,
physical feed coordinates, workflow evidence, preflight/dispatch application and OpenWrt signing/index operations.
No controller policy was moved into worker code to avoid the C++ cap.

## Documentation accounting

Task-owned current content is counted separately from pre-existing unrelated dirty hunks.
The new design is 194 lines; WORK_LOG retention entries from the design checkpoint onward are 185 added lines.
The deployment guide task hunk is 6 added / 2 removed (+4); template README is 6 added / 0 removed (+6);
AGENTS cap update is 1 added / 1 removed (0). The new report is 171 added lines.
Existing unrelated README, sandbox and deployment/work-log hunks are excluded; nothing is staged.

## Focused commands and outcomes

Build configuration: `build/codex/slim-controller`, installed public SNode.C targets, task fixtures under
`/tmp/codex/build/core-ci/retention/fixtures`. Managed test databases are isolated real MariaDB; GitHub is synthetic.

```sh
TMPDIR=/tmp/codex/build/core-ci/retention cmake --build build/codex/slim-controller --target core-ci graph-test graph-store-test format-check -j2
PYTHONDONTWRITEBYTECODE=1 TMPDIR=/tmp/codex/build/core-ci/retention ctest --test-dir build/codex/slim-controller --output-on-failure -R "^(graph-state|worker-package|openwrt-archive|worker-provenance)-boundaries$"
python3 /tmp/codex/build/core-ci/retention/run-selected.py
python3 /tmp/codex/build/core-ci/retention/run-final.py
node --check ui/app.js
git diff --check
```

Build and format-check passed. Final four graph/worker checks passed (6.79 seconds total).
The selected script runs each check independently so one failure cannot suppress the others:
persistence-graph, persistence-graph-missing, pipeline-chain, pipeline-overlap, pipeline-recovery,
pipeline-preflight-publish-known, pipeline-status-delayed-approval, pipeline-retention, worker-environment-boundaries.
All nine passed. After the final report-phase loop simplification, `run-final.py` reran chain (10.74 seconds)
and retention (3.50 seconds), both passing. These are test elapsed times, not controller processing latency.
Node syntax, changed Python parsing, changed production line <=160 scan and diff whitespace checks passed.

New coverage extends the existing graph/native/OpenWrt/provenance checks and independently registers pipeline-retention.
It includes a 128-component receipt split across bounded dispatches; highest successful revision; failed replacement;
retry before withdrawal and withdrawal before retry; runtime closure versus export expiry; exact removed receipt bytes;
late first FeedProof through two synthetic root contexts and HTTPS lag; cleanup restore/reconcile without replay;
actual local Git root/lease conflict/status parent; real DEB/RPM index/signature boundary and other-architecture preservation.
OpenWrt layout/deletion uses synthetic tools: not real SDK/client acceptance. Existing replay/wrong-key/tamper checks remain.
Temporary cli-probe.py exercised the actual cleanup parser/phase with mocked identity/network/signing boundaries;
it is a synthetic diagnostic, not permanent regression coverage or live proof.

During development, provenance fixture mocks needed the captured-head output, package fixtures needed real retained
bytes after availability filtering, and DB fixture JSON reads needed HEX to avoid CLI backslash escaping.
Those fixture failures were corrected and affected checks rerun successfully. No production bypass was added.

## Request accounting (local synthetic)

Every outgoing request recorded by the synthetic fixture, including token and status work, is included below.
Counts are scenario totals, including cold selection, report order, explicit reconciliation/restart and derivative status.
They exclude GitHub worker SDK/artifact transfers, Git protocol traffic and Pages activity because those do not run here.
One Git push must not be interpreted as one HTTP request.

| Category | Twenty-node chain | Retention scenario | Delayed status approval |
| --- | ---: | ---: | ---: |
| artifact_list | 20 | 4 | 0 |
| execution_dispatch | 40 | 14 | 5 |
| feed_git_identity_and_receipt_proof | 80 | 20 | 0 |
| feed_https | 20 | 5 | 0 |
| repository_and_tag_selection | 8 | 12 | 7 |
| run_job_reconciliation | 11 | 10 | 0 |
| source_and_workflow_approval_proof | 232 | 116 | 22 |
| status_dispatch | 86 | 32 | 2 |
| tokens | 17 | 11 | 7 |
| **Total** | **514** | **224** | **43** |

Artifact lists: one per each of the twenty successful builds. Existing unchanged eight-notification burst assertions
remain unchanged and passed with **zero outgoing requests**. Cleanup terminal duplicates also caused zero requests
or dispatches. The retention scenario includes fourteen real synthetic execution dispatch POSTs, not fourteen packages.
Source and workflow proof counts combine selection/preflight and derivative status approval. In the chain, worker
`commits/main` plus its tree lookup account for 184 of 232 proof reads; status dispatches account for 86 POSTs.
This remains a material status cost; no latch/debounce/cache/timer is introduced to hide it. No comparable before/after
live timing or request baseline was collected for this retention change; historical session totals are not a comparison.
No local millisecond or throughput success is inferred from fast GETs or total fixture duration.

## Remaining gates and limitations

- Historical retries keep old immutable workflow/tooling pins. That publisher makes parented commits, so draining
  assigned work alone does not establish the all-package orphan-root invariant. Activation needs an explicit compatible
  upgrade disposition. No pin rewrite, blanket tooling expiry or new compatibility mechanism is implemented.
- Canonical cleanup scope intentionally rejects old noncanonical paths. Actual feed contents were not inspected here.
- Real feed-writer lease permissions, other refs and old/new workflow trust compatibility need an authorized read-only
  activation check. No orphan root was pushed to a real feed. No GitHub GC timing or physical purge promise.
- Unknown cleanup outcome retains assignment and pauses its scope. Explicit original-run reconciliation is available;
  there is no automatic replay/recovery or finite deletion deadline when GitHub/evidence is unavailable.
- Protected active/uncertain extras can exceed three; the unchanged900 MiB guard can refuse additional publication.
- Old controllers cannot safely ignore expiry annotations after activation; rollback requires a separately checked plan.
- Browser inventory was empty and in-app browser returned `Browser is not available: iab`. UI browser checks were not run.
  JavaScript syntax and API eligibility/expiry outcomes passed; this is not visual/browser acceptance.
- Actual native/OpenWrt client installation, public-link/signature checks and graph/SSE/Pages/live timing remain unverified.
- Source is local and uncommitted. No owner acceptance of live behavior is claimed.

Task evidence/scripts/logs remain in `/tmp/codex/build/core-ci/retention/`. Stopped own database data directories were
removed to reclaim tmpfs; evidence logs/JSON retained. Persistent build remains in the approved build directory.
No unrelated files or private contents were removed.
