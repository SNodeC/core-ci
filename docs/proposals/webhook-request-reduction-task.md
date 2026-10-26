# Implementation task: remove redundant GitHub requests and controller progress delay

## Authority and scope

Work in `/home/voc/projects/snodec/core-ci`. Follow AGENTS.md and the global engineering
instructions. This prompt records the accepted reporting boundary and investigation findings;
it does not grant net production-code growth, new mechanisms, commit, push or merge approval.
Do not treat numerical budgets as permission to weaken verification.

The owner accepted:

- Standard authenticated GitHub webhooks supply run/job progress.
- One artifact-list REST request after successful build completion supplies artifact identity.
- No custom workflow-to-controller reporting protocol or additional callback authentication.

The goal is to remove unnecessary work, not merely batch the existing full inspections.
Use one graph/state authority and one shared validation/application path for authenticated
webhook facts and REST reconciliation facts. Preserve the original product scope.

## Workspace and evidence checkpoint

Verify current state before editing; these values describe the investigation checkpoint:

- Main/base:23f881f4419f243403ccaa9ef3cad890fdf8c383.
- Current task branch:`fix/webhook-verification-latency`.
- Existing intended changes: WORK_LOG.md, a33-line burst probe in tests/pipeline.py,
  and docs/proposals/webhook-request-reduction.md. Preserve them.
- Local cleanup checkpoint1a64901 is on `chore/sandbox-cleanup`. Do not mix it into this task.
- Do not reset, rebase, switch another thread's branch or create another checkout/worktree.
- Preserve .qtcreator/ and .local-credentials/ without inspecting their contents.
- Temporary checks belong under /tmp/codex/build/core-ci/webhook-latency/.
- Evidence: docs/proposals/webhook-request-reduction.md and the corresponding WORK_LOG.md entries.

The running sandbox controller was last observed at PID1407899, using the3aa775d build.
Verify its identity rather than assuming that PID remains current. No controller fix from this
investigation is deployed. Do not confuse the current development binary with the checkout.

## Verified problem

The old path acknowledges a webhook, then reads the attempt-one run, five Git objects for
the workflow and attempt-one jobs. Each REST call mints its own installation token.
Eight unchanged notifications produce112 HTTP requests and40 Git-object reads. A local
synthetic test with30ms response latency took4.515s to drain, without changing graph generation.

The live session opened1,914 GitHub connections for five core builds and five failed
publication attempts. It did not complete all20 graph executions. GitHub's sampled completion
notifications arrived promptly; controller progress remained stale for minutes. Sampled direct
GitHub reads were about0.4s and local graph reads25–33ms. Preserve the distinction between
webhook delivery, evidence processing, graph commit, worker refresh and Pages deployment.

Source trace:

- Pipeline::event matches reports then unconditionally invokes inspect.
- Pipeline::inspect repeats immutable workflow proof even after run binding.
- Matching an assigned run ID also admits unrelated helper-job notifications.
- Pipeline::workflow resolves a commit then SourceReader fetches that commit again.
- Approval readers download workflow bytes when they only consume its approved blob hash.
- GraphStore suppresses identical commits, after the redundant network work.
- Every committed change calls refreshStatus, creating another approval/dispatch chain.
- publicationResult searches the feed before applying its existing skipped-push failure rule.
- GitHub::request mints an installation token for every API call.

## First checkpoint: design before production changes

Re-read the affected current code and focused tests. Trace ownership, callback lifetime,
persistence, presentation and all callers. Study relevant SNode.C and MQTTSuite client/lifecycle
patterns before any framework-facing change.

Present one concrete reduction-first design with:

1. The invariant and exact existing code to remove or reshape.
2. How webhook and REST facts share validation and graph application.
3. How first binding, out-of-order reports, overlapping completion reads and restart work.
4. How status refresh avoids redundant approval/dispatch work while reaching the latest generation.
5. Necessary transient authentication/proof/in-flight data, if any; explain each item and
   what repeated mechanism it replaces. Do not add every investigated optimization by default.
