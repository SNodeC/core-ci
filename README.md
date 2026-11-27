# core-ci

Orchestrator for the SNode.C package build pipeline — see SPEC.md.

Version 0.1.2 uses snodec-auth's renewable browser sessions and requires IDP 2.0.5 or later. Upgrade the provider first; the SDK retains tokens in memory, so restarting core-ci requires a new login. Dashboard streams renew through their existing maintenance timer, pause protected updates during temporary authentication unavailability, and close when the session expires or is revoked.

Version 0.3.0 reads repository-owned schema-2 build declarations from each source's
default-branch tip while keeping tagged source fixed. Frozen wave plans carry the
CI commit and resolved dependency versions; build identity follows the `ci/` tree,
so CI corrections can rebuild an unchanged tag. Unsatisfied ranges block units
before dispatch. The persisted dispatch pause exposes the pending plan without
starting waves or publications. See [upgrade and configuration notes](packaging/README.md)
before starting against an existing database.
