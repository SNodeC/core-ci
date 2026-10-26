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

## Fixed ports for direct sandbox testing

The agreed port allocation is:

| Endpoint | Address/port | Router forwarding |
| --- | --- | --- |
| Local configuration UI | 127.0.0.1:8080 HTTP | None |
| SNode.C HTTPS webhook listener (disabled by default) | Machine LAN address:8443 TCP | External TCP 8443 to this machine TCP 8443 |
| Public GitHub webhook | https://gw-home.vchrist.at:8443/github/webhook | Public HTTPS port 8443 |

On 2026-10-25 the machine's active IPv4 route used LAN address 192.168.1.239; ports 443,
8080 and 8443 had no local listeners. Reserve the machine's LAN address in the router's DHCP
settings, or use its actual stable LAN address as the forwarding destination. Do not
forward 8080 or MariaDB. No UDP forwarding is needed for GitHub HTTPS webhooks.

The application has separate named `web` HTTP and `webhook` TLS instances. Only the TLS
instance owns `/github/webhook`; it exposes no UI or administration API. It defaults to
disabled and loopback:8443. Both listeners start only after initial database/schema success.
A TLS bind failure is logged while the local UI/database remain available; `/api/status`
reports database health, not proof that webhooks are reachable.
No reverse proxy was found installed during this check; one is not assumed or installed.
The owner selected gw-home.vchrist.at on public HTTPS port 8443 because public port 443
is occupied. It resolved to 212.241.96.69 during setup. A temporary nonce-response listener
verified that the public address on 8443 forwarded to this machine from inside the LAN;
that listener was closed. This does not yet prove external GitHub delivery or valid TLS.
Configure the actual TLS listener port through SNode.C's registered command-line options.
The existing port-443 service, which presented an office.vchrist.at certificate, stays
untouched. Valid certificate/private-key paths for gw-home.vchrist.at remain required.
Keep TLS key material in protected local configuration, never source JSON or tracked files.
Do not disable GitHub's certificate verification to bypass missing TLS setup.

## Signing keys belong to the trusted worker

Package signing private keys are GitHub Actions secrets of the trusted worker's protected
`package-publication` environment, outside controller configuration and MariaDB:

| Secret | Material |
| --- | --- |
| APT_SIGNING_KEY | Armored OpenPGP archive/RPM signing key |
| PACKAGES_GPG_PASSPHRASE | Only if that key is passphrase protected |
| OPENWRT_USIGN_KEY | IPK/opkg index signing key |
| OPENWRT_APK_KEY | EC P-256 PEM for OpenWrt APK v3 |
| PACKAGES_APP_PRIVATE_KEY | Separate App key with feed-only write access |

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

## Build environments belong to GitHub

The controller has no registry or SDK client. Remove obsolete `registry.*` options and
`environment-http` instance settings from controller configuration. Repository JSON keeps
its image/platform or OpenWrt suite/arch/target/format inputs unchanged.

The trusted native worker uses standard Docker pull/inspect on GitHub to resolve the image,
check its actual platform, and build with its digest-qualified reference. It records that
reference and the configuration digest in the artifact manifest. The trusted publisher
validates agreement with the assigned row and records the environment in its verified
publication receipt; MariaDB retains that receipt as execution evidence.

OpenWrt metadata discovery is worker code. Public worker variable
`OPENWRT_DOWNLOAD_BASE_URL` defaults to the official `https://downloads.openwrt.org`
origin. Roots select the latest stable numeric patch in the configured series; dependents
reuse the SDK/toolchain identities in all selected prerequisite receipts. Incompatible joins
fail explicitly. The worker checks profiles.json and records the official SDK URL/checksum.
Repository owners maintain no SDK SHA. SDK execution, signing and real client installation are
verified in the sandbox; see [acceptance evidence](sandbox-deployment.md). Resolved checksums
currently trust the configured HTTPS metadata origin; signed checksum-list verification is not implemented.
`OPENWRT_WORKER_IMAGE` (default ubuntu:24.04) and `OPENWRT_WORKER_PLATFORM` (default linux/amd64)
select the GitHub-side SDK host container, resolved to an immutable identity before hooks.

