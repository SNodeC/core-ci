# Fresh worker and feed deployment files

These files are prepared locally. They have not been installed in GitHub. The native
package build/sign/index boundary has local real-tool coverage; there is no live GitHub,
container-isolation or package-manager installation acceptance yet. OpenWrt tooling and
full live publication acceptance remain unfinished. Controller dispatch/receipt proof is wired with local synthetic GitHub evidence. Do not activate this
checkpoint as a complete core-ci deployment.

The invariant is one immutable controller assignment across build, independent validation,
signing and feed publication. The manifest describes bytes; it never grants namespace rights.
No old orchestration code or workflow was reused.

## Layout and installation order

1. Review `worker/`, then commit its contents at the root of the trusted worker repository.
   `core_ci/` is deployment-owned Python code and `keys/` contains only reviewed public keys.
   Protect edits to workflows and tooling. Record the resulting full tooling commit SHA.
2. Set the worker's public `CORE_CI_TOOLING_SHA` variable to that reviewed SHA. The reusable
   build workflow checks out this assignment-pinned tooling separately from the source.
   The publication workflow checks out the protected deployment variable, then verifies it
   agrees with the immutable assignment. No workflow contains its own impossible self-SHA.
3. Render source workflow bytes locally with the reviewed worker identity:

   ```sh
   python3 deployment/render-source-workflow.py --worker OWNER/WORKER --tooling-sha REVIEWED_FULL_SHA --output /tmp/codex/build/core-ci/TASK/core-ci-build.yml
   ```

   The arguments are required deployment inputs; the renderer refuses non-full SHAs.
   It prints the Git blob identity for controller `pipeline.source-workflow-blob` approval.
   Review and commit the rendered `.github/workflows/core-ci-build.yml` separately in each
   source repository. JSON save does not silently authorize a workflow write. The UI workflow
   preview/commit action uses the same approved workflow blob and reports the committed revision.
4. Record the publication workflow Git blob SHA, dispatch ref and tooling SHA in the protected
   controller config. Keep workflow and source-to-build commits separate. A workflow upgrade
   needs review, updated approved blobs and a new tooling pin; old assignments remain unchanged.
   Old assigned work with unavailable approved workflow bytes is an error, never rewritten.
5. Review `feed/.github/workflows/pages.yml` for the chosen feed branch. The prepared sandbox
   branch is main in VOC-Experiments/packages. Proposed HTTPS root is
   https://voc-experiments.github.io/packages/. Pages is currently disabled. Installing a
   workflow and enabling Pages are external activation steps, not performed by these files.
6. Review credential scopes, protect environments and complete live acceptance before use.

The controller commits dispatch authorization before POST. An uncertain response remains
assigned. The worker rejects GitHub reruns; an explicit controller retry reserves a new
package revision. Source workflows have contents/read, no inherited secrets, and a reusable
workflow pinned by full commit. Third-party Actions are pinned by full reviewed commits.

## Native hook contract

`reference/native/ci/native.sh` supports a small real DEB and RPM project using the same C
source. It compiles with warnings as errors, runs the executable test and packages it with
`dpkg-deb` or `rpmbuild`. There is no CMake/CPack assumption. Copy/adapt hooks only after
reviewing the actual project's requirements; these are reference code, not invented demo
repository recipes.

The hook runs with `/bin/sh`, its source directory as cwd, and these environment variables:

| Variable | Meaning |
| --- | --- |
| SOURCE_DIR | Writable isolated copy of the exact selected checkout, excluding .git |
| TARGET_JSON | Read-only selected row JSON |
| SOURCE_VERSION | Exact SemVer tag, including optional v and build metadata |
| PACKAGE_REVISION | Positive reserved rebuild revision |
| PACKAGE_VERSION | Encoded complete package version |
| PACKAGE_UPSTREAM_VERSION / PACKAGE_RELEASE | Separate RPM/spec-friendly version fields |
| PACKAGE_NAMESPACE | Assigned repository-owned package namespace |
| PREREQUISITES_DIR | Read-only directories keyed by prerequisite execution, with receipt.json, exact packages and optional development/ |
| OUTPUT_DIR | Writable output directory only |

The native worker pulls the assigned image digest/platform and verifies Docker's image config
identity against worker-recorded immutable image evidence. It installs baseline tools with apt-get
or dnf according to package format, checks actual `/etc/os-release` and the package manager's
architecture without aliases, imports the exact prerequisite packages and verifies installed
versions. A matching CPU runner or working emulation is required; none is silently selected.

