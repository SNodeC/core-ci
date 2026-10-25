# Sandbox deployment status — 2026-10-25

The owner authorized deployment to VOC-Experiments. The following fresh files are now
committed on sandbox main branches. Existing tags and published package identities were
preserved. A new demo-core v2.0.0 tag selects source commit
7cffc1403a20664c8c07e49ae559e16b7089479e; selected source and workflow refs are independent.

## Installed revisions

| Repository | Installed revision |
| --- | --- |
| VOC-Experiments/ci-coordinator | [5d124fb](https://github.com/VOC-Experiments/ci-coordinator/commit/5d124fbdec05cfdfe2ac4998847c51325f6c7a98) |
| VOC-Experiments/demo-core | [1cfcfa1](https://github.com/VOC-Experiments/demo-core/commit/1cfcfa17de500bb68f389bf6b8c4cbf27ddcbb57) |
| VOC-Experiments/demo-app | [e406984](https://github.com/VOC-Experiments/demo-app/commit/e4069844a0b4235451a31e97b92818e991c6b4a6) |
| VOC-Experiments/demo-side | [6b8f46a](https://github.com/VOC-Experiments/demo-side/commit/6b8f46a8311c0c968dc8fcbccf33f199b6e3dff9) |
| VOC-Experiments/demo-join | [90f2dbb](https://github.com/VOC-Experiments/demo-join/commit/90f2dbbfb5ffcc138aaeccdc53cfcf394bbead81) |
| VOC-Experiments/packages | [46530d7](https://github.com/VOC-Experiments/packages/commit/46530d78c2b7eb1438ed2a8c8f1537d439f0650f) |

All source calls pin the full worker commit `5d124fbdec05cfdfe2ac4998847c51325f6c7a98`.

Approved source workflow blob: `b9aad9c42f1c59994fadf607eeb283c89b3ef1d4`.
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
Actual OpenWrt package signing/client installation remains outstanding.

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
The feed's pre-existing status/*.md and status.json are stale and are not updated by this
publisher. Use the controller graph, assigned GitHub runs and HTTPS receipts for these results.

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

Actual APK client installation acceptance, completion of dependency
chains/diamonds and the remaining full live acceptance are outstanding. Both OpenWrt builds now pass with
corrected SDK preparation and source-owned tool lookup:
[IPK 40231880390](https://github.com/VOC-Experiments/demo-core/actions/runs/40231880390),
[APK 40231884159](https://github.com/VOC-Experiments/demo-core/actions/runs/40231884159).
These builds include independent validation and artifact upload. OpenWrt APK signing/feed
publication and APK client installation remain unverified.
Both SDK preparation stages passed in real GitHub runs
[IPK 40231441419](https://github.com/VOC-Experiments/demo-core/actions/runs/40231441419) and
[APK 40231443502](https://github.com/VOC-Experiments/demo-core/actions/runs/40231443502).
Their subsequent source-hook compiler lookup failure is corrected in new source tag
v2.1.0 at 1cfcfa17de500bb68f389bf6b8c4cbf27ddcbb57; v2.0.0 remains unchanged.
The core-ci application branch is committed locally; no application push, PR or installation
was performed. The running executable is a development build under the temporary build root.
The local SNode.C TLS webhook listener/certificate setup remains outstanding; selected public
endpoint is https://gw-home.vchrist.at:8443/github/webhook. Existing port 443 stays untouched.
No extra source checkout was created. Task scripts/evidence are under the permitted temporary
build directory; they contain no private key or token values.
