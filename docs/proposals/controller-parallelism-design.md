# Controller parallelism proposal

This proposal turns the [joint review](controller-parallelism-review-agreement.md) into
three implementable checkpoints. It is a design, not approval to change code or deployment.
The recommended choices requiring owner approval are collected near the end.

Reviewed source: `4257cb0fc67412da8696afdba307fc2e85ff64aa`, branch `deploy/core-vchrist`,
canonical checkout `/home/voc/projects/snodec/core-ci`. Default branch is `main`; remote is
`git@github.com:SNodeC/core-ci.git`. The working tree contains deployment and review
documentation. Preserve it. No open PR was returned by the read-only listing.

For later implementation, use `fix/controller-parallelism` based on then-current `main`
after the current checkout's task ownership/documentation is settled. Do not switch this
dirty checkout or make another checkout automatically.

## Invariant and limits

Independent work overlaps within configured resource bounds. A shared feed update and a
database transaction remain exclusive. A dependent starts only after every exact selected
prerequisite is verified published. One graph owns execution state; no new persisted queue,
state, schema, scheduler, controller thread or polling timer is needed.

Keep the existing source/tag/retry policies, definitions on GitHub, immutable assignments,
package identities, history, signatures, webhook validation, attempt-one binding and
artifact/receipt verification. Unknown mutations keep their assignments; they are never
replayed or replaced automatically. Builds and all package/image/SDK work stay on GitHub.

Recommendation: eight concurrent operations **per HTTP client**, and eight assigned
publications **per feed**, initially. These are separate deployment settings, not platform
limits or performance claims. With the two current clients, up to eight GitHub API operations
and eight public-feed reads can overlap. The database remains one asynchronous connection.

## Current responsibilities and waits

| Boundary and source | Inputs and lifetime | Current wait | Proposed treatment |
| --- | --- | --- | --- |
| `Pipeline::request/readSources/commitSelection` | Shared Selecting object owns enrolled rows, immutable source results and callback until completion | Enrollment query, then repositories read sequentially, then global generation guard | Concurrent independent source reads; retain all checks and the explicit guard |
| `SourceReader::select/file`, `Pipeline::workflow/proof/read` | Captured repository/ref/path and shared callbacks; immutable proof cache already exists | Tag/ref resolution and tree walks require prior results; identical reads already coalesce | Keep each real proof chain ordered; overlap different chains without adding a cache |
| `GitHub::authorize/request` | Existing exact-scope token entry and waiting callbacks | Cold token precedes its API request; client serializes every request | Preserve token coalescing/scope/expiry; bound concurrent transport |
| `GraphStore::commit`, `Database::write` | GraphChange/SQL currently captured before entering Database FIFO | Global generation captured too early; next SQL starts before previous graph application | Derive at FIFO head; apply before releasing the operation |
| `Pipeline::advance/dispatch` | Selected nodes become durable assignments before HTTP | Builds fan out; only one publisher admitted; HTTP remains serial | Keep commit-before-dispatch; fill available publication capacity and dispatch independently |
| `Pipeline::event/observe/record/complete` | Validated immutable facts; completionReads owns in-flight completion exclusion | Webhook ACK is already early; stale commit retries and unrelated HTTP cause waits | Same validation path; lazy fact application; keep completion-read coalescing |
| `buildArtifacts` | Assigned build identity, one bounded artifact listing, fresh attempt checks | One required artifact request after successful completion | Preserve one lookup; derive accepted result against current graph at persistence time |
| `publicationResult`, `FeedProof` | Assigned publisher and exact receipt; callbacks retain selected identities | Skipped push can fail directly; otherwise six API reads and one HTTPS read normally | Keep independent proof; overlap different executions; no shared feed reservation for proof |
| `GraphStore::onChanged`, `main.cpp` | Current committed graph and existing SSE receiver snapshots | Status starts before execution continuation; full view even with no receivers; enrollment read every change | Execution continuation first; view only for receivers; enrollment event only on enrollment changes |
| `Pipeline::refreshStatus` | Existing refreshing flag and captured minimum generation | Approval result discarded/re-read if generation changes | Use approved result; dispatch the captured minimum, then handle a newer generation |
| Publisher workflow | Assignment, build result and trusted tooling; each job owns its filesystem | Workflow-wide lock includes validation, setup and HTTPS propagation | Validate outside lock; serialize feed job; confirm public bytes outside lock |
| Status workflow | Current public facts, templates and current feed | Feed job includes tool installation | Keep current safe job boundary initially; quantify remaining setup rather than claiming it is parallel |
| Feed update | Current branch head, retained packages, trusted keys, prepared bytes | Real shared Git mutation | Keep current-head checkout through checked push exclusive across publisher and status jobs |

There are two distinct HTTP clients: `github-http` has fixed logical origin api.github.com;
`feed-http` handles the configured public feed. This proposal does not introduce origin pools.
Source selection still reads every required enrolled selection. The existing F3 rule stays:
an unreadable newly selected definition is reported and blocks the request, including a
required prerequisite introduced by that definition. A known repository with no SemVer tag
is the existing explicit exception; a generic 404 is not silently omitted.

## Checkpoint A: derive and commit one graph change coherently

### Interfaces

Replace, rather than supplement, eager graph commit:

