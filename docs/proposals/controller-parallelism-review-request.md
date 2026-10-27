# Review request: restore parallel progress controlled by core-ci

## Owner's instruction

The owner requires the external controller to drive eligible GitHub work in parallel.
The current long serialized chain does not meet that intent. Investigate **core-ci's
architecture and scheduling**, rather than explaining delays as GitHub infrastructure.
Propose simplification first. This is investigation/design only: no production edits,
commits, pushes, deployment, API mutations or policy weakening are authorized by this review.

## Workspace and limits

Canonical checkout: /home/voc/projects/snodec/core-ci. Read AGENTS.md. Current branch is
`deploy/core-vchrist` at `4257cb0`, with unrelated uncommitted deployment/monitoring docs.
Preserve everything. Do not switch branches, create checkouts, run builds/tests, or write
files. Return your report as text. Do not read .local-credentials/, .qtcreator/, keys,
tokens, private configuration, home-directory logs or personal files. No SSH or GitHub
access is needed: use current controller source, its worker consumers and supplied evidence.
Do not recover discarded core-ci code or inspect old production SNodeC workflows.

## Verified evidence

Live server core.vchrist.at runs the tested binary containing controller changes through
47737f7. MariaDB and the graph were restored exactly. A fresh v2.1.2 sandbox core tag at
unchanged source commit 1cfcfa17de500bb68f389bf6b8c4cbf27ddcbb57 selected20 new nodes:
core → app/side → join, five target rows, revisions125–144. All work is in VOC-Experiments.
Independent targets should advance independently; join waits for every selected prerequisite.

At11:38UTC:10published,1publishing,4build-stage,5waiting. No new session failure observed.
One completed session already required explicit administrative recovery for a terminated
publisher's rejected Git push403; runtime had no operator resolution action. Cause of the
intermittent Git push403 is unresolved. A missing receipt alone must not clear uncertainty.

Measured examples (UTC):

- core APK r129 build11:06:51–11:09:15; publication validation11:11:17–11:13:11;
  metadata/provenance step98s; privileged publish job11:13:15–11:14:40;
  install tools20s, SDK/sign/index preparation15s, push/HTTPS starts11:13:54.
- core IPK r128 build started11:06:54, but privileged publication started11:29:16.
  Its completed build waited behind other feed publications.
- side IPK r138 build11:31:28–11:34:30; publication validation11:35:00–11:36:16
  (metadata step51s); privileged job11:36:21 onward, tools15s, preparation79s,
  push/HTTPS starts11:38:00.
- Completed privileged publisher jobs sampled42–153s. The table's timestamp is build start,
  not publisher start; do not misclassify total elapsed time as signing time.
- Server-local full graph GET119–160ms with140nodes, controller CPU117s over3109s wall
  (3.7%). These are NOT measurements of eligibility-handler or graph-commit latency.
- core APK workflow completed11:14:40; next app RPM run created11:14:53. This13s gap
  includes delivery, evidence, external HTTP and dispatch, not just C++ computation.
- Port8080 browser shows Live and changes via SSE without reload. Pages lag several graph
  generations/minutes while status refresh competes with publication; distinguish that lag.

## Source structures already traced (verify independently)

- Graph::publicationReady in src/Graph.cpp rejects any next publication for a feed if any
  node for that feed remains publishing.
- Pipeline::advance in src/Pipeline.cpp selects only one publication per invocation using
  publicationSelected. It commits dispatch authority then dispatches eligible builds.
- A publication state currently includes unprivileged validation, fresh runner/tool setup,
  SDK preparation, signing/indexing, feed push, public HTTPS propagation and controller proof.
- deployment/worker/.github/workflows/core-ci-publish.yml places validate and publish jobs
  under a workflow-wide same-feed concurrency group.
- The public status workflow shares the feed concurrency resource for its feed writes.
- validate.py and sign_openwrt.py each call sdk.prepare, which downloads/checks/extracts
  the SDK and performs defconfig/toolchain queries. Tooling setup is repeated on fresh runners.
- FeedProof and publicationResult perform controller-side publication verification before
  marking published. Trace what blocks release of the feed slot and dependent eligibility.
- Dispatch iterations follow the ordinary node map; assess head-of-line blocking/fairness
  and critical prerequisite progress without silently inventing a new owner priority policy.

## Questions to answer

1. Trace all serialized dependencies inside the controller: authoritative graph, event
   processing, database transaction queue, advance, dispatch preflight/proof, shared HTTP
   client requests, report application, FeedProof, status refresh and persistence. Identify
   exact code paths/lines and lifetime/ownership effects. The per-origin client refactor is
   deferred; do not prescribe it automatically.
2. For each boundary, state whether serialization protects an actual shared mutation,
   is required by exact prerequisite availability, or is an unnecessary consequence of
   combining distinct phases. Distinguish CPU work, asynchronous waits and policy gating.
3. Determine how to dispatch ALL eligible independent work promptly. Which validation,
   artifact preparation, SDK/tool acquisition and independent target work can overlap?
   What is the smallest actual feed critical section? Do not serialize independent reads
   merely because a later write uses the same feed.
4. Present concrete reduction/reshape alternatives. Prefer the existing graph and state
   machine with fewer gates. Do not add a generic engine, ready queue, second authority,
   controller threads, polling timer, blanket retries or custom workflow callback protocol.
5. Explain exactly how a proposed short critical section handles current feed checkout,
   indexes over retained packages, signing, concurrent status writers and checked push.
   Do not allow two jobs to prepare stale feed commits and then force/rebase over each other.
   Do not release an uncertain writer merely because GitHub failed or the receipt is absent.
6. Could independently verified immutable tooling be reused without accepting executable
   tools or key authority supplied by untrusted source artifacts? Assess duplication without
   removing required artifact/package/signature verification.
7. Does current report handling postpone authoritative graph updates or advance behind
   unnecessary network work? Propose existing-boundary measurements that separate receipt
   time, local eligibility/commit, dispatch initiation, remote run creation and Pages update.
   Millisecond local reaction is a requirement, not a measured fact yet.
8. Identify status-work contention and starvation separately from execution authority.
   Do not add another scheduler to solve derivative page refreshes.

## Guarantees to preserve

Exact selected sources/configurations/targets/prerequisites, immutable assignment identities,
GitHub App HMAC and assigned actor/workflow/attempt-one binding, artifact identity and
metadata validation, trusted keys and signed feeds, no different bytes under an existing
package identity, graph history, commit-before-dispatch, restart durability, and uncertainty.
A join waits for ALL prerequisites. Do not replace independent verification with a hook's
success claim. No automatic prerequisite builds, source fallback, mutation replay or changed
source/tag/retry/version policy. Compilation/signing/publication stay on GitHub.

## Requested report

- Verified findings with code references, ranked by impact; separate hypotheses.
- A concurrency/dependency map: what may run together and what genuinely must serialize.
- The smallest coherent reduction-first implementation plan and alternatives/tradeoffs.
- Explicitly identify required behavior changes, owner decisions, additional evidence fields
  or new states (prefer none), obsolete gates to delete, and exact consumers affected.
- Production/test added, removed and net estimates per checkpoint; growth/architecture
  approval remains necessary before implementation.
- Focused public-boundary tests and a live acceptance plan that proves independent jobs
  overlap, joins remain correct, uncertainty is preserved, and local reaction is measured.
- Do not promise total wall-clock speedup without measurement. Identify the expected critical
  path and what observations would refute your diagnosis.
