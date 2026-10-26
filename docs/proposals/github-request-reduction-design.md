# GitHub request reduction: implementation design checkpoint

Status: the owner approved the revised ceilings of 350 net production lines and
260 net test lines. Implementation and focused local verification are complete.
Nothing from this fix has been committed, pushed, installed or deployed. Real sandbox
progress/publication acceptance is not claimed.

Checkout: `/home/voc/projects/snodec/core-ci`. New branch: `fix/github-request-reduction`,
based on main23f881f4419f243403ccaa9ef3cad890fdf8c383. Investigation/test edits are carried
forward intact. Cleanup checkpoint1a64901 remains on its separate branch. No open PR exists.

## Invariant and the code being replaced

Authenticated reports and REST recovery observations must enter one validation/application
path and converge in the existing persisted graph. Duplicate notifications must not repeat
immutable proofs or concurrent completion reads. Dispatch remains durably authorized before
its HTTP mutation; only verified feed publication satisfies a dependency.

Replace Pipeline::inspect's nested validation/application chain with shared run/job
observation and completion functions. Pipeline::event supplies authenticated payload facts;
inspect/reconcile supplies REST facts. Delete the unconditional event-to-inspect forwarding.
Keep one implementation of actor/repository/attempt/workflow validation, one graph evidence
merge and the existing graph transitions. GraphStore's existing equality and generation
checks remain in force.

GraphStore::view/publicView and the management SSE path continue reading that same graph.
No main sequence, listener, transport, database lifecycle, source-selection policy or worker
workflow changes are included.

## Concrete report flow

1. Validate raw-body HMAC, body shape, installation/repository, event/action, assignment,
   attempt one and the report's identity. Helper jobs must match the assigned job name as
   well as the run; matching its run ID alone is insufficient.
2. For a run report, validate the controller actor, dispatch event/title, repositories,
   workflow path and immutable commit. Reuse a matching persisted run identity or matching
   preflight proof. Otherwise verify the missing workflow proof once.
3. For a job report, require the correctly bound run/commit/attempt and assigned job. If the
   run is not bound yet, fetch attempt-one run metadata once and pass it through the same
   run validator, retaining the job report until that proof completes. Do not invent an actor.
4. Normalize the report into existing evidence fields. Add only `run_conclusion` and the
   terminal publisher `push_step_conclusion`; retain existing job conclusion separately.
   Record the step result only from a completed job of the approved publication workflow.
5. Merge against the current graph, preserving monotonic status, immutable identities and
   terminal outcomes. An unchanged observation needs no database mutation or status refresh.
   Valid missing terminal-history facts can still be recorded without reopening the attempt.
6. After commit, examine canonical merged facts, rather than a captured old run snapshot.
   A failed build can fail directly. A completed successful build needs the one artifact-list
   request. Publication uses definite skipped-push evidence first, otherwise real feed proof.

Acknowledgement remains prompt and separate from eventual processing. Report processing
retains callback ownership after the HTTP response; failures go to the controller log.
The accepted initial-tag crash window is unchanged.

## Ordering, overlap and restart

Both run-first and job-first order must converge. A queued report delivered after completion
cannot erase completion. If a report arrives while workflow proof is being read, its callback
waits for that same proof rather than being dropped or starting another path walk.

One transient completion-read marker per execution/attempt/phase prevents parallel artifact
or feed reads. It is not a ready queue: report facts are committed to the graph, and a read's
callback uses the current graph to decide whether it can still apply. Every success/error
path releases the marker. Changed attempts, terminal outcomes and late results cannot be
replaced by a stale captured Node. A missing proof after a failed read remains visible and
can be obtained by explicit reconciliation or a subsequent relevant report; no timer retries.

Restart restores the existing assignments/evidence. Transient data starts empty. Bound runs
need fresh attempt-one status/jobs only for recovery, using the same validators as webhooks.
Unbound assigned runs retain existing search/binding behavior. Inaccessible results do not
fail work or authorize a replacement. No SQL migration or new execution state is introduced.

## One file lookup and reusable workflow proof

Extend the existing SourceReader path walker to accept a known immutable root tree and to
return regular-file identity without downloading contents. Reuse its current path, mode,
symlink/submodule, hash and size checks. Do not add a separate path resolver. Definition,
editing-preview and receipt callers still read and verify actual contents.

The preflight REST commit response already supplies its root tree. Additionally, the real
signed Fedora run report contains `head_commit.id == head_sha` and `head_commit.tree_id`.
The latter was verified in the actual delivery for run40374669998, and the official REST
run example contains the same fields. Validate both SHA fields and their relationship before
using that tree. If it is absent, obtain the missing commit metadata; never guess a branch.

Workflow approval then normally needs the three existing directory-tree reads, with no
second commit fetch and no unused blob download. Reuse this proof when actual run commit,
repository, workflow path and approved blob match exactly. A changed branch/commit requires
new proof. Resolve mutable dispatch refs before dispatch; a cached branch name is not proof.

