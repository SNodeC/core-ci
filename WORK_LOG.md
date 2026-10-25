# Work log

## 2026-10-25 — reset and design checkpoint

- Verified the exact canonical root, Git metadata, current branch and dirty-file inventory.
  Remote API confirms main as default and no open PR. Local main and previous task HEAD
  were 9a7472f. SSH remote inspection failed on local SSH configuration; GitHub connector
  reads succeeded. No credentials or SSH settings were changed.
- Owner explicitly authorized discarding existing application changes. Created
  feat/persistent-package-graph from main and removed prior application/design/build
  contents. Preserved .git/, .qtcreator/ and .local-credentials/ without reading their
  contents. No sibling checkout, global instruction or external credential was changed.
- Read only repository engineering instructions and metadata from the old project;
  did not read or reuse its source, tests, protocols or design.
- Studied SNode.C database/HTTP demos, MariaDB client/connection internals, SNodeC lifecycle,
  and MQTTSuite mqttstore configuration, asynchronous storage and HTTP client ownership.
  Inspected installed snodec CMake discovery and the canonical formatting manifest.
- Owner reiterated explicit startup MariaDBClient::connect(). Later commands establish
  a connection lazily only when its framework connection object is absent; uncertain
  mutations are not replayed. Startup success gates listener creation.
- Prepared a fresh design and production/test estimates. Production implementation
  awaits the explicit net-growth approval required by global engineering instructions.
- Asked owner about already-tagged enrollment and coherent diamond change waves.
  These policies remain unsettled; no execution policy is silently selected.
- No build, database, job, feed, key provisioning, install or live acceptance performed.
