# Contract X v2 scheduler verification for Claude

## Review revision

The code under review is signed commit `cd9b7ef1b36a9dc5ab7442ee46624a7e34c133e4` on `review/fixture-wip`. It includes the six-target stress case. The branch adds this report and extracts of the local CTest results. The earlier signed revision `6f984bd32714e339242fc9ba1370f44c6f57c7ac` contains the same daemon, contract, and GitHub/OIDC fixture. The base of the fix branch was `main` at `4f4b93ef33bef5055d76fc6a3423413cf747d334`.

The canonical checkout is `/home/voc/projects/snodec/core-ci`. The pinned §7 hash is `314288d853b7eade6091b80a695a5960dce619c83f9cfb57960acb3852633cb8`. The daemon counts 4,141 source lines plus 16 DDL lines, totaling 4,157 of 5,000.

## Results

| Revision and configuration | Result | Evidence |
| --- | --- | --- |
| Earlier fixture revision, two full loopback runs | 30/30 each | [Run 1](scheduler-proof-repair-offline-1.log), [run 2](scheduler-proof-repair-offline-2.log) |
| Earlier fixture revision, package test selection | 28/28 | [Log](scheduler-proof-repair-package-scope.log) |
| Six-target revision, full loopback run 1 | 31/31 in 73.30 s | [Log](scheduler-six-targets-full-1.log) |
| Six-target revision, full loopback run 2 | 31/31 in 76.64 s | [Log](scheduler-six-targets-full-2.log) |
| Six-target revision, package test selection | 29/29 | [Log](scheduler-six-targets-package-scope.log) |
| Six-target revision, Clang 21 ASan and UBSan with leak detection | 24/24 in 71.75 s | [Log](scheduler-six-targets-asan-ubsan.log) |

The full runs used the Release daemon with SNode.C adapters enabled, real daemon processes, a disposable MariaDB, and a loopback TLS fake for GitHub and OIDC. Both runs used a network namespace with one interface, `lo`, and no IPv4 or IPv6 default route. The sanitizer build covered the scheduler and persistence test executable with adapters disabled. Package test selection used the native hook's `ctest -LE mariadb` scope.

## What the stress tests assert

