# Sandbox deployment status — 2026-10-25

The owner authorized deployment to VOC-Experiments. The following fresh files are now
committed on sandbox main branches. Existing source tags and package bytes were unchanged.

## Installed revisions

| Repository | Installed revision |
| --- | --- |
| VOC-Experiments/ci-coordinator | [b45e47c](https://github.com/VOC-Experiments/ci-coordinator/commit/b45e47c415501c8afe0d3abb9c5cfb71f4e8c71c) |
| VOC-Experiments/demo-core | [7cffc14](https://github.com/VOC-Experiments/demo-core/commit/7cffc1403a20664c8c07e49ae559e16b7089479e) |
| VOC-Experiments/demo-app | [00f26fe](https://github.com/VOC-Experiments/demo-app/commit/00f26fe353f7014187d1be292e5cd93e93811494) |
| VOC-Experiments/demo-side | [b92d0c0](https://github.com/VOC-Experiments/demo-side/commit/b92d0c0c301754b0be38dc778b3714cfcb5f298c) |
| VOC-Experiments/demo-join | [0a9c3cf](https://github.com/VOC-Experiments/demo-join/commit/0a9c3cf99f7684ef8a41764466a16aa92d8525e0) |
| VOC-Experiments/packages | [48144d1](https://github.com/VOC-Experiments/packages/commit/48144d1106b07252c5519f16df34133f9d67f496) |

All source calls pin the full worker commit `b45e47c415501c8afe0d3abb9c5cfb71f4e8c71c`.

Approved source workflow blob: `4a628a8ff400dde010378864170c2fe6f90f352d`.
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

## Still outstanding

No graph-driven GitHub package build, signed package publication or apt/dnf/opkg/apk client
installation acceptance is claimed. Source tags were not created. No core-ci application
push, PR, installation or running controller was included in the workflow-file deployment.
The local SNode.C TLS webhook listener/certificate setup remains outstanding; selected public
endpoint is https://gw-home.vchrist.at:8443/github/webhook. Existing port 443 stays untouched.
No extra source checkout was created. Task scripts/evidence are under the permitted temporary
build directory; they contain no private key or token values.
