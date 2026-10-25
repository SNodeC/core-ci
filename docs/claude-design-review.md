# Independent Claude review

Requested by the owner on 2026-10-25. Produced by the locally installed Claude CLI
using its configured default model, with tools, MCP and session persistence disabled.
Input: the complete proposed docs/design.md, a detailed condensation of the current
owner specification, and the explicit MariaDB-connect clarification. Claude did not
inspect source, execute checks or verify current external APIs. Findings and suggested
mechanisms below are reviewer opinions, not accepted requirements or verified facts.
The design has not yet been amended in response.

# Design review: persistent package graph controller

## Verdict

**Approve with required revisions, not as written.** The core invariants are right: GitHub is the definition authority, MariaDB holds execution decisions, dispatch authority is committed before the request, and uncertainty is never treated as failure. The design is also honest about what is unverified.

Four definite defects must be fixed before approval, because they decide whether the graph is correct:

- The diamond rule is made conditional on owner policy.
- Retry breaks dependent edges.
- The APT pool layout collides across suites.
- Feed-writer serialization has no sound mechanism.

None of these fixes needs new architecture.

## Definite defects (fix before approval)

**D1. The diamond guarantee is conditional, but the owner made it mandatory.** See *Graph, source and storage*, paragraph 4. The rule "J cannot run from A while B building/failed" is owner-stated. Only the enrollment and wave *details* are pending.

As written, J can still use an older B. When R changes, B is rebuilt under the *same* B source tag. "Highest tag" selection therefore cannot tell old B from new B, because the selection is keyed by tag, not by execution.

Smallest fix, with no wave entity:
- When a change at R is accepted, traverse the affected dependent subgraph in **one transaction**.
- Create the waiting executions for A, B and J together.
- Bind J's prerequisite edges to those specific new A/B **execution IDs**, never to "latest published".

J's derived eligibility then waits for both automatically. A wave ID is only needed if the owner later wants cross-change coalescing.

**D2. Retry orphans dependents.** See *State transitions*: "explicit retry … creates an identified attempt; a new build reserves a new package revision."

If an attempt is a new execution, J's edge still points at the failed one and J waits forever.

Fix:
- Split the stable **execution** (selection) from its **attempts** (dispatch, run, revision, evidence).
- Edges target executions.
- An edge is satisfied when any attempt of that execution reaches verified publication.

This is one table split. It also gives uncertain dispatches a natural home.

**D3. The APT pool collides across suites.** See *Feed layout*: `apt/<distribution>/pool` is shared by every `dists/<suite>`.

A bookworm and a trixie build of `name_V-R_amd64.deb` have the same filename and the same version but different bytes. That violates "never different bytes same published identity".

Fix, choosing one:
- a per-suite pool (`apt/<distribution>/<suite>/pool`), or
- a suite-qualified version suffix.

The per-suite pool is simpler and keeps the encoding unchanged.

Relatedly, `openwrt/<suite>/<arch>` has no format component. IPK and APK, or two SDK releases, could then only be rejected, never coexist. Add `<release>/<format>` to the path instead of relying on rejection.

**D4. Writer serialization is unspecified, and the obvious mechanism loses work.** See *Workflow and artifact boundary*: "Serialize writers per feed."

A GitHub Actions `concurrency` group keeps at most one *pending* run. A newer pending run cancels the older one, so publications would be silently dropped.

Fix: the controller enforces at most one `publishing` attempt per feed path. This is a transactional predicate, not a queue. The checked non-force push remains as a backstop.

**D5. Image identity timing is self-contradictory.** "Image digests … become immutable assignment inputs before a build can consume them." No component is assigned to resolve tag→digest before dispatch. The controller has no registry client, and resolving in the worker happens *after* assignment.

Smallest fix: require `image` values to be digest-pinned (`…@sha256:`) and reject tags at validation. That avoids building a registry client. The OpenWrt SDK side is already correct because `sha256` is in the JSON.

**D6. Build-run binding accepts forged runs.** See *State transitions*, reconciliation. Matching on execution ID and inputs is not enough. Anyone with write access to a source repository can dispatch the thin workflow with a copied execution ID, or edit it.

Fix: before binding a run, also require all of the following:
- `triggering_actor` is the controller App bot.
- The workflow path is the expected one.
- The run's workflow file blob at `head_sha` equals the approved template.
- `referenced_workflows` contains the pinned reusable-workflow SHA.

Artifacts belong to runs, not jobs. A modified caller with an extra job could otherwise upload a competing artifact.

**D7. The publisher must not trust manifest claims it cannot verify.** Hooks run in the same job that writes the manifest, so the manifest is attacker-influenced.

The publisher must take these from the **controller's assignment**, not from the artifact:
- source SHA
- prerequisite identities
- revision
- target

It must recompute every checksum and read package metadata with standard tools. Treat the manifest only as a cross-check. State this explicitly in *Workflow and artifact boundary*.

## Unresolved choices (ask the owner; do not invent)

1. **Supersession.** Suppose a new prerequisite tag, or a second R change, arrives while J executions are still waiting. Either both run, or the older one is superseded.
   - Revision R is reserved *at dispatch*, so an older-input J dispatched later receives a higher R. It would then become "latest" in the feed.
   - Either reserve R at selection time, or define supersession.
