# Extracted GitHub interoperability contracts

Extraction date: 2026-10-31. Reference root:
`/home/voc/projects/snodec/core-ci`. These are external data/wire contracts,
not a design for the replacement controller. Reference files have local changes;
the hashes below identify the observed bytes rather than assuming HEAD does.
No reference controller, test, UI, SQL, proposal, report or log is an input.
This document records observations and explicitly identifies remaining gaps.

## Provenance

| Relative reference path | SHA-256 of observed bytes |
| --- | --- |
| deployment/sandbox/definitions/demo-app.json | cf38da0ebbb191b946b8be9e26add74be9953e060e015585df46637bdabdf493 |
| deployment/sandbox/definitions/demo-core.json | 35c699e3388be7478bfc72f44ad8577bf9cac5d6124ded59e7e429f63df6058e |
| deployment/sandbox/definitions/demo-join.json | b924f44750e8d22088a591bc119929c36fbe3d4114cd56e58c38fc33f8d744e4 |
| deployment/sandbox/definitions/demo-side.json | 37269b588701bb097ccb1b1ebad63e0b419419d0dc7bf62d043fd2514424afd8 |
| deployment/worker/core_ci/contract.py | 80f7bb8d8cf75dad12959ca9e6eea2c7774dc93e6b889db4693c78bf7533318f |
| deployment/worker/core_ci/validate.py | d11b0cf99c018ddebf8c680c929bdf81d5dfd8cedf491d1f913848e9f1081917 |
| deployment/worker/core_ci/github.py | 2db2a5ec36db9a720951f1dcc000f630e12d4788cc0d5aa1a108706cddecd5d3 |
| deployment/worker/.github/workflows/core-ci-build.yml | b9ae384979a17cea827cb6525d312172ec1058d5d6b411651837eb1d35a61e16 |
| deployment/worker/.github/workflows/core-ci-publish.yml | 560d892e2ea0cbe0524a8265e0f97d6fbdc0ff6af4d465c69f705b5604e0e939 |
| deployment/source/core-ci-build.yml.in | 041a95157a67c2e2ea20d82c5946f299e692215f168ddbf5ef9125a9ffa6fc9a |
| deployment/worker/core_ci/native.py | 0ac5f2e0dbecb74b659117ecadc97aa536bc8db484c9d496e268fd2831bfd822 |
| deployment/worker/core_ci/sign_native.py | d962316cb50700626a6981a9e2295933fab7847e403a248f7aba7cc41921910f |
| deployment/worker/core_ci/sign_openwrt.py | 85c255b4db10ac555474a2a8c5171a1aa62760d8b8c3af4d56db928f265e05c0 |
| deployment/worker/core_ci/publish.py | 1f3f81b1e62e40137bac1820568b58141c99c1e613b73bf55af50d2860576d7a |

The GitHub REST read of `VOC-Experiments/demo-app:ci/packages.json` returned
Git blob `4d7744361faa1724e08c5a218d3baab822d92e5e`; its decoded bytes have
the same SHA-256 as the local demo-app definition above. This observation does
not authorize live runs or establish that any other repository matches examples.

Additional narrow extraction sources are the definition/provenance checks in
`deployment/worker/core_ci/native.py`, the receipt serializers in
`sign_native.py`/`sign_openwrt.py`, `publish.py:run_identity`, and the access,
workflow and tag-selection statements in `docs/deployment/README.md`.

## Committed source definition

`ci/packages.json` is JSON with observed fields:

| Field | Observed contract |
| --- | --- |
| schema | Integer 1 |
| title | Human-readable project title |
| package | Package namespace; worker validates `[a-z0-9][a-z0-9+.-]{0,127}` |
| depends_on | Array of package-name strings; empty for a root |
| build | Family to source-relative hook path, e.g. native/openwrt |
| matrix | Array of exact target objects |

