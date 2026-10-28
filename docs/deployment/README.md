# Deployment and operations

[Documentation guide](../README.md) · [Architecture](../architecture/README.md) ·
[Extension guide](../extensions/README.md)

This guide deploys the controller **and** the GitHub package pipeline. Building a controller
binary, uploading an Actions artifact and publishing a signed package feed are different
steps. Work through them in order; each has its own credentials and verification boundary.

Commands using `OWNER`, repository names or `/secure/...` paths are setup templates. Supply
real owner-approved values before running them. They are not evidence of a configured system.
The [sandbox README](../../deployment/sandbox/README.md) records the actual current deployment.

## Contents

- [1. Decide the deployment roles](#1-decide-the-deployment-roles)
- [2. Build and place the controller](#2-build-and-place-the-controller)
- [3. Prepare MariaDB before application startup](#3-prepare-mariadb-before-application-startup)
- [4. Configure the controller App](#4-configure-the-controller-app)
- [5. Install trusted worker and source workflows](#5-install-trusted-worker-and-source-workflows)
- [6. Provision publication keys and least-privilege access](#6-provision-publication-keys-and-least-privilege-access)
- [7. Make a real HTTPS package feed available](#7-make-a-real-https-package-feed-available)
- [8. Enable public HTTPS webhooks without exposing the UI](#8-enable-public-https-webhooks-without-exposing-the-ui)
- [9. Enroll projects and follow a first publication](#9-enroll-projects-and-follow-a-first-publication)
- [10. Configure and verify package clients](#10-configure-and-verify-package-clients)
- [11. Operate, upgrade and recover](#11-operate-upgrade-and-recover)

## 1. Decide the deployment roles

Choose four locations:

1. The application checkout for controller/UI and deployment sources.
2. Enrolled source repositories for project definitions, source and hooks.
3. One trusted worker repository for reusable builders, validators, publisher and public keys.
4. A feed repository/branch and an actual package-compatible public HTTPS base URL.

The controller machine needs SNode.C, MariaDB access and outbound GitHub/HTTPS access. It does
not need a local SDK build farm, Docker daemon, package signer or repository indexer. Those
run on GitHub. Two controller CPU cores do not limit GitHub runner capacity.

Choose source editing branches, the publisher dispatch ref, exact trusted tooling commit,
approved workflow Git blob identities, controller App grants, feed-writer grants and trusted
key identities. Keep these deployment-wide settings out of source `ci/packages.json`.

## 2. Build and place the controller

Install a C++20 toolchain, CMake 3.25+, Ninja, OpenSSL/nlohmann-json development packages and
installed public SNode.C packages providing:

```text
db-mariadb
http-server-express-legacy-in
http-server-express-tls-in
http-client
net-in-stream-tls
```

Then build from the canonical checkout:

```sh
cmake -S . -B build/codex/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/codex/release -j2
cmake --build build/codex/release --target format-check
```

Persistent build directories belong under `build/codex/CONFIGURATION`; temporary checks belong
under `/tmp/codex/build/core-ci/TASK`. Use the committed formatter configuration. A successful
build does not install or start the application. CMake currently has no application install
rule; an administrator may place the built executable explicitly, for example:

```sh
sudo install -d -m 0755 /srv/core-ci/bin
sudo install -m 0755 build/codex/release/core-ci /srv/core-ci/bin/core-ci
```

Run the service as a dedicated non-root account. Configuration, private files and any managed
MariaDB datadir must be accessible to that account and protected from other users. A supervisor
can launch the same explicit command used interactively; do not introduce another application
startup loop. The sandbox currently runs a development binary, not an installed system service.

## 3. Prepare MariaDB before application startup

### Unmanaged mode

Use this when an administrator owns an existing MariaDB service. core-ci never starts, stops
or reconfigures that server. Prefer a local Unix socket for a controller on the same machine.
Create a fresh application schema and restricted runtime account while core-ci is stopped.

For a matching Linux service account named `core_ci`, an administrator can use MariaDB's
installed Unix-socket authentication plugin:

```sql
CREATE DATABASE core_ci CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
CREATE USER 'core_ci'@'localhost' IDENTIFIED VIA unix_socket;
GRANT SELECT, UPDATE ON core_ci.* TO 'core_ci'@'localhost';
```

Apply the schema as an administrative account:

```sh
mariadb --defaults-extra-file=/secure/database-admin.cnf core_ci < sql/schema.sql
```

Do not apply the fresh schema over an existing deployment. The runtime account performs no
DDL and needs no CREATE, ALTER or administrative server privilege. If password authentication
is required, provision it through a protected administrative channel and put the password in
an owner-only regular file. Do not put it in command arguments. An explicitly empty password
file selects an empty password for a separately configured socket-authenticated account;
it does not grant socket authentication by itself.

Set `database.socket` (or explicit `database.host`/`database.port`), `database.user`,
`database.name` and `database.password-file` in the protected settings file. Start with:

```sh
/srv/core-ci/bin/core-ci --config-file /secure/controller.conf database --mode unmanaged
```

### Managed mode

Use a separate private MariaDB instance, not the administrator's shared server. Pre-provision
its datadir, schema/account and separate server configuration. The datadir must belong to the
controller account and be private. Socket/PID paths must be directly inside that datadir and
absent when startup begins.

```sh
/srv/core-ci/bin/core-ci --config-file /secure/controller.conf \
  database --mode managed \
  --socket /srv/core-ci/database/server.sock \
  --managed-executable /usr/sbin/mariadbd \
  --managed-config-file /secure/mariadb.cnf \
  --managed-datadir /srv/core-ci/database \
  --managed-pid-file /srv/core-ci/database/server.pid
```

The private server config supplies its process-user/log/filesystem requirements. The controller
starts its owned child with networking disabled, waits for the native readiness notification,
connects explicitly once and owns that child's shutdown. It does not initialize the schema or
poll for startup. The mode is deliberately CLI-only; a default settings file cannot silently
change database ownership.

Initial connection/schema restoration failure leaves no application listener. Later lost
connections use the framework's existing lazy establishment. Unknown mutations are never
replayed as a connection-recovery strategy.

## 4. Configure the controller App

Create/install a GitHub App for the controller. Grant only the enrolled source repositories
and trusted worker repository. It needs repository metadata read, contents read/write for
source/configuration reads and approved commits, Actions read/write for dispatch/evidence,
and workflow-file write access for the explicit thin-workflow installation action.
The App does not need the feed writer's credential.

### App creation and installation, step by step

1. Open the owning account or organization's **Settings → Developer settings → GitHub Apps**
   and create a new App with a recognizable controller name. A GitHub App is the controller's
   machine identity; it does not introduce a user-login system into core-ci.
2. Configure repository permissions: **Contents: Read and write**, **Actions: Read and write**,
   and **Workflows: Read and write** for the authorized workflow-file editing operation.
   Metadata read access accompanies the repository grant. The write permissions are needed
   for explicit configuration/workflow commits and dispatch; build jobs do not inherit them.
3. After the relevant permissions are selected, subscribe to Push, Workflow run and Workflow
   job. If the webhook endpoint is not ready, keep delivery inactive until section 8 is done.
4. Create the App, record its App ID, and generate its controller private key. Move the
   downloaded PEM to the protected server-side path, preserving an owner-held recovery copy.
   Do not put it in an Actions source repository or paste it into a JSON definition.
5. Choose **Install App**, select the authorized organization, and use **Only select
   repositories**. Select the enrolled source repositories and worker repository. The separate
   writer App handles the feed repository. Installation grants and App permissions are
   separate restrictions; both must allow an operation.
6. Record the installation ID from its installation page/API. Read the controller bot's numeric
   user ID and each repository's numeric ID through GitHub's API. Configure the explicit
   `owner/repository=ID` grants; spelling a repository name alone is insufficient.
7. Keep the App settings URL and an owner able to administer its installation available for
   future permission/repository changes. A new enrollment outside these grants requires an
   intentional administrator change first.

The controller obtains short-lived installation tokens through the App's private key and
GitHub's REST API. Operators may use `gh` during setup; the running controller uses SNode.C
HTTP clients. GitHub's [App creation guide](https://docs.github.com/en/apps/creating-github-apps/registering-a-github-app/registering-a-github-app)
and [installation guide](https://docs.github.com/en/apps/using-github-apps/installing-your-own-github-app)
explain the GitHub settings screens.

Record App ID, installation ID, bot user ID and numeric repository IDs. Protect the App's RSA
PEM on the controller machine. Tokens are minted for the configured installation, repository
ID subset and operation permissions; changing a display name in the UI cannot broaden access.

Use [config/controller.conf.example](../../config/controller.conf.example) as the actual
option-name reference. These are the registered controller settings:

| Setting | Purpose |
| --- | --- |
| `github.app-id` | Controller App numeric ID |
| `github.installation-id` | Authorized installation |
| `github.bot-id` | Expected dispatch actor |
| `github.allowed-owner` | Authorized owner boundary |
| `github.repository` | List of `owner/repository=numeric-ID` grants |
| `github.private-key-file` | Protected App RSA PEM path |
| `github.webhook-secret-file` | Protected raw-body HMAC secret path |
| `github.api-version` | Configured GitHub REST version |
| `pipeline.worker-repository` / `pipeline.tooling-sha` | Trusted worker and full tooling commit |
| `pipeline.source-workflow` / `pipeline.source-workflow-blob` | Thin workflow path and approved Git blob |
| `pipeline.publication-workflow` / `pipeline.publication-workflow-blob` | Publisher path and approved Git blob |
| `pipeline.publication-ref` | Publisher dispatch ref |
| `pipeline.feed-repository` / `pipeline.feed-id` | Feed name and stable numeric identity |
| `pipeline.feed-branch` / `pipeline.feed-base-url` | Checked-update branch and public package root |
| `pipeline.gpg-fingerprint` | Trusted OpenPGP fingerprint |
| `pipeline.usign-key-sha256` / `pipeline.apk-key-sha256` | Trusted public-key file hashes |

Actual settings belong outside the checkout in an account-owned mode-0600 file. The App PEM
must be an owned regular nonsymlink file, at most 64 KiB; an interactive encrypted PEM is not
supported. Password/HMAC files are bounded and protected similarly; the HMAC secret needs at
least 32 bytes. Never place private material in MariaDB, source JSON, browser code or logs.

Inspect the complete framework configuration with:

```sh
/srv/core-ci/bin/core-ci --help=expanded
/srv/core-ci/bin/core-ci web --help=expanded
/srv/core-ci/bin/core-ci webhook --disabled=false --help=expanded
```

## 5. Install trusted worker and source workflows

### Worker repository

For a first installation, commit the contents of [deployment/worker](../../deployment/worker) at the trusted repository
root: `core_ci/`, `status-templates/`, `.github/workflows/` and reviewed `keys/`.
Preserve unrelated repository content. Record the resulting full tooling commit SHA.
The publisher and status YAML are installation templates: replace `@TOOLING_SHA@` with that
reviewed commit in a subsequent workflow commit. Their workflow-local `CORE_CI_TOOLING_SHA`
and checkout select that exact tooling revision, independently of the selected source.
Never install a template with an unresolved marker. Keep the repository variable for any
retained older workflows that still use it; changing it would invalidate their exact retries.

The publication workflow installs the pinned Jinja2 dependencies before the signing step.
Review the deployment-owned templates, public `presentation.json` parameters and any declared
assets together with that tooling revision. Source hooks cannot provide templates. Rendering
runs after temporary signing files are removed and before the checked feed push. See the
[template setup and context](../../deployment/worker/status-templates/README.md).

The worker's public variables are:

```text
CORE_CI_TOOLING_SHA
CONTROLLER_APP_BOT_ID
FEED_REPOSITORY
FEED_REPOSITORY_ID
FEED_BRANCH
FEED_BASE_URL
PACKAGES_GPG_FINGERPRINT
OPENWRT_DOWNLOAD_BASE_URL
FEED_WRITER_APP_ID
FEED_WRITER_INSTALLATION_ID
```

`OPENWRT_DOWNLOAD_BASE_URL` defaults to `https://downloads.openwrt.org`. The feed-writer IDs
may be protected publication-environment variables; validation/build jobs do not need the
private feed-writer key. Other trusted identity/feed variables must be available in the jobs
that validate them. Public identifiers are variables, not secret values.

### Source repositories

Render the thin workflow using the reviewed worker commit:

```sh
python3 deployment/render-source-workflow.py \
  --worker OWNER/WORKER \
  --tooling-sha FULL_REVIEWED_COMMIT_SHA \
  --output /tmp/codex/build/core-ci/setup/core-ci-build.yml
```

Create the output directory first. Review the generated bytes and the printed Git blob SHA.
Configure `pipeline.source-workflow-blob` and record the publication workflow's Git blob SHA
in `pipeline.publication-workflow-blob`. The controller also checks that its rendered thin
workflow matches the approved blob; inconsistent pins fail before selection.

Commit the rendered file as `.github/workflows/core-ci-build.yml` in each authorized source
repository, or use the controller's explicit preview/commit UI. The thin workflow has read-only
permissions, accepts the exact assignment and calls the trusted reusable workflow at a full
commit. It has no signing keys, feed writer or controller PEM.

Workflow ref and source commit are different values. Do not replace the worker's explicit
selected-source checkout with a checkout of the workflow branch.

## 6. Provision publication keys and least-privilege access

Create the trusted worker environment **`package-publication`** and restrict deployment to
the reviewed publisher branch/ref. Protect who can edit trusted workflows and tooling.
Create a separate feed-writer App with contents-write access restricted to the configured
feed repository. Its private key is used only by the publisher push step to mint a feed-only
token; do not install it in source repositories or the controller database.

Provide these exact secret names:

| Secret | Location and use |
| --- | --- |
| `APT_SIGNING_KEY` | Publication environment; armored OpenPGP private key for APT/RPM |
| `PACKAGES_GPG_PASSPHRASE` | Publication environment, only if that key is passphrase protected |
| `OPENWRT_USIGN_KEY` | Publication environment; private usign key for IPK indexes |
| `OPENWRT_APK_KEY` | Publication environment; EC P-256 PEM for OpenWrt APK v3 |
| `PACKAGES_APP_PRIVATE_KEY` | Publication environment; feed-only writer App PEM |
| `PACKAGES_OBSERVER_TOKEN` | Worker validation job; cross-source Actions/artifact and workflow-content read access |

The observer credential's actual type and minimum grants must be reviewed by the owner. Its
name does not prove it is narrowly scoped. The controller App private key is not a substitute.
Repository-scoped or organization-scoped secrets must not expose publication authority to
untrusted workflows; environment names alone do not restrict a repository-level secret.

Keep the corresponding trusted public files in worker `keys/` and publish them with the feed.
For the selected APK contract, use an EC P-256 key and the matching OpenWrt APK v3 tooling.
The current files/fingerprints are documented in [keys/README.md](../../deployment/worker/keys/README.md).
Verify imported private/public correspondence before activation. Never obtain public-key
authority from a build artifact, generate replacement signing keys per build, or rotate an
existing feed key implicitly.

Provision from owner-held files, without putting secret contents in shell arguments:

```sh
gh secret set APT_SIGNING_KEY --repo OWNER/WORKER --env package-publication < /secure/archive-private.asc
gh secret set PACKAGES_GPG_PASSPHRASE --repo OWNER/WORKER --env package-publication < /secure/archive-passphrase
gh secret set OPENWRT_USIGN_KEY --repo OWNER/WORKER --env package-publication < /secure/usign-private.key
gh secret set OPENWRT_APK_KEY --repo OWNER/WORKER --env package-publication < /secure/apk-private.pem
gh secret set PACKAGES_APP_PRIVATE_KEY --repo OWNER/WORKER --env package-publication < /secure/feed-writer.pem
gh secret set PACKAGES_OBSERVER_TOKEN --repo OWNER/WORKER < /secure/observer-token
```

Omit the optional passphrase secret when unnecessary. GitHub does not return stored secret
values: maintain protected owner-held recovery copies. These file-input commands follow
[GitHub's secret documentation](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-secrets).
The publisher imports keys into temporary restrictive files/keyrings and removes them on
completion/failure. They must not enter caches, artifacts or logs.

## 7. Make a real HTTPS package feed available

Configure the feed branch and actual public root before dispatching publication. For Pages,
install [deployment/feed/.github/workflows/pages.yml](../../deployment/feed/.github/workflows/pages.yml)
as the feed's Pages workflow and configure Actions-based Pages deployment. Select the actual
branch in that workflow. Protect feed writes so unrelated credentials cannot bypass the writer.

The deterministic layout is:

```text
keys/apt.asc
keys/opkg.pub
keys/core-ci-apk.pem
apt/DISTRIBUTION/SUITE/pool/NAMESPACE/...
apt/DISTRIBUTION/SUITE/dists/SUITE/main/binary-ARCH/...
rpm/DISTRIBUTION/SUITE/ARCH/...
openwrt/SUITE/ARCH/FORMAT/SDK_IDENTITY/...
exports/EXECUTION/REVISION/development.tar.gz
receipts/EXECUTION/REVISION.json
status.json
index.html
status/index.html
status/README.md
status/REPOSITORY.md
```

A repository literally named `README` uses `status/README/README.md` for its own page. Public
status is derived from retained receipts; it describes publications, not live controller work.

APT indexes come from actual DEB controls. Release contains index checksums and is signed as
InRelease and Release.gpg. RPM packages are signed with rpmsign; createrepo_c creates metadata
and repomd.xml is signed. IPK Packages includes metadata/checksums, Packages.gz is generated,
and the uncompressed index is signed with usign as Packages.sig. OpenWrt APK uses the selected
SDK's `apk adbsign`, `mkndx --sign` and verification facilities to produce `packages.adb`.
It does not generate an Alpine APKINDEX.tar.gz v2 substitute.

Packages, affected indexes, receipt/export and status projections are committed together.
Normal checked updates preserve unrelated paths and retained package identities. A stale
push is rejected rather than force-pushed or rebased over another publisher.

The worker enforces a conservative 900 MiB site bound. GitHub Pages limits published sites
to 1 GB, has a soft 100 GB/month bandwidth limit and a ten-minute deployment timeout.
Review the current [Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits)
for your usage. Capacity exhaustion is a deployment decision, not permission to delete retained
versions or silently introduce another hosting service.

## 8. Enable public HTTPS webhooks without exposing the UI

Keep `web` on `127.0.0.1:8080`. Both `localhost` and `127.0.0.1` can be used in the browser.
Configuration POSTs require JSON; the controller does not restrict their Origin or Host headers.
There is no application user-login protection; agree an access-protection
arrangement before making the UI public. Do not forward its port as part of webhook setup.

The named `webhook` TLS listener owns only `/github/webhook`. Append:

```sh
webhook --disabled=false local --host YOUR_STABLE_LAN_ADDRESS --port 8443 \
  tls --cert /secure/tls/fullchain.pem --cert-key /secure/tls/private-key.pem
```

These are framework CLI settings, not a custom listener configuration parser. Use a trusted
full chain and matching private key, accessible only as required by the service account.
An encrypted TLS key needs its registered password setting in protected configuration.

For the sandbox, forward router TCP 8443 to controller TCP 8443 and use
`https://gw-home.vchrist.at:8443/github/webhook`. Leave occupied port 443, UI 8080 and
MariaDB untouched. Ensure public DNS points to the router and external GitHub delivery can
reach the forwarding destination. A successful connection from inside the LAN alone does
not prove public reachability; verify a delivery from GitHub.

### Obtain a certificate on an OpenWrt router

A private LAN server can use a public certificate for its public hostname. DNS-01 proves
ownership through DNS and needs no inbound port 80/443 for issuance. HTTP-01 instead requires
public port 80; it cannot validate on 8443. See [Let's Encrypt challenge types](https://letsencrypt.org/docs/challenge-types/).

In LuCI, install `luci-app-acme` and the DNS API package for the installed OpenWrt generation:
older releases use `acme-dnsapi`; newer package layouts use `acme-acmesh-dnsapi`. Update package
lists and inspect the release's available packages rather than mixing repositories.

Create an enabled certificate for the public hostname, disable staging for a trusted final
certificate and choose DNS validation. For standard IONOS domain hosting the API is
`dns_ionos`. Add two separate credential entries, using full values from the IONOS portal:

```text
IONOS_PREFIX=your-private-prefix
IONOS_SECRET=your-private-secret
```

Keep alias fields empty unless intentionally using delegated challenge DNS. Do not share
credential screenshots. Start/restart the ACME service and inspect its logs. A missing DNS hook
means the provider scripts are absent; it is not proof of bad credentials.

For the sandbox router, issued files are `/etc/acme/gw-home.vchrist.at/fullchain.cer` and
`/etc/acme/gw-home.vchrist.at/gw-home.vchrist.at.key`. Transfer them over authenticated SSH
into restrictive local files; never print or commit the private key. Check hostname, validity,
chain trust and matching public keys before activating the listener.

Renewal on the router does not reload the controller. The operator must securely deploy
renewed files and restart the controller; automatic renewal transfer/restart is not currently
configured in the sandbox. Monitor expiry and establish the desired operational automation
before relying on unattended long-term service.

### Configure and verify the App webhook

Set the App's webhook URL, a high-entropy signing secret that exactly matches the controller's
secret file, Active and SSL verification. Subscribe to **Push**, **Workflow run** and
**Workflow job**. No user identity/OAuth callback is required for this pipeline.

Verify these separately:

- A signed GitHub ping succeeds.
- An unsigned or modified-body notification is rejected with `401`.
- The public TLS listener does not expose `/` or `/api/graph`.
- A new sandbox version tag creates work without an operator reconciliation call.
- Completed GitHub jobs progress through publication and controller verification.
- Duplicate deliveries and restart during assigned work do not create replacements.

Accepted work notifications receive `202` promptly. Their eventual processing status/result is
in `GitHub webhook completed` logs. If a notification is acknowledged before its selection is
persisted and the process crashes, explicitly request/rescan the tag or redeliver the GitHub
notification. This accepted limitation has no durable inbox or automatic retry policy.

## 9. Enroll projects and follow a first publication

Commit the source-owned hook files, then use the local UI to create or edit the definition.
Select enrolled dependencies with checkboxes; the repository itself is excluded. Add, edit,
duplicate or remove matrix rows through the target form. Enter distribution, suite, architecture,
runner name/labels and family-specific build parameters without editing JSON. Native rows need
an image/platform and DEB or RPM; OpenWrt rows need an SDK target and IPK or APK. Distribution
and runner values are deployment inputs, not a support catalogue. Supply repository-relative
hook paths for the families used. Review the expandable read-only JSON preview, then commit.
The original revision/blob guards still apply. Server validation rejects invalid configuration
before the GitHub write; failures preserve the draft. If an imported document cannot be edited
by the supported form, its original content remains visible; Start new definition explicitly
replaces the draft using the retained GitHub write guard. Install the thin workflow. Follow
the [extension guide](../extensions/README.md) for the exact schema and hook environment.
An existing highest SemVer is selected immediately; dependencies must have explicitly selected
work and compatible rows. New tags and explicit older requests follow the same graph rules.

The controller UI shows active work first, readable repository/version/target/rebuild labels,
prerequisites, stage, blockers, failure reason and GitHub links. The local SSE stream updates
the view automatically; there is no Refresh view button. The connection indicator shows Live,
Reconnecting or Disconnected. Reconnection receives current state and preserves unsaved edits.
All work is the default filter. Use each row's chevron for inline details, prerequisites,
logs and explicit retry. Recorded build start, completion and elapsed time refer to the assigned
attempt-1 package job, not publication. Times use the browser's local timezone, shown explicitly.
Older records and outcomes without captured timing show Not recorded; no historical timestamps
are inferred. Timing is retained with successful verified build results and sent over SSE.
Full identities remain available in technical details and API evidence. Inspection of assigned
GitHub work remains a separate explicit action, not a side effect of opening the UI.
The public feed status combines verified retained publications with a read-only projection
of accepted graph selections and current work. Enable it by configuring
`pipeline.status-workflow=".github/workflows/core-ci-status.yml"` and
`pipeline.status-workflow-blob="<reviewed workflow Git blob SHA>"`. Set the trusted worker's
public variable `CORE_CI_STATUS_URL` to the HTTPS `/public/status` endpoint, for example
`https://gw-home.vchrist.at:8443/public/status`. Leave port 443 unchanged. Include the status
workflow in tooling rollout and calculate its approved blob alongside the other workflows.

The status job needs the existing `PACKAGES_APP_PRIVATE_KEY`, `FEED_WRITER_APP_ID` and
`FEED_WRITER_INSTALLATION_ID`; it references no signing secret. Install both workflows'
matching feed concurrency settings together, so status refresh cannot replace a pending
publication. Templates remain pinned with the tooling SHA. The controller must be rebuilt
and restarted for the new projection/notification code; current assignments remain pinned.
Apply the parent implementation's documented [execution-evidence database setup](../extensions/execution-evidence.md#database-setup)
before restarting an existing database. This integration reuses that evidence column.
Verify HTTPS facts, a real status workflow, and the rendered `status/controller.json` generation.
Only display paths may change during a status-only update; compare package and key bytes.
See the [public context and template rules](../../deployment/worker/status-templates/README.md).
Missing historical timestamps and unverified running-job URLs stay unknown. Failed or lost
dispatches are logged by the controller; template/push failures appear in the status workflow logs.
Use the local Reconcile action for a fresh render. This public snapshot
has GitHub/Pages latency; the management UI continues to use SSE.

For an explicit rescan, request the repository with an empty tag through the UI/API; core-ci
selects its highest SemVer. To recover a particular older notification, request that exact tag
or redeliver it in the App's webhook delivery page. There is no separate background rescan
service. Inspect the graph after the request: an accepted HTTP response is not a package
publication result.

A useful first acceptance target is one native row. Then verify DEB/RPM, OpenWrt IPK/APK,
available architectures, chain/diamond behavior, definite failure/explicit retry, duplicate
reports, assigned-work restart and exact source selection. Real package-manager installation
is required: an Actions artifact download or file-existence assertion does not prove a feed.

## 10. Configure and verify package clients

Download public keys from a trusted deployment origin and verify their fingerprint/hash
against independently configured authority. For APT, use a repository-specific Signed-By key;
for RPM enable package and repository signature checks. For IPK install the trusted usign
public key under the client's opkg key directory. For APK v3 install the corresponding key
under `/etc/apk/keys` and use the matching OpenWrt client/toolchain.

An APT source for the current layout is:

```text
deb [signed-by=/usr/share/keyrings/core-ci.asc] https://YOUR_FEED/apt/DISTRIBUTION/SUITE SUITE main
```

An RPM repository entry uses the corresponding target path:

```ini
[core-ci]
name=core-ci packages
baseurl=https://YOUR_FEED/rpm/DISTRIBUTION/SUITE/ARCH/
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=https://YOUR_FEED/keys/apt.asc
```

OpenWrt feed URLs include format and SDK identity from the receipt. Do not invent a shorter
path that drops SDK compatibility. Use the current receipt's actual feed root/path and the
matching package-client version. The live acceptance scripts in [tests/live](../../tests/live)
show actual refresh/install/runtime and wrong-key checks for all supported client families.

Test both the intended key and rejection with an unrelated key. Modified-index/package
rejection is a separate check. Do not use `--allow-untrusted` to turn a failing trust test green.
The sandbox client scripts assert the requested top-level package version; compatible runtime
dependencies can resolve other retained versions. Build-time inputs remain exact. The demo's
static runtime output is not proof of dynamic runtime selection of every prerequisite.

## 11. Operate, upgrade and recover

### Read the state before acting

Use the graph for execution authority and GitHub for assigned run logs. `GET /api/status`
queries real database availability; it is not proof that the public webhook, feed or whole
pipeline is healthy. `GET /api/graph` exposes graph state and whether mutations are allowed.
The local UI's inspection action calls explicit reconciliation; it is not a blanket retry.

A failed node offers explicit retry. An uncertain publishing node does not: it may already
have changed the feed. Inspect the assigned run/receipt/feed revision, preserve its assignment
and reconcile it. Never delete state or dispatch a replacement merely because a result is
inaccessible or an HTTP response was lost.

### Activate the controller with a fresh graph

The controller restores one `graph_state` row from the schema above. This source has no
schema migration or importer. Do not point it at an existing deployment's database or apply
its schema over that deployment. Keep the existing database and its backup intact. A fresh
schema does not adopt in-flight assignments; finish or explicitly resolve those assignments
before routing notifications to another controller. Do not trigger replacements for unknown
outcomes.

Before using an existing feed with a fresh graph, inventory its retained rebuild revisions.
Provision `next_revision` above that maximum while the new controller is stopped. The default
value of 1 is appropriate only for a feed without previously used rebuild revisions. Preserve
all retained package bytes, signing keys and tags. This requires an operator-reviewed feed
inventory; the controller does not discover it automatically.

Review and pin the worker tooling SHA and the exact source, publication and status workflow
blobs. Install the matching thin source workflows in the explicitly authorized repositories.
Retain every workflow/tooling identity still needed by a selected execution or explicit retry;
a branch edit cannot silently update its immutable deployment inputs. The publication and
status feed-mutating jobs must use the same actual GitHub concurrency group. Validate that
exclusion in the sandbox before live publication.

There is no controller publication-admission limit. All eligible assignments are committed
and dispatched, while dependencies wait for verified published prerequisites. GitHub owns
runner capacity and the workflow's feed-write exclusion. HTTPS operations remain bounded per
named client (`github-http --max-in-flight 8` and `feed-http --max-in-flight 8`, range 1–16).
This transport limit is not a limit on assigned or running GitHub jobs.

Record assignments and database identity before starting or stopping a deployment. Verify
restoration, standard webhook delivery, local graph/SSE progress, signed publication and
public HTTPS bytes separately. A passing synthetic pipeline is not this live acceptance.
The fresh controller is not yet deployed; listener addresses, ports and TLS settings have
not been changed by its implementation.

Each workflow dispatch is independent, but a later job is not free to adopt arbitrary current
CI files. Its assignment pins still govern which bytes may execute. A workflow/blob mismatch
is rejected rather than silently adopting a branch edit.

### Back up and restore

Stop the controller before a coordinated restore or administrative schema change. For an
unmanaged instance, use the administrator's protected client option file for a consistent
InnoDB dump:

```sh
mariadb-dump --defaults-extra-file=/secure/database-backup.cnf \
  --single-transaction --routines --events core_ci > /secure/backups/core-ci.sql
```

Protect/encrypt that backup as appropriate. Keep configuration, App credentials, HMAC/TLS
files and owner-held signing-key recovery material backed up separately. Feed Git history and
public packages do not recover GitHub secret values. Do not copy a running InnoDB datadir as
though it were a consistent backup.

For a managed server, stop the controller normally so its owned child also shuts down before
an offline datadir backup. Preserve ownership and permissions. For a logical restore, provision
the intended schema, restore the dump with an administrator and reapply the restricted runtime
grants. For example, with the controller stopped and the intended target schema provisioned:

```sh
mariadb --defaults-extra-file=/secure/database-admin.cnf core_ci < /secure/backups/core-ci.sql
```

Restore into an isolated environment first when testing the procedure.

After restoring, start the controller and inspect existing assignments. A backup can predate
real feed updates or GitHub jobs: reconcile those external facts before authorizing more work.
Restore does not prove that a previously uncertain mutation failed, and lowering the rebuild
allocator is not safe permission to reuse already published package identities.

### Troubleshooting

| Symptom | First checks |
| --- | --- |
| No UI listener | Initial DB connection, schema version, file ownership and explicit database mode |
| UI POST rejected | JSON content type, request body and the reported validation error |
| Webhook `401` | Raw-body secret equality and signature header; do not disable validation |
| GitHub delivery timeout | Running controller revision and prompt acknowledgement; inspect processing logs |
| Selection blocked by another repository | Its selected tag/definition readability; no silent omission policy |
| Waiting join | Every pinned prerequisite for that target, including failed branches |
| Successful build cannot publish | Assigned attempt-one job/artifact and complete listing; missing/expired artifacts are definite failures |
| Explicit retry refuses environment | Earlier environment evidence may be deleted/expired; do not resolve different inputs silently |
| Publishing stays uncertain | Assigned run, sole push-step evidence, retained receipt, feed revision and HTTPS availability |
| SDK export rejected | Exact prerequisite SDK/toolchain/target compatibility, not runner name |
| Package client rejects feed | Trusted key, affected signed index, exact target path, client/toolchain version and HTTPS bytes |

The sandbox still requires owner confirmation of observer-token scope and feed ruleset
hardening, renewal transfer automation and remaining full live tamper-rejection coverage.
These are current deployment limits, not instructions to bypass checks or broaden credentials.
