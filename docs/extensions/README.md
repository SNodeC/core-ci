# Extending core-ci and enrolling projects

[Documentation guide](../README.md) · [Architecture](../architecture/README.md) ·
[Deployment](../deployment/README.md)

Most extensions are source-repository changes. A new project supplies a definition and hooks;
a new target supplies an explicit matrix row. Change the controller only when the existing
contract genuinely cannot express the required behavior. This keeps project build systems,
distribution choices and SDK details out of graph/state code.

## Contents

- [1. Choose the right extension boundary](#1-choose-the-right-extension-boundary)
- [2. The source-owned definition](#2-the-source-owned-definition)
- [3. Enroll and verify a new repository](#3-enroll-and-verify-a-new-repository)
- [4. Write a native hook](#4-write-a-native-hook)
- [5. Write an OpenWrt hook](#5-write-an-openwrt-hook)
- [6. Versions and rebuild identity](#6-versions-and-rebuild-identity)
- [7. Understand the artifact contract before extending it](#7-understand-the-artifact-contract-before-extending-it)
- [8. Add worker support or change controller behavior](#8-add-worker-support-or-change-controller-behavior)
- [9. Validate the changed boundary](#9-validate-the-changed-boundary)

## 1. Choose the right extension boundary

| Requirement | Normal place to change |
| --- | --- |
| Add a project or dependency | Source `ci/packages.json`, authorization grant, then enrollment |
| Add an architecture or distribution supported by the existing worker contract | An explicit source matrix row and appropriate runner |
| Use Make, Meson, a custom compiler or another project build system | Source-owned hook |
| Export headers/libraries for dependents | Source hook's own development archive |
| Change tool installation, container preparation or SDK handling | Trusted worker tooling and its focused checks |
| Add another package format/family | Definition validator, compatibility contract, worker, publisher and client acceptance |
| Change eligibility, retry or source-selection semantics | Graph/state authority, persistence implications and public-boundary checks |
| Improve presentation | Graph-derived UI/receipt projection; preserve immutable identifiers |

Do not add a controller runner mapping or distribution catalogue to support another JSON row.
The worker validates what it can actually execute. An unsupported image, architecture,
package manager, SDK or format must fail clearly rather than be guessed.

## 2. The source-owned definition

The authoritative file is `ci/packages.json`. This complete example uses two native rows and
one OpenWrt row. It is an example configuration, not an automatic support catalogue.

```json
{
  "schema": 1,
  "title": "Example Library",
  "package": "example-library",
  "depends_on": [],
  "build": {
    "native": "ci/native.sh",
    "openwrt": "ci/openwrt.sh"
  },
  "matrix": [
    {
      "id": "debian-trixie-amd64",
      "family": "native",
      "distribution": "debian",
      "suite": "trixie",
      "arch": "amd64",
      "runner": "ubuntu-24.04",
      "build": {
        "image": "debian:trixie",
        "platform": "linux/amd64",
        "format": "deb"
      }
    },
    {
      "id": "fedora-43-x86_64",
      "family": "native",
      "distribution": "fedora",
      "suite": "43",
      "arch": "x86_64",
      "runner": "ubuntu-24.04",
      "build": {
        "image": "fedora:43",
        "platform": "linux/amd64",
        "format": "rpm"
      }
    },
    {
      "id": "openwrt-25.12-x86_64",
      "family": "openwrt",
      "distribution": "openwrt",
      "suite": "25.12",
      "arch": "x86_64",
      "runner": "ubuntu-24.04",
      "build": {
        "target": "x86/64",
        "format": "apk"
      }
    }
  ]
}
```

### What each field means

`schema` selects the supported definition schema. `title` is a display title. `package` is the
repository's exclusive namespace: output names must equal it or begin with it followed by `-`.
For example, `example-library-dev` belongs to `example-library`; `another-project` does not.
Overlapping namespace claims are rejected across enrolled definitions.

`depends_on` lists enrolled repository names, not arbitrary URLs. For a library depending on
`demo-core`, use `"depends_on": ["demo-core"]`. A dependency must provide exactly one
compatible row for each dependent row. Missing prerequisite work is an explicit error; request
that repository first. Do not rely on enrollment automatically creating all upward work.

`build` maps each used family to its repository-relative hook. Hooks must remain within the
selected checkout, including after resolving paths. Absolute paths and traversal are rejected.
The definition and hooks are read at the selected version commit, not from the current editing
branch. Commit the hooks and definition before tagging the source you intend to build.

A row's `id` is unique within its repository. `family`, `distribution`, `suite` and `arch`
are explicit coordinates. `runner` is a string or nonempty label array, preserved as supplied;
runner-group objects are unsupported. `arch` is not repeated inside `build`.

Native `build` has exactly `image`, `platform` and `format`, where format is `deb` or `rpm`.
OpenWrt `build` has exactly `target` and `format`, where format is `ipk` or `apk`. OpenWrt
`suite` selects the feed series. The trusted worker resolves the SDK patch release and records
its URL/checksum/toolchain identity; the source definition contains no SDK URL, checksum or
another duplicate series field.

Unknown fields are rejected. Definitions support at most 128 dependencies and 256 nonempty
matrix rows; selected graph validation is bounded to 512 repositories. Identical compatible
coordinates cannot be repeated merely under different row IDs.

Implementation: [Definition.cpp](../../src/Definition.cpp).

## 3. Enroll and verify a new repository

1. Have the deployment administrator grant the controller App access to the repository and
   add its actual numeric ID to `github.repository`. UI enrollment cannot broaden that grant.
2. Commit the project definition and hooks. Read them through the UI before replacing an
   existing file. Saving must report the actual committed GitHub revision.
3. Install the thin source workflow using the UI preview/commit action or the reviewed
   renderer described in the deployment guide. Check the full trusted worker pin.
4. Enroll the repository. Its highest numeric SemVer, including prereleases, is selected if
   present. If prerequisites have no selected work, request them explicitly first.
5. Follow one target from waiting through published. Check the assigned source SHA and
   dependency receipts, not only the GitHub run's green check mark.
6. Add the remaining rows once their runners and worker prerequisites are available.

A new definition can introduce a new prerequisite. That prerequisite's selected definition
must be readable and compatible even if the enrollment snapshot did not previously mention it.
An unreadable unknown repository blocks selection; it is not silently excluded.

## 4. Write a native hook

A hook implements the project's build, tests and packaging. It may use any build system; the
worker does not assume CMake or CPack. The working directory is the isolated selected source.
The shell hook receives these environment variables:

| Variable | Meaning |
| --- | --- |
| `SOURCE_DIR` | Writable isolated selected source copy, without Git credentials |
| `TARGET_JSON` | Read-only selected row JSON file |
| `SOURCE_VERSION` | Exact source tag, including optional `v`, prerelease and build metadata |
| `PACKAGE_NAMESPACE` | Authorized package namespace |
| `PACKAGE_REVISION` | Reserved positive rebuild revision |
| `PACKAGE_VERSION` | Complete encoded package version |
| `PACKAGE_UPSTREAM_VERSION` | Upstream part suitable for the package format |
| `PACKAGE_RELEASE` | Separate release/revision part |
| `PREREQUISITES_DIR` | Read-only exact packages, receipts and development exports keyed by execution |
| `OUTPUT_DIR` | Writable declared output location |

Prerequisite receipt copies omit repository `indexes`; full signed receipts remain in the feed.
Package/export identities and SDK compatibility facts stay exact.

Read the small [working reference hook](../../deployment/reference/native/ci/native.sh)
before adapting your own. It builds a C project, runs its test and produces DEB or RPM packages
using standard tools. It is deliberately usable without a CMake-based project.

The worker prepares the configured distribution image, installs baseline tools using apt-get
or dnf, checks the distribution/architecture and imports exact prerequisite packages before
invoking the hook. A suitable runner or functioning emulation must exist. Choosing an ARM
image on an x86 runner does not itself provide emulation.

The hook runs without publication secrets, checkout tokens or a Docker socket. Its container
has bounded resources and restricted writable mounts. A separate fresh validator container
checks the resulting files; a background process from the hook cannot define trusted output
metadata after validation starts. This isolation belongs to GitHub workers, not the controller.

### Declare outputs

Write `outputs.json` in `OUTPUT_DIR`:

```json
{
  "packages": [
    {"file": "example-library.deb", "component": "example-library"},
    {"file": "example-library-dev.deb", "component": "example-library-dev"}
  ],
  "development_export": "development.tar.gz"
}
```

Use real filenames produced by the hook. The worker independently reads actual package
metadata; a manifest name does not authorize another namespace. Omit `development_export`
if the project has nothing to export. Output directories contain only declared packages,
the optional export and `outputs.json`; the wrapper produces the trusted manifest afterward.

An export contains only the project's own `include/`, `lib/` and `share/` files, not an entire
container filesystem or copies of prerequisites. Archive paths must be relative, bounded and
free of links/special files. Use real file copies where the contract rejects symlinks.
Dependents set their own build system's include/library search paths from the exported files.

The graph consumes exact selected package/export identities at build time. Runtime package
dependency ranges remain source-owned compatible ranges. A graph edge can be build-only;
core-ci does not invent a runtime dependency or replace your recipe's range with an exact pin.

## 5. Write an OpenWrt hook

Use `family: openwrt`, an explicit suite/architecture/target and the matching package format.
The worker resolves and verifies the official SDK archive before extracting it. A dependent
uses the SDK/toolchain compatibility identity of its selected prerequisites. At a join, all
prerequisite SDK identities must agree. The runner name is not a substitute for that check.

The common hook variables remain available. OpenWrt additionally supplies `SDK_DIR` and
`SDK_STAGING_DIR`. The source hook prepares the project recipe inside the selected SDK and
uses its toolchain. See the [reference OpenWrt hook](../../deployment/reference/native/ci/openwrt.sh)
and [sandbox recipes](../../deployment/sandbox/ci) for concrete packaging examples.

Prerequisite packages and SDK development exports are imported from the assigned receipts.
Do not rebuild another project merely to recover its headers or libraries: doing so would
replace the graph's selected inputs with another unrecorded build. Export only the current
project's own development files and declare only its intended package outputs.

IPK targets use the matching opkg-compatible package/index tooling. APK targets use the
selected OpenWrt SDK's APK v3 tooling, including its host-library environment. Alpine APK v2
index generation is not an interchangeable implementation.

SDK discovery trusts the configured HTTPS metadata origin and verifies the advertised archive
SHA-256 before extraction. Verification of a separately signed SDK checksum list is not
implemented. Record that trust boundary when deciding whether a deployment is suitable.

## 6. Versions and rebuild identity

The controller compares SemVer source tags; the worker's single `contract.versions` function
encodes them for package tools. Do not introduce another comparator or codec in a hook.
Use the supplied `PACKAGE_*` variables.

For source `v1.2.3`, rebuild 7:

| Format | Package version |
| --- | --- |
| DEB | `1.2.3-7` |
| RPM | Version `1.2.3`, Release `7` |
| IPK | `1.2.3-r7` |
| OpenWrt APK v3 | `1.2.3-r7` |

Build metadata stays in source provenance but is omitted from the package Version. New rebuild
revisions distinguish executions that could produce different bytes. A successful explicit
retry may become the newest rebuild of the same source version; existing package bytes are
never replaced under their published identity.

Prereleases use an order-preserving token encoding. For each prerelease identifier, a numeric
identifier contributes `[1, digit-count, individual-digits...]`; a nonnumeric identifier
contributes `[2, character-code..., 0]`. A final `0` ends the sequence. DEB/RPM/IPK append
`~` and dot-separated `p<number>` tokens to the upstream version. APK appends `_pre0` and
successive `_p<number>` tokens. The generated complete version is bounded to 240 characters.
The code and package-manager comparator checks define the exact supported ordering.

Implementation: [contract.py](../../deployment/worker/core_ci/contract.py),
[Version.cpp](../../src/Version.cpp), [worker tests](../../tests/worker.py).

## 7. Understand the artifact contract before extending it

Assignments are grants of work authority. Manifests describe produced bytes. The manifest
carries execution/repository/source/target evidence, canonical assignment hash, version and
revision, exact selected dependencies, package filenames/components/metadata/sizes/checksums,
and optional development-export compatibility.

The publisher independently checks package name/namespace, version, architecture and declared
runtime dependencies using standard package tooling. Archive handling rejects traversal,
absolute paths, links, special files and excessive resource use. Current bounds are 512 MiB
per file/compressed archive, 1 GiB expanded and 20,000 archive entries.

Readable labels are generated once from immutable graph inputs and carried in new assignments.
They are used by workflow run/job names and verified by controller/worker consumers. They do
not replace canonical hashes, actor checks, approved workflow blobs or exact run/job/artifact
IDs. Retained assignments keep their original expected names; no input is rewritten merely
to improve its presentation.

A contract change must be traced through `Graph::buildAssignment`, `Pipeline`, the thin and
reusable workflows, `contract.py`, `github.py`, environment retry retrieval, validation,
publication and receipt checking. Update every consumer coherently and account for already
assigned work before changing deployment pins.

## 8. Add worker support or change controller behavior

For a new supported distribution using an existing format, first test whether a JSON row and
source hook suffice. The image must actually contain the expected package manager and tool
availability; architecture/distribution checks must pass without a controller alias table.

For a new family or package format, define its compatibility coordinates, hook interface,
version representation, real metadata inspection, signing/indexing, feed layout and actual
client installation proof. Update the schema validator and worker contract together. Adding a
new UI option alone does not create working support.

For a graph/state change, state the invariant first. Trace eligibility, source selection,
identity construction, persistence, restart/report handling and UI blockers. Keep one graph
authority. A waiting-to-failed transition without dispatch, a new persisted queue or a retry
policy is not a harmless local patch.

For SNode.C-facing changes, study its applications and MQTTSuite ownership/lifecycle patterns.
Use installed public package targets, asynchronous callbacks and the existing event loop.
Preserve the project's braces, single-return style, line bound and committed formatting tools.
Net production growth or an architectural extension requires the approval in `AGENTS.md`.

## 9. Validate the changed boundary

| Change | Relevant checks |
| --- | --- |
| SemVer/graph/eligibility | Version, definition and graph tests |
| Enrollment, reports, SQL, dispatch or restart | Pipeline and applicable database/lifecycle boundaries |
| Environment/retry evidence | Worker environment and provenance checks |
| Package metadata/signing/indexing | Worker package tests and relevant format client |
| SDK extraction/exports/APK v3 | OpenWrt tests and affected live OpenWrt target |
| UI presentation | JavaScript syntax and the actual browser view |

Run only affected suites, broadening when failures or dependency impact justify it. Local
synthetic GitHub responses are not live integration evidence. A full package acceptance needs
real GitHub jobs, signed HTTPS publication, actual package-manager refresh/install and runtime
checks where executable. Wrong-key rejection and modified-byte rejection are separate proofs.

Deploy revised tooling only after reviewing it, updating the matching source/publication blob
pins and deciding what happens to assigned work. Do not rewrite a pinned execution to make it
fit newly deployed workflow bytes. See the [upgrade procedure](../deployment/README.md).
