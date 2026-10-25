# Refined core-ci implementation proposal

Status: complete proposal for review, not an implementation or deployment claim.
This revision follows the current owner specification and incorporates the independent
[Claude review](claude-design-review.md). No discarded core-ci source or design is reused.

Owner SDK policy update on 2026-10-25 supersedes the original exact source-owned SDK fields:
use the latest stable SDK for each configured OpenWrt suite/target on new selections. No
SDK SHA, URL or exact patch release is maintained in repository JSON. Resolve these through
the configured official metadata service and freeze the resulting identities per execution.
Existing execution evidence remains immutable. The owner approved GitHub-owned evidence artifacts and read-only retry retrieval; see section 4.

## 1. Scope, roles and decisions

Build a small persistent dependency graph with an integrated state machine. The
single-threaded SNode.C controller coordinates GitHub and MariaDB, serves a local web UI,
and performs no compilation, package processing, indexing or signing. GitHub does that work.
No IdP, passwords, MFA, role system, second event loop or application thread pool.

| Role | Responsibility | Initial acceptance setting |
| --- | --- | --- |
| Application repository | Controller, UI, fresh deployment templates and documentation | SNodeC/core-ci |
| Enrolled source repositories | Authoritative definitions and source-owned hooks/recipes | VOC-Experiments/demo-core, demo-app, demo-side, demo-join |
| Trusted worker repository | Pinned reusable builders, trusted publisher and reviewed public keys | VOC-Experiments/ci-coordinator |
| Feed repository | Retained packages, signed indexes, publication receipts and public keys | VOC-Experiments/packages |

Repository names are deployment settings. Production SNodeC repositories, credentials,
workflows, feeds and tags are outside scope. External activation requires a separate
reviewable handover with owner-supplied values. Existing keys are never replaced implicitly.

Owner decisions received on 2026-10-25:

- Enrollment immediately requests the highest numeric semantic-version tag, if one exists.
- New selections supersede older waiting automatic work, preserving its history.
- Dependency-triggered selection includes semantic-version prereleases.
- Runtime packages preserve source-declared compatible dependency ranges; graph builds
  always consume exact selected artifact versions. Build-only edges need no runtime dependency.
- A prerequisite target with no selected execution produces a visible work/configuration
  error until its work is explicitly requested; no implicit upward prerequisite builds.
- An explicit successful retry of older inputs may become the feed's newest rebuild of
  the same source version, using its newly reserved revision.

Diamond safety is mandatory, not a policy question. Assigned work and explicit requests
for older tags are not silently superseded. Other unresolved choices are listed in section 16.

## 2. Architectural invariants

1. GitHub JSON is the build-definition authority. Database copies are revision-addressed
   evidence, not independently editable matrices.
2. One graph contains repository/target selections and explicit prerequisite relationships.
   MariaDB transactions authorize transitions; there is no durable ready queue.
3. A logical execution has one immutable selected source, definition, target and prerequisite
   execution set. Its attempts preserve dispatch/build/publication history across explicit retry.
4. A join becomes eligible only after every specifically selected prerequisite execution
   has a verified publication. An unrelated or older published prerequisite cannot satisfy it.
5. Every dispatched attempt has one immutable assignment, including actual prerequisite
   publication identities and tooling/workflow revisions. Workers resolve and record image/SDK
   content identities before using them; they are execution evidence, not controller discovery.
6. Dispatch authority is committed before the HTTP request. Unknown dispatch or COMMIT
   outcomes preserve uncertainty and never authorize another request or replacement.
7. Published package identities are immutable. Publisher replay preserves existing signed
   bytes; a rebuild that can change bytes receives a new revision.
8. The publisher trusts deployment-owned assignment and keys, independently checks actual
   packages, and never obtains authority from a source manifest or executable artifact.

## 3. Authoritative JSON and enrollment

Finalize schema 1 with the owner's field layout:

| Location | Required fields and meaning |
| --- | --- |
| Root | schema=1; title; package namespace; depends_on enrolled names; build hook map; matrix rows |
| build | native and/or openwrt repository-relative hook paths used by the corresponding rows |
| Every row | id, family, distribution, suite, arch, runner, build |
| Native row build | image, platform, format=deb or rpm |
| OpenWrt row build | target, format=ipk or apk; latest SDK resolved per suite on GitHub |

Runner may be a GitHub-supported selector string or label array; preserve it without
rewriting. Runner-group objects require a separately specified worker capability, not a
controller guess. Preserve supplied worker JSON; validate against the pinned worker's
versioned contract and reject unknown/unsupported parameters explicitly. The controller
contains no distribution list, architecture alias table, SDK catalogue or runner mapping.
Arch appears only at row level; suite is feed series. The latest stable SDK patch is
resolved automatically from deployment-owned metadata. Its exact HTTPS URL/checksum belong
to execution evidence, with no SDK hash maintained by repository owners.

Enrollment stores stable GitHub repository ID, full name and a unique local dependency
name. Namespace ownership is exclusive: package names must equal the enrolled namespace
or start with that namespace followed by '-'. Reject overlapping namespace claims and
undeclared components. This is the proposed concrete interpretation of package namespace;
non-prefix component naming would need an explicit source-schema change, not a bypass.

Validate unique row IDs and coordinate matches, safe path/namespace strings, known enrolled
dependencies, cycles and missing prerequisite targets before accepting an effective graph.
Hook paths are relative, remain inside the exact checkout after symlink resolution, and
cannot access a different source checkout. Bound JSON size and graph size explicitly.

The UI reads and conditionally writes ci/packages.json on the configured editing branch.
Use GitHub's current blob SHA to reject concurrent overwrites. Show HTTP/auth/validation/
conflict failures accurately. After success, show the committed SHA and definition hash.
A save is not an execution: show 'committed; not selected by a build tag yet' where applicable.
Each execution reads the definition and hooks from its selected source SHA, including
older explicitly requested tags. A branch edit never changes an existing selection.

