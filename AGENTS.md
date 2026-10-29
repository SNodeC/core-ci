# core-ci engineering

The current owner specification is a fresh implementation. Do not recover requirements,
source, architecture or protocols from discarded core-ci history. No migration or
legacy compatibility. There is no IdP integration.

Canonical checkout: /home/voc/projects/snodec/core-ci. GitHub: SNodeC/core-ci.
Use feature branches based on main. External integration is restricted to explicitly
authorized VOC-Experiments repositories. Never change production SNodeC workflows,
credentials, tags or feeds. Commit, push, activation and publication are separate stages.

Use persistent builds under build/codex/<configuration> and temporary checks under
/tmp/codex/build/core-ci/<task>. No additional source checkouts without approval.
Ignore build/, checkouts/ and credential directories. Preserve .qtcreator/ and
.local-credentials/; do not inspect or commit their contents.

Study SNode.C src/apps and relevant MQTTSuite applications before implementation.
Use installed public SNode.C package targets, one thread and one framework event loop.
Keep blocking storage, network operations and subprocess waits out of event handlers.
Register configuration and named instances before core::SNodeC::init(). Call init once.
Start managed MariaDB if selected and call MariaDBClient::connect() explicitly once.
Only initial database success configures the application and starts the listener.
On initial failure, log and release resources at the database boundary, leaving no
listener or pending startup work. End main with return core::SNodeC::start().
No startup polling, retry loops, deferred startup, explicit stop/exit or custom help.
Later commands may use framework lazy establishment after the connection disappears;
never replay uncertain mutations. Unmanaged mode must leave the external server alone.
Keep configuration and route registration visible in main and useful callbacks inline.
Separate successive tasks with blank lines.

GitHub ci/packages.json is authoritative for build definitions. MariaDB stores enrollment,
immutable selections, dependency relationships and execution evidence. One graph/state
authority; no persisted ready queue or generic workflow engine. Commit dispatch authority
before calling GitHub. Unknown outcomes preserve assignments and uncertainty.
A join waits for all relevant prerequisites; verified publication alone satisfies an edge.
An explicitly acknowledged operator retirement may mark an unbound publication failed
while preserving its assignment/history and unknown remote outcome; it never replays work.
Builds, signing and publication run on GitHub, with privileged publication kept separate.
Image/SDK discovery and registry authentication also belong to GitHub worker tooling.
The controller dispatches selected source/target/prerequisite inputs and records verified
execution evidence; do not introduce controller registry/SDK clients.
Keep deployment secrets out of source definitions, browser code, artifacts and logs.

Use braces for compound statements, one return at function end and no avoidable
break/continue. Changed production lines are at most 160 characters. Copy applicable
canonical files and cmake modules from ~/.codex/formatting byte-for-byte before the
first C++/CMake implementation commit. Use committed copies for formatting/check targets
covering src/ and tests/, excluding generated/third-party files and .qtcreator/.
Do not introduce unused build tooling in the design-only checkpoint.

Record significant decisions and verification in WORK_LOG.md. Report production,
test and documentation changes separately. Distinguish local proof from live acceptance.
Before net production-code growth, present the design and estimate and obtain the
explicit approval required by the global engineering instructions. Do not claim completion
until the real graph-driven package pipeline and required acceptance have been verified.

The owner approved the fresh controller implementation with a maximum of 3,810 physical
C++ source/header lines under src/, including main.cpp. Reused UI assets and GitHub worker
tooling are separate; do not move controller logic there to evade the bound. This replaces
the earlier implementation estimate. Re-estimate before exceeding the bound or introducing
unagreed architecture. Runtime dependency declarations remain source-owned compatible
ranges; build artifact selection stays exact.

Canonical formatting files/modules are byte copies. The top-level build extends the
canonical format target to tests/ and adds format-check for src/ and tests/. Source
and CMake discovery excludes generated builds, checkouts and user-owned .qtcreator/.
