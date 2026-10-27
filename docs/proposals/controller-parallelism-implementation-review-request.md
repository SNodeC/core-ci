# Review the controller parallelism implementation

Review `fix/controller-parallelism` against base `4257cb0fc67412da8696afdba307fc2e85ff64aa`.
This is an independent detailed review, not implementation/deployment authority.

Read AGENTS.md, then these documents in order:

1. `controller-parallelism-review-agreement.md`.
2. `controller-parallelism-review-validation.md`.
3. `controller-parallelism-review-discussion.md`, especially final corrections.
4. `controller-parallelism-design.md` and current implementation/test diff.

The owner approved P1 independent publication despite another assigned uncertainty, HTTP
capacity 8 per client (configurable 1–16), publication capacity 8 per feed (1–32), secret-free
HTTPS confirmation, and retention of two reviewed O/N workflow/tooling generations through
separate deployment-owned aliases. Canonical bounds: +310 net production, +790 tests;
subsequent sandbox copies/aliases: +450 production. These approvals do not authorize further
privileged setup splitting, new queues/schedulers/states or weakened verification.

Trace every agreement requirement through all consumers and current supported paths. Check:

- Database FIFO lazy derivation, graph application/generation before next derivation,
  execution continuation before derivative notification, exactly-once completion, no-op and
  rejection/error/rollback/uncertain commit paths, reentrant enqueue, lifetime and shutdown.
- All GraphStore commit/enroll and Database completion callers. Explicit selection/enrollment
  guards must remain; no stale Node retry or second mutation authority. No transaction over HTTP.
- SNode.C request/connection/flow correlation, intermediate versus final connect failure,
  early HTTP disconnect versus transport cleanup, cancellation/shutdown, TLS identity,
  origin switching only after drain, bounded concurrency, scoped token expiry/coalescing and
  no uncertain mutation replay. Verify framework interfaces from current source when needed.
- Parallel source aggregation: selected immutable definitions, deterministic errors,
  all required prerequisites, enrollment/tag/deployment/generation checks, F3 blocking policy.
- Status approval under sustained generations, uncertain POST handling and no new latch;
  unchanged notifications, same-state evidence, SSE receivers and enrollment list freshness.
- Publication capacity in advance, graph validation/blockers and restore, batch reservations,
  uncertain assignment retention, all-prerequisite joins and exact dispatch-after-commit.
- Workflow/job feed mutex shared with status; current feed read inside the lock; independent
  validation and secret-free confirmation outside it. Signing/setup still inside the locked
  publisher is an acknowledged approved partial boundary, not completion of a privileged split.
- Mandatory receipt hash/local revision outputs derived before push; exact HTTPS hash/origin/
  size bounds; `needs: publish`; no ignored/conditional push or always bypass, private secrets
  crossing jobs, publisher-name collision, executing-attempt bypass or new reporting protocol.
  The controller must still independently prove committed receipt/publication. Possible push
  remains uncertain; skipped push retains existing definite-failure semantics.
- O/N retention: old deployed wrappers/global pin preserved; new literal template pin and
  separate tooling/wrapper commits; new source aliases; historical exact retry, old assigned
  work and mixed-generation lock compatibility. Actual sandbox installation is not done.
  Check upgrade documentation for deployability and rollback restrictions.
- Whole-diff coherence, deleted superseded paths, unnecessary state/branches, code style,
  line bounds, meaningful public-boundary tests and honest local versus live evidence.

Local proof: focused Debug build/format-check; real isolated MariaDB graph/enrollment/pipeline;
TLS/App transport checks, worker provenance and actual local DEB/RPM package/signing checks.
Logs are under `/tmp/codex/build/core-ci/controller-parallelism/`. Eight unchanged notifications
remain zero HTTP requests; measured local graph/SSE progress was 0.0919 seconds with 0.300-second
synthetic derivative API latency. No live performance/rollout claim. Review test coverage,
not just pass summaries. Do not run a full suite or mutate anything for this review.

Return severity-ranked findings with current file/line, concrete trigger, violated invariant,
and smallest correction. Distinguish definite source defects, hypotheses needing a probe,
owner policy and known deferred scope. Do not accept a mechanism merely because our earlier
review preferred it. Explicitly list any remaining disagreement and acceptance gaps; give an
overall recommendation. Return text only; the primary agent records your report.

Do not inspect credentials, secrets, private configuration, `.local-credentials/`, `.qtcreator/`,
`secrets/`, personal logs or old discarded implementation. No edits, commits, GitHub mutations,
restarts, deployments, additional checkouts or production SNodeC changes.