Hooks run as UID/GID 1000 in a disposable container, with all capabilities dropped, no new
privileges, a read-only root and input mounts, bounded memory/processes/CPU, and writable
scratch/output only. They receive no Docker socket, Actions environment tokens, checkout
credentials or signing files. The build image is prepared before hooks run. A second fresh
container without network validates outputs after the hook container exits, preventing a
background hook process from racing the manifest step. This isolation is prepared code;
Docker is unavailable locally and the boundary still needs execution/adversarial proof.

The hook writes `outputs.json`, for example:

```json
{"packages":[{"file":"project.deb","component":"project"}],"development_export":"development.tar.gz"}
```

Development export is optional. It contains only the project's own include/, lib/ and share/
files. Absolute paths, traversal, duplicate entries, links and special files are rejected;
export real files instead of symlinks. The worker records target compatibility. Headers and
libraries from prerequisite exports are available without rebuilding those projects. Hooks
must set their own build system's search paths from PREREQUISITES_DIR.

Only declared package files, optional development.tar.gz and outputs.json may be produced.
The wrapper replaces outputs.json with manifest.json after independent checks. Package names
must equal the assigned namespace or have its dash-prefixed component namespace. Components,
version and architecture must agree with actual metadata. Runtime dependency ranges are read
from the packages unchanged. Graph edges may be build-only; they do not invent runtime ranges.

## Versions and artifacts

Stable 1.2.3 at rebuild 7 becomes DEB/RPM 1.2.3-7 and planned IPK/APK 1.2.3-r7.
SemVer build metadata is retained in provenance and omitted from package Version. The single
worker `contract.versions` codec is used for both building and validation. General prerelease
identifiers use the numeric token scheme in docs/design.md. Adjacent ordering for numeric,
alphabetic, hyphenated, prefix and multipart identifiers is proved with installed dpkg and
RPM comparators. IPK and selected SDK APK v3 comparator proofs remain outstanding. No claim
of OpenWrt version support is made from the native comparisons.

The schema-1 manifest contains the complete assignment evidence/hash, controller-assigned
GitHub-safe artifact name, encoded package version, filenames/components, actual package
name/version/architecture/dependencies, sizes/SHA-256 and optional development-export
compatibility. Archive bounds are 512 MiB compressed/per file, 1 GiB expanded and 20,000
entries. Assigned GitHub digest, run, job, artifact ID and workflow file are verified again
by the unprivileged validation job before download. Metadata uses standard package tooling;
no package scripts, hooks or artifact executables run during validation/publication.

