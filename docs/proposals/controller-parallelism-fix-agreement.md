# Controller parallelism: agreed correction plan

Source compared: `fix/controller-parallelism`, `63ab7b207f55afb5a71c7b2f54d76fa05ee7ebef`,
against base `4257cb0fc67412da8696afdba307fc2e85ff64aa`.

Codex and Claude compared every implementation finding against the
[approved proposal](controller-parallelism-design.md),
[joint architectural conclusions](controller-parallelism-review-agreement.md),
[current review](controller-parallelism-implementation-review.md), current source and focused
local probe evidence. The [discussion](controller-parallelism-fix-discussion.md) records
corrections from both reviewers. This is a correction plan, not completed fixes or acceptance.

Claude’s final acknowledgment: **“I have no remaining fix-direction disagreement.”**
Codex accepts the recorded clarifications. Runtime coverage, feasibility and owner acceptance
remain separate below.

No new architecture is proposed: keep one graph, the existing Database FIFO, current state
machine, bounded transport, standard authenticated reports and independent publication proof.
No new cache, flag, queue, timer, scheduler, retry protocol or controller thread.

## Every finding and its disposition

| Finding | Proposal requirement and actual implementation | Agreed correction/disposition |
| --- | --- | --- |
| M1: O/N instructions | Preserve old immutable workflow/tooling pins and historical retry. Already identified installation wording could overwrite O. No further documentation audit was performed. | Qualify first-install instructions; N tooling commit leaves O wrappers byte-identical; render N only into new aliases in a separate wrapper commit. |
| H1: disabled client | Normal response/error must complete once. Current status handling waits for flow termination which DISABLED does not cause. Installed-library probe returned callback count 0. | Existing status callback handles DISABLED: definite unsubmitted message and finish(operation). Keep intermediate ERROR/FATAL establishment handling; do not finish every non-OK status. |
| L1: phase arguments | Confirmation needs no feed checkout/artifacts, but preparation and push must receive what they actually consume. Empty feed currently becomes current directory; mocked push boundary reproduced this. | Immediately after parsing, require feed for prepare/push and build-result/artifacts for prepare. Validate before assignment processing, identity, key handling or network. Do not require unused push inputs. |
| L2: callback clearing | Empty stored callback is the existing delivery marker. Move alone gives a valid/unspecified source by the C++ standard; installed libstdc++ does empty it. | Replace move with std::exchange(callback, nullptr). No new delivered flag or test mirroring this one line. Existing callback-count tests remain. |
| L3: shutdown text | Known queued unsubmitted work must not be described as an unknown remote outcome. submitted=false already stays correct. | Set the existing unsubmitted-shutdown message before finish for unstarted queued work. Do not relabel submitted requests. |
| L4(a): response classification | Lazy derivation must retain prior caller error classification and relevant preservation context. No broken UI consumer was found, but some metadata/classifications changed. | Existing PipelineResult preparation-failure argument to change(), default 409; restore per-site responses below. Do not invent a no-op execution-ID contract. |
| L4(b): latched no-ops/SSE | Mutation uncertainty blocks producers before derivation. Thus an otherwise unchanged report or no-work advance now reaches unavailable-authority handling. | Keep internal 503/uncertain:true and failed-mutation SSE without SQL, dispatch or evidence traffic. No eager duplicate path/cache. Record the observable difference; do not claim byte-identical response behavior or an owner waiver. External authenticated webhook ACK already remains 202. |
| L4(c): built blockers | A verified built node with free publication capacity is eligible for publishing. The former generic 'attempt is building' text was not a prerequisite blocker. | No change. beginBuild is used only for waiting work. |
| T1: coverage | Some proposed checks were not run. Existing successful rollback, concurrent report/MariaDB-lock, GraphStore overlap and callback-enqueued HTTP work do have meaningful coverage. | Add focused reachable-case tests below; distinguish unrun defensive checks and environment limits from covered behavior. No full suite or production injection machinery. |
| I1: read callback order | Existing release-before-read-callback order allows startup teardown to destroy Database safely. This order predates the change; no dependent consumer was found. | No change. Do not globally reorder reads to resemble writes. |

## H1 timing and ownership

The installed framework's FlowController::startFlow defers establishment to its own next tick.
DISABLED therefore normally arrives after client.connect returned and the application installed
its flow handle/hook. Codex's initial explanation that the post-connect missing-membership branch
cleans this real disabled path was incorrect and is withdrawn.

finish() removes the operation, delivers its callback and releases capacity; connector teardown
releases framework captures. There is no active transport. No new cancellation manager or
explicit promise that every disabled framework flow emits termination is needed. The membership
and pump guards retain once-only delivery/reentrant safety.

Cancellation without status during process shutdown is not a second demonstrated running-service
failure. The proposal explicitly allows undelivered callbacks across teardown and forbids calling
Pipeline after its lifetime. No destructor callback or new lifecycle mechanism is introduced.

## L4(a): concrete response preservation

Reshape only the existing private helper:

```cpp
void change(const std::function<Node(const Graph&)>& derive,
            const Callback& callback,
            const PipelineResult& preparationFailure = {409, {}});
```

The asynchronous completion captures preparationFailure **by value**. On preparation failure,
copy its body, set error and uncertain=result.uncertain (false), and use its status. Assigning
body["error"] converts the null default JSON body into an object. SQL failures retain 503 with
error/uncertain. There is no new error taxonomy or eager graph calculation.

