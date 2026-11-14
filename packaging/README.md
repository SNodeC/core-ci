# Native packaging and operator configuration

The package installs only core-ci, its reference dashboard assets/schema and placeholder configuration examples. Tests and reset helpers are never installed. Package scripts do not enable, start or restart services.

Supply registry as owner/repository:path, target-catalog as a relative path in that registry repository, and workflow-repository as owner/repository. Registry build and publisher entries are workflow filenames in that repository. No repository name is built in.

Create a GitHub App with Actions write and Contents read on the workflow/registry repository, Contents read and Metadata read on source repositories; subscribe to push and install it on those repositories. Configure github-app-id, github-installation-id and github-app-key-file. The RSA PEM must be readable by the service user; core-ci rereads it on each token mint so an operator can rotate it. Installation tokens refresh in memory and are never configured or persisted. Protect the key as an operator-managed credential. No production access or key creation is part of this change.

Provide webhook and callback HMAC secrets through the environment and optionally the database password. IDP login and dashboard-users protect retry, rebuild, reload and reconcile. Public output is status.json and badges. All configuration keys are listed in SPEC 6.3.

Start with a fresh database (schema 4); older event histories are unsupported. The single writer lock, write-ahead logging and replay remain. The binary embeds the dashboard assets and schema from authoritative sources at configure time; installation-prefix and working-directory changes do not introduce alternate lookup paths.

The unit template preserves the existing service user and operator-owned configuration/TLS paths. Installing or activating it is a separate operator action; this task performs neither.