6. Request budgets for normal, cold, duplicate and recovery cases, including token requests
   and derivative status work. Explain any case that exceeds the normal budget.
7. Production/test added, removed and net-line estimates for the exact checkpoint.

Prefer neutral/reduced production lines. Obtain the explicit approval AGENTS.md requires
before net growth or an architectural extension. The earlier55–85-line estimate is not
approval and does not cover every idea in the investigation. Do not re-ask the accepted
standard-webhook/artifact-list choice or create a new blanket sandbox activation blocker.

## Required reporting behavior

Validate raw-body HMAC, event/action and payload shape, installation, enrolled repository,
assigned execution, attempt one, run identity, controller actor and approved workflow.
A source hook's claims are not authenticated GitHub evidence.

Known assigned-run progress should not cause evidence REST reads. A duplicate, older,
terminal or unrelated report should cause no reads or derivative refresh when it adds no
facts or required missing proof. Terminal history may still acquire valid missing evidence;
never regress its outcome. Do not rely on delivery IDs as a new durable deduplication authority.

A job report lacks the actor/workflow proof supplied by a run report. Accept its facts only
after that run is correctly bound, or fetch the missing proof once. A dispatch response's
run ID/URLs alone are insufficient full binding. Missing fields or changed identities must
not become permission to guess. Use actual attempt-one evidence, never the latest rerun view.

Keep run conclusion distinct from job conclusion. Merge progress monotonically into the
existing evidence JSON. Retain the terminal publisher push-step evidence needed for failure
certainty if reports arrive in another order. Explicitly account for any additional JSON
facts in the design; no new execution states or schema migration is planned.

After complete successful build evidence, make the agreed artifact-list request. Enforce
assigned name/run/job/attempt binding, digest, size, expiry and ambiguity/resource bounds.
Preserve the approved missing/expired-artifact failure policy. Prevent concurrent reports
from multiplying the same artifact read. Recheck the current graph/attempt when an async
read finishes; do not overwrite a newer or terminal result with a captured stale Node.

If an earlier read fails or evidence is inaccessible, retain the assignment/uncertainty.
Do not add a blanket retry policy. A lost completion during overlapping processing is not
an acceptable optimization. Use existing explicit reconciliation/restart behavior for recovery.

REST reconciliation remains necessary when reports are missing or after restart. Its facts
must enter the same validation/application path; no second REST state machine may remain.

## Workflow proof and dispatch

Keep the committed transition authorizing dispatch before its HTTP mutation. Never replay
an uncertain dispatch or substitute a replacement run.

Reuse the already verified preflight commit/blob when the actual run uses that commit.
A mutable ref can advance between checking and dispatch: compare actual run commit and
obtain new proof when necessary. A branch name alone is not an approval identity.

Remove duplicate commit reads where trusted response data already supplies its root tree.
Avoid downloading workflow contents merely to compare a blob SHA. Keep regular-file,
symlink/submodule, path, hash and resource-bound checks through one file-lookup implementation.
Definitions, editing previews and receipts still require their actual contents.

Consider shared preflight for identical workflow questions in one dispatch batch, without
introducing another scheduler. Reuse immutable proof rather than trusting old source selections
or changing GitHub JSON authority. Preserve all current source/tag/dependency policies,
including the F3 rule that an unreadable newly selected repository blocks the request.

## Failure certainty and feed verification

Apply the existing trusted skipped-push rule before FeedProof. A definitively skipped
privileged job/push step needs no receipt search to establish that it could not publish.

After any possible push, preserve uncertainty until real publication is verified. A missing
receipt, failed GitHub conclusion or empty listing is not proof that no push occurred.
Keep committed receipt identity and public HTTPS-byte checks before marking published.
Retain exact inputs, signatures, package identities, history and per-feed writer exclusion.

## Authentication and status refresh

If token reuse is included in the approved checkpoint, preserve the exact installation,
repository and permission scope. Keep tokens only in server memory, honor returned expiry,
and account for simultaneous cold requests. Do not broaden read scopes to write scopes or
replay a mutation after401/403. No token/key values may enter logs or request-count output.

