# Workflow review corrections — 2026-10-26

Review findings are advisory; this records verification against current sources and owner policy.
Only relevant graph, pipeline, worker environment/provenance and formatting checks were run.
Existing five-target package-client diamond evidence remains recorded separately in
[sandbox deployment](sandbox-deployment.md). New live tooling evidence will be added there.

| Finding | Classification and result |
| --- | --- |
| F1 | Verified latest-attempt reader defect. Corrected run/job readers to attempt one, with current-process rerun rejection preserved. Focused fixtures expose contradictory latest evidence. |
| F2 | Verified preparation placement defect. Render/stage/hash/commit now precede the existing push step. Push-step failures remain uncertain. No new evidence protocol or recovery mechanism. |
| F3 | Owner chose blocking/reporting for an unreadable selected definition. Existing behavior retained; no unknown repository silently omitted and no old definition substituted. Assigned work remains independent of a new selection request. |
| F4 | Verified batch-abort defect. Graph-owned input construction and size blockers isolate oversized nodes, expose UI/HTTP reasons and preserve writer availability. No new persisted state. |
| F5 | Verified environment/evidence substitution gap at the container-to-host boundary. Host compares trusted environment and assignment evidence before recording a manifest. Trusted publisher still validates actual package metadata. |
| F6 | Verified pin consistency gap. Single rendered source-workflow check is shared between enrollment and deployment selection; no extra HTTP call. |
| F7 | Deployment input still unknown: observer token type/minimum scope cannot be retrieved from GitHub secret values. Existing operational credential unchanged; no broadened access. |
| F8 | Live metadata: feed ruleset list empty; main branch protection endpoint reports unprotected. No branch-access changes made. Protecting the feed remains an owner deployment action. |
| F9 | Empty job list is not documented proof of pre-push failure. Preserve uncertainty; no receipt-absence inference introduced. |
| F10 | Reproduced a definite generation-guard rollback with concurrent reports on real isolated MariaDB. After that specific rollback, revalidate once against current graph. Still reject conflicting/uncertain outcomes; no generic retry policy. |
| F11 | Verified copied-title ambiguity. Filter by the assigned controller bot before counting search matches. HTTP fixture includes forged title matches. |
| F12 | Suspected client lifetime defect not established. Framework client destruction detaches the separately owned connection; connection/command own callbacks. No speculative lifecycle change or ASan claim. |
| F13 | README collision corrected and OpenPGP passphrase limited to native preparation. Lexical all-version display is not a source-selection defect. HTTPS checksum trust documented; signed-list support not added. |
| F14 | Corrected active secret names, receipt/feed layout and obsolete acceptance wording. Distinguished exact build artifacts from compatible runtime package selection and static runtime output. |

The package-client scripts assert the requested top-level version. Compatible dependency
ranges can select other versions; installed prerequisite versions in logs are evidence,
not assertions of every graph-selected build dependency. `demo-join core 60, side 62`
proves installed program execution using build-time static exports, not dynamic consumption
of prerequisite packages. Client installation does prove the package manager resolved their
declared runtime relationships. Modified-byte rejection has local coverage; full live tamper
coverage remains incomplete and must not be conflated with live wrong-key rejection.

Official API references are recorded in WORK_LOG.md. Public webhook TLS remains approved,
but valid certificate-chain/key paths for gw-home.vchrist.at are still missing. No SNode.C
organization deployment, credential/key rotation, tag rewrite or package overwrite is authorized
by this review correction. Deployment and live verification remain in VOC-Experiments.
