# OpenWrt publication wait correction: agreed design and implementation prompt

Status: design agreed with Claude; implementation not started. Owner approval of the new
mechanism and concrete growth bounds is required before Phase 1. Commit, push and deployment
are separate stages. Current branch rewrite/slim-graph-controller, HEAD
1250f43c57dee10006cffe68083dac21257b09d5; same tree as deployed merge 8f14486.

## Agreed correction

The feed lock currently includes full SDK acquisition and preparation. Move that work into
one unprivileged OpenWrt-only job after validation, outside feed concurrency. It independently
checks the official SDK and produces a small tools artifact. The publisher verifies its
manifest hash directly against that job output, exact assignment/environment, file set,
sizes and hashes before any executable runs. It retains package verification, signing,
current-head index generation, checked commit/push and separate public HTTPS confirmation.

Remove the whole-SDK upload/download, publisher full-SDK preparation and OpenWrt publisher
apt installation. Native builds skip the helper; there is no empty native helper job.
The helper holds no publication environment, secrets or feed lock, and never parses packages.
Both distributed publication templates follow the same contract. No controller or C++ change.

The validator must not supply executable authority: its package parser handles untrusted
input. Its bounded environment output is a selector only; the separate helper independently
verifies it against official metadata and SDK bytes. Its artifact ID/hash reaches publication
directly, rather than through the validator.

## Concrete evidence and boundaries

Both assigned official SDK archive hashes matched the recorded receipts. Their host tools
are wrapper scripts that require hidden ELF binaries, the SDK loader, runas.so and recursive
DT_NEEDED dependencies. Measured current closures: IPK 8 files / 2,152,850 bytes; APK 6 files /
4,082,672 bytes. These are substantially smaller than the 259–286 MB full archives.
Local minimal-closure checks passed IPK ephemeral-key sign/verify and mkhash, plus APK version
3.0.5. These do not establish APK signing/indexing, real IPK indexing, future SDK ABI compatibility,
GitHub conditions or live performance. No production key was used; disposable test keys removed.

Pinned upload source documents hidden-file exclusion by default and loss of ZIP executable
modes. The helper must include hidden files; publisher modes are restored only after verification.
Pinned download source places a single selected artifact at the requested root. Official GitHub
context documentation permits inputs and cancelled() in job conditions. Explicit family/result
conditions preserve the skipped-push certainty boundary, without always() or continue-on-error.
The upload Action pins @actions/artifact 6.3.1, whose artifact service client uses
ACTIONS_RUNTIME_TOKEN, independently of GITHUB_TOKEN Actions permission. This is source proof;
actual sandbox execution remains unverified. Keep helper contents:read, with no secret grant.

Evidence: /tmp/codex/build/core-ci/publication-wait-fix/, including review rounds 1–5,
sdk-closure/{results,runtime-closure,minimal-tool-checks}.json and upload-credential-source.json.
Claude session: 2c479bf5-90fc-4bae-ae99-726bdcdc3958. The following prompt incorporates the
independently checked and Claude-confirmed signature, style, test-scope and manifest corrections.

## Estimate and proposed approval bounds

| Area | Estimated added | Estimated removed | Estimated net | Proposed hard bound |
| --- | ---: | ---: | ---: | --- |
| Canonical publication YAML | 55 | 6 | +49 | Both YAML files together <= +120 net |
| Worker publication YAML | 63 | 8 | +55 | Included above |
| sdk.py | 85 | 0 | +85 | All Python together <= +110 net |
| validate.py | 5 | 0 | +5 | Included above |
| sign_openwrt.py | 3 | 4 | -1 | Included above |
| publish.py | 3 | 3 | 0 | Included above |
| Production total | 214 | 21 | +193 | <= +230 net and <= 260 added |
| Tests | 150 | 30 | +120 | <= +180 net, separate |
| C++ | 0 | 0 | 0 | No changes; current 3,572 of 3,576 |