```cpp
using StatementProducer = std::function<std::vector<DatabaseStatement>()>;
void Database::write(const StatementProducer& prepare, const Callback& callback);

using GraphDerivation = std::function<GraphChange(const Graph&)>;
using CommitCallback = std::function<void(const DatabaseResult&, const GraphChange*)>;
void GraphStore::commit(const GraphDerivation& derive, const CommitCallback& callback);
```

The GraphChange pointer is present only for an actual successful change, valid during the
callback. Consumers copy committed nodes before retaining them for HTTP callbacks. A shared
optional GraphChange in this one operation owns it; this is not another queue or graph.

Add `preparationFailed=false` to the existing transient DatabaseResult. It distinguishes a
producer/validation rejection before SQL from a database failure. This replaces the eager
try/catch error classification; it is needed to retain existing 409/422 versus 503 responses.
It is not persisted and is not an execution state. Existing three-field initializers retain
their meanings. No new error taxonomy or generic command framework is proposed.

Both GraphStore write callers use this producer interface. Enrollment still captures and
checks its explicit expected generation. Constructing its SQL at the head does not refresh
that expected generation or bypass namespace checks.

### Exact sequencing

The existing FIFO remains occupied throughout its completion callback:

1. At the head, check database mutation uncertainty before calling the producer.
2. Derive from current graph, validate, identify no-op, and build SQL using current generation.
3. For nonempty statements, execute START/SQL/COMMIT using existing asynchronous APIs.
4. On success, GraphStore applies the prepared change and advances generation.
5. Invoke the caller's continuation. It may enqueue follow-up DB work and start independent
   asynchronous HTTP; it must not wait for HTTP. The current FIFO entry still prevents a
   recursively enqueued database operation from starting prematurely.
6. Run derivative `onChanged`, then return to Database.
7. Database removes the completed FIFO entry and starts the next operation.

Thus application and generation advancement precede every next derivation; execution
continuation precedes its derivative work. Enqueuing while completing is safe because the
current entry has not been popped. There is no GraphStore write queue.

Consolidate Database's repeated write `completed(); callback(...)` pairs into one finish
path that invokes its callback **before** `completed()`. Keep **query** completion in its
existing release-before-callback order: startup restore failure can destroy the Database
wrapper inside that callback (`main.cpp:94–104`). No code may access the wrapper afterward.
Read callbacks are framework-asynchronous: MariaDBConnection::execute_async schedules its
commandStartEvent rather than invoking the query callback inside enqueue. Existing startup
failure/cleanup must be tested, not assumed from write-callback behavior.

Avoid recursion through thousands of synchronous no-ops/rejections by reshaping enqueue/
completed into one drain loop. Keep shared ownership of the current callable while executing
it; after it returns, a different queue head means synchronous completion and the loop can
start that head. The same head means asynchronous work remains pending. One drain reentrancy
guard replaces the current recursive start-next calls. A reentrant enqueue/completed call
does not recursively drain. This is bookkeeping in the existing FIFO, not another queue,
timer or next-tick mechanism. Current write callers retain Database ownership throughout
their completion; startup teardown occurs through the separate read path above.

| Outcome | Required behavior before releasing FIFO |
| --- | --- |
| Producer/validation throws | Definite preparation failure; no transaction, graph application or derivative notification; caller receives original error classification |
| Empty/no-op SQL | Success with no GraphChange pointer; no generation change, status dispatch or transaction |
| START fails | Existing definite database failure; no graph application |
| Statement/affected-row check fails | Existing rollback/connection-loss classification; no stale-node retry |
| Rollback succeeds | Definite failure; no graph application |
| Commit succeeds | Apply once, continue once, notify once, then start next operation |
| Commit or rollback uncertain | Set existing mutationUncertain before callback/release; queued writers do not invoke producers |
| Read completes or fails | Existing query finished guard delivers once; release before callback, preserving startup teardown ownership |
| Framework shutdown | No new sends/transactions after the framework boundary; unfinished operations are not replayed; restart restores durable assignments |

All write finish paths own their operation locally while invoking callbacks. Producer exceptions
are caught before SQL. Owned completion callbacks must handle domain errors without throwing.
As a final boundary, if an unexpected callback exception occurs, retain the existing mutation
uncertainty latch, log without private data, and release the FIFO without invoking it twice.
This conservatively blocks further writes rather than assuming the in-memory application
completed. Do not call application stop/exit or add a lifecycle manager.

### Caller conversion and deletions

| Caller | Capture before enqueue | Derive at FIFO head / continuation |
| --- | --- | --- |
| `commitSelection` | Immutable source map, root, automatic flag, original generation | Recheck explicit generation and select; use committed change for selected count |
| `retry` | Execution ID | Current graph.retry; advance after commit |
| `advance` | Callback and response-owned blocker collection | Scan current graph once; commit all selected nodes together; dispatch only returned committed nodes |
| `record` | Execution/attempt IDs, phase, verified identity/observation, recovery/readJobs flags | bindRun on current graph; unchanged non-recovery reports finish without HTTP; changed/recovery completion uses current facts |
| `change` and all rejected/failed/built/published callers | IDs and accepted immutable result/reason/receipt facts | Replace Node argument with derivation; recheck current attempt; build one GraphChange |
| `enroll` | Exact definition and explicit expected generation | Existing conditional enrollment/namespace SQL, retaining guard and GitHub-write feedback |

