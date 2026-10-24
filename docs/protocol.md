# Proposed protocols and trust boundaries

These are initial contracts for review, not deployed endpoints. Version the external worker protocol from its first implementation; do not invent compatibility modes for every old workflow shape. Legacy publisher utilities are refactored at a controlled sandbox cutover.

## Source and GitHub webhook ingress

`POST /api/v1/github/events` accepts only configured GitHub event types/actions and repository IDs. Authenticate raw bytes with the configured webhook secret before JSON interpretation. SNode.C express::Request exposes the raw body byte vector; install HMAC validation before JsonMiddleware, which itself parses the body. Deduplicate delivery IDs and semantic source events separately. A redelivery retains its delivery ID; a newly created event can refer to the same tag/commit without being the same notification.

For `push` events, require a nondeleted strict-semver tag and allowed source repository. Resolve annotated/lightweight tags to the exact commit and validate VERSION/definition through async GitHub access or the capture worker before activation/registration. Do not treat a mere ref difference as proof of tag-byte fidelity. Unsupported source events are rejected or recorded as ignored without product activation.

For `workflow_job` / `workflow_run` events, bind only known assigned intents/runs; do not let webhook payload fields alone create arbitrary completed work. Job-level completion can release a verified job commitment even if the aggregate run continues. A workflow failure without the expected artifact/receipt is not success.

GitHub requests a response within ten seconds. Persist the received event in its owning direct-request/attempt record before acknowledgement; detailed source validation can remain pending. If durable acceptance cannot complete, fail the request. Do not acknowledge into an in-memory-only queue.

GitHub does not automatically retry failed webhook deliveries. Core-ci must recover failed/missed deliveries through the installed App's delivery API where available, with a durable deployment recovery position, bounded pagination and explicit uncertain gaps. This is one recovery function inside core-ci, not a second scheduler. Startup and a low-frequency SNode.C timer trigger it. A gap beyond API delivery retention is not proof that no tag was created: show an actionable recovery gap and allow verified operator re-submission of the exact source event/tag. Never silently bootstrap all old tags as new activations.

