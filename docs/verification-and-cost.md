# Verification plan, budgets and initial cost projection

No C++ application, DB schema, network protocol or live deployment was tested by this design stage. Documentation consistency checks are recorded separately. Existing local sandbox tests are prior evidence, not proof of the new architecture.

## Test families and stable boundaries

| Family | Required proof |
| --- | --- |
| T01 Source intake | Verified tag activation only; inactive isolation; exact/older/moved/annotated tags; matching VERSION; duplicate events; latest requested graph; definition differs from main by actual digest; non-CMake source |
| T02 Capture/worker | Immutable reservation/bundle/tooling/target; parent→child registration; saved replay; changed-input refusal; own exports and prerequisite import; no source HEAD substitution |
| T03 Transactions | Prospective graph cycle/namespace rejection under concurrent intake; zero mutation on rejection; row-scoped registration/supersession; DB failure before/after commit; no dispatch before durable admission |
| T04 DAG readiness | Chains/diamonds/deeper graphs, full arm gate at three boundaries, permutations/repeated events, outside-lineage direct arm, independent rows, finite capacity and priority |
| T05 Revision | Five-row/two-root equality; unequal recipe/tag/arm identity; row claims; moved-back tag; transient reservation failure; strict per-row/version growth and uniqueness; seeded interleavings; direct captures always fresh |
| T06 Failure/retry | Deterministic/configuration/expiry/exhausted/cancelled/unsatisfiable terminal retention; no artifact-based reset; independent budgets; operator retry; stale pinned source; overlap off; held backoff/root retirement; fresh separate recovery |
| T07 Execution | Authenticated started/completed; late/duplicate reports; aggregate lag; ambiguous dispatch; job versus run facts; known queued work never timer-expires; 404/scope/rate-limit uncertainty preserves commitments |
| T08 Publisher/client | All formats, own/retained metadata/namespace/arch/version/duplicates, IPK complete healing, APK completeness, siblings/versions/SDKs/retention, public-key authority, signing isolation and real client install/runtime where possible |
| T09 Receipts/stale | Same receipt no package/sign/index mutation; conflicting bytes rejected; checksum-only binding changes; identical baseline no-op; stale artifact rejected even if arm identity unchanged in feed; one pinned re-admission |
| T10 Capacity | Own-worker budget includes captures/publishers/builds; queued/uncertain commitment retained; one publisher alongside eligible builds; direct last; exhaustion fail-closed, no unrelated-org scan |
| T11 Security | HMAC/OIDC/JWKS/claim negatives, task/run/job/artifact/actor binding, source hook cannot impersonate publisher, operator auth, narrow token scopes, no secrets in logs/exports, bounded inputs |
| T12 Presentation | Highest-only lifecycle and source-version heading; all states/icons; per-row revisions; multiline immediate-arm/deeper links; unsatisfiable history not live wait; banner/navigation; published-only Packages links; real job URLs; static/live snapshot times |
| T13 Restart | Crash around every commit/dispatch/ack, artifact, publication permit/push/receipt; rebuild indexes/queues; no duplicate effects; database unavailable/reconnect; local deadline rearming; restore backup |
| T14 Budget/performance | Fast parallel wave, duplicates, lost reports, server outage and GitHub 403/429/5xx; API counts by credential and endpoint; no unbounded reconciliation; one app thread, bounded event-loop latency and memory |
| T15 Publication fence | New source/higher arm between prepare/authorize/push; conflicting feed CAS; DB restart while permit live; successful push with lost confirm; publisher still able to push after timeout; metadata writer serialization; no stale acceptance or unsafe fence release |
| T16 Migration/reuse | VOC-only target proof, safe quiescence and receipt import, single scheduler/writer after cutover, deployment-only differences, no SNodeC production mutation, independent owner config, rollback rehearsal |

## Mandatory diamond sequences

Each is tested through public task/report/admission/publication boundaries, not just a helper predicate. Use more than one row and vary root order and event delivery order.

| Case | Sequence | Expected |
| --- | --- | --- |
| A | R publishes, A and B stale | A/B captures admitted; no J capture/build |
| B | A child publishes while B pending/running/failed | Zero J captures/builds under every root |
| C | Direct A publishes under its own root while B bound to old R | Zero early J; the earlier narrow affected(root) predicate would fail this |
| D | B publishes inside or outside R lineage | Exactly one fresh J capture/build/publication for resolved arms; duplicate reports/wakes do not add another |
| E | B fails deterministically, operator repairs/retries, B publishes | Original failure retained; no root-level synthetic J; one J after recovery |
| F | J child pending, moved R makes arms stale | Child waits with pinned source, no parallel fresh J capture; after arms recover builds once; failed arm closes child actionably |
| G | Baseline changes after J capture intent | Child admission uses actual current binding/gate, no duplicate |
| H | Synthesized stale J artifact reaches publisher | Rejected before mutation; one eligible pinned re-admission after arm recovery |

## Single-tasking acceptance

