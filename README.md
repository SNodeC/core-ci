# core-ci

Orchestrator for the SNode.C package build pipeline — see SPEC.md.

Version 0.1.2 uses snodec-auth's renewable browser sessions and requires IDP 2.0.5 or later. Upgrade the provider first; the SDK retains tokens in memory, so restarting core-ci requires a new login. Dashboard streams renew through their existing maintenance timer, pause protected updates during temporary authentication unavailability, and close when the session expires or is revoked.