GitHub forbids colon characters in artifact names. Attempt IDs retain execution:revision;
the graph assigns the safe artifact_name once and all consumers use that frozen field.
[GitHub artifact validation](https://github.com/actions/toolkit/blob/main/packages/artifact/src/internal/upload/path-and-artifact-name-validation.ts).

## Trusted native publication

The separate validation job has no signing/feed key. PACKAGES_OBSERVER_TOKEN is the existing
sandbox secret name used as source artifact reader. Its actual type, minimum actions/read
repository grants and cross-repository access are unverified activation inputs. Do not reuse
an overly broad token silently. If it cannot meet least privilege, the owner must supply the
reviewed read-only App/token path described in docs/setup-inputs.md. Controller App private
keys never enter a workflow.

The privileged job runs only deployment-owned code and uses package-publication. It checks
controller actor, worker ID, tooling, feed and public fingerprint against trusted worker
variables. Private/public correspondence is checked before signing. Keys are temporary
owner-only files/keyrings, with cleanup on success/failure and no caches/uploads. Existing
repository secrets must be reviewed/restricted before activation; a named environment alone
does not restrict repository-level secrets. Key identity/byte changes require owner approval.

DEB Packages indexes come from apt-ftparchive over retained actual packages. Release contains
checksums; InRelease and detached Release.gpg are signed and locally verified. DEBs do not
need embedded signatures. RPM packages are signed with rpmsign; createrepo_c generates
repodata and repomd.xml has a detached signature verified with gpgv. RPM package signatures
are checked with a private rpmdb containing only the configured trusted public key.

Paths are apt/DISTRIBUTION/SUITE/pool/NAMESPACE and
apt/DISTRIBUTION/SUITE/dists/SUITE/main/binary-ARCH; RPM uses
rpm/DISTRIBUTION/SUITE/ARCH. Packages retain all existing versions. Public archive/RPM key:
keys/apt.asc. Exports: exports/EXECUTION/REVISION/development.tar.gz.
Receipts: receipts/EXECUTION/REVISION.json. Receipts retain their exact feed repository/branch/HTTPS root, so a later deployment URL
change does not rewrite prerequisite artifact locations. Receipt package entries record final signed hashes
separately from unsigned build hashes, paths and exact dependency/build provenance.

An identical receipt replay verifies immutable package/export bytes and does not sign or
rewrite them. Index paths may have advanced after another publication; their recorded hashes
refer to the original feed commit. Existing package identities reject differing bytes rather
than overwrite them. Feed updates use a normal checked Git push, with no force/rebase. An
indeterminate push or missing HTTPS receipt must be reconciled before replacement; GitHub job
failure alone cannot prove the feed update did not happen. Controller receipt reconciliation and publication serialization are wired and exercised with
real MariaDB and synthetic GitHub/HTTPS evidence. A failed privileged job with no verified
receipt remains uncertain unless canonical assigned job/step evidence proves the sole
feed-write step was skipped. That definite pre-push failure permits explicit retry; a possible
write remains assigned until reconciliation. No replacement is inferred from a missing receipt.

The worker checks a conservative 900 MiB site bound before push. Pages currently limits
published sites to 1 GB, has a soft 100 GB/month bandwidth limit and a ten-minute deployment
timeout. The prepared workflow rejects links and oversized payloads and does not delete old
packages for capacity. Select another actual package-compatible hosting destination with the
owner if capacity becomes insufficient; no external service is silently added.
[GitHub Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits).

## Public variables and secret provisioning

Configure public worker variables CORE_CI_TOOLING_SHA, CONTROLLER_APP_BOT_ID, FEED_REPOSITORY,
FEED_REPOSITORY_ID, FEED_BRANCH, FEED_BASE_URL, PACKAGES_GPG_FINGERPRINT, FEED_WRITER_APP_ID
and FEED_WRITER_INSTALLATION_ID. Values belong to deployment configuration, not source JSON.
The native workflow consumes existing secret names APT_SIGNING_KEY, PACKAGES_APP_PRIVATE_KEY
and PACKAGES_OBSERVER_TOKEN, plus optional PACKAGES_GPG_PASSPHRASE. OpenWrt publication uses existing OPENWRT_USIGN_KEY and OPENWRT_APK_KEY. Secret names alone do not prove key match.
See keys/README.md for public provenance and the read-only secret-name inventory.

Use file input if owner-held material needs provisioning; these are templates, not executed
commands. Existing GitHub secrets cannot be downloaded. Do not expose them to migrate them.

```sh
gh secret set APT_SIGNING_KEY --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/existing-archive-private.asc
gh secret set PACKAGES_GPG_PASSPHRASE --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/existing-passphrase
gh secret set OPENWRT_USIGN_KEY --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/existing-usign-private.key
gh secret set OPENWRT_APK_KEY --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/existing-apk-private.pem
gh secret set PACKAGES_APP_PRIVATE_KEY --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/existing-feed-writer-app.pem
```

Omit optional passphrase when the key has none. Never put key values in arguments, shell
substitution, source, database or logs. Do not delete/change existing repository secrets until
the owner has reviewed all consumers and approved the protection/provisioning operation.
[GitHub secret file input](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-secrets).

Client acceptance must use scoped trusted keys, gpgcheck=1/repo_gpgcheck=1 for RPM and the
matching OpenWrt client tooling. APT base is FEED_BASE_URL/apt/DISTRIBUTION/SUITE with suite
SUITE and component main, using a dedicated Signed-By=/usr/share/keyrings/core-ci.asc file.
No --allow-untrusted, file-existence substitute or global trusted-key installation is accepted.
Actual apt/dnf/opkg/apk refresh/install/runtime acceptance remains undone.

## Latest OpenWrt SDK policy

The owner selected automatic latest SDK discovery. Source rows use suite/arch at row level
and build.target/build.format only. There is no SDK URL, SHA or exact patch release to edit.
The trusted worker owns discovery through its public OPENWRT_DOWNLOAD_BASE_URL variable
(default https://downloads.openwrt.org). It selects the latest stable patch in that suite
and records release/URL/official checksum as execution evidence. The worker must verify
that checksum before extraction. No registry/SDK request is made by the controller.
Roots choose the latest stable SDK; dependent work reuses the exact SDK/toolchain identity
in all selected prerequisite receipts, failing on disagreement. The prepared GitHub worker
verifies the SDK archive, imports only selected dependencies/own development exports and
runs the source OpenWrt hook. The publisher uses independently verified SDK host tools for
IPK/usign and APKv3/EC signing and indexing. Real SDK/build/sign/install execution has not
been accepted yet. The trusted build workflow stores
resolved environment evidence before hooks, separate from package artifacts. Retries use
read-only Actions access to earlier assigned runs and retain their resolved environment.
Evidence requests 90-day retention; repository limits and artifact expiry can prevent a
retry, which then fails clearly instead of changing inputs. Source hooks receive no Actions
token. Real GitHub execution remains an acceptance requirement.

Sandbox source recipes, explicit matrices and the external activation review are in
[sandbox/README.md](sandbox/README.md). No sandbox workflow installation is implied by local preparation.
