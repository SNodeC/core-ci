# Deployment inputs and setup contract

This is a concrete proposed input inventory for the refined design. The application,
workflows and commands that depend on them are not installed or activated by this document.
Names below are the implementation contract to register through SNode.C configuration;
no existing local credential, GitHub secret, signing key, SDK or hosting value is assumed.

## Controller configuration

Use a protected deployment-owned settings file and explicitly supplied secret-file paths.
Never put these settings into source-owned ci/packages.json. The table names configuration
keys; the eventual CLI/help must expose their registered framework equivalents.

| Key | Owner-supplied value |
| --- | --- |
| http.bind | Loopback for development; public binding only after access-protection agreement |
| http.port | Local listener port |
| http.allowed_origin | Exact trusted UI origin |
| github.app_id | Controller App ID |
| github.installation_ids | Authorized installations, restricted to enrolled source/worker access |
| github.app_bot_id | Stable App bot identity expected on authorized dispatches |
| github.private_key_file | Protected server-side App private-key file |
| github.webhook_secret_file | Protected raw-body HMAC secret file |
| github.api_version | Verified REST version used for dispatch/evidence |
| github.webhook_url | Actual public HTTPS endpoint and deployment TLS arrangement |
| database.mode | CLI-only --mode managed or unmanaged; never read from the default application config |
| database.host / database.port | Explicit administrator endpoint when TCP is used |
| database.socket | Explicit UNIX socket where selected |
| database.name / database.user | Application schema and restricted account |
| database.password_file | Protected credential file; no secret in command arguments |
| database.tls_ca_file | Administrator trust anchor if database TLS is selected |
| database.managed_config_file | Separate explicitly selected private-server config |
| database.managed_executable | Actual readiness-capable mariadbd executable |
| database.managed_datadir | Pre-provisioned isolated data directory owned by this deployment |
| database.managed_socket / database.managed_pid_file | Dedicated endpoints, never system-service files |
| worker.repository | Trusted worker owner/name; sandbox VOC-Experiments/ci-coordinator |
| worker.tooling_sha | Reviewed immutable full commit SHA |
| worker.publication_workflow | Reviewed trusted publication workflow path |
| worker.publication_ref | Authorized workflow branch/tag, verified against actual workflow revision |
| source.workflow_path / source.workflow_ref | Enrollment defaults; per-repository settings stored with enrollment |
| source.approved_workflow_hash | Exact approved thin-workflow blob identity/template version |
| feed.repository | Feed owner/name; sandbox VOC-Experiments/packages |
| feed.branch | Actual branch explicitly selected by owner |
| feed.base_url | Actual HTTPS package root, not an Actions artifact URL |
| feed.hosting | Owner-approved Pages deployment or actual existing HTTPS destination |
| signing.gpg_fingerprint | Trusted archive/RPM key fingerprint |
| signing.usign_public_key | Reviewed usign public-key identity/path |
| signing.apk_public_key | Reviewed EC P-256 public-key identity/path |
| limits.definition_bytes / limits.graph_nodes | Explicit supported controller resource bounds |
| limits.assignment_bytes | Bound within verified GitHub dispatch payload limit |
| registry.credentials_file | Optional protected credentials for explicitly configured private registry auth |
| registry.allowed_origins / sdk.allowed_origins | Deployment-approved download/token origins; restrict redirects and credential forwarding |
| worker.trusted_sdk_manifest | Reviewed worker trust file approving exact SDK/host-tooling digests for privileged execution |

Secrets are read as files and not serialized back into UI responses, source definitions,
logging or database snapshots. Repository installation grants and actual SDK URLs/checksums
are supplied explicitly; no credentials are copied from preserved local directories.

## Trusted worker public variables and reviewed files

Deployment variables: CONTROLLER_APP_BOT_ID, FEED_REPOSITORY, FEED_BRANCH,
FEED_BASE_URL, FEED_WRITER_APP_ID, FEED_WRITER_INSTALLATION_ID,
PACKAGES_GPG_FINGERPRINT, PACKAGES_GPG_PUBLIC_KEY_FILE,
OPENWRT_USIGN_PUBLIC_KEY_FILE, OPENWRT_APK_PUBLIC_KEY_FILE.
Record the trusted tooling SHA and approved workflow identities with deployment settings.
Public key files are reviewed worker keys/ files, also published under feed keys/.
No key authority is read from source artifacts or an SDK. Keep a reviewed trusted-sdk manifest
with approved exact SDK/host-tooling digests. Source JSON selects SDK bytes but cannot grant
them permission to run beside signing keys. SDK approval changes are reviewed deployment
inputs, not implicit support for arbitrary source-provided executables.

Protect package-publication so only approved trusted worker code can use it. Agree whether
reviewers are required: manual approval on every publication changes automated operation.
Use deployment-branch restrictions and actual repository protections; do not claim a named
environment alone prevents untrusted workflow edits. Keep builds outside this environment.

## Publication secrets