Each observed target has `id`, `family`, `distribution`, `suite`, `arch`,
`runner`, and `build`. Native build objects contain `image`, `platform`,
`format` (deb/rpm). OpenWrt build objects contain `target` (e.g. x86/64)
and `format` (ipk/apk). These are external worker formats, not graph roles.

The worker verifies the source checkout commit, definition Git blob, exact target
membership in matrix, and equality of package/title/depends_on/build with the
assignment source. A hook must resolve to a file inside the selected checkout.
Definitions must be read as committed GitHub data and bound to their Git identity.

The deployment guide specifies that an explicit empty-tag request selects the
repository's highest SemVer; an explicit tag can recover an older notification.
The worker accepts optional leading `v`, three nonnegative decimal components,
optional dot-separated prerelease and build identifiers. Core components cannot
have leading zeroes; numeric prerelease identifiers cannot have leading zeroes.

The observed definitions have no dependency-version range field. Runtime package
dependency ranges are source-hook/package metadata, not invented JSON fields.
The worker verifies package metadata and records its `dependencies` value.

## Exact target compatibility

`contract.py:compatibility` produces an object containing family, distribution,
suite, arch, format, plus native platform; OpenWrt adds resolved release,
build.target, sdk_sha256 and toolchain. The worker requires exact equality
between a selected environment's compatibility and each prerequisite receipt's
compatibility, and also its development export compatibility if present.
Prerequisite execution and attempt must equal the embedded receipt identities.

The owner-supplied rules below establish source-version selection. Equal target
coordinates must identify exactly one target; ambiguous matches are rejected.

## Assignment and hashing

Canonical JSON is UTF-8, sorted object keys, no insignificant whitespace,
Unicode preserved (`ensure_ascii=False`), separators comma/colon. SHA-256 is
lowercase hex over these canonical bytes, without the JSON file's final newline.

Top-level assignment fields consumed by the workers:

| Field | Contract |
| --- | --- |
| schema | 1 |
| execution | 64 lowercase hexadecimal characters |
| revision | Integer, greater than 0 and less than 2^64 |
| attempt | execution + ':' + decimal revision |
| artifact_name | `[A-Za-z0-9_-]{1,200}` |
| label | Human-readable label used verbatim in current run/job names |
| source | Immutable source and deployment object described below |
| target | Complete exact target object from the committed matrix |
| dependencies | Array of objects containing execution, attempt and exact receipt |

Source fields consumed include `repository`, `repository_id`, `title`, `package`,
`tag`, `commit`, `definition_revision`, `definition_blob_sha`, `depends_on`,
`build`, `deployment`. Source commit, definition revision and definition blob
are 40 lowercase hex characters. `deployment` contains:

- `controller_app_bot_id` (numeric).
- `worker`: repository, repository_id, tooling_sha (full 40-character commit).
- `source_workflow`: path, blob_sha.
- `publication_workflow`: path, blob_sha.
- `feed`: repository, repository_id, branch, base_url.
- `signing`: gpg_fingerprint, usign_key_sha256, apk_key_sha256.

The worker does not specify an execution-hash preimage or an artifact-name
construction algorithm. Its accepted identity constraints are recorded above.
Any controller choice must preserve immutable attempt identity and avoid reuse.

## Dispatch, run and job linkage

The thin source workflow accepts four required string workflow_dispatch inputs:
`execution`, `attempt`, `assignment_sha256`, `assignment`. It calls the worker's
reusable `.github/workflows/core-ci-build.yml` at the rendered tooling SHA,
with exactly those four inputs and read-only contents/actions permissions.

The publication workflow accepts those same inputs and optional string
`build_result`. Its environment is `package-publication`. Feed mutation is
serialized by its existing GitHub publish-job concurrency group; the controller
must not implement a second feed lock or alter the workflow.

Current run names are `Build ` + assignment.label and `Publish ` + label.
The build job is `Build ` + label, either exact or a reusable-workflow name
ending with ` / ` + that name. Legacy no-label naming is present in the worker
validator but is not a reason to add compatibility to the fresh controller.
The publisher job has name `Publish ` + label. Workflow dispatch ref and selected
source commit are distinct: the worker explicitly checks out source.commit.