Retain the last verified immutable tuple per repository/workflow path, plus callbacks for
currently active exact-tuple lookups. Completed proof has no mutable execution state.
Concurrent requests for the same tuple share its lookup; different tuples never share proof.
In-flight entries disappear on completion/error. Only valid proofs update the retained tuple.
A single batch preflight can serve targets sharing the same repository/ref/path/blob;
each eventual run is still checked against its actual executed commit.

FeedProof can pass the root tree already present in receipt commit history to the same
lookup, removing its redundant Git commit request. Keep repository ID, branch/path commit,
regular receipt bytes and HTTPS equality checks.

## Exact-scope authentication

Replace fresh minting per call with one in-memory token entry per exact installation,
repository-ID set and requested permissions. Store returned expiry and share one active mint
among simultaneous requests for that exact scope. Pending callbacks are authentication
waiters, not graph work or another persisted queue. Every waiter is completed once on mint
success/failure, including shutdown. Do not broaden permissions to improve cache hits.

On expiry, mint before a new request. No expiry timer is needed. Authentication failure must
not replay an already submitted mutation. Report its original result and preserve existing
uncertainty rules. Tokens, JWTs, request bodies and authorization headers must not appear in
request-count diagnostics.

## Derivative status refresh

Keep one refresh in flight. Compare its captured generation with the existing graph generation;
intermediate generations can be superseded without creating a second latest-state record.
If approval completes for an obsolete generation, skip its dispatch and prepare the current
one. If graph state changes during dispatch, follow with the newer generation, never a retry
of the same uncertain dispatch. Release the in-flight flag on every path.

Use the shared workflow proof above. A refresh still resolves its mutable ref; when its
commit/proof is unchanged, only that ref read and the dispatch remain. Local SSE remains
independent of worker/Pages propagation. No renderer, public-template or worker push change
is included in this checkpoint.

## Transient data and the mechanism each replaces

| Data | Replacement |
| --- | --- |
| Exact-scope token, expiry and active-mint callbacks | One fresh token request before every REST call |
| Last verified workflow tuple and active-proof callbacks | Repeated path/commit/blob reads for identical immutable proof |
| Active completion-read marker | Concurrent duplicate artifact/feed reads before the first finishes |
| One status-refresh-in-flight flag; existing generation | Parallel refresh chains for obsolete intermediate generations |
| Local dispatch-batch grouping | Repeated preflight of identical workflow questions in one batch |

No notification-ID store, dirty-progress cache, execution queue, scheduler, thread, timer,
second graph or general-purpose event system is added. Existing HTTP/SQL operation queues
and SNode.C callback ownership remain. The HTTP-client/per-origin refactor stays deferred.

Reduction alone removes the unconditional repeated reads but does not retain out-of-order
completion facts, prevent overlapping missing-proof reads or handle token expiry correctly.
Those are why the listed bounded coordination/data additions are necessary. No additional
optimization is included solely to reach an arbitrary request counter.

## Proposed measured budgets

Counts include all outgoing HTTP requests, including token creation where specified. These
are design budgets for normal current sandbox paths, not implemented/verified results.

| Operation | Warm | Cold normal case |
| --- | ---: | ---: |
| Unchanged/older/unrelated report with no missing proof | 0 | 0 |
| Bound queued/running evidence, excluding a genuine public refresh | 0 | 0 |
| Successful build, job evidence present | 1 artifact read | 2 including token |
| Completed build needing job fallback | 2 reads | 3 including shared Actions-read token |
| Definite build/pre-push failure with complete evidence, excluding derivative work | 0 | 0 |
| Publication verification using known receipt root tree | 7, including public HTTPS | 8 including Contents-read token |
| Status refresh with unchanged workflow commit | 2 | 4 if both scoped tokens expired |
| First status refresh requiring workflow proof | 5 | 7 including both scoped tokens |
| Dispatch plus first run metadata binding, full new workflow proof | 6 | 9 including three exact-scope tokens |
| REST recovery of an already bound run, status and jobs | 2 | 3 including token |

Initial job-first binding, incomplete payloads and a changed dispatch ref require the missing
proofs. For example, a post-preflight branch move adds three tree reads for the actual commit.
A restart that lost all pre-binding knowledge can combine initial binding and publication
verification, exceeding ten requests (roughly14–15 in the current design). Count and justify
that cold recovery explicitly; never drop verification to manufacture a hard ceiling.

New dependent dispatches and real public refreshes are additional necessary operations.
The test report must include their combined total as well as per-operation categories.
Do not compare an old five-root failure session with a complete20-node run as equal work.
A few hundred full-session requests remains an estimate until real measurement.

## Reviewable changes, estimates and approval boundary

Planned production edits: Pipeline.cpp/.h, SourceReader.cpp/.h, GitHub.cpp/.h and FeedProof.cpp/.h.
Graph evidence merging and SQL/state model remain in place; no new schema or execution states.
Main, HTTP transport, worker workflows, source hooks and renderer remain unchanged.

Point estimates relative to main23f881f:

| Area | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| Shared reports/completion, workflow proof and refresh; Pipeline | 250 | 245 | +5 |
| Shared known-tree/content-versus-identity lookup; SourceReader | 60 | 35 | +25 |
| Exact-scope token lifetime and active mint; GitHub | 100 | 40 | +60 |
| Known receipt-root forwarding; FeedProof | 10 | 10 | 0 |
| Production total | 420 | 330 | +90 |
| Focused tests, including the carried33-line probe | 280 | 60 | +220 |

