# Compact package-work interface proposal

Approved UI design, implemented locally from `7c5e68dfd9230696968ad8905da2e7b3f0c8870c` on
`rewrite/slim-graph-controller`, canonical checkout `/home/voc/projects/snodec/core-ci`.
Implementation is deployed on core.vchrist.at under explicit owner authorization.
Publication recovery is outside this checkpoint.

## Invariant and source findings

Show the identity, target, progress and recorded timings of an execution compactly,
without obscuring uncertainty or inventing evidence. This is presentation of the existing
graph, not another state authority. Preserve filtering, ordering, actions and SSE updates.

- `ui/style.css` sets the details label track to 90px. This causes labels such as
  “Publisher job finished” to wrap and increases vertical space unnecessarily.
- The global `td` rules impose a 240px maximum and `overflow-wrap:anywhere` on the
  expanded details cell as well as ordinary cells. Override these for the details cell.
- `ui/index.html` and `renderWork()` in `ui/app.js` devote separate columns to distribution,
  architecture, format and rebuild, but show only build-start time in the collapsed row.
- `timestamp()` combines date, time and zone into one string. Use one shared date/time
  presentation for the row and details, with intentional line separation.
- The graph already supplies build/publisher-job start and completion times and verified
  publication time. No controller, database, worker, REST, webhook or SSE protocol change
  is required. No additional API requests are needed.
- Format is explicit source data. OpenWrt supports IPK and APK: distribution must not be
  used to infer it. Retain it inside Target, without a dedicated Format column.

## Collapsed work table

Use two header rows with these visual groups:

| Group | Contents |
| --- | --- |
| Expand | Existing labelled expand/collapse button |
| Repository | `VOC-Experiments/demo-core` on line 1; `v2.1.4 · r169` on line 2 |
| Target | `openwrt 25.12` on line 1; `x86_64 · apk` on line 2 |
| State | Existing state/activity, with natural word wrapping |
| Build | Started and Finished timestamp subcolumns |
| Publisher job | Started and Finished timestamp subcolumns |
| Publication verified | One timestamp column, separate from the job |

This is nine leaf columns including the expander, but only seven visual groups. Three
standalone identity columns disappear; the recovered space makes the four additional
recorded timestamps useful. Do not combine start/finish into a four-line cell: normal
rows should remain approximately two lines tall, unless long identity/state text needs more.

For example, each timestamp cell displays:

```text
28/10/26
10:37:40
```

The example is illustrative; retain browser locale date formatting rather than hard-coding that
order. Time explicitly uses the 24-hour h23 cycle. Date is always line 1; time including seconds is line 2, centered with the date. Use 24-hour HH:mm:ss. Neither line wraps.
Use the browser's local time everywhere, as explicitly requested by the owner. Show no
timezone labels in the overview, details or tooltips. Keep semantic `time datetime`
values for the original instant; full local date/time descriptions remain accessible.
No timezone selector or timer.

Preserve current Pending/Not recorded decisions, including historical/superseded rows.
Do not turn missing evidence into a fabricated date, elapsed-time counter or failure.
Use three shared Intl formatters for date, time and full timestamp; reuse the existing
pending decisions rather than copying their conditions across each new timestamp cell.

Use proper grouped headers and matching column groups (4 / 2 / 2 / 1), scope attributes,
rowspans and colspans. Update the expanded cell's colspan from 8 to 9. Keep current expander
accessible names, expanded state and controlled panel identity. Top-align row cells and
use tabular numerals for dates, times and revisions. No new sorting or sticky columns.

## Expanded details

Keep the desktop two-column composition: facts/timings on the left; prerequisite executions
on the right. The left side becomes:

1. Activity and Selection as a compact definition list.
2. A timing table with rows Build and Publisher job, and columns Started, Finished, Duration.
3. Publication verified as a separate fact, using the same two-line timestamp presentation.
4. Existing failure/blocker explanation, hook-reported compiler/test summary and report link.

Expanded timestamps use the same local date/time presentation, without timezone labels.

Keep log links, conditional retry action and expandable technical identities unchanged.
The timing table replaces six repeated label/value rows. It preserves durations while
reducing vertical height and makes start/finish comparisons easier. No information is lost.

Scope label sizing to execution details: `max-content minmax(0,1fr)` for fact lists;
no wrapping of labels; values can wrap long compiler names, errors, identifiers and URLs.
Remove inherited arbitrary word breaking from the panel, while retaining it for values
that can contain long tokens. Avoid global label changes affecting repository editing.

## Width, zoom and accessibility

This remains a desktop table with horizontal scrolling at narrow widths. Approximately
1,050–1,250 CSS pixels is a provisional width estimate, not a verified fit or fixed promise.
Do not truncate evidence or shrink fonts to force a fit. Long state text may wrap at spaces.

The expanded panel should remain usable within the visible scroll viewport rather than
stretching its contents across the entire off-screen table. Proposed CSS: make the table
wrapper an inline-size container; constrain a panel-content wrapper to that container's
width and align it with sticky left positioning. Stack its two columns below 760px of
available container width. Verify this interaction in a real browser before accepting it;
CSS layout behavior here has not been measured. Avoid nested scrolling timing tables.