Primary sources: [webhook practices](https://docs.github.com/en/webhooks/using-webhooks/best-practices-for-using-webhooks), [failed delivery handling](https://docs.github.com/en/webhooks/using-webhooks/handling-failed-webhook-deliveries). App-delivery permissions and retention must be confirmed during the protocol spike; no live webhook/App settings changed in design.

## Core-ci dispatch to GitHub

The installed App dispatches a configured allowlisted workflow/ref with a compact server task/attempt identity, intent and protocol version. Do not send private keys, broad tokens or arbitrary server URLs from untrusted source definitions. Endpoint/issuer location comes from trusted deployment configuration; task data cannot redirect secrets to another host.

Before dispatch: durable admission and commitment. After success: bind returned run details when supported. A response lost after GitHub accepted the dispatch leaves the same attempt uncertain. Targeted intent/run lookup must precede another dispatch. A second matching live run is an explicit conflict, not a request to pick whichever is newest.

Capture/build/publisher workflow identities and permitted tooling revisions are separate from the selected source SHA. Pin trusted reusable worker code. The source's untrusted hook never chooses publisher code.

No GitHub API call runs through a synchronous `gh`/`git` subprocess inside core-ci. Use the SNode.C HTTPS client. GitHub workers may retain trusted Python/shell tooling and standard artifact actions.

## Worker claim and report

Proposed endpoints:

| Endpoint | Purpose |
| --- | --- |
| `POST /api/v1/attempts/{attempt}/claim` | Bind verified run/attempt; obtain immutable task instructions and task-scoped read access |
| `POST /api/v1/attempts/{attempt}/reports` | Idempotent started/artifact-ready/terminal report |
| `POST /api/v1/captures/{capture}/register` | Register verified immutable capture metadata and bundle artifact identity |
| `POST /api/v1/publications/{attempt}/prepare` | Submit trusted candidate feed facts, receipt and exact expected feed commit |
| `POST /api/v1/publications/{attempt}/authorize` | Re-evaluate single server policy; durably enter final commit fence |
| `POST /api/v1/publications/{attempt}/confirm` | Report external commit; core verifies receipt before recording published outcome |
| `GET /api/v1/status` and `/status/...` | Read-only projection/live pages |
| `GET /api/v1/exports/{revision}` | Immutable core-rendered metadata snapshot for assigned feed-writer operation |
| `POST /api/v1/operator/reconcile` | Wake without resets |
| `POST /api/v1/operator/tasks/{task}/retry` | Authorized retry of exactly one task |

Routes are illustrative; reduce unnecessary round trips during the spike. Prepare and authorize remain logically distinct from long artifact processing; authorization cannot be granted before final candidate facts are known. An ordinary idempotent POST may return the already committed result. Reusing an idempotency key with different content is rejected. Request sequence numbers never override verified higher attempts or published receipts by themselves.

Typical report fields: attempt ID, operation, run ID, run attempt, check/job identity where available, event kind, artifact ID/digest where applicable, and an outcome. Selected SHA/tag/row/recipe come from the assigned task; echoes must match them. Server acceptance/ordering is not inferred by comparing GitHub timestamps with feed timestamps.

A report emitted before actual GitHub job teardown is not enough to release a capacity commitment. Use verified job outcome/event, or a targeted job lookup. Artifact-ready and job-completed are separate facts. Reports do not upload package payloads to core-ci.

## OIDC authentication

A worker requests an OIDC token for the configured core-ci audience using `id-token: write`. Core verifies signature using GitHub's trusted discovery/JWKS endpoints, issuer/audience/time constraints, repository and owner IDs, run/attempt and allowed workflow refs/SHAs. Reusable workflow claims must refer to the trusted reusable worker revision, not just the caller repo. Inspect the actual [OIDC claim reference](https://docs.github.com/en/actions/reference/security/oidc) and [reusable workflow guidance](https://docs.github.com/en/actions/how-tos/secure-your-work/security-harden-deployments/oidc-with-reusable-workflows).

Task ID, artifact bytes and arbitrary request body fields are not authenticated simply because the same request carries a valid JWT. Bind the claim to the assigned attempt; verify run intent/actor against targeted GitHub facts, and artifact/job relationships separately. Signed `check_run_id`, when present, is checked against the actual job API; do not assume it substitutes for every job/artifact verification.

Use vetted JWT/JOSE verification facilities, not a homegrown crypto protocol. Allow only expected algorithms and known issuer key URLs. Bounded in-memory JWKS/token caches are transport conveniences, not scheduling authorities; expiry/miss triggers async refresh. Unknown keys/network failure fail authentication closed. No persistent polling cache is proposed.

OIDC establishes worker origin, not trustworthy source-hook behavior. The capture manifest must be finalized from trusted pinned inputs. Prefer reports/claims before untrusted source hooks and trusted completion verification afterwards. A source hook with its job's token must still be unable to impersonate a publisher or alter another task.

## Capture/build/publisher credentials

Core-ci stores a deployment-specific GitHub App private key and webhook secret outside the repository. The App is installed only on participating deployment repos with necessary contents/actions permissions. Issue repository-scoped installation tokens for worker read operations. Core may have an App-level ability to mint feed write tokens; no untrusted build claim can request one.

Sources retain their ordinary GITHUB_TOKEN limited to their own source plus task-scoped coordinator capture read access. No sibling source reads, publisher token, signing key or permanent broad App key is supplied. Private-feed download support is a separate scope decision; the existing public feed path needs no such expansion.

The trusted publisher retains signing secrets in GitHub. Feed write access is issued only to the assigned allowlisted publisher workflow. Public-key authority remains the deployment worker repository's approved keys overlay. A worker refreshing a token after a long build authenticates the same assigned attempt again.

Operator identity is separate from worker OIDC. Use an existing deployment IdP/integration plus explicit operator allowlist; exact choice is pending design approval. Do not reuse worker credentials as an administration login.

## Publication candidate, fence and recovery

A publisher can download, verify, sign and construct a candidate before final authorization. It works against an exact feed commit and preserves sibling/version data. All external writes use feed CAS; a conflict requires candidate re-evaluation, not a force overwrite.

Before final authorization, verify task/source/artifact identities, key consistency and internal package metadata on GitHub. Send the complete scheduling-relevant feed manifests and checksum identities from the trusted checkout, bound to the candidate/feed commit. The server's one C++ policy evaluates arm currency/current work. The publisher does not keep its own old Python scheduling gate in parallel. Hash/receipt and package-control verification remain publisher responsibilities.

After authorization, core preserves the relevant decision until commit confirmation as described in architecture.md. Accepted conflicting source events wait durably for application. Incoming nonconflicting reports can be recorded and live status served. An uncertain publisher cannot lose its authorization just because a timer expired while it could still push. This is the explicit safety/availability trade-off D3.

Confirm checks the feed commit/receipt through a trusted API/read path. Publication is not accepted merely because the workflow returned success. If core crashes after the push and before recording confirmation, the receipt recovers it. If the publisher crashes before a push, verify actual terminal execution and absence of the authorized receipt before release/retry. If both are uncertain, retain the fence and show operator action rather than dispatch another publisher.

Any metadata-only status writer uses the same serialized write authority and feed CAS. It cannot mutate package bytes, receipts or signing outcomes. Unexpected manual/old writers cause CAS conflict; old writers must be disabled at cutover.

## Protocol acceptance cases

Mandatory negatives: forged webhook/HMAC; wrong issuer/audience/algorithm/repository/workflow; valid token for another task; missing reusable workflow identity; stale/replayed JWT outside validity; old attempt after retry; conflicting artifact digest; report claiming a job that never ran; signed but unassigned run; source hook claiming publisher role; duplicate authorize/confirm; lost dispatch response; lost webhook after process restart; uncertain commit with publisher still able to write; source event arriving between authorize and push.

No protocol endpoint, token or webhook has been created by this design.