GitHub records a separate `<assigned artifact name>-environment` artifact before source
hooks run. It contains the assignment hash and resolved environment identities, with
90-day requested retention, subject to repository/organization limits:
[GitHub artifact retention](https://docs.github.com/en/enterprise-cloud%40latest/actions/tutorials/store-and-share-data). The source and
reusable workflows request only contents/read and actions/read; the Actions token is
passed only to the environment step and never to the source hook. The caller must grant
these permissions because a reusable workflow cannot elevate them:
[GitHub reusable-workflow permissions](https://docs.github.com/en/actions/reference/workflows-and-actions/reusing-workflow-configurations).

On explicit retry, the assignment references earlier assigned runs, their assignment hashes
and environment artifact names. The worker checks the same approved run/workflow identity
used for package validation, verifies downloaded bytes, and reuses the recorded environment.
A prior attempt without evidence is skipped only when GitHub proves the hook step was
skipped and environment recording did not succeed. Missing, expired or inaccessible evidence
after a possible build stops the retry clearly; it never permits mutable re-resolution.
Prerequisite receipts also remain the originally assigned inputs. No new controller state,
registry client or scheduling queue is introduced. This path has local fixture coverage;
real GitHub execution and full package pipeline acceptance remain outstanding.

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
for trusted publication. Signing and Git/HTTPS receipt verification operate in the sandbox;
format-specific live acceptance is recorded in [sandbox status](sandbox-deployment.md).

## Verified feed receipts

The named feed-http instance is registered before init and used only for the configured
feed origin. Publication authorization is committed before dispatch and derived from graph
state: an unresolved writer blocks the same repository/branch. The public feed is excluded
from controller App grants. Explicit public reads use an existing granted repository
contents:read installation token, without adding feed access grants or anonymous quota use.
Ordinary source reads/writes still require explicit repository grants. Reads verify feed ID, receipt
commit and exact regular Git blob through the existing file reader. HTTPS receipt bytes must
match before published state releases dependents. Failed jobs without sufficient feed proof
remain publishing/uncertain; no replacement is dispatched. Explicit inspection exposes the
reason. An assigned origin differing from the configured feed endpoint remains unresolved
until inspected with the proper deployment settings. The environment client has been removed.

A latest-SDK row is, for example:

```json
{"id":"openwrt-25.12-x86_64","family":"openwrt","distribution":"openwrt","suite":"25.12","arch":"x86_64","runner":"ubuntu-24.04","build":{"target":"x86/64","format":"apk"}}
```

The suite is configured feed series, not a compiled support list. SDK discovery does not
create a timer or an automatic rebuild policy: it runs in the GitHub worker for assigned work.

The sandbox preserves the existing feed APT/usign identities. New SDK-specific APK feeds
publish their EC P-256 public key as keys/core-ci-apk.pem; legacy keys/apk.pem remains RSA
and unchanged. Install the key corresponding to the chosen feed. Do not substitute the
legacy RSA file for the new APK publisher's EC P-256 key.


## Enabling the dedicated TLS listener

After the owner supplies a trusted certificate chain and matching protected private-key
file for `gw-home.vchrist.at`, append this CLI fragment to the configured controller command.
Set the two shell variables to file paths, not key contents:

```sh
webhook --disabled=false local --host 192.168.1.239 --port 8443 \
  tls --cert "${CORE_CI_TLS_CERTIFICATE:?certificate chain file required}" \
      --cert-key "${CORE_CI_TLS_PRIVATE_KEY:?private-key file required}"
```

Use the actual stable LAN address. Port selection remains on the command line. Keep the
`web` UI instance on `127.0.0.1:8080`; the TLS instance has no configuration-writing routes.
If the TLS key is encrypted, supply `webhook.tls.cert-key-password` only through the
owner-only server configuration file, never shell arguments or source JSON.

Framework help suppresses descendants of disabled instances. These commands expose the
registered TLS and parser configuration without starting a database or listener:

```sh
core-ci webhook --disabled=false --help=expanded
core-ci webhook --disabled=false http parser --help
```

Only after a trusted external TLS connection is proved should the GitHub App webhook be
activated at `https://gw-home.vchrist.at:8443/github/webhook`, with SSL verification enabled
and the configured raw-body HMAC secret. Public activation is not implied by local tests
using a generated test certificate. No public certificate/key paths have been supplied yet.


## Execution evidence and supported runners

The controller reads the assigned GitHub run's attempt-one result and jobs explicitly.
A later GitHub rerun cannot replace that evidence; a rerun process is refused before build
resolution or protected publication. Explicit controller retry creates its own package revision.
Oversized dispatch inputs remain visible blockers; another target may advance independently.
The full serialized inputs object is conservatively bounded by UTF-8 bytes, including JSON
escaping and publication evidence, within GitHub's character limit. Worker assignment JSON
also retains its 60,000-byte bound. No persistent blocker/ready queue is added.

Workflow blob and tooling pins must match the single rendered thin template. Builds run on
source-selected runner labels; only trusted, disposable GitHub-hosted or equivalently isolated
runners support the stated provenance boundary. A source-controlled/self-hosted runner can
alter host tooling, credentials and evidence; labels alone do not establish trust.

## Active sandbox webhook (2026-10-26)

The sandbox App now delivers signed push, workflow_run and workflow_job events to
https://gw-home.vchrist.at:8443/github/webhook with certificate verification enabled.
Core-ci uses /home/voc/ci-test/core-ci-tls/fullchain.pem and the matching protected
private-key.pem; its HMAC secret file is /home/voc/ci-test/core-ci-webhook.secret.
Only the webhook route is public; administration remains on loopback port8080.
These are sandbox deployment paths, not compiled-in application defaults.

Validated work notifications receive202 before asynchronous GitHub verification finishes.
Processing outcomes are logged as GitHub webhook completed; failures do not authorize
replacement executions. A crash before a new tag selection is persisted still needs an
explicit rescan/redelivery. No durable delivery inbox or automatic replay was added.
The router renews its ACME certificate; automatic transfer of renewed files and controller
restart is not configured yet. Do not confuse renewal on the router with controller reload.
