# Work log

## 2026-10-31: fresh repository and external contract extraction

The owner changed workspace scope to an independent repository at
/home/voc/projects/snodec/core-ci-new, branch feat/graph-controller. The reference
checkout is read-only input for the specifically permitted interoperability
extraction; no existing source or dirty work was overwritten. No remote or
commit exists in the new repository. No live mutation was authorized or run.

The governing invariant is one durable MariaDB graph authority, immutable exact
selected inputs and prerequisite attempts, successfully committed authority
before GitHub mutation, and publication proof before dependent release.
Unknown outcomes preserve their assignment across restart without replay.

Provisional production C++ budget: graph/state/persistence 900, GitHub
authentication/dispatch/evidence/proof 800, bootstrap/management routes 500,
shared declarations 150: 2,350 physical lines with 150 lines remaining under
the 2,500 cap. This estimate is not a demonstrated fit and must be revised from
actual formatted physical counts. No old controller implementation was inspected.

First extracted the permitted external contracts into docs/contracts.md, with
observed-file hashes and unresolved rules distinguished from observed behavior.
Read public installed MariaDB client/command-sequence and SNode.C APIs/examples
as allowed framework inputs. Read-only GitHub access confirmed that demo-app's
committed definition bytes match the local definition example. No workflow,
tag, feed, credential, service or database changes were made.

External contract gaps identified in docs/contracts.md prevent dependent
selection/proof implementation. Questions requesting those rules remain pending.
Runtime ranges remain source-owned package metadata; no new definition range
syntax or additional source-actor allowlist has been introduced.

After documenting contracts, implemented the independent standard GitHub
authentication functions from public GitHub/OpenSSL interfaces. No controller
network operation, selection, database or proof logic was implemented against
an assumed missing rule. This is an incomplete implementation checkpoint.

Validation of the new source files:

- GNU C++ 16.2.0, Debug, OpenSSL 3.6.5, nlohmann_json 3.11.3.
- Build: /tmp/codex/build/core-ci/fresh-auth, configured from this new checkout.
- Built auth-contract-test; CTest github-authentication-contract passed (1/1).
- Official GitHub HMAC vector, modified-body/wrong-key/missing-signature rejection,
  independent RSA JWT verification/claims, canonical UTF-8 JSON and Git/SHA-256
  identities exercised. Test RSA private material is generated only in memory.
- format-check passed; all eight baseline configuration/module files are byte
  copies of ~/.codex/formatting. No changed production line exceeds 160 columns.
- Formatted physical production counts: src/Auth.cpp 137, src/Auth.h 17,
  total 154. Test code: tests/auth.cpp 93 lines. No hidden production C++ exists.
- No MariaDB, populated management UI, remote execution or live sandbox acceptance
  has been tested. No complete controller executable exists at this checkpoint.

The temporary build contains only generated test artifacts and may be removed
after the source/build identity is recorded. No additional checkout was created;
there is no commit, remote, push, PR, installation, activation or deployment.

Exact SHA-256 identities of the files compiled by that build:

| File | SHA-256 |
| --- | --- |
| src/Auth.cpp | d362ab1bde54087fd28010b0bbf3ae94b3be57a7b279e10642a3c29fe9dc5543 |
| src/Auth.h | 2613df5c35eeb5c1fdbed13ef8aab74c303fe0b296b13de5dd7e79f416e2daa0 |
| tests/auth.cpp | df596512dd96769c385caf3f4086d34709c3ab61c41df266a14c6a21395785b5 |
| CMakeLists.txt | 8bd61dfada6fac0fe58b76fd7aa39fb4d532a998005f03f3e9dc7bf7f5312f33 |

Removed /tmp/codex/build/core-ci/fresh-auth after recording successful checks
and these identities; it held no source changes or database state.

## 2026-10-31: graph controller implementation checkpoint

The owner resolved both contract gaps: highest SemVer prerequisites for older
explicit requests, and immutable execution-receipt proof independent of later
shared index changes. Recorded them in docs/contracts.md before implementing
dependent behavior. No reference controller architecture or tests were read.

Implemented one durable InnoDB graph row with acknowledged compare-and-swap
updates, immutable selections/attempts, commit-before-dispatch, exact prerequisite
receipts and atomic publication/dependent release. No restart replay, polling,
ready queue, quota, reservation or feed lock exists. Source duplicates reuse
committed selection facts without re-resolving newer prerequisites. Explicit
reconciliation observes only; definite failure retry and acknowledged retirement
preserve immutable history. Run/job/step facts accumulate despite out-of-order
visibility, while timeouts remain unresolved rather than definite failures.

Implemented App JWT/scoped installation tokens, raw-body HMAC webhooks, exact
run/workflow/actor/job/artifact binding, bounded archive validation and independent
committed/public HTTPS receipt comparison. Frozen wire deployment pins exclude
unrelated repository grant lists. Run evidence uses the frozen actor identity;
dispatch acknowledgements cannot replace an existing run binding. Mutable index
membership/bytes are not publication proof.

Implemented native asynchronous REST/SSE delivery and a fresh presentation-only
8080 UI. Real Chromium fixtures verify dynamic repositories, local dates, compact
stable State width, centered grouped headings, multiple expanded rows, prerequisite
expansion and scrolling, retirement, and within-state progress updates.

Runtime review corrected a repeated configuration-final callback. The owner
then directed inspection of the previous main.cpp specifically for framework
usage patterns, and confirmed small nonblocking database ownership is allowed.
No other old controller source was read. Bootstrap now configures named instances
before init, calls init once, starts database work directly, and configures
routes/listeners only after initial database success. Removed deferred startup.
Error reporting uses the public SNode.C logging API.

Managed database readiness and child exit use public SNode.C ReadEventReceiver
tasks and their native shutdown lifecycle. The server starts with no defaults,
the operator-supplied initialized datadir, a Unix socket, and networking disabled.
The standard filesystem READY=1 notification works with the installed MariaDB;
an earlier isolated abstract-socket probe had failed with EACCES. No host policy
or framework was modified. There is no blocking wait, polling, timer, retry,
explicit SNodeC::stop or controller thread. The owner corrected my interpretation
of single tasking: counting all native OS threads was the wrong requirement.
All controller work stays in the one SNode.C event loop.

Real isolated tests cover early child exit, managed initial schema failure,
managed success/shutdown, initial unmanaged connection/schema failure, and
unmanaged server preservation. The browser checks pass against the real 8080
listener, including changing within-state facts. All six focused CTest entries
pass. The final build/format and exact source identity are recorded separately.

Current accounting: 2,384 formatted production C++ lines (116 below the cap),
729 test C++ lines, 529 Python/JavaScript test-driver lines, plus presentation
assets and separate documentation/tooling. Per-file counts and exact checks are
in docs/validation.md and docs/source.sha256. The 190-line production growth
implements mandatory managed ownership and the corrected bootstrap pattern.
No full-system or speedup claim is made. Live sandbox acceptance remains
separately unauthorized; no production workflow/feed/tag/key/service was changed.

Automatic review rejected relaxing exact workflow-commit binding because it
could accept unrelated commits. The patch was not applied; exact commit and blob
checks remain. No commit, push, PR, merge, installation, deployment or live run
occurred. All changes remain in this checkout on feat/graph-controller. Temporary
build/browser artifacts are retained under the approved task build root for
reproduction and handover; generated databases/keys/processes were removed.

## Authorized live sandbox acceptance: preparation

The owner authorized running the VOC-Experiments acceptance and using Jupiter's
existing port forwarding. The supplied rule forwards IPv4 TCP WAN 8443 to
192.168.1.239:8443; Jupiter owns that LAN address. At inspection neither local
8080 nor 8443 had a listening process. The documented runtime credential paths
under /etc/core-ci and service credential directory are absent on Jupiter;
only existence/ownership metadata was checked, never private file contents.
The existing Jupiter launch command/configuration location remains requested.

Public sandbox repository/workflow metadata and the coordinator's explicitly
named non-secret identity/feed variables are readable. The standard source and
publication workflows currently declare tooling e7b1423cc9537070f84aea7bf7893fb590f75301;
multiple other workflow variants also exist. Historical documented pins differ,
so those historical values were not adopted as current deployment inputs.
No credentials were inspected, created or changed. No GitHub workflow, webhook,
tag, feed, service or database mutation and no live run has occurred.

The documented public HTTPS endpoint answers HEAD with HTTP 404 and SNode.C
headers over IPv4; this alone does not identify the new controller or prove
delivery to its local listener. It must be verified against the new process
after launching with the existing approved runtime settings.

## Live acceptance executed on Jupiter

The owner directed searching the existing local setup rather than asking for
its paths. Located saved runtime configuration and /home/voc/ci-test credentials.
Extracted only public deployment identities/pins and file paths for operations;
no secret contents were printed or used as controller design inputs. Existing
private files were consumed directly by cryptographic authentication and TLS.
No credential was copied into this repository, created or rotated. Existing
databases and their configuration were not needed or changed.

The saved certificate covers gw-home.vchrist.at. Reused the supplied IPv4
8443 forwarding, verified that hostname over HTTPS and changed only the
VOC-Experiments controller App webhook URL to its /github/webhook endpoint.
SSL verification stayed enabled. A real GitHub authenticated notification
returned 202; an unsigned POST returned 401. No workflow source changed.

Started this development binary on Jupiter with a fresh isolated managed
MariaDB under /tmp/codex/build/core-ci/acceptance. Enrolled four committed
definitions, then requested demo-app once. Selection froze app v2.1.0 and
the highest library tag v2.1.13 across five targets. All five ready library
builds dispatched; only exact prerequisite proof released dependent targets.

