# Configuration and key ownership

SNode.C reads deployment configuration through its registered `--config-file` option.
Use the framework parser, help and `--write-config` support; core-ci has no second
configuration parser. The committed [template](../config/controller.conf.example)
contains no deployment IDs or credentials and is not a runnable deployment.

Keep the actual file outside the checkout, owned by the service account with mode 0600.
GitHub App and installation IDs, allowed owner, and repository name/ID grants belong there.
`github.repository` is a list of `owner/repository=numeric-ID` entries. Token requests use
`repository_ids` and the permission subset needed by that operation. Reading repository
metadata checks the returned ID against the configured grant. Enrollment persists the
verified identity as graph evidence; it cannot authorize an additional repository.

`github.private-key-file` references the controller's RSA PEM. `database.password-file`
references the database password. Both must be owned regular files, not symlinks, without
group/other permissions. The PEM is limited to 64 KiB and the password to 4096 bytes.
An encrypted PEM is rejected without an interactive prompt. Private material is loaded
before event handlers and is never returned to the browser or persisted in MariaDB.

Launch with `core-ci --config-file /absolute/settings.conf database --mode unmanaged`.
The database section's user, name, endpoint and password-file must be supplied in that
file. Ownership mode stays explicit on the CLI so a default config cannot accidentally
start a server. Managed mode additionally requires the registered managed-server options.
Use root `--help=expanded` and named instance `web --help=expanded` to inspect all options.

The browser's origin and Host must match `ui.origin` for JSON configuration writes.
Development listens on loopback. HTTPS proxy deployment must retain the configured Host;
public access protection must be agreed before exposing this configuration-writing UI.
There is no user identity provider or login system.

## Signing keys belong to the trusted worker

Package signing private keys are GitHub Actions secrets of the trusted worker's protected
`package-publication` environment, outside controller configuration and MariaDB:

| Secret | Material |
| --- | --- |
| PACKAGES_GPG_PRIVATE_KEY | Armored OpenPGP archive/RPM signing key |
| PACKAGES_GPG_PASSPHRASE | Only if that key is passphrase protected |
| OPENWRT_USIGN_PRIVATE_KEY | IPK/opkg index signing key |
| OPENWRT_APK_PRIVATE_KEY | EC P-256 PEM for OpenWrt APK v3 |
| FEED_WRITER_APP_PRIVATE_KEY | Separate App key with feed-only write access |

Organization secrets must be restricted to the trusted worker repository; environment
secrets permit the tighter publication-job boundary. Source builds do not reference that
environment. Public fingerprints, verification keys, feed URLs, workflow revisions and
other deployment settings are configuration, not secret material. Review public keys in
the worker's `keys/` directory and publish them with the feed. File-input provisioning
commands are in [setup inputs](setup-inputs.md). No signing key is assumed to exist and
none is generated, replaced or rotated by the controller.

## Fresh MariaDB schema

An administrator creates the selected empty schema/account and applies `sql/schema.sql`
while core-ci is stopped. Runtime permissions are SELECT, INSERT, UPDATE and DELETE on
that schema only. The application performs no DDL. Startup checks schema version and
validates enrolled definitions before opening the listener. A missing/inconsistent schema
leaves no listener. This is fresh setup, with no legacy import or migration.

GitHub owns build definitions. MariaDB retains each committed definition revision and
its enrollment pointer; there is no independently editable local matrix. The enrollment
UI can import committed `ci/packages.json` or commit an explicitly edited definition.
Stale effective revisions and changed file blobs are rejected; unrelated branch commits
do not prevent an edit. Invalid committed JSON can be read and repaired explicitly. A lost GitHub write response is shown as uncertain,
not retried automatically; reading the committed definition reconciles enrollment.
A successful GitHub commit followed by database failure is reported with its revision.

The database owner serializes entire transactions on its asynchronous connection.
An uncertain COMMIT or failed rollback prevents subsequent mutations until restart and
reconciliation; no operation is blindly replayed. Reads may re-establish a lost connection
through the framework. Backup and restore guidance is in [setup inputs](setup-inputs.md).

Current implementation status and remaining pipeline work are recorded in the README
and work log. These configuration instructions are not deployment/acceptance evidence.

## Immutable build environments

`registry.default-registry` supplies the HTTPS host[:port] for unqualified image names;
`registry.default-namespace` optionally supplies the namespace for single-component names.
There are no compiled registry aliases. Use an explicit registry authority in an image
name when it differs from that default. Native images require an explicit tag or SHA-256
digest and a literal `linux/architecture[/variant]` platform. Architecture names are not
translated from package architecture names.

Repeat `registry.allowed-origin` for every approved HTTPS registry, pull-token service
and configuration-document redirect origin. Values contain only scheme and authority.
Anonymous registry access and standard scoped Bearer pull challenges are supported;
private registry credentials and other auth schemes are currently rejected clearly.
Redirects are bounded and cannot send a registry token to another origin. IPv6 URL
literals are currently unsupported; DNS and IPv4 endpoints are accepted.

The controller reads bounded OCI/Docker schema-2 indexes, manifests and configuration
blobs. It verifies SHA-256 and descriptor sizes, selects an unambiguous platform, checks
the image configuration, and records the original image/platform, root and selected
manifest digests, configuration digest and canonical digest-qualified image. It ignores
optional embedded data/alternative descriptor URLs and fetches canonical registry paths.
It never pulls filesystem layers or executes a container locally.

`registry.sdk-origin` separately approves exact SDK download origins. Selection retains
the source definition's exact SDK URL/checksum; GitHub workers must download, verify and
validate release/toolchain compatibility before executing any hook. Pinning an SDK here
is not approval to execute its tooling beside publication secrets.

The registry transport still reuses one named instance between approved endpoints.
The owner deferred refactoring to fixed endpoint clients; that work remains recorded.
