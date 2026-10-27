# Controller parallelism corrections — implementation and local verification

## State and authority

Checkout: `/home/voc/projects/snodec/core-ci`. Branch: `fix/controller-parallelism`.
Starting and final HEAD: `63ab7b207f55afb5a71c7b2f54d76fa05ee7ebef`; the remote-tracking branch matched at inspection.
Reviewed base: `4257cb0fc67412da8696afdba307fc2e85ff64aa`.
The corrections are uncommitted. No push, merge, installation, deployment, live restart or activation was performed.
The original four decisions and cumulative +310/+790 approval cover this implementation; later stages were not inferred from that approval.

The pre-existing public documentation changes, review records and improvements were preserved.
This task only appends its WORK_LOG entry and changes the two identified deployment passages.
Private directories were neither inspected nor modified. No production SNodeC resources, credentials, keys, tags,
feed bytes or deployed state were touched. No live GitHub mutation was made.

The invariant is that lazy graph derivation retains its caller's response contract, and each HTTPS operation
releases its existing capacity and callback correctly without changing dispatch authority or mutation uncertainty.
One graph, Database FIFO, event loop and state machine remain authoritative. No production mechanism was added.

## Findings and source references

| Finding | Disposition and source |
| --- | --- |
| M1 | `docs/deployment/README.md:227` explicitly describes first installation. The upgrade instructions at line575 require O publisher/status bytes to remain identical in the N tooling commit; N aliases are rendered in a separate wrapper commit. No documentation audit was performed. |
| H1 | `src/HttpClient.cpp:163` finishes DISABLED with status0, submitted=false and a definite unsubmitted error. Intermediate establishment errors retain existing handling. The direct public HttpClient fixture asserts one asynchronous callback and zero requests. |
| L1 | `deployment/worker/core_ci/publish.py:184` validates feed for prepare/push and build-result/artifacts for prepare immediately after parsing, before assignment or trust processing. Confirmation needs neither; push does not require unused inputs. |
| L2 | `src/HttpClient.cpp:196` exchanges the callback with nullptr. No delivered flag or line-mirroring test was added. Existing callback-count tests remain. |
| L3 | `src/HttpClient.cpp:226` gives unstarted queued work the existing unsubmitted-shutdown message before finish. Submitted operations are not relabelled. |
| L4a | `src/Pipeline.cpp:635,747,778,811,916` and `src/Pipeline.h:90` restore preparation-error classifications and bodies through the existing helper. Its asynchronous callback captures the PipelineResult by value. Two obsolete dispatch catches were removed. |
| L4b | No change. `Database::write` rejects producers while mutation uncertainty is latched, before derivation. `GraphStore::commit` retains failed-mutation SSE. Internal unchanged/no-work outcomes can be503/uncertain=true; the authenticated external webhook acknowledgement remains202. |
| L4c | No change. `Graph::blockers` handles a verified built result as publication work; `Pipeline::advance` calls beginBuild only for waiting nodes. A built node with free feed capacity remains publishable. |
| T1 | Focused public-boundary tests were added below. A pre-existing duplicate local variable in GraphTest prevented rebuilding that target; the fixture map was renamed without removing assertions. The rebuilt graph test passed. |
| I1 | No change. `src/Database.cpp:65` releases the FIFO before a read callback. This permits initial-failure teardown; write completion retains its different ordering. |

### L4a actual preparation responses

All rows add the actual error and `uncertain:false`. SQL/latched mutation failures keep503 with their actual uncertainty.

| Site | Status | Additional preparation body |
| --- | --- | --- |
| record / bindRun |409|assignment_preserved:true|
| buildArtifacts missing/expired failure |503|execution ID; assignment_preserved:true|
| buildArtifacts built-result acceptance |503|execution ID; assignment_preserved:true|
| publicationResult receipt application |409|assignment_preserved:true|
| rejectedDispatch, complete-failed, before-push-failed |409|Existing default body|
| commitSelection / retry / advance |409 /409 /422|Existing handling|

No no-op execution-ID contract was introduced. Default null helper bodies become objects when error is assigned.
The removed catches were the workflow-approval rejection and HTTP dispatch-rejection wrappers in `Pipeline::dispatch`.
Those catches surrounded derivations that now execute inside the Database FIFO and could not catch their failures.

Retained catches guard actual synchronous validation:

- request/readSources (`Pipeline.cpp:62,101`): selection input, selected definition/tag/deployment evidence.
- event (`162,224`): unrelated identity candidates and authenticated event JSON/identity parsing.
- workflow (`384`): returned commit/root-tree fields.
- inspect (`496`): current assignment/attempt and REST lookup setup.
- observe (`593,615`): job/run/actor/workflow/attempt identity, timestamps and push-step fields.
- complete (`697`): bounded job listing and selected-job evidence; existing concurrent-evidence handling remains.
- buildArtifacts (`784,788`): current attempt, job and artifact metadata, identity, digest, bounds and expiry.
- publicationResult (`824`): late feed result versus current assigned attempt and verified receipt application setup.
- search (`874`): run-page shape, actor matching, pagination bounds and ambiguity.