Delete `change`'s stale Node retry and `record`'s generation-conflict retry. Keep the SQL
generation guard against external writers/stale restoration. Do not weaken Graph::validate.
Prepared changes cannot overwrite newer evidence. A terminal or newer attempt makes a late
result a no-op/rejection according to existing Graph methods, never a replacement execution.

### Status and SSE changes in this checkpoint

Change the existing notification signature to
`onChanged(bool committed, bool repositoriesChanged)`. Only a successful enrollment sets
the second argument. This argument replaces unconditional repository-list queries; it does
not store a second configuration view or add an event bus. Graph SSE still reports database
uncertainty on failed mutations where it currently does so.

In main, copy receivers and build/send the graph view only when receivers exist. Query/send
repositories only for repositoriesChanged. Preserve `/api/events` initial graph/list bootstrap,
disconnect cleanup, heartbeat and `/api/repositories` explicit reads. All enrollment save/
import callers use GraphStore.enroll, so this covers their SSE feedback without a cache.

In refreshStatus, retain `refreshing`. After approval succeeds, POST with the captured
minimum generation even if the graph advanced. After that response, dispatch newer-generation
derivative work if needed. Approval failures and uncertain/rejected POSTs remain logged.
There is no same-generation blind replay, timer or outstanding-run latch. If no newer commit
arrives after failure, the existing explicit reconcile endpoint/startup refresh supplies
another opportunity; do not promise automatic eventual success in that case.

Do not filter on the five states. Queued/running/completed job facts, accepted build results,
times and links remain public progress. GitHub's status workflow retains its existing outer
single-pending coalescing, and reads live facts when it runs.

Checkpoint A prevents loss/starvation and unnecessary derivative work. It does **not** yet
remove API serialization or the long publication gate.

## Checkpoint B: bounded concurrent HTTP and source reads

### Verified installed framework interfaces

The installed package is discovered at `/usr/local/lib/cmake/snodec`, with public headers
under `/usr/local/include/snode.c`. These paths document this inspection; implementation
CMake continues using `find_package(snodec CONFIG ...)` and exported targets.

| Interface/pattern | Verified use |
| --- | --- |
| `web/http/client/Client.h` | One client creates HTTP contexts with shared HTTP-connected/disconnected handlers |
| `core/socket/stream/SocketClient.h::connect` | Each call creates an independent FlowHandle; retry/reconnect remain disabled |
| `SocketConnector.hpp::connectEvent/init/finishAttempt` | Both immediate and asynchronous TCP success deliver that connect call's status callback, then onConnect, then onConnected synchronously |
| `MasterRequest::getSocketContext()->getSocketConnection()->getConnectionId()` | Public connection ID correlates HTTP callbacks after onConnect |
| `core/socket/stream/tls/SocketConnection.hpp::startSSL` | SSL_new copies the configured context; keep one origin's TLS identity stable while its flows exist |
| `FlowHandle::terminateFlow/setOnFlowTerminated`, weak MasterRequest/disconnect | Correlate final connect failure/cancel separately from an intermediate address failure; termination is not evidence of a remote mutation result |
| `net::config::ConfigInstance`, `utils::SubCommand` | Public per-instance option registration; qualify the ConfigInstance base to avoid multiple-inheritance ambiguity |

Studied framework `apps/http/httpclient.cpp`, `httplowlevelclient.cpp`, database/testmariadb.cpp,
MQTTSuite mqttcli/mqttstore HTTP clients and mqttstore/lib/MariaDbStorage.cpp. These demonstrate
request-context ownership, configuration, callbacks and asynchronous database use. Their
retry-enabled long-lived MQTT policy is not copied into controller mutations. Existing
explicit initial MariaDB connect/lifecycle in core-ci remains unchanged.

A temporary object-only compile against installed `snodec::http-client` and
`snodec::net-in-stream-tls` verified FlowHandle, request connection-ID lookup, termination and
option registration. The first compile found ambiguous unqualified option methods; casting
to ConfigInstance corrected it. This proves interface compilation, not runtime concurrency.

### Concrete correlation and lifetime

Keep the existing deque as the sole collection of queued/active HTTP operations. Add one
transient started marker and optional FlowHandle per operation, replacing the implicit
assumption that only the front is active. Reuse its connectionId and weak MasterRequest.
Look up the bounded active operations by connection ID; no second request queue is needed.

Use the captured operation in its per-connect status callback. On TCP success, set one
short-lived HttpClient `connecting` pointer. The immediately following onConnect consumes
and clears it, assigning the real connection ID. Do not invoke application callbacks or
launch another flow between those two callbacks. This uses the verified installed connector
ordering, not launch order, predicted IDs or peer-address matching. HTTP/TLS callbacks then
find the operation by actual connection ID. A focused runtime test must pin this ordering
before shipping; framework upgrades must retain that test.

Replace connect-front/finish-front with pump and finish(operation). Pump starts oldest
unstarted operations for the current origin up to capacity. A small reentrancy guard prevents
synchronous connect errors or callback-enqueued requests from recursively pumping the deque.
It replaces the single-front launch rule; it is transport bookkeeping, not execution scheduling.

