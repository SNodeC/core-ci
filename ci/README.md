# Post-release package declaration

Merge this conversion only after the implementation PR is merged and the old
schema-1 pair has built and published core-ci 0.3.0. It is deliberately separate
from the implementation release commit. No new source version or tag is needed.

Schema 2 preserves the source's CMake requirements: SNode.C discovery is
unversioned (`>=0.0.0`); `find_package(snodec-auth 2 REQUIRED)` consumes IDP's
SameMinorVersion export (`>=2.0,<2.1`). Targets and existing family build lists
are unchanged. Only Rocky 9 adds and activates GCC Toolset 14 here.

Hooks use `SOURCE_DIR` for tagged source and `CI_DIR` for their own packaging
configuration and maintainer scripts. CPack records exact native dependency versions/revisions from
`DEPENDENCIES_JSON`; core-ci checks the declared compatibility ranges while planning. Local
packaging uses the same configuration under `ci/`. Existing formatter discovery
includes these CI CMake assets; shared style settings are unchanged.
