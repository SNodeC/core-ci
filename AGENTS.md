# Fresh controller

This independent repository is /home/voc/projects/snodec/core-ci-new. The owner
authorized its creation instead of changing /home/voc/projects/snodec/core-ci.
Branch: feat/graph-controller. The owner authorized committing, pushing and merging
the accepted fresh controller into the existing SNodeC/core-ci repository, replacing
its old controller. Production deployment remains separately authorized.

Use only the current task specification and extracted external contracts in
docs/contracts.md as controller design inputs. The reference repository may be
read solely for source definitions, workflow YAML, worker JSON contract
validators/serializers and documented authorization. Never read its controller,
tests, UI, SQL schema, proposals, reports, history or logs for implementation.
Never inspect credentials, private configuration, secrets/, .qtcreator/ or
.local-credentials/ in either repository.

The limit is 2,500 physical controller-owned C++ source/header lines everywhere,
including bootstrap, utilities, comments and blank lines. Lines are at most 160
characters. No generated business logic or custom external implementation.
Use braces, one final return and no avoidable break/continue.

MariaDB is the sole durable graph/state authority. Immutable selections and exact
prerequisite attempts govern readiness. Persist assignment authority successfully
before dispatch; preserve unknown outcomes without replay across restart.
All controller tasks use one installed SNode.C event loop; no controller threads, timer, scheduler,
ready queue, cache, admission policy, feed lock or worker execution.

Persistent builds: build/codex/<configuration>. Temporary builds and fixtures:
/tmp/codex/build/core-ci/<task>. No additional checkout. Commit applicable
canonical formatting files/modules from ~/.codex/formatting byte-for-byte.
Use repository copies for explicit formatting/check targets.

Document extracted contracts before dependent implementation. Missing contract
semantics must be resolved explicitly, never recovered from the old controller.
Public deployment identities/pins are supplied inputs, never invented defaults.
Live VOC-Experiments acceptance needs separate authorization. Never mutate
production SNodeC workflows, feeds, tags, keys, databases or services.

Record significant decisions and checks in WORK_LOG.md. Report production, test
and documentation changes separately, exact source/build identity, physical C++
counts and unavailable acceptance. Do not call the full system working without
the specified end-to-end sandbox acceptance.

Formatting targets cover src/ and tests/ explicitly. The optional analyze target
uses the repository's canonical .clang-tidy with the selected build's compile
database; configure clangd with that same build directory. No third-party or
generated source is part of these checks.

The owner permits reading the reference src/main.cpp specifically for SNode.C
runtime usage patterns. Its controller behavior remains excluded as design input.
Use SNode.C patterns wherever they solve the problem; ask before substituting
another pattern. Managed MariaDB may use small nonblocking ownership code in that
event loop. Configure before init, start database work directly after init, and
let the framework own event dispatch and shutdown.

The owner subsequently authorized live VOC-Experiments sandbox acceptance and
routing its existing App webhook to the foreground Jupiter controller through
existing WAN 8443 forwarding. This does not authorize production resources,
commits/pushes, a system installation, key rotation or workflow source edits.
The acceptance database must retain original selections and execution history.
Existing renewed sandbox workflow pins and certificate provenance are recorded
in docs/contracts.md; changing runtime inputs does not rebind assigned work.

The owner subsequently authorized copying the reference ui/index.html,
ui/style.css and ui/app.js as a presentation/layout template. This permission is
limited to UI assets; old controller logic remains excluded. Adapt presentation
to the fresh graph/API, preserve the current retry/retirement rules, and use the
browser locale's hour format. UI filtering/order cannot affect graph readiness.

The owner subsequently authorized an SSE-only heartbeat using the previous
implementation's SNode.C timer pattern. Each connected SSE stream sends a comment
every 30 seconds; disconnect cancels its timer and removes the response. This is
transport maintenance only, never graph polling, scheduling or reconciliation.

The owner subsequently required source-triggered forward propagation in addition
to accepted recursive prerequisite creation, with no net production-code growth.
Every source selects its transitive enrolled dependents; reuse existing exact
prerequisites. The newest selected tree is shown together in reverse dependency
order above older work; presentation never governs activation. The owner also
explicitly removed independent HTTPS and duplicate publication-content proofs.
The owner corrected publication authority: require the matched Publish job and
exactly one Confirm public receipt job to complete successfully in the same
authenticated pinned run/attempt, then read its committed receipt for dependent
inputs. Retain execution/attempt matching and terminal historical facts. Do not
repeat the workflow's HTTPS confirmation or signing/package/index checks.

The owner subsequently approved direct incremental SSE delivery and net growth
of 35–55 C++ and 20–30 JavaScript lines. Each connection gets one full initial
snapshot, then committed RFC 6902 field updates. No-change operations do not
write or notify. Pending SSE output is bounded to the remaining initial framed
snapshot plus 64 KiB; the initial allowance shrinks as bytes leave the socket.
Updates and heartbeat comments share this bound, including framing allowance.
Close an over-limit peer immediately; EventSource reconnects with a new snapshot.
No custom pipe/source, delivery replay queue, polling timer or graph cache.
This source-work approval does not authorize replacing the running sandbox.

The owner authorized the publication gate's net growth of 7 production C++ lines
(2328 to 2335), the sandbox native-confirmation workflow condition correction
and corresponding approved pin update, local restart and one new core-triggered
full-chain acceptance. No controller commit/push or production SNodeC resource
change is authorized. Existing assignments retain their frozen workflow pins.


The owner authorized the 2026-11-02 checkpoint before startup recovery work.
Preserve existing Git history by parenting the replacement to current origin/main.
Remove the superseded controller, schema, tests and obsolete documentation. Keep
existing deployment/ GitHub worker/workflow assets and public keys byte-identical;
they are external execution contracts, not a second controller. Do not inspect
those worker implementations as controller architecture. The reference checkout's
unrelated dirty work remains untouched. Automatic startup reconciliation was outside that checkpoint.


The owner subsequently authorized startup recovery with a +20–30 production C++
line estimate. One pass after successful management-listener startup reads the
MariaDB graph, then observes each latest assigned Building/Publishing attempt
through the existing immutable-attempt observation path. Independent errors do
not block other observations. No polling, redispatch of old assignments, new
queue/state/cache or source-tag discovery is added. Terminal/unassigned work is
skipped. New verified evidence may authorize the next stage or exact dependents
only through normal successful graph commits. This source-work approval does not
authorize restarting the live controller or starting live GitHub work.


The owner subsequently authorized committing the startup-recovery change, running
it locally with the existing sandbox inputs/database, triggering one fresh core
full-chain build, deliberately stopping core-ci during active builds, waiting for
those GitHub builds to finish while offline, and restarting to verify automatic
continuation. Do not redeliver webhooks, manually reconcile, retry or re-trigger
completion during this acceptance. Preserve assignments/history and observe only.
This does not authorize production deployment or production GitHub resources.
