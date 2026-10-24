# Development and VOC-Experiments integration stages

## Explicit location and ownership

Develop on jupiter in `/home/voc/projects/snodec/core-ci`, a sibling of the framework repository. Host the reusable source at SNodeC/core-ci. Do not put the application inside the framework or the existing DistributionPackages repo.

The orchestrated integration organization is **VOC-Experiments**:

| Repository | Future role |
| --- | --- |
| ci-coordinator | Thin trusted capture and signing/publication workers; authored sandbox content and public keys |
| packages | Existing signed feeds, receipts, static status export and authored content |
| demo-core | Framework-like source worker |
| demo-app | Dependent source worker |
| demo-side | Second diamond arm |
| demo-join | Join source worker |

Source hosting in SNodeC does not authorize tests or jobs against live SNodeC package sources. The four production integration repos and Packages remain outside this migration stage.

## Stage 0 — current design

New repository and architecture documents only. Existing sandbox local fixes are preserved separately; do not claim that they are committed or deployed. No token, webhook, workflow, source tag, feed or host service is changed. User approval is needed for D1–D6 before application implementation.

## Stage 1 — local application

Use a feature branch in the new repository. Integrate canonical C++/CMake formatting when code starts. Compile against supported installed SNode.C package targets. Async DB and HTTPS proof precedes the scheduler implementation. Keep all builds/caches in approved locations. No new framework API is assumed without tracing it.

Local fake GitHub and real HTTP/DB boundary tests exercise the complete catalogue. An organization fixture is data, not another source-tree copy. Do not introduce a compatibility scheduler merely to run old task JSON.

## Stage 2 — sandbox worker refactor, prepared but not active

Prepare coherent changes in the existing VOC coordinator and four sources, preserving authored config, site/install content, public keys and signing secrets. Existing generic Python code that validates/builds/indexes packages is reusable; no reason to rewrite package tools into C++ merely because the scheduler is C++.

Remove feed-write scope from capture registration. Core owns requests/reservations/register. Source builds claim immutable tasks with OIDC and task-scoped read access. Source tag webhooks replace Notify-App relays. Retain only the workflows necessary for capture, building, trusted package publication and the proposed shared-writer metadata export.

Remove old control-packages.yml scheduling and old dispatch observe/tick/capacity state engine in the refactored deployment. Keep trusted artifact identity, source-hook, index/signature, ownership and receipt protections until their replacement passes equal/stronger tests. Do not permanently leave an old/new scheduling mode flag.

Pin worker tooling/reusable workflow revisions. No sandbox refactor is pushed merely because this design repository is pushed.

## Stage 3 — required provisioning decisions

Jupiter needs reachable authenticated HTTPS for worker reports/webhooks, plus approved MariaDB access. No core.snodec.org configuration is needed for local development. Public jupiter connectivity is not established by this design; choose an owner-approved ingress arrangement, without exposing an unauthenticated operator API. A third-party tunnel or new host install needs separate approval.

Provision/update the VOC deployment App/webhooks and id-token permissions only after approval. Keep the application App key/webhook secret on jupiter outside Git, signing keys on GitHub, and no permanent broad source secrets. Verify source/capture/publisher permissions and JWT workflow claims before live work. No secret values in design documents or evidence.

An operator authentication provider must be chosen. Do not silently require deploying snode.c-idp as a dependency merely because it exists.

## Stage 4 — quiescent cutover, no dual scheduler

1. Record hashes, current workflows, task ledger, feed commit, public keys and active executions. Preserve evidence and rollback references under an approved location.
2. Stop new sandbox intake/old controller admissions through the approved workflow configuration. Let known executions/publications finish or explicitly resolve them; do not cancel jobs or discard uncertain commitments without separate approval.
3. Verify quiescence from execution facts and receipts, not only a green workflow list. Disable old scheduled/manual scheduling authority before the new instance can admit work.
4. Import a verified bounded snapshot of activation/direct-intent definitions, counters/row claims, immutable provenance, terminal histories needed for operator visibility, and existing publication receipts. Preserve identities; do not infer activation from any package that happens to exist. No feed reset or history rewrite.
5. Import is a one-time controlled tool/procedure, not a permanent legacy compatibility engine. Identify its exact schema/retention scope for approval before writing it. Do not claim the queue can be reconstructed from feed receipts alone.
6. Verify highest baseline, namespace ownership, task uniqueness and revision floors before enabling new admissions. Publish no package merely to test import.
7. Enable core-ci as the sole scheduler and run deliberate new sandbox events under the existing/newly confirmed sandbox authorization. Workers stay on GitHub. Metadata export shares the one feed-writer authority.

If quiescence or faithful import cannot be demonstrated, stop. Starting on empty local state against a populated feed and ignoring old identities is not a valid shortcut.

## Stage 5 — measured live acceptance

Exercise the deliberate DAG, moved/older tags, exact-source difference from main, terminal retention, replay, stale publication, concurrent event and restart/lost-report cases. Use actual package clients where authorized. Keep all failures/evidence. Measure API calls and event-to-status latency, including static-export lag and commit-fence duration.

The server live view and Packages export must explain their snapshot time. No percent-complete claim based on selected happy paths. No universal readiness claim from convergence alone.

## Rollback

Before cutover, rollback is simply not enabling the new scheduler. After new core-ci has admitted tasks, do not re-enable the old scheduler against stale state. First quiesce/resolve new work and prepare an explicitly reviewed state/receipt reconciliation. Preserve old tooling and evidence for recovery, but never run both scheduling authorities. The rollback decision may require manual work; it is not an automatic claim that old code can interpret the new schema.

SNodeC production deployment remains a later separately authorized stage. No production tags, feeds, credentials, workflow toggles, Apps or merges are touched by the VOC design/refactor.
