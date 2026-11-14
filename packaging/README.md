# Native packaging and operator configuration

The package installs only core-ci, its reference dashboard assets/schema and placeholder configuration examples. Tests and reset helpers are never installed. Package scripts do not enable, start or restart services.

Registry defaults to SNodeC/Packages:ci/registry.json, target-catalog to ci/targets.json and workflow-repository to SNodeC/Packages. The controller dispatches wave.yml on main with a public frozen plan, one chain per target and one publication. wave-delay defaults to 120 seconds, wave-deadline to 23400, and feed-branch to feed (feed-staging is also supported).

Create a GitHub App with Actions write and Contents read on the workflow/registry repository, Contents read and Metadata read on source repositories; subscribe to push and install it on those repositories. Configure github-app-id, github-installation-id and github-app-key-file. The RSA PEM must be readable by the service user; core-ci rereads it on each token mint so an operator can rotate it. Installation tokens refresh in memory and are never configured or persisted. Protect the key as an operator-managed credential. No production access or key creation is part of this change.

Provide webhook and callback HMAC secrets through the environment and optionally the database password. IDP login and dashboard-users protect retry, rebuild, reload and reconcile. Public output is status.json, badges and /api/v1/waves/{wave_id}/plan.json. All configuration keys are listed in SPEC 6.3.

Start with a fresh database (schema 5); older event histories are unsupported. The single writer lock, write-ahead logging and replay remain. The binary embeds the dashboard assets and schema from authoritative sources at configure time; installation-prefix and working-directory changes do not introduce alternate lookup paths.

The unit template preserves the existing service user and operator-owned configuration/TLS paths. Installing or activating it is a separate operator action; this task performs neither.
