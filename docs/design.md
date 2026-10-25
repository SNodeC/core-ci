# Proposed implementation

Status: design for approval, not implemented. The current owner specification is the
functional authority. Prior core-ci source, designs and protocols are discarded.

## Invariant and responsibility

An execution has exactly one immutable source/configuration/target/prerequisite selection
and one persisted state. GitHub owns definitions; MariaDB owns execution decisions and
evidence. An edge is satisfied only by verified publication of its selected prerequisite.
Dispatch uncertainty retains the assigned execution and never authorizes a replacement.
The two-core controller performs asynchronous coordination; GitHub does package work.

## JSON schema

Keep schema 1 and the exact owner-proposed top-level fields: schema, title, package,
depends_on, build and matrix. build maps native/openwrt to source-relative hooks.
Each matrix row contains id, family, distribution, suite, arch, runner and build.
Native build parameters are image, platform and format (deb/rpm); OpenWrt parameters
are release, target, format (ipk/apk) and sdk containing HTTPS url and sha256.
Preserve runner values and worker parameters. Do not maintain a compiled platform list.
Do not duplicate arch inside build or suite with another series field.

Validate namespace ownership, unique row IDs/coordinates, enrolled dependency names,
cycles, missing targets, safe hook paths and worker parameter contracts. Resolve hooks
inside the exact selected checkout, rejecting symlink/path escape. Target edges require
equal row ID and family/distribution/suite/arch/format coordinates. Worker verification
also requires matching actual native ABI/platform or OpenWrt SDK/toolchain identities;
runner identity is not compatibility. Reject mismatches explicitly.

Enrollment identifies a GitHub repository by stable repository ID and owner/name.
The local enrolled name used by depends_on must be unique. GitHub commits persist UI
edits using the previously read blob SHA, with conflict/error feedback. Show the returned
commit SHA only after GitHub confirms the write. Keep last validated definitions with
GitHub revision/hash as evidence, never as independently editable database configuration.
Selected source revisions always supply their own definitions and hooks. Validate the
selected dependency graph before accepting it as effective.

## Graph, source and storage

Use ordinary repository/target nodes with prerequisite and dependent adjacency lists.
Keep immutable execution records and selected prerequisite execution edges in MariaDB.
The in-memory graph is the restored view of those records, not a second scheduler.
Traverse affected dependent subgraphs rather than construct a generic workflow model.

Direct work pins the requested semantic-version tag, including older tags, and its peeled
commit. Dependency work pins the highest numeric semantic-version tag and commit at
selection time, including proper prerelease ordering. Neither default branch nor last
publication is a source fallback. Tag moves create distinct selections. Configuration
edits cannot mutate past inputs. Image digests, SDK checksums/toolchain identities,
workflow/tooling SHAs, selected target JSON and prerequisite artifact identities become
immutable assignment inputs before a build can consume them.

Proposed tables: repository enrollments; revision-addressed validated definitions;
executions with state, target/source inputs, rebuild revision and failure; execution
prerequisite edges; assigned build/publication run/job/artifact evidence; processed
webhook delivery identities. Store references and snapshots for reproducibility, not a
separate configurable matrix or persistent ready queue. Use uniqueness constraints and
transactional state predicates to enforce idempotency. Keep transactions sequential on
the asynchronous connection so unrelated requests cannot interleave transaction bodies.

A dependent requires ALL selected prerequisites. If the owner chooses coherent change
waves, record the originating change on affected executions so a diamond join cannot use
an older B while the relevant B is waiting/building/failed. The wave is causal execution
metadata, not a separate orchestration model. Independent targets progress independently.
Already-tagged enrollment and wave policy await the owner's decision.

## State transitions and recovery

waiting -> building -> publishing -> published; definitive build or publication failure
moves the execution to failed. Eligibility is derived from selected prerequisites.
Reserve assignment and package revision transactionally before a dispatch request.
A building/publishing assignment may have an unknown remote outcome; absence of a run
ID or an inaccessible GitHub result is not failure. Reconcile using execution identity,
configured workflow/ref, repository and exact assigned inputs before binding run/job IDs.
Never redispatch merely because a response was lost. Ambiguous matches remain visible.

Build completion is verified through GitHub before authorizing trusted publication.
Publication evidence must match assigned run/job/artifact identity and verified feed
commit/package checksums. Verify HTTPS availability before declaring installable
publication complete; a feed Git commit alone does not prove hosting has deployed it.
Reject conflicting or state-regressing reports; identical reports are harmless.
Restart restores assignments and inspects their existing remote work before traversal.
Explicit retry retains failed evidence and creates an identified attempt; a new build
reserves a new package revision. Publication reconciliation can reuse identical assigned
bytes without republishing a different identity. No blanket automatic retry policy.

