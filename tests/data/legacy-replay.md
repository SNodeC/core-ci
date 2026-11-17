This is a production-shaped regression fixture, not a database export.

`legacy-w964-plan.json` is the sanitized production plan captured for w964 on
2026-11-17: six packages, 40 targets and 173 units. The existing production
declaration fixtures supply the dependency graph and source branch identities.
`W_ReplayHistory.cpp` constructs typed legacy events with gaps in sequence numbers
so the allocating records retain the production wave identifiers w964 and w1339.
It reproduces the observed outcome counts (w964: 148 Built, 24 Failed, 1 Building;
w1339: 148 Built, 10 Failed, 3 Building, 12 Planned), timeout, automatic retry,
core-ci version change, second timeout/retry and process resumption. Earlier
allocations are represented by per-package revision bases from the first plan
entry; this deliberately does not claim to reproduce all production event rows
or target-specific earlier revisions.

`legacy-replay-baseline.json` was generated before changing the event/engine ABI,
using Engine.cpp compiled from commit f19aaaf (the pre-0.1.6 implementation), with
the original 23400-second timeout and the same fixture history. The checked-in
golden result contains all existing wave fields, frozen plans/revisions, every
unit's complete revision map, statuses, allocation identities and timing/retry
state. The test compares the complete result under configured timeouts 23400,
86400 and 60 seconds. It must not regenerate the baseline with the new engine.

Baseline SHA256: 647bd83dcd3fdff8e3e1f280d04d45e4c04323ab04691fc334cd32c61691ac50
Plan SHA256: bd3cebf49a407f0b794467c306c18b956e54fc3bbe77eea40cc7e356f6850850
