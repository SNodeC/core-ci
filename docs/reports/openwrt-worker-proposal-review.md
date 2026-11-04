# OpenWrt build-worker proposal review

Status: reviewed with Claude; no implementation requested in this review stage and no
production or test changes made. This is separate from the publication-tools proposal.

Source: rewrite/slim-graph-controller at 1250f43c57dee10006cffe68083dac21257b09d5,
identical source tree to deployed merge 8f14486. Review session:
2c479bf5-90fc-4bae-ae99-726bdcdc3958. Three review exchanges are retained under
/tmp/codex/build/core-ci/openwrt-worker-review/.

## Source-grounded findings

1. Exact published prerequisite packages and small development exports already cross jobs.
   native.prepare_dependencies verifies receipt identity, compatibility, byte size and hash;
   openwrt_container stages development files and refuses to overwrite SDK/prerequisite files.
   A new shared staging service, SDK per project or development-package format is unnecessary.
2. Build jobs do not publish an integrated SDK. Each job still downloads/prepares the original
   SDK, copies it into a Docker image, and copies it again to a private writable hook workspace.
   The separately retained publication SDK artifact is not changed by this proposal.
3. The pristine SDK is important to independent post-hook validation. Hook writes belong only
   to /work/sdk. A shared writable SDK mount used by both containers would weaken that boundary.
4. The OpenWrt Dockerfile installs host tools. Removing the SDK layer does not remove the image
   build or package installation. Further elimination needs a separately reviewed setup or
   prepared image; no such mechanism is included or approved here.
5. Existing development exports are regular files under include/lib/share. They are not a full
   OpenWrt staging snapshot: archive links are rejected and host executables are not preserved.
   No widening of that contract is proposed by this worker simplification.

## Agreed smallest coherent change

Only deployment/worker/core_ci/native.py would change:

- Keep the verified SDK at work/sdk inside the existing TemporaryDirectory.
- Keep the existing readability/execute-mode handling and cleanup ownership.
- Hoist the existing shared container-input argument list before the OpenWrt branch.
- In that existing branch, add the SDK read-only bind mount at /sdk and the existing
  CORE_CI_APK path as a container environment argument, for both hook and validator.
- Exclude sdk from the Docker build context with its generated .dockerignore.
- Remove the sdk_setup initializer, SDK COPY/ENV assignment and Dockerfile concatenation.
- Retain the hook's private /work/sdk copy. Preserve UID/GID 1000, capabilities, read-only
  roots, resource limits, source credential exclusion, sequential hook/validator execution,
  network-disabled validator and read-only validation outputs.
- Preserve native Dockerfile bytes and native container arguments.

Keeping SDK inside work is the smallest change, not a universal requirement; other locations
would need explicit readability/cleanup handling. Dropping redundant OpenWrt image COPY entries
for tooling/prerequisites/assignment is optional and excluded from this smallest candidate.

## Estimates and proof limits

The in-memory design parses as Python and has 15 added / 15 removed / net 0 production lines,
including 12 unchanged relocated lines. Three new operation lines are at most 100 characters.
Claude confirmed that its earlier growth estimate depended on an unnecessary formatting
assumption. There is no new helper, flag, branch or state, and no C++/workflow/controller change.
These are proposal counts, not a changed working-tree implementation.

Estimated focused test addition: 45–60 lines in tests/worker_environment.py, covering an
OpenWrt build's Docker context, both read-only SDK mounts and APK environment, validator
isolation, preserved native behavior and existing hook restrictions. Discover actual test
registration before implementation. No tests, builds or Docker operations were run here.

A real IPK and APK sandbox build must verify SDK wrapper execution and make through the
read-only mount and private working copy. Performance needs measured before/after preparation
spans. No speedup is established by source review or an in-memory syntax check.

No production, tests, workflows, credentials, tags, feeds or deployed state were modified.
The pending publication-tools job and its production-growth bounds remain unapproved and
separate. Unrelated dirty documentation and private directories were preserved.
