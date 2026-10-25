# core-ci

Fresh implementation of a persistent dependency graph and package execution state machine.
The controller uses SNode.C and MariaDB; GitHub executes builds and trusted publication.

This checkpoint contains the proposed design, not an implemented or activated service.
See [the refined design](docs/design.md), [setup inputs](docs/setup-inputs.md),
[the independent Claude review](docs/claude-design-review.md) and [the work log](WORK_LOG.md).
No existing core-ci implementation, protocol or design is reused.
