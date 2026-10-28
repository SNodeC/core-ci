# core-ci

**core-ci turns versioned source repositories into a dependency-aware, signed package feed.**
It is a small C++ controller built on SNode.C, a MariaDB database, a local web interface,
and GitHub Actions workflows. The controller decides what may run. GitHub performs the
builds, package validation, signing, indexing and publication.

A version tag can start a chain of builds. Each dependent waits for the exact prerequisite
executions selected for it. A project depending on two branches of a diamond waits for both;
one successful branch cannot stand in for the other. A restart preserves assigned GitHub
work instead of starting replacements.

## Read the documentation

| You want to… | Start here |
| --- | --- |
| Understand the terms and find a reading path | [Documentation guide](docs/README.md) |
| Understand selection, graph traversal, state transitions and recovery | [Architecture and algorithms](docs/architecture/README.md) |
| Enroll a project, write a hook, add targets or extend supported behavior | [Extension guide](docs/extensions/README.md) |
| Install and configure the complete system, keys, GitHub Apps, HTTPS and MariaDB | [Deployment and operations](docs/deployment/README.md) |
| Find the deployable workflow and worker files | [Deployment files](deployment/README.md) |
| Inspect the configured example graph and current live verification | [VOC-Experiments sandbox](deployment/sandbox/README.md) |

The guides describe the current implementation. Engineering instructions are in
[AGENTS.md](AGENTS.md); implementation decisions and check records are in
[WORK_LOG.md](WORK_LOG.md).

## What happens when a tag is pushed?

1. GitHub sends an authenticated notification to core-ci.
2. Core-ci resolves exact source commits and reads their `ci/packages.json` definitions.
3. The graph selects repository/target executions and their prerequisite relationships.
4. Eligible build assignments are committed to MariaDB before GitHub is contacted.
5. A thin workflow in each source repository calls pinned tooling in a trusted worker repository.
6. A separate trusted workflow verifies, signs and publishes the packages.
7. Core-ci verifies publication evidence before releasing dependent work.

The configuration-writing UI stays local. Only the signed webhook endpoint needs public
HTTPS access. There is no application login, identity provider or role-management system.

## Where the responsibilities live

| Location | Owns |
| --- | --- |
| Controller and MariaDB | Enrollment, graph, immutable selections, attempts, dispatch authority and verified results |
| Source repository | Build definition, source, project-specific hooks and recipes |
| Trusted worker repository | Reusable builders, independent validators, publisher and trusted public keys |
| Feed repository and HTTPS host | Packages, development exports, publication receipts, indexes and public verification keys |

The controller does not run package builds or containers. Containers, SDK discovery and
package-manager tools run in GitHub jobs. Supported package paths are native DEB/RPM and
OpenWrt IPK/APK v3; distributions and runner choices come from explicit source-owned rows.

## Build the controller

Install the public SNode.C development packages, OpenSSL and nlohmann-json development
packages, a C++20 compiler, CMake 3.25 or later, and Ninja. The required SNode.C CMake
components are listed in [CMakeLists.txt](CMakeLists.txt). No personal framework source
path belongs in the build configuration.

```sh
cmake -S . -B build/codex/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/codex/debug -j2
cmake --build build/codex/debug --target format-check
```

Building does not configure a database, install a service or activate GitHub. Follow the
[deployment guide](docs/deployment/README.md) for those steps. It also explains the required
private files and why the configuration template is not a ready-to-run installation.

For a change to selection, dispatch or worker evidence, relevant focused checks include:

```sh
ctest --test-dir build/codex/debug --output-on-failure \
  -R '^(graph-state-boundaries|pipeline-.*|worker-environment-boundaries|worker-provenance-boundaries)$'
```

Pipeline tests use real isolated MariaDB with synthetic GitHub/HTTPS services. Those are
local boundary checks, not live GitHub acceptance. Package-tool and live client checks have
additional requirements described in the extension and deployment guides.

## Operational guarantees and limits

Source selections and assigned inputs are immutable. Explicit retries reserve a new package
rebuild revision. Unknown dispatches, database commits and possible feed writes are preserved
as uncertain; they are never a reason to launch a replacement automatically.

Validated webhook notifications are acknowledged promptly, while asynchronous work continues.
If the controller crashes before a new tag selection reaches MariaDB, the operator must
explicitly rescan/request the tag or redeliver the notification. This accepted boundary does
not have a durable notification inbox.

This source branch contains a fresh controller implementation. Its 20-node pipeline checks
use isolated MariaDB and synthetic GitHub/HTTPS endpoints. The rewrite has not been deployed
or accepted through a real signed package publication. Existing sandbox deployment evidence
does not validate this binary. See the architecture guide for its persistence and activation
limits.
