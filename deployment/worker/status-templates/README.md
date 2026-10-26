# Deployment-owned package status templates

This bundle is part of the trusted worker tooling commit. Customize it in the worker
repository, review it, and upgrade the matching approved tooling/workflow pins together.
Source repositories and build artifacts cannot provide templates or presentation settings.

The renderer replaces the hard-coded status-page assembly. It does not schedule work or
change whether a package is eligible/published. Rendering runs in a bounded subprocess with
no inherited GitHub/signing credentials, after temporary signing files have been removed and
before the sole feed push step. A template error fails preparation; it does not imply that
an already-started push failed. No template/rendering dependency is added to the controller.

## Files and configuration

- `presentation.json`: schema 1, `title`, public `parameters`, optional `projects` and `assets`.
- `index.html.j2`: HTTPS root and status index HTML.
- `index.md.j2`: GitHub status index Markdown.
- `project.md.j2`: project-specific Markdown, with safe HTML tables.
- `table.html.j2`: shared include; templates can use includes/macros in this bundle.
- `requirements.txt`: pinned GitHub-side Jinja2/MarkupSafe dependencies.

Owner-defined `parameters` need no new controller option for each field. For example:

```json
{
  "schema": 1,
  "title": "My package feed",
  "parameters": {"support_url": "https://example.org/support", "introduction": "My public introduction"},
  "projects": {"Example/library": {"title": "Example Library", "page": "library", "catalog_url": "https://example.org/catalog"}},
  "assets": ["assets/logo.svg"]
}
```

A template can use `{{ site.parameters.introduction }}` and
`{{ project.parameters.catalog_url }}`. Only explicitly public settings belong here.
Project `page` selects a bounded status-page basename, so `snode.c` can use `snodec.md`
without a compiled-in repository mapping. The template context exposes `project.status_path`;
colliding output names fail explicitly. Required missing variables raise errors; absent optional facts use null/empty values.
HTML escaping is enabled for every output. Default Markdown uses HTML tables so package
metadata cannot introduce Markdown table/link syntax. Custom Markdown outside HTML needs
appropriate Markdown-context escaping; HTML escaping alone is not sufficient there.

Assets must be regular files below this bundle's `assets/` directory and are copied only
below feed `status/assets/`. Links, traversal and linked output parents are rejected.
The bundle is bounded to 4 MiB, each rendered page to 4 MiB and total outputs to 32 MiB.
Rendering does not accept arbitrary output paths. Keys, package bytes, receipts and signed
indexes are not output destinations. Templates remain trusted deployment code; this is not
an invitation to execute arbitrary hostile templates in a privileged workflow.

## Template context

`publications` retains the existing repository/target/package/version, source commit,
checksum and package/receipt/build/publication URL fields. It adds structured `coordinates`,
selected `target_row`, selected `source`, `revision`, `dependencies`, `environment`, `summary`, `build_job`,
`build_duration_seconds`, `published_utc` and `work`.

`projects` groups these rows by repository. A project contains `repository`, display `title`,
owner `parameters`, all its `publications`, and coordinate-grouped `targets`. Each target
contains `coordinates`, retained `publications` and optional `work`. No template decides
which source to build or invents another version comparator. Default pages show all retained
versions; they do not claim that lexical ordering identifies the newest available package.
The context is also written to `status.json`; the existing `publications` array stays present.

New receipts retain compact selected-prerequisite references: execution/attempt and assignment
identities, source, revision, compatibility, packages, development export and feed. They do not
recursively embed prerequisite receipts or capture another graph snapshot. These describe
consumed immutable inputs; they cannot show the prerequisite's current controller state.

Job timing is read from the already verified assigned GitHub job, without another API call.
The unprivileged trusted validator attaches it only after rejecting source artifacts that
claim verified job facts. Publication rechecks its run/job/attempt binding against the
assignment's build result. Duration is completed-at minus started-at for that whole build job;
it includes setup/upload/cleanup, not only the compiler's runtime. Missing times remain null.

Compiler/test data is optional **hook-reported display information**, not independently
attested testing and never permission to publish. The hook may add this to `outputs.json`:

```json
{
  "packages": [{"file": "library.deb", "component": "library"}],
  "summary": {
    "compilers": [{"name": "cc", "version": "actual compiler version"}],
    "tests": {"passed": 42, "failed": 0, "skipped": 1, "coverage_percent": 78,
              "report_url": "https://example.org/public-test-results"}
  }
}
```

Compiler strings and arrays, test counts, optional coverage and HTTPS report links are
bounded/validated and carried through manifest verification into the new publication receipt.
Summary keys cannot replace source, target, assignment, run or publication state. The native
reference hook reports the compiler it invoked and its one actual successful runtime test.
Old receipts remain untouched; facts not recorded in them stay null.

## Current GitHub-side boundary

This phase is publication-only. `work` is null: a worker cannot infer waiting/failed controller
state from retained receipts. `published_utc` stays null unless supplied by recorded verified
publication evidence; no render/prepare time is substituted. The controller records its first successful
verification time separately as `publication_verified_utc` in its persisted receipt and displays
it in management execution details. Exporting that fact to public templates and supplying a
minimal current-work projection still require the status-update integration. Therefore the
full information content of the SNode.C reference pages is not yet reproduced by this phase.

Run locally with the pinned dependencies installed:

```sh
python3 deployment/worker/core_ci/feed_status.py --feed /absolute/feed --templates /absolute/trusted-bundle
```

The CLI prints the exact generated path list. It does not push, sign packages, execute hooks,
contact GitHub or change graph state. Local fixture tests are not live GitHub acceptance.

The job timestamp fields come from [GitHub’s assigned job API](https://docs.github.com/en/rest/actions/workflow-jobs#get-a-job-for-a-workflow-run).
Template syntax and escaping follow the [Jinja documentation](https://jinja.palletsprojects.com/en/stable/templates/).
