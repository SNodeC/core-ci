# core-ci slim — implementation and qualification record

The canonical checkout is /home/voc/projects/snodec/core-ci, remote
git@github.com:SNodeC/core-ci.git, default branch main. The implementation branch
is refactor/core-ci-slim. No production access, deployment, service activation,
system installation or tags are part of this task.

## Gate and contract

The owner-authorized atlas move preserved the untracked docs directory unchanged
at /home/voc/projects/core-ci-rest-atlas-20261113. Its top-level names were
core-ci-github-rest-diagrams.zip and core-ci-github-rest.html. The destination did
not exist and nothing was overwritten. After the move the checkout was clean,
on main, pulled, with HEAD 0dad3da51221b35648fe004d7f4b203f411d7799
(the required merge). The unrelated branch chore/owner-pending-core-ci remains
at d17dfaada853d480f4fe823ae47df72cb80badf2.

SPEC PR: https://github.com/SNodeC/core-ci/pull/69, title "spec: core-ci slim".
Its merge is 559216b412d010da8962cc2aafa5b431a5953df3. Implementation
clarifications accompany the implementation PR so its merged code and SPEC agree.

The architectural invariant is that the event-sourced engine owns workflow state.
The dashboard projects recorded events; GitHub supplies declarations and accepts
dispatches, without a parallel run/job state authority. Graph definitions and
their selected source releases enter the engine atomically. Authentication has
one GitHub App token authority, replacing configured static GitHub credentials.
Old persisted histories are unsupported; schema 4 requires a fresh database.

## Baseline versus implementation

Physical lines include comments and blank lines. Production counts include all
tracked implementation files in the indicated directories, including SQL and
configured header templates. Dashboard assets are separate from app/. Baseline
is 0dad3da51221b35648fe004d7f4b203f411d7799.

| Area | Baseline | After | Delta |
|---|---:|---:|---:|
| core/ | 1314 | 1291 | -23 |
| persistence/ | 843 | 749 | -94 |
| adapters/ | 1653 | 1746 | +93 |
| app/ excluding dashboard assets | 1392 | 1115 | -277 |
| app/dashboard/ assets | 531 | 496 | -35 |
| **Production total** | **5733** | **5397** | **-336** |
| Test C++/Python/JavaScript | 6073 | 5875 | -198 |
| All test files, including fixtures | 15883 | 9119 | -6764 |

Adapters grow because RSA signing/token renewal and tag-bound declaration loading
replace the smaller deleted job lookup there; the complete production change
reduces both lines and conceptual state. Initial code-only accounting excluded
SQL/templates: 5651 to 5306 (-345); its adapters count was 1581 to 1665 (+84).
Documentation and CMake/tooling are outside these production/test totals.
InMemoryEventStore is now test-only. The obsolete 6562-line job model fixture
and removed-feature tests are deleted, while retained behavior has functional
coverage.

| Surface | Baseline | After |
|---|---:|---:|
| Inbound route shapes | 17 + optional redirect | 10 + optional redirect |
| Outbound GitHub endpoint shapes | 8 | 6 |
| Non-secret configuration keys | 25 | 29 |
| Production CORECI environment variables | 5 | 3 |
| Service-template XDG environment variables | 3 | 3 |

Remaining inbound routes (complete list):

- POST /api/v1/github/webhook
- POST /api/v1/builds/{buildId}/report
- GET /api/v1/status.json
- GET /api/v1/badge/{package}/{target}.svg
- GET /
- GET /style.css
- GET /app.js
- GET /events
- POST /api/{operation}
- GET /auth/callback
- Optional separate HTTP-to-HTTPS 308 listener.

The first four are HMAC-protected machine endpoints or public output as specified;
all six dashboard routes use IDP login and the subject allowlist, with the login
callback performing the OAuth exchange. Supported operations are retry, rebuild,
reload and reconcile. Removed route shapes: GET /api/v1/status,
GET /api/v1/graph.dot, POST /api/v1/units/{package}/{target}/retry,
POST /api/v1/units/{package}/{target}/rebuild, POST /api/v1/reload,
POST /api/v1/reconcile, and GET /favicon.ico. Method guards do not add route shapes.

Remaining outbound GitHub endpoints (complete list):