For stable source version V and positive monotonic rebuild R, proposed encodings are
DEB V-R, RPM Version=V/Release=R, IPK V-rR and APK V-rR. Encode semantic prereleases
with each format's native ordering rules and verify with actual package tooling; reject
unrepresentable versions instead of guessing. Reserve identities across target feed
coordinates. Identical inputs do not authorize different bytes under an existing identity.
Record both unsigned build checksums and final signed package checksums, since signing
can alter package bytes. Signed replay preserves the existing published bytes.

## REST and UI

Expose explicit routes in main for repository list/enrollment, definition read/save,
graph/per-target status, requested-version work, explicit retry and GitHub webhook input.
The local UI shows GitHub write errors, committed revisions, pinned sources/dependencies,
state, failures and assigned execution log links. No IdP or user identity subsystem.
Bind development access to loopback. Public configuration-writing access waits for an
owner-agreed deployment protection and TLS arrangement.

Use GitHub App JWT/installation tokens through asynchronous SNode.C HTTPS clients.
Controller App permissions: metadata/read, contents/read-write, actions/read-write and
workflows/write only where workflow enrollment is authorized; subscribe to tag pushes
and relevant workflow/job events. The worker feed-writer App is distinct and restricted
to feed contents writes. Confirm exact App permissions against GitHub before setup.
Authenticate webhook raw bytes with HMAC-SHA256 and constant-time comparison before
JSON processing; verify installation/repository/event and assigned execution identities.
GitHub API reads establish run outcomes; source hook success strings are not authority.
No additional workflow-to-controller reporting authentication is planned initially.

## Workflow and artifact boundary

Ship fresh deployment templates in this repository, with explicit setup to install them
into the authorized source and worker repositories. Enrollment commits packages.json;
authorized workflow enrollment also commits a thin workflow, never silently rewrites it.
The thin workflow accepts execution identity and immutable assignment inputs, has only
source-read permissions, and calls trusted reusable tooling pinned to a full commit SHA.
Explicitly checkout the assigned source SHA, independently of the workflow dispatch ref.
Tool upgrades are reviewed commits updating pinned workflow references.

Reusable tooling resolves/verifies configured images/SDKs and executes native/OpenWrt
hooks with source directory, target JSON, source version, package revision, selected
prerequisite locations and output directory. Native worker setup dispatches on package
format/tool availability, not distribution names; unsupported configurations fail clearly.
Provide small DEB and RPM reference projects/hooks and explicit OpenWrt recipe contracts.
Import selected development exports instead of rebuilding graph prerequisites. Exports
contain only project-owned files and identify target/SDK/toolchain compatibility.

Manifest: execution, repository, source SHA/tag/version, definition revision, target,
revision, prerequisite identities, package filenames/sizes/SHA256/components and optional
development export identity. Bound archive entry counts/sizes and reject traversal,
unsafe links and unexpected files before extracting. Source hooks never supply namespace
or signing authority. Publisher uses standard package metadata tools without executing
artifact scripts or package installation hooks.

A separate trusted publication workflow obtains only the assigned artifact, independently
verifies GitHub identity and actual package metadata, then uses the protected environment.
Serialize writers per feed; start from current feed SHA and perform normal checked pushes.
Preserve retained versions and unrelated feeds. Conflicting updates require reconciliation,
not force-push. An identical replay uses retained bytes/receipt without re-signing everything.

Feed layout: apt/<distribution>/pool and dists/<suite>; rpm/<distribution>/<suite>/<arch>;
openwrt/<suite>/<arch>. Reject incompatible format or SDK identities sharing a feed path.
Use apt-ftparchive/dpkg tooling plus signed Release/InRelease, rpmsign/createrepo_c plus
signed repomd.xml, OpenWrt control/index tooling plus usign Packages signatures, and the
selected SDK's apk-tools v3 adbsign/mkndx for packages.adb. Verify the exact selected
SDK CLI before execution. Never substitute Alpine v2 or allow-untrusted client proof.

GitHub Pages is an optional deployment choice, not assumed hosting. Its published site
limit is currently 1 GB with a soft 100 GB/month bandwidth limit. Confirm suitability
with the owner; otherwise require an actual package-compatible HTTPS destination.

## SNode.C implementation patterns and remaining verification

Verified references: src/apps/database/testmariadb.cpp (SubCommand registration and async
queries); jsonserver.cpp/jsonclient.cpp (routes, shared request/response lifetime and
socket callbacks); MariaDBClient.cpp/MariaDBConnection.cpp (explicit connect, command
sequences, disconnect ownership and lazy establishment); core/SNodeC.cpp (lifecycle).
MQTTSuite mqttstore/mqttstore.cpp, lib/ConfigSections.cpp and lib/MariaDbStorage.cpp
show concrete option registration, inline callbacks and database/request ownership.
MQTTSuite CMake and installed /usr/local/lib/cmake/snodec show public imported targets.
Do not copy example credentials or example retry policies.

