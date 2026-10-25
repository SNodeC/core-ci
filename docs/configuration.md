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

`registry.sdk-base-url` selects the deployment-owned OpenWrt HTTPS metadata origin.
OpenWrt JSON rows contain target and format, with suite and arch at row level. There is no
source-owned SDK SHA, URL or exact patch release. For each new selection, read .versions.json,
choose the highest stable numeric release in that suite, verify target/architecture/format
from profiles.json, and resolve the SDK through sha256sums. The resulting URL, release and
checksum are recorded only in immutable execution inputs. Workers verify bytes before use;
retries keep their recorded inputs. No compiled release/architecture/runner catalogue exists.
The sandbox origin is https://downloads.openwrt.org, configured outside the checkout.

The registry transport still reuses one named instance between approved endpoints.
The owner deferred refactoring to fixed endpoint clients; that work remains recorded.

## Source requests, dispatch and reports

`github.bot-id` is the immutable controller App bot user ID expected on dispatches.
`github.webhook-secret-file` is a protected file containing the exact raw GitHub webhook
secret bytes (32–4096 bytes); trailing newlines are significant. Configure the same secret
in the App's webhook settings without printing it. A missing secret rejects all deliveries.
The controller App PEM remains server-side and does not become a workflow secret.

`pipeline.worker-repository` selects a configured GitHub grant; its repository ID comes
from that single grant mapping. `pipeline.tooling-sha` is the reviewed full worker commit.
The source/publication workflow path and approved Git blob SHA are configured separately
with `pipeline.source-workflow`, `pipeline.source-workflow-blob`,
`pipeline.publication-workflow`, `pipeline.publication-workflow-blob` and
`pipeline.publication-ref`. Source dispatch uses the enrollment's explicit editing branch.
Workflow code is read at its actual GitHub run commit and checked against its approved
blob; that commit is intentionally independent of the exact selected source commit.

The feed role uses `pipeline.feed-repository`, `pipeline.feed-id`, `pipeline.feed-branch`
and `pipeline.feed-base-url`. It must differ from source and worker roles. Feed-write
access remains with the separate worker App. Public trust inputs are
`pipeline.gpg-fingerprint` (uppercase OpenPGP fingerprint), `pipeline.usign-key-sha256`
and `pipeline.apk-key-sha256` (reviewed public-key file SHA-256). No actual signing private
key belongs in these options. All deployment values are snapshotted into selected inputs.

New enrollment requests the highest SemVer tag when present; a configuration save and
initial-work failure are reported separately. Existing enrollment edits do not rewrite
old work. The UI can request an exact older tag, retry definitively failed work with a
new rebuild revision, or inspect assigned work. Missing setup, incompatible/missing
prerequisites and changed workflows remain explicit errors. Tagless unrelated repositories
do not acquire execution inputs or block independent work.

`POST /github/webhook` handles raw-body HMAC-SHA256 authenticated `push`, `workflow_run`
and `workflow_job` deliveries before browser-origin middleware. It checks installation
and configured repository identity. Non-version/deleted tags and unassigned job reports
create no work. Duplicate events use the existing graph identities. REST inspection
verifies run/attempt/event/actor, approved workflow bytes, assigned package job and bounded
artifact ID/digest metadata; an arbitrary success string cannot publish a node.

The graph transaction authorizes dispatch before the GitHub request. A definitively
rejected or never-submitted dispatch can fail; a timeout, inaccessible result or lost
response preserves the assignment for reconciliation. Startup inspects assigned work once
after listener success, then advances eligible waiting work. There is no startup polling,
mutation replay or automatic retry. Identical results require no database write.

Current limit: assignments up to 60,000 UTF-8 bytes, at most 100 jobs/artifacts per inspected
run, one assigned build artifact up to one GiB, and bounded workflow-run history search.
A successful build records GitHub job/artifact identities; it remains building while waiting
for trusted publication. Signing, feed receipts and the worker tooling remain ongoing work;
these interfaces do not claim a working live deployment.

## Verified feed receipts

The named feed-http instance is registered before init and used only for the configured
feed origin. Publication authorization is committed before dispatch and derived from graph
state: an unresolved writer blocks the same repository/branch. The public feed is excluded
from controller App grants. Public GitHub reads verify its immutable repository ID, receipt
commit and exact regular Git blob through the existing file reader. HTTPS receipt bytes must
match before published state releases dependents. Failed jobs without sufficient feed proof
remain publishing/uncertain; no replacement is dispatched. Explicit inspection exposes the
reason. An assigned origin differing from the configured feed endpoint remains unresolved
until inspected with the proper deployment settings. Existing environment-client endpoint
refactoring remains deferred as instructed.

A latest-SDK row is, for example:

```json
{"id":"openwrt-25.12-x86_64","family":"openwrt","distribution":"openwrt","suite":"25.12","arch":"x86_64","runner":"ubuntu-24.04","build":{"target":"x86/64","format":"apk"}}
```

The suite is configured feed series, not a compiled support list. SDK discovery does not
create a timer or an automatic rebuild policy: it runs when new work is selected.
