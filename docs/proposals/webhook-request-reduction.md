# Reducing GitHub requests in core-ci

This is an investigation and proposed correction, not documentation of deployed behavior.
The controller still runs the implementation measured below. The owner accepted using
standard authenticated GitHub webhooks for progress, with one artifact-list request after
successful build completion. No custom reporting callback is proposed.

## Governing invariant

An authenticated GitHub report is execution evidence, rather than merely a reason to fetch
that same evidence again. It must match the enrolled repository, controller-assigned run,
attempt one, controller actor, approved workflow and immutable execution inputs.

The persisted graph remains the only execution authority. Reports merge monotonically;
GitHub UI reruns, duplicates, delayed reports and conflicting identities cannot replace an
assigned execution, regress state or authorize another dispatch. A successful GitHub job
is not by itself proof of publication: the assigned receipt must exist in the feed commit
and at the configured public HTTPS location.

## What the real deliveries contain

The inspected Fedora run40374669998 supplies these facts in GitHub's signed payloads:

| Run report | Job report |
| --- | --- |
| Run ID and attempt | Run ID, attempt and job ID |
| Repository and head-repository IDs | Enclosing repository ID |
| Controller actor ID | Workflow commit SHA |
| Workflow path and commit SHA | Assigned job name |
| Dispatch event and assignment title | Job status and conclusion |
| Run status and conclusion | Start/end times and step results |

The raw-body HMAC authenticates the payload as a GitHub delivery. A job payload does not
independently establish the controller actor or approved workflow blob. Those must already
be bound to the run, or the missing proof must be obtained before accepting the job facts.
The run payload contains an artifacts URL, not the assigned artifact's ID, digest and size.

Twelve identity/timing comparisons against the persisted real Fedora execution matched.
This is an offline comparison of real evidence, not a test of a changed controller.

## Request accounting in the current implementation

For a three-component workflow path, each `Pipeline::inspect` performs seven REST calls:

1. Read the attempt-one run.
2. Read its workflow commit again.
3. Read the root Git tree.
4. Read the `.github` tree.
5. Read the `workflows` tree.
6. Download and decode the workflow blob.
7. Read attempt-one jobs.

Each REST call first mints its own installation token. This makes14 HTTP requests per
inspection. A successful build then adds an artifact-list call and another token request.
The workflow blob is downloaded even though these readers only use its already approved
hash, and the complete path is walked again even after that immutable run identity is bound.

Eight concurrent notifications for the same unchanged run produced112 HTTP requests,
including40 Git-object reads. With30ms synthetic API response latency, they took4.515s to
drain. The graph generation did not change. This test uses a real isolated MariaDB and the
real controller TLS boundary with a synthetic GitHub server; it is not live acceptance.

A status refresh separately performs six approval reads and one dispatch. Its14 HTTP
requests share the same serial client with execution verification. Identical graph changes
are already suppressed by GraphStore, but this happens after the unnecessary evidence reads.
The current matcher also accepts unrelated helper-job reports once their run ID is known.

Publication verification has seven GitHub reads plus one public HTTPS receipt read. With
fresh tokens, a successful verification costs15 HTTP requests. When a new receipt is absent,
the lookup normally stops after repository metadata and receipt-commit history: four HTTP
requests. The failure classifier currently performs even that lookup before acting on
trusted evidence that the push step was skipped.

The live session opened1,914 GitHub HTTP connections between18:04:14 and18:30Z. Sampled
GitHub token/run requests took0.414s/0.402s, while local graph responses took25–33ms. This
supports removing duplicated serialized work; it does not justify adding controller threads.

## Reduction of the reporting path

### Apply authenticated reports directly

Both webhook reports and REST reconciliation results should feed one shared validation and
graph-application path. Extract the current identity validation and graph transformation;
do not implement a second webhook state machine alongside the existing REST implementation.

For an already bound run, queued/running/job-completion notifications can update graph
facts without REST calls. A duplicate or irrelevant report can finish locally. A changed
public projection can still require a status workflow dispatch; zero evidence reads does
not mean that derivative publication is free.

Run conclusion must be kept distinct from job conclusion. A successful package job can
belong to a failed workflow, and a failed validation job can leave the publisher skipped.
The existing evidence JSON can retain the run conclusion and the publisher push-step result
needed when notifications arrive in another order. These are additional recorded facts,
not new execution states or another authority; they still require implementation review.

Only completed job reports can supply terminal step results. Accept only the assigned
package/publisher job, not every job belonging to the same run. Ignore attempts two and
later as new evidence; REST recovery continues to inspect immutable attempt one.

### Fetch only facts that are absent

A report arriving before the run is bound needs the missing actor/workflow proof once.
A completed run arriving without its required job evidence can need one jobs-list read.
Neither situation justifies fetching every run, tree, blob and job again on later progress.
Do not silently drop a completion merely because an earlier verification is unfinished.

After successful build completion, retain the agreed one artifact-list request. Select the
assigned name and verify ID, run binding, digest, size and expiry before recording built
work. The trusted publisher continues its own provenance and package validation.

Transient suppression may be needed around overlapping artifact/receipt reads. Its only
purpose would be to prevent two reads of the same missing proof while the first is active.
Report facts must first converge in the existing graph; no persisted notification inbox,
ready queue, timer, automatic mutation retry or additional scheduler is required.

### Classify definite pre-push failure first

