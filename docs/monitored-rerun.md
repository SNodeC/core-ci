# Monitored sandbox rerun, 31 October 2026

The owner explicitly authorized VOC-Experiments/demo-core tag v2.1.14 at
fc3acdb94779dafb879c183143c20e19b435e766 and the full sandbox chain. The tag was
created once. The join request selected 20 new executions across four repositories
and five targets. All 20 reached independently verified publication; their 40
distinct assigned GitHub runs completed successfully. No assignment was replayed.

This was a recovered run, not uninterrupted timing acceptance. A controller
update backlog, two controlled restarts, operator reconciliation and a browser
observation gap are included in the elapsed time. Native publisher confirmation
jobs remain skipped by the existing workflow; explicit reconciliation was needed.
No workflow source, production feed, key, existing tag or original history changed.

## Measured times

Tag mutation began at 13:33:56.283 UTC; first selection was created at 13:34:02;
the final independent proof completed at 13:58:50. Elapsed tag-to-last-proof was
24 min 53.7 s; first-selection-to-last-proof was 24 min 48 s. GitHub timestamps
have second precision. These are observed timings, not speedup estimates.

| Boundary | Samples | Minimum | Median | Maximum |
| --- | ---: | ---: | ---: | ---: |
| GitHub build run created → assigned job starts | 20 | 4 s | 5 s | 14 s |
| Assigned build job queued → starts | 20 | 2 s | 3 s | 13 s |
| Assigned build job execution | 20 | 29 s | 44 s | 202 s |
| Build job finishes → controller publication authority | 20 | 5 s | 12.5 s | 438 s |
| Publication run created → privileged publisher job starts | 20 | 23 s | 105 s | 234 s |
| Assigned publisher job queued → starts | 20 | 3 s | 28 s | 103 s |
| Privileged publisher job execution | 20 | 12 s | 22.5 s | 32 s |
| Publisher job finishes → independently verified publication | 20 | 14 s | 73 s | 721 s |
| Proof timestamp → dependent build authority | 15 | 0 s | 0 s | 8 s |

Queue/setup boundaries include GitHub workflow dependencies, preparation and
shared writer ordering as applicable; they are not pure runner queue durations.
Publication proof delays include Pages/HTTPS availability, controller backlog and
operator reconciliation. Controller-boundary measurements include asynchronous
GitHub/artifact/receipt requests and DB work. Controller CPU time and the exact
first public availability of every receipt were not isolated. No new production
instrumentation or polling timer was added.

## Timestamp and SSE findings

Final authoritative graph and final real browser DOM each matched all 80 assigned
runner Started/Finished values against GitHub REST jobs: zero discrepancies.
Browser-local date/hour rendering matched the recorded UTC values in de-DE,
Europe/Vienna. Those final matches do not establish uninterrupted live alignment.

Before recovery, one job briefly displayed 13:38:36 instead of actual runner start
13:38:41. A late queued response carried the creation-time placeholder and replaced
the newer timestamp while the merge preserved in-progress status. Completion
corrected it. The merge now preserves phase timestamps as well as status and
conclusion against older phases. Queued jobs' placeholder start times are hidden.

The initial browser recorded 142 SSE frames and 143 DOM snapshots, including 70
within-state activity changes. For 93 matching displayed changes, SSE arrival to
DOM mutation was 9.25–25.14 ms, median 11.99 ms. Runner-to-SSE updates before the
fix were much slower: starts 4.01–56.73 s, median 10.45 s (17), and finishes
6.18–61.41 s, median 13.90 s (16). These include upstream delivery and observation
requests; they are not controller-only processing times.

The initial Chromium process disappeared after its last captured frame at
13:41:11.730. Its actual exit time/cause were not established. The observer lacked
a disconnect handler, so the missing interval cannot be treated as SSE latency.
A new browser, with disconnect recording, began at 13:57:04 and captured the five
join publication transitions through the final proof. For 14 matching displayed
changes excluding its initial snapshot, SSE arrival to DOM mutation was
14.60–19.28 ms, median 17.55 ms. DOM mutation is not a paint-time measurement.

## Stall correction and source accounting