Reduction alone cannot make a GitHub job-level lock release during selected steps. A separate
job is needed to move bulk SDK work outside it. Fresh hosted jobs need an explicit handoff;
this small verified artifact replaces the full-SDK handoff and repeated privileged setup.
No cache, controller queue, timer, retry protocol, image registry mechanism or shared runner.

Tradeoff: total bulk transfers drop from four to three; none remains under the feed lock.
There is one extra serial OpenWrt job hop and an official archive download outside the lock.
Small package/tool artifacts, venv/pip and native setup still occupy the feed job. This is a
bounded correction of OpenWrt bulk preparation, not a claim that all serialized preparation
or every identified issue is solved. Status churn, native setup, dialog validation and finer
timing instrumentation remain separate. No numeric speedup is claimed before measurement.

## Implementation prompt

Phase 0 source/documentation checks below have already passed for the recorded pinned revisions.
Reuse the recorded evidence unless those revisions change. Claude confirmed final agreement
after the upload credential question was resolved; no design contradiction remains.

```text
Task: Narrow the OpenWrt publication feed lock by replacing the whole-SDK handoff
with a verified, unprivileged tools job (agreed "corrected A").

Repository: /home/voc/projects/snodec/core-ci, branch rewrite/slim-graph-controller.
Obey AGENTS.md fully. Do not switch/create branches, commit, push, tag, deploy,
dispatch workflows or touch GitHub. Preserve all unrelated working-tree changes.
Never inspect secrets/, .qtcreator/, .local-credentials/, .git/, credentials/configs
or private logs. Temporary work only under /tmp/codex/build/core-ci/publication-wait-fix/.

PHASE 0 — STOP GATE (mandatory, before ANY production edit)
This task adds a new GitHub job, a small executable artifact handoff and net production
growth. AGENTS.md requires explicit owner approval of concrete bounds first.
1. Read-only: re-read AGENTS.md, docs/reports/publication-wait-investigation.md,
   deployment/source/core-ci-publish-bundle.yml.in,
   deployment/worker/.github/workflows/core-ci-publish.yml,
   deployment/worker/core_ci/{sdk,validate,publish,sign_openwrt,contract,environment}.py,
   tests/{openwrt,worker,worker_provenance,pipeline}.py, src/Pipeline.cpp:178-330,
   /tmp/codex/build/core-ci/publication-wait-fix/sdk-closure/{results,runtime-closure}.json.
2. Verify against the pinned action sources/docs (read-only, no execution):
   actions/upload-artifact@cf430e030ddbb5b0abf93d22962f4752f3646cd9 needs
   include-hidden-files: true for dot-files, and file modes are not preserved;
   actions/download-artifact@9000827ccba6bdab643e8b6fd33ac0654aef8333 single-ID
   extraction layout. Report findings; if either contradicts the design, STOP and report.
3. Present: design summary (below), per-file added/removed/net estimate, and the bounds:
   production net <= +230 and added <= 260 (both YAML files and all Python combined;
   YAML <= +120 net, Python <= +110 net); tests <= +180 net (separate); C++ 0;
   docs minimal current-state only.
4. STOP. Do not edit production files until the owner explicitly approves these bounds and
   the new mechanism (unprivileged OpenWrt-only tools job + hash-pinned tools artifact).
   If you would exceed any bound or need any mechanism not listed, STOP and re-estimate.

PHASE 1 — after explicit owner approval only

Design (implement exactly; no extra flags, caches, retries, fallbacks, generic loaders):

A. deployment/source/core-ci-publish-bundle.yml.in
 validate:
  - Delete the "Retain verified official SDK archive" step.
  - Give the validation step `id: validation`; add job output
    `environment: ${{ steps.validation.outputs.environment }}`.
  - Leave the validated upload otherwise unchanged.
 New job `tools` (insert between validate and publish):
  - name: Extract verified OpenWrt publication tools   (must never equal/end with "Publish ...")
  - needs: validate
  - if: fromJSON(inputs.assignment).target.family == 'openwrt'
  - runs-on: ubuntu-24.04, timeout-minutes: 15, permissions: contents: read
  - NO environment:, NO secrets., NO concurrency:, NO always(), NO continue-on-error.
  - outputs: artifact-id (from upload step id `tools`), tools-sha256 (from step id `extracted`).
  - steps: tooling checkout (same pinned action/inputs as other jobs);
    `sudo apt-get update && sudo apt-get install -y zstd build-essential`;
    step id `extracted` with env CORE_CI_SDK_ENVIRONMENT: ${{ needs.validate.outputs.environment }}:
      set -euo pipefail; umask 077; write $ASSIGNMENT to $RUNNER_TEMP/assignment.json;
      python3 tooling/core_ci/sdk.py --assignment ... --assignment-sha256 "$ASSIGNMENT_SHA256"
        --output "$RUNNER_TEMP/tools"
    upload-artifact (same pinned SHA) id `tools`: name <artifact_name>-tools,
      path ${{ runner.temp }}/tools/, include-hidden-files: true, if-no-files-found: error,
      overwrite: false, retention-days: 7.
  - It must not download the validated artifact or parse any package.
 publish:
  - needs: [validate, tools]
  - if: >-
      !cancelled() && needs.validate.result == 'success' &&
      ((fromJSON(inputs.assignment).target.family == 'openwrt' && needs.tools.result == 'success') ||
       (fromJSON(inputs.assignment).target.family == 'native' && needs.tools.result == 'skipped'))
  - Keep job name, environment, concurrency group/queue, timeout, outputs, step name
    "Publish checked feed update" and push step byte-identical.
  - Keep validated download; add a tools download (OpenWrt only) by
    needs.tools.outputs.artifact-id to ${{ runner.temp }}/tools.
  - Split install: apt-get line only `if: ...family == 'native'` (same package list);
    venv + pip for all families.
  - Prepare step: replace `--sdk-archive ...` with `--tools "$RUNNER_TEMP/tools"`; add env
    CORE_CI_TOOLS_SHA256: ${{ needs.tools.outputs.tools-sha256 }}.
 confirm: unchanged.

B. deployment/worker/.github/workflows/core-ci-publish.yml
  Make byte-identical to the new bundle template (same @TOOLING_SHA@ contract). Do not delete
  either file; do not add any generator.

C. deployment/worker/core_ci/validate.py
  For OpenWrt only, after writing the final manifest, append
  `environment=<canonical(manifest["environment"])>` to GITHUB_OUTPUT; require no newline and
  <= 16384 bytes. Native behavior unchanged.

D. deployment/worker/core_ci/sdk.py (reuse prepare/extract unchanged; reuse contract helpers
   require, output, regular_file, relative_path, file_hash, read_json, write_json, canonical,
   digest, assignment; validate.trusted_deployment)
  - Constants: fixed paths per format
      ipk: scripts/ipkg-make-index.sh, staging_dir/host/bin/{usign,.usign.bin,mkhash,.mkhash.bin}
      apk: staging_dir/host/bin/{apk,.apk.bin}
      both: staging_dir/host/lib/{ld-linux-x86-64.so.2,runas.so}
    ELF seeds: the .bin files, loader, runas.so.
  - publication_tools(root, selected, environment, destination): bounded recursive DT_NEEDED closure
    via output(["readelf","-d","-W",file]) parsing "(NEEDED) Shared library: [name]"; names must
    be plain basenames and resolve only to staging_dir/host/lib/<name> inside the verified root;
    missing/escaping/non-file -> ValueError; at most 32 files and 64 MiB total. Copy resolved
    bytes to regular files at identical relative paths. Write tools.json:
    {schema:1, assignment_sha256, execution, attempt, target, environment, files:[{path,size,sha256}]}.
    Return sha256 of tools.json. Derive format from selected, and pass selected/environment
    explicitly from main: no globals or hidden captures. No host-lib fallback, no other scripts,
    no caching.
  - verified_tools(directory, expected_sha256, selected, environment): regular tools.json whose
    file_hash == expected (SHA-256 format checked); schema/assignment digest/execution/attempt/
    target equal; environment == given manifest environment exactly; fixed format paths present;
    on-disk set (including symlinks, excluding plain directories) == listed files + tools.json;
    reject duplicate paths; type(size) is int and size > 0; hashes match SHA256;
    every file via regular_file, size+hash match; count/size bounds; only then chmod 0o755;
    for apk set CORE_CI_APK to the wrapper. Return directory.
  - main(): argparse --assignment --assignment-sha256 --output; assignment(); trusted_deployment();
    EXECUTION/ATTEMPT match; family openwrt; read CORE_CI_SDK_ENVIRONMENT (<=16384 bytes,
    no newline, JSON object); prepare(target, environment, RUNNER_TEMP/"tools-sdk") with no
    archive (prepare performs the single official metadata read + download + checks); require the
    environment unchanged after prepare; publication_tools(); append tools-sha256 to GITHUB_OUTPUT.
    Do NOT call environment.sdk() separately.

E. sign_openwrt.update: replace the prepare(...) call with verified_tools(tools,
   os.environ["CORE_CI_TOOLS_SHA256"], selected, environment) before verify_manifest; rename the
   sdk_archive parameter to tools; drop unused staging and the prepare import. Tool paths stay
   relative to the returned root. Replay via retained_receipt unchanged.
F. publish.py: rename --sdk-archive to --tools (argument, require message, pass-through). No
   other change.

Code rules: global single-final-return and no avoidable break/continue rules apply to Python
as well as C++. Changed production lines <= 160 characters. No C++ changes.

Tests (focused; count separately):
 - tests/openwrt.py: adapt publication_paths to patch verified_tools; add synthetic cases for
   publication_tools (recursive NEEDED, missing lib, slash in NEEDED, escaping symlink, internal
   symlink copied as regular file, bounds, tools.json fields) with sdk.output patched for readelf;
   verified_tools accept + rejections (wrong output hash, assignment/attempt/target/environment
   mismatch, extra/missing file, symlink, size/hash tamper, modes set only after verification,
   CORE_CI_APK for apk); sdk.main selector rejections (oversize, newline, non-object) before
   prepare, prepare called once, environment.sdk not called by main, changed environment rejected.
 - tests/worker_provenance.py: replace Retain/--sdk-archive assertions; assert tools job has the
   exact if, include-hidden-files: true, no environment:/secrets./concurrency:/always()/
   continue-on-error; publish has the exact if, unchanged name/push step/feed group, native-only
   apt; --tools present; worker core-ci-publish.yml bytes == bundle template bytes; existing
   publish/confirm assertions still hold; validate output emission bounded and OpenWrt-only.
 - Discover/run only affected worker-package-boundaries, openwrt-archive-boundaries,
   worker-provenance-boundaries and the actual format-check target. Broaden only for proven
   dependency impact; no full suite or unchanged C++ tests by default. Persistent builds stay
   under build/codex/<configuration>, temporary checks under the task directory. Report commands
   and results.
Local official-tools proof (read-only, no SDK executable run): using the two archives under
 /tmp/codex/build/core-ci/publication-wait-fix/sdk-closure/, verify hashes, extract into fresh
 directories with sdk.extract, run publication_tools (system readelf only), and compare file lists
 and byte totals with runtime-closure.json (IPK 8/2152850, APK 6/4082672). Report results.
 This is local proof, not acceptance: real signing/indexing and GitHub job conditions require a
 later, separately approved sandbox run.

Docs/log: minimal current-state updates where docs describe the retained SDK handoff (find them
by search); WORK_LOG.md entry with decisions, bounds, actual per-file added/removed/net, test and
proof results, and the disclosed trade-off (bulk transfers 4->3, all outside the lock; added serial
OpenWrt job hop and official download outside lock; small artifacts and pip still under lock).
No timing outcome claims.

Final report: production, test and documentation changes listed separately with line counts
against the approved bounds; local proof vs pending live acceptance; anything unverified.
Stop there; commit/push/deploy are separate, later approvals.
```
