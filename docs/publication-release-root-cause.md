# Why the OpenWrt 24.10 join was dispatched before public availability

Investigation date: 2026-10-31. This is an analysis-only report. No controller,
worker, workflow, test, deployment input or running process was changed. No
build, retry or reconciliation was triggered.

## Governing invariant and conclusion

A dependent must be released only when every frozen prerequisite has evidence
sufficient for consumption through the assigned feed. The graph must derive
readiness from its one durable published state; the producer of that state must
qualify the correct external publication boundary.

The current implementation qualifies Git feed-writer completion as publication
completion. Its consumers fetch packages from GitHub Pages, which deploys
separately. The controller already knows the public-confirmation job is running,
but deliberately does not include that job in the release condition. This is an
explicit boundary mismatch, rather than an inference about slow notifications.

The exact failed HTTP request remains unrecorded. Independently, feed-tree and
Pages deployment evidence identify app r6 as absent from the last successful
Pages revision when the worker returned 404. Those are distinct conclusions.

## Direct committed evidence

The real browser's committed SSE patch sequence 230 was received at
18:15:38.534 UTC (19:15:38.534 Austrian local time). In that one patch:

- app attempt `049b98e9e99386f40f839b89115125cea68d9e236def5923c0f13ab3040dc4a4:6`
  becomes `published`;
- its publication run 41228278762 is `in_progress`, conclusion null;
- its matched Publish job 124617485989 is completed/success;
- its Confirm public receipt job 124617589420 is `in_progress`, conclusion null;
- join attempt `aa4137d44dcd005bbb4231679713edf322b2cfa6feb16c6de9162740d2077297:5`
  becomes `building`, with `build_authorized` 18:15:37 UTC.

Thus the controller did fetch the incomplete confirmation status and still
committed the release. The SSE timing is receipt by the observer, not an exact
commit timestamp. The state and dependent assignment were already committed by
then, because notification follows the successful database update callback.

Extract: `/tmp/codex/build/core-ci/prerequisite-404-20261031/release-sse-evidence.json`.

## Full controller path

1. **Authenticated notification.** `src/main.cpp:216` checks the raw-body HMAC.
   `src/Execution.cpp:330` checks installation/repository identity. A workflow job
   or run notification is matched to the immutable assigned run by name or saved
   run ID (`src/Execution.cpp:346`). It requests a fresh observation; the webhook
   payload alone does not constitute publication evidence. HTTP 202 acknowledges
   receipt before asynchronous observation finishes.
2. **Run/job identity.** `Controller::run`, `src/Execution.cpp:83`, fetches the
   assigned run attempt, checks repository, actor, workflow_dispatch, attempt 1,
   pinned commit/path/blob and assignment title. It fetches every job, retaining
   their status and steps, and identifies the Publish job by its exact assignment
   label. This is why confirmation status is present in the same stored facts.
3. **The premature condition.** `src/Execution.cpp:311` checks only that the
   attempt is publishing and the matched Publish job has conclusion success.
   Unlike the build path above it, this path does not require whole-run success.
   It neither requires nor examines Confirm public receipt success. Any relevant
   authenticated notification that observes those conditions can enter it.
4. **Receipt read from GitHub, not the feed.** `publicationReceipt`,
   `src/Execution.cpp:247`, finds the latest Git commit touching this execution's
   receipt path, then reads those bytes at that immutable revision through the
   GitHub Contents API. It does not GET the public receipt or package URLs.
   `verification_started_at`/`verified_at` here measure that GitHub receipt-read
   operation; they do not establish Pages availability.
5. **Published state.** `src/Graph.cpp:397` checks receipt execution/attempt,
   retains the receipt/facts and sets `state = published`. `ready = true` invokes
   `commands(graph)` within the same Store change (`src/Graph.cpp:429`).
6. **Exact prerequisite barrier.** `commands`, `src/Graph.cpp:245`, finds the
   existing exact core, app and side executions. All now have current state
   published. The count check at line 261 therefore passes, freezes those exact
   receipts into join's assignment, records building/authority and returns the
   build command. It does not discover tags again, use UI order or bypass an
   exact dependency. Its input published state is semantically premature.
7. **Commit before dispatch.** `Store::change`, `src/Store.cpp:81`, persists the
   graph update before invoking dispatch. `src/main.cpp:134` routes the committed
   commands to `Controller::send`; `src/Execution.cpp:61` constructs the standard
   workflow_dispatch inputs. No failed/uncertain commit is needed to explain the
   observed release. The assignment was successfully committed too early with
   respect to the external availability boundary.
8. **Later facts cannot repair release.** Once published, later confirmation
   events merge facts but do not undo published state or recall a dispatched
   dependent. Published terminality is intentional. The solution belongs before
   that transition, rather than in a later rollback or automatic replacement.

## What actually happened in GitHub

All times below are Austrian local time on 2026-10-31.

