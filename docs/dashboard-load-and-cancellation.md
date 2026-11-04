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

Release validation also exposed browser reuse of the previous app.js. Static
responses now require revalidation (Cache-Control: no-cache), and the script URL
changes for this update so already-cached scripts are not reused. Asset paths
and all API contracts remain unchanged. This adds no C++ or HTML lines.

The Release startup observation burst still delayed two raw HTTP reads by
16.38 and 10.85 seconds. After it drained, three reads took 0.547–0.592 seconds
for 3,607,890 bytes. Release improves the idle comparison but does not eliminate
whole-history read cost or prove responsiveness under arbitrary notification
bursts. Full hydration remains O(history); no cache or second state authority
was added.

Observation callbacks now share one immutable selected execution per observation,
rather than repeatedly deep-copying that execution into nested callbacks. This
value dies with the observation; it is not a graph cache. Fresh committed-state
guards remain in Store::change/applyEvidence. No production lines are added.

An isolated Release fixture using the same 178-execution, 3,607,087-byte snapshot
and 59 synthetic failed run reads took 4,032.71 ms before and 2,968.66 ms after.
Both delivered 59 completions and zero dispatch effects. These are local fixture
measurements, not GitHub latency or a full-chain throughput guarantee. Existing
publication/restart/uncertainty, graph and JSON diagnostic tests pass. Full
hydration and serialized Store writes remain O(history).

The dashboard must distinguish committed build authorization from a running
GitHub job. A Building assignment whose observed job/run is queued is displayed
as Queued; the summary uses the same presentation function as the badge. The
active assignment, readiness rules, management actions and dispatch are unchanged.
Cancelled assignments retain the existing presentation distinction and now have
an explicit summary count. Started cells already suppress GitHub timestamps on
queued jobs. Job/run webhook observations do not provide continuous step telemetry;
a step name is the last observed detail, not a stream of every step transition.

Validated the178-record scenario:20in_progress,56queued,102prerequisites waiting.
Cancellation and queued/running tests pass; production delta app.js+5, index.html0.
This UI-only change is deployed without a controller restart or build interruption.