2. **Prereleases in dependency selection.** Under proper ordering, `2.0.0-rc1` outranks `1.9.0`. Should dependency work ever pick a prerelease?
3. **Missing upward executions.** Dependency selection may pin a prerequisite (tag, target) that has no execution yet, for example a newly added row. Is a prerequisite execution created in the same transaction (upward closure), or is the selection rejected? I recommend creating it, because that is consistent with D1.
4. **Permanently unmatched dispatch.** Absence of a run never becomes definitive, so such an execution stays stuck. Decide whether an explicit, confirmed operator action ("declare dispatch lost") is acceptable replacement permission. Without one, the system has a dead end.
5. **Missed tag webhooks during downtime.** Startup polling is excluded, so offer an explicit "rescan tags" UI action rather than leaving a silent gap.
6. **Runtime dependency strictness.** Should J's packages declare `A (= V-R)` or `>=`? This determines whether installed systems match the pinned graph.
7. **Feed retention.** "Preserve retained versions" is undefined. Growth is unbounded, which matters directly under the 1 GB Pages limit.
8. **Cross-repository edge matching.** "Target edges require equal row ID" couples repository-local labels. I recommend matching on coordinates plus, for OpenWrt, equal `sdk.sha256` (controller-checkable). Native ABI stays a worker-verified check.

Already pending: the enrollment/already-tagged policy and the wave policy.

Note for the UI: edits on the default branch take effect only after a new tag. The UI must show "committed, not yet effective" or users will misread state.

## Transactional async ownership

The statement "keep transactions sequential on the connection" needs a mechanism. Transaction bodies span callbacks, so there must be an explicit in-memory FIFO of transaction closures (non-durable, which is acceptable). Otherwise a webhook's queries can land inside a UI transaction.

Two further rules belong in the design:
- **Lost COMMIT acknowledgment** means unknown, never dispatch. Re-read state after lazy re-establishment.
- **Recording the dispatch response** is a second transaction. If it fails, the attempt stays "assigned, dispatch outcome unknown" and goes to reconciliation. Never retry the request.

## Native prerequisites and package tooling

- **Prerequisite source.** "Selected prerequisite locations" is unspecified. Fetch prerequisites from the signed feed by exact version and recorded checksum, using real client verification (`Signed-By`, gpgcheck). This exercises the trust path and matches the edge semantics.
- **Package manager detection.** Format does not determine the package manager: RPM could mean dnf, zypper or microdnf. Tool-availability detection is fine, but say so.
- **Architecture verification.** Check `arch` and `platform` at the worker against actual tool output (`dpkg --print-architecture`, `rpm --eval %_arch`, SDK `ARCH_PACKAGES`). This avoids a compiled mapping list.
- **APK re-signing.** Re-signing in the publisher requires the selected SDK's apk-tools there. The publisher must therefore download the SDK and verify its sha256, adding work not reflected in the estimate.
- **Version encoding.** Explicitly reject semver build metadata (`+…`). Map prereleases to `~` for DEB and RPM and `_rcN` for APK, and verify the results with each format's real comparison tools.

## Implementation verification (not defects)

- **Managed MariaDB readiness.** The owner forbids startup polling or timers, but requires a single explicit `connect()` after starting the server. Connecting to a server that is not yet ready fails, so an event-driven readiness signal is needed. Investigate whether the app can provide a `NOTIFY_SOCKET` datagram socket and receive mariadbd's `READY=1`. If SNode.C has no child-process primitive, this blocks checkpoint 1 and must be resolved first.
- **Dispatch input limits.** Confirm the current `workflow_dispatch` input count and payload limits against the size of the inputs. Pass compact, hashed assignment data if needed.
- **Protected environment.** Use deployment-branch restriction on `package-publication`. Required reviewers would block automated publication.
- **Token scoping.** Request per-operation installation tokens with permission subsets. In particular, use `workflows:write` only for enrollment tokens.
- **Feed availability check.** Specify who performs the HTTPS check before `published`: the controller's HTTPS client fetching the index and comparing checksums.

## What is sound

- The invariant: one immutable selection per execution, uncertainty never authorizes replacement.
- GitHub blob-SHA conditional writes, with the commit SHA shown only after confirmation.
- Definitions and hooks read at the selected revision, with no branch or publication fallback.
- Raw-byte HMAC validation before parsing, and delivery deduplication.
- Publication runs separately in the worker repository. Secrets of the calling repository cannot reach the reusable workflow, so this separation is structurally real.
- Unsigned and signed checksums recorded separately, and replay without re-signing.
- No force push.
- An honest Pages caveat.
- Secrets supplied by file input, with no implicit key generation.
- Explicit statements of what was not verified.
- Per-target edges keep independent targets independent.

## Estimate concern

The range of 5.5–8.5k production lines plus 1.8–3k test lines is plausible but probably low-biased. It omits:
- RS256 JWT signing via OpenSSL,
- GitHub pagination and rate handling,
- the transaction FIFO,
- run-forgery verification (D6),
- SDK handling in the publisher,
- four signed index pipelines, each with client-verification tests,
- the managed-server readiness work.

Ask for a per-checkpoint estimate with an explicit trigger for re-approval, for example more than 20% over a checkpoint's estimate.

The checkpoint order also defers the real path. The owner wants the path early, so make checkpoint 2 a thin vertical slice: one repository, one DEB target, a signed feed and an apt install. Widen to the other formats only after that works.
