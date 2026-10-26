# Trusted public verification keys

This directory contains public verification material. Private counterparts belong in the
trusted worker's protected `package-publication` environment and owner-held recovery storage,
never this directory or a source repository.

## Selected publisher keys

| File | Use | SHA-256 of public file |
| --- | --- | --- |
| `apt.asc` | APT repository metadata and RPM package/repository verification | `cc1ac352f62047eb693040b876e4c6bf6ca10da93cd8bb6bb130a618c572086d` |
| `opkg.pub` | OpenWrt IPK index verification | `478a109a7c2c0d846801e4a4a6ad2873e885add6cbe3d6d7e01f7ff2c9b39aef` |
| `core-ci-apk.pem` | OpenWrt APK v3 EC P-256 verification | `7cae8895c8af1a543c72f1676865fbd8dbf12b3015f6064a4d288a9aa3fe8f26` |

OpenPGP fingerprint: `A4B307A9CFD583089547481DE58C165F4340EE99`.
Usign fingerprint: `9bc0ca6b2dc8a9a9`.

The current APK publisher selects `core-ci-apk.pem`, not other key files that may also be
retained in the feed. Do not substitute another key merely because its filename looks suitable.

Controller settings pin the trusted OpenPGP fingerprint and IPK/APK public-file hashes.
Trusted worker variables and reviewed key files must agree. The publication job checks private
key correspondence before signing. Public keys supplied in build artifacts grant no authority.

## Provision and verify

The [deployment guide](../../../docs/deployment/README.md) lists exact secret names and
file-input provisioning commands. Use standard GnuPG/usign/OpenSSL and matching APK v3 tools
to inspect public identities and verify a signing challenge. Do not print private material or
retrieve it from workflow logs. GitHub stored secret values cannot be downloaded.

Changing a deployed signing identity requires explicit owner approval and a client trust
rollout. Matching key filenames or secret names alone does not prove matching key material.
