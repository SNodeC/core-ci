# Sandbox deployment status — 2026-10-25

The owner authorized deployment to VOC-Experiments. The following fresh files are now
committed on sandbox main branches. Existing tags and published package identities were
preserved. A new demo-core v2.0.0 tag selects source commit
7cffc1403a20664c8c07e49ae559e16b7089479e; selected source and workflow refs are independent.

## Installed revisions

| Repository | Installed revision |
| --- | --- |
| VOC-Experiments/ci-coordinator | [929f140](https://github.com/VOC-Experiments/ci-coordinator/commit/929f140d890fff398772949d24e30b60a18f5969) |
| VOC-Experiments/demo-core | [6e3c974](https://github.com/VOC-Experiments/demo-core/commit/6e3c97496f8b2e9e187d2fdc6b626cbc2ef44209) |
| VOC-Experiments/demo-app | [13fc80d](https://github.com/VOC-Experiments/demo-app/commit/13fc80d91df1e4f9b8481e9c22535b545ef6d5b3) |
| VOC-Experiments/demo-side | [c31caeb](https://github.com/VOC-Experiments/demo-side/commit/c31caeb264394240636aed62c6ba2bcfe9161ecb) |
| VOC-Experiments/demo-join | [15abe86](https://github.com/VOC-Experiments/demo-join/commit/15abe8651234b2c2251bac1e3eafe6c93f29e72a) |
| VOC-Experiments/packages | [a5d5ada](https://github.com/VOC-Experiments/packages/commit/a5d5adad37fd6cfbb470f3ca6ef585cb1962eba4) |

All source calls pin the full worker commit `929f140d890fff398772949d24e30b60a18f5969`.

Approved source workflow blob: `808c3a838f52b9287144835ba23f027ab516534d`.
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

## Still outstanding

Actual apt/dnf/opkg/apk client installation acceptance, dependency chains/diamonds and the
remaining full live acceptance are outstanding. OpenWrt extraction now passes for both
official SDKs; its SDK configuration/build verification continues with corrected tooling.
The core-ci application branch is committed locally; no application push, PR or installation
was performed. The running executable is a development build under the temporary build root.
The local SNode.C TLS webhook listener/certificate setup remains outstanding; selected public
endpoint is https://gw-home.vchrist.at:8443/github/webhook. Existing port 443 stays untouched.
No extra source checkout was created. Task scripts/evidence are under the permitted temporary
build directory; they contain no private key or token values.