Thin workflow installation is a separate authorized enrollment step with an explicit diff.
Do not hide a workflow rewrite inside a JSON save. If either GitHub write succeeds while
another fails, show each result and its revision; do not claim the whole enrollment works.
An enrolled repository with no semantic-version tag is visible but has no automatic work.
Missing/invalid definitions at the highest tag are errors; do not fall back to a lower tag.

## 4. Target compatibility and immutable environment selection

Row id is a local label, not a cross-repository foreign key. Match native prerequisites by
family/distribution/suite/arch/format plus build.platform. Match OpenWrt prerequisites by
family/distribution/suite/arch/format and build.target. The worker additionally requires
identical resolved SDK release/content identities when consuming artifacts. Different row labels can match; zero or multiple matching
rows is an explicit configuration error. An old prerequisite selected with a different SDK
is not silently substituted or rebuilt; request compatible prerequisite work explicitly.

Matching coordinates alone is insufficient proof of native ABI compatibility. The worker
checks the actual image OS/platform, package architecture, package dependencies and export
compatibility against the assignment before building. It uses standard tools rather than
inventing arch aliases. Incompatible images/sysroots fail clearly. OpenWrt exports carry
SDK hash, toolchain identity, release, target and package architecture; verify all of them.
Runner identity is never a substitute for either compatibility check.

Keep tag-form image references as allowed by the specification. Environment resolution runs
in the trusted GitHub worker. For native work, use standard Docker pull/inspect to select the
configured platform and freeze the digest-qualified image/configuration identity before
hooks execute. Do not implement registry authentication or OCI parsing in the controller.

The worker discovers latest SDK metadata from its deployment-owned official HTTPS origin.
It records the release, URL and official checksum and verifies downloaded bytes before
extraction. Actual SDK/toolchain/architecture checks and SDK signing remain unfinished.
The publisher obtains trusted tooling independently of source artifact executables.

The artifact manifest carries resolved environment evidence separately from the immutable
controller assignment. The publisher checks it against the assigned target, and the
verified feed receipt carries that identity back to the graph. Physical SDK/export
compatibility is checked by the worker when importing selected prerequisites, not by a
controller SDK resolver. The graph checks declared target coordinates and all prerequisites.

The trusted build workflow records a separate environment artifact before source hooks.
An explicit retry carries references to prior assigned runs and their assignment hashes;
the worker validates those GitHub identities using the same run validator as package
artifact retrieval and reuses the recorded environment. Evidence is requested for 90 days.
Expired/inaccessible evidence after a possible build fails clearly, never authorizing an
upgrade. An attempt proven not to have reached recording/hooks permits first resolution.
This owner-approved path adds no graph state or controller discovery client.

## 5. Graph selection and diamond traversal

Represent repository/target nodes with prerequisite and dependent adjacency lists. Logical
executions record immutable source selections and edges to prerequisite executions. Existing
published executions are reusable only for the exact selected inputs, never because they
are merely the latest publication. No second orchestration model or wave object is needed.

On a tag delivery, enrollment request or explicit request:

1. Validate the event and enrolled repository. Resolve the exact requested tag and peeled
   commit. Direct requests can select older versions; dependency work selects the highest
   numeric SemVer tag, including prereleases. Paginate tags and peel annotated tags.
2. Read selected definitions at those exact commits. Traverse affected dependents, selecting
   their highest SemVer sources. Use selected definitions to establish the affected closure;
   reject missing targets, unresolved prerequisites or cycles before accepting it.
3. Resolve prerequisite executions. Dependencies created within this affected dependent
   closure can be linked together, as in the diamond below. If a required prerequisite
   outside that closure has no matching execution, report a visible error until its work
   is explicitly requested. Do not create upward work automatically. Existing waiting or
   failed prerequisites remain explicit blocking edges. Never substitute a default branch
   or last-published source.
4. Gather GitHub source information outside the transaction. In one transaction validate
   the graph generation, create/reuse the affected logical executions, and bind prerequisite
   execution IDs. If the generation changed, discard the uncommitted selection and re-read;
   do not replay an uncertain committed mutation.
5. Traverse committed waiting executions. Only freeze an attempt's build assignment after
   all selected prerequisite publication receipts are available and compatible.

For R -> A/B -> J, one accepted R selection creates the affected A, B and J executions
atomically. J points to the new A and B execution IDs. A publishing alone does not satisfy
J while B builds or fails. On B's explicit retry, its logical execution ID stays the same;
J can use B only after that execution obtains a verified publication. Unaffected edges use
explicitly selected executions. Independent targets have independent dependency conditions.

Selection time and dispatch time are distinct. Source/definition/target/prerequisite logical
IDs are frozen at selection. Actual prerequisite package versions/checksums and artifact
IDs are frozen into an attempt assignment after those prerequisites publish. Before that,
there are no invented future artifact IDs. A successful execution has exactly one accepted
published attempt, and its receipt remains immutable.

A tag move creates a distinct selection with the new SHA. Deduplicate identical accepted
selections by semantic inputs and causal prerequisite IDs, not webhook delivery alone.
SemVer build metadata does not alter precedence, but exact tag and commit remain recorded.
If multiple highest-precedence tags resolve to different inputs, show ambiguity and require
an explicit source request; do not invent a lexical tie-break or choose the default branch.

## 6. Supersession, attempts and state

The small state machine belongs to the active attempt. The logical execution points to its
current attempt; do not persist a competing aggregate node state or eligibility flag.
Waiting is the initial attempt state, so graph state remains visible before dispatch.
Older attempts are immutable history. Edges point to logical executions, not failed attempts.

