# Sandbox deployment status — 2026-10-25

The owner authorized deployment to VOC-Experiments. The following fresh files are now
committed on sandbox main branches. Existing tags and published package identities were
preserved. A new demo-core v2.0.0 tag selects source commit
7cffc1403a20664c8c07e49ae559e16b7089479e; selected source and workflow refs are independent.

## Installed revisions

| Repository | Installed revision |
| --- | --- |
| VOC-Experiments/ci-coordinator | [792949f](https://github.com/VOC-Experiments/ci-coordinator/commit/792949f2331fc7da76a70b1c5422ef55456c3ac9) |
| VOC-Experiments/demo-core | [8dc5db6](https://github.com/VOC-Experiments/demo-core/commit/8dc5db6734b8ba64df421616977f2b409fc6255a) |
| VOC-Experiments/demo-app | [ba25b1e](https://github.com/VOC-Experiments/demo-app/commit/ba25b1e850096824baa05fcb9d9265553111f944) |
| VOC-Experiments/demo-side | [4d2a5b2](https://github.com/VOC-Experiments/demo-side/commit/4d2a5b260a230337d1da9b171ee7c4554a76c53f) |
| VOC-Experiments/demo-join | [823112e](https://github.com/VOC-Experiments/demo-join/commit/823112ea395f212d185d704facbe2097f73bb887) |
| VOC-Experiments/packages | [a3a4427](https://github.com/VOC-Experiments/packages/commit/a3a4427407583d440920af03b9342df5da1ec2e5) |

All source calls pin the full worker commit `792949f2331fc7da76a70b1c5422ef55456c3ac9`.

Approved current source workflow blob: `0cba89d9016b81f9ce028eaa09e9b5d2aaeb0ecd`.
Approved publication workflow blob: `3d84013c0d7e77147b310c5647a471dc3d1d0a91`.
Protected controller configuration and worker public variables match these identities.

## Verified live deployment evidence

- All twelve old sandbox worker/source workflows are disabled. The fresh reusable build,
  publication, four source workflows and feed Pages workflow are active.
- [Pages deployment](https://github.com/VOC-Experiments/packages/actions/runs/40226358594)
  succeeded. HTTPS retrieval of the existing keys/apt.asc exactly matched its GitHub bytes.
- [Source Actions reader check](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40226480435)
  succeeded for all four source repositories using the actual repository observer secret.
  The temporary workflow was disabled and removed after success. This proves current read
  access, not the complete token permission set or a package build/publication.
- The owner-held local observer-token file returned HTTP 401; it is not assumed to match
  the working GitHub secret. No secret value was read back or exposed.
- package-publication remains main-only, with four configured environment key secrets.
  Public worker variables and feed-writer identifiers are configured.

## Signing identity alignment — resolved without replacing existing feed keys

Credential-only log inspection located the original owner-held sandbox APT/IPK private
files. Standard GnuPG/usign challenge signing and verification prove their correspondence
to existing feed keys. Protected publication secrets, worker trusted public files and
controller configuration now use those identities.

| Identity | Active key |
| --- | --- |
| APT/RPM OpenPGP fingerprint | A4B307A9CFD583089547481DE58C165F4340EE99 |
| IPK usign fingerprint | 9bc0ca6b2dc8a9a9 |
| New SDK-specific APK feeds | EC P-256, keys/core-ci-apk.pem |
| Legacy APK feeds | Existing RSA-3072 keys/apk.pem, unchanged |

The legacy APK private/public pair was also located, verified and retained. RSA-3072 does
not meet the new publisher's required EC P-256 contract. The already-provisioned P-256 key
therefore uses a distinct public filename for new SDK-specific feeds. No existing trust key
was replaced or generated. Exact Git-tree comparison proves every prior feed blob remained
unchanged; only keys/core-ci-apk.pem was added. HTTPS retrieval verified all four public
key files against the selected or preserved local public bytes.

[Pages deployment after key alignment](https://github.com/VOC-Experiments/packages/actions/runs/40228213639)
succeeded. Three affected local suites pass, including real native signatures and regression
checks for preserving the old APK file, replaying the new key and rejecting an overwrite.
Actual OpenWrt package signing/client installation is verified in the later acceptance results below.

Obsolete signing/feed-writer repository secrets and old source App secrets were removed as
planned. Active keys remain in main-only package-publication; the valid read-only observer
secret remains at worker repository scope. Owner-held original and collected private/public
files remain available in Git-ignored local storage with restrictive permissions. No secret
value was retrieved from GitHub or exposed in logs/artifacts.

## Graph-driven native acceptance

The development controller is running at http://127.0.0.1:8080/ with a real isolated managed
MariaDB. UI enrollment selected demo-core v2.0.0 and dispatched actual GitHub builds.

| Target | Successful build | Successful publication |
| --- | --- | --- |
| Fedora 43 x86_64 | [40229812225](https://github.com/VOC-Experiments/demo-core/actions/runs/40229812225) | [40229989203](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40229989203) |
| Debian trixie amd64 | [40229819909](https://github.com/VOC-Experiments/demo-core/actions/runs/40229819909) | [40230229221](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40230229221) |
| Debian trixie arm64 | [40229822170](https://github.com/VOC-Experiments/demo-core/actions/runs/40229822170) | [40230358488](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40230358488) |

The controller verified publication receipts and marked all three targets published.
The feed status pages now derive from retained publication receipts, as described below.
Use the controller graph for waiting/failed work and retry administration.

## Actual package-manager client acceptance

The following GitHub runs reject an unrelated key, refresh the signed HTTPS repository,
install the exact selected package version, and execute the installed program:

| Client | Version | Acceptance run |
| --- | --- | --- |
| apt / Debian trixie amd64 | 2.1.0-21 | [40234719240](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40234719240) |
| apt / Debian trixie arm64 | 2.1.0-22 | [40235051112](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40235051112) |
| dnf / Fedora 43 x86_64 | 2.1.0-23 | [40234717535](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40234717535) |
| opkg / OpenWrt 24.10.8 x86_64 | 2.1.0-r24 | [40234883261](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40234883261) |

The OpenWrt IPK publisher [40234419872](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40234419872)
completed signing and a checked feed update. The controller verified the HTTPS receipt and
started dependent work. These are real GitHub jobs using the published public keys;
no untrusted-install override is used. Client runtime output is `demo-core 60`.

## Still outstanding

Public webhook activation and remaining full live acceptance are outstanding. All 20 v2.1.0
graph executions are published. Native/IPK diamond clients and all four root package clients
pass, including the final APK diamond client check recorded below. Both OpenWrt builds now pass with
corrected SDK preparation and source-owned tool lookup:
[IPK 40231880390](https://github.com/VOC-Experiments/demo-core/actions/runs/40231880390),
[APK 40231884159](https://github.com/VOC-Experiments/demo-core/actions/runs/40231884159).
These builds include independent validation and artifact upload. APK signing and client
installation are now verified below.
Both SDK preparation stages passed in real GitHub runs
[IPK 40231441419](https://github.com/VOC-Experiments/demo-core/actions/runs/40231441419) and
[APK 40231443502](https://github.com/VOC-Experiments/demo-core/actions/runs/40231443502).
Their subsequent source-hook compiler lookup failure is corrected in new source tag
v2.1.0 at 1cfcfa17de500bb68f389bf6b8c4cbf27ddcbb57; v2.0.0 remains unchanged.
The core-ci application branch is committed locally; no application push, PR or installation
was performed. The running executable is a development build under the temporary build root.
The separate SNode.C TLS webhook listener is implemented and locally tested, disabled by
default. Public certificate/key configuration and activation remain outstanding; selected public
endpoint is https://gw-home.vchrist.at:8443/github/webhook. Existing port 443 stays untouched.
No extra source checkout was created. Task scripts/evidence are under the permitted temporary
build directory; they contain no private key or token values.


## Native diamond installation

The controller enrolled demo-app/demo-side/demo-join at their new v2.1.0 source tags. The
amd64 join stayed waiting without an assigned run while only demo-app was published. It
became eligible after demo-side publication. [APT client run 40236964119](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40236964119)
then rejected an unrelated key and installed all four signed packages from the HTTPS feed:
core 2.1.0-21, app 2.1.0-26, side 2.1.0-31 and join 2.1.0-36. The installed join printed
`demo-join core 60, side 62`. This is actual package-manager dependency resolution and
execution, not a manifest/file-existence check.


## OpenWrt IPK diamond installation

All four projects published against the same recorded OpenWrt 24.10.8 SDK identity,
including [join publication 40237059539](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40237059539).
[opkg client run 40237394589](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40237394589)
passes prerelease/rebuild comparisons, rejects an unrelated signing key, refreshes the
signed HTTPS feed and installs the join and prerequisites. Installed runtime output is
`demo-join core 60, side 62`. The side prerequisite's cancelled rebuild 34 and explicit
successful retry/rebuild 41 are both retained in MariaDB.

The same native diamond installation/runtime check also passes on
[ARM64 APT 40237555269](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40237555269)
and [Fedora DNF 40237553823](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40237553823).
These are native GitHub runners for the selected architectures, not file-only checks.


## OpenWrt APK v3 signing and installation

[Publisher 40237512801](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40237512801)
completed EC P-256 package/index signing, checked feed publication and HTTPS receipt proof.
[Client run 40237801462](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40237801462)
uses the checksum-verified OpenWrt 25.12.6 rootfs and its actual apk-tools 3.0.5. It rejects
an unrelated key with `UNTRUSTED signature`, validates encoded version grammar and ordering,
refreshes signed `packages.adb`, installs demo-core 2.1.0-r25 and executes `demo-core 60`.
There is no APK v2 index or untrusted-install override. All dependent APK builds and
publications subsequently passed, including join publisher
[40238830522](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40238830522).
All four receipts record the same SDK SHA-256
`1864152619092e781fa36a401960c8e9ae42d6a364c44ee5f0b197ca5f3c0fcc`.


[APK diamond client 40239260480](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40239260480)
passes wrong-key rejection, version grammar/ordering, signed index refresh and installation
of core 2.1.0-r25, app 2.1.0-r30, side 2.1.0-r35 and join 2.1.0-r40. The installed join
prints `demo-join core 60, side 62`. All five configured targets now have complete diamond
publication and actual package-manager installation/runtime proof.

## Final v2.1.0 graph snapshot

The following is dated acceptance evidence, not a separate state authority. On 2026-10-25,
the real MariaDB-backed graph reports all 20 executions published. A further explicit
reconciliation returns `assigned: 0` and preserves execution revisions and run assignments.
This proves idle reconciliation, not public webhook delivery or duplicate delivery acceptance.

| Repository | Target | Rebuild | Build | Publication |
| --- | --- | --- | --- | --- |
| demo-app | debian-trixie-amd64 | 26 | [40234838728](https://github.com/VOC-Experiments/demo-app/actions/runs/40234838728) | [40235213985](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40235213985) |
| demo-app | debian-trixie-arm64 | 27 | [40235215552](https://github.com/VOC-Experiments/demo-app/actions/runs/40235215552) | [40235428210](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40235428210) |
| demo-app | fedora-43-x86_64 | 28 | [40234840873](https://github.com/VOC-Experiments/demo-app/actions/runs/40234840873) | [40235920829](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40235920829) |
| demo-app | openwrt-24.10-x86_64 | 29 | [40234882705](https://github.com/VOC-Experiments/demo-app/actions/runs/40234882705) | [40236509206](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40236509206) |
| demo-app | openwrt-25.12-x86_64 | 30 | [40237822465](https://github.com/VOC-Experiments/demo-app/actions/runs/40237822465) | [40238385559](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40238385559) |
| demo-core | debian-trixie-amd64 | 21 | [40231878745](https://github.com/VOC-Experiments/demo-core/actions/runs/40231878745) | [40234287306](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40234287306) |
| demo-core | debian-trixie-arm64 | 22 | [40231882198](https://github.com/VOC-Experiments/demo-core/actions/runs/40231882198) | [40234884519](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40234884519) |
| demo-core | fedora-43-x86_64 | 23 | [40231876849](https://github.com/VOC-Experiments/demo-core/actions/runs/40231876849) | [40232214325](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40232214325) |
| demo-core | openwrt-24.10-x86_64 | 24 | [40231880390](https://github.com/VOC-Experiments/demo-core/actions/runs/40231880390) | [40234419872](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40234419872) |
| demo-core | openwrt-25.12-x86_64 | 25 | [40231884159](https://github.com/VOC-Experiments/demo-core/actions/runs/40231884159) | [40237512801](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40237512801) |
| demo-join | debian-trixie-amd64 | 36 | [40236507555](https://github.com/VOC-Experiments/demo-join/actions/runs/40236507555) | [40236806285](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40236806285) |
| demo-join | debian-trixie-arm64 | 37 | [40235752993](https://github.com/VOC-Experiments/demo-join/actions/runs/40235752993) | [40236928097](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40236928097) |
| demo-join | fedora-43-x86_64 | 38 | [40236060513](https://github.com/VOC-Experiments/demo-join/actions/runs/40236060513) | [40237366899](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40237366899) |
| demo-join | openwrt-24.10-x86_64 | 39 | [40236808481](https://github.com/VOC-Experiments/demo-join/actions/runs/40236808481) | [40237059539](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40237059539) |
| demo-join | openwrt-25.12-x86_64 | 40 | [40238571733](https://github.com/VOC-Experiments/demo-join/actions/runs/40238571733) | [40238830522](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40238830522) |
| demo-side | debian-trixie-amd64 | 31 | [40234978539](https://github.com/VOC-Experiments/demo-side/actions/runs/40234978539) | [40236338792](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40236338792) |
| demo-side | debian-trixie-arm64 | 32 | [40235217799](https://github.com/VOC-Experiments/demo-side/actions/runs/40235217799) | [40235560767](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40235560767) |
| demo-side | fedora-43-x86_64 | 33 | [40234973732](https://github.com/VOC-Experiments/demo-side/actions/runs/40234973732) | [40235750833](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40235750833) |
| demo-side | openwrt-24.10-x86_64 | 41 | [40235443413](https://github.com/VOC-Experiments/demo-side/actions/runs/40235443413) | [40236058413](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40236058413) |
| demo-side | openwrt-25.12-x86_64 | 35 | [40237824712](https://github.com/VOC-Experiments/demo-side/actions/runs/40237824712) | [40238019305](https://github.com/VOC-Experiments/ci-coordinator/actions/runs/40238019305) |


## Approved receipt-derived status deployment

[Feed status](https://voc-experiments.github.io/packages/) now displays all 29 retained
publication receipts, including the 20 v2.1.0 diamond results. Root/status HTML, Markdown
pages and JSON are generated by one trusted worker renderer. The initial documentation-only
refresh [a3a4427](https://github.com/VOC-Experiments/packages/commit/a3a4427407583d440920af03b9342df5da1ec2e5)
preserves all package/index/key/receipt and unrelated document blobs. It does not re-sign
or rebuild packages. [Pages deployment 40241209262](https://github.com/VOC-Experiments/packages/actions/runs/40241209262)
passes; HTTPS bytes and the visible Chrome table were verified. The new publisher integration
is locally tested; a subsequent real package publication with this renderer has not yet run.

Worker tooling and all four source dispatch workflows now pin
`792949f2331fc7da76a70b1c5422ef55456c3ac9`. Past executions retain their original selections
and tooling pins. The development controller restarted with updated public configuration;
all persisted execution IDs, revisions, states and run assignments are unchanged.

The owner approved public TLS activation. Actual certificate-chain and matching private-key
paths for gw-home.vchrist.at are still required; no suitable certificate was found in the
standard local locations. No public webhook activation or real delivery is claimed yet.
