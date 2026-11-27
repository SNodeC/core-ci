# core-ci package operations

The package installs the core-ci daemon, its service unit, dashboard assets,
Contract X schema, and an example configuration. Service activation is an
operator action. See IMPLEMENTATION-X.md for the quiesce, backup, staging,
and production gates.

SNode.C reads application settings from the coreci section. Listener and TLS
settings belong to the coreci and coreci-redirect WebApp instances. Use the
instance selections in the supplied systemd unit. Application settings and
secret-file paths come from the SNode.C configuration.

Use a fresh schema 9 database. While paused and with the service stopped, seed
revision floors as the next allowable revision for each
`package/version/target`. Import matched legacy publication dates into
`legacy_dates` with keys `package/version/revision/target` and Unix seconds as
values. These values come from the verified old database backup and the pre-x
ledger using the rules in X5 and X6. Use the cutover timestamp plus 30 days in
`retirement_at`. Each seed is a row in `coreci_records` with its section,
record key, and JSON payload. Scalar records use an empty key. The section for
revision floors is `revisions`.

The initial `dispatch-paused` setting is applied once on the new database.
Later Pause and Resume actions are persisted and survive service restarts.
The `qualification_fault` option accepts `after_push:<token>` on feed-staging
only. Keep the token in staging configuration for the restart check in T4.