Build run validation requires:

- Repository REST metadata ID equals source.repository_id.
- Run repository.id and head_repository.id equal that same ID.
- actor.id equals deployment.controller_app_bot_id.
- event is workflow_dispatch; run_attempt is 1; status completed; conclusion success.
- head_sha equals the assigned run's workflow_commit; path equals source_workflow.path.
- Contents API at that head SHA yields the approved source_workflow.blob_sha,
  also equal to run identity.workflow_blob_sha.
- display_title exactly matches the assignment's build run name.

The artifact job must have matching id, run_id and run_attempt 1, completed/success,
and the build-job name described above. The publication worker rejects any actor
other than the configured numeric bot ID and any invocation other than
workflow_dispatch/run attempt 1. It checks worker name/ID, selected tooling SHA
against checked-out tooling and trusted workflow configuration, feed identity and
GPG fingerprint against its trusted deployment configuration.

## Build result and artifact retrieval

The build-result JSON consumed by the publisher requires:

- assignment_sha256: exact assignment digest.
- run: at least id, workflow_commit, workflow_blob_sha.
- artifacts: exactly one object, with id, name, job_id, digest, size_bytes.

Run details are retrieved from `/repos/{source}/actions/runs/{id}/attempts/1`;
job details from `/repos/{source}/actions/jobs/{job_id}`; workflow file identity
from `/repos/{source}/contents/{workflow_path}?ref={head_sha}`.
Artifact identity is retrieved from `/repos/{source}/actions/artifacts/{id}`.
Its id/name match the assignment and build result; workflow_run.id matches the
run; expired is false; digest and size match the result. Digest is
`sha256:` plus 64 lowercase hex characters; size is 1..512 MiB.

The authenticated GET of `/repos/{source}/actions/artifacts/{id}/zip` yields a
302 to HTTPS, port 443/default, no userinfo/fragment, on a subdomain of
blob.core.windows.net or actions.githubusercontent.com. The storage download
has no GitHub credentials and follows no further redirect. Exact archive size
and SHA-256 must match. The archive contains `manifest.json`, the declared
package files and optional `development.tar.gz`. No source hook is executed
while validating artifacts. Environment-only evidence is a separate artifact
named assignment.artifact_name + '-environment', containing environment.json;
it cannot establish successful build completion.

Archive constraints: compressed/file bound 512 MiB, expanded bound 1 GiB,
20,000 entries; no duplicate paths, encryption, symlinks or special files.
Relative paths have 1..512 printable ASCII characters, no space/control,
backslash, absolute prefix, empty segment, dot or dot-dot segment.

## Manifest and package metadata

The manifest repeats schema/execution/attempt/revision/artifact_name/source/
target/dependencies exactly from the assignment and adds assignment_sha256,
environment, package_version, packages, development_export and optional summary.
Its source artifact cannot supply trusted build_job facts. The trusted validator
adds build_job only after inspecting the assigned GitHub job.

package_version is `{version,release,full}`. For deb/rpm, release is decimal
revision; for ipk/apk it is 'r' + revision; full is version + '-' + release.
Upstream is the tag's three numeric components. Prerelease identifiers encode
as numeric tokens: a numeric identifier yields 1, decimal length, each decimal
digit; a nonnumeric identifier yields 2, each character code, 0; append final 0.
For apk append '_pre0' then '_p' + token for each token; other formats append
'~' then dot-joined 'p' + token. Build metadata is not part of package version.
The encoded version-release cannot exceed 240 characters.

There are 1..128 package objects. Each contains file/component/size/sha256/name/
version/arch/dependencies. Component equals actual package name, belongs to the
assigned namespace (exact package or package + '-' prefix), and actual version
and arch equal assignment-encoded version and target.arch. Dependencies reflect
the actual format's package-tool output: native deb/rpm arrays, IPK string,
APK array. Formats and package identities are independently checked by workers.
Optional development_export contains file='development.tar.gz', size, sha256,
compatibility, and archive content only under include/lib/share.