## Verification

Build directory: `/tmp/codex/build/core-ci/controller-parallelism/build`.
New evidence: `/tmp/codex/build/core-ci/parallelism-corrections/`.
The existing findings directory was not overwritten. Task-controlled compiler TMPDIR remains inside corrections/tmp.

Final build-input identity: HEAD63ab7b2 plus SHA256
`364d21faa549290a3532b3bf1fdb3db93d7b93d8fdc1a6b00acb81ab0169fd84`.
This hashes the binary dirty diff of src/, deployment/worker/, CMakeLists.txt and tests/ against HEAD;
document-only changes are excluded from build inputs. Full tracked dirty-diff identity is also saved in the evidence directory.

Actual commands (B is the build directory above; E is corrections):

```sh
cmake -S . -B "$B"
TMPDIR="$E/tmp" cmake --build "$B" -j2
cmake --build "$B" --target format-check
ctest --test-dir "$B" -R '^(lifecycle-help|lifecycle-initial-failure|lifecycle-unmanaged-initial-failure|lifecycle-schema-failure|enrollment-boundaries|database-start-failure|database-rollback-failure|graph-state-boundaries|github-transport-.*|pipeline-boundaries|worker-package-boundaries|worker-provenance-boundaries)$' --output-on-failure
ctest --test-dir "$B" -R '^github-transport-.*$' --output-on-failure
ctest --test-dir "$B" -R '^(database-start-failure|database-rollback-failure)$' -V
ctest --test-dir "$B" -R '^pipeline-boundaries$' -V
ctest --test-dir "$B" -R '^graph-state-boundaries$' --output-on-failure
python3 tests/lifecycle.py --executable "$B/core-ci" --directory "$E/uif" --case unmanaged-initial-failure
git diff --check
git diff --check 4257cb0
```

- The selected27 checks all executed independently:26 passed in CTest. The unmanaged-initial-failure fixture
  exceeded Linux's107-byte Unix socket path limit in the reusable build path. Its exact error was
  `The socket file path is too long (> 107)`; the unchanged test passed using the shorter permitted E/uif directory.
  The fixture now uses a shorter socket filename; all three registered selected startup checks subsequently passed.
- Rebuilt final targets succeeded, including graph-test. Earlier graph results from an older binary are not validation.
  `verified-final-build.log` and `verified-graph.log` are the controlling evidence for that correction.
- Final transport16/16 passed after the direct disabled-client fixture was added.
- Final database fault2/2 passed with actual captured COM_QUERY text `START TRANSACTION` and `ROLLBACK`.
- Final pipeline passed after the TCP-connect stall and artifact-overlap assertions were added.
- Worker package/provenance, enrollment, help and other selected startup checks passed.
- Canonical format-check and diff whitespace checks passed. Every changed production addition is <=160 characters;
  changed C++ retains braces, one final return per function and no added break/continue.
- No full suite was run. Passing checks were repeated only after affected source/test changes or to resolve a failed fixture.

### New and extended coverage

1. Direct disabled HttpClient: callback after submission call returns, exactly once; status0; submitted=false;
   definite message; no token or repository requests. Capacity1 sends12 correlated operations using one token with peak1.
   Capacities0/17 reject. Existing capacity8/reverse responses, expiry, TLS identity and mutation non-replay remain.
2. Queued shutdown: capacity1 has one submitted and11 queued operations. All12 callbacks arrive in this fixture;
   queued requests have the definite unsubmitted message. The submitted result remains status0/submitted=true.
   On this installed parser it reports `HTTPS response has an invalid status`, an existing error, rather than a literal
   unknown-outcome string. No production wording for submitted work was changed and no universal teardown callback guarantee is claimed.
3. Disabled feed-http: publishing assignment survives startup and two completed explicit reconciliations,
   demonstrating completionReads release. Public receipt bytes remain independently required.
4. Publication limits0/33 reject. Capacity1 visibly blocks another built target; definite rejection frees capacity.
   Existing default8, restore-under-lower-limit and join/prerequisite tests remain.
5. Worker malformed phase arguments are rejected before mocked assignment/identity/token/push entry points can be called.
   Existing confirmation without feed/artifacts and push without unused inputs remain exercised.
6. Two signed run claims queue while a real MariaDB graph_revision write lock is held. Both observe an unbound assignment;
   the first identity persists, the second gets internal409 with assignment_preserved:true/uncertain:false, and neither
   creates replacement dispatch. External responses are202. The error text is the same in observe and bindRun;
   contrary to the task's wording, text alone cannot distinguish them. The held-lock/unbound-state assertions establish the path.
   An accessible buildArtifacts503 also asserts its execution ID and assignment_preserved:true.
7. Delayed approval under same-state evidence commits dispatches the captured minimum and then reaches the latest generation,
   without duplicate unchanged-generation POSTs in the tested interval. No inference about remote job or Pages order is made.
