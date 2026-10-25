# Trusted public verification keys

These public files were read from SNodeC/DistributionPackages at commit
`7a28f2bd8c21887453c968e095055c601702d4b6`. No private key was copied.
The sandbox publisher must verify private/public correspondence before signing.

| File | SHA-256 |
| --- | --- |
| apt.asc | 746217db57c5a8667b9f827c41bf933b4972e62463555e4bf95b332c09c8ae39 |
| opkg.pub | 8d31ae50a78ee2cd6935ec17e8e55fd3de7ccc787f66ef84e215889c4df208e6 |
| apk.pem | 7cae8895c8af1a543c72f1676865fbd8dbf12b3015f6064a4d288a9aa3fe8f26 |

OpenPGP fingerprint: `8BBFD49E3C826FDB1416C79E60046744B15B0E05`.
`apk.pem` is EC P-256, verified with OpenSSL. The usign public file has not yet
been exercised with a signature/client locally; do not infer that from its name.

Read-only GitHub metadata on 2026-10-25 confirmed repository secrets named
APT_SIGNING_KEY, OPENWRT_USIGN_KEY, OPENWRT_APK_KEY, PACKAGES_APP_PRIVATE_KEY
and PACKAGES_OBSERVER_TOKEN in both VOC-Experiments/ci-coordinator and
SNodeC/DistributionPackages. Values cannot be retrieved and have not been inspected.
Names do not prove correspondence with these keys. Neither repository currently
has a publication environment. No secret was moved, replaced or rotated.

Only the privileged publication job may receive signing/feed credentials. Creating
an environment does not remove access to existing repository-level secrets. Before
activation, review worker workflows and restrict signing keys to the protected
package-publication environment using the existing owner-held material. Do not
export secret values through workflow logs or artifacts to accomplish that setup.