- POST /app/installations/{installation_id}/access_tokens
- GET /repos/{owner}/{repo}/contents/{path}?ref={commit}, raw
- GET /repos/{owner}/{repo}/tags, paginated
- GET /repos/{owner}/{repo}, canonical repository/default branch
- GET /repos/{owner}/{repo}/commits/{branch}, registry HEAD or no-release bootstrap
- POST /repos/{owner}/{repo}/actions/workflows/{file}/dispatches

The baseline had the five repository shapes above, plus
GET /repos/{owner}/{repo}/actions/runs,
GET /repos/{owner}/{repo}/actions/runs/{id}, and
GET /repos/{owner}/{repo}/actions/runs/{id}/attempts/{n}/jobs.
Those three are deleted; the installation-token endpoint is new.
Links to recorded runs are dashboard links, never API lookups.

Baseline configuration keys: registry, callback-base-url, github-api-url,
github-ca-cert, db-host, db-port, db-socket, db-user, db-name, build-timeout,
publish-timeout, retry-delay, max-dispatch-attempts, max-concurrent-builds,
reconcile-interval, status-interval, listen-address, listen-port, tls-cert,
tls-key, idp-base-url, idp-client-id, idp-public-key-path, dashboard-users,
dashboard-redirect-port.

After: remove status-interval; add target-catalog, workflow-repository,
github-app-id, github-installation-id, github-app-key-file. All other baseline
keys remain. Registry is owner/repository:path; target-catalog is another relative
file path in that repository. Build/publisher registry values are workflow
filenames in workflow-repository. No repository name is built in.

Baseline production environment: CORECI_ADMIN_TOKEN, CORECI_CALLBACK_SECRET,
CORECI_DB_PASSWORD, CORECI_GITHUB_TOKEN, CORECI_WEBHOOK_SECRET.
After: CORECI_CALLBACK_SECRET, CORECI_DB_PASSWORD, CORECI_WEBHOOK_SECRET.
Service-template XDG_CONFIG_HOME, XDG_STATE_HOME and XDG_RUNTIME_DIR are unchanged
(three additional declared runtime variables; total 8 to 6). Test-only variables
are not production configuration.

## Remaining optional areas

These physical attributable lines include owned includes and configuration, but
exclude CMake wiring and shared HTTP/HMAC/SSE machinery. Shared code is identified
separately to avoid implying that removing one route removes shared functionality.

| Optional area | Attributable lines | Accounting |
|---|---:|---|
| Badges | 93 | core/Badge.h/.cpp 83; route 9; include 1 |
| Public status.json route | 12 | route 11; include 1 |
| HTTP-to-HTTPS redirect | 17 | option 1; optional-setting override 1; validation 4; listener 9; member/include 2 |
| Webhooks | 95 | Push contract 5; parser 23; delivery dedup 6; handler 46; authentication 9; route 4; member/include 2 |

The status projection/writer/view code is another 191 lines shared with the SSE
dashboard. Removing only the public status.json route does not remove those lines.
No further optional-area removal has been performed.

## Decisions

- Decision: apply the owner's explicit SPEC rewrite and full-task authorization
  because it supersedes the repository's older hash-script-only SPEC and
  phase-stop instructions. Do not modify AGENTS.md.
- Decision: configure target-catalog and one workflow-repository, with workflow
  filenames in registry entries, because repository ownership must have one
  configuration authority.
- Decision: use a default-branch declaration only when a source has no accepted
  release, to display NoRelease units. Every selected release reads its own tag
  commit; no default-branch declaration change triggers a rebuild.
- Decision: reload and reconcile use the same tag-bound loader because a second
  traversal would duplicate release/declaration policy.
- Decision: retain configured tag/declaration reconciliation as workflow
  management, because it recovers missed webhook observations. The dashboard
  timer performs no GitHub I/O; Actions run/job polling is deleted entirely.
- Decision: GraphChanged atomically records its selected releases, because
  changing the graph before applying tags could dispatch a new unit with the
  previous release. Fresh schema 4 stores those typed release rows.
- Decision: reject a load that loses or regresses an already recorded release or
  replaces its tag with an equal-version alias, including dormant packages for
  the same source, because its declaration cannot be paired with the engine's
  retained newer release. Keep the current graph and release together.
- Decision: rebuild requires a settled unit, including settled prerequisites,
  because that is the requested dashboard operation contract.
- Decision: the startup-resumption smoke test kills its disposable process
  abruptly, because graceful termination may legitimately persist a transport
  rejection and its existing backoff before exit.