The prepared worker uses the sandbox's existing secret names below rather than creating a
second set of aliases. Metadata confirms these signing/App names already exist at repository
level; their values and correspondence to the public keys remain unverified. The protected
package-publication environment does not exist yet. No provisioning or rotation was performed.
See [worker setup](../deployment/README.md) for current implementation status and public inputs.

| Exact GitHub environment secret | Input material |
| --- | --- |
| APT_SIGNING_KEY | Existing armored OpenPGP private key for APT/RPM |
| PACKAGES_GPG_PASSPHRASE | Only when that key has a passphrase |
| OPENWRT_USIGN_KEY | Existing usign key for IPK indexes |
| OPENWRT_APK_KEY | Existing PEM EC P-256 key for OpenWrt APK |
| PACKAGES_APP_PRIVATE_KEY | Feed-only GitHub App key; never the controller App key |

After the owner creates/protects the environment and supplies actual files, provision the
sandbox worker using file input. These are setup commands, not commands executed here.
The /secure paths describe owner-held files and must be replaced with their actual paths.

```sh
gh secret set APT_SIGNING_KEY --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/archive-private.asc
gh secret set PACKAGES_GPG_PASSPHRASE --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/archive-passphrase
gh secret set OPENWRT_USIGN_KEY --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/ipk-private.key
gh secret set OPENWRT_APK_KEY --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/apk-private.pem
gh secret set PACKAGES_APP_PRIVATE_KEY --repo VOC-Experiments/ci-coordinator --env package-publication < /secure/feed-writer-app.pem
```

Omit the passphrase command for an unprotected OpenPGP key. Never run echo, command
substitution, shell tracing or argument interpolation with secret values. Restrict source
file permissions; commands receive values only on stdin. Verify only secret names/configured
public fingerprints, never print private material. GitHub documents stdin/file provisioning.
[GitHub secret setup](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-secrets).

The privileged job uses umask 077 and a temporary keyring/directory; imports via file/FD;
checks private/public correspondence against reviewed keys/fingerprints; and removes key
material through cleanup on success and failure. It must not upload/cache the directory.
No automatic generation or rotation. rpm/gpg passphrase handling must use a verified FD/file
interface rather than a password argument. Publish only public verification material.

## Source artifact read access

The publisher must fetch an artifact from the assigned source repository. Do not assume the
worker repository's GITHUB_TOKEN can read another repository's private Actions artifacts.
Prove the permitted token/API path in the first real sandbox slice. The feed-writer token
is restricted to the feed and cannot be stretched into a general source-reader token.

If private source artifact access requires a separate credential, the concrete proposal is
a dedicated read-only source-reader App, available only in a protected validation environment,
with SOURCE_READER_APP_PRIVATE_KEY and public SOURCE_READER_APP_ID /
SOURCE_READER_INSTALLATION_ID. It gets only actions/read and required metadata on selected
source repositories, no contents/write, dispatch or feed access. This is a conditional setup
choice needing owner approval, not a secret assumed to exist. Source builds never receive it;
the controller App private key stays on the controller. Reuse the same trusted App-token
helper for reader/writer credential use, not a parallel authentication implementation.

## Database setup, backup and restore deliverables

The implementation must ship fresh schema SQL and a schema-version check. No import or
migration from the discarded application. Administrator creates the selected schema/account
and applies SQL while core-ci is stopped. Runtime grants are SELECT, INSERT, UPDATE, DELETE
on that schema only; no global grants, schema creation, GRANT OPTION or server control.

Managed mode's separate server config specifies isolated datadir/socket/pid/log paths and
an appropriate server account. Owner initializes the datadir before application startup.
Unmanaged mode only uses the provided endpoint/account and never manages the system service.
Actual database/TLS/auth-plugin and filesystem requirements are verified with real MariaDB.

Backups use a dedicated protected client defaults file and consistent InnoDB snapshots,
for example the eventual deployment's real schema name in place of CORE_CI_SCHEMA below:

```sh
mariadb-dump --defaults-extra-file=/secure/backup-client.cnf --single-transaction --routines --triggers CORE_CI_SCHEMA > /secure/core-ci-backup.sql
```

This is a command template, not evidence of a completed backup. Protect/encrypt the output
and copy it off-host. Restore into the selected isolated schema with core-ci stopped using
an owner-held restore client file. Retain deployment config and public trust identities
separately. On restart, reconcile every assigned run and existing feed receipt before new
work; a backup can predate a completed dispatch or publication. Never restore signing private
keys from core-ci because core-ci never stores them. Prove the restore procedure on a
sandbox database before calling the operation recoverable.

## Activation handover

Provide actual App/installation grants, source hooks/recipes, exact SDK URLs/checksums,
trusted tooling SHA, workflow refs, public keys/fingerprints, feed branch/HTTPS destination,
TLS/webhook reachability and UI access protection. Review generated source/worker/feed changes
before deploying them. For Pages include the reviewed deployment workflow and limit check.
Keep commit/push/workflow installation/App configuration/key provisioning/feed publication
and public activation distinct. None has been performed by this proposal refinement.