Main registers options and instances, calls init once, starts a managed database when
selected and explicitly calls MariaDBClient::connect() once. Initial database success
alone initializes graph/routes and listener. Initial failure logs and releases resources;
start returns naturally. Later commands use existing framework establishment when its
connection object is absent. No application connection retries or mutation replay.
Managed server readiness, shutdown and startup-failure cleanup still need a focused
framework/platform investigation before implementation; no process lifecycle primitive
has yet been verified. Unmanaged mode never controls the administrator's server.

Use find_package(snodec CONFIG REQUIRED COMPONENTS ...) and snodec:: public targets;
no sibling source paths. Copy the canonical formatting baseline byte-for-byte and wire
explicit format/check targets for src/ and tests/. Braces, single final returns, short
concrete methods, inline callbacks and blank task separators are mandatory.

## Deployment inputs and secret inventory

Owner-supplied controller settings: GitHub App ID, installation IDs/repository grants,
private-key file, webhook-secret file, public webhook URL/TLS, listener binding, database
mode/connection/credential files, managed server paths and database names. Deployment
settings: trusted worker repository/full tooling SHA, source workflow paths/dispatch refs,
feed repository/branch/public URL/hosting, signing public-key paths and fingerprints.
Exact SDK URL/checksum and project recipes/hooks come from source owners. No fabricated
values or reuse of discovered credentials. Provision schema with a separate administrator;
runtime grants, backup/restore and recovery steps belong in the implementation setup guide.

Worker environment package-publication secrets:
PACKAGES_GPG_PRIVATE_KEY; optional PACKAGES_GPG_PASSPHRASE;
OPENWRT_USIGN_PRIVATE_KEY; OPENWRT_APK_PRIVATE_KEY;
FEED_WRITER_APP_PRIVATE_KEY. Public keys are reviewed trusted worker keys/ files, also
published in the feed. App IDs, feed names/URLs and fingerprints are variables.
Import secrets into restricted temporary files/keyrings, verify public-key correspondence
and clean up on success/failure. Never generate or rotate deployed keys implicitly.

Setup uses file input, for example:

gh secret set PACKAGES_GPG_PRIVATE_KEY --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/archive-private.asc

Use the same file-input form for each required secret; no values in arguments or logs.
The complete setup guide will specify every file, variable, permission and provisioning
command before activation. Proposed secret names are not evidence that secrets exist.

## Estimate, approval and acceptance

Estimate after removing the old application: 5,500-8,500 new production lines total,
including roughly 3,000-4,500 controller/database lines, 500-900 UI lines, 1,700-2,700
worker/workflow/reference-hook lines and 300-400 schema/build lines. Estimate 1,800-3,000
test lines separately; documentation and canonical tooling copies accounted separately.
These are estimates, not a promised minimal architecture or measured completed diff.
Deleting old mechanisms handles reset but cannot implement absent graph, database, HTTP,
UI or package workers. Reshaping retained application code is excluded by the owner.
Therefore fresh production code and net growth from the empty reset are necessary.
Obtain explicit approval of this scope/growth before implementation; seek further
approval for architectural extensions or material estimate growth.

Implement reviewable vertical checkpoints: enrollment/GitHub commit and database lifecycle;
immutable graph/dispatch/reconciliation; trusted package pipeline and UI; real acceptance.
Establish a real end-to-end path early once owner inputs and activation are available.
Tests must include real MariaDB, exact tags/tag moves, configuration errors, chain/diamond,
independent targets, failure/retry, report duplication/delay and restart with assigned work.
Live sandbox proof requires DEB/RPM/IPK/APK, available architectures, signature rejection
and actual authenticated apt/dnf/opkg/apk refresh/install with runtime checks. Record
unavailable runners/environments explicitly. No synthetic test is live acceptance.
No implementation, external activation, publication or acceptance is claimed here.

## External references reviewed

- GitHub webhook validation: https://docs.github.com/en/webhooks/using-webhooks/validating-webhook-deliveries
- GitHub secret file input: https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-secrets
- RPM signing: https://rpm.org/docs/6.1.x/man/rpmsign.1
- OpenWrt v3 index calls: https://github.com/openwrt/openwrt/blob/openwrt-25.12/package/Makefile
- GitHub Pages limits: https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits
- Owner-linked Debian format page returned HTTP 403; OpenWrt signature guide returned
  an anti-bot page. Their contents were not independently verified in this checkpoint.
