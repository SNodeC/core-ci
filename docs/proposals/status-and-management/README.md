# Status templates and management interface proposal

The Work table management layout is accepted and implemented with automatic SSE updates;
Refresh view is removed. Status-template implementation is tracked separately; public work-state
projection needs its own final design. The accompanying management preview uses illustrative
work states; it is not a live monitor and its actions do not call core-ci or GitHub.

## 1. Required result for package status pages

The reference is the current [SNodeC/Packages status index](https://github.com/SNodeC/Packages/blob/main/status/README.md)
and the [SNode.C project page](https://github.com/SNodeC/Packages/blob/main/status/snodec.md).
MQTTSuite and SNode.C IDP use the same presentation structure. Inspection was read-only.
Only the published presentation and its visible data requirements are reference material;
its orchestration implementation and status storage are not reused.

The proposed default example must reproduce these features:

- A project index with title, latest release and project-status link.
- Per-project heading, navigation artwork/links, distribution/series jump table and anchors.
- Distribution/series sections with architecture rows.
- Separate **Available**, **Published (UTC)**, **In build** and **Links** columns.
- An existing published version shown alongside newer work, rather than replacing one with
  the other. Waiting information can name immediate prerequisites and link to their rows.
- Verified build-job links, package-directory links, install/catalog/troubleshooting links.
- Accessible state artwork, a legend and a centered dash for absent/inapplicable information.

Markdown containing safe HTML tables/images is necessary for the GitHub repository view.
A standalone HTML page with CSS is a separate output for GitHub Pages; arbitrary CSS/scripts
cannot be assumed to work in GitHub's Markdown preview. Templates can share normalized data
while choosing different output markup. Existing feed/package paths remain unchanged.

## 2. One explicit presentation-data contract

User-defined public presentation parameters are passed through under `presentation.parameters`
from the trusted bundle's `presentation.json`. Owners can add a brand name, support URL,
project introduction or release note link without adding a controller option for every key.
Additional execution facts can be exposed by core-ci or trusted CI tooling when they are
actually recorded, with their producer/provenance identified. Templates choose which to show.

Keep two concepts distinct in the same view model: verified package availability and observed
controller work. A build success must never populate the availability column.

| Group | Proposed public fields | Authority |
| --- | --- | --- |
| `site` | Title, base URL, navigation, artwork, help/installation links | Trusted deployment presentation configuration |
| `snapshot` | Schema, graph generation, capture time, publication-data revision | Captured projection, explicitly dated |
| `projects[]` | Repository, title, package namespace, selected source version, links | Selected definitions and deployment links |
| `projects[].targets[]` | Row ID, family, distribution, suite, architecture, format, runner selector | Selected explicit target rows |
| `targets[].available` | Source/package versions, rebuild, package/export URLs and identities, receipt, verified job URL, publication time when recorded | Verified publication evidence |
| `targets[].work` | Selected tag/rebuild, state, derived activity, immediate prerequisites, blockers, failure and verified run/job links | Read-only graph projection |
| `targets[].environment` | Image digest or SDK identity/release/target when resolved and recorded | Validated execution evidence |
| `extensions` | Additional public metadata under owner-defined namespaces | Declared producer and provenance, never execution authority |

Use structured fields, not parsing human labels to recover target coordinates. Supply grouping
and ordering in the view adapter using existing source-selection/version semantics; templates
must not implement a second SemVer algorithm or decide graph eligibility. Display names and
navigation order are presentation settings; they are not a controller distribution catalogue.

Keep `selected_source_version` distinct from latest published source/package version. The
reference heading's latest-release policy must be explicit; it cannot be inferred from lexical
package filenames. Target availability likewise uses verified publication/version semantics.
A permitted successful retry can become the newest rebuild of the same source version.

Optional fields are `null`/absent when unrecorded. Never label the render time as the package's
publication time. Do not fabricate a job URL from an unverified run ID. The current compact
receipt/view does not expose every proposed field; add only provenance-backed fields needed
by the accepted presentation contract. Do not import the reference repository's status.json.

`extensions.deployment.*` and `extensions.worker.*` let owners expose additional public
information: documentation links, compiler versions, test summaries, support notes or build
parameters. Producers declare which fields they provide and where they came from. Source-owned
annotations remain untrusted text and cannot override reserved identity/state/publication
fields. Credentials, private filesystem paths and arbitrary process environments are excluded.

## 3. Template execution and ownership

Use [Jinja2](https://jinja.palletsprojects.com/en/stable/templates/) in trusted GitHub tooling; Python already owns rendering. Replace the hard-coded
Markdown/HTML assembly in `feed_status.py`, rather than adding a parallel renderer. Supply a
versioned, documented normalized context and default templates. Support includes/macros for
navigation, tables and legends. Strict missing-variable errors catch broken required fields;
optional data is explicit. Use appropriate escaping for HTML, Markdown table cells, links and
anchors rather than treating one HTML-escape operation as sufficient for every output context.

Proposed deployment-owned bundle:

```text
status-templates/
  presentation.json
  index.md.j2
  project.md.j2
  index.html.j2
  partials/
    navigation.md.j2
    target-table.md.j2
    legend.md.j2
  assets/
    status-icons/
```

The templates/assets live in the trusted worker repository, pinned to its reviewed tooling
revision. They are never supplied by a source hook or untrusted artifact. The render process
gets only public normalized data and bounded template/assets paths. It runs outside the phase
that exposes signing keys or feed-write tokens. Local output is bounded and restricted to
configured status/documentation paths; it cannot overwrite packages, receipts, keys or indexes.
Trusted template execution is not presented as a security sandbox for arbitrary hostile code.

For example, a row template can access:

```jinja2
{% for target in project.targets %}
<tr>
  <td><code>{{ target.arch | e }}</code></td>
  <td>{{ target.available.package_version | e if target.available else '—' }}</td>
  <td>{{ target.available.published_utc | e if target.available and target.available.published_utc else '—' }}</td>
  <td>{% if target.work %}{{ target.work.activity | e }}{% else %}—{% endif %}</td>
</tr>
{% endfor %}
```

The example demonstrates markup freedom, not an implemented schema or production template.
A reproduction fixture must demonstrate the complete reference structure, including concurrent
available/in-build values, a waiting diamond and unavailable/unrecorded data.

## 4. Additional data needed for full reproduction

Current `feed_status.py` reads retained publication receipts only. It cannot know waiting,
building or failed graph work. Current `GraphStore::view()` provides execution state and the immutable `target_row`, but
not every timestamp or publication-selection detail as separate fields.
The gap is data and update timing, not merely choice of template language.

Publication timestamps, job duration, compiler versions and structured test summaries do not
require a complete graph snapshot. They can be recorded/retrieved as execution evidence and
exposed through existing receipts/view data. Test summaries are optional additional display
information, not a requirement for the reference layout or authority to publish.

For full in-build-column parity, propose a minimal public work projection captured after
committed state changes. Include only project/target/rebuild, work state, immediate prerequisite
references/blockers, verified links, generation and capture time. Do not export the entire
internal graph or duplicate its authoritative state. The controller remains the only execution authority. A trusted GitHub status job
renders that snapshot together with verified retained receipts and deployment presentation
settings, then performs a serialized checked feed update. No source build gets feed credentials.
Snapshots are projections, not a second scheduler/configuration store. Include generation and
capture time so a delayed job cannot overwrite a newer controller projection; recombine with
current retained receipts when publishing rather than replacing package state with a stale copy.

A publication-only change cannot update the page when a node merely waits or fails. Therefore
this status job must also be triggerable for committed work-state changes. It is a distinct
proposed extension needing approval: reuse authenticated dispatch and the feed's writer
serialization, without a timer, durable notification queue or new persisted execution state.
Coalesce/reject stale projections at the writer boundary. Do not give a queued snapshot
permission to regress availability or overwrite retained package bytes. Detailed dispatch
payload bounds and reconciliation behavior must be designed before implementing this stage.

The public page is a dated snapshot; the management UI is the current controller view.
Uncertain assignments stay uncertain in both. Existing five execution states stay unchanged;
labels such as blocked, build-complete or superseded are derived activity, not new persisted
states. The reference's additional legend entries can be styled by templates but cannot imply
that core-ci implements unrecorded execution states.

## 5. Management UI proposal

Use a desktop-first operations screen with a compact horizontal application header and two
main tabs: **Work** and **Repositories**. Avoid a permanent left build list and an always-open
configuration editor. The execution table gets the usable width.

Work rows show repository/source version, distribution/series, architecture, package format,
rebuild revision and state in aligned columns. Use 13px body text, 12px secondary metadata,
compact row spacing and restrained headings. Long identifiers wrap in the detail view and
are bounded in the table. All essential data remains accessible without hover or color alone.

The active view includes waiting, building, publishing and failures. Published/history is
available through a local filter. Repository/target search and state filtering operate on the
existing graph response, not new GitHub queries. SNode.C SSE delivers committed controller
updates and a fresh baseline on reconnect, preserving filters, selection and drafts. No automatic controller reconcile or new polling policy is introduced.

Selecting a row exposes source/target identity, immediate prerequisite states, the actual
blocker/failure, assigned job links, publication evidence and collapsed technical identities.
Explicit retry is available only when allowed by the existing graph. An uncertain publication
must never gain a retry button merely because its display looks stalled.

The preview includes two meaningful layouts:

- **Work table** (recommended): full-width table and a selected execution detail area below it.
  Best use of space for many architectures/targets and the smallest presentation change.
- **Inspector**: a compact work list with a persistent right detail column on wide desktops.
  Useful for repeated investigation; stacks below the list on narrower screens.

Repositories use a separate compact list and selected editor. Dependencies/matrix editing,
JSON view, committed revision and explicit GitHub write feedback remain visible. Workflow
preview/install is contained in repository details. A form save still means an actual GitHub
commit; pending, failed and uncertain writes must remain distinct in the finished UI.

Narrow screens get a scroll-contained data table or stacked inspector, without changing the
desktop-first priorities. Fine-pointer controls stay compact; coarse-pointer targets expand.
Keyboard row-selection buttons, form labels and visible focus are retained.

## 6. Proposed implementation bounds and acceptance

These are estimates for approval, not authorization to implement. Stage the work so the owner
can accept the UI independently of public feed-state reporting.

| Change | Estimated net production lines | Estimated test lines |
| --- | ---: | ---: |
| Desktop management UI replacing existing layout; structured target fields in existing graph view | 180–300 | 50–90 |
| Replace feed renderer with one normalized context plus trusted templates; publication-only first | 220–360, including templates | 100–170 |
| Full public in-build projection and trusted status-update job | 180–300 | 100–170 |

Before the final stage, refine the concrete dispatch/snapshot/writer boundary and its line
estimate. Preserve one graph authority and remove replaced presentation code. No new graph
states, general workflow engine, controller build infrastructure, registry/SDK client, user
identity system or blanket retry policy is part of this proposal.

Focused acceptance covers existing API behavior, compact layout at desktop/narrow widths,
long names/architectures, row selection and uncertainty/retry rules, accurate GitHub save
feedback, strict template errors, escaping and path bounds, deterministic rendering, and
reference-layout reproduction from explicit fixtures. Status snapshot acceptance additionally
covers waiting diamonds, failures with an older available package, stale generations and
serialized updates that preserve package bytes/keys/indexes. Live proof stays in the sandbox.

The accompanying preview uses illustrative data and performs no live operations.
