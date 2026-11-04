# Clean build-graph model

Work in this existing checkout and SNodeC/core-ci repository. The owner explicitly
requested deletion of the previous project and a fresh model, not a refactor.

Current scope: per-repository JSON description/schema, examples, and the C++ class
and object diagrams they describe. Use ordinary typed objects and C++ containers.
JSON is the input/serialization format. Do not implement loading, traversal,
scheduling, state transitions, persistence, transport, UI or version validation.
Do not restore the deleted implementation as design input.

Leave VOC-Experiments, other SNodeC repositories, feeds and deployed services
unchanged. No additional repository, clone or worktree.