| Transition | Governing condition |
| --- | --- |
| waiting -> building | Not superseded; prerequisites verified; immutable assignment and build dispatch authority committed |
| building -> publishing | Assigned build and artifact verified; publication slot and publication dispatch authority committed |
| building/publishing -> failed | Authenticated, reconciled definitive failure with recorded reason; never inference from absence |
| publishing -> published | Feed commit, signed package/index identities and HTTPS deployment verified |
| failed -> new waiting attempt | Explicit retry; previous attempt remains history; no uncertain assignment may still be live |

A completed build awaiting the feed writer remains building with its verified build result;
this does not mean it is rebuilding. UI text distinguishes build complete/publication waiting
without a sixth scheduler state. A dependency failure leaves its consumer waiting with a
derived blocking reason rather than manufacturing a failed build that never ran.

Apply the owner's supersession rule only to waiting automatic selections. Record a reason
and superseded-by execution reference; retain graph/history. Supersession is terminal
selection metadata, not another workflow engine or ready list. Supersede affected waiting
descendants together, and create their replacement edges in the same selection transaction.
Never mutate their frozen source/edge sets. Explicit requests are retained. Already assigned
building/publishing work is reconciled and completed; a newer selection is not permission
to cancel it or reuse its identity. Do not dispatch a superseded waiting execution.

A retry preserves the logical selection and environment digest, and creates a new attempt
only after a definitive failed outcome. It may not silently upgrade source, tooling, image,
SDK or dependencies. Once a consumer attempt is assigned, its prerequisite artifact receipt
IDs are frozen. Retrying a failed prerequisite can unblock only still-unassigned consumers.
A request to rebuild already published inputs is separate work with a new revision, never
an overwrite or mutation of that published execution.

Late reports may update matching historical evidence but cannot make an older attempt
current, regress state or create work. Identical reports are idempotent. Conflicting success
claims remain visible for investigation rather than overriding an accepted receipt.

## 7. MariaDB, transaction ownership and recovery

Use InnoDB foreign keys, unique constraints and conditional transitions. Proposed storage:

| Record | Contents |
| --- | --- |
| repositories | Enrollment, stable GitHub identity, namespace, editing/workflow settings |
| definitions | Immutable GitHub source/blob revision, validated JSON/hash and references |
| executions | Source/definition/target selection, causal trigger, active attempt, supersession reference |
| prerequisites | Logical execution edges; no competing readiness value |
| attempts | State, immutable assignment, reserved rebuild revision, build/publication evidence and failure |
| deliveries | GitHub delivery ID plus accepted semantic event reference for deduplication |
| feed deployment | Configured identity and revision allocator; transaction lock boundary for publication |

Database snapshots do not accept independent matrix edits. Reconciliation data are evidence
attached to attempts, not a persisted task queue. The in-memory graph is the current view
of these records; update it only after confirmed commits, or reload after an unknown outcome.

Use one asynchronous MariaDB connection and one narrow, in-memory FIFO of database
operations. An operation owns the connection through its complete transaction callbacks.
All reads/writes go through this owner; no handler can insert queries into another handler's
transaction. This FIFO serializes database access only; it does not decide graph eligibility,
authorize GitHub requests or survive restart. It replaces unsafe interleaved submissions.
Use bound parameters supported by the installed asynchronous API, or its documented escaping
interface with explicit lengths; verify the available interface before choosing SQL assembly.

Do not pre-enqueue COMMIT after commands that may fail. On statement failure, schedule
ROLLBACK through the same connection owner and release ownership only at a known boundary.
Framework command sequences alone do not establish application transaction/error policy.
Losing a connection invalidates the transaction's session; never resume the body on another.

Record a stable command/assignment identity before mutations. If COMMIT acknowledgement
is lost, do not dispatch and do not replay the transaction. Through later lazy establishment,
read by that identity and establish whether authorization committed. A read that cannot
establish the outcome leaves it unknown. When a dispatch response arrives, persist its run
ID in a separate transaction. Losing that write also leads to reconciliation, not redispatch.

After startup connection success, restore the graph and inspect all assigned build and
publication executions using GitHub. Reconcile feed receipts before issuing new related
publication work. Unknown/inaccessible results preserve assignments and feed serialization.
Read-only reconciliation can be repeated explicitly; mutation retries remain owner actions.
Honor GitHub pagination/rate limits without treating throttling as failure or retrying POSTs.
No startup connection polling or generic automatic retry scheduler is introduced.

## 8. Dispatch and authenticated GitHub evidence

Configure a least-privilege controller GitHub App, with private key and webhook secret in
protected server files. Request installation tokens restricted to selected repositories and
operation permissions: metadata/read, contents/read for selection; contents/write for JSON;
actions/write for dispatch; actions/read for evidence; workflows/write only for an explicitly
authorized workflow change. Use OpenSSL for JWT signing and existing TLS verification.
Never give source builds the controller App key or publication credentials.