If authenticated evidence from the approved attempt-one publisher proves the privileged
job or push step was skipped, classify the attempt as failed before looking for a receipt.
This reorders the existing certainty rule rather than adding a new failure policy.

A started/failed push step remains uncertain. Continue verifying the actual feed; never
infer failure from a missing receipt or dispatch a replacement to resolve uncertainty.

## Reduction around dispatch and workflow approval

`Pipeline::workflow` resolves the ref with the REST commit endpoint, then asks SourceReader
to read that Git commit again. The first response already supplies the root tree identity.
Forwarding that existing proof can remove the second commit lookup.

The dispatch preflight also already proves an immutable workflow commit/blob. If the
assigned run uses that same commit, reuse this proof rather than walking the workflow again.
A branch move to another commit requires checking the actual executed workflow, even if the
configured branch name stayed the same. The preflight cannot prove which commit a later
branch-based dispatch executes; its result must be compared with the run's commit.

Workflow approval needs the regular-file mode and approved blob hash, not blob contents.
The existing file lookup should support that distinction without duplicating path validation.
Keep content decoding/hash verification for definitions, editing previews and feed receipts.
Do not replace regular-file/path checks with an unexamined Contents API shortcut.

Targets dispatched together from the same repository/ref/workflow share the same preflight
question. It can be answered once for that batch, while each run remains independently
bound to its actual executed commit. This is request sharing, not another work scheduler.

## Authentication and public refresh

A fresh installation token for every REST call is unnecessary. GitHub returns an expiry and
documents a one-hour lifetime. Exact-scope reuse must stay in the GitHub authentication
boundary, remain server-memory only, and preserve repository/permission restrictions.
Expired or revoked tokens must not cause a mutation to be replayed automatically. Cold
concurrent requests must be measured too; a token cache that mints several identical tokens
before its first response arrives has not removed all duplication.

Public refresh should publish the newest committed generation. Intermediate generations
may become obsolete while an earlier refresh is being prepared. Combining those requests
needs at most transient in-flight coordination against the existing generation, not another
snapshot authority. A retained approval is proof only for its exact repository/path/commit/
blob; a mutable branch name alone is not an approval cache key.

The worker currently mints a writer token and attempts a checked push even when an older
refresh fetches the same facts already published by another refresh. Avoiding duplicate
refresh dispatches at the controller removes the larger source of wasted work. The remaining
no-change push is a separate worker reduction, not an authentication failure.

The management UI can reflect a committed graph change through its existing SSE stream.
GitHub status publication additionally waits for its job and Pages deployment. Those
propagation delays must be measured separately from controller processing and cannot be
promised away by reducing REST calls.

## Honest budgets and remaining work

These are proposed normal-case evidence budgets, not measured results of an implementation:

| Operation, after immutable run binding | Evidence network work |
| --- | --- |
| Duplicate, older or unrelated notification | None |
| Queued/running update | None |
| Definitive failed build | None |
| Definitive pre-push publication failure, with recorded step proof | None |
| Successful build, complete job evidence present | One artifact-list REST request |
| Successful publication, existing bounded file-verification method | Seven REST reads plus one public HTTPS read |

An exact-scope token reused for the publication chain makes its normal cost eight HTTP
requests, or nine including one token mint. Missing job evidence, first run binding and
restart recovery are separate cold cases and must have explicit measured budgets. Reusing
a known commit tree can remove a further redundant Git commit read in feed verification.

A real graph change can also cause a status dispatch, and verified publication can release
new dependent jobs. Those are necessary new operations. Count them explicitly rather than
hiding them in completion callbacks. A whole graph with20 nodes cannot have fewer than ten
total requests: each build and publication needs its own dispatch.

Required focused checks before a production claim:

- A valid progress notification updates SSE/graph without evidence REST requests.
- An unchanged notification burst adds neither evidence reads nor status refreshes.
- Completion arriving during other processing is retained; only one assigned artifact read
  is active, and its result is checked against current graph state before committing.
- Invalid HMAC, actor/repository/workflow mismatches and attempt-two reports are rejected.
- Job-first, run-first and delayed progress orders converge without regressing facts.
- A skipped push avoids feed lookup; a possibly attempted push preserves uncertainty.
- Token expiry/scope and changed workflow refs cannot silently reuse inappropriate proof.
- Restart reconciliation retains assignments and uses attempt-one endpoints without
  redispatching uncertain mutations.
- Public refresh catches the newest generation without a new queue, timer or state authority.

The preceding55–85-line proposal is not an estimate for every reduction in this document.
Estimate the shared report-path refactor and its exact scope before editing production
code; obtain any production-growth and architectural approval required by AGENTS.md. No production
change, commit, push, restart or deployment has been performed for this investigation.

## Official references

- [Webhook signature validation](https://docs.github.com/en/webhooks/using-webhooks/validating-webhook-deliveries)
- [Run/job webhook events](https://docs.github.com/en/webhooks/webhook-events-and-payloads#workflow_run)
- [Artifact metadata](https://docs.github.com/en/rest/actions/artifacts#list-workflow-run-artifacts)
- [Attempt-specific jobs](https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt)
- [Workflow dispatch, including returned run ID](https://docs.github.com/en/rest/actions/workflows#create-a-workflow-dispatch-event)
- [Git tree modes and bounds](https://docs.github.com/en/rest/git/trees#get-a-tree)
- [Installation tokens and expiry](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-an-installation-access-token-for-a-github-app)
