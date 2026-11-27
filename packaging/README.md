# Native packaging and operator configuration

The package installs core-ci, the separate coreci-migrate tool, reference dashboard assets/schema and a configuration example. Tests and reset helpers are never installed. Package scripts do not enable, start or restart services. Installing or activating the unit is a separate operator action.

SNode.C reads application options from [coreci] in coreci.conf. Server options belong to [coreci.local], [coreci.tls], [coreci.socket], [coreci.http.parser] and [coreci-redirect.local]. The example retains the previous listeners and HTTP limits. Select these instance sections on the command line as shown by core-ci.service.in. To disable the redirect listener, use disabled=true in [coreci-redirect]. Host, port and TLS options are never overridden in application code.

The unit keeps its three XDG directory variables for SNode.C bootstrap under the existing service sandbox. There are zero core-ci configuration environment variables. Replace the former secrets.env with service-readable files selected by webhook-secret-file, callback-secret-file and db-password-file. A trailing newline is removed. An empty database password file option permits passwordless local database authentication. Never place credentials directly in command lines or configuration examples.

The unit retains systemd TLS credentials and passes their paths through the coreci instance's tls options. Other operators may set cert and cert-key directly in the configuration file. Keep private keys and HMAC/database files readable only by the required service identity.

Create a GitHub App with Actions write and Contents read on the workflow/registry repository, Contents read and Metadata read on sources; subscribe to push and install it on those repositories. Configure github-app-id, github-installation-id and github-app-key-file. The RSA PEM is read again on each token mint for rotation. Installation tokens stay in memory. IDP login and dashboard-users protect retry, rebuild, reload and reconcile.

Registry defaults to SNodeC/Packages:ci/registry.json, target-catalog to ci/targets.json and workflow-repository to SNodeC/Packages. wave.yml builds target chains; publish.yml publishes completed targets independently. Both dispatch on main. feed-branch is feed or feed-staging. wave-delay defaults to 120 seconds; wave-deadline and publication-deadline to 23400; start-timeout to 900. Plans, status and badges remain public. See SPEC sections 6 and 7 for all interfaces.

## Upgrade an existing database

Back up the database and record the existing scheduling options. Stop all core-ci writers using the owner's normal maintenance procedure. This repository task does not operate any service.

Prepare a migration-only SNode.C file containing [coreci] database options (including db-password-file) and the previous wave-delay, wave-deadline, start-timeout, retry-delay, max-dispatch-attempts, feed-branch and workflow-repository values. In particular, old event rows do not record the debounce/retry configuration: the one-shot replay must use the original values.

Run coreci-migrate --config-file /path/to/migration.conf coreci once. It acquires the same database writer lock, converts schema 5 or 6 using the former replay semantics, installs typed state transactionally, preserves display history and atomically renames the six source tables to legacy_*. Success prints “migration complete” and returns zero. Already migrated databases are refused. Keep the backup and legacy tables until the owner has verified the converted state.

If interrupted, inspect coreci_schema before retrying: version 5/6 means the source tables still own the database and the tool can safely replace incomplete staged state; version 8 means the atomic rename completed and migration must not be repeated. No source history is discarded. The daemon refuses an unmigrated database and never loads legacy tables or display history. A fresh empty database initializes directly as schema 8.

For a schema-7 state database, run the installed `state-7-to-8.sql` through the
MariaDB client once, after backup and stopping the daemon. It acquires the same
writer lock and atomically replaces the five affected tables, retaining their
originals as `schema7_coreci_*`. All revisions, dependency inputs and source commits
remain intact. Imported CI commits equal the historical source commit; their unknown
CI tree hashes are empty and reconcile will select the current repository CI tree.
Dispatch defaults to paused in this conversion. Keep the backup and retained tables
until the state has been verified. The script refuses repeat conversion. On an
interruption, inspect `coreci_schema` and the staged/retained tables: schema 8 means
the rename completed and must not be repeated; schema 7 means staged `coreci_v8_*`
tables may need inspection and removal before another attempt. Never use client
`--force`, which could continue after a failed migration guard.

Set `[coreci] dispatch-paused=true` before restarting for the coordinated rollout.
Reconcile displays a pending frozen plan without dispatching any wave or publication.
The authenticated dashboard Pause/Resume control persists this setting. Once the
startup override is omitted, restart preserves the last dashboard value; an explicit
configuration value overrides it before dispatch. Running plans remain frozen.

The binary embeds authoritative assets and DDL at configure time. No installation or working-directory-dependent asset lookup is introduced.

Legacy per-unit artifacts remain in unit history. They are not W7 target archives with a manifest, so migration does not advertise them as publication items. Existing unit states and revisions stay intact; normal subsequent build planning produces the required target archives.

## Configuration translation

| Previous option | SNode.C option |
|---|---|
| application options previously at the root | same name in [coreci], except below |
| wave-start-timeout | [coreci] start-timeout |
| listen-address / listen-port | [coreci.local] host / port |
| tls-cert / tls-key | [coreci.tls] cert / cert-key |
| dashboard-redirect-port | [coreci-redirect.local] port; disabled=true in [coreci-redirect] replaces port 0 |
| CORECI_WEBHOOK_SECRET | [coreci] webhook-secret-file |
| CORECI_CALLBACK_SECRET | [coreci] callback-secret-file |
| CORECI_DB_PASSWORD | [coreci] db-password-file |
| implicit publication deadline | [coreci] publication-deadline (new) |
| programmatic parser limits | [coreci.http.parser] maximum-body-bytes, maximum-header-bytes, maximum-header-fields, maximum-start-line-bytes |
| programmatic socket reuse | [coreci.socket] reuse-address |
| XDG_CONFIG_HOME / XDG_STATE_HOME / XDG_RUNTIME_DIR | retained framework directory paths in the unit |