Optional summary has only compilers/tests, at most 8 KiB canonical JSON.
Compilers: at most 16 objects with name/version strings of 1..512 noncontrol
characters. Tests: required passed/failed/skipped integer counts 0..10,000,000;
optional coverage_percent 0..100 and report_url of at most 2,048 characters,
HTTPS, hostname, no credentials/control characters.

## Receipt and independent publication proof

Receipt path is `receipts/{execution}/{revision}.json` under feed.base_url.
Receipt serializer fields:

- schema=1, execution, attempt, assignment_sha256, source_commit, revision, target.
- source: repository/title/package/tag/definition_revision/definition_blob_sha.
- dependencies: each prerequisite receipt projected to execution/attempt/
  assignment_sha256/source_commit/source/revision/compatibility/packages/
  development_export/feed (missing values serialize null).
- summary, build_job, run, build_result, feed, environment, compatibility,
  packages, development_export, indexes.

Publication run identity is id, run_attempt=1, repository_id, actor_id,
workflow_blob_sha, workflow_commit, html_url. It is bound to the approved
publication path/blob, workflow_dispatch and controller bot.

Published package objects retain manifest identity metadata, add path, replace
size/sha256 with published bytes, and add unsigned_sha256 from the manifest.
The extracted local serializer replaces `file` with the published filename: DEB/IPK use
`name_version_arch.format`; RPM uses `name-version.arch.rpm`; APK uses
`name-version.apk`. Input artifact filenames are not required to use these names
or extensions. The receipt path ends in its published filename.
The signer may change bytes. Each index is `{path,size,sha256}`. Exports keep
manifest export metadata and add path/size/sha256. The exact export path is
`exports/{execution}/{revision}/development.tar.gz`.

Native paths use distribution/pool/suite and distribution/dists/suite for DEB,
distribution/suite/arch/Packages with repodata for RPM; OpenWrt uses
openwrt/suite/arch. Actual package and index paths come from the receipt.
Public signing paths are keys/apt.asc, keys/opkg.pub, keys/core-ci-apk.pem.
The worker records prepared receipt SHA-256 and feed revision in publish-job
outputs. A successful run alone is not proof of public availability.

The owner subsequently specified the receipt-only proof rule below. HTTPS must
validate certificates and the independently served receipt must equal the trusted
committed bytes. Its assignment/source/target/dependencies/build result/run and
package identities must match the authorized attempt. The controller does not
fetch current indexes, inspect membership, or wait for later feed writers.
Indexes recorded in a receipt describe its writer's mutation, not a continuing
availability condition. Signing paths and signing identities remain unchanged
and are enforced by the trusted publication workflow.

## Authorization and deployment inputs

The documented controller App uses Contents read, Actions read/write and metadata
read, installed only for explicitly granted source and trusted-worker repositories.
Configured owner/repository names have numeric ID grants; a display name cannot
broaden access. Installation tokens are scoped to that ID subset and permissions.
The feed writer is a separate GitHub credential confined to the publisher.
Subscribed events are push, workflow_run and workflow_job. Raw-body webhook
HMAC is mandatory, including SSL verification; there is no custom callback protocol.

The management guide specifies trusted local access on 127.0.0.1:8080, JSON POSTs,
and no application login/IdP. External access protection must be agreed before
exposing management. The sandbox document records a previous explicitly approved
public test listener; that is historical access, not authorization for this new
repository's deployment. Webhook path is /github/webhook, documented on a
separate named TLS listener. Do not add the public/status portal to this task.

Required public deployment inputs include App ID, installation ID, numeric bot
ID, owner boundary and repository-ID grants; worker repository/ID/tooling SHA;
source and publication workflow paths/approved blobs/dispatch refs; feed
repository/ID/branch/HTTPS root; approved public-key hashes/fingerprint.
They must be supplied or read from approved public sources and frozen with work.
Example documentation values are not silently adopted as current approved pins.
No private configuration or credential contents were inspected.

