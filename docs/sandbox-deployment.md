# Sandbox deployment status — 2026-10-25

The owner authorized deployment to VOC-Experiments. The following fresh files are now
committed on sandbox main branches. Existing source tags and package bytes were unchanged.

## Installed revisions

| Repository | Installed revision |
| --- | --- |
| VOC-Experiments/ci-coordinator | [eda8bb7](https://github.com/VOC-Experiments/ci-coordinator/commit/eda8bb7ca7431479666f2a99a8c7e5659912646b) |
| VOC-Experiments/demo-core | [9421ca6](https://github.com/VOC-Experiments/demo-core/commit/9421ca65b245e56b46b08fe957dee5e155d4efdf) |
| VOC-Experiments/demo-app | [92da783](https://github.com/VOC-Experiments/demo-app/commit/92da78300f9f27fbcc4684c05730432e73b72bc0) |
| VOC-Experiments/demo-side | [89faa7a](https://github.com/VOC-Experiments/demo-side/commit/89faa7aa1c5734dfb181e7dd05f5ae3e2bece717) |
| VOC-Experiments/demo-join | [2a8283f](https://github.com/VOC-Experiments/demo-join/commit/2a8283f2fa9ef870a70a5cfb1121d2fad3ae3cb8) |
| VOC-Experiments/packages Pages workflow | [da55734](https://github.com/VOC-Experiments/packages/commit/da55734a76947186d675afd5b15367cc98738e0a) |

The worker main branch subsequently advanced only to install and remove a temporary
read-only deployment credential check. All source reusable-workflow calls remain pinned
to full tooling commit `eda8bb7ca7431479666f2a99a8c7e5659912646b`.

Approved source workflow blob: `08df317df5592f3aca7b85f0057dc38c5ec9d2c4`.
Approved publication workflow blob: `3d84013c0d7e77147b310c5647a471dc3d1d0a91`.
The protected controller configuration was updated with these exact deployed identities.

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

## Signing identity conflict — publication blocked pending owner decision

The feed already has three different public keys at the expected keys/ paths. The guard
rejected replacement before any feed key commit or package publication. These are different
key identities, not formatting differences. Existing repository secrets remain untouched:
they may be the only remaining copies of the old sandbox signing keys.

| Identity | Existing feed | Selected/provisioned worker |
| --- | --- |
| OpenPGP fingerprint | A4B307A9CFD583089547481DE58C165F4340EE99 | 8BBFD49E3C826FDB1416C79E60046744B15B0E05 |
| usign fingerprint | 9bc0ca6b2dc8a9a9 | f6fd78dca70698e8 |
| APK public key | Different EC key | Reviewed worker apk.pem |

Before publication, either explicitly authorize the sandbox transition to the selected
keys (retaining old public keys and package versions), or provide matching owner-held
private files for the current feed identities. Do not generate replacement keys or silently
rotate the feed. The newly provisioned keys have not signed a package/feed publication.

## Still outstanding

No graph-driven GitHub package build, signed package publication or apt/dnf/opkg/apk client
installation acceptance is claimed. Source tags were not created. No core-ci application
push, PR, installation or running controller was included in the workflow-file deployment.
The local SNode.C TLS webhook listener/certificate setup remains outstanding; selected public
endpoint is https://gw-home.vchrist.at:8443/github/webhook. Existing port 443 stays untouched.
No extra source checkout was created. Task scripts/evidence are under the permitted temporary
build directory; they contain no private key or token values.