- Decision: preserve the existing explicit formatting/check targets and add the
  three missing byte-identical canonical CMake modules, because including the
  generic src-discovery modules would duplicate targets and miss this layout.
  The repository forbids editing AGENTS.md; this record documents that integration.
- Decision: build the current clean snodec-auth client into a temporary prefix,
  because the installed client lacks the API already required by the retained
  IDP integration. No dependency sources or system installations are changed.
- Decision: use monotonic elapsed time for SSE keep-alives and session checks,
  starting the check after 59 seconds on the existing one-second tick, because
  rounded wall-clock seconds can exceed the retained authentication timing
  test's tolerance. No additional timer or GitHub polling is introduced.

## Qualification

Build: GCC 16.2.0, C++20, RelWithDebInfo, OpenSSL 3.6.5, nlohmann-json 3.11.3,
Python 3.14.10, Chrome 156.0.8078.79. SNode.C comes from its installed package.
The auth client was built from clean local revision
b0b1c8c702a6f80319b1799a996fac8d87f1b7a9, client-only, under
/tmp/codex/build/snodec-idp/core-ci-slim with its prefix inside that directory.
The controller build is /tmp/codex/build/core-ci/slim-qualification; compiler
temporaries stay in its tmp subdirectory. No extra checkout or source copy exists.

Configuration uses CORECI_WITH_SNODEC=ON, CMAKE_BUILD_TYPE=RelWithDebInfo,
snodec-auth_DIR at that temporary prefix's lib/cmake/snodec-auth, and
CORECI_PLAYWRIGHT_PATH at the runtime's playwright-core package.
Full build and format-check pass. The pure CORECI_WITH_SNODEC=OFF build also
passes for core, persistence and status. Canonical formatting files/modules are
byte-identical to the supplied baseline. No sanitizer or clang-tidy run is claimed.

Full CTest uses a disposable MariaDB at 127.0.0.1:13316, database coreci_slim,
with its data/socket under /tmp/codex/build/core-ci/slim. It does not use or
modify the separately running system database. Test environment:
CORECI_TEST_DB_HOST=127.0.0.1, CORECI_TEST_DB_PORT=13316,
CORECI_TEST_DB_USER=root, CORECI_TEST_DB_NAME=coreci_slim,
PYTHONDONTWRITEBYTECODE=1, and TMPDIR inside the qualification build.
Command: ctest --test-dir /tmp/codex/build/core-ci/slim-qualification
--output-on-failure --parallel 4 --output-log <run-log>.

- Final run 1: 38/38 passed, no failures or skips, 258.53 seconds.
- Final run 2: 38/38 passed, no failures or skips, 257.71 seconds.

Two preceding full runs passed 38/38 in 258.85 and 257.65 seconds. Final review
then found the missing/older-tag declaration mismatch; qualification restarts
on its correction rather than claiming the preceding runs validate later code.

Earlier trial runs exposed a fixture writer-lock output assumption, graceful
shutdown versus crash semantics, and the SSE timer rounding failure. These were
corrected at their source; failing trials are not counted as qualification runs.
The retained 60.5-second authentication assertion was not weakened.

Coverage includes cryptographic JWT verification with a test key pair; cache
renewal boundary/key reread/one 401 retry; source static-credential scan; tag-commit
raw fetches and pagination; atomic registry add/remove without settled rebuild;
publish-start history/replay/binding; phase start/end/state/attempt links;
removed-route 404s; HMAC/TLS/IDP/subject checks; write-ahead errors and recovery;
dispatch/backoff/deadlines/automatic retry/races; real browser SSE operations;
missing/older/equal-version tag rejection and dormant reactivation;
MariaDB replay/locking; and 10000 randomized invariant scenarios per full run.
GitHub behavior is tested through loopback fixtures, without a production App.

## Owner setup and handover

The owner must later create a GitHub App: Actions write and Contents read on the
workflow/registry repository; Contents read and Metadata read on source
repositories; webhook event push; installed on all those repositories. Supply
the App id, installation id and an operator-protected RSA PEM through the new
configuration keys. SNodeC/Packages can be configured for the new slim CI; it is
not a built-in name.

The implementation PR and its eventual merge SHA are recorded in the final
handover. Task branches will be deleted after authorized merge, and the canonical
checkout left on main, pulled and clean. Disposable test processes/build artifacts
will be cleaned after qualification; final CTest logs are retained under the
ignored build/codex/core-ci-slim-results directory for review (removable after
review). The moved REST atlas is preserved.
