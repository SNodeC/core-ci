# Native packaging

`VERSION` supplies the requested 0.1.0 project version. The only installed executable is `core-ci`; interrupt/reset helpers and other test tools are never installed. Temporary enrollment builds use `/tmp/codex/build/core-ci/enrollment-native` and `/tmp/codex/build/core-ci/enrollment-container`.

Decision: declare `snodec-idp` alongside `snodec` because the existing dashboard links the installed `snodec-auth::auth-client`. The owner approved this edge and the IDP client-dev export. The cleanup thread must merge the IDP revision/export PR before starting the enrolled chain.

Decision: preserve the existing embedded dashboard and SQL schema. Both are generated from the authoritative source files at configure time, so the binary has no working-directory or install-prefix dependency and the `/opt` release layout keeps working. The package also installs reference copies under `share/core-ci`; no alternate runtime lookup path or C++ application behavior is added.

Decision: the package hook selects CTest labels `core`, `persistence` and `adapters`. These include all database/browser-independent scenarios and the G4 asset check; browser and database integration scenarios retain their explicit labels and are run in full local qualification.

The environment example contains placeholders only and is installed below `share/doc/core-ci`. Package verification must use throwaway containers; never install the package on the development host or activate services during installation.

Decision: keep CPack's default unknown RPM license metadata because the current repository contains no license file or source license declaration; do not infer a license from dependencies.

Remaining deployment prerequisite: the specified Phase 4e deployment-plan/handover evidence is unavailable, and the Phase 4i directory contains only test-gate reports. The deployed unit's user, environment/config paths, arguments, hardening and restart policy must be established before adding the service template and sysusers entry, completing unit verification and merging this draft packaging change. No guessed unit is shipped.
