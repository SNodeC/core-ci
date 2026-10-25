# core-ci

A persistent dependency graph and package execution state machine. The SNode.C controller
coordinates GitHub and MariaDB; GitHub runs builds and trusted publication. There is no IdP.

Implementation is in progress. The initial database lifecycle works with real managed and
unmanaged MariaDB. Local repository enrollment/editing and GitHub committed-revision feedback are implemented.
The UI, authenticated tag events, immutable source selection, graph traversal,
committed GitHub dispatch authority and assigned-run/job/artifact reconciliation are wired
and covered locally. Trusted worker tooling, signing/publication and live package acceptance
remain in progress. Fresh native worker workflows, isolated build preparation and trusted
DEB/RPM signing/indexing are now prepared under deployment/. Local tests build real packages,
verify signatures, reject modified data and preserve bytes on replay. OpenWrt workers and
live package publication acceptance are unfinished. Controller publication dispatch and
Git/HTTPS receipt proof are now wired and covered by local synthetic-service fixtures. Nothing has been installed, pushed or
externally activated. See [worker setup and limitations](deployment/README.md).

Controller-side registry authentication, image parsing and SDK discovery have been removed.
GitHub workers own environment resolution and return identities in verified publication
receipts. GitHub records environment evidence before hooks; explicit retries reuse it
through read-only artifact retrieval and assigned-run references. Missing evidence after
a possible build stops retry without changing inputs. Live activation remains outstanding.

See [the refined design](docs/design.md), [setup inputs](docs/setup-inputs.md),
[the independent Claude review](docs/claude-design-review.md) and [the work log](WORK_LOG.md). Deployment settings and credential-file ownership are
documented in [configuration](docs/configuration.md).
No discarded implementation, protocol or design is reused.

## Build and focused checks

Use installed SNode.C development packages with db-mariadb and Express IPv4 components,
CMake 3.25+, a C++20 compiler, Ninja, Python 3, clang-format and cmake-format.

```sh
cmake -S . -B build/codex/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/codex/debug -j2
cmake --build build/codex/debug --target format-check
ctest --test-dir build/codex/debug --output-on-failure -j2
```

Lifecycle tests require mariadbd, mariadb-install-db and mariadb. They create only isolated
private test servers under the selected build directory, with networking disabled. Their
HTTP listeners bind loopback. The worker package suite also requires cc, dpkg-deb, dpkg,
rpmbuild, rpm, rpmkeys, rpmsign, gpg, gpgv, gpgconf, apt-ftparchive and createrepo_c.
It uses isolated generated test keys, not deployment signing secrets. They never control the administrator's MariaDB service.

## Current database lifecycle

Register configuration before core::SNodeC::init(), explicitly connect once, and listen only
after initial success. Named framework help exposes database options and web instance options:

```sh
build/codex/debug/core-ci --help=expanded
build/codex/debug/core-ci web --help=expanded
```

Supply an explicit application config file when running local experiments. Select ownership
on the CLI using database --mode managed or unmanaged; it cannot come from the default
application configuration. Specify --socket (or --host/--port), --user, --name and a private
--password-file. The file must be owned, regular, at most 4096 bytes and inaccessible to
group/other users. An empty file explicitly selects an empty password for a separately
configured socket-authenticated account; it does not grant database permissions.

Managed mode also requires --managed-executable, --managed-config-file, --managed-datadir
and --managed-pid-file. The pre-provisioned datadir must be private and owned by the current
process account; absent socket and PID paths must be directly inside it. The separate server
config chooses the actual database account and log settings. The controller starts it with
networking disabled and waits for its native authenticated readiness notification, without
polling. It owns shutdown only for that child. No datadir/schema initialization runs inside
the controller. Later command execution uses the framework's lazy connection establishment
when its connection object has disappeared; mutations will not be automatically replayed.

Apply the fresh `sql/schema.sql` before startup; missing/invalid schema leaves no listener.
The local web UI supports enrollment, JSON target/dependency editing, import and GitHub save
feedback. Existing committed definitions must be read before replacing them.

GET /api/status queries the real database asynchronously and returns available/unavailable.
It is a lifecycle boundary check, not evidence that repository orchestration is ready.