| Site | Preparation response |
| --- | --- |
| record/bindRun | 409, actual error, assignment_preserved:true, uncertain:false |
| buildArtifacts missing/expired-result failure and built-result acceptance | 503, actual error, execution ID, assignment_preserved:true, uncertain:false |
| publicationResult published receipt application | 409, actual error, assignment_preserved:true, uncertain:false |
| rejectedDispatch branches, complete failed, before-push failed | Existing default 409; no invented former reachable classification |
| commitSelection/retry/advance | Existing 409/409/422 classifications stay |

record's unchanged response already includes its execution ID. Base change() had no comparable
no-op ID guarantee; its terminal continuation returned advance's body. No identifier restoration
is needed for that new no-op branch.

Remove the two obsolete dispatch try/catches around formerly eager rejectedDispatch derivation.
Retain catches that still validate JSON, identity, artifact metadata or current attempt state.

A response regression must distinguish the intended lazy failure from an earlier rejection:
hold the graph-revision lock while two authenticated run claims for one unbound assignment queue;
after release, assert bindRun's conflict message, the first assigned run retained and no replacement
dispatch. Inspect the logged internal outcome separately from the external webhook 202.

## Focused verification plan

- Existing transport harness: disabled instance returns exactly one callback, status 0 and
  submitted=false, with no token/server request; capacity 1 handles twelve correlated requests
  with one token and peak 1; invalid capacities 0/17 reject. Existing capacity-8/reverse-response,
  expiry, TLS identity and mutation non-replay tests stay.
- Where the existing publication fixture permits, disable feed-http and verify retained publishing
  assignment plus a second reconcile completing, proving completionReads was released. Shared-branch
  source proof is not a substitute falsely labelled as that runtime test.
- Existing option/lifecycle/pipeline harnesses: publication limits 0/33 reject, capacity 1 visibly
  blocks a second built node, and a definite terminal result frees capacity. Keep default-8 and
  restore-under-lower-limit checks.
- Worker provenance: malformed phase arguments fail before assignment/identity/secrets/network.
- Delayed approval while same-state graph evidence keeps committing: captured-minimum dispatch
  proceeds, then latest generation follows after settling; no duplicate unchanged-generation POST.
  Callback order is not evidence of remote job arrival or Pages order.
- Stalled TLS and response shutdown: clean exit/no crash, durable restoration and no replacement
  mutation after restart. Exercise a held connect phase if a real local fixture can do so. A normal
  exit alone does not prove absence of use-after-free; instrumented focused evidence is separate.
- Real MariaDB proxy: one-shot rewrites of targeted START/ROLLBACK COM_QUERY into invalid SQL,
  maintaining packet length/sequence and verifying interception count. START failure is definite,
  unlatched and permits a subsequent write. Failed rollback latches uncertainty and prevents further
  producers/mutations. The deliberately failed rollback leaves a transaction open; inspect safe
  in-memory graph facts/non-locking independent DB facts, close/restart to roll back, then restore
  permissions and check no revision leaked. Do not rely on DB-backed application reads while open.
- Preserve existing successful rollback, lost successful COMMIT acknowledgement, GraphStore
  evidence/built/advance ordering and real-MariaDB pipeline concurrent-report checks. Add cheap
  forced artifact-result overlap if the public fixture can hold it meaningfully; no production hook.

Callback-enqueued HTTP requests already occur when token completion calls send/http.request;
the capacity-1/twelve-request probe exercises that boundary. Do not add a duplicate test merely
for the same phrase.

## Checks not run and genuine evidence limits

Design-listed active-origin switching and late orphan-onConnect checks have not been run.
Current FeedProof requires selected.origin==configured.origin and configuration is fixed for the
process; the product does not drive an active cross-origin switch. Framework status/onConnect
adjacency excludes the orphan sequence in the traced path. These are source dispositions,
**not runtime coverage** or owner waivers. Initial feed origin setup is exercised.

The proposal's 'synchronous disabled connect' label is incorrect for this installed framework;
the disabled test must exercise the actual asynchronous delivery.

Multi-address establishment is a real reachable behavior, but tls::in uses AF_INET; a dual-stack
localhost test cannot establish it. An optional private user/mount namespace with a controlled
hosts file can supply multiple IPv4 candidates without global changes, if supported. Read actual
resolver ordering inside that namespace; do not assume first candidate. Otherwise record the
precise environment limitation and leave that evidence unclaimed. No framework/global DNS change.

Connect-stall control, namespace support, actual ROLLBACK wire text, feed-disabled fixture,
forced artifact overlap and sanitizer evidence remain test-feasibility work, not completed checks.

## Retained policies and later acceptance

- P1 independent publication remains bounded; unknown nodes keep assignments and block their
  dependents. Eight HTTP operations/client and eight publication assignments/feed stay approved.
- Old/new workflow-level/job-level feed-group exclusion requires real sandbox proof before mixed
  O/N rollout at capacity above one. Matching group strings alone is not that proof.
- Status churn, response-to-transport-close occupancy, graph/DB/SSE timing, runner setup and Pages
  propagation require separate measurements. No latch/debounce/timer is approved to hide a delay.
- Selection/enrollment generation guards remain deliberately unchanged. Higher concurrency can
  expose their already-known rejections; do not replace them with an unsound epoch.
- Signing/setup remains within the privileged feed lock. Further splitting stays outside scope.
- Performance targets and any outstanding owner acceptance remain distinct from reviewer agreement.

No fix, new test, commit, push, merge, restart or activation was performed in this discussion.
The prior bounded implementation approval is not expanded by this agreement. The remaining fixes
and focused tests must be accounted against its +310 production/+790 test limits (current net
+200/+282), with re-estimation before exceeding a bound or adding a mechanism. This record is
not a new implementation or deployment request to the owner.