At HTTP termination, move the operation's callback into a local and invoke it promptly;
an empty callback means delivery already occurred, so no new delivered flag is needed.
Keep the operation's transport slot until actual transport disconnect. That callback erases
membership, delivers only if a callback remains, and pumps capacity. This both avoids waiting
for TLS shutdown before applying a result and prevents an origin change while an old transport
is still closing. Response/parse callbacks retain the operation but cannot deliver twice.
A final connect failure without a transport releases the slot. Do not treat every error
status as final: the framework can try the next resolved socket address, and strips its
internal NO_RETRY marker before invoking the public status callback. Record that error;
finish on the captured flow's termination instead. If TCP later succeeds within that same
flow before any HTTP submission, clear only that intermediate connection error. This is
framework address establishment, not an application mutation retry.

Register a flow-termination callback with a weak operation reference, avoiding a flow/operation
ownership cycle. After connect returns, check whether it terminated synchronously before the
hook was registered and finish once in that case. Never reinsert a finished operation. Any
late success/context for an operation no longer owned by the deque is closed without sending
HTTP, never rebound to the next operation. Measure response-to-disconnect slot occupancy
separately. Shutdown may cancel a connector without terminating its flow, as qualified below.

Maintain Host, SNI, certificate verification, body bounds and Connection:close. Configure a
single origin's shared TLS parameters before launching its batch. Never mutate remote/SNI/
host identity while that client's flows are active. If queued work requests another origin,
stop launching at that queue boundary, drain the current origin, then perform the existing
renew/host change. This preserves existing routing, not an origin-pool refactor. Normal
FeedProof already requires the configured origin; GitHub is fixed-origin.

Register `--max-in-flight` on each existing named HTTP instance before SNodeC::init, default 8,
accepted range 1–16. Thus existing framework help/configuration shows the whole setting.
No app configuration framework or new named clients are required.

GitHub's documented concurrent API limit is 100, shared with GraphQL; that is not a recommended
operating target. Eight gives independent chains room while limiting simultaneous bounded
bodies and burst pressure. It does not guarantee freedom from primary/secondary rate limits.
Return rate-limit errors under current policy; no automatic backoff/replay is added.
[REST rate limits](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api).

On STOPPING, launch nothing new. Available callbacks finish/remove operations at most once;
known unsubmitted queued work may be canceled as currently. Critically, connector cancellation
can occur without onStatus: SocketConnector::destruct only closes its attempt. Do not invent
a shutdown callback guarantee or call Pipeline after its lifetime. At process teardown,
remaining closures are discarded with the transport; durable assignments survive and startup
reconciles them. No callback is emitted from HttpClient destruction after Pipeline is gone.
Exactly-once applies to a delivered normal response/error, not a promise of delivery across
process shutdown. Test connecting/TLS/response shutdown and memory lifetime explicitly.

### Concurrent source aggregation

Replace Selecting.index recursion with a results vector indexed by the existing enrolled
row order and a remaining count. Start each independent source selection; transport capacity
bounds actual connections. Each callback writes only its own slot and decrements once.
After all finish, validate and assemble sources in enrollment order, selecting the first
reported error deterministically. Report the repository; do not drop unknown definitions.
No callback can mutate selection after its single final response.

Retain expected tag object, enrollment IDs, no-tag exception, deployment pinning, required
prerequisite traversal and the final generation guard. Waiting for all dispatched reads on
an error avoids callback cancellation machinery; it trades some error-response time for a
single simple lifetime. The graph is not partially accepted.

Checkpoint B removes avoidable same-origin head-of-line blocking within capacity. It does
not narrow generation guards or make remote work complete in milliseconds.

## Checkpoint C: publication admission, write job and confirmation

### One graph-owned admission calculation

Recommend write-boundary exclusion (P1), capped at eight latest attempts in `publishing`
per feed. Count assigned-but-unbound, validating, writing, confirming and uncertain attempts
conservatively until published or definitely failed. One unknown node consumes one slot;
it never becomes a successful prerequisite. Eight unknown nodes stop admission without a
new queue/state. Existing reconciliation/restart and explicit recovery remain necessary.

Register `pipeline --publication-limit`, recommended default 8, range 1–32. Graph receives this
deployment limit during configuration, not from repository JSON. It is not persisted state.

Reshape Graph::publicationReady into one `publicationBlocker(id, reservedInChange)` calculation:
verify built result/dispatch bounds, selected feed capacity and, only if chosen, P2's halt.
Use it for beginPublication, the advance scan, change validation and existing blocker views.
The scan tracks only candidates reserved in its current GraphChange, keyed by feed. This
temporary count is not another authority/queue; it prevents one atomic batch overfilling
available capacity. Graph::validate independently checks the whole proposed batch through
the same calculation. Invalid direct mutations still fail.

Remove the old publishing-feed uniqueness sets in restore/validate and the one-publication
flag in Pipeline::advance. Restore must accept valid previously admitted assignments even
if the operator lowers the current limit; block new admission rather than reject history.
Every definite failure/published transition already advances work, filling released capacity.

For a built node awaiting publication, Graph::blockers uses publicationBlocker rather than
the generic "attempt is building" reason. Waiting build nodes retain their prerequisite/
dispatch blockers. GraphStore/publicView continue consuming this one derived calculation;
do not add presentation-specific eligibility. Wire the parsed pipeline limit into GraphStore's
Graph at construction before restore, and pass the same limit explicitly in domain fixtures.

Report capacity/halt reasons in the existing derived blockers/advance response. Keep builds
independent of publication capacity. Map-order selection remains within a full capacity
window; no priority engine is added and no fairness bound is claimed.