| Time | Evidence |
| --- | --- |
| 19:15:28.655 | Publish job prints pushed feed revision `5fe31ca…` |
| 19:15:30 | App Publish job completes successfully |
| 19:15:32.728 | Confirm queued webhook delivered |
| 19:15:32.737 | Publish completed/success webhook delivered |
| 19:15:35.536 | Confirm in-progress webhook delivered |
| 19:15:36.233 | Confirm job begins its HTTPS command |
| 19:15:36–37 | Controller reads app's committed GitHub receipt |
| 19:15:37 | Controller authorizes join in the graph |
| 19:15:38.534 | Observer receives committed app Published / join Building patch |
| 19:15:41 | Join dispatch accepted, recorded HTTP 200 |
| 19:15:44 | Join build job starts |
| 19:16:42.343 | Worker package request raises HTTP 404 |
| 19:16:45 | App's deploy-pages step finishes |
| 19:16:47 | Pages deployment reports success |
| 19:16:47.147 | Confirm command prints verified public receipt |
| 19:16:50 | App Confirm job completes successfully |
| 19:16:52.992 | Whole publication run completed webhook delivered |
| 19:16:53.260 | Confirm completed webhook delivered |

There are several relevant notifications before release. The controller does
not persist a causal delivery GUID on the transition, so the exact individual
notification whose asynchronous observation first committed it cannot be
identified. This does not affect diagnosis of the explicit predicate or the
committed incomplete-confirmation facts. Delivery timestamps are GitHub's
records, not exact local handler ingress measurements.

The latest successful Pages revision before the failure was side's `57060f0…`,
reported successful at 19:15:45. Its complete Git tree contains the selected core
and side IPKs, but not app `demo-app_2.1.0-r6_x86_64.ipk`. The next revision,
app's `5fe31ca…`, contains all three. Its Pages deployment had not yet reported
success at the failure. This establishes the missing file in the preceding
published revision; it does not reveal precise CDN edge switch times.

The Publish job's pinned YAML ends after prepare/push. A separate Confirm job
checks the immutable receipt over public HTTPS, matching its prepared SHA-256.
It retries read-only observations on GitHub for up to its 600-second deadline,
with 10-second sleeps. It does not hold the feed writer's concurrency group.
The controller ignores this existing completion signal rather than performing
that confirmation twice.

## Native targets expose the same integration gap

OpenWrt tools extraction runs before publish, so its Confirm job runs afterward.
Native tools extraction is intentionally skipped. Publish explicitly handles
that skipped tools prerequisite with a status-aware condition. Confirm does
not: its condition lacks a status function, so the skipped ancestor propagates
through GitHub's default success condition. Observed native core jobs for Debian
amd64, Debian arm64 and Fedora all show Confirm completed/skipped.

GitHub documents skipped dependency propagation:
https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-jobs

Requiring Confirm success in the controller alone would strand native targets.
Requiring only whole-workflow success would still accept native runs whose
confirmation was skipped. Treating skipped as confirmed would retain the gap.

## Why current checks did not prevent this

The current documented override in `docs/contracts.md:447` explicitly accepts
publisher-job success and committed receipts without waiting for public Pages.
The implementation follows that authority. This is the concrete engineering
consequence: that authority is insufficient for downstream consumers of Pages.
It does not require restoring duplicate artifact/signature/index validation to
recognize the distinct deployment stage.

The focused graph tests construct a receipt and call `applyEvidence` directly
(`tests/fixtures.h:218`). Their observation fixture supplies one successful
Publish job; it does not represent a pending public confirmation. They verify
exact graph release, immutability and persistence, not whether remote observation
has qualified the correct publication boundary. The native HTTP startup checks
exercise authentication/listeners, not this full GitHub/Pages sequence.

The SSE fix did not implement this predicate. The observed patch correctly
reported the committed graph, including its premature Published state. No
measurement establishes that the SSE change caused this failure or that its
performance alone explains previous successful runs.

## Minimal coherent correction, not implemented

Use the existing trusted workflow's confirmation completion as the release
boundary, consistently across targets. Make the native Confirm job execute
after successful Publish, then require that same pinned run/attempt's matched
Publish and Confirm jobs to succeed before the controller transitions published.
Preserve exact receipt identity, same-transaction dependent release and successful
commit before dispatch. Existing relevant webhooks can drive the observation.
No global feed-idle wait, index-membership proof, new controller timer, feed lock,
cache or replay is needed for that route.

This would change the documented publication authority and the workflow blob/pin;
it has not been authorized or implemented by this investigation. Existing frozen
assignments must retain their own pins. Failed/skipped/missing confirmation must
not be treated as success or as definite publication failure. Historical published
executions must not be re-evaluated automatically.

A corrected receipt boundary addresses the demonstrated Git-to-Pages gap, but
one runner's receipt observation is not proof that every immutable package is
simultaneously downloadable from every CDN edge. No stronger guarantee is
claimed. The separate missing failed-URL logging remains relevant to diagnosing
any future download error.

Focused validation should exercise Publish success while Confirm is pending,
Confirm success releasing exact dependents once, failed/skipped confirmation,
native confirmation actually running, a later writer changing shared indexes,
independent ready work, duplicate/out-of-order notifications and restart without
replay. No tests or live runs were executed for this read-only investigation.

## Identity and retained evidence

Fresh checkout `/home/voc/projects/snodec/core-ci-new`, branch
`feat/graph-controller`, no commits or remote. Production C++ remains 2,328
physical lines; source/build identity unchanged from the v2.1.17 report.
Production/test changes zero. Documentation: this report and WORK_LOG.md.

The new compact release facts, app webhook delivery timeline, complete run log
archive, deployment status histories and full feed trees are retained in
`/tmp/codex/build/core-ci/prerequisite-404-20261031`. Claude's earlier independent
review and pinned workflow/proof-contract extracts are under
`/tmp/codex/build/core-ci/claude-publication-review-20261031`.