The dispatch payload contains execution ID, attempt ID, assignment JSON and its SHA256.
It includes exact selected source/definition/row and prerequisite receipts,
version/revision and trusted tooling identity. All values are data, never shell interpolation.
Use a run name containing attempt identity and assignment hash. Bound the compact payload
and reject oversize assignments explicitly; do not add an unplanned object store. Current
dispatch documentation permits 25 input properties and 65,535 input characters; use four
inputs and a configured lower byte bound.
[GitHub dispatch event limits](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#workflow_dispatch).

Current GitHub documentation describes a successful dispatch returning a run ID and URLs.
Persist and verify that ID. A lost response, or a server/API version returning no usable ID,
still requires reconciliation; no request is blindly repeated. The configured API version
and actual response contract must be recorded and proved in sandbox acceptance.
[GitHub dispatch API](https://docs.github.com/en/rest/actions/workflows#create-a-workflow-dispatch-event).

Do not bind by run name alone. Verify stable repository ID, workflow ID/path, dispatch event,
original actor and triggering actor against the controller App bot's stable identity, run
attempt, workflow commit and exact approved thin-workflow blob, and pinned referenced
reusable tooling SHA. Read the workflow file at the run's actual revision. Extra caller jobs
or modified upload steps invalidate the approved workflow. Reruns by another actor do not
become authorized work. Missing evidence means unknown/rejected binding, never guessed trust.
[GitHub run evidence fields](https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run).

Workflow refs and source refs are separate. Dispatch uses the configured workflow branch
or tag; the worker checks out the assignment source SHA explicitly. If a workflow ref moves
between preflight and execution, require its actual blob to match the approved version or
reject the run. Tool upgrades are reviewed full-SHA workflow updates, not silent reuse.

GitHub artifacts are associated with runs, not directly with jobs. Record run attempt, job
IDs, artifact ID, expected name, API digest, size and trusted upload-step evidence. Reject
multiple/conflicting artifacts. Do not claim the artifact API alone proves job ownership.
The approved thin/reusable workflow and isolated hook execution establish that boundary.
[GitHub artifact API](https://docs.github.com/en/rest/actions/artifacts#get-an-artifact).

Webhooks authenticate raw bounded body bytes with HMAC-SHA256 and constant-time comparison
before parsing. Validate installation, repository, event/action and delivery ID. Tag pushes
create selections; workflow/job notifications cause GitHub evidence reads against assigned
attempts. Source hooks cannot mark work published by sending arbitrary success/run IDs.
No separate workflow REST reporting or human identity subsystem is required initially.
Cross-repository private artifact reads require an explicitly authorized read credential;
do not assume the worker GITHUB_TOKEN or feed-only App can read private source artifacts.
The conditional least-privilege reader proposal is documented in setup-inputs.md and needs
owner approval if the first sandbox proof establishes it is necessary.
The trusted publisher reports by producing a feed receipt and assigned workflow result,
which the controller verifies through GitHub and HTTPS.

## 9. Workflows, hooks and development exports

Fresh files are delivered as reviewable source/worker/feed deployment templates. Source
repositories receive a thin workflow_dispatch workflow installed on an owner-approved ref
and present on the default branch as required by GitHub. It has contents/read permissions,
no inherited publication secrets, and calls reusable build tooling pinned to a full SHA.
Pin third-party Actions to full SHAs and record upgrade instructions.

Reusable build steps run on the supplied runner selector and pull the assigned image digest
or exact SDK. Native setup detects supported installed package-manager tools, not distro
names; format alone does not imply dnf rather than zypper/microdnf. Install build tools via
the selected distribution package manager. Fail unsupported setup with the missing capability.
Verify actual runtime platform and package architecture before running the native hook.

Hook environment contract:

| Name | Meaning |
| --- | --- |
| CORE_CI_SOURCE_DIR | Exact selected checkout, with no ambient GitHub credentials |
| CORE_CI_TARGET_FILE | Read-only selected row JSON |
| CORE_CI_SOURCE_VERSION | Exact selected SemVer text, also preserving original tag separately in assignment |
| CORE_CI_PACKAGE_REVISION | Reserved numeric rebuild revision |
| CORE_CI_PREREQUISITES_DIR | Selected verified packages, manifests and project-owned exports |
| CORE_CI_OUTPUT_DIR | Writable directory for declared packages and own development export |

The pinned trusted helper supplies format-specific package versions from source version
and revision. Hooks build, test and package according to project needs; no CMake/CPack
assumption. Provide working small DEB and RPM projects/hooks and actual demo OpenWrt
recipes supplied or approved by their source owner. A recipe is not fabricated for an
unknown project. The demo acceptance sources remain reviewable before deployment.

Install selected prerequisites using exact package versions from the signed feed and verify
the recorded published checksums. Development exports also have publication receipt hashes;
they are bounded archives of the project's own files. Native exports identify ABI/sysroot
compatibility. SDK exports identify exact SDK/toolchain/target compatibility. Never copy an
entire container/SDK filesystem or rebuild dependencies merely to obtain headers/libraries.

For OpenWrt, verify SDK SHA256 before extraction, then check actual release, target and
ARCH_PACKAGES using SDK metadata/tools. Import selected packages/exports into the SDK's
supported staging interfaces. Execute only the source-owned recipe/hook for that project.
Collect only declared output components, not the SDK's dependency package directory.
IPK and APK tooling is chosen by the configured format and verified SDK capability, never
by a compiled release mapping. Unsupported mixtures fail before source execution.

Run source hooks in an unprivileged isolated build container with bounded mounts, no Docker
socket, host workspace, Actions runtime tokens, signing material or publication access.
Do not source the hook into the trusted wrapper shell. Trusted checkout/upload/manifest
steps run outside that boundary. Persisted checkout credentials are disabled. This prevents
a hook from replacing the uploader or acquiring privileges merely because the manifest is
written later in the same job. Isolation limits must be tested, not inferred from YAML.
[GitHub workflow security guidance](https://docs.github.com/en/actions/reference/security/secure-use).

## 10. Package versions, revisions and manifests

Reserve a monotonically increasing positive rebuild revision R per package namespace and
feed deployment at logical selection acceptance. A failed build retry that can change bytes
reserves another R. Never recycle reserved numbers. Existing components share their
execution revision. Missing dependency package metadata uses prerequisite receipts, not
source-version guesses. Stable V=major.minor.patch encodings are:

| Format | Version/revision |
| --- | --- |
| DEB | V-R |
| RPM | Version=V, Release=R, Epoch absent/zero |
| IPK | V-rR |
| APK v3 | V-rR |

Prereleases are included by the owner's decision. One canonical trusted worker helper
encodes them for both build and independent publication validation; the controller does
not implement a second package-version codec. Preserve exact source tag, SemVer text,
build metadata and source SHA in the assignment/manifest. SemVer build metadata is omitted
from package Version because it does not affect SemVer precedence; distinct inputs still
receive distinct R values. A tag move also receives a new selection and revision.

Proposed general prerelease encoding, avoiding format-specific guesses about 'rc' names:
encode each SemVer prerelease identifier as numeric tokens. Numeric identifier: token 1,
its decimal digit count, then each digit as a token. Nonnumeric identifier: token 2, each
ASCII character code, then token 0. Finish the identifier list with token 0. The alphabet
is the SemVer identifier alphabet, not Unicode. This preserves numeric-before-nonnumeric,
numeric magnitude, ASCII lexical order and shorter-identifier/list ordering.

For DEB/RPM/IPK use V~pT.pT...; for APK use V_pre0_pT_pT...; append the normal R release
encoding. Example rc.1 tokens: 2,114,99,0,1,1,1,0. APK candidate:
1.2.3_pre0_p2_p114_p99_p0_p1_p1_p1_p0-rR. This is intentionally mechanical rather than
an incomplete prerelease-name catalogue. Standard package tooling still owns parsing.

This codec is a proposal requiring comparator proof, not an accepted implementation fact.
Twenty adjacent prerelease/stable comparisons passed locally with dpkg and RPM. IPK/opkg
and selected SDK apk-tools v3 have not been executed. Before committing the contract,
compare generated version sets with real clients, including numeric identifiers, hyphens,
prefixes, multiple components, long values and build metadata. Verify format length bounds.
If a required client cannot preserve order, stop for a revised proposal; do not silently
exclude prereleases or translate them to an unordered hash. APK upstream's restricted
version grammar is relevant; using its v3 source is not using an Alpine v2 feed.
[apk-tools version implementation](https://github.com/alpinelinux/apk-tools/blob/master/src/version.c).

Reserve revisions at selection rather than late dispatch so an older waiting selection
cannot acquire a higher revision just because it starts late. An explicit retry reserves
another revision when that retry is accepted. Per the owner's decision, that successful
retry may become the newest rebuild of the same source version, even when its pinned
inputs are older. Retain and display those exact inputs; this does not change SemVer
ordering against a higher upstream source version. Existing published identities remain
immutable and available for exact-version installation.

Artifact manifest schema 1 contains assignment hash, execution/attempt/repository/source/
definition/target identities, source version, package revision, selected prerequisite receipt
identities, package filenames/sizes/SHA256/declared components and optional development
export filename/hash/compatibility. The manifest is untrusted data used as a cross-check.
The expected namespace/source/target/dependencies/revision come from the authenticated
controller assignment. Recompute checksums, use standard package metadata tools and
verify every declared component and actual file, including no unexpected packages.

Bound compressed and expanded sizes, entry count, path length and tool execution resources.
Reject absolute paths, traversal, unsafe links/special files, duplicate names and overwritten
entries before extraction. Standard metadata tools run in constrained unprivileged validation
jobs; none of the package maintainer scripts or artifact programs is executed there.
Check actual namespace/name, full version/release, architecture, dependency metadata and
agreement with the assigned source/target. A manifest filename cannot grant namespace rights.
Record unsigned build hashes separately from final signed package hashes.

## 11. Trusted signing, publication and feed identity

Publication is a separate dispatch to trusted worker code at its pinned revision. It takes
the complete controller assignment and assigned build run/job/artifact IDs/digest, not a
source-supplied publication request. Verify the publication initiator and trusted workflow
before accessing package-publication. Download only that artifact by ID and verify build
identity again. A validation job uses no signing/feed-write secrets. The privileged job
gets only validated package data, verifies its digest again, and executes trusted tooling.
It never executes source hooks, package install scripts or arbitrary artifact scripts.

The controller authorizes at most one publishing assignment per feed repository AND branch,
not merely per subdirectory, because every Git push advances the same branch. Transactionally
lock the feed-deployment row, check that no active attempt is publishing for that branch,
and transition the selected attempt before committing. Other completed builds stay in
building until that predicate permits publication. Do not store a second current-publisher
pointer or ready queue; publishing attempts themselves are the authority. Hold publication
exclusivity through unknown outcomes and HTTPS verification.
The worker uses checked non-force updates as an independent correctness backstop.

Do not rely on GitHub concurrency as durable work retention. Its default pending behavior
can cancel replaced work. Current documentation also has an optional bounded queue; neither
provides the graph's required persistence/reconciliation guarantees. A non-canceling worker
concurrency group is only defense against accidental outside invocations.
[GitHub concurrency](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency).

Deterministic feed paths separate identities that can have different bytes:

| Format | Proposed path below configured HTTPS root |
| --- | --- |
| APT packages | apt/<distribution>/<suite>/pool/<namespace>/... |
| APT metadata | apt/<distribution>/<suite>/dists/<suite>/main/binary-<arch>/... |
| RPM | rpm/<distribution>/<suite>/<arch>/... |
| OpenWrt | openwrt/<suite>/<release>/<target-path>/<arch>/<format>/<sdk-sha256>/... |
| Public keys | keys/<trusted-key-file> |
| Receipts/exports | receipts/<attempt-id>.json and exports/<receipt-id>/... |

OpenWrt target path is validated, not interpolated unsafely. SDK hash in the path permits
same-series incompatible SDKs without overwriting feed identities. APT client sources use
apt/<distribution>/<suite> as the base and suite as the distribution. Pool filenames cannot
collide across suite builds. Namespace revision allocation plus checksum checks also prevents
same-version different-byte collisions within a feed. Retain old package and export versions.

Publication sequence:

1. Read current feed branch SHA; verify the assigned artifact and existing receipt/identities.
2. If the exact publication already exists, verify retained signed bytes and reuse the receipt.
   Do not sign or replace them again. Different bytes for an existing identity are rejected.
3. Add only assigned new packages/exports. Regenerate affected indexes from retained actual
   package metadata, preserving unrelated architectures, repositories and versions.
4. Import trusted keys temporarily, sign changed packages/indexes and verify using public
   trust material. Record final hashes, versions, dependency identities and indexes in receipt.
5. Commit packages, indexes, keys as needed and receipt together. Push by normal fast-forward
   from the observed revision. Reject stale/conflicting updates; no force push or silent rebase.
6. Report result through the assigned workflow and committed receipt. The controller reads
   actual resulting commit/receipt, matches assignment and confirms package/index signatures
   were checked by the trusted workflow. It uses async HTTPS to fetch bounded receipt/index
   bytes and confirm the deployed revision/checksums before marking published.

A job exit status alone is not publication proof. If the push succeeded but a later response
or hosting check failed, retain publishing/uncertain state and reconcile the same receipt.
Do not build/sign again merely to repair missing acknowledgement. External feed writers
must use the same serialization owner or be disabled for this deployment; unexpected branch
updates are visible conflicts, not an excuse to overwrite another writer.

Use standard format tooling:

- DEB: actual control metadata into Packages/Packages.gz; Release index checksums; OpenPGP
  InRelease and Release.gpg; public archive key and repository-scoped Signed-By instructions.
  APT authenticity is signed repository metadata, not embedded signatures in every DEB.
- RPM: rpmsign package signatures, createrepo_c metadata, OpenPGP repomd.xml signature;
  publish the key and verify both package and repository signatures with target dnf/RPM.
- IPK: actual package controls/checksums into Packages, Packages.gz, usign over uncompressed
  Packages -> Packages.sig; publish the corresponding trusted public usign key.
- APK: obtain the selected verified SDK's apk-tools v3; verify exact help/version; use its
  adbsign and mkndx facilities for package signatures and signed packages.adb, with the
  trusted EC P-256 key. No APKINDEX.tar.gz, Alpine v2 substitute or --allow-untrusted proof.
  OpenWrt's own package Makefile is the starting reference, not an excuse to copy its local
  key-generation or untrusted-build flags into publication/client acceptance.

If Pages is chosen, provide its deployment workflow and prove deployment of the receipt's
feed revision. Current limits include a 1 GB published site and soft 100 GB/month bandwidth.
Otherwise obtain the actual owner-approved package-compatible HTTPS destination. No extra
hosting service is selected. Never imply a GitHub artifact is an installable repository.
[Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits).

## 12. Main, managed MariaDB and resource lifetime

Main remains teaching code with named instances/options registered before init, blank lines
between tasks and visible route registration. Sequence:

1. Register controller/database/GitHub/worker/feed settings and HTTP instances so framework
   help includes their full configuration.
2. Call core::SNodeC::init(argc, argv) once.
3. In unmanaged mode, explicitly call MariaDBClient::connect() once. In managed mode,
   start only the configured private server, establish real readiness, then call connect once.
4. Initial connection success configures storage/graph/application and listener. Restore
   assignments and reconcile them before permitting new related dispatches.
5. Finish with return core::SNodeC::start().

Initial connection failure logs and releases database/startup resources at that boundary.
No listener, retained request, startup timer, retry loop or deferred startup callback survives.
Let the framework return naturally; no explicit stop/exit or application help branch.
Later database operations use SNode.C's existing establishment only if its connection object
is absent. That is not permission to retry the failed transaction or resume partial SQL.

Managed mode owns an isolated datadir, socket, pid and child process. Unmanaged mode does
not start, stop or reconfigure any administrator server. Provision the private datadir and
schema explicitly before normal startup; do not initialize/migrate them inside handlers.
Start the configured executable with argument arrays and its separate explicit config,
without inheriting the system database defaults. Refuse an already-owned endpoint/datadir.

Concrete readiness proposal: use mariadbd's native NOTIFY_SOCKET READY=1 event, authenticated
to the owned child, plus child-exit observation. Attach the descriptors to SNode.C event
receivers. On the actual readiness event, close that receiver and explicitly connect once;
this is readiness IO, not a next-tick startup deferral or polling. Spawn/termination ownership
belongs to one small managed-server resource, not a custom lifecycle framework. On child
exit before readiness, log and release startup resources. On initial DB failure terminate
only the owned child, close startup descriptors and clean up/reap outside blocking handlers.
Normal framework shutdown releases this resource; do not intercept it with explicit app exit.

Source evidence: MariaDB 11.8 sends READY=1 when it is ready for SQL requests; SNode.C has
public descriptor receiver/shutdown interfaces. Actual installed MariaDB readiness support,
child-exit integration, shutdown ordering and failure cleanup are NOT yet proven together.
They are a checkpoint-1 gate. If this approach cannot satisfy the main sequence without
polling, stop and resolve the conflict before writing another lifecycle mechanism.
[MariaDB readiness implementation](https://github.com/MariaDB/server/blob/11.8/sql/mysqld.cc).

Verified framework references: src/apps/database/testmariadb.cpp; jsonserver.cpp/jsonclient.cpp;
HTTP low-level client; MariaDBClient/Connection/CommandSequence; core/SNodeC and descriptor
receivers. MQTTSuite mqttstore options, async storage and HTTP request lifetime demonstrate
concrete ownership. Do not copy demo credentials or their unrelated connection-retry policy.
Use installed find_package(snodec CONFIG REQUIRED COMPONENTS ...) and snodec:: targets.
No dependency on sibling checkout paths. Copy canonical formatting/analysis files/modules
byte-for-byte, integrate explicit format/check targets over src/ and tests/, and exclude
third-party/generated files and .qtcreator/. No compile-time source rewriting.

## 13. REST and local UI

Register routes explicitly in main in reading order. Proposed resources:

| Route | Purpose |
| --- | --- |
| GET / | Local application UI |
| GET/POST /api/repositories | List/enroll; report definition/workflow outcomes separately |
| GET/PUT /api/repositories/:id/definition | GitHub revision-addressed read/conditional commit |
| GET /api/graph | Repository/target relationships, selected executions and derived state |
| GET /api/executions/:id | Exact source/inputs, attempts, failures, receipts and GitHub log URLs |
| POST /api/repositories/:id/builds | Explicit exact-version request, including older versions |
| POST /api/executions/:id/retry | Explicit retry after definitive failure |
| POST /api/reconcile | Re-read already assigned remote work; never authorize replacements |
| POST /github/webhook | Raw-body authenticated GitHub events |

Graph and history show waiting prerequisites, supersession, exact selected version/commit,
definition revision, package revision, target, actual environment digest, failure/uncertainty,
run/job links and publication receipt. A successful GitHub JSON save is separate from
configuration validation, source-tag selection, workflow readiness and package publication.

Development access binds loopback. Restrict mutation requests by allowed Host/Origin and
same-origin request requirements to prevent a hostile site from writing through localhost;
this is request protection, not a user system. Serve escaped data with a restrictive content
policy. Agree deployment access protection and webhook TLS before exposing a writing UI.
No OAuth/IdP/password/role-management machinery is introduced.

## 14. Deployment setup, secrets and operations

The exact proposed configuration names, protected files, public variables and secret
provisioning commands are in [setup inputs](setup-inputs.md). All are required setup inputs,
not fabricated deployment values. No existing secret is assumed or implicitly discovered.

Use a separate administrative schema-setup operation. Runtime account gets only the selected
schema's required SELECT/INSERT/UPDATE/DELETE rights, no global/admin rights or application
DDL. Connection credentials are protected server-side files; managed launch settings are
separate from the default application config. Document MariaDB version/schema checks,
consistent backup with mariadb-dump --single-transaction, off-host protection, restoration
into a stopped controller's isolated database and post-restore GitHub/feed reconciliation.
A restored backup never authorizes a replacement run merely because later evidence is absent.
Before enabling dispatch after a backup restore, inventory source/worker runs and feed receipts
since the backup, including work not present in that backup. If their assignments cannot be
reconciled, keep dispatch blocked for operator recovery rather than reconstructing guessed
inputs or inventing a legacy import path.

Signing keys stay only in the protected trusted worker publication environment and temporary
restricted job-local files/keyrings. Verify imported private keys correspond to reviewed
public keys/fingerprints; remove material on success/failure. Public keys live in reviewed
worker keys/ and the feed. No source hook, artifact or downloaded SDK establishes key trust.
No private signing material in core-ci database, JSON, Git, cache, artifact or log. No implicit
key generation/rotation. Avoid passphrases/secret values in arguments; use file/FD input.

## 15. Implementation checkpoints, estimate and acceptance

The estimate is now 7,000-10,500 new production lines and 2,500-4,000 test lines, excluding
documentation and byte-copied canonical formatting modules. These are design estimates,
not measured finished code. Controller includes GitHub App HTTPS, graph, database ownership,
managed resource and reconciliation; worker includes version encoding, four publication
formats and archive isolation. There is no justification for hiding these costs in scripts.

| Checkpoint | Incremental production estimate | Required result |
| --- | --- | --- |
| 1. Foundation and enrollment | 1,800-2,600 | Real MariaDB in both modes, explicit connect/failure cleanup, local UI enrollment and confirmed GitHub JSON commit |
| 2. First complete path | 2,400-3,500 | Tag -> immutable graph/dispatch -> one real DEB build -> trusted signing/HTTPS feed -> apt refresh/install -> dependent execution -> UI |
| 3. Remaining formats and graph cases | 2,000-3,000 | RPM, OpenWrt IPK/APK, architectures, chain/diamond and owner supersession/prerelease behavior |
| 4. Recovery and hardening | 800-1,400 | Lost responses, duplicate/late reports, restart, retry, adversarial artifacts/signatures and operational handover |

Checkpoint 2 includes a dependent repository so the first real path proves graph traversal,
not isolated jobs. Expand formats after that path works. Prepare files and local checks
before requesting external activation. Missing owner inputs can block the real path and
must be reported; synthetic responses cannot substitute for it.

Deleting old code satisfies reset but cannot implement the absent controller and workers.
Modification/reuse of the old implementation is explicitly prohibited. New code is therefore
necessary; choose concrete components and delete replaced mechanisms within each checkpoint.
Production growth and this refined proposal still need the explicit approval required by
global instructions. The previous estimate is superseded, not silently expanded. Stop and
re-estimate before a checkpoint exceeds its upper bound or introduces an unagreed extension.
No discretionary '+20%' growth allowance is assumed.

Acceptance records exact source commits/toolchain/configuration and separates local from live:

| Boundary | Required evidence |
| --- | --- |
| Configuration/UI | Real enrollment/commit/error/conflict feedback; committed vs effective revisions; invalid cycles/missing dependencies/ambiguous targets rejected |
| Source selection | Numeric ordering, prereleases, exact older requests, tag moves, highest invalid definition, immutable historical inputs |
| Database/lifecycle | Real managed and unmanaged MariaDB, initial failure with no listener, shutdown ownership, lazy later establishment, uncertain COMMIT handling |
| Graph/state | Chain and diamond; J blocked while B builds/fails; B retry unblocks the same J selection; independent targets; supersession retains history |
| GitHub | Actual authorized sandbox runs, approved workflow/ref/tooling/run/job/artifact binding, spoof/rerun rejection and exact source checkout |
| Recovery | Duplicate/delayed events, lost dispatch response, restart during assigned work, inaccessible evidence remains unknown, no duplicate dispatch |
| Package workers | DEB/RPM/IPK/APK, exact dependencies/own exports, matching SDKs, multiple architectures where runners/emulation execute |
| Publication | Actual retained packages and signed indexes over HTTPS, checked branch update, conflicting writer rejection, identical replay preserves bytes |
| Client trust | Actual apt/dnf/opkg/apk refresh AND install with published public keys; wrong key and modified index/package rejection; no --allow-untrusted |
| Runtime | Execute installed demos where architecture/environment permits; explicitly record unavailable runner/emulation/kernel limitations |
| Isolation/keys | Malicious manifest/path/archive/component cases; hook cannot access uploader/signing credentials; private/public key mismatch rejected and temporary keys cleaned |

Report actual verification, branch/commit/PR, production/test/docs changes and removed
mechanisms separately. Final completion requires the real graph-driven pipeline, not passing
synthetic tests or separate worker demonstrations. No implementation/live acceptance is
claimed by this design checkpoint.

## 16. Remaining owner choices and verification gates

The six received policy choices are fixed in section 1. These remaining issues are
explicit rather than silently implemented:

| Question | Proposed handling pending owner decision |
| --- | --- |
| Permanently unmatched dispatch | Preserve unknown state; do not add 'declare lost' replacement permission without a separately approved, evidence-based protocol |
| Missed tag deliveries during downtime | Offer GitHub redelivery/explicit selected-version request now; an explicit tag-rescan action is a proposed scope choice, not hidden startup polling |
| Feed retention/hosting capacity | Preserve all versions initially; no automatic deletion. Owner chooses retention before a capacity limit requires removal |
| Public access/hosting | Owner provides webhook TLS, UI access protection, feed branch and package-compatible HTTPS destination or approves Pages |

Required implementation proofs: managed readiness/child cleanup; exact installed async SQL
parameter/transaction API; prerelease codec ordering on actual opkg/APK clients; selected
SDK adbsign/mkndx CLI; GitHub App actor/run binding and dispatch API response in sandbox;
private-repository artifact access with least privilege; approved SDK/tooling provenance
before privileged execution and safe credential handling across download redirects. An API
limitation must be surfaced before replacing the design with a new authentication or scheduling mechanism.

## 17. Claude review disposition and original-scope accounting

| Review point | Integrated resolution |
| --- | --- |
| D1 conditional diamond | Mandatory atomic affected selection and edges to exact prerequisite executions, section 5; no optional wave engine |
| D2 retry orphans edges | Stable logical execution with historical attempts and one active attempt, section 6 |
| D3 feed collisions | Suite-separated APT pools and SDK/release/format-separated OpenWrt feeds, section 11 |
| D4 writer retention | One controller-authorized publisher per feed branch, section 11; GitHub pending queues are not state authority |
| D5 image timing | Resolve configured images on GitHub using standard tooling, section 4; retain tag-form source inputs |
| D6 forged runs | App actor, approved actual workflow blob, reusable SHA, run-attempt/job/upload/artifact checks, section 8 |
| D7 manifest authority | Authenticated assignment supplies expectations; manifest only cross-check; isolated hooks, approved publisher SDK tooling and independent package tooling, sections 4/9-11 |
| Async transaction/COMMIT gaps | Single database operation owner and explicit uncertain-outcome readback, section 7 |
| Prerequisites, architecture and package tools | Exact signed-feed packages, own verified exports, capability detection and SDK checks, sections 4/9 |
| Versions and mutable feed order | Shared codec, selection/retry-time revision allocation, supersession and owner-approved retry promotion, sections 6/10/16 |
| Managed readiness | Concrete native readiness/event-receiver proposal with unproven integration gate, section 12 |
| Estimate/early path | Increased explicit estimates and real signed DEB+dependent+apt path in checkpoint 2, section 15 |

Not adopted blindly: an extra wave entity; a separate durable queue; a digest-only source
schema; OIDC/report endpoints without a demonstrated need; an operator 'lost' override;
automatic retention; blanket prerelease/build-metadata rejection; or a 20% unapproved growth
allowance. Some review suggestions were alternatives, not owner requirements. API claims
about pending queues and dispatch responses were updated against current primary docs.

| Original specification | Proposal coverage |
| --- | --- |
| A reset/workspace | Completed reset checkpoint; canonical checkout preserved; no additional checkout |
| B framework/lifecycle | Sections 7/12; explicit connect and verified reference patterns |
| C deployment roles | Section 1 |
| D authoritative JSON | Sections 3/4 |
| E graph/source | Sections 4-6/10/16 |
| F state/MariaDB | Sections 6/7/10/12/14 |
| G fresh workflows | Sections 8/9/11 |
| H native Linux | Sections 4/9/10/15 |
| I OpenWrt | Sections 4/9-11/15 |
| J artifacts | Sections 8-11 |
| K keys/secrets | Sections 11/14 and setup-inputs.md |
| L format signing | Sections 10/11/15 |
| M feed/publication | Section 11 |
| N App/REST events | Sections 7/8/13 |
| O UI/access | Sections 3/13 |
| P teaching style | Section 12 and repository AGENTS.md |
| Q implementation/acceptance | Section 15 |
| R owner inputs | Section 16 and setup-inputs.md |
| S completion reporting | Section 15; no operational completion claimed |