Live management probes timed out after five seconds. MariaDB remained available;
debugger snapshots showed the controller serializing graph updates and 87 native
DB command sequences pending. Separate read/write sequences allowed many
notifications to read the same revision and repeatedly retry stale snapshots.
The correction makes each graph read and acknowledged autocommit write one
SNode.C MariaDBCommandSequence, removing the optimistic retry mechanism. One
controller connection remains the writer. Dispatch still follows commit success;
unknown DB outcomes do not dispatch. No controller queue, flag, timer or thread
was added. A post-recovery full graph read took 0.21–0.34 s in recorded probes.

The stalled controller stopped cleanly at 13:53:07; the corrected process began
at 13:53:16.790 against the same managed DB. Explicit observation reconciled
existing assignments. A second clean stop at 13:58:04 and start at 13:58:14.495
activated recording of real verification start facts. Both gaps remain in timing.

The layout keeps the original package, target, state, Build and Publisher widths. State activity is a single ellipsized
line with full text in its title. Publication verified has Started and Finished
inside its existing column. Started is the earliest recorded actual independent
proof check, including unsuccessful checks; historical unrecorded starts are not
reconstructed. The two remaining joins recorded starts after activation. Finished
is the successful exact HTTPS proof time. The final approved presentation is reverse
breadth-first dependency levels, with newer committed selections first within a
level. Literal reversed creation order was superseded because its depth-first
insertion sequence interleaved dependency levels. There is no state-based or
GitHub-time-based reordering. The old waiting rows remain in the app level and old
failed rows in the core level; age orders rows within their logical level.
Two of 44 published executions have recorded verification starts. The other 42
predate that recording; their missing historical times are not recoverable from
publisher/confirmation job timestamps. GitHub CI was inspected; it does not own
the controller's independent HTTPS check and requires no change for this field.

The owner's precise overhead question has a narrower requested boundary than the
tables above: REST ingress at the controller to resulting dispatch on GitHub.
The run did not capture local ingress/send timestamps or causal notification IDs
on transitions, so exact controller overhead cannot be reconstructed. For the five
post-recovery build-to-publication transitions, GitHub's first completed assigned
job/run notification delivery timestamp to resulting run creation was
5.634–9.471 s, median 6.896 s. This is a proxy with GitHub's one-second run-creation
precision, transport/acceptance time and unproven causal notification attribution.
It must not be reported as exact core-ci overhead.

Production C++: Controller.h 94, Auth.cpp 137, Auth.h 17, Evidence.cpp 346,
Execution.cpp 396, GitHub.cpp 310, Graph.cpp 427, Http.cpp 130, Managed.cpp 161,
Managed.h 24, Store.cpp 97, main.cpp 237: **2,376 / 2,500**, one fewer than the
preceding source. UI: 452 lines, two fewer. Test C++: 798, 25 added. Test drivers:
595, forty added. Documentation changes are this report, the work-log entry and
historical-report pointers; source.sha256 records the updated source identity.

Build/format-check and JS syntax checks passed. Focused real-MariaDB persistence
and graph/evidence checks passed (0.81 s combined on the first corrected build;
1.13 s in the later combined run). Native startup/HTTP and real Chromium UI checks
passed in 3.86 s, including long same-state SSE activity without row-height or
column-width changes, full hover text, stable order, multiple expansion and
prerequisite navigation. One attempted browser test first failed because its
browser-cache environment was missing; setting the existing approved browser
path resolved that setup error. The subsequent inverse-order and exact recorded
start-value browser checks passed in 4.12 s (4.13 s CTest total). A formatting check initially caught unformatted
new test calls; canonical formatting and the final format check passed.

Checkout /home/voc/projects/snodec/core-ci-new, unborn feat/graph-controller,
no remote/commit/PR. Debug GCC 16 build: /tmp/codex/build/core-ci/controller.
Final binary SHA-256:
6d6c8cb16d64f05767aeaf5061206d2711841dc81a8d5c13d0fd412a68a65abc.
Source manifest SHA-256:
084add6793b2e165c590b70ae12bf6d32aec471c611d3e9c726a852af45676ed.
Foreground sandbox PID 177384 remains on 8080/8443 with its original DB history.
Observer evidence and previews remain under
/tmp/codex/build/core-ci/rerun-20261031; completed observers are stopped. No
commit, push, merge, system installation or production activation occurred.

## Subsequent compact timestamp and prerequisite presentation