Retain horizontal table scrolling on small screens; do not introduce a second card UI,
column picker, layout persistence, sticky header/column system or new frontend dependency.
Use current theme colors, compact font sizes, action semantics and coarse-pointer sizing.

## Approved implementation boundary

Only `ui/index.html`, `ui/app.js`, `ui/style.css` and the existing UI fixture need changes.
CMake embeds these assets, so deployment of approved changes requires reconfiguration,
rebuild and the separately authorized installation/restart stages. No C++ line growth;
the 3,510-line controller bound remains unchanged. No GitHub workflow changes.

Provisional production UI accounting: 90–130 added, 50–70 removed, net +40–60 lines.
These replace row construction, timestamp formatting and details timing markup rather than
adding alternate renderers. Test fixture: +30–50 lines, net +30–50. Documentation: this
proposal and WORK_LOG entry. Re-estimate before exceeding these proposed bounds.
The owner approved net +40–60 UI production and +30–50 fixture lines, with no C++ growth.

## Focused verification plan

Extend `tests/ui_enrollment.py` with real-shaped synthetic execution facts: fully recorded
publication, partial publication, no publication evidence, long activity/repository/compiler
values, explicit IPK/APK targets and timestamps spanning a DST boundary. Keep existing
fixtures and enrollment checks. No production hooks or new browser-test framework.

Browser checks at 1280/1440px, 125%/200% zoom and approximately 390px:

- Dates and times occupy exactly two intentional lines; labels do not accidentally wrap.
- Repository/tag/rebuild and distribution/series/architecture/format remain identifiable.
- All five timestamps and both durations match the supplied facts; local date/time descriptions
  are accessible and no timezone labels appear.
- Grouped header associations and expander controls are correct; long values remain readable.
- Expanded details are usable after horizontal scrolling and at narrow container widths.
- SSE updates retain the open execution and technical disclosure state; search, filtering,
  busy controls, log links and existing failed-only retry rules remain unchanged.
- Inspect recorded graph data and confirm no additional API requests or state changes.

Verification results for the local implementation are recorded below. No full backend test suite
is required for a rendering-only change. Any later live UI acceptance is separate from
package publication acceptance. Stuck-publication recovery and delay investigation remain
outside this design task.

## Reviewer discussion

Claude reviewed the current UI sources and existing fixture read-only, covering all requested
layout, column and timestamp changes. It supported the nine-leaf-column layout over hiding
start times or doubling row height with stacked start/end pairs. It identified inherited
cell wrapping and narrow-panel width as additional source issues. Width claims remain
estimates. Automatic supersession can preserve waiting nodes; existing missing-time
semantics are deliberately unchanged. Claude confirmed the final design after these corrections, with no remaining disagreement.
Repeated Pending labels for superseded waiting work remain a separate presentation-semantics
question; no state interpretation change is included. The full review and discussion are retained under
`/tmp/codex/build/core-ci/ui-compact-proposal/`.

## Local implementation verification

The final UI merges identity/target columns, shows all five recorded timestamps on two
intentional lines and groups timing facts compactly. Header padding is 3px so group titles
sit directly above Started/Finished. All times use browser local time without timezone labels.
The scoped details layout retains labels on one line. At narrow widths the timing table
uses three value columns with the job heading above them; the expanded panel stays within
the visible horizontal-scroll viewport. No additional renderer or data authority was added.

Installed headless Chrome exercised the existing synthetic fixture through its public HTTP
and SSE boundaries. UK and US locales pass at 1440, 1280, 1152, 720 and 390 CSS-pixel widths.
The 1152/720 cases model reduced layout space for zoom; actual browser zoom controls were
not exercised. No panel overflow remains in either locale at 390px. Header associations,
all five timestamps, two-line/no-wrap styling, exact Pending/Not recorded display, explicit
IPK/APK identity, text-safe compiler content, filtering, failed-only retry visibility,
busy controls and retention of expanded execution/technical details across SSE were checked.
The UI generated no extra API or mutation requests. Recorded timestamps and durations are
unchanged; DST-spanning instants stay exact even though local clocks can repeat.

`node --check ui/app.js`, Python fixture compilation and the `core-ci`/`format-check` build
targets pass. The format target checks the committed C++/CMake baseline; JS/CSS browser
verification is separate. Evidence, scripts and screenshots remain under
`/tmp/codex/build/core-ci/ui-compact/`. No new browser dependency was added to the repository.
The UI deployment and restart are verified on core.vchrist.at: active service PID222160;
HTML/JS/CSS match tested source; graph unchanged at generation2100 with40 executions.
Public TLS status and SSE bootstrap are available. No package run, retry or production
integration change was performed. Deployment details and binary identity are in WORK_LOG.md.

Build and Publisher job group headers are centered over their two timestamp columns.
Chrome measured each title within 0.008px of its group midpoint.