## Owner-supplied selection and publication rules (2026-10-31)

The owner explicitly supplied the intended rules after extraction. They are
requirements for this implementation, not assertions about reference behavior.

An explicit tag builds exactly that application tag. Each transitive prerequisite
selects its highest valid SemVer at selection time, by SemVer precedence. Do not
use tag dates, application dates, automatic downgrades or a compatibility solver.
Freeze source identities and exact matching target executions. Reuse that exact
execution whether waiting, active or verified published; an older application
request does not force a prerequisite rebuild. Never substitute inputs after
selection or repeat discovery when releasing dependents. Runtime compatible
ranges remain source-owned; exact build consumption uses selected receipts.

Proof compares an execution-specific receipt from a trusted committed GitHub
feed revision with the receipt served over the configured public HTTPS origin.
Bind its assignment, attempt, source, build result, publication run and package
identities to the authorized work. Current shared index bytes and membership are
not this execution's publication proof. A later writer cannot invalidate it.
Missing/mismatched receipts preserve uncertainty and never authorize replay.

On sufficient evidence, commit the publication and newly eligible dependents
together. After successful commit, dispatch all commands immediately. No feed
idle wait, batching delay, global refresh, repeated tag discovery, unrelated
reconciliation or additional controller admission policy is permitted.

No additional source-actor restriction was found beyond the documented
repository/installation/owner grants. This records the authorization that was
actually found; it does not introduce a new allowlist requirement.

## Live sandbox wire identity (2026-10-31)

The existing standard source and publication workflows on GitHub both pin
tooling `e7b1423cc9537070f84aea7bf7893fb590f75301`. Their Git blobs are
`30c4d3074c5031f4b904f70b106eeabd655f52dd` and
`c2b5e78b721d2e649490baa390e3ea5e7f8cdf9b` respectively. Public deployment
inputs for the authorized live run freeze these observed identities explicitly.
The saved local configuration has older pins; those were not silently used.

