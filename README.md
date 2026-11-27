# core-ci

core-ci admits repository releases, freezes exact build inputs, and schedules
builds and feed publications through GitHub Actions. The scheduler persists
reservations, attempts, and publication receipts in MariaDB. GitHub Actions
executes the claimed requests. The dashboard uses IDP OAuth with MFA and
shows the committed repository queues and package states.

See [SPEC.md](SPEC.md) for Contract X and [packaging/README.md](packaging/README.md)
for configuration and installation. [IMPLEMENTATION-X.md](IMPLEMENTATION-X.md)
defines qualification and rollout.

Build and test the standalone scheduler with CMake and CTest. Set
`CORECI_WITH_SNODEC=ON` with the installed SNode.C and snodec-auth development
packages to build the daemon. Run `python3 tests/database.py BUILD_DIRECTORY`
to exercise the daemon with a disposable local MariaDB instance.