The owner clarified the desired format as 24-hour local time with seconds above
the date. Account locale inspection found LANG=en_US.UTF-8 and LC_TIME=C; the
browser's English locale default had selected 12-hour time independently of that
account time format. The page now explicitly uses h23 as requested, without
overriding the browser-local timezone. Date text remains browser-localized.
No OS, browser-account or Qt configuration was changed. This is an explicit
24-hour page format, not a claim that JavaScript can read the desktop LC_TIME.

Build and Publisher retain their original 184-pixel columns. Publication verified
alone grows from 156 to 184 pixels to match them; all timing pairs use 72-pixel
Started/Finished slots with an 8-pixel gap. The table is 1,076 pixels instead of
the original 1,048. Time is again 13-pixel bold text; date is 11 pixels beneath it.
The wider intermediate layout was rejected and is not retained.

Prerequisite navigation now uses actual anchors with fragment destinations,
preserving expand/scroll. The former generic button helper and its 240-pixel
maximum caused unnecessary label wrapping. That limit and the unused shared
row-selection style were removed; the remaining button style belongs to chevrons.
Live OpenWrt join labels measured one text line each at widths 247.83–258.53 pixels
inside the available 510-pixel prerequisite section. Native/runtime/Chromium
checks passed in 4.77 seconds after the anchor change. The earlier compact timing
check passed in 5.39 seconds. Three live locale/timezone cases each checked 234
timestamps: English/Vienna, Austrian German/Vienna and English/New York, with
zero text mismatches, zero overlaps, time above date and equal timing-column gaps.
UI and C++ physical line totals are unchanged; 15 test-driver lines were added
since the inverse-order source. JS syntax and source-manifest checks passed.

Publication verification is not guaranteed to take two or 16 seconds: those are
only the two recorded check-start-to-proof intervals. Public receipt availability
can require more waiting. The full publisher-finish-to-proof observations above
also include the stall and manual reconciliation and are not pure proof timings.

## Dependency-level presentation correction

The owner clarified that literal reversed creation order is insufficient: creation
visits dependencies per target and therefore interleaves join/side/app blocks.
The requested page order is reverse breadth-first dependency levels, independent
of targets and actual GitHub execution timings. The UI derives each row's level
from its exact immutable prerequisite IDs, using the deepest prerequisite level
for joins. It sorts the filtered presentation copy by descending level, preserving
newest committed creation order within a level. All graph rows contribute their
levels before filtering, so hiding prerequisites does not change a dependent's
position relative to other visible work. This transient presentation calculation
has no readiness, selection or scheduling authority.

No controller C++ or GitHub activation logic changed. A join can still dispatch
as soon as its own exact prerequisite publications are verified; no whole-level
execution barrier was introduced. No runtime restart or new GitHub mutation was
needed. UI line count remains 452: consolidating timestamp property initialization
offset the display-level calculation. C++ remains 2,376; the existing test assertion
was replaced with the newly specified exact dependency-level order, without losing
the existing same-state progress/failure stability assertions.

Native/runtime/Chromium checks passed in 6.00 seconds. Live 8080 verified all 50
rows: ten join executions first, 25 app/side executions next and 15 core executions
last. Every exact prerequisite appears below its dependent; a suite filter
preserves the same relative ordering. The first ad-hoc filter probe used an
unsupported combined distribution-suite search string; the final check used the
supported suite query. The source manifest above identifies the final presentation
and tests; the running controller binary and process remain unchanged.

## Between-phase delay column

The expanded timing table now has Job, Started, Finished, Duration and Delay.
Delay is the row's actual start minus the preceding row's actual finish. Build's
delay is empty. Missing timestamps show Not recorded; queued placeholder starts
are excluded, and an overlapping phase displays a signed negative delta. Duration
retains its existing missing/invalid handling. The compact main table is unchanged.
The detail table's small-screen layout now accommodates all four data columns.

For the owner's screenshot execution, publisher delay is 2 min 44 s and independent
verification delay is 21 s. These intervals include all work and waiting between
the two timestamps, including GitHub queue/setup or proof availability; they do
not isolate controller overhead. Existing source timestamps are used without any
controller, persistence or workflow change. Consolidating DOM element property
initialization offsets the additional rendering lines; UI remains 452 lines.
Focused native/runtime/Chromium checks passed in 4.53 s, including an empty first
delta, a positive delta, negative overlap, missing data, unchanged durations and
four aligned timing-data cells at a 560-pixel viewport. Test drivers add 12 lines;
test C++ and production C++ counts are unchanged. JS syntax passed.

