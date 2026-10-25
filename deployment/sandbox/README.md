# VOC-Experiments sandbox activation review

These files are prepared locally. They have not been committed to the sandbox repositories,
installed as workflows, or exercised by GitHub jobs. They implement source-owned examples,
not controller policy. No production SNodeC repository, credential, tag or feed is a target.

## Source-owned files

For each of demo-core, demo-app, demo-side and demo-join, commit:

- `ci/build.py`, `ci/native.sh`, `ci/openwrt.sh` from this directory's `ci/`.
- `ci/packages.json` from `definitions/REPOSITORY.json`.
- `.github/workflows/core-ci-build.yml`, rendered from the reviewed thin template with the
  actual full trusted worker commit SHA. Do not dispatch an unrendered template.

The recipe uses each project's existing C sources. Core exports demo-core.h and its own
static library; side exports demo-side.h and its own static library. App and join export no
other project's files. All four provide a runtime binary. Native hooks execute the built
binary and check its interface; join also checks that side's result equals core plus two.
Cross-target OpenWrt runtime checks require a matching client/emulator and remain outstanding.

The graph is core → app/side → join, with an additional declared core → join edge because
join includes core's public header directly. Join also declares app as a package prerequisite
so that its publication exercises both arms of the diamond. Recipe dependencies are by name,
without exact rebuild pins: these demo source interfaces declare broad compatibility. Build
inputs still use the graph's exact selected artifacts. This example is not a policy for
unrelated enrolled projects.

Rows are explicit JSON: Debian trixie AMD64 and ARM64, Fedora 43 x86_64, OpenWrt 24.10 x86_64
IPK and OpenWrt 25.12 x86_64 APK. OpenWrt roots discover latest stable patch releases and
freeze SDK identities. Dependents inherit the selected prerequisites' SDK/toolchain, including
at joins. The controller contains no row catalogue or SDK resolver.

## External activation changes requiring owner approval

1. In VOC-Experiments/ci-coordinator, add the fresh `deployment/worker/` contents at repository
   root. Preserve unrelated files. Record the resulting full commit and publication workflow
   blob SHA. Configure public variables described in ../README.md and protect trusted code.
2. Disable these existing sandbox worker workflows through GitHub's reversible workflow
   disable API; do not reuse their implementation:

   - build-packages.yml
   - control-packages.yml
   - live-capacity-check.yml
   - notify-release.yml
   - orchestrate-packages.yml
   - publish-guides-and-status.yml
   - sign-and-publish-packages.yml

3. Disable `.github/workflows/build-distribution-packages.yml` in all four source
   repositories, plus `.github/workflows/capacity-probe.yml` in demo-side (all were active
   at the read-only inventory). Install the three hook files, definition and rendered thin
   workflow above through checked commits on main before creating tags.
4. In VOC-Experiments/packages, add the prepared `deployment/feed/.github/workflows/pages.yml`
   as `.github/workflows/pages.yml` and enable GitHub Pages using Actions. Preserve existing
   package versions and unrelated feed contents. The selected public package root is
   https://voc-experiments.github.io/packages/; current Pages access returned HTTP 404, so
   this address is not yet accepted as a serving feed.
5. Package signing keys and the feed-writer key are provisioned in package-publication,
   restricted to worker main. Review existing repository-scoped signing/feed-writer secrets
   before activation: they remain present and are not protected by this environment policy.
   After the old workflows are disabled, the proposed cleanup removes worker repository
   secrets APT_SIGNING_KEY, OPENWRT_USIGN_KEY, OPENWRT_APK_KEY and PACKAGES_APP_PRIVATE_KEY;
   their environment-scoped copies remain. Keep PACKAGES_OBSERVER_TOKEN for validation.
   Remove obsolete source-repository PACKAGE_CAPTURE_APP_PRIVATE_KEY and
   PACKAGE_NOTIFY_APP_PRIVATE_KEY after disabling their consumers. These exact removals
   are part of the requested activation approval. Owner-held originals remain available.
   Never rotate keys merely for this transition.
6. Configure the controller with the actual tooling/workflow pins and existing App file,
   local-only UI, explicit MariaDB settings and raw-body webhook signature secret. The owner
   must supply the public HTTPS webhook URL/TLS deployment. There is no assumed external
   tunnel or public configuration-writing UI.

This list is a review boundary, not a claim that external activation was authorized or done.
No application push, PR, merge or installation is included implicitly.

## Initial execution sequence to approve

Propose fresh v2.0.0 tags on the reviewed source commits, leaving all existing tags intact.
The existing v0/v1 tags predate these fresh definitions and cannot stand in for valid new
source selections. Create the root tag/enrollment first and wait for verified publication;
then app and side; then join. This follows the owner's rule that missing prerequisite work
is an explicit error rather than automatically created upstream work. Every assigned target
must progress independently, and join must wait for all selected prerequisites.

Also propose v2.1.0 for a distinct newer demo-core source commit during acceptance, with a
reviewable one-value source change; preserve v2.0.0 so direct older-source selection can be
proven. No other new tag name is implicitly authorized by this proposal.

Subsequent acceptance must cover a distinct newer source commit/tag, direct older selection,
tag movement without changing assigned inputs, failed work with explicit retry, delayed and
duplicate reports, restart with assigned GitHub work, and a diamond with one failed arm.
Any new acceptance tags/commits are sandbox-only and must be included in the activation scope.

## Evidence and outstanding checks

The sandbox hook test builds all four projects as DEB and RPM using real local tools over
synthetic C fixtures. It checks package metadata, dependency declarations, own-only exports
and the join's runtime result with a changed fixture value. The controller's definition
parser validates all four committed definitions together, including their five target rows.
These are local proofs, not GitHub/container/package-client installation acceptance.

Real GitHub SDK preparation, builds, publication, Pages serving and signature verification
must run next. Perform actual apt/dnf/opkg/apk refresh and exact-version installation with
trusted public keys; reject wrong keys and modified metadata/packages. Record architecture
and runtime limitations explicitly. Do not substitute file-existence checks or untrusted
installation flags for those proofs.
