# core-ci: build graph model

This is a fresh start. The previous implementation has been deleted.

1. [Per-repository JSON schema](schema/repository-builds.schema.json)
2. [Four repository descriptions](examples/): core, app, side and join
3. [Class and object diagrams](docs/build-graph.md)

Each repository describes its own builds. A prerequisite names a repository and
one build in that repository. The combined object graph shares prerequisite
objects across repositories.

This step defines structure only. It contains no controller, algorithms, state
machine, version rules, database, UI or GitHub integration.