## Published-package navigation and aligned actions

Published execution details now link each package filename directly to its path
under the verified receipt's public HTTPS feed base. Path segments are encoded;
unpublished attempts show no package links. The existing action row vertically
centers links with Reconcile. Reconcile's behavior is unchanged: observe remote
evidence, apply verified facts, and allow normal committed transitions to release
eligible work, without replaying an assignment.

UI remains 452 physical lines, production C++ 2,376 and test C++ 798. Test drivers
add 14 lines (609 total). Native/runtime/Chromium passed in 4.53 s with exact
receipt link assertions, unpublished-link exclusion and action-center checks;
the populated screenshot was inspected. JS syntax passed. A live receipt-selected
IPK HTTPS HEAD returned 200 (4220 bytes). Live assets match source. No new chain
run, live reconciliation, restart, commit or push was performed for this UI edit.

## Packages link correction

The filename links described above have been replaced: published build details
now show exactly one Packages link to their directory on GitHub, pinned to the
verified feed commit. No package names are displayed in this action. Multiple
files still produce one link. Unpublished executions show none. Production line
counts and action alignment are unchanged; the GitHub-only link policy is restored.
Final source manifest SHA256: 32662c845209ec4c657724c1e0ad581cf83d32dbd47c280f203de639706523c2.
Focused native/runtime/Chromium passed in 6.31 s; JS syntax passed. Live JS matches source.

## SSE heartbeat activation

The owner authorized the transport-only heartbeat and inspection of the previous
heartbeat block. A native SNode.C intervalTimer emits an SSE comment every 30
seconds per connected stream. Its weak response capture does not retain a closed
stream; disconnect cancels the timer and removes the response immediately,
replacing deferred broadcast-time pruning. The SSE connection's read-idle timeout
is disabled through the installed public SocketConnection API because comments
alone did not prevent the observed 60-second read timeout. Write protection and
ordinary HTTP/webhook timeouts remain unchanged. No graph polling, scheduler,
extra thread, state transition, UI data event or framework modification is added.

The initial frame probe was corrected to accept the framework's valid extra CRLF
separators. The first duration test then reproduced a close at 60 seconds despite
outbound heartbeats. After the SSE-only read-timeout correction, the complete
native/startup/HTTP/MariaDB/Chromium check passed in 95.89 s. One idle connection
received comments at 30, 60 and 90 seconds, with no graph revision change. Existing
startup-failure, shutdown, SSE progress, ordering, timestamp and action-link checks
also passed. Formatting, 160-column and Python syntax checks passed.

Production changes: main.cpp +17 physical lines, all other C++ files unchanged;
UI unchanged at 452 lines. Per-file C++ counts: Auth.cpp 137, Auth.h 17,
Controller.h 94, Evidence.cpp 346, Execution.cpp 396, GitHub.cpp 310,
Graph.cpp 427, Http.cpp 130, Managed.cpp 161, Managed.h 24, Store.cpp 97,
main.cpp 254: total 2,393 / 2,500. Tests: runtime.py +20 lines, drivers 632 total;
C++ tests unchanged at 798. Documentation: AGENTS.md records the explicitly
authorized heartbeat exception; this report and WORK_LOG.md record the checks.
No workflow edits, new tag/build chain, replay, commit or push.

The local sandbox controller was cleanly restarted with its existing deployment
and secret-file paths against the existing managed database. Exact graph equality
before/after startup was verified. PID 221052, foreground tool session 43795;
binary /tmp/codex/build/core-ci/controller/core-ci, SHA256
32e95cd8b83355d879be7ed2f3feeb7dd517638ba332729c6b2b1bf6e9befa02.
Checkout /home/voc/projects/snodec/core-ci-new, unborn feat/graph-controller.
Final source-manifest SHA256:
655b72a814518fee20b3e1077ee272dcbc0fbedfb838a9c963e1e179022dfa61.
The separately observed live SSE connection received comments at 29.883,
59.897 and 89.894 seconds without reconnecting; its graph was unchanged afterward.