The alternative P2 halts **future** admission if persisted publisher job evidence is completed
but named push outcome is neither success nor skipped. Already admitted jobs continue. Lost
job completion remains unknown rather than proof of completion. This is weaker than the old
full gate; it requires owner selection, not a hidden defensive default.

Eight is an operating recommendation. Under the controlled worker, at most eight controller
publication jobs and the existing status workflow's active job can contend. External/manual
writers or old unrelated runs invalidate that accounting. All writers must use the same
group; a checked fast-forward push still rejects conflicts. Queue overflow/cancellation does
not establish no push. GitHub documents 100 pending with queue:max and ordering by lock-wait
arrival, not dispatch order.
[Concurrency semantics](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency).

### Workflow/data boundary

Remove workflow-wide publication concurrency; put the same feed group, queue:max and
cancel-in-progress:false on `publish`. Keep the current named Publish job and named
`Publish checked feed update` step for evidence matching. `validate` runs independently;
its existing artifact ID handoff/provenance checks remain.

Within publish, current feed checkout through prepare/index/sign/render/commit/push stays
locked. Prepare also computes two job outputs before push: `receipt-sha256` (64 lowercase
hex) and `feed-revision` (40 lowercase hex of local HEAD). Validate receipt regular path,
existing size bound, assignment/build/run identity and staged bytes first. A retained identical
receipt follows current no-rewrite/no-resigning behavior. No output is a private key/token.

Push performs current trusted invocation checks, then token mint and checked push. Remove
only its HTTPS wait, retaining uncertain failure semantics from the step boundary. Use the
prepared SHA as context, never as an assertion that the push succeeded.

Add `confirm` outside feed concurrency, `needs: publish`, a 15-minute job timeout, explicit
contents:read-only permissions, no protected environment and no signing/writer/observer
secrets. Give it pinned tooling checkout and public config/assignment plus
the two mandatory job outputs. It validates executing actor/event/attempt 1 and trusted
deployment before HTTP; it must not run publisher identity API reads or private-key branches.
Its name must not match Publish. No `always`, continue-on-error, ignored push error, conditional
push skip or extra pushed flag is permitted.

Refactor existing Python phase control so confirm is handled before key selection/loading.
It derives the receipt URL from the validated assignment, validates both output formats,
and uses the existing 600 s / 10 s HTTPS wait with its existing body bound, origin/redirect rule
and exact-byte expectation expressed as SHA-256. The controller independently checks actual
committed receipt and HTTPS bytes; confirm is a normal-time liveness event source only.

Job outputs can be missing/redacted; treat missing or malformed outputs as failure, not
permission to skip proof. GitHub propagates failed/skipped prerequisites unless conditions
override them; output values are evaluated at job end. These public outputs are tiny relative
to documented output bounds.
[Job dependencies and outputs](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax).

```mermaid
sequenceDiagram
    participant C as Controller
    participant D as MariaDB
    participant V as Validate jobs
    participant P as Publish job, feed lock
    participant H as Confirm job, no feed lock
    participant F as Feed and Pages
    C->>D: Commit independent publication assignments
    D-->>C: Applied graph and generation
    C->>V: Dispatch independent workflows within capacity
    V->>P: Exact validated artifact IDs
    P->>F: Read current head; add/index/sign/commit/checked push
    P-->>H: Successful prerequisite plus bounded hash/SHA
    Note over P: Feed lock released
    H->>F: Wait for exact public receipt bytes
    H-->>C: Standard run completion webhook
    C->>F: Independent Git receipt and HTTPS verification
    C->>D: Commit published; derive dependent eligibility
```

Run conclusion now includes confirmation; publisher job conclusion remains distinct. Existing
complete waits for terminal run plus publisher evidence. A failed confirm after successful
push still invokes FeedProof; failure to verify remains assigned. A skipped push can still
fail definitively without feed reads. Cancellation after push, missing outputs or lost terminal
delivery can leave uncertainty requiring existing reconciliation. Never mark failed merely
because confirm failed, and never replay the push to test it.

Recommendation is confirm rather than Pages-event integration: it reuses the assigned run
and requires no new controller event path or feed App grant. It adds unmeasured runner startup
and retains GitHub-side waiting. Reviewer preference is not implementation approval.

### What remains serialized and why it is not hidden

The minimal C job still holds concurrency during its runner/tool installation and SDK
preparation. Status does too. Therefore A+B+C do not achieve literal maximal GitHub setup
parallelism. They remove the controller gate, independent validation and HTTPS wait from the
long critical path without introducing a tool-artifact trust protocol by default.

An additional setup/sign split has two concrete choices to measure before requesting it:

| Choice | Exact boundary and unresolved cost |
| --- | --- |
| Trusted prepare/sign job before locked writer | It may sign only this execution's RPM/APK packages and prepare the independently verified SDK. Transfer only bound, bounded outputs from that trusted job, never source-supplied executables/key authority. Locked writer must validate current retained receipt first and skip/discard an identical replay without re-signing published bytes, verify signed packages and tool/SDK compatibility, then index/sign current retained feed. Host dependencies and key import for index signing still recur. SDK/tool artifact transfers and repeat setup may cost more than they save. |
| Deployment-owned prepared runner environment | Standard tools can be present before job execution, avoiding repeated install. This requires a separately reviewed immutable environment and maintenance/hosting policy; it is not a source-selected build image or an implicit new service. |