8. Shutdown under stalled TLS, stalled HTTP response and an actual TCP SYN_SENT phase exits with SNode.C's normal
   SIGTERM code241 (-15 modulo256). Restart restores the same run with no replacement mutation.
   The SYN stall uses a full local accept backlog and verifies the socket state; no firewall/routing change is needed.
9. Database proxy rewrites one real targeted COM_QUERY to invalid SQL, recomputing the three-byte length and preserving sequence ID.
   START failure is definite/unlatched and the next write succeeds. Failed ROLLBACK latches uncertainty and blocks another producer.
   GraphStore::view reads memory only; independent root consistent reads inspect durable state while rollback left a transaction open.
   Shutdown closes it, permissions are restored, restart verifies explicitly that no graph revision leaked, then a write succeeds.
10. Existing successful rollback, lost-successful-COMMIT acknowledgement, graph evidence/built/advance ordering and concurrent
    real-MariaDB reports remain covered. A held artifact-list response overlaps duplicate completion; it produces one artifact lookup,
    retains the accepted result, completes publication assignment and waits for both callbacks before accounting requests.
11. Private user/mount namespace with two IPv4 hosts entries verifies actual AF_INET resolver order and connects through the second
    candidate after the first fails. It asserts the framework fallback diagnostic. No global DNS/hosts or framework file changed.

### Not runtime coverage / remaining limits

- Active-origin switching remains source-excluded: FeedProof requires selected/configured origins to match and configuration is fixed.
- Late orphan onConnect remains source-excluded by the traced status/onConnect adjacency. Neither is presented as a runtime test.
- Sanitizer/instrumented memory-safety testing was not run. Normal shutdown is not proof that all use-after-free possibilities are absent.
- Additional preparation-failure branches for built-result acceptance, missing-artifact mutation and receipt application were source-reviewed,
  not forced at runtime. Existing current-attempt checks and completion-read coalescing constrain those races; no valid public-fixture
  sequence was established that enters these lazy validation failures. No production hook or altered evidence was added to force them.
  The binding-race test covers a real preparation failure and its false uncertainty field; artifact503 metadata is checked separately.
- Latched unchanged-report/no-work503/SSE behavior was confirmed in source, not separately exercised end-to-end under a rollback fault.
- Test-side held responses, clocks, counters and threads are synthetic controls, not controller mechanisms.

## Measurements and gates

The unchanged eight-notification burst made **zero HTTP requests**, including token minting, workflow reads and derivative status dispatch.
The final pipeline measured0.074241379 seconds with synthetic API delay0.03s. This is local fixture latency, not live throughput.
The historical0.0919s result is not validation of these corrections. These fixes make no new performance improvement claim over the
already-reduced committed request path.

The final extended stress-fixture trace contained475 requests:52 token,62 execution evidence,114 source reads/edits,
119 worker approvals,9 build dispatches,4 publication dispatches,70 status dispatches,42 feed GitHub reads and3 public HTTPS reads.
This fixture intentionally includes invalid evidence, multiple restarts, administrative edits, failures and reconciliation;
it is not a20-node successful graph run or a normal per-build budget. Status activity remains visible in the accounting.

Controller/DB/SSE functionality was checked locally; exact individual latency distributions were not measured here.
Runner setup, real worker refresh, GitHub status deployment and Pages propagation were not measured or activated.
No local timing is represented as live acceptance. The unchanged mutation-uncertainty policy retains assignments and blocks their
prerequisites while allowing independent work within approved capacity; failed rollback blocks producers until explicit recovery/restart.

Open gates remain: mixed O/N workflow-level/job-level feed-group exclusion proof before rollout above capacity1;
sandbox aliases under their separate +450 bound; real graph/SSE/status/runner/Pages timing; owner acceptance; and every unperformed
commit, push, merge, installation, live restart or activation stage. No owner waiver or completed live acceptance is claimed.

## Line accounting

Classification is fixed: src/, deployment/worker/ and CMakeLists.txt are production; tests/ are tests.
Working-tree changes are included. The starting652/452=+200 production and398/116=+282 tests were reproduced.

| Relative to | Production added / removed / net | Tests added / removed / net |
| --- | --- | --- |
|4257cb0|697 /464 /+233|917 /271 /+646|
|63ab7b2|67 /34 /+33|534 /170 /+364|

Remaining cumulative headroom: **77 production /144 test lines**. No increase beyond approved bounds or unagreed production mechanism
was introduced. Production growth is the agreed error-response context, disabled completion, phase validation and focused test registration;
the obsolete catches were removed. The sandbox +450 allowance was not used. Documentation is counted separately in the handover evidence;
pre-existing dirty documentation and untracked review records are not claimed as new task work.

Documentation accounting (public tracked documentation plus this new report; pre-existing untracked review records excluded):
relative4257cb0: 3581 added /56 removed /+3525 net; relative63ab7b2: 898 added /38 removed /+860 net.
Task-only documentation, relative the preserved starting files: 240 added /2 removed /+238 net (M1 edit,46 WORK_LOG lines and 190 report lines).
