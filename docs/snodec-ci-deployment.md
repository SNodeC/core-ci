# SNodeC package CI deployment

The additional SNodeC controller instance runs independently on vchrist.
Its existing dashboard is http://core.vchrist.at:8081/ and its signed
webhook listener is https://core.vchrist.at:8444/github/webhook. VOC remains
independent on port 8080. No additional SSE page or controller endpoint exists.

## Scope

Migration changes belong to ci/, .github/workflows/, and the trusted worker.
Do not modify application VERSION files, CMake files, package recipes or product
sources to accommodate CI. Versions remain SNode.C 2.1.0, MQTTSuite 1.0.2 and
snode.c-idp 2.0.2. The initial migration's changes outside CI were reverted.

Native hooks configure the existing source and use its CPack components. The
worker prepares declared system dependencies before the unprivileged hook runs.
Package revisions come from the controller assignment, not VERSION edits.

MQTTSuite's OpenWrt hook consumes the exact core development export. Before
skipping core compilation it asks GNU Make to evaluate core's prerequisites
from the SDK-generated tmp/.packagedeps and .config, then builds those external
prerequisites. It neither changes the product recipe nor copies core's dependency
list into MQTTSuite. The normal SDK graph builds the application afterward.

## Deployed identities

- Coordinator App: 5452899; installation: 174754904.
- Source artifact reader App: 5453119; installation: 174760616.
- Status notifier App: 5453314; installation: 174770512.
- Worker: SNodeC/DistributionPackages, tooling commit
  8b2c2fe63b4094ecfea286fca95ed6c02bcd077c.
- Source workflow blob: 7ca413158acb144b480c0141c4a3878473110e0d.
- Publisher workflow blob: 41359032ccb96c378b735b232a74760530042188.
- Feed: SNodeC/Packages, main; https://snodec.github.io/Packages/.

Three enrolled definitions retain 178 targets in total: 76 core, 76 MQTTSuite,
and 26 IDP. Both Raspberry Pi OS environments were prepared from official
checksum-pinned disk images; their immutable OCI references are in the source
definitions. The read-only artifact App and source Actions registry grants
cover the participating repositories. Secrets are not part of this document.

## Static status

The GitHub Markdown presentation is preserved, including
https://github.com/SNodeC/Packages/blob/main/status/snodec.md#banner. A status-only
workflow consumes the dashboard's existing public /events initial snapshot
through ready=true, then disconnects. It writes static status documents using
the existing feed-writer concurrency group. Source notifications and publisher
workflow events request refreshes; they never schedule package builds.

The old 76-row SNode.C status rendered byte-identically with the same saved
facts (33128 bytes). The new empty feed deliberately has no old versions,
packages or publication times. Public signing key bytes are unchanged.

## Validation and activation boundary

Local real CPack DEB/RPM component fixtures and HTTP/SSE status fixtures pass.
A GNU Make fixture exercises the actual CI dependency-query block, including
conditional dependencies. Source hooks pass shell syntax checks. The prepared
Raspberry Pi images and source-notify to status-refresh path succeeded on GitHub.
Signed TLS ping succeeds and unsigned requests are rejected.

These checks do not validate all 178 target builds. No package chain has run.
The owner must authorize the next build trigger and any exact release tags.
Existing immutable tags can use separately frozen, enrolled CI definitions and
hooks. The application tag/commit stays unchanged; CI_DIR holds the verified CI
checkout and SOURCE_DIR holds the tagged product. Refresh enrollment before new
selections when CI changes. Duplicate authenticated tag events remain idempotent.
Do not move tags, change product versions or invent release versions for CI.
The App webhook is configured; signed ping and real workflow events were accepted.

## Pre-activation CI review corrections

OpenWrt recipes receive numeric rebuild revisions; the SDK supplies the `r`
prefix. MQTTSuite reads core receipt source tag/revision/format through the
existing encoder, without assuming a manifest-only `package_version` field.
The assigned Publish job checks its original native/OpenWrt/retention predicate
in its first step, making rejected inputs visible as a skipped feed-push step.
Failed or completed push without confirmed publication retains uncertainty.
These fixes change CI only; product versions, build files and recipes stay intact.