Neither is a default addition. A third alternative is to accept measured job-granularity
setup overhead after C. Recommend measuring first; choose a further stage only if its net
critical-path saving justifies the trust boundary and extra code. Feed-dependent render/
index/sign/commit remains inside exclusion in every option. No claim that Pages coalescing
bounds visibility time follows from any split.

## Trust upgrade preserving historical explicit retry

Current assignments store exact source/publication workflow paths, refs/blobs and worker
tooling SHA. Dispatch uses those stored values. Source thin workflows call a pinned reusable
workflow. Publisher trusted_deployment compares selected tooling, environment pin and checkout.
Changing the one global CORE_CI_TOOLING_SHA or replacing old main workflow bytes breaks old
selection dispatch. Main-only protected environment policy does not solve that.

Recommend bounded retention of **two reviewed tooling generations**, through deployment-owned
workflow paths, not a controller compatibility implementation:

1. Retain old source/publisher/status paths byte-for-byte and keep the existing repository
   CORE_CI_TOOLING_SHA variable at O. Existing workflows continue to check out O.
2. Commit new tooling N first, including required scripts and the reusable build workflow.
   This separates tooling commit creation from embedding that commit in a later workflow;
   do not create an impossible self-referential commit hash.
3. Add a new publisher path and new status path on worker main, both with a trusted workflow
   literal CORE_CI_TOOLING_SHA=N and checkout of that exact env pin. Keep old files untouched.
   New workflow blobs are approved after this separate commit W.
4. Add a new thin source-workflow path to each sandbox source repo, rendering the existing
   template with N. Keep its old path/bytes on the configured editing branch for historical
   retries. New selections use the new configured source path/blob; source checkout remains
   the exact selected source commit, independent of workflow ref.
5. Configure future selections with N and the new approved paths/blobs. Persisted old inputs
   remain unchanged. Both generations use the same feed group/keys/origin and main-only
   publication environment. No extra secret grant is required by path retention itself.

These aliases are deployment-owned examples, not hardcoded controller names:
`core-ci-publish-parallel.yml`, `core-ci-status-parallel.yml`, `core-ci-build-parallel.yml`.
The canonical templates become the new generation; old approved deployed wrappers remain
as a bounded retention exception requiring owner approval. Retire them only when the owner
explicitly revokes that historical dispatch capability or selects another retry policy.
Do not pretend that deleting them preserves exact historical retry. No silent pin rewrite,
global arbitrary-SHA allowlist or source-controlled privilege is introduced.

| Controller / selected inputs / worker | Source-derived compatibility and required proof |
| --- | --- |
| Old controller, O assignments, O workflows | Existing baseline; unchanged |
| New controller A/B, O assignments, O workflows | Schema/evidence unchanged; test restore/reconcile. Publication remains serialized by old outer workflow lock |
| New controller C, O assignments, O workflows | Up to configured capacity may be assigned; old outer feed group still protects writes. Test mixed old/new lock groups before activation |
| New controller C, N assignments, new workflows | Intended path; test confirm helper ignored, run/job conclusions distinct and independent publication proof |
| Either controller, O assignments, overwritten old blobs/global pin N | Rejected by existing approval/trusted-deployment checks; prohibited rollout |
| Old controller rollback with multiple publishing nodes per feed | Restore rejects them. Cannot roll back until drained to its supported graph shape; never delete assignments to force rollback |

Stage rollout: prepare/review changes; deploy new controller initially with `--publication-limit 1`
and old pins; install N/W and new source aliases without touching O; verify exact bytes/pins;
then restart with new selection configuration and approved capacity. Snapshot assignments and
database backup before restart. Mixed assignments require the compatibility tests above; a
quiet session boundary reduces risk but is not a substitute for those tests. Existing sandbox
authority covers later concrete activation; this design does not execute any stage.

## Failure, overlap and restart timelines

```mermaid
sequenceDiagram
    participant E as Reports / HTTP results
    participant Q as Existing Database FIFO
    participant G as GraphStore
    participant X as Execution continuation
    participant S as Derivative status / SSE
    E->>Q: Immutable evidence facts A
    E->>Q: Immutable built result B
    Q->>G: Derive A from applied graph; SQL commit
    G->>G: Apply A; increment generation
    G->>X: Continue A without waiting for HTTP
    G->>S: Notify A
    Q->>G: Derive B including A; SQL commit
    G->>G: Apply B; increment generation
    G->>X: Authorize/dispatch eligible work
    G->>S: Notify B
    Note over Q,G: No stale Node retry or second queue
```

| Event | Result |
| --- | --- |
| Duplicate signed progress | Current merge/no-op; no generation, evidence HTTP or derivative refresh |
| Completion overlaps an artifact/proof read | Existing completionReads coalesces; immutable facts survive; lazy result checks current attempt before application |
| Unrelated evidence commit during selection | Original generation guard still rejects selection; explicit rescan/redelivery remains necessary. This limitation is not fixed by a fake epoch |
| Producer error or definite SQL rollback | Visible error, graph unchanged, next operation can proceed; no network mutation replay |
| Uncertain database commit | Existing write latch blocks remaining producers; do not dispatch from unconfirmed authority |
| Possible feed push with missing receipt/public bytes | Node stays publishing; P1 allows other slots, P2 only halts future admission under its specified predicate |
| Restored assignments with inaccessible GitHub evidence | Bounded attempt-one reconciliation; no replacement dispatch |
| Crash after early tag ACK but before persistence | Existing accepted exception: explicit rescan/redelivery; no new durable inbox |
| Live progress committed but public page old | Separate controller facts, status job and Pages propagation timestamps; never call this proof of a stale controller without checking its graph |