Live discovery corrected three controller issues. Public feed proof reads
exhausted the anonymous API quota; the existing scoped installation token
already had permission to read that public feed, so proof now uses it with no
grant/permission expansion. Native restarts exposed missing socket reuse;
adopted SNode.C setReuseAddress before init and consolidated release callbacks
as in the permitted main pattern. Finally, the frozen publisher's IPK filename
differed from the local serializer example. Removed controller derivation of
publisher-selected filenames while retaining safe paths, exact file/path,
assigned format, package identities and manifest digests. Added focused positive
and negative filename/path cases. These corrections have zero net production
C++ growth and add sixteen test C++ lines. Current total remains 2,384 / 2,500.

Build and canonical format checks pass. Authentication/TLS/graph/persistence
checks passed after proof authentication; native startup/browser passed after
reuse/cleanup; graph/receipt, archive and persistence passed after filename
validation. Before/after real restarts preserve all ten execution/attempt/input
identities and assignments. Independent GitHub audit found exactly fourteen
matching runs for fourteen committed commands: no duplicate dispatch effects.
No assigned command was replayed. Explicit reconciliation observed original
publications after restart; normal proof transitions then authorized dependents.

Both IPK/APK library and dependent application publications reached independently
verified published: the required complete chain is demonstrated for both paths.
After the later application changed each shared index, the earlier library
receipt still matched its original committed bytes and public HTTPS bytes.
No current index was fetched as proof. Native publishers definitely failed in
preparation with 'deployed verification key differs; rotation needs owner
approval'; mutation steps were skipped and their three dependents remain waiting.
No rotation, bypass, retry or replacement was attempted. Full-system readiness
is not claimed.

Real Chromium checks passed with ten live executions, dynamic repositories,
multiple expansion, prerequisite navigation, GitHub links and browser-local
verification dates for all four published rows. Runtime state is four published,
three definite failed and three waiting. Timings are separated in
docs/sandbox-acceptance.md; setup, signing/preparation and HTTPS/Pages waits remain.
Diagnostic restarts added delay; no overall speedup is claimed.

The controller remains running on Jupiter (PID 73110), management loopback 8080,
authenticated webhook on public 8443. Its source/binary identities, native failure
links, successful paths and preserved runtime directory are recorded in the
validation and acceptance reports. No commit, push, PR, installed service, new
tag, production SNodeC mutation or existing database reset/migration occurred.
The isolated acceptance database now owns this instance's durable history;
preserve it until the instance is explicitly retired. Test observers exited.

Final startup review added the native required-listener error boundary: webhook
startup follows management-listener success; bind failure releases initial
resources through the existing cleanup callback. This adds two production lines
and ten Python check lines, with no state, retry or lifecycle mechanism. The
first test invocation hit a false-positive availability probe on TCP TIME_WAIT;
the probe now uses the same socket-reuse semantics as the application. The full
native startup/browser entry then passed, including an actually occupied 8080
port, no webhook startup and owned-server release. Final production count is
2,386, test C++ 745 and test-driver Python/JavaScript 539. The final live restart
preserved the entire graph byte-for-byte and started no new remote work. The
current foreground PID is 80071; final identities are in docs/validation.md.

The remaining dispatch diagnostic callback now uses SNode.C logging too, with
no line-count growth. Final build and format-check pass; the running binary
identity matches the recorded build, and the entire graph is still unchanged.
Final foreground PID: 81491.

### Continue complete sandbox chain, 2026-10-31

Owner requested completion of the full chain after partial OpenWrt acceptance.
The native publication failure was traced through public workflow inputs and
public-key serialization only: old pinned tooling contains the expired GPG
certificate; the feed contains its renewed self-signature with the same primary
fingerprint. Existing `*-renewed.yml` workflows pin tooling with the exact deployed
certificate. Their relevant wire contracts were compared and documented before
use. The invariant is immutable selected identities: existing work remains intact;
new explicit work selects the already committed renewed pair. No production code,
workflow, key, feed trust or validation guard is changed for this correction.

The full RPM join exposed an invented 60,000-byte assignment restriction in the
fresh graph transition. Its real exact assignment is 62,053 ASCII characters
(62,247 summed dispatch input value characters). That guard rolled back a valid
app publication proof together with its newly ready join. The governing invariant
is that verified publication and dependent authority commit together; GitHub owns
its external dispatch acceptance limit. Deleted the artificial graph check
(one production line removed); existing definitive REST rejection handling remains.
The isolated MariaDB boundary regression now commits a >60,000 and <65,535 byte
exact receipt assignment and checks proof/authority visibility before dispatch.
Graph/receipt and real-MariaDB commit/restart tests passed, as did build and format
check. Test C++ grows fourteen lines; no production mechanism is added.

