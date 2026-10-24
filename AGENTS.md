# core-ci engineering and ownership

core-ci is a single-process, single-threaded, event-driven SNode.C application. Follow framework apps/ patterns through installed public SNode.C targets; do not vendor the framework, introduce a second event loop or add an application thread pool. Keep blocking storage, subprocess waits and network work out of event handlers. The current repository is design-only: no implementation until design approval.

Canonical checkout: `/home/voc/projects/snodec/core-ci` on jupiter, beside `/home/voc/projects/snodec/snode.c`. This is the new project, not a task worktree. Initial documentation establishes main; subsequent coherent implementation work uses feature branches. GitHub source: SNodeC/core-ci. Integration target: VOC-Experiments only. Do not change live SNodeC package CI, sources, credentials, tags or feeds through this task.

Use persistent builds at `<repo-root>/build/codex/<configuration>`, temporary checks at `/tmp/codex/build/core-ci/<task>`. Any approved additional checkout belongs under `<repo-root>/checkouts/<task>` and needs recorded purpose and cleanup condition. Ignore build/, checkouts/ and local credential directories; never put credentials into tracked documentation. Do not move existing deployment credentials here implicitly.

One authority per concept: local transactions own scheduling; verified feed receipts own publication outcomes; the publisher owns package-byte validation/signing. No permanent dual scheduler. Approval for this architecture does not authorize weakening dependency, provenance, callback or publication guarantees.

Use braces, one return at each function's end, no avoidable break/continue, changed production lines at most 160 characters. Before the first C++/CMake implementation commit, copy the applicable canonical formatting/analysis files and modules from ~/.codex/formatting byte-for-byte, integrate explicit formatting/check targets over src/ and tests/, and record exclusions. The initial Markdown-only stage intentionally does not introduce unused build tooling.

Tests cover stable boundaries and restored invariants. Test the affected behavior; do not repeatedly run unrelated suites. Distinguish design estimates, local proofs and live evidence. Report production/test/docs growth and removed mechanisms separately. Preserve all earlier evidence. Do not claim operational readiness from documentation or synthetic tests.