## Request accounting and measurement

Transport overlap adds no evidence reads by itself. Preserve exact-scope token waiting lists,
immutable proof/read coalescing and completion exclusion across concurrent callbacks.

| Work | Warm normal requests | Additional cases |
| --- | --- | --- |
| Unchanged bound run/job report | 0 | Invalid/unrelated/attempt 2 adds no proof or status work |
| Meaningful known progress | 0 execution-evidence reads | Derivative status cycle below still costs real requests |
| Successful bound build completion | 1 artifact-list GET | Missing proof/recovery may need attempt-one run/job evidence; cold scope token adds a mint |
| Successful publication verification | 6 GitHub GET +1 public HTTPS | Existing public-read token path may add a cold mint; inaccessible evidence preserves assignment |
| Warm dispatch | Ref GET + dispatch POST; immediate inspect only for a returned run ID | Shared identical preflights coalesce; cold commit/tree/blob proof and token add their actual reads |
| Status cycle | Ref GET + dispatch POST | Cold approval/token adds work; newer generations may require another cycle; worker GETs facts and pushes feed separately |
| Source selection | Existing repository/tag/definition chains per enrolled repository | Parallelize chains, not their dependent steps; report total including failures |
| Restart | Attempt-one inspection per assigned run, plus missing job/proof/result work | Unbound search is bounded existing behavior; never dispatch a duplicate for inaccessible assignment |

Worker requests, including publisher run identity, validation downloads, confirmation HTTPS
polls, status facts, Git push and Pages jobs, must be counted separately from controller REST.
Moving polling outside a lock is not eliminating those requests. For confirmation the existing
deadline/interval plus request time bounds polling; report actual attempts, not an invented
fixed count. No production instrumentation or automatic pacing policy is added by default.

Recorded baselines: the earlier obsolete full-inspection path's unchanged eight-report burst
was 112 requests/40 Git objects and 4.515 s at synthetic 30 ms latency. Current request-reduction
behavior already targets zero for that burst; do not claim its removal again as this change's
benefit. The later live sample's 119–160 ms graph GET/low mean CPU was not eligibility latency.
Use both facts with their provenance, not as new runtime measurements.

Proposed local acceptance targets on the fixed 20-node/5-target synthetic fixture: p95 below 10 ms
for pure eligibility/merge work; p95 below 50 ms and no sample over 250 ms from last required
local evidence application to durable follow-up assignment with isolated MariaDB. Exclude
remote API/Pages time and report DB queue separately. For the 140-record history fixture,
report the same distribution and identify scaling cost rather than relaxing it silently.
These thresholds require owner acceptance and measurement; a failed target triggers analysis,
not removal of identity/signature checks or a fabricated pass.

## Focused verification to implement later

| Existing harness/boundary | Required checks |
| --- | --- |
| Graph / GraphStore + isolated MariaDB | Overlapping evidence/built/published/advance, large synchronous no-op/rejection drains with bounded stack, enqueue from callbacks, START/SQL/rollback/uncertain commit, all finish paths once; startup read failure destroys wrappers safely; current guard/error classifications; no producer after uncertainty |
| `tests/pipeline.py` | Durable authority before every dispatch, multiple independent publishers within capacity, join waits all, report orders converge without another event, unchanged burst 0 requests, latest status under sustained commits, current graph/SSE before remote dispatch finishes |
| HTTP through pipeline synthetic HTTPS | Reverse response order, multi-address establishment and late orphan callback, connect/TLS/parse/drop errors, synchronous disabled connect/termination, capacity 1/8, origin drain, callback-enqueued request, scope/expiry coalescing, shutdown/restart without UAF or resend |
| Enrollment/SSE consumers | Bootstrap/reconnect includes repositories, successful save/import emits list, execution-only commit emits no list read, failed save preserves accurate GitHub feedback and explicit guards |
| Graph admission/restore | Batch capacity uses one calculation, lowered limit does not reject restored assignments, visible blockers, P1/P2 chosen semantics and uncertain slots retained |
| Worker harnesses | Validate outside mutex; publish/status share group; exact mandatory public outputs; confirm has no secret/private-key/API-identity path; attempt 2 denied; skipped push versus possible push; retained replay produces no new bytes/signatures |
| Trust upgrade | O/N source paths/tooling/blobs, mixed runs and environment checks, old explicit retry, returned evidence names/schema, rollback rejection when old restore cannot handle graph |

Do not weaken the existing publication-count assertion silently. Replace its old-policy
expectation only in the approved C change, while retaining its durable-dispatch assertion.
Relevant formatting/build and these affected tests run per checkpoint; no unrelated full suite.

Later sandbox acceptance uses a fresh explicitly requested core-up session, not retries of
uncertain nodes. Before restart, record assignments and backups. Observe delivered webhook,
graph generation/SSE, dispatch creation, runner start, publisher lock interval, confirm and
Pages timestamps separately. Demonstrate validation/build/proof overlap, exclusive actual
feed writes including status, exact source/prerequisites and no replacement on inaccessible
evidence. Compare graph/UI8080, controller public facts, committed status and public Pages.
Verify permitted feed diff/receipt/HTTPS and identical replay; preserve keys, tags, existing
package bytes and unrelated paths. Reuse relevant signed apt/dnf/opkg/apk client checks and
tamper rejection where affected; file existence is not installation proof. No production
SNodeC resources or port 443 changes.

