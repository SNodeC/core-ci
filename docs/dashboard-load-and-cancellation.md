# Dashboard load and cancelled assignments

The November 4 SNodeC deployment used a Debug controller. The 76-build selection
and its notifications produced a sustained graph-read/observation backlog. New
SSE connections could time out while awaiting their snapshot. After the backlog
drained, the same 3,607,087-byte raw graph took 0.880–0.950 seconds to retrieve
in three sequential public HTTP measurements. The page then took 0.15 seconds.
These are end-to-end local network observations, not isolated CPU timings.

Deploy the same reviewed source with CMAKE_BUILD_TYPE=Release, retaining the
installed SNode.C runtime, TLS settings, database authority and SSE contract.
Compare the same retained snapshot before and after; do not infer a general
throughput guarantee from an idle-page measurement.

A cancelled GitHub build remains an assigned Building attempt under the existing
uncertainty contract. Cancellation does not authorize replacement or automatic
retry. The badge displays Cancelled when that attempt's recorded build run has
a cancelled conclusion. Terminal history and all management enabling rules stay
unchanged. The summary calls the stored Building category "build phase", rather
than implying that every assignment is currently executing on GitHub.

This is a presentation correction, not a new durable state or readiness rule.
No production lines are added. The pre-existing uncommitted phase-cell UI work
is preserved separately from this patch.