GitHub rejected that full RPM join assignment with definite HTTP 422. The
prerequisite proof nevertheless committed correctly after removal of the guard.
Worker contract extraction confirms dependency build inputs do not consume the
shared-index inventory; environment remains required for OpenWrt. New assignments
omit only indexes from embedded prerequisite input receipts, preserving complete
original receipts/proof in MariaDB. No callback, retrieval or artifact protocol
changes. Retry assignment construction was consolidated into normal commands:
source/target/prerequisite execution IDs are frozen, and their published attempts
are terminal, so rebuilding an assignment cannot rebind consumed input facts.
This deletes duplicate retry construction and preserves the old assignment.
Graph.cpp is now 422 lines; total production C++ is 2,377 (nine fewer than this
continuation's starting point). Focused graph/receipt and isolated real-MariaDB
commit/restart checks pass again. Tests now additionally assert complete durable
large receipts, small dispatch inputs and exact dependent inputs across retry.

### Full acceptance and copied management UI

The renewed selection completed all twenty executions across four repositories and
five targets, including all five three-prerequisite joins. Independent GitHub and
HTTPS audit passed: forty unique successful exact App runs, twenty original-commit
receipt byte matches and fifteen earlier-receipt/later-index comparisons. Earlier
failed/waiting selections remain intact; no unknown mutation was replayed.
The RPM join definite-rejection retry preserved the original assignment/history
and exact consumed prerequisite facts and successfully published as its new attempt.
Native confirmation in the existing renewed workflow is skipped, so explicit
reconciliation was used. This is full sandbox-chain acceptance with that supported
operation, not proof of unattended native confirmation or production readiness.

The owner then explicitly requested copying the previous UI and layout. Inspected
only reference ui/index.html, ui/style.css and ui/app.js under that new permission;
no old controller logic was read or changed. Observed source SHA-256 values:
HTML 63bea118e032fd65c23be88159dc628cb06a611a413ef0ddf9f538124407f77a,
CSS 2ce0c3fed12a7424c2022f5a23eb2a25c9c843c5b5b7a077bbaa929305302578,
JS 5d3c36a902ddb0c193cc3f30ba8155c03b19163a4f711fc11c8d75b038ba8585.
Copied tabs/table/headings/compact columns/repository view and adapted presentation
to the fresh graph and per-execution REST endpoints. Filtering/display ordering
has no graph or scheduling authority. Old supersession flags, mutation flags and
retry-after-retirement wording were not imported. There is no definition editor.

GitHub sends job events rather than continuous step telemetry. The UI now labels
step information Last reported and shows Awaiting public receipt proof after a
successful publisher job. Browser-local times use the locale hour format; short
displayed times prevent overlap in the copied narrow columns, with seconds in
tooltips. No polling timer, callback protocol or invented progress was added.
The copied UI is 454 presentation lines (231 added with the owner's explicit copy
instruction); production C++ is 2,377 (nine removed). Test C++ is 773 (+28); test
drivers 555 (+16). All changed production lines remain at most 160 characters.

Live Chromium passed 30 rows, 24 published, five joins, four repositories,
search/filter, tabs, read-only definitions, multiple expansion, prerequisite scroll
and zero page errors. The updated native/bootstrap/UI test passed with isolated
MariaDB and real Chromium, including same-state SSE progress and real retirement.
Tests use ordinary named-instance CLI ports 18080/18443, leaving the live
8080/8443 instance and database untouched. A port-edit SyntaxError in the test
driver was fixed; final test passed in 4.05 seconds after the locale adjustment.
Build/format and node syntax checks passed. Completed test-only SSE observer and
browsers were stopped. Live PID is 107024, binary SHA-256
37484576f4c06ffa5f0212e137b829e18413eec00f58a5e16b4eab1a4905707c.
Exact results, accounting, timing boundaries and limitations are in
 docs/validation.md and docs/sandbox-acceptance.md. No performance speedup claim,
commit/push/PR/merge, installed service, workflow source edit, key rotation or
production change occurred. Preserve the active acceptance database and history.
# Monitored v2.1.14 rerun and small UI correction

Owner authorized the concrete new sandbox tag/full chain. Twenty selections
reached verified publication through 40 successful distinct assigned runs.
Monitoring exposed an actual five-second transient runner-start regression and
a stale-snapshot update retry backlog. The graph update now uses one native
SNode.C command sequence for its read/write; optimistic retries were removed.
Out-of-order merges preserve phase timestamps. Restart preserved all assignments;
explicit reconciliation observed outcomes without replay. A Chromium observation
gap and both clean restarts prevent claiming uninterrupted timing acceptance.

Owner rejected added columns and approved only a one-line ellipsized state
description/full hover plus Started/Finished in the existing verification column.
Implemented those small changes and recorded the first actual proof-check time;
older missing starts remain unrecorded. Net production C++ decreased by one line
to 2,376; UI decreased by two. Focused persistence/evidence and native/browser
checks passed. Final binary 6d6c8cb16d64f05767aeaf5061206d2711841dc81a8d5c13d0fd412a68a65abc,
foreground sandbox PID 177384. Detailed timing, accounting and limits:
docs/monitored-rerun.md. Inverse fixed logical-chain ordering is a subsequent
owner proposal, not yet implemented at that measurement point. No commit/push
or production mutation.

## 2026-10-31: inverse UI order approved and verified

The owner explicitly approved reverse committed creation order, keeping newer
work above older work and dependents above prerequisites through every state.
Changed only the UI's filtered array to reverse order; the authoritative graph
and readiness stay unchanged. Native/runtime/Chromium checks passed in 4.12 s,
including exact recorded verification-start rendering and stable order during
same-state progress and a real retirement. A live Chromium read verified all 50
row IDs against reversed committed graph order: the six original waiting/failed
rows are last. C++ remains 2,376; UI remains 452; tests add three driver lines.

Only two of 44 published executions have a recorded actual controller proof
start. The other 42 predate recording. Inspected the approved sandbox publication
workflow; its separate receipt-confirmation job cannot supply that missing
controller timestamp, and native confirmation jobs were skipped. No historical
start, publisher-finish substitution or CI protocol was invented. Future checks
record actual starts. No workflow edit is needed for that controller-owned fact.

Exact REST-ingress-to-outgoing-dispatch timing was not captured. Recomputed the
closest post-recovery GitHub delivery/run-creation proxy using the exact assigned
build job or completed run: five samples, median 6.896 s, range 5.634–9.471 s.
It includes transport/acceptance and lacks exact causal ingress attribution;
it is not exact controller overhead. docs/monitored-rerun.md preserves this limit.
Source manifest a373ab6d68fa21da32022888dc8c67af848901abbf0953e13fc41fceb15f6445;
running C++ binary unchanged, PID 177384. No commit, push or production change.

## 2026-10-31: compact timestamps and proper prerequisite links

Owner requested seconds, then clarified 24-hour time above date and compact
columns. Locale-only account inspection found LANG=en_US.UTF-8, LC_TIME=C and
Europe/Vienna timezone. Browser English defaults had selected 12-hour display;
the page now explicitly follows the owner's 24-hour request while retaining the
browser-local timezone. No account configuration was modified. The owner withdrew
the Qt request. Build/Publisher are restored to 184 pixels; only verification
grows from 156 to 184, matching their 72-pixel slots and 8-pixel inner gap.
Time is 13-pixel bold, date is below, and fixed row/order behavior is retained.

Removed the unrelated 240-pixel button width policy that wrapped long dependency
labels. Prerequisites are now actual anchors, with real fragment hrefs and the
existing expand/scroll action. Chevron-only styling replaces the obsolete shared
row-select class. All three OpenWrt join prerequisite labels fit on one text line
in the live page. An initial ad-hoc check compared a flex-stretched anchor box to
text line height; measuring actual text Range rectangles established one line.
That probe's browser/process was stopped. The permanent native/runtime/browser
checks passed in 4.77 s after this change, including anchor semantics and
navigation. Timing checks passed in 5.39 s before the anchor-only change. Live
English/Vienna, de-AT/Vienna and English/New York each validated 234 timestamps,
time-above-date, no overlap, correct local conversion and matching timing gaps.

Production C++ remains 2,376, UI remains 452, test C++ remains 798; test drivers
are 583 (+15 since the inverse-order change). Documentation updates describe the
final approved layout and measurement limits. JS syntax passed. Current source
manifest db28eeeffc540c17cc3d33d0fcc02636c432a81dff57f4e7e2611adff75cfc0c.
Binary SHA-256 6d6c8cb16d64f05767aeaf5061206d2711841dc81a8d5c13d0fd412a68a65abc
and foreground PID 177384 are unchanged; static assets are served directly.
No new GitHub run, workflow edit, restart, commit, push or production change.

## 2026-10-31: reverse breadth-first dependency display, presentation only

Owner clarified that the website must follow logical dependency levels, reversed:
joins above all app/side work above core, rather than reversed depth-first creation
blocks, target groups or GitHub timing order. The UI derives transient levels from
the authoritative immutable prerequisite IDs. A join's level follows its deepest
prerequisite, including when it also has a direct root edge. Rows sort by descending
level with reverse committed creation order as the stable tie order. Filtering
does not remove prerequisites from the level calculation. States, runner times and
progress do not participate in ordering.

The owner explicitly reiterated that real controller trigger activation must not
change. No controller C++, readiness traversal, assignment transaction, dispatch
or workflow code was edited. No new execution barrier, restart or GitHub mutation.
The website holds presentation order only. UI remains 452 physical lines: one
timestamp property-initialization consolidation offsets one added level-calculation
line. Production C++ remains 2,376 and test line counts are unchanged. The previous
reverse-array assertion was updated to the approved dependency-level order; all
existing state/progress stability and navigation checks remain.

Native/runtime/real Chromium checks passed in 6.00 s. Live 8080 checks passed for
50 rows: ten joins, then 25 app/side, then 15 core. Every prerequisite is below its
dependent; suite filtering preserves the order. A first ad-hoc probe used an
unsupported combined distribution-suite query; the corrected supported suite
query passed. JS syntax and the updated source manifest passed. Manifest:
de6d8d1d2216be512d7a9dc489ea170708159fe4970667da58e6b63316667ba4.
Controller binary 6d6c8cb16d64f05767aeaf5061206d2711841dc81a8d5c13d0fd412a68a65abc
and foreground PID 177384 unchanged. No commit or push.

## 2026-10-31: continued check in the owner's live Chrome tab

Checked the existing http://localhost:8080/ Chrome tab through the browser
extension's accessibility and screenshot APIs, rather than another test browser.
It already has the final presentation loaded: ten join rows, then the app/side
level, then core; times are 24-hour with seconds above dates and compact timing
pairs. The authoritative page reports 44 published, three failed and three waiting,
with no building or publishing work. The page was not reloaded or modified and
no execution/reconciliation/retirement action was sent. A DOM/CDP read timed out;
the existing supported accessibility and screenshot surfaces provided the actual
visible result. Corrected superseded ordering wording in the acceptance report.
No production or test code changed, no repeated suite, no new GitHub mutation.


## 2026-10-31: remove prerequisite separators

Removed only the prerequisite list border-bottom declaration as requested.
Live Chromium verified three prerequisite entries with 0px bottom borders.
C++ remains 2,376 lines; UI remains 452; no test-source or controller change.
Source manifest 08ba31d1c89b04529b1429818257cd7316b84558d57a3d6ccb5b615c11b3532e.
The owner-provided untracked screenshot in the repository is preserved.
No restart, database mutation, GitHub action, commit or push.

## 2026-10-31: detail timing Delay column

Added the requested Delay column only to the expanded execution timing table.
It uses actual next-start minus previous-finish facts; the first Build delta is
empty. Missing data stays Not recorded, queued placeholder starts are excluded,
and real overlap is shown as a negative interval. Duration preserves its former
missing/invalid handling. The owner's example yields publisher 2 min 44 s and
verification 21 s. These are whole between-phase gaps, not isolated controller
overhead. Main columns, dependency ordering, controller and persisted facts stay
unchanged. The responsive detail row now accommodates four data columns.

Consolidating the DOM helper's property initialization offsets the added timing
rendering lines. Production UI stays 452, C++ stays 2,376, test C++ stays 798;
test drivers become 595 (+12). Native/runtime/Chromium checks passed in 4.53 s:
first delta blank, signed overlap, positive/missing values, unchanged durations,
and 560-pixel responsive alignment. JS syntax passed. Source manifest:
a1d0abd2e293403ac82b351cd455a86f729546ed6f4a36b3e024213fb5343145.
The owner-provided screenshots remain untouched. No runtime restart, GitHub
mutation, production change, commit or push.

## 2026-10-31: published package links and action alignment

Invariant: package navigation uses the active published attempt's independently
verified receipt, with no new selection, proof or scheduling authority in the UI.
The existing link renderer now also accepts that receipt's trusted HTTPS base;
each receipt path segment is URL-encoded. Published package filenames appear as
direct links alongside GitHub links. Unfinished attempts expose no package links.
The existing action flex row now centers links and buttons vertically. Reconcile
still observes GitHub/public proof and applies verified facts through the state
machine; it never replays an assignment, but newly eligible commands can follow
committed transitions. No controller, workflow or persistence changes.

Consolidating the existing link helper offsets package rendering: UI stays 452
physical lines, C++ stays 2,376; test drivers add 14 lines (609 total), test C++
stays 798. Documentation changes are this log, monitored report and source hash
manifest. Focused native-startup-http-contract passed in 4.53 s, including real
MariaDB, Chromium receipt URL/label assertions, unfinished-row exclusion, action
centering and prior timestamp/SSE/order checks. JS syntax passed. Inspected the
populated Chromium screenshot. A public HTTPS HEAD for a receipt-selected live
IPK returned 200, Content-Length 4220; live app.js/style.css match source.
No live reconciliation, dispatch, restart, database write, commit or push.
Checkout remains /home/voc/projects/snodec/core-ci-new, unborn branch
feat/graph-controller. Running binary unchanged: SHA256
6d6c8cb16d64f05767aeaf5061206d2711841dc81a8d5c13d0fd412a68a65abc.
Source manifest SHA256: 084add6793b2e165c590b70ae12bf6d32aec471c611d3e9c726a852af45676ed.

## 2026-10-31: one GitHub Packages directory link per build

Replaced per-package filename/download links with exactly one Packages link for
published work. The directory comes from the verified receipt, and the GitHub
URL is pinned to its independently verified feed commit. Multiple package files
in that build do not create extra links. Restored the existing link helper to
GitHub-only validation; no alternate feed-link policy remains. Action alignment
is preserved. Production UI remains 452 lines; C++ remains 2,376. Test driver
changes are +3 lines relative to the filename-link revision (612 total); test
C++ remains 798. Browser coverage checks the exact directory URL and label,
unpublished exclusion, and one link after multiple receipt packages arrive via
SSE. No controller/state/trigger/workflow changes or live mutations. Branch
feat/graph-controller remains unborn; binary unchanged; no commit or push.
Source manifest SHA256: 32662c845209ec4c657724c1e0ad581cf83d32dbd47c280f203de639706523c2.
Final native/runtime/Chromium test passed in 6.31 s; JS syntax passed.

## 2026-10-31: SSE transport heartbeat

The owner explicitly authorized the heartbeat and inspection of only the previous
heartbeat block. Its public SNode.C intervalTimer/weak-response/disconnect-cancel
pattern is used here. Invariant: idle SSE transport stays active without creating
graph facts, scans, dispatches or browser data events. Each stream emits the SSE
comment `: keep-alive` plus a blank line every 30 seconds. Disconnect immediately
cancels that stream's timer and removes its response, replacing deferred pruning
on graph changes. No thread, graph timer or remote polling is introduced.

Reduction alone cannot make an idle transport send bytes; reshaping the old
notification-only sender cannot provide periodic transport liveness. The owner
requested the addition immediately. main.cpp adds 17 net physical lines, becoming
254; all other production C++ files stay unchanged, total 2,393/2,500. UI remains
452 lines. Tests extend only the native runtime public boundary with a real idle
HTTP connection and isolated MariaDB. Its first probe exposed the framework's
additional CRLF frame separators; the probe now accepts valid empty SSE lines
while still requiring timed comments and the same open connection.

The first actual idle-duration run received a heartbeat but still closed at 60
seconds: outbound comments do not reset the separate inbound-read idle timeout.
The SSE handler now disables only that established stream's read timeout through
SNode.C's public SocketConnection API; write timeouts remain unchanged. Ordinary
HTTP and webhook listeners retain their timeout policies. No framework changes.

Final focused native/runtime/MariaDB/Chromium check passed in 95.89 s, including
one connection across three 30-second comments and unchanged DB revision.
Formatting, <=160-column and Python syntax checks passed. Live sandbox PID
221052 (foreground session 43795) now uses the rebuilt binary with unchanged
startup inputs and exact retained graph. A separate live idle connection received
comments at 29.883, 59.897 and 89.894 seconds; graph remained identical afterward.
Binary SHA256 32e95cd8b83355d879be7ed2f3feeb7dd517638ba332729c6b2b1bf6e9befa02.
Source-manifest SHA256 655b72a814518fee20b3e1077ee272dcbc0fbedfb838a9c963e1e179022dfa61.
Test runtime.py adds 20 physical lines, drivers total 632; C++ tests unchanged
798. Production C++ 2,393, UI 452. No new chain/workflow/tag, commit or push.
Existing owner screenshots and other untracked source work remain untouched.

## 2026-10-31: source-triggered forward selection, no production growth

The owner identified forward propagation as mandatory for every enrolled source,
and required no code growth. Invariant: source selection creates its transitive
dependents once, reuses exact prerequisite work and freezes every relationship;
readiness remains solely the existing committed graph state machine. One shared
selection traversal is used with committed enrollment definitions for discovery
and frozen source definitions for graph insertion. The initiating explicit tag
is preserved; other repositories resolve highest SemVer once through the existing
request-local source deduplication. No readiness projection, global refresh,
publication-time discovery, queue, timer or name-specific policy was added.

Existing complete-source duplicate checks use standard any/all predicates, and
the transaction reuses that same complete-source predicate instead of a separate
per-target duplicate scan. Exact selectWork identity reuse remains unchanged.
Publication workflow metadata is constructed once, and API array collection uses
the container's range insertion. These changes absorb forward propagation with
zero physical production C++ growth: Controller.h 94->95, Graph.cpp 427->429,
GitHub.cpp 310->307; total stays 2,393. UI remains 452. Tests graph.cpp +57,
store.cpp +3, other test C++ physical counts unchanged: 858 total. Test drivers
remain 632. Fixtures now include the actual committed definition package and
prerequisite fields used by forward discovery.

Graph tests passed (0.05 s): core selects the diamond, root-only dispatch,
duplicate event idempotence, independent branch release, join's final barrier,
app->join only with prerequisite reuse, join-only selection, immutable old work,
and multi-target propagation. Real MariaDB commit/restart tests passed initially
in 0.93 s, then in 1.78 s with the persisted operation itself changed to select
from the root. Rejected/uncertain commits dispatch nothing, committed assignments
survive restart without replay, and release proof/authority are visible together
on an independent SQL connection. Runtime/Chromium passed in 95.97 s, including
90-second SSE liveness, timestamps, order, package action and startup/shutdown.
Formatting and <=160-column checks passed.

The local sandbox restarted with identical runtime inputs and existing database;
no definitions, workflows, keys or original executions changed. The owner-
authorized final chain uses exactly one new core tag v2.1.15 at the existing
fc3acdb94779dafb879c183143c20e19b435e766 commit. GitHub returned the created ref.
No app/side/join build request was sent. Its real authenticated event selected 20
new executions; initially five core building and 15 dependents waiting. Browser
SSE/DOM and read-only GitHub observers started before the tag. Evidence directory:
/tmp/codex/build/core-ci/forward-20261031. Full publication acceptance is pending.

## 2026-10-31: owner removes duplicate publication proofs; show selected trees

The owner explicitly overrode independent public-HTTPS proof, asking to remove
pedantic checks already guaranteed by the authorized publisher implementation.
The controller now accepts the matched pinned publisher job's success and reads
its execution-specific committed GitHub receipt for exact dependent inputs. It
no longer fetches public receipt bytes, compares HTTPS availability/content,
revalidates package inventories/source/environment/signing metadata, or retries
that proof on early publishing progress. Authenticated webhook/run/job linkage
remains, and receipt execution/attempt must match before state advancement.
The successful publisher job is checked before receipt retrieval; the complete
workflow need not wait for its separate public-HTTPS confirmation job. Missing
GitHub receipt remains unresolved without replay. Source and prerequisite facts
are never rebound. The UI phase is Publication recorded; historical evidence keys
and timestamps are retained rather than inventing a data migration.

The UI now shows newest logical selection first, with reverse dependency levels
inside it and exact reused prerequisites included with their latest consuming
tree. Stable selection membership is recorded only for newly inserted work using
its committed creation boundary, never as readiness or priority state. Retained
work uses its original creation facts for presentation grouping. State, progress
and receipt changes do not reorder a tree. Existing selected work and historical
facts are not backfilled. No duplicate execution rows, graph projection deciding
readiness, scheduler, queue or new discovery on release is introduced.

A real Chrome check initially showed the old global-level order and a reconnecting
stream. Reload completed successfully and displayed the corrected newest tree
above old history. Independent server probes: root 1.474 ms, app.js 1.467 ms,
graph 315.237 ms (2.31 MB). This did not reproduce the reported intermittent
spinner and does not establish its cause or resolution. CDP observer recorded
all five new core rows as Building at 15:44:30.784 UTC; they were below old work
in the former order. All five GitHub core builds succeeded. OpenWrt bottom-up
propagation proceeded automatically through app/side and join; native branches
were held by the now-removed HTTPS requirement. No downstream build request was
used. The active test spans the publication-rule change and a restart, so it is
not uninterrupted acceptance or a speedup measurement.

Graph/real-MariaDB tests passed after the publication changes. Native idle-SSE
and Chromium checks (including historical join below current roots) passed, but
the overall runtime test then failed its managed-startup expectation: the new UI
history fixture left eight rows where the later ownership check expected five.
The test now restores its extra presentation fixture rows before returning;
startup/ownership assertions remain unchanged. The corrected runtime check is
pending. No controller startup regression was inferred from that fixture error.

## 2026-10-31: observed crash on abandoned asynchronous SSE response

The spinner investigation subsequently caught an actual SIGSEGV, not merely a
browser rendering issue. The 17:09:16 local coredump stack points to main.cpp:138:
getSocketContext() was null when a delayed Store::read callback ran after the SSE
client disconnected. The heartbeat setup accessed socket lifetime without
checking the still-connected response. Fix: enter setup only for a connected
response, send an error only while connected, and install timer/disconnect cleanup
before sending headers/data. No new flag, wrapper, thread or production line is
added; main.cpp remains 254. This is a response lifetime correction at the source.

The new regression holds the isolated MariaDB table lock, proves the SSE SELECT
is actually pending via information_schema, closes the HTTP client, processes a
normal page request, unlocks, and verifies the controller stays alive. It passed.
The complete native/runtime/Chromium/ownership check passed in 96.11 s, including
this regression and 90-second heartbeat survival. Existing startup/ownership and
UI assertions were preserved. Live endpoint probes later returned root 1.386 ms
and graph 1.340 s under current work; these are observed requests, not guarantees.

The managed DB child also exited after the crash. On recovery its Unix socket was
left stale: connect_ex returned ECONNREFUSED and no server was listening. Only
that confirmed-dead socket was removed to permit the same managed startup; no DB
files, package objects or history were removed/reset. The same DB recovered all
70 durable executions. Observing six already-assigned native publications applied
their recorded GitHub success and released the three native joins normally, with
no replay or new build selection. Real Chrome reload became populated/Live and
showed those joins Building/Publishing, with all core/app/side work Published.
The remaining joins and final audit are pending. This run includes recovery and
operator observations, not unattended continuous acceptance.

The original headless observer page had closed at 16:02:41 UTC, so its later SSE/
DOM record is a gap and must not be treated as measured latency. The observer was
restarted with its original 50-execution baseline retained; live Chrome inspection
was separate. GitHub observation remained read-only. No additional tag, workflow
edit, run resubmission, commit, push or deployment occurred.

## 2026-10-31: approved incremental SSE and bounded direct sending

The owner approved one initial snapshot, then direct field-only updates, with
pending bytes bounded to the initial remainder plus 64 KiB. They approved net
source growth, explicitly excluding live replacement. The architectural failure
was unconditional unchanged writes and full-history broadcasts accumulating
hundreds of MB per SSE socket and overloading the single event loop.

Store now suppresses unchanged writes/revisions/notifications and emits committed
RFC 6902 field patches. main serializes each patch once, accounts for pending
bytes and HTTP framing using public SNode.C counters, and closes over-limit
peers immediately. Heartbeats use the same bound. Initial subscription ordering
and disconnect lifetime remain owned by the existing event loop. The browser
applies field patches to presentation facts; graph readiness and timestamp
meanings are unchanged. No custom pipe/source, extra timer, cache or replay.

Focused real-MariaDB graph/store tests passed. Native pressure testing passed
with a 4.33 MB initial snapshot and 24 ordered ~4.24 KB changes: the slow peer
closed, healthy delivery and fresh-snapshot reconnect continued. Native runtime
and real Chromium passed, including 90-second heartbeat survival and populated,
changing progress/timestamps. A missing test browser-path environment variable
and a test parser's valid CRLF separator handling were corrected; no production
assertion was weakened. Full results, limits and identities are recorded in
docs/sse-delivery-check.md. Production C++ is 2,328 lines (+27); UI JS +19.
Test sources +170 and CMake +8 are separate from production accounting.

No live controller restart/replacement, reconciliation, build trigger, workflow
change, deployment, commit or push. The source fix is not a claim that the live
8080 process has been updated, nor a measured GitHub-chain speedup.

## 2026-10-31: owner-authorized local SSE activation

After the owner explicitly requested "Restart!", stopped old PID 262184 with
SIGTERM and verified its listeners/owned MariaDB process exited. Started the
validated ece3700f binary using the same approved startup argument list, paths
and retained database, as PID 346835 (owned MariaDB 346836). Management 8080 and
TLS webhook 8443 are listening. Full before/after graph equality preserves all
90 executions and immutable authority; initial SSE equals the HTTP graph.
Both existing browser tabs were reloaded and showed populated/Live with 84
published, 3 historical waiting, 3 failed. No replay, manual reconciliation,
new build/tag, workflow change, commit or push. The restart is not a new
full-chain timing acceptance. Evidence and limitations are appended to
docs/sse-delivery-check.md; snapshots are under the permitted temporary task path.

## 2026-10-31: owner-authorized core v2.1.17 full-chain run

Created exactly one VOC-Experiments/demo-core tag v2.1.17 at the already approved
fc3acdb source commit. Read-only GitHub and real Chromium SSE/DOM observers were
armed first. No manual downstream selection, reconciliation, retry, source change
or restart followed. All 20 exact-target executions propagated automatically.
Outcome: 19 published, OpenWrt 24.10 join definitely failed before compilation
with a public prerequisite package HTTP 404. Its app prerequisite's deploy-pages
step was still running at the failure and ended three seconds later. All selected
package URLs subsequently returned 200. This supports an availability race, with
the precise failed URL absent from the log. The publisher/committed-receipt
boundary does not establish public Pages readiness; that gap remains unresolved.
The independent OpenWrt 25.12 join published normally. No automatic retry.

Verified 39 distinct assigned runs (38 success/one failure), frozen inputs and
historical assignments/attempt counts, and all 78 applicable runner timestamps
against browser DOM values. Authoritative final graph equals SSE reconstruction.
Captured 268 patches (largest 41,805 bytes) and two initial snapshots, the second
following the bounded selection-event reconnect. SSE receipt-to-DOM median 28 ms,
maximum 121 ms; last prerequisite receipt observation to dependent authorization
0–4 s (median 1 s). These are observed intervals, not exact isolated controller
CPU overhead or an overall speedup. Full findings/limits: docs/core-v2.1.17-run.md.
Raw evidence and timings.csv: /tmp/codex/build/core-ci/sse-chain-20261031.
Observers stopped; controller remains live. Code unchanged (2,328 C++ lines).
Pagination, production resources, commits, pushes and deployments were untouched.

## Owner-requested Claude publication review (2026-10-31)

- Asked installed Claude Code (`claude-opus-5-5`) for a detailed independent
  read-only evidence review of the OpenWrt 24.10 join 404. Tools/MCP/customizations
  disabled; supplied only current fresh-controller source, allowed external
  contracts and the sandbox run's public/frozen evidence. A supplemental review
  supplied exact receipt checking and native skipped-confirmation records.
- Revised conclusion: public-feed race strongly supported, exact failing URL
  unproven; controller ignores existing confirmation, while the native workflow
  skips it. Controller-only confirmation gating would stall native targets.
- Saved the revised report and integration qualifications in
  docs/claude-publication-review.md. Raw packets/results remain under
  /tmp/codex/build/core-ci/claude-publication-review-20261031.
- Production/test changes: zero. Documentation: new review and this log entry.
  No tests rerun for this analysis-only task; no source/build/runtime change,
  retry, reconciliation, workflow mutation, restart, commit or push. Proposed
  publication-authority/workflow changes remain unimplemented and unapproved.

## Identify the exact prerequisite 404 request (2026-10-31)

- Re-fetched the complete failed job through the GitHub connector and checked
  its only run artifact (pre-download environment). Retrieved the complete run
  log archive, including system and individual step logs. No request trace or
  failed-run workspace is present in available artifacts.
- Confirmed runner-logged assignment equals the persisted frozen assignment.
  Exact sequential downloads: core package, core export, app package, side
  package, side export. Traceback is at package download, not export.
- Exact failed URL remains unrecorded: no request/success log and Python's
  HTTPError string omits its URL. First request is core; app is only an inferred
  failure candidate. Do not assert either was the failed request.
- Documentation: expanded run report and this log entry. Production/test
  changes zero; no tests, retry, reconciliation, source/workflow edit or runtime
  change. Retain evidence in /tmp/codex/build/core-ci/prerequisite-404-20261031.

## Identify the prerequisite missing from deployed Pages revision (2026-10-31)

- Read GitHub Pages deployment history/statuses and exact feed trees. Last
  successful deployment before 18:16:42 UTC failure was side revision 57060f0
  (success 18:15:45). Its tree contains exact core/side IPKs but lacks app r6.
  App revision 5fe31ca contains all three; its deployment success is 18:16:47.
- Therefore app r6 was absent from the last successfully deployed revision.
  This is stronger than timing-only inference, but does not recover the failed
  request URL or prove the exact CDN edge activation instant.
- Expanded run report; retained deployment statuses and full feed trees in
  /tmp/codex/build/core-ci/prerequisite-404-20261031. No source/tests/runtime
  changes, new build, reconciliation, retry or workflow/resource mutation.

## Detailed premature publication release investigation (2026-10-31)

- Traced HMAC ingress -> authenticated pinned run/jobs -> success predicate ->
  committed GitHub receipt -> published state -> exact prerequisite check ->
  successful DB update -> dispatch, plus worker feed consumption and Pages.
- New decisive evidence: committed SSE patch 230 contains app Published and
  join Building while app publish_run and Confirm are in_progress. The required
  incomplete status was fetched; the predicate excludes it explicitly.
- Saved compact release facts and webhook delivery timeline. Notification GUID
  is not attached to transitions, so no unsupported claim about the individual
  triggering delivery. Checked publisher/confirmation job logs and native skipped
  confirmation; verified skipped-needs semantics against official GitHub docs.
- Recorded root cause, test gap, timeline, correction boundary and limits in
  docs/publication-release-root-cause.md. Production/test changes zero, no tests
  or live mutations. Publication authority/workflow/pin correction not implemented.

## Publication gate fix: reviewed growth request (2026-10-31)

- Owner supplied the implementation task and reiterated: ask before code growth.
  Read AGENTS, current metadata/ownership, all relevant fresh execution/state/DB
  boundaries and tests. Current production C++ baseline remains 2328 physical
  lines (Execution.cpp 373). No remote/commits; feat/graph-controller remains
  the existing task branch. No source, test or live changes made.
- Invariant: successful matched Publish plus exactly one successful completed
  Confirm job in the same authenticated run/attempt must precede receipt fetch
  and Published. Same graph transaction/commit-before-dispatch remain unchanged.
- Prepared an unapplied, canonically formatted controller patch adding 7 C++
  lines (Execution.cpp 380, controller total 2335). Existing job authentication
  and unique Publish matching are reused; two counts enforce one Confirm job
  and two successful completed required jobs. No helper, flag or durable state.
- Reduction/modification considered: whole-run success cannot substitute for
  explicit confirmation (native runs currently skip it). No redundant relevant
  mechanism can be deleted to absorb the additional required predicate without
  altering unrelated behavior or hiding code growth through compression.
- No authoritative local VOC-Experiments/ci-coordinator checkout found using
  repository metadata/allowed paths. SNodeC/.github checkout is unrelated and
  untouched. Do not edit reference worker templates as a substitute for the
  actual renewed sandbox workflow. Prepared its exact reviewable patch instead.
- Workflow patch adds 3 YAML lines to the existing condition; base Git blob
  e4ca49f44c893aa9ce64e4a4b5475a1390f319cc, proposed blob
  ce11809b876f119ccb521edd27aee66b7c39d0ac derived from exact bytes. No runtime
  pin changed, no additional checkout, no GitHub mutation.
- Reviewable patches: docs/patches/require-publication-confirmation.patch and
  docs/patches/fix-native-publication-confirmation.patch. Controller patch passes
  git apply --check; source manifest verifies every existing source unchanged.
- Pending owner approval of growth before applying production edits. Planned
  focused coverage uses real Execution/Store with isolated MariaDB and test-only
  public GitHub API boundary doubles, no production injection hooks; existing
  graph/persistence tests remain intact. Tests/docs/build wiring counted separately.


## Apply publication confirmation boundary (2026-10-31)

- Owner approved the proposed +7 production C++ ceiling, sandbox workflow
  activation/pin correction, local restart and one full core-triggered acceptance.
- Execution.cpp now requires the unique Confirm public receipt job and matched
  Publish job both completed successfully in the already authenticated pinned
  run/attempt before reading the committed receipt. No new correlation, polling,
  proof implementation, state authority or replay. Existing graph transaction
  still records publication and exact dependent assignments before dispatch.
- Focused duplicate evidence checks exposed a merge defect: preserving missing
  top-level timestamps created new null fields on otherwise unchanged events.
  Graph.cpp now preserves those fields only when previously present. Assertions
  remain in place. Reduced a single-use error-report helper into its sole
  callback without changing logging; no second implementation or compressed code.
- Final production impact: Execution.cpp +1, Graph.cpp +2; 2328 -> 2331 physical
  C++ lines, below the approved 2335 and hard 2500 bounds. No UI changes.
- Added tests/publication.cpp (266 lines), tests/publication.py (81 lines), and
  eight CMake wiring lines. Test-only GitHub public-boundary doubles exercise
  real Controller/Store/Graph and isolated real MariaDB. No production hooks.
- Passed publication-confirmation-contract (2.61s), graph-and-receipt-contract
  (0.06s), mariadb-commit-and-restart-contract (1.02s), native-startup-http-contract
  (105.07s), native-sse-backpressure-contract (19.31s); canonical formatting,
  Python syntax and source-manifest verification. Coverage includes pending,
  missing/skipped/failed/duplicate confirmation, wrong identities/receipt,
  duplicate/out-of-order facts, rejected commit, lost commit acknowledgement,
  restart without replay, committed dependent authorization before dispatch.
- Corrected only the existing sandbox confirm job condition. Exact new blob
  ce11809b876f119ccb521edd27aee66b7c39d0ac; GitHub commit
  2af037e60189b755dc8ce9f9711f801a12720d69 on VOC-Experiments/ci-coordinator/main.
  Existing needs, steps, inputs and feed-writing concurrency unchanged. Structured
  YAML comparison passed; live native confirmation remains an acceptance check.
- No authoritative local workflow checkout exists. Connector/Contents updates
  definitively rejected insufficient workflow scope, without mutation. Existing
  SSH Git access applied the reviewed workflow-only fast-forward commit using
  an isolated temporary Git index. No additional checkout, controller commit,
  controller branch/index change, permission expansion or production resource.
- Updated the approved public sandbox deployment pin only. Existing 110 executions
  compare exactly equal across restart, retaining original selections/attempts
  and old pins. The stopped managed database left a stale socket; after verifying
  its ownership, no owner and ECONNREFUSED, removed only that socket and restarted.
  MariaDB performed its normal recovery; no reset/migration or history removal.
- Locally running controller PID 402667, managed MariaDB 402668; existing external
  MariaDB untouched. Binary SHA256
  9eebe99fd8a1c4d9d7d1d62a7764ca0f3502ba1ed1b5963750c51617220e40bc;
  docs/source.sha256 hash
  d5a06d2b11f994a83ccd85b93a143892d9701d8df5e7ab99f388966ba068622b.
  Build /tmp/codex/build/core-ci/controller, GCC 16.2.0 Debug, installed SNode.C.
- Armed read-only SSE and GitHub observers, checked live 8080 browser, and created
  exactly one core tag v2.1.18 at source fc3acdb94779dafb879c183143c20e19b435e766
  (POST began 19:39:42.793678 UTC). Automatic propagation selected 20 executions.
  No manual downstream requests, retry or reconciliation. Acceptance ongoing.
  Evidence /tmp/codex/build/core-ci/confirmation-chain-20261031.


## Interrupted v2.1.18 acceptance and local restoration (2026-10-31)

- Confirmed all five actual core Publish/Confirm workflows succeeded, including
  native confirmation after skipped tools. First Debian core Published released
  app/side in the same committed SSE patch; both builds subsequently succeeded.
- Foreground controller and diagnostic observers disappeared during acceptance;
  sessions no longer exist, no exit status establishes cause. GitHub continued;
  graph traversal/new stage authorization stopped. No unsupported crash diagnosis.
- Restored unchanged binary/database at 19:46:07 UTC as detached PID 412713 with
  managed MariaDB 412714, after verified stale-socket removal. Normal DB recovery;
  exact graph equality across outage/restart and all 110 old executions retained.
- 20 new executions freeze corrected workflow blob. New state: 1 Published,
  4 Publishing, 2 Building, 13 Waiting. Six remote-completed assignments require
  explicit reconciliation to resume; no replay/retry/reconciliation performed.
- Stopped diagnostic observers after evidence capture, controller left running.
  No new tag, production source change, controller commit/push or extra checkout.
- Saved precise available timestamps, per-file accounting, validation, activated
  identities and interrupted acceptance limits in
  docs/publication-confirmation-acceptance.md. Exact local-only overhead and
  uninterrupted end-to-end/full timing remain unproven. Post-restoration browser
  inspection unavailable; HTTP/SSE snapshots are verified, not final DOM latency.


## Authorized fresh core v2.1.19 full rebuild (2026-10-31)

- Owner accepted preserved unresolved assignments after the outage for now, and
  explicitly requested another fresh complete graph build. Left v2.1.18 history
  untouched; no reconciliation/retry or replacement of its assignments.
- Verified v2.1.18 highest existing core tag and v2.1.19 absent. Created exactly
  one new v2.1.19 tag at the same approved fc3acdb94779dafb879c183143c20e19b435e766
  source. Single source webhook automatically selected 20 executions; five core
  Building, ten app/side Waiting, five join Waiting. New workflow pin unchanged.
- Controller PID 412713 and managed MariaDB 412714 survived subsequent user turns.
  Isolated detached read-only observers retain new run evidence under
  /tmp/codex/build/core-ci/core-v2.1.19-20261031. Baseline includes all 130 prior
  executions, including interrupted work; exact old history comparisons continue.
- Claimed the existing user 8080 tab and verified Live/changing new tree, reverse
  dependency levels and second-resolution local job times. No UI/source changes,
  new workflow edit, manual downstream request, controller restart or extra tag.
  Full acceptance monitoring ongoing.


## Full v2.1.19 graph acceptance passed (2026-10-31)

- One core tag propagated automatically through all five targets: 20/20 Published,
  all 20 build + 20 publication + 20 Pages workflows successful. Both OpenWrt joins
  succeeded; no retry/reconciliation/manual downstream selection or restart.
- Every publication requires the matched successful Publish and unique successful
  Confirm job; native confirmation actually ran. Every non-root first became
  Building in the same committed SSE update as its last prerequisite publication.
  Exact assigned dependency execution/attempts verified; zero checker violations.
- All 130 prior executions unchanged, including owner-accepted interrupted work.
  HTTP graph equals reconstructed SSE. Actual browser Live, newest 20 Published,
  all 120 displayed timestamp attributes match committed job/receipt facts exactly,
  seconds/24-hour local display confirmed. No reload required for final state.
- Measured tag POST -> final Published SSE 1191.060s (19m51.060s). Build authority
  -> GitHub run creation 3-4s. Receipt read -> Published SSE 1.210-21.541s, median
  3.547s; do not conceal that remaining controller/DB/delivery delay or claim it
  is pure CPU overhead. Precise ingress/outgoing-start instrumentation absent.
- 246 SSE messages on two connections, 244 incremental, 963053 incremental bytes,
  largest patch 39839; initial tree-addition boundary reconnected for fresh snapshot.
  No observer errors. Existing heartbeat preserved; no production changes during run.
- Detailed per-target/phased timings and limits in docs/core-v2.1.19-run.md.
  Source manifest verified; production stays 2331 physical C++ lines. Diagnostic
  observers stopped; controller 412713 / owned MariaDB 412714 left running. No
  controller commit/push/PR, additional checkout, production installation or SNodeC
  resource mutation. Evidence retained under /tmp/codex/build/core-ci/core-v2.1.19-20261031.


## Owner-authorized commit/push/merge checkpoint (2026-11-02)

- Owner requested committing, pushing and merging the existing accepted controller
  before implementing automatic startup reconciliation. Recovery remains analysis
  only: no production/test/UI changes, new recovery mechanism or live mutation.
- Checkout is still an unborn feat/graph-controller branch without a remote.
  Reference repository metadata identifies SNodeC/core-ci; its source/history was
  not inspected. Asked which destination repository/default branch should receive
  this independent implementation; no target inferred and no remote created.
- Prepare only controller-owned source, UI, tests, build/format wiring, contracts
  and evidence documentation. Preserve unrelated root pasted images and exclude
  build/cache/credentials. Updated stale README acceptance/publication status.
- Existing accepted source manifest and binary identity remain the validation
  baseline. Production is unchanged at 2331 physical C++ lines. No additional
  checkout, runtime restart, workflow activation, tag or package operation.


## Existing SNodeC/core-ci replacement checkpoint (2026-11-02)

- Owner explicitly chose the existing SNodeC/core-ci repository and required its
  old implementation replaced before any startup recovery work. Target default
  branch main, observed base f98669b71f2313371ebd27f0bd2127183fed5a41, no open PRs.
- Added origin in the fresh checkout and fetched metadata/objects; inspected path
  inventory only, not old controller/test/schema/docs source or architecture.
  Existing reference checkout feat/publication-retention has unrelated dirty work
  and remains untouched. No branch switch/reset/rebase/stash or extra checkout.
- Replacement contains 55 fresh files, preserves 45 external deployment/ worker,
  workflow and public-key assets byte-identical, and removes 94 superseded paths
  (30 old-only controller paths, 23 old test paths, old schema/config/docs).
  Remaining old filenames that overlap are replaced by the fresh accepted bytes.
  No old controller C++ remains outside the fresh source list.
- Fresh production/test/UI/contract bytes match the accepted manifest. Only README,
  AGENTS and handover documentation changed; regenerated the manifest for the
  updated AGENTS authorization. The sandbox accepted binary/source identities in
  reports remain historical exact identities. Production remains 2331 C++ lines.
- Canonical format-check target passes. Staged whitespace check excludes blank
  EOF warnings present in mandatory byte-identical canonical formatting configs;
  these baseline files were verified identical. No secret material or unrelated
  root screenshots staged. No startup recovery source, test or UI changes.
- Attach the replacement commit directly to main's existing history, push the
  existing task branch and open/merge one PR. No controller deployment, GitHub
  workflow activation, live tag/build, data migration/reset or key change.


## Implement one-time startup reconciliation (2026-11-02)

- Owner authorized implementation against the estimated +20–30 production C++
  lines after merging the accepted baseline. Existing task branch remains
  feat/graph-controller at 87061ab; origin/main merge 3fce12d has the identical
  baseline tree. Tracked work was clean; eight unrelated pasted images preserved.
- Invariant: startup observes original assigned attempts, never resends them.
  Only newly verified evidence and a successful authoritative graph commit can
  authorize a next stage or exact dependent. Errors must not block independence.
- Traced listener/DB ownership -> graph snapshot -> immutable-attempt observation
  -> GitHub validation -> applyEvidence/commands -> Store.change -> dispatch/SSE.
  Existing reconcile delegates to the same observation path. Reduction/change
  alone cannot add the missing startup invocation/selection; approved addition
  reuses that path, adds no parallel validation or state machine.
- Controller::resume reads once, scans latest assigned Building/Publishing work,
  and observes each exact attempt ID. main invokes it after successful management
  listen, with ordinary error reporting. No timer/thread/cache/flag/ready queue,
  retry/replay or source-tag discovery. Actual production +17 lines: header +1,
  Execution.cpp +15, main.cpp +1; total 2348, below estimate and hard 2500 bound.
- Extended existing publication test's public GitHub-boundary doubles with real
  observation/Store/Graph and isolated MariaDB. Covers missed publication release,
  running/unbound work, terminal/unassigned skips, independent error/success,
  concurrent webhook (one dispatch), missed definite build failure, running build,
  separate-process repeated restart, rejected and lost-ack commits with no replay.
- First focused run exposed test-only build workflow hash and reused proxy socket
  defects; corrected those fixtures without weakening assertions. Extended
  publication-confirmation-contract then passed in 3.32s. Other relevant checks
  pending. UI/runtime fixtures now populate after empty startup, preventing fake
  assignments from querying real GitHub; populated/changing assertions retained.
- Build isolated under /tmp/codex/build/core-ci/startup-recovery-20261102 (GCC
  16.2.0 Debug, installed SNode.C); original live binary/configuration untouched.
  No restart/reconciliation/live build, workflow/deployment change, commit/push or
  additional checkout authorized/performed for this new source change.


## Startup recovery final verification (2026-11-02)

- Final strengthened publication-confirmation-contract passed (3.52s): 29 primary
  cases plus prepared and separate-process restart phases. Actual webhook race
  reads twice/dispatches once; an error before independent success does not block.
  Rejected/unknown commit emits zero dispatch/SSE, and restart never replays.
- Native startup/HTTP/real-browser UI passed 104.99s; native SSE backpressure
  18.43s; MariaDB commit/restart 1.11s; graph/receipt 0.06s. Canonical format-check,
  changed Python syntax, 160-column production check and git whitespace check pass.
- Final production 2348 C++ lines (+17). Tests +97 net, no production hooks or
  test framework. No CMake/UI/external worker/workflow changes. Full per-file
  counts, checks and limits in docs/startup-recovery.md.
- Final manifest 443789c8bd53fb3b5f50203092dbb23480c73335556e8406510771070b2d515c; binary 185c18b622e7db3c6c2535fc2ba44ff9131f21e08a7da38ff58b36f2af260777.
  Uncommitted source on feat/graph-controller at base 87061ab; separate temporary
  GCC 16.2.0 Debug build. No live binary replacement/restart, execution observation,
  new build/tag, deployment, DB migration/reset, commit or push for this change.


## Authorized live restart-recovery acceptance (2026-11-02)

- Owner explicitly authorized committing the recovery source, going live locally,
  one fresh core-triggered chain, a deliberate outage during active builds, then
  restart after GitHub finishes those builds. No redelivery/manual reconciliation
  or completion retrigger: startup alone must recover and resume the full chain.
- Pre-upgrade graph: 150 executions, 124 Published, 4 Publishing, 2 Building,
  16 Waiting, 4 Failed. PID 412713 runs the previous binary. No data reset,
  migration, feed lock, key/pin/workflow change or historical assignment replacement.
- The normal startup pass observes every unfinished assignment, including existing
  ones; acceptance tracks the fresh v2.1.20 tree separately. Confirmed v2.1.19
  highest existing tag and v2.1.20 absent, using approved source fc3acdb94779dafb879c183143c20e19b435e766.
- Evidence under /tmp/codex/build/core-ci/restart-chain-20261102; reuse the separately
  built/tested 2348-line controller. Existing sandbox credentials are passed only
  by approved paths; no credential contents inspected or printed.


## Local recovery live; controlled acceptance blocked before selection (2026-11-02)

- Committed implementation as 1e76e9a, not pushed/merged. Graceful local upgrade
  succeeded; new binary SHA 185c18b622e7db3c6c2535fc2ba44ff9131f21e08a7da38ff58b36f2af260777
  runs PID 487201 with preserved DB/inputs. 8080 and 8443 listen; local real-name
  TLS GET verifies certificate and returns 404. Production count unchanged 2348.
- Created core v2.1.20 exactly once. GitHub source push webhook returned 502,
  failed to connect to host at 10:35:35.454 UTC before planned interruption.
  Public endpoint connection also fails; current Jupiter LAN 10.20.99.86 differs
  from documented forwarding target 192.168.1.239. No network/router changes.
- Verified zero v2.1.20 selections and zero matching core runs. The active-build
  interruption driver never stopped the controller; no completion events replayed,
  manual reconciliation, retry or additional tag. Diagnostic observers stopped;
  recovery controller remains running. Existing startup observations are normal.
- Full new chain/restart acceptance remains unexecuted pending reachable public
  callback route. Detailed findings and continuation in
  docs/restart-recovery-acceptance.md; raw evidence under
  /tmp/codex/build/core-ci/restart-chain-20261102. Preserve existing tag/assignments.


## 2026-11-02 — bounded GET reissue and attempt-bound uncertainty

Owner authorized implementation with an initial +50 C++ ceiling, then explicitly
approved +55 after the measured review patch exceeded +50. JavaScript allowance
remained +2–5. No commit, push, deployment, restart or live action is part of this
change. Pre-existing AGENTS.md, reports, prompts, PNGs and WORK_LOG prefix were
preserved; only this entry is appended to WORK_LOG.md.

Invariant: one latched final outcome per transport attempt, at most one final
callback per HTTPS call, and uncertainty writes/clearances guarded by the committed
latest attempt and exact awaited stage. Store success remains the authority for
commands and SSE. No-change operations remain write/revision/notification-free.

Trace: token POST and API GET in GitHub.cpp, dispatch POST plus artifact redirect
and blob GET in Execution.cpp all use HttpDone. The extra safe-context argument
survives all five callers. Observation selects the immutable attempt, routes early
errors as null-observed facts through applyEvidence, and shares the existing
Store.change path with validated evidence. MariaDB's deque keeps the appended
UPDATE in its read's command sequence; autocommit/single connection/single writer
establish Store commit order. No lock, sequence, cache or second pass was added.
Store was not modified; only acknowledged writes dispatch and send field patches.

Production changes: request-local direct callback GET reissue (three attempts
maximum), unchanged strict TLS and disabled native retry/reconnect, fixed ERR
context, JSON exception-id classification in checked(), consolidated token/API
parsing, durable read/evidence clearance levels with legacy/idempotence guards,
and the existing UI activity line/tooltip/expanded first-recorded detail.
Receipt identity rejection is handled at its existing Graph authority before
completion, leaving evidence uncertainty instead of losing the diagnostic on a
thrown operation. Exact bindings remain required. No bootstrap, wire contract,
workflow, pin, worker, readiness policy, deployment asset or listener change.

Physical controller-owned C++ recount command:
`rg --files --hidden -g '!.git' -g '!build'`, select .cpp/.h/.hpp/.cc/.cxx outside
tests, enumerate physical splitlines; assert that all identified production files
are src/*.{cpp,h}. Compare baseline using `git show HEAD:<path>`.

| File | Before | After | Net |
| --- | ---: | ---: | ---: |
| src/Auth.cpp | 137 | 137 | +0 |
| src/Auth.h | 17 | 17 | +0 |
| src/Controller.h | 95 | 95 | +0 |
| src/Evidence.cpp | 272 | 272 | +0 |
| src/Execution.cpp | 389 | 400 | +11 |
| src/GitHub.cpp | 307 | 306 | -1 |
| src/Graph.cpp | 437 | 454 | +17 |
| src/Http.cpp | 130 | 158 | +28 |
| src/Managed.cpp | 161 | 161 | +0 |
| src/Managed.h | 24 | 24 | +0 |
| src/Store.cpp | 103 | 103 | +0 |
| src/main.cpp | 276 | 276 | +0 |
| **Total** | **2348** | **2403** | **+55** |

No difference from the reported baseline. Below 2500 by 97 lines; exactly at the
approved +55. C++ diff: 202 inserted/147 deleted. UI app.js: 8 inserted/3 deleted,
net +5. Maximum C++ width 146; maximum app.js width 157, both <=160. No blank or
comment deletion, statement merging, generated runtime logic or hidden library
was used to fit the allowance.

Tests: net +504 lines in tests (599 inserted/95 deleted, including the new
80-line diagnostics.cpp); CMake adds 10 lines solely for the diagnostics test.
Focused additions cover TLS/response close/recovery/concurrent budgets, POST and
received error finality, parse latching, strict certificate rejection, safe logs,
malformed token/API/receipt JSON, no fake facts, E-F-S/legacy clearance, exact
legacy zero UPDATE/revision/SSE, duplicate F, commit ordering, interleaved dispatch
acknowledgement, failed/unknown diagnostic commits, history guards, retry policy,
restart without replay, receipt completion and browser SSE/layout behavior.
Documentation: README +6; contracts +12; new observation-uncertainty.md +130;
this WORK_LOG entry only appended. Existing dirty documentation is untouched.

Source identity: /home/voc/projects/snodec/core-ci-new, feat/graph-controller,
HEAD 6ed69016d632caf9da44ad6611f4de3c098f8395 plus uncommitted task changes.
Production/CMake/UI fileset SHA-256 (sorted path + NUL + bytes + NUL):
`e08e5bd2791afe39a4274310f30bebb0a8a839f4820b3efa02b9aec442cb20d0`.
The exact files and counts are in the task directory's accounting.json.
Build: Debug, /usr/bin/c++ (GCC 16.2.0), installed SNode.C from /usr/local,
OpenSSL 3.6.5, nlohmann_json 3.11.3, Zlib 1.3.2.
Directory: /tmp/codex/build/core-ci/bounded-read-20261102/build.
Binary SHA-256: 738cf9f37791bf6b3aa734cbf6d93bdd300ec3c0a4eba41d78c57b7f9a0b3452.
ELF build ID: 8cd95ac08e7457770dc4205afecceefe5972e7dd.

Exact checks (commands run from the checkout; TASK below is
/tmp/codex/build/core-ci/bounded-read-20261102):

- `TMPDIR=$TASK/tmp cmake -S . -B $TASK/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`
  and `TMPDIR=$TASK/tmp cmake --build $TASK/build -j4`: pass. Initial GCC 16
  internal compiler error at the dependent Instance::getSubCommand expression
  was resolved using an explicit existing Client shared_ptr type. A first attempt
  to disconnect through the response callback's Request base failed compilation;
  the original MasterRequest capture/lifetime pattern was retained. No framework
  or runtime wrapper was introduced. Final build output: final-build.log.
- `ctest --test-dir $TASK/build -R 'native-https-contract|github-json-diagnostics-contract|artifact-bytes-contract|mariadb-commit-and-restart-contract|publication-confirmation-contract|graph-and-receipt-contract' --output-on-failure`:
  6/6 pass, 10.65s; final-tests.log. Additional final real-Store legacy no-write
  assertion: publication-confirmation-contract passes in 5.74s; legacy-tests.log.
- `CI_HTTPS_MEMCHECK=$TASK/memcheck python3 tests/https.py $TASK/build`: pass
  for six selected native lifetime cases. Installed Valgrind reports zero errors
  and zero bytes/blocks at exit for every case; lifetime-tests.log and memcheck/.
  No production hooks. Earlier failed attempts remain logged before success.
- `PLAYWRIGHT_BROWSERS_PATH=/tmp/codex/build/core-ci/browser/browsers CI_BROWSER_MODULES=/tmp/codex/build/core-ci/browser/node_modules ctest --test-dir $TASK/build -R native-startup-http-contract --output-on-failure`:
  pass, 104.12s; browser-final.log. Populated/changing Chromium SSE checks cover
  full initial data, small updates, progress/timestamps, diagnostic line/tooltip,
  first-recorded detail, evidence clearance with stage transition, stable row
  height/columns and reconnect. An initial run failed because Playwright looked
  in its default cache; the existing installed browser path resolved it. No tools
  or browsers installed. Three existing 30-second heartbeats also passed.
- `ctest --test-dir $TASK/build -R native-sse-backpressure-contract --output-on-failure`:
  pass, 17.83s, recorded in browser-tests.log. This test was not repeated after
  unrelated const annotations/test-only edits. SSE implementation is unchanged.
- `cmake --build $TASK/build --target format` and `--target format-check`:
  pass; final-format-check.log. `node --check ui/app.js`, `node --check tests/ui.cjs`,
  `git diff --check`, complete production line/column checks: pass.
- `cmake --build $TASK/build --target analyze -j1`: exit 0, with 24 reported
  user-source warnings plus suppressed dependency diagnostics; analyze.log.
  Three touched-code warnings were addressed without growth. Final
  `clang-tidy -p $TASK/build src/Http.cpp src/Execution.cpp` exits 0 with five
  warnings in unchanged lines: two declaration parameter-name mismatches,
  webhook copy, protectedFile fdopen analysis and its const suggestion.
  Full target also reports unchanged Auth/Managed/Store/main warnings, including
  Store potential-leak analysis. These are recorded, not claimed resolved.

Earlier focused test failures/output remain in first-tests.log, second-tests.log
and third-tests.log: incorrect raw-header/log-destination fixture assumptions;
old assertions disallowing newly required diagnostic SSE; legacy test incorrectly
requiring normalized fields; unknown-commit fixture trying a disconnected DB
connection; and out-of-range status 600 rejected by SNode.C. Fixtures now use
real native behavior, independent committed DB reads for unknown outcomes, and
explicit new invariants rather than suppressing failures.

Transport observations: incomplete Content-Length reaches synthetic no-response
in this installed framework and exhausts three attempts. Malformed HTTP reaches
one latched delivery/parse failure. Status 600 is rejected before a response is
reported, so only terminal parse failure is exposed; no SNode.C modification is
required or made. Connect FATAL/DISABLED and X509 setup failure paths are governed
by the explicit source predicate but were not individually fault-forced. Public
TLS/HTTP fixture tests exercise ERROR, TLS end, response end, parse, POST, normal
and received error responses, including Retry-After without retry.

No controller-overhead or GitHub timing claim is made. Test elapsed times and
fixture-to-browser delivery are isolated validation, not live controller/GitHub
queue/setup/build/publication/Pages timing or evidence of overall speedup.

Limits: bounded best-effort, no polling and no guarantee after arbitrary outage;
token POST and all received responses are never retried. Exhausted observations
stay awaited until a webhook observation, next startup pass or explicit Reconcile
supplies the required successful check. Underlying outage cause remains unknown;
this is not a root-cause fix. No promise that every request eventually completes.
Live clean single-restart full-chain interruption acceptance remains unexecuted.

Preservation check: SHA-256 manifest from task start verifies every pre-existing
dirty/untracked file, including AGENTS.md and all eight PNGs, byte-for-byte;
WORK_LOG prefix matches its original hash. Nothing added to the git index.
No commit/push/PR/deployment/restart/live action. Public vchrist listener and
runtime remain untouched. No extra checkout. Disposable TLS/database fixtures
clean themselves; retained task build, logs, accounting and preservation manifest
are handover artifacts, removable after review and when the build is no longer
needed. No pre-existing temporary directories were removed.


## 2026-11-02 — Claude review and owner-authorized publication/deployment

Owner clarified "cpmd" as commit, push, merge and deploy on vchrist. This supersedes
the implementation phase's no-live-actions restriction for those stages only; no
new tag, workflow change, manual reconciliation, retry or new chain is requested.

Claude reviewed the exact fresh source/contracts/tests and public SNode.C interfaces
with all tools disabled. The initial blocking B1 webhook-stage finding was real:
late build events could become publish observations and overwrite uncertainty.
Removing the two-stage loop fixes it without adding policy. D1 job validation was
moved inside the existing evidence callback; the duplicate completed-success
predicate remains solely in observe. Same-reason kind upgrades preserve at.
Synchronous signing failures now reach token's callback. These four corrections
remain within the approved +55: Execution399, GitHub306, Graph455, Http158,
Controller.h95; total2403. All other production file counts remain unchanged.
JS remains +5, max157; C++ max146. No framework/bootstrap/readiness changes.

Regression tests add real late build webhook cases, missing/skipped assigned jobs,
invalid signing keys, same-reason kind upgrades and lifetime probes that expire
before framework stop. All18 HTTPS cases pass Valgrind with zero bytes retained
and zero errors. Unusual in-range226/427 preserve status with one connection.
The additional test-only timer is confined to tests/https.cpp, never production.

Recheck: review-tests.log records6/6 affected CTests passing (11.56s). Existing
browser-final104.12s and SSE17.83s checks cover unchanged UI/transport code.
review-final-format.log passes. review-analyze.log checks all corrected production
files; remaining warnings are on unchanged lines. No new tools installed.

Claude's re-review verdict: **No blocking findings**. It verified B1/D1/D3, signing
failure routing and adequate pre-shutdown lifetime proof. Non-blocking continuation
throw convention, JSON-id wording, receipt Store-status reporting and unforced
faults are documented in docs/bounded-read-claude-review.md. Full app.js scope and
final static/count evidence were sent to close the remaining evidence questions.

Commit scope excludes the pre-existing dirty AGENTS.md and protected untracked
reports/prompts/PNGs. WORK_LOG is staged selectively: HEAD content plus task-only
appendices, leaving its pre-existing dirty prefix unchanged in the worktree.
The source hash manifest is refreshed against the prospective committed tree,
including HEAD's AGENTS bytes rather than the protected dirty working copy.

The vchrist candidate native Release build succeeds with GCC14.2.0 against its
existing private SNode.C prefix. No framework or system libraries changed. Public
pre-deployment graph:170 executions (163 Published,4 Failed,3 Waiting); no assigned
Building/Publishing work currently needs recovery. PID318583 remains active.
After reviewed commit/merge, deploy the exact committed artifact, preserving the
existing service configuration, credentials, database, public0.0.0.0:8080 binding
and private runtime. Activation and actual identities will be appended separately.
No new full-chain or clean interruption acceptance is implied by this deployment.

Claude final confirmation closed the UI/static evidence questions and reaffirmed
no blocking findings. Reviews were tool-disabled single-turn calls; its prose
claim to run commands is unsupported by the CLI record. Actual command, source,
count and preservation checks were executed here and supplied to the reviewer.