## Checkpoints, estimates and approval

These are source-based planning estimates, not measured diffs. C++/Python/YAML count as
production. Checkpoints are coherent commits only after separate commit authority is given.

| Checkpoint in canonical core-ci | Production added / removed / net, central estimate | Test added / removed / net | Documentation added / removed / net | Requested upper bound for later code approval |
| --- | --- | --- | --- | --- |
| A: lazy database + callback/status/SSE boundary | 190 /110 /+80 | 250 /20 /+230 | 75 /30 /+45 | +90 production, +280 tests |
| B: bounded HTTP + source aggregation, after A | 195 /105 /+90 | 230 /35 /+195 | 60 /15 /+45 | +110 production, +250 tests |
| C: admission + publish/confirm + template pins, after A/B and policy choices | 150 /75 /+75 | 225 /30 /+195 | 140 /55 /+85 | +110 production, +260 tests |
| Total canonical implementation | 535 /290 /+245 | 705 /85 /+620 | 275 /100 /+175 | +310 production, +790 tests |

Reduction alone cannot correlate concurrent operations, defer derivation through SQL completion,
or provide the later HTTPS event. Reshaping removes eager retries, front-only coupling and
broad gates; the remaining net additions are per-operation correlation, producer outcome,
bounded capacity and secret-free confirmation. No second scheduler/authority offsets these
counts. If implementation cannot fit the approved checkpoint, re-estimate before proceeding.

Deployment retention has a separate visible cost: approximately 190 new publisher-workflow
lines and 70 status-workflow lines in the worker, plus about 29 thin-workflow lines per source
repository (116 for four). Old deployed paths are retained rather than edited. Worker script
mirrors also carry the corresponding canonical Python delta. Report actual deltas per repo
when installing; do not hide aliases as zero production change or add mirrored lines to a
canonical-source estimate twice. For a later bounded deployment approval, allow at most 450
net production lines across the sandbox worker/source repos, including those retained aliases
and mirrored script deltas; re-estimate before exceeding it. No external repo is edited
during this refinement.

Further privileged setup splitting is **not** in these bounds. It needs stage measurements,
the chosen handoff/environment design and its own estimate/approval. Current proposal/
WORK_LOG documentation is separate from production/test estimates.

| Owner decision | Recommendation | Alternative / consequence |
| --- | --- | --- |
| Admission after an unknown push | P1: independent writers within capacity; keep unknown node blocked | P2 partial future halt, which can reduce availability and cannot stop admitted jobs |
| Capacity | HTTP 8 per client (config 1–16), publication 8 per feed (config 1–32) | Lower limits reduce overlap; larger limits need measured pressure/accounting |
| HTTPS liveness | Secret-free confirm job on the assigned run | Pages-event path needs extra grant/handler and its own estimate |
| Historical trust | Retain exactly O/N approved deployed paths/pins initially | New-selection-only recovery or retirement changes explicit historical retry semantics and needs owner policy |
| Additional privileged setup | Measure residual locked setup after C before choosing a split | Extra trusted job/tool artifact or prepared environment requires a new reviewed trust/maintenance boundary |
| Implementation scope/growth | Approve A/B/C individually or the stated bounded series after policy selection | Design remains unimplemented; earlier advisory estimates are not permission |

The source/API questions above are resolved far enough to implement focused tests; runtime
callback/TLS behavior and mixed-tooling compatibility are acceptance gates, not owner
questions. Narrowing selection/enrollment guards remains explicitly deferred. The already
accepted early-tag-ACK crash gap remains unchanged.

## Evidence and status of this refinement

- Read current authority, persistence, dispatch/evidence, presentation and worker consumers;
  retained the agreed review qualifications and existing lifecycle.
- Prior isolated real-MariaDB probe results remain available; no live causality inferred.
- Ran only an object-only public API compile in
  `/tmp/codex/build/core-ci/parallelism-refinement/`; no network/server/database activity.
  Configure/build used installed public snodec package targets. No runtime concurrency or
  proposed production behavior has been tested by that compile.
- Verified official REST limits, job dependency/output and concurrency documentation.
  Custom Actions Pages publishing is exempt from the branch-based ten-build/hour soft limit;
  this says nothing about current deployment/CDN latency.
  [Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits).
- Production changes 0; existing test changes 0. No commit/push/merge, restart, configuration
  change, GitHub mutation, live test or deployment. Private directories and unrelated dirty
  work preserved. Current documentation remains uncommitted on the existing branch.

Successful diagnostic commands (the second includes the final termination-hook type check):

```sh
cmake -S /tmp/codex/build/core-ci/parallelism-refinement \
  -B /tmp/codex/build/core-ci/parallelism-refinement/build
TMPDIR=/tmp/codex/build/core-ci/parallelism-refinement cmake --build \
  /tmp/codex/build/core-ci/parallelism-refinement/build -j2
```

The temporary API sources/build remain for review; remove them after implementation tests
replace this compile-only evidence. Documentation whitespace, relative links and code fences
were checked; git diff --check passed. No production format/build suite was run because the
repository's production/test sources were unchanged.