- `X_admission_stress` admits five synthetic repositories, ten versions per repository, and two targets. It drains 100 items through seven slots. Six items are eligible at the first cutoff and must be picked in one fill. Late entries wait for the next pass, repeated admissions coalesce, and completion in reverse order preserves dispatch order and frozen requests.
- `X_active_arrival_stress` starts with five repositories, opens new cutoffs with builds already claimed, and adds seven more repositories across those cutoffs. Each of 12 repositories ends with 12 versions and six targets. The test runs independently at slot limits of 1, 7, and 20, for 864 items per limit and 2,592 attempts total.
- The six target IDs are `apk-aarch64`, `deb-amd64`, `deb-arm64`, `deb-riscv64`, `ipk-mips64`, and `rpm-amd64`. These are synthetic catalog entries. The two ARM-runner entries use the ARM request prefix and the other entries use the standard prefix.
- The test derives expected order from the sequence of distinct admission calls and the declared target IDs. After each fill it checks the exact queued and running counts, the identities of the expected items, and the attempt count. It sends completion and finished reports in both orders, completes active items out of admission order, and periodically reloads serialized state. Each claimed request must retain its exact document bytes and each scenario must end with zero reservations. [Stress test source](../tests/X_Scheduler.cpp#L266)

## Scheduler and adapter coverage

| Behavior | Tests and asserted outcomes |
| --- | --- |
| Pass cutoff and discovery | `X_turns`, `X_registry_arrivals`, `X_active_arrivals`, both stress cases, and `X_Http` cover single-fill capacity, older ready work, arrivals before and after the cutoff, numeric tag ordering, explicit admission of older versions, idempotent discovery, and retag conflict. |
| Declarations | `X_admissions` verifies normalized abbreviated endpoints, five comparison operators, conjunctive ranges, and an unsupported tag with its reason. `X_Http` rejects an invalid declaration through Reconcile, ignores a prerelease, discovers missed stable tags, and re-reads a changed CI tree on a default-branch webhook. The rebuild keeps the source commit and advances its revision. |
| Dependencies and per-target disposition | `X_binding`, `X_closure`, `X_active_arrivals`, and `X_targets_and_receipts` cover waiting for a newer eligible prerequisite, fallback to a retained output after failure, cycle detection, closure conflict versus pending refresh, unavailable targets, an unsatisfiable range, and a failed prerequisite. One target can fail while a second publishes and resolves a consumer in that same target. |
| Outcome cutoff | `X_targets_and_receipts` commits a provider's publication receipt during an open pass. The dependent consumer stays enrolled in that pass and binds the output when it becomes eligible. |
| Reservation accounting | `X_attempts`, `X_observation`, `X_handoff`, `X_publications`, `X_publication_retry_capacity`, and `X_Http_Recovery` cover normal fills, release at observed completion, publication priority, batching within a reservation, superseding at capacity, late claims, and a known run awaiting observed termination. Publication recovery waits for capacity and takes the next free slot before a queued build. |
| Identity and retries | `X_identity`, `X_retry_identity`, `X_revision_floor`, `X_expiry`, and `X_expiry_before_turn` cover source, declaration, target, contract, and binding identity inputs. They check same-revision byte equality, changed-input revision allocation, stale dashboard Retry rejection, no reallocation on duplicate admission, and refresh limited to the newest published consumer when bindings change. |
| Revision floors | `X_revision_floor` supplies two cutover floors with the larger value coming from the old database for one key and from the ledger for the other. The scheduler consumes the supplied floor and advances it on later picks. The old database and feed extraction procedure is outside this fixture. |
| Publication, retention, and snapshots | `X_publications`, `X_snapshots`, `X_retention`, `X_retiring_binding`, `X_backoff`, `X_targets_and_receipts`, and `X_Http` cover receipt recovery and acknowledgement, mixed quarantined and successful items, retry backoff, expiry, retirement-only dispatch, protection of active and retry-eligible pins, three rollback refs, and direct and transitive retirement exclusions. |
| HTTP ownership and monotonic outcomes | `X_Http` verifies HMAC and the hash of the exact frozen request bytes, rejects a foreign claim, foreign report, and rerun claim, and accepts repeated owner claims. A finished build remains built after a later conflicting finished report. |
| Observation and deadlines | `X_observation`, `X_attempts`, and the two HTTP cases exercise queue, execution, and callback-grace timing, lost dispatch response, run-name discovery, known-run cancellation, and failure after each modeled GitHub conclusion without a report. |
| Durable transitions | `X_persistence` uses an in-process store to simulate losing acknowledgement of a committed write. `X_Http` and `X_Http_Recovery` separately use real MariaDB and restart real daemon processes. |
| Adapter checks | `A_WebhookHmac`, `A_CallbackHmac`, `A_TagPages`, `A_AppToken`, and `A_SlimSource` cover signature and timestamp checks, webhook parsing, Link-header and tag parsing, installation JWT/token behavior, and the daemon source budget. |

The initial offline proof caught three failures on the earlier daemon. New repositories could defer an unrelated eligible item, publication retry could exceed the slot limit, and an expiry receipt could reuse a stale revision after a declaration change. Later regressions reproduced premature closure readiness, a stale explicit Retry, older consumer refresh during a newer rebuild, retirement-sensitive picks, an incorrect blocked reason, and publication output visibility before its eligibility. The green runs above include the corresponding regressions. [Initial reproductions](scheduler-proof-offline.log), [binding and retirement reproductions](scheduler-binding-siblings.log), [publication cutoff reproduction](scheduler-publication-cutoff.log)

## Actual daemon restart cases

The HTTP helper saves the dashboard's item IDs, request IDs, attempt and run IDs, item states, pause status, reservation counts, and publication summary. It also saves raw public request or plan bytes. The daemon process is terminated and started again against the existing MariaDB. After the service is ready, the test compares the saved data and checks the fake dispatch queue for duplicate work.

| State at restart | What the fixture controls or observes |
| --- | --- |
| Built awaiting publication and paused | The owner dashboard pauses dispatch before observed build completion. The item becomes built and the publication list stays empty. Built state and pause persist across restart. |
| Publication batching | The daemon reserves a publication during a ten-second batch. The plan endpoint still returns 404. After restart, the eventual dispatch uses the same publication ID and attempt 1. |
| Publication completed but unreconciled | The fake holds feed-head reads while the fake run completes. After restart, publication state and bytes match. Feeding the snapshot and receipt reconciles the original publication. |
| Superseded attempt | An initially missing build has already been replaced through run-name lookup and the queue deadline. The old document remains available, claiming it yields 409 on both sides of restart, and the replacement keeps its slot. |
| Cancellation requested | The fake acknowledges cancellation while leaving the known run active. Restart preserves the attempt and reservation. Observed completion and callback grace later release the slot and fail the item. |
| Active claims and pause under new arrivals | Registry and tag additions are processed while a build is claimed and dispatch is paused. Restart preserves the build, its frozen bytes, the admitted items, and pause. A free slot after unpause goes to the oldest ready item. |

[HTTP test and restart assertions](https://github.com/SNodeC/core-ci/blob/6f984bd32714e339242fc9ba1370f44c6f57c7ac/tests/contract_x.py)

## Fake GitHub behavior and review targets

The fake is a Python `ThreadingHTTPServer` with a fixture-generated TLS certificate. It serves registry, catalog, repository metadata, default-branch commits, CI trees, tags, workflow dispatch, run observations, cancellation, feed refs, snapshots, and ledger content. Dispatch asserts the main ref, input keys and input types. The usual simulated response contains a run ID. One publication returns 204 while retaining a findable run. The lost-build case returns 204 with no run in the fake. Cancellation can be acknowledged separately from later completion. Tests change these facts explicitly. [Fake server](https://github.com/SNodeC/core-ci/blob/6f984bd32714e339242fc9ba1370f44c6f57c7ac/tests/dashboard/fixture.py)

The fake verifies the application JWT signature and relevant claims at token mint, then returns a fixture installation token. It uses signed callback requests and a local OIDC flow for dashboard actions. The HTTP tests exercise the daemon side of the protocol. Their build and publication results are synthetic; product compilation, publisher signing, atomic Git pushes, and T1–T8 staging evidence are separate validation.

Review the following as **candidate gaps**, with production and fixture line references and a deterministic event sequence for each confirmed finding.

1. **Commit-consistent feed reads.** The fake serves a single mutable ledger. Compare that with the daemon requesting a ledger for a specific head. Test an old head or snapshot response that was started before a push and delivered after completion. Check that a stale response cannot cause a retry or hide a valid receipt.
2. **Run-name pagination and delayed visibility.** The fake run list returns all matching rows it knows in a single response. Link parsing has a separate unit test. Exercise a match on a later page, an ambiguous dispatch that becomes visible after a lookup, and a completed or rerun row next to the expected run.
3. **Overlapping dispatch and lost acknowledgements.** Check same-name duplicate rows, a callback arriving before the dispatch response, and a stop after submitting dispatch but before seeing its result. The unit store models a lost SQL commit acknowledgement. A restart in that timing window against the actual MariaDB transport would provide different evidence.
4. **Known-run cancellation and observation errors.** The existing recovery test holds the run state after cancellation is accepted. Check how repeated observations, delayed completion, an API error, or a stale observation affect the reservation and avoid a second attempt.
5. **Fixture fidelity and determinism.** Review the fake's shared run counters and maps, ordering across request threads, time-based waits, authorization checks, and representative payload shapes. Identify assumptions that make a failing production schedule impossible in the fixture. Distinguish a confirmed discrepancy from a scenario proposed for testing.

Start at the fake server and HTTP tests above. Trace the corresponding production paths through `app/App.cpp` methods `send`, `observe`, `locate`, and `refreshFeed`, then `core/Scheduler.cpp`, `app/ApiRouter.cpp`, `app/WebhookRouter.cpp`, and `adapters/github`. Review against `SPEC.md` §7 and `IMPLEMENTATION-X.md` §3 in the same revision. Keep the review read-only and use the local evidence to reproduce findings. [Production app](https://github.com/SNodeC/core-ci/blob/6f984bd32714e339242fc9ba1370f44c6f57c7ac/app/App.cpp)

## Local reproduction

From the canonical checkout with the SNode.C adapters configured in the Release build, the full and package-scope commands used this setup. Run the full command again for a fresh disposable database.

```bash
build=/home/voc/projects/snodec/build/codex/core-ci/contract-x-v2/adapters
sdk=/tmp/codex/artifacts/snode.c/v3-impact-inventory/stage/usr/lib
bwrap --bind / / --dev /dev --proc /proc --unshare-net --die-with-parent \
  --chdir "$PWD" env LD_LIBRARY_PATH="$sdk:$sdk/snode.c/web/http" \
  PYTHONDONTWRITEBYTECODE=1 bash -ec '
    ip -brief addr
    test "$(ip -o link show | wc -l)" = 1
    test -z "$(ip route show default)"
    test -z "$(ip -6 route show default)"
    cmake --build "$1" --parallel 3
    python3 tests/database.py "$1"
    ctest --test-dir "$1" -LE mariadb --output-on-failure
  ' bash "$build"
```

The sanitizer run configured `CORECI_WITH_SNODEC=OFF`, Clang 21, `-O1 -g1`, `-fno-omit-frame-pointer`, and `-fsanitize=address,undefined` for compile and link. It enabled leak detection and aborted on sanitizer errors. The normal and sanitizer logs above list every CTest outcome.
