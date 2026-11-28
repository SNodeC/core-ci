# Contract X v2 follow-up review

The core-ci source under review is signed commit `fcc7d8383563aded4c56f3dee61cdd17f9b223a4` on `fix/x-scheduler-proof`. The companion Packages changes are signed commit `61040ba` on `fix/x-restore-review`. The previous review snapshot was `28b2b36`. The source in this review includes the six-target admission test accepted in that round.

The two owner-approved restore amendments are in commit `097d762`. The staged §7 SHA-256 verified before that commit is `b07b30c1a78d7b4f526f3c911d0ff39d58c0f206bb83a6b2376e21c7900f19bf`. T7 evidence now includes restored and paused display after each restore, and checks for duplicate republication after the final unpause.

## Defects and verification

| Item | Before the fix and what the regression checks |
| --- | --- |
| A1 definite rejection | An HTTP 422 left a run-less reservation in processing. Four sends of the same frozen attempt now produce `dispatch rejected 422` and release the reservation. `X_Http_Dispatch_4xx`. |
| A2 unknown dispatch result | A 503 or a closed connection produced one POST and a long queue wait. The persisted backoff schedules the same attempt and document. A later claim or known run keeps ownership. An earlier ambiguous failure prevents classifying later 4xx responses as proof that no run exists. `X_Http_Dispatch_5xx`, `X_Http_Dispatch_Connection`. |
| A3 process death | The fixture enforces distinct run titles for the test lifetime, with explicit exceptions for deliberate duplicates. A MariaDB proxy holds the commit acknowledgement so SIGKILL can land after the pending pick or sent transition. A third case kills after a committed claim. Stored documents, attempts, and reservations are checked after restart. `X_Http_Kill_Pending`, `X_Http_Kill_Sent`, `X_Http_Kill_Claimed`. |
| A4 paginated discovery | The fake honors page size and Link. Discovery admits a tag from page 2 and finds the existing run on page 3. A foreign origin Link is rejected. `X_Http_Pagination`. |
| A5 feed consistency and restore | The prior feed transition accepted the observed head even before its snapshot was visible. A newer unpinned head now holds publication reconciliation, including if the next read returns an older head. Ledger order uses the publication ID recorded even for a retirement-only push. A backward head needs a successful restore job, an exact branch and ref match, and completion after the last accepted change. The fixture models a production pre-X ref restored to staging, failed and mismatched evidence, stale success, and the return to the newest snapshot. `X_Http_Restore`. |
| A5 restored artifacts | An available original artifact can be republished at the same revision. After the publisher confirms expiry of a previously published artifact, the scheduler builds the next revision from a fresh feed pin. The previous request remains immutable even if its snapshot ref was deleted. A never-published artifact retains the same-revision expiry behavior. `X_restore_expiry`, `X_expiry`, `X_expiry_before_turn`. |
| A6 claimed cancellation | A finished `cancelled` report leaves the active reservation until GitHub completion is observed. The item finishes with reason `cancelled`, and a rerun cannot overwrite it. `X_Http_Claimed_Cancel`. |
| A7 rerun race and pending publication | Polling the latest run previously hid attempt 1's cancellation if attempt 2 began between polls. Observation now reads attempt 1. A duplicate pending publication in the same concurrency group cancels the original pending run, fails to claim work, and the scheduler converges through one backoff retry. `X_Http_Rerun`, `X_Http_Publish_Pending`. |

[Original dispatch reproductions](review-dispatch-regressions.log) · [Dispatch verification](review-dispatch-fixed.log) · [Original rerun reproduction](review-run-attempt-regressions.log) · [GitHub run-attempt endpoint](https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run-attempt) · [GitHub attempt-jobs endpoint](https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt)

## Dependency stress and cutover rehearsal

`X_dependency_stress` runs a 12-repository graph with chains and diamonds, two versions and two synthetic targets, at each of slot limits 1, 7, and 20. It checks all 144 final item dispositions across the three scenarios. For every fill it checks the slot cap, publication priority, each declared direct dependency in the frozen closure, publication of every bound output for that target, and unique package identities in a closure. The second version wave waits on newer inputs despite usable older entries. It injects superseding, an execution deadline, a manual retry, artifact expiry, and a publication with successful and quarantined items. The original independent-load case separately covers 2,592 attempts with six targets.

The cutover floor procedure in `tests/revision_floor.py` was run twice from verified copies of the real schema-7 SQL backup and the production ledger at `refs/snapshots/feed/pre-x` `9fdc1f9f06a3f7724bbc94b96d423a7aebabb5e8`. It verified 635 floors, with highest floor 9. Both outputs were byte-identical and matched the independent staging seed for every key. A mismatched source hash failed before restoring a database. The floor JSON SHA-256 is `97fe52d3fb8c8c4f7d2be6ff00c7117f3b2fb6482f602b3e59a2501863656b57`. The exact floor values and source copies are recorded in the local step 5d evidence.

## Final local results

| Check | Result |
| --- | --- |
| Full Release suite with loopback fake and disposable MariaDB | 44/44 twice, 185.98 s and 186.59 s |
| Package test selection with MariaDB cases excluded | 31/31 |
| Clang ASan, UBSan and leak detection with adapters disabled | 26/26 |
| Packages build, publisher and workflow fixtures | 22/22 |
| Daemon source budget | 4,358 source plus 16 DDL, 4,374 of 5,000 |
| Packages tools | 1,000 Python lines |

[Full run 1](restore-review-full-1.log) · [Full run 2](restore-review-full-2.log) · [Package scope](restore-review-package-scope.log) · [Sanitizers](restore-review-asan-ubsan.log) · [Packages fixtures](packages-restore-final.log)

The daemon suite uses the local TLS fake under a loopback-only namespace. Package tests use fixture repositories and signing keys. These results cover the scheduler, recovery and workflow fixtures. Staging qualification remains a separate phase.
