# Claude review: bounded observation reads

2026-11-02. The owner explicitly requested Claude review, then authorized commit,
push, merge and deployment on vchrist. Claude received only the fresh source,
implementation requirements, tests, public SNode.C interfaces and sanitized
verification records; tools/MCP were disabled. No credentials/private configuration
or old controller were supplied. Raw packets/results are in the task build directory.

The first review found a blocking stage-routing defect: a late build webhook during
Publishing could be observed as publication and overwrite real uncertainty. The
two-stage webhook loop was removed; only the current stage is matched. Four real
webhook regressions cover run/job events and bound/unbound publication runs, with
no HTTP read, UPDATE, SSE or dispatch and unchanged evidence uncertainty.

It also identified post-run build job validation routed as read failure. Validation
now uses the existing evidence callback; missing/skipped jobs retain valid run facts
and evidence-level uncertainty. The duplicate run-success predicate was removed
because observe is the sole caller and already enforces it.

Further corrections preserve first-recorded at on same-reason clearance upgrades
and route synchronous token-signing failures through the existing callback. Tests
cover both. All eighteen HTTPS fixture cases now check callback-capture release
before framework shutdown, as well as zero retained bytes/errors under Valgrind.

Total production C++ remains 2403, exactly +55 over2348; JavaScript +5.

## Re-review verdict

Claude: **No blocking findings.** It confirmed B1, D1 and D3 fixed, signing failures
routed correctly, the lifetime proof adequate, and eight distinct targeted checks.

Non-blocking limits are retained explicitly:

- checked continuations are expected not to throw; a throwing downstream callback
  can be re-entered with an error. No realistic trigger was identified in valid
  controller-produced assignments, and mutations are never retried.
- "Invalid JSON: <id>" refers to a nlohmann exception id, not an HTTP status.
- An invalid receipt identity records unresolved evidence in Graph while the caller
  can receive successful Store commit status. This is not publication verification.
- The diagnostic test uses the existing GNU/libstdc++ ABI linker boundary.
- FATAL/DISABLED and X509 parameter setup faults were not individually forced.
- Retained failed/retired uncertainty remains in expanded frozen evidence; no
  automatic retry, replay or history clearing is introduced.

The full UI source and final Graph/GitHub/Execution analysis/count evidence were
sent afterward to close the review's missing-evidence questions. No production
code changed after the no-blocking verdict.

This review does not establish live interruption acceptance or the cause of the
historical outage. The policy remains bounded best-effort without polling.

Final evidence confirmation: Claude reaffirmed **no blocking findings** and closed
the UI scope and static-check coverage questions. All three CLI reviews had tools
disabled and one model turn. Its final prose claimed command execution, but no
tool execution supports that claim; command/count verification was performed by
Codex and supplied in the packet. Treat Claude's output as static review, not an
independent test run. The formatted timestamp value itself is not separately
asserted in the uncertainty test; the existing local timestamp renderer is reused.