Use SNode.C real listeners, async HTTP client and async MariaDB path. A fake clock/transport is allowed for exhaustive state transitions; public integration tests use actual async I/O and transactions. Inject delayed DB/network replies and verify that health/status routes continue to progress. Check for accidental synchronous connector calls, blocking subprocess waits and extra application threads. Bound individual graph/JSON operations and yield long traversals.

Separate storage-service threads from the application's thread count. MariaDB being another process is an explicit deployment dependency, not an application thread-pool loophole. The user must approve it. No host installs or DB connection were performed for this design.

Use existing publisher tests as guards; do not rewrite them merely to fit the new control API. Update changed boundary assertions with equal/stronger evidence. Preserve real-tool environment skips honestly. No live client installation success is claimed from stdlib-built fixtures.

## API acceptance budget

Proposed acceptance target: below 2,500 chargeable API requests in every rolling hour for the configured installation during the agreed fast parallel sandbox workload, including controller and worker calls that share that bucket. Record a second total for HTTP requests, Git transfers and artifact-storage traffic: none is interchangeable with primary REST quota.

Measure a full 178-row deployment-sized successful model and a smaller real VOC fast-wave workload. A happy-path model with C capture attempts, B builds and P publishers must make O(C+B+P) targeted requests, not O(number of wakes × all organization repositories). A model without retries cannot establish a bound under arbitrary outages or user activity.

Count installation-token creation, run/job/artifact verification, capture downloads, pagination, receipt reads, webhook-delivery recovery, JWKS refresh, metadata-only exports and startup. No per-phase HTTP request is exempt merely because it runs in an action rather than core-ci. Installation quotas and personal-user quotas are separate; measure each instead of summing unrelated limits. Rate headers provide actual charged usage where observable.

Initial planning expectation for 178 successful builds/publications is a few thousand targeted API requests over the whole wave, not the old estimated tens of thousands of organization scans. This is not a certified count. Build an instrumented protocol model before approving numeric performance claims. Forced failure/retry cases have their own bounded request budgets and cannot be silently excluded from the final report.

Webhook first; targeted fallback only for known overdue executions. Webhook delivery recovery is low-frequency and bounded by durable position, not a full-org active-run scan. After 403/429, wait for actual Retry-After/reset before retry. On restart do not hammer every known execution at once. Admission is bounded by own-worker budget. No independent always-polling worker observer is proposed.

## Cost: measured old spans versus estimated replacement

Measured old mixed-responsibility file sizes are in current-system-trace.md. The following is a **preliminary design projection**, not measured implementation LOC, a cap or approval to implement:

| New area | Estimated production lines | Why necessary / what it replaces |
| --- | ---: | --- |
| C++ domain state, DAG, revision/coverage/priority policy | 450–750 | Replaces tick/coverage/root/reopen mechanisms spread across Python and Git snapshots |
| Async Store and transitions/recovery | 300–550 | Replaces Git status transactions as scheduling persistence; genuine new DB integration |
| Async GitHub transport, claims/authentication, targeted recovery | 400–750 | Replaces gh subprocess polling/dispatch/callback verification; includes genuine HTTPS/OIDC work |
| SNode.C lifecycle/config/routes/operator boundary | 250–450 | Replaces separate control invocations; follows framework conventions |
| Pure status projection/render/export | 200–350 | Replaces Python projection/render after parity, not parallel production rendering |
| SQL schema/build/format integration | 180–330 | New application/database integration, self-contained build tooling |
| Worker/publisher protocol adapters and fence | 200–400 | Replaces feed scheduling registration/reserve writes and old scheduling gate consultation |
| **Gross new production** | **1,980–3,580** | Includes ordinary C++ declarations; compact policy does not guarantee fewer physical lines |
| **Old mechanisms removed, estimate** | **550–850** | Partial functions/workflow steps, not all three files; refine from actual diff before approval |
| **Indicative net production** | **+1,130 to +3,030** | Must be remeasured; new server/persistence/security are real additions |

New test estimate: 2,000–4,000 lines for state/security/restart/boundary proof, plus retained publisher/client suites. Documentation is separate. Byte projections cannot be reliably measured before code exists; report actual per-file bytes after each implementation slice. Moved publisher code remains moved complexity, not a deletion credit. Ordinary C++ formatting should not be compressed to chase these estimates.

Reduction first: remove full-org scanner, control cron/wake workflow, workflow-chain schedule reconstruction, feed scheduling writes, source App-key relays, duplicate render/gate logic. Modification next: retain immutable worker bundles and trusted publisher tooling with new thin boundaries. Add only then: local Store, HTTP trust, explicit attempts and commit fence. No unreviewed generic engine.

## Suggested implementation order after design approval

1. Framework async persistence/HTTPS proof and schema/SQL safety decision; no live credentials needed.
2. Pure Scheduler plus reference-vector/state-transition/property tests.
3. Store transactions, crash cases and admitted-attempt recovery.
4. GitHub protocol mock, then real local HTTP/OIDC verification fixtures.
5. Refactor sandbox worker boundaries; preserve publication tests.
6. Prove publication fence and status export before sandbox cutover.
7. Measured VOC wave and recovery cases, then a separate SNodeC migration decision.

The initial repository contains no production code and no claims that these tests already passed.