At this frozen tooling revision, the OpenWrt receipt serializer publishes IPK
and APK filenames as `name-version.format`; the local extraction above observes
a different IPK naming choice. This is the same receipt field and identity
contract, with a publisher-selected output filename. The controller must not
recalculate that filename from package metadata. It checks the exact filename
against the receipt path, safe relative paths and the assigned format, while
binding package identity and unsigned digest to the validated build manifest.
Independent committed/public receipt byte equality remains mandatory.
[Frozen OpenWrt receipt serializer](https://github.com/VOC-Experiments/ci-coordinator/blob/e7b1423cc9537070f84aea7bf7893fb590f75301/core_ci/sign_openwrt.py).

The controller uses its existing narrowly scoped installation token for public
feed metadata and committed receipt reads. The live API confirmed those reads
are permitted without adding a feed repository grant or permission. Anonymous
reads exhausted GitHub's lower anonymous quota during real notifications; public
HTTPS receipt retrieval remains credential-free.

## Standard GitHub authentication

Incoming webhooks use the exact raw body with HMAC-SHA256 and compare the full
`sha256=` header in constant time before interpreting it. The official example
vector is used in the focused authentication test.
[GitHub webhook validation](https://docs.github.com/en/webhooks/using-webhooks/validating-webhook-deliveries).

App JWTs use RS256, the supplied App identity as iss, iat with 60 seconds of
clock-drift allowance, and exp less than ten minutes into the future.
[GitHub App JWT](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-a-json-web-token-jwt-for-a-github-app).

Installation access tokens come from
`POST /app/installations/{installation_id}/access_tokens`, authenticated with
the App JWT, requesting the configured repository_ids and minimum permissions.
[GitHub installation tokens](https://docs.github.com/en/rest/apps/apps#create-an-installation-access-token-for-an-app).

Dispatch uses `POST /repos/{owner}/{repo}/actions/workflows/{workflow_id}/dispatches`
with ref and inputs. The current official reference for API version 2026-03-10
documents HTTP 200 with workflow_run_id, run_url and html_url. This response
does not replace subsequent validation of repository/actor/workflow/job evidence.
[GitHub workflow dispatch](https://docs.github.com/en/rest/actions/workflows#create-a-workflow-dispatch-event).

## Live sandbox renewed certificate workflow inputs (2026-10-31)

The initial selected standard tooling `e7b1423cc9537070f84aea7bf7893fb590f75301`
contains an expired public GPG certificate (expiration 2026-10-29). Its SHA-256
is `cc1ac352f62047eb693040b876e4c6bf6ca10da93cd8bb6bb130a618c572086d`.
The deployed feed certificate has SHA-256
`86b51a8df13821f88036ad6c5f810a2fa27aa47961124c9783ba28ebe6625ef0`.
Both bind fingerprint `A4B307A9CFD583089547481DE58C165F4340EE99`;
packet comparison identifies a renewed self-signature/expiration, not a different
primary key. The byte-equality publication guard correctly rejects the old
certificate. No certificate rollback, private-key change or guard bypass is used.

The four public source repositories already contain
`.github/workflows/core-ci-build-renewed.yml`, identical Git blob
`d413f2396dd9c05bc4890f4e59c556dacf3b9570`. Each calls the coordinator's reusable
build workflow at exact commit `230e594f78ccf51108cba5db910ea41f87f59876`.
The existing coordinator `.github/workflows/core-ci-publish-renewed.yml` is blob
`e4ca49f44c893aa9ce64e4a4b5475a1390f319cc` and selects the same tooling commit.
Its committed public certificate exactly matches the deployed feed certificate.
These are existing public deployment inputs, not newly authored workflows/pins.

The renewed pair retains the four build dispatch inputs and five publication
inputs documented above; `build_result` is optional in YAML but remains supplied
for every build publication. Assignment validation without `kind`, receipt
serialization, trusted deployment validation and publication run identity are
unchanged from the initial pair. The added worker retention branch is outside
controller scope and is never selected. Reusable build job naming, build artifact
location and publication job naming preserve the documented correlation.
Existing workflow-side SDK preparation and feed concurrency remain on GitHub.
No workflow is edited and no preparation job is added by the controller.

A new explicit selection freezes these renewed tooling/workflow identities.
Original selections, failed attempts, published results and waiting dependents
retain their old identities. They cannot be rebound by a retry or configuration
change. Matching source tags with different tooling/workflow identities represent
different exact work; their rebuild revisions remain distinct in the same graph.

GitHub documents a 65,535-character maximum `workflow_dispatch` inputs payload:
[official workflow syntax](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#onworkflow_dispatchinputs).
The controller must not substitute an arbitrary smaller assignment limit or roll
back a verified prerequisite because of it. Actual dispatch acceptance/rejection
belongs to the existing REST boundary. The live renewed RPM join assignment is
62,053 ASCII characters, including all three exact prerequisite receipts.

### Dependency dispatch receipt fields

Narrow inspection at renewed tooling commit
`230e594f78ccf51108cba5db910ea41f87f59876` confirms that
`native.py:prepare_dependencies` consumes receipt execution/attempt, feed,
compatibility, packages and optional development_export. `environment.py` also
requires environment for inherited OpenWrt SDK identities. The assignment
validator permits additional receipt fields and does not require `indexes`.
The receipt serializer's dependency identity subset already excludes indexes.
Thus a build dependency input may omit its unused shared-index inventory while
retaining every other field, including environment; no new field or lookup
protocol is introduced. Full original publication receipts and proof hashes stay
unchanged in MariaDB and on HTTPS. This distinction matters for large RPM index
inventories and joins. The complete old RPM join assignment was rejected by
GitHub with HTTP 422, despite its decoded assignment string being under 65,535
characters; summing input value lengths alone is not an acceptance guarantee.

An explicit retry creates a new attempt/assignment through the normal readiness
path. The execution's source/target and prerequisite execution IDs remain frozen.
Those prerequisite attempts were verified published before the original build
and are terminal, so their identities and all consumed input facts cannot change.
Earlier assigned JSON remains immutable, including its original index inventory.

## Owner clarification: source-triggered forward propagation (2026-10-31)

A source tag or explicit build selection initiates work for that repository and
its transitive enrolled dependents. For the sandbox diamond: core selects core,
app, side and join; app selects app and join; side selects side and join; join
selects join. This is generic committed-definition dependency behavior, never
repository-name policy. Recursive prerequisite creation/reuse remains accepted.
Resolve the requested source tag exactly and other selected source tags once by
the existing highest-SemVer rule. Reuse exact prerequisite work, freeze the new
relationships, and preserve earlier executions unchanged. Publication releases
only ready exact dependents, without further selection/discovery or a level-wide
barrier. No manual downstream build requests may substitute for proving the
source trigger's propagation. The owner requires no production-code growth for
this correction; ask if that cannot be achieved.

## Owner correction: existing workflow confirmation gates publication (2026-10-31)

This supersedes the earlier publisher-job-only override. Publication authority
requires the matched Publish job and exactly one Confirm public receipt job to
complete successfully in the same authenticated, pinned run and attempt. A
whole-run success with skipped confirmation is insufficient. Only after both
jobs succeed does the controller read the execution-specific committed GitHub
receipt and apply the existing execution/attempt identity checks.

The existing workflow performs its exact prepared receipt SHA-256 comparison
over the configured public HTTPS feed. The controller does not duplicate that
check, fetch mutable indexes or revalidate the publisher's signing/package
inventory. Pending, missing, skipped or failed confirmation cannot release
exact dependents; an already successful feed mutation remains unresolved rather
than definitely failed. Historical Published executions remain terminal.

Publication and newly eligible dependent assignments commit together before
dispatch. Relevant authenticated workflow notifications or explicit reconciliation
observe facts; no new polling, replay or global feed-idle wait is introduced.
The existing timestamp keys continue to record committed receipt-read timing;
the UI retains its Publication recorded label and existing layout.

The sandbox workflow condition is corrected so confirmation runs after a successful
Publish job even when native targets skip OpenWrt tools. Confirmation stays outside
the existing feed-writing concurrency group. The reviewed content's Git blob is
`ce11809b876f119ccb521edd27aee66b7c39d0ac`, derived from the exact existing blob
`e4ca49f44c893aa9ce64e4a4b5475a1390f319cc` by the condition-only patch. New selections
must freeze the corrected approved pin. Existing assignments are never rebound.

The owner authorized applying this sandbox correction, updating its public runtime
pin, restarting locally and running one new core-triggered full chain. Receipt
confirmation observes one HTTPS client/edge; it is not a guarantee of simultaneous
package availability at every CDN edge. No package-download retry or logging
change is part of this correction.


## Startup observation of existing assignments (2026-11-02)

After the management listener successfully starts, the controller reads one
MariaDB snapshot and observes the latest assigned Building/Publishing attempts.
Each observation binds to that snapshot's immutable attempt identity and uses
exactly the existing explicit-reconciliation/webhook observation path. No new
GitHub authentication, callback, assignment or result contract is introduced.

Finished remote work can supply the same validated results/confirmation receipts
as a delivered webhook. Normal state-machine commits record those facts and any
newly authorized next stage or exact dependent together before dispatch. Terminal
and unassigned work is skipped; running, unbound and uncertain assignments remain
intact, without automatic dispatch replay/replacement/retry. An error in observing
one execution does not suppress independent observations. The pass is once per
startup, not polling, and adds no durable state or graph readiness authority.

This recovers missed execution notifications for already selected work. A source
tag created while the controller was unavailable, with no selection persisted,
is not discovered by this pass. All durable unfinished assignments are eligible;
there is no age filter or new policy for abandoning history. Startup recovery is
source-tested independently of historical sandbox acceptance; replacing/restarting
the running controller and any resulting live continuation require authorization.
