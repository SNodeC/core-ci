# Deployment source files

This directory contains the files installed into the source, trusted worker and feed
repositories. The controller does not run these package jobs locally.

For the complete installation procedure, configuration inventory, keys, TLS, backup and
recovery instructions, read the [deployment and operations guide](../docs/deployment/README.md).
For JSON definitions and hooks, read the [extension guide](../docs/extensions/README.md).

## Directory map

| Path | Installed/use location |
| --- | --- |
| `source/core-ci-build.yml.in` | Rendered as `.github/workflows/core-ci-build.yml` in each source repository |
| `render-source-workflow.py` | Local reviewed renderer; prints the resulting workflow Git blob identity |
| `worker/.github/workflows/core-ci-build.yml` | Trusted reusable build workflow |
| `worker/.github/workflows/core-ci-status.yml` | Trusted status-only refresh using the existing feed writer |
| `worker/.github/workflows/core-ci-publish.yml` | Separate validation/sign/publication workflow |
| `worker/core_ci/` | Trusted worker implementation |
| `worker/status-templates/` | Trusted Jinja2 templates, pinned dependencies and public presentation settings |
| `worker/keys/` | Deployment-owned public verification keys only |
| `feed/.github/workflows/pages.yml` | Pages workflow in the chosen feed repository |
| `reference/native/` | Small working native DEB/RPM and OpenWrt hook references |
| `sandbox/` | Source-owned example graph, recipes and explicit target definitions |

The thin source workflow calls a full trusted tooling commit. It has read-only source
permissions and no inherited publication secrets. Source checkout uses the assigned source
SHA, which need not equal the workflow dispatch ref.

The publisher first validates the assigned build without signing secrets. Only the protected
publication job receives format-specific signing material and feed-write access. Its prepare
step validates/signs/stages/commits; the later push step mints the restricted writer token,
performs a checked normal push and confirms HTTPS publication.

Readable run/job labels come from immutable assignment metadata. Full assignment hashes,
source/definition/workflow commits, assigned run/job/artifact IDs and final package checksums
continue to be verified. Changing display names does not authorize different source or bytes.

## Source map for worker extensions

| Module | Responsibility |
| --- | --- |
| `contract.py` | Assignment/artifact helpers, canonical hashing, version encoding and bounded paths/archives |
| `github.py` | Assigned attempt-one run/job/artifact provenance and bounded downloads |
| `environment.py` | Immutable environment resolution and read-only retry evidence retrieval |
| `native.py`, `native_container.py` | Native environment preparation, isolated hook execution and host output boundary |
| `sdk.py`, `openwrt_container.py` | OpenWrt SDK identity, compatibility and build preparation |
| `validate.py` | Independent artifact/package validation and trusted deployment checks |
| `sign_native.py` | DEB/RPM publication and retained receipt/package checks |
| `sign_openwrt.py` | SDK-matched IPK/APK v3 signatures and indexes |
| `publish.py` | Prepared checked feed commit, restricted token, normal push and HTTPS confirmation |
| `feed_status.py` | Deployment-owned Jinja HTML/Markdown/JSON over publications and public graph facts |

[Current sandbox](sandbox/README.md) · [Status templates](worker/status-templates/README.md) · [Public keys](worker/keys/README.md)