Status refresh is derivative work, not execution authority. Combine obsolete generations
without a new persisted queue, timer or snapshot. The newest committed generation must
still be dispatched; an uncertain dispatch must not be blindly resubmitted. Bound any
retained workflow proof to exact repository/path/commit/blob and detect changed refs.

Count status approval and dispatch requests separately and in the session total. Do not
claim zero network cost for a real public refresh. GitHub job/Pages propagation can lag a
correct controller; report those timestamps separately. Keep the existing local SSE path.

## Non-goals

No custom callback protocol, generic workflow engine, ready queue, controller thread,
polling timer, automatic mutation retry, new persisted execution state, IdP/auth system,
registry/SDK client, package processing on the controller or unrelated UI redesign.
The shared HTTP-client/per-origin refactor stays deferred. Do not change SNode.C itself.
Do not alter production SNodeC workflows, credentials, feeds or tags.

The renderer's mixed missing/integer revision defect is separate and remains deferred in
this task. Do not bypass rendering or fabricate publication success to obtain live proof.
The settled session has five successful builds, five failed publications and15 waiting
nodes. Preserve them; never retry those failures silently.

## Focused tests and measurement

Use existing tests/pipeline.py and relevant graph/store harnesses. Run only affected tests;
no unrelated full suite. Keep tests public-boundary focused with real isolated MariaDB and
synthetic GitHub clearly labelled. Relevant existing tests must not be removed or weakened.

Cover:

- Valid signed progress reaches graph/SSE promptly without evidence API reads.
- The existing eight-notification unchanged burst produces zero HTTP requests, including
  token minting and status dispatches; verify the reports were actually handled.
- Completion during another read is retained and one required artifact lookup occurs.
- Run-first/job-first/duplicate/delayed orders converge; run/job conclusions remain distinct.
- Invalid HMAC, wrong actor/repository/workflow and rerun attempts cannot advance work.
- Exact proof reuse avoids repeat Git reads; changed commit/ref requires new proof.
- Skipped push needs no feed read; possible push preserves assignment and uncertainty.
- Included token reuse respects expiry/scope, cold concurrency and mutation non-replay.
- Latest public generation is eventually refreshed without duplicate unchanged refreshes.
- Restart retains assigned work, performs bounded attempt-one reconciliation and creates
  no replacement for inaccessible/uncertain executions.

Test-side request counters must include all outgoing HTTP requests. Classify evidence,
initial approval, token mint, dispatch, feed HTTPS and status refresh; also report the total.
Do not add production instrumentation/state merely to produce the measurement. Test clocks,
observers and synthetic server threads are not application threads or live acceptance.

Normal targets after binding are zero progress reads, one artifact-list request per successful
build, and approximately8–9 HTTP requests for successful publication verification with
correctly scoped token reuse. Genuine new dispatches and cold/recovery proofs are additional
and must be justified. A few hundred requests for the full20-node session is an estimate,
not verified evidence or permission to leave known duplication operating.

## Sandbox validation and handover

Honor existing VOC-Experiments activation authority for concrete in-scope deployment stages;
verify separate commit/push/merge authorization rather than inferring it. Before restart,
record current assignments and confirm durable restoration. Keep UI loopback and public
webhook TLS settings unchanged. Never touch port443.

Verify real progress and processing latency through GitHub delivery, graph/SSE commit,
worker refresh and Pages visibility. Do not equate a late completion log that also waits
for dependent dispatch with the exact database commit time.

A full graph run cannot prove publication until the separate renderer defect is resolved
within its own authorized scope. Report that gate honestly; continue independent local
and live progress checks. Do not claim a failed/partial graph session as complete acceptance.

Update WORK_LOG.md. Report source branch/commit, exact changed lines by production/test/docs,
focused commands and outcomes, baseline versus measured requests/latency, local versus live
proof with run URLs, deployment/restart status and remaining gates. Preserve unrelated work
and private directories. Keep one coherent change per authorized commit.