The initial estimate above was too low. The owner subsequently approved a revised ceiling
of 350 net production lines, with the test ceiling unchanged at 260. Actual production
changes are +514/-238 (net +276); build/test wiring is +11/-2 (net +9), so even counting
that wiring as production the result is +285. Tests are +250/-59 (net +191). No unrelated
deletion was used for LOC credit. Documentation is accounted separately.

Required tests are the accepted task's pipeline/graph/store boundary cases, with real isolated
MariaDB, synthetic GitHub and complete request counters. Update unrealistic webhook fixture
shape rather than weakening identity assertions. The unchanged eight-report burst must become
zero outgoing requests, including tokens/status work, with evidence it was actually handled.
Do not repeat unrelated suites. Formatting and relevant build targets validate the final diff.

No commit/push/merge, installation, live restart, retry, new live build or deployment has
occurred. Real publication acceptance remains blocked by the separately deferred renderer
defect. Existing failed/waiting sandbox history was not changed.

## References verified for this design

- https://docs.github.com/en/webhooks/using-webhooks/validating-webhook-deliveries
- https://docs.github.com/en/webhooks/webhook-events-and-payloads#workflow_run
- https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run-attempt
- https://docs.github.com/en/rest/actions/workflows#create-a-workflow-dispatch-event
- https://docs.github.com/en/rest/git/trees#get-a-tree
- https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-an-installation-access-token-for-a-github-app

SNode.C references read: src/apps/http/httpclient.cpp and model/clients.h; MQTTSuite
mqttintegrator/mqttintegrator.cpp startClient and its request/response callback ownership.
Use installed package interfaces and the existing one-thread event loop; no lifecycle change.
Safe additional evidence: /tmp/codex/build/core-ci/webhook-latency/webhook-tree-proof.json.


## Implemented verification result

The existing graph, state transitions, SQL schema, main lifecycle, local SSE transport and
worker workflows remain the authorities they were before this change. There is no new
persisted execution state or reporting protocol. The shared read coordinator coalesces
only simultaneous GETs with identical repository/path/permissions; mutations are never
coalesced or replayed. Workflow proofs additionally match repository/path/commit/blob.

Both webhooks and attempt-one REST observations now use `observe` and `record`. Run and
job conclusions remain separate. A run-based completion can obtain one missing job list;
a still-running response records progress and stops, rather than polling recursively.
Out-of-order facts merge against the current graph. A definite generation-guard rollback
recomputes that merge; an uncertain database outcome is not replayed. Completion reads
are shared while active and check the current attempt before applying artifact/feed results.
An unchanged notification does not repeat an earlier inaccessible artifact/feed lookup;
explicit reconciliation supplies the recovery operation.

Local results use isolated real MariaDB and synthetic GitHub/feed TLS endpoints, not real
GitHub executions:

| Check | Measured result |
| --- | --- |
| Eight unchanged notifications | 0 HTTP requests, 0 Git-object reads, 0.063 seconds |
| Previous synthetic burst baseline | 112 HTTP requests, 40 Git-object reads, 4.515 seconds |
| Graph/SSE progress with a 300 ms API response delay | 96 ms, no run/job evidence reads |
| Eight overlapping job-first notifications | One attempt-one run metadata request |
| Overlapping successful completions with job evidence | One artifact-list request, no job-list request |
| Warm publication receipt verification | Six GitHub reads plus one public HTTPS read |
| Expiring token / simultaneous cold requests | Expiry honored, shared mint, separate permission scopes |
| Lost mutation response | One mutation submission, no replay |

Request counters include every fixture GET, POST and PUT, including lost responses and
feed HTTPS. The final synthetic scenario used 329 HTTP requests: 25 token mints, 40 evidence
reads, 104 source reads/edits, 84 worker approval reads, eight build dispatches, three
publication dispatches, 45 status dispatches, 18 feed GitHub reads and two feed HTTPS reads.
Worker approvals include shared status/publication questions; they are counted once, not
attributed twice. This scenario includes enrollment edits, deliberate errors, eight build
assignments, explicit reconciliations and multiple restarts. It is not the earlier live
five-root failure session and is not a measured complete 20-node graph acceptance.

The 20 relevant transport/source/graph/enrollment/pipeline checks passed after fixture
corrections. `format-check`, `cmake-format --check CMakeLists.txt` and `git diff --check`
passed. Test artifacts and command logs are under
`/tmp/codex/build/core-ci/webhook-latency/`; the development binary is in its `build/`
subdirectory. The enrollment fixture now supplies the real API's token expiry and commit
root tree, and the pipeline fixtures deliver actual run/job event shapes.

No new run URLs exist for this change. Live delivery-to-commit, worker refresh and Pages
visibility timings still need sandbox measurement after authorized activation. The renderer
failure remains a separate gate to successful full-session publication. No production SNodeC
resource, private directory, key, tag or retained package byte was modified.
