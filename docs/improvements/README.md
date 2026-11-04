# Operational improvements

This register collects known gaps and deployment limitations. It is a planning record,
not approval to implement features or weaken evidence checks. The graph remains the sole
execution authority; package builds, signing and publication remain on GitHub.

## Recovery and diagnosis

| Gap | Consequence | Required improvement | Evidence / status |
| --- | --- | --- | --- |
| No operator recovery action for an uncertain publication | A terminated publisher can hold the feed-writer slot indefinitely, blocking otherwise completed builds | Add an explicitly authorized, evidence-backed resolution through the existing graph transaction path: verify publication or record a definite failure. Preserve assignments, history, package bytes and uncertainty when evidence is inconclusive | Verified: APK publisher [40465675752](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40465675752), revision 113; retry endpoint returns HTTP 409 while state is publishing |
| Recovery currently needs administrative database access | An operator can violate persisted validation rules or restart invariants; a transaction alone does not validate the graph | Route recovery through existing graph validation and conditional persistence, with an audit record and reviewable evidence. Do not add parallel state or a second scheduler | Verified operational gap; supported execution evidence phases are only build/publication |
| Dispatch response can report an identity mismatch before a valid webhook binds the run | A request response can look unsuccessful even though its assigned job is running | Investigate the response/run-title timing and give a precise pending-verification result without replaying dispatch | Observed on app APK retry r124; correct authenticated webhook subsequently bound run 40489115089. Cause remains a hypothesis |
| Git push HTTP 403 lacks sufficient diagnostic context | Fresh-token authorization failures cannot be confidently distinguished from policy or transient server failures | Consider narrowly bounded, secret-safe diagnostics: status, request ID and relevant permission/rate-limit metadata. No token values, Authorization headers or blanket mutation retry | Cause unresolved. Same writer configuration also succeeds; ordinary token expiry and a persistent key mismatch are not established explanations |
| No dedicated presentation for a failed publisher with an uncertain feed outcome | “publication complete; verifying results” can sound successful despite a failed GitHub push | Show the verified GitHub failure and unresolved publication outcome together, with the actual blocker and execution link | Verified in graph/UI evidence for revision 113 |
| Blocker text reports state names rather than the actionable shared resource | A successfully built node says “attempt is building” while its actual obstacle is the occupied feed writer | Derive readable blockers from the existing eligibility logic without creating another state authority | Verified in graph generation 865 |

A missing receipt alone must never authorize a retry. Recovery evidence must establish that
no old publisher can still write and distinguish committed feed state, public HTTPS state,
package identities and recorded assignments. Never edit a GitHub run's evidence to pretend
that its push step was skipped.

## Starting and observing work

| Gap or limitation | Consequence | Required improvement / decision |
| --- | --- | --- |
| Small package publications spend substantial time in serial validation and tool setup | Feed-wide serialization includes SDK download/preparation and metadata validation before the push, so trivial packages wait behind minutes of preparation | Measure each stage and consider reusing independently verified immutable tooling and shortening the serialized section while preserving the trusted publisher boundary and uncertain-writer exclusion | 
| No explicit “rebuild this entire graph with the same source selection” operation | Repeating a source request can reuse existing executions; retry handles failed work, not a new complete session | Define an owner-approved rebuild operation and input semantics using the existing graph and revision allocator. Do not manufacture tags or silently rebuild prerequisites |
| Aggregate UI counts include historical failures | A user can mistake old failures for failures in the current session | Make current selection/session context clear while retaining history |
| Static public pages update through a separate worker and Pages deployment | Correct controller state can precede public pages by minutes | Show projection generation/time and distinguish controller commit, status job and Pages visibility. Do not imply instantaneous static-page updates |
| Root status rows can render an empty “Waiting for” block; dependency lists can include already satisfied prerequisites | Presentation is noisy and does not explain the real blocker | Have templates distinguish dependencies from currently unsatisfied prerequisites; hide empty waiting sections |
| Framework logs include TLS shutdown/descriptor warnings on completed webhook connections | Transport warnings may obscure actual application failure | Investigate separately with delivery outcomes and a focused reproduction. No framework change is authorized by this register |

## Accepted limitations and deployment gates

- A tag webhook acknowledged before selection persistence can be lost on a crash. Explicit
  rescan/redelivery is the accepted recovery; no durable inbox is currently planned.
- Management HTTP on `core.vchrist.at:8080` is an owner-authorized initial test setting.
  Management TLS and authorization are deferred and are not implemented by public webhook TLS.
- The service must restart after TLS certificate renewal to load refreshed systemd credentials.
  Verify or arrange renewal integration before unattended long-term use.
- Root SDK discovery trusts official HTTPS checksum delivery; stronger independent checksum
  authority remains a proposal, not an implemented guarantee.
- Client acceptance, public tamper rejection and new graph-session success must be reported
  separately. An earlier successful session does not prove a later partially blocked session.

## Scope of this register

These entries consolidate observed shortcomings and accepted limitations; they are not an
exhaustive new architecture/security audit. Designs, production growth estimates, approval
and focused verification remain required before implementation. Keep the operational evidence
in WORK_LOG.md and link new verified findings here.
