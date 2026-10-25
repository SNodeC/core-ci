# Trusted public verification keys

APT/RPM and IPK use the existing VOC-Experiments/packages feed identities. Their public
files were verified against feed commit da55734a76947186d675afd5b15367cc98738e0a and
matching owner-held private keys. Standalone GnuPG/usign signing and verification passed.
Private material is stored only in protected owner-held files and publication secrets.

| File | SHA-256 |
| --- | --- |
| apt.asc | cc1ac352f62047eb693040b876e4c6bf6ca10da93cd8bb6bb130a618c572086d |
| opkg.pub | 478a109a7c2c0d846801e4a4a6ad2873e885add6cbe3d6d7e01f7ff2c9b39aef |
| core-ci-apk.pem | 7cae8895c8af1a543c72f1676865fbd8dbf12b3015f6064a4d288a9aa3fe8f26 |

OpenPGP fingerprint: A4B307A9CFD583089547481DE58C165F4340EE99.
Usign fingerprint: 9bc0ca6b2dc8a9a9.

The existing feed keys/apk.pem is RSA-3072 and remains unchanged for its legacy feeds.
The new publisher requires EC P-256. Its already-provisioned public key is named
core-ci-apk.pem, copied from SNodeC/DistributionPackages commit
7a28f2bd8c21887453c968e095055c601702d4b6 and independently matched to the owner-held
EC private key. New APK feeds are SDK-specific; their clients install this separate key.
No existing key is replaced, and no new private key was generated for this correction.

Signing keys and the feed-writer key are supplied to the privileged publication job through
package-publication, restricted to the trusted worker main branch. Source hooks receive none
of these secrets. GitHub does not permit retrieving stored secret values. Verify identities
with the trusted public files and standard signing tools; never expose private material in
logs or artifacts. End-to-end package/client acceptance remains separate from key verification.
