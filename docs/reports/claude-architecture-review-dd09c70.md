# Claude architecture review and discussion correction

This is a read-only external review, not independently validated findings or implementation approval.
Claude reviewed source `ea38ff6abcadf93118c1b0d12024a37604f5a3a4` (merge checkpoint `dd09c702ae5e3d9447af7a3c396aadc7cbbd7f2a`).
The deploying agent verified that final source `e78923e78a41a20bdc4d9a55533339ee8a0d96e6`
and deployed merge `a11d98b7b3c401168e36a1029218631ba01d0f6e` have identical trees.
Their sole source delta from the reviewed checkpoint excludes the chevron from a CSS hover underline.
Claude's follow-up used supplied deployment facts, without additional source or live inspection.

The P2 recommendation below incorporates Claude's correction: state-only refresh is withdrawn.
Useful within-state progress, timestamps and links remain required. Suppressing unchanged public
facts is a candidate simplification requiring code validation; no new timer or latch is proposed.
LOC alone is not treated as a defect. No review finding has been implemented.

# core-ci controller review: revision ea38ff6, read-only

**Verdict:** the implementation is problem-aligned overall. The graph and state machine are small, pure and explicit, and most code that looks redundant is a real trust or uncertainty boundary. I found four real defects, all bounded in impact: one breaks recovery once workflow history grows, one causes false failures during normal activity, and two are smaller. I also found three small deletions and one reshape that removes duplicate work. Several owner policy points remain open, and a few scaling bounds need measuring. No broad rewrite is warranted.

## 1. Revision, coverage and limits

**Revision.** I reviewed the working tree at HEAD `ea38ff6abcadf93118c1b0d12024a37604f5a3a4`, branch `rewrite/slim-graph-controller`. You state that HEAD and main `dd09c70` have identical source trees; I did not check that against git internals.

**Read in full:**
- **`src/` (everything that matters):** `main.cpp`, `Pipeline.{h,cpp}`, `Graph.{h,cpp}`, `GraphStore.{h,cpp}`, `Database.{h,cpp}`, `GitHub.{h,cpp}`, `HttpClient.{h,cpp}`, `SourceReader.{h,cpp}`, `FeedProof.{h,cpp}`, `Enrollment.{h,cpp}`, `PipelineConfig.{h,cpp}`, `ManagedDatabase.{h,cpp}`, `DatabaseConfig.cpp`, `Version.{h,cpp}`, `Crypto.{h,cpp}`, `Error.h`, `PrivateFile.cpp`, `UiAssets.h.in`.
- **Also read:** `ui/app.js`, `CMakeLists.txt`, `AGENTS.md`.
- **Workflow and worker files:** `deployment/source/core-ci-build.yml.in`, the worker `core-ci-build.yml`, `core-ci-publish.yml` and `core-ci-status.yml`, `deployment/worker/core_ci/publish.py` and `status_update.py`.
- **Framework (public headers only):** `core/socket/stream/SocketClient.h`, `SocketConnector.hpp`, `tls/SocketConnector.hpp`, `SocketAcceptor.hpp`.

**Read partly or by search:**
- `ui/index.html` (element IDs only).
- `tests/pipeline.py` (the fixture's REST mock and the inspection, retirement and enrollment-race blocks).
- `tests/ui_enrollment.py` (the fixture's mocked responses).
- Searches across `tests/` for endpoint and field use.

**Not examined:** `DatabaseConfig.h`, `PrivateFile.h`, `ui/style.css`, `cmake/*.cmake`. Also the worker modules `build.py`, `native*.py`, `openwrt_container.py`, `sdk.py`, `sign_*.py`, `validate.py`, `contract.py`, `github.py`, `environment.py`, `feed_status.py`, plus `status-templates/` and `keys/`. Among the tests, I did not read the C++ files (`GraphTest`, `GraphStoreTest`, `GitHubTest`, `SourceReaderTest`, `DefinitionTest`, `DatabaseBoundary`, `GraphFixtures.h`) or most Python test bodies. I skipped `__pycache__`, and per your boundaries I did not touch any secret, credential, history or proposal material.

**What each kind of claim rests on:**
- **Proven from source:** everything cited with `file:line` below.
- **Your deployment observations:** taken as given, not checked (host, PID, hashes, generation 3116, 100 executions).
- **Earlier synthetic checks:** your DOM probes; not repeated and not relied on.
- **Hypotheses and unmeasured claims:** labelled where they appear. They cover GitHub run counts, document size, feed tree size and performance. I had no GitHub, database or browser access, and ran no build or test.

## 2. Responsibilities, state and lifetimes

| Concern | Where | State it owns | Lifetime |
|---|---|---|---|
| Graph and state machine (pure) | `Graph.cpp` (`select`, `advance`, `blockers`, `observe`, `built`, `published`, `failed`, `retire`, `retry`) | none (operates on the document) | n/a |
| Authoritative document | `GraphStore` | `state`, the in-memory mirror of `graph_state.document`. It changes only after a committed conditional UPDATE (`GraphStore.cpp:45-55`) | process |
| Write serialization and uncertainty | `Database` | `operations` deque, `pumping`, sticky `uncertain` | process; `uncertain` lasts until restart |
| Transition plus dispatch authority | `Pipeline::change` (`Pipeline.cpp:13-28`) | runs `derive` and then `graph::advance` in **one** commit; dispatches only after commit | per change |
| Dispatch | `Pipeline::dispatch` | records a **preflight** marker (write-ahead record that a POST may follow), then POSTs, then records the dispatch run ID | per attempt |
| Authenticated facts | `Pipeline::event` → `accept` → `record` | none; facts go into `attempt.evidence` | per webhook |
| Completion boundary | `Pipeline::complete` | `completionReads` (in-flight `attempt:stage` keys) | per artifact read or feed proof |
| Feed proof | `FeedProof::verify` | its own `HttpClient` (`feed-http`) | process |
| GitHub transport | `GitHub` / `HttpClient` | `tokens` (cache plus waiters), `reads` (coalesces identical in-flight GETs), per-request operations, `connecting` hand-off slot, bound of 8 in flight | process |
| Source reads | `SourceReader` | a `Reading` per call, held alive by its callbacks | per call |
| Presentation | `GraphStore::view` / `publicView`, SSE in `main.cpp`, `refreshStatus` | `eventReceivers`, heartbeat timers (owned by the disconnect lambda), `refreshing`, `statusSent` | process / per connection |
| Startup | `main.cpp` | `initialConnection` (one-shot) | process |

**Persisted document:** `generation`, `next_revision`, `repositories{}` and `executions{}`. Each node holds `source` (including an immutable `deployment`), `target_row`, `prerequisites`, `automatic`, `superseded_by` and `attempts[]`. Each attempt holds `id`, `revision`, `state`, `assignment`, `preflight{}`, `dispatch{}`, `build_run`, `build_result`, `publication_run`, `receipt`, `evidence{}` and `failure`.

**State machine:**
- `waiting` → `building` (assignment frozen, authority committed) → (`build_result` set) → `publishing` → `published` or `failed`.
- `failed` → `retry` → a new `waiting` attempt.
- An unbound `publishing` attempt → `retire` → `failed`.

**Legitimate boundary code:**
- Raw-body HMAC, installation, repository-ID and attempt-one checks.
- Run identity binding: repository and head repository IDs, actor, event, path, branch, and the workflow blob at `head_sha`.
- Job-name binding, including the unevaluated name of a skipped publish job.
- Monotonic evidence merging for out-of-order webhooks (`Graph.cpp:377-392`).
- One artifact-list read, with checks on bound, uniqueness, digest, run and time window.
- The feed proof chain: repository ID, commit listing, tree and blob, HTTPS bytes equality, and `build_result` equality.
- Preflight write-ahead, the stored dispatch run ID, frozen retry dependencies and `environment_from`.
- Selection race guards (`since`, and `repositories == enrolled`).
- The dispatch payload-size blocker.

## 3. Findings by impact

### A. Real defects

**F1. Enrollment uses the global graph generation as its concurrency check, so unrelated activity causes false 409s.**
- **Where:**
  - `Enrollment.cpp:42`: `refresh` captures the generation.
  - `Enrollment.cpp:20`: `persist` requires `state.generation == generation`.
  - `Enrollment.cpp:73,79`: `save` captures the generation and rejects if it changed.
  - Callers: `main.cpp:223-239`, and `save` → `refresh` at `Enrollment.cpp:95`.
- **Current behaviour:** any committed graph change during the 4–5 sequential GitHub reads fails the operation. That includes job progress, preflight and dispatch-ID records, and artifact or receipt commits.
  - `save` then reports "committed definition changed; read it before saving", which is untrue: only unrelated progress happened.
  - After a successful PUT, `persist` fails with "graph changed while editing enrollment", so GitHub holds the new definition while enrollment keeps the old one.
  - For a new enrollment, the automatic first request (`onEnrolled`) is skipped too.
  - During active builds, generation bumps are frequent. The rate is unmeasured, but each execution produces roughly 10–15 commits.
- **Why it is wrong here:** the facts the check should protect are this repository's enrollment row and the validity of the source set. The apply step already re-runs `validateSources` against the current executions and repositories, and checks the ID (`Enrollment.cpp:23-25`). `Pipeline::request` already uses the narrow form (`Pipeline.cpp:74`).
- **Plan:**
  - In `refresh`, capture `previous = repositories.value(name, null)` and have `persist` require `state.repositories.value(name, null) == previous`, replacing the generation parameter.
  - In `save`, compare the current row with the captured `enrolled` instead of the generation.
- **Invariants kept:** two concurrent refreshes of the same repository still conflict; ID change is still rejected; validation still runs at commit time against current state.
- **Counterexamples checked:** a concurrent change to another repository is still caught by `validateSources` in the apply step. A concurrent source selection is still caught by `Pipeline.cpp:74`. No persisted field changes.
- **Effect:** no change to schema, restart or uncertainty behaviour. Fewer failed operator actions; no extra requests.
- **Verify:** an enrollment test that injects an unrelated evidence commit while refresh and save are reading, expecting 200. Also keep a same-repository concurrent refresh test expecting 409.

**F2. Recovering an unbound run requires a complete listing of the workflow's entire dispatch history, so it fails permanently once a workflow passes 100 runs.**
- **Where:**
  - `Pipeline.cpp:503-540`: `inspect` with `runId` null lists `…/runs?event=workflow_dispatch&per_page=100`.
  - `:514-517`: requires `total_count == workflow_runs.size()`.
  - `:531`: reads the run only if the listing is whole and has exactly one match.
  - Tested deliberately: the fixture serves `total_count 101` (`tests/pipeline.py:281`), and `:423-429` asserts that the "one" and "incomplete" modes are not read.
- **Current behaviour:** when the dispatch response was lost and webhooks were missed (for example, a restart during dispatch), the listing is the only recovery. `total_count` grows with every dispatch the workflow has received within GitHub's retention.
  - The worker's publication workflow receives **every** publication from every repository.
  - Each source build workflow receives every target × release × attempt.
  - Past 100 runs the result is always "incomplete", so the run is never bound. A publication can then only be retired, which discards a proof that may exist. A build has no exit at all (see P1).
  - Hypothesis: the live worker publication workflow is likely near or beyond 100 dispatch runs, given 100 executions. Unverified; I had no GitHub access.
- **Why it is wrong here:** the contract keeps REST reconciliation as necessary. The completeness rule is sound, but the query scope is the whole workflow history rather than the attempt's dispatch window.
- **Plan (owner decision):** keep the completeness and ambiguity rule, and narrow the query:
  - (a) Add `&head_sha=<preflight.commit>`. That field is already stored, and any attempt that can have a run has a preflight; startup reconciliation fails those without one (`Pipeline.cpp:554-557`). This bounds source build workflows, but **not** the worker publication workflow, whose head stays constant between worker commits.
  - (b) Record a UTC time in the existing preflight record (`Pipeline.cpp:123-124`) and add `&created=>=<time - skew>`. This is one new field in an existing write-ahead record; no new mechanism. Attempts without the field keep the current query, with no migration.
  - The alternative, accepting a single match from a partial page, reverses a tested safety rule and is not recommended.
- **Invariants kept:** never bind on an incomplete or ambiguous listing; actor and label filtering unchanged; never replay.
- **Effect:** persistence gains one optional field (for b). Same request count. Restores recovery after a restart.
- **Verify:** the same pipeline fixture with more than 100 historical runs outside the filter window, expecting "runs read". Keep the "ambiguous" and "incomplete" cases within the window.

**F3. The refresh after a definition commit reads the mutable branch head and can get a stale answer.**
- **Where:**
  - `Enrollment.cpp:93-95`: after a successful PUT, `refresh(input)` → `sources.definition(repository, branch)` → `GET /commits/<branch>` (`SourceReader.cpp:156`).
  - `GitHub.cpp:193-206` coalesces identical in-flight GETs. The key is `[repo, suffix, permissions, public]`.
  - The dispatch preflight issues the identical `GET /commits/<branch>` (`Pipeline.cpp:110-114`), as does `Enrollment::workflow`.
- **Current behaviour:** if a preflight's branch read for the same repository is in flight when the post-PUT refresh starts, the refresh joins it and receives the **pre-PUT** head. It then persists the old definition while reporting `github_committed: true`. GitHub's own ref consistency has the same shape. The failure is narrow but real, and the user recovers by re-importing.
- **Plan:** use the commit SHA returned by the PUT (`result.body.commit.sha`) as the ref for the post-PUT definition read. The row keeps `branch` and records that commit as `revision`. If the SHA is absent, report committed plus "re-import" (the existing failure shape). The GET coalescing stays as it is.
- **Invariants kept:** repository-ID check; definition parsed from the committed bytes.
- **Counterexample checked:** a third-party push after the PUT is picked up by the next refresh, which is the same meaning as today.
- **Verify:** fixture holds the branch read open, saves, and asserts the persisted `revision` equals the PUT commit.

**F4. The UI reads fields the server does not send.**
- **Workflow preview:** `ui/app.js:544` reads `workflowPreview.current`, but the server sends `content` (`Enrollment.cpp:118-123`). The "Current committed workflow" textarea therefore shows the text `undefined`.
- **Unsupported definition import:** `ui/app.js:520` shows `error.result.github_revision`, which the refresh error body (`Enrollment.cpp:51`) never contains. The message reads "Read GitHub revision: undefined".
- **Why tests miss it:** the fixture server invents `github_revision` (`tests/ui_enrollment.py:163`), and no test reads the preview's current content.
- **Plan:** have the UI read `content`. Either add `"github_revision": result.source.value("commit","")` to the refresh error body or drop the revision from the message. Align the fixture with the real controller body.
- **Effect:** UI only. No state, persistence or request change.

### B. Unnecessary constructs that can be removed or reshaped

**S1. The "public change" filter in `GraphStore::change` is bypassed by the status refresh's trailing re-check, and it costs two full projections per commit.**
- **Where:**
  - `GraphStore.cpp:46-53`: computes `publicView(state)` and `publicView(next)` on every commit, using the private overloads at `GraphStore.h:26-27`.
  - `main.cpp:173-176`: calls `refreshStatus` only when that filter says something public changed.
  - `Pipeline.cpp:606,616-618`: after the status POST, `if (generation changed) refreshStatus()`, and `refreshStatus` only checks `generation != statusSent`.
- **Current behaviour:** preflight and dispatch-ID commits are non-public, two per dispatch. When they land while a status POST is in flight, which is the common case right after the public "building" commit, the trailing re-check dispatches another status run anyway.
  - Each status run then holds the per-feed lock that publications also need (`core-ci-status.yml:18-22`, `core-ci-publish.yml:77-80`).
  - Each projection also recomputes `blockers`, and with it the candidate assignments.
- **Plan:** move the comparison to the one consumer.
  - `Pipeline` remembers the last dispatched public facts with `generation` removed, replacing `statusSent` (a `uint64` becomes `Json`).
  - `refreshStatus` computes `publicView()` once and returns if the facts are unchanged. The trailing re-check uses the same comparison.
  - Delete the before/after block, the two private overloads and `onChanged`'s first `bool`. `main` calls `refreshStatus()` on every notification, and it is cheap when status is disabled.
- **Invariants kept:**
  - At most one status POST in flight.
  - No replay of a dispatch for facts already dispatched, including when the dispatch failed (the current "not replayed" meaning).
  - Startup still dispatches once, with `null` facts in place of `UINT64_MAX`.
  - The reconcile route still re-tries after a failed workflow preflight, because `refreshing` is reset and the facts were not recorded.
- **Effect:** removes one mechanism (the split filter plumbing), cuts two projections per commit to one, and avoids status runs that carry no new facts. About −12 to −15 LOC.
- **Verify:** a pipeline case that counts status POSTs across a build dispatch (preflight and dispatch-ID commits must not add one), plus the existing tests for unconfirmed status dispatch.

**S2. `Enrollment::list` is synchronous but callback-shaped, so the `unavailable` event is dead code.**
- **Where:** `Enrollment.cpp:10-16` always reports 200. That makes these branches unreachable: `main.cpp:187` and `:213` (`status == 200 ? "repositories" : "unavailable"`) and the UI listener at `ui/app.js:445`. `/api/repositories` (`main.cpp:217-221`) also wraps a synchronous value.
- **Plan:** return `Json` from `list()` (or read `graph().at("repositories")` directly in `main`), and delete the dead event type and listener.
- **Effect:** removes one SSE event type and three dead branches; about −8 LOC. No behaviour change.

**S3. `FeedProof` runs the committed transition once beforehand as a dry run.**
- **Where:** `FeedProof.cpp:73-74` applies `graph::published` to a copy of the node. The real transition (`Pipeline.cpp:335-339`) applies the same function to current state, so the dry run adds no check. The receipt-to-`build_result` equality at `:69` is **not** duplicated and must stay.
- **Plan:** delete the two lines and the `Graph.h` include.
- **Difference in behaviour:** a receipt that does not match the assignment would be logged as a 409 from the change, instead of a 503 "uncertain". The node stays `publishing` either way. The re-check on changed evidence (`Pipeline.cpp:347-350`) can never fix an immutable mismatched receipt, so nothing is lost.

**S4. `Pipeline::refreshing` stores a generation that is never read.**
- **Where:** `Pipeline.h:55`; it is only ever tested for presence (`Pipeline.cpp:588,596,607`).
- **Plan:** make it a `bool`; this folds naturally into S1.

### C. Optional readability (no change to mechanisms)

- **Misleading 422 messages:**
  - A missing acknowledgement is reported as "invalid retire JSON" (`main.cpp:273-280`).
  - "Repository not enrolled" is thrown synchronously at `Pipeline.cpp:42` and reported as "invalid source request JSON" (`main.cpp:261`).
  - Map semantic rejections to 409 with the `require` message.
- **`Pipeline::accept` (`Pipeline.cpp:213-310`):** four levels of nesting, with three different actions in the innermost branch (fetch the run, record the job, verify the run identity). Pulling out the matching predicate as a local lambda would flatten it. Purely optional.
- **`Pipeline::reconcile`:** the completion handler (`Pipeline.cpp:566-568`) allocates `inspected` before branching and rebuilds the same empty result in a ternary. Trivial.

### D. Owner policy decisions (not defects)

- **P1. No operator exit for an unbound, uncertain build dispatch.**
  - `retirable()` covers only `publishing` (`Graph.cpp:347-352`).
  - A build POST that is uncertain (submitted, then a 5xx or lost response) and never surfaces a run stays `building` forever and blocks its dependents. Neither retry nor a re-request can replace it.
  - A build has no feed side effect, and a retry uses a new revision label and artifact name, so a late run binds harmlessly to the old attempt.
  - **Decision:** extend `retirable` to the current stage when no run is bound (state `building`, `build_run` null, no build dispatch ID, no build evidence).
**P2. Status refresh cadence versus the shared feed lock.**

- **Required behaviour (contract):** public status must carry useful within-state progress, timing and links, not only state transitions. Dispatching status on public changes within a state is therefore required behaviour, not overhead to remove.
- **Facts from source, as read at `ea38ff6`:**
  - A status refresh is triggered on every commit that `GraphStore::change` classifies as public (`main.cpp:173-176`). It is also triggered by the trailing generation re-check after a status POST (`Pipeline.cpp:606,616-618`), which fires on non-public commits as well.
  - The status job checks out the feed and pushes while holding the same per-feed concurrency group as publication (`core-ci-status.yml:18-22,37-42`; `core-ci-publish.yml:77-80`).
  - The workflow-level group cancels older pending status runs (`core-ci-status.yml:11-13`). This bounds the backlog but leaves cancelled runs behind.
- **Unmeasured (no data):**
  - Status runs per execution.
  - How long a status run holds the feed lock.
  - The added queueing delay for publications.
  - The number of cancelled status runs.
  - These are owner-run, read-only measurements from GitHub run history.
- **Contract-preserving options:**
  1. **Accept the current cadence** if the measured publication delay is acceptable.
  2. **Assess S1.** S1 removes only status dispatches whose public facts did not change, such as those triggered by preflight or dispatch-ID commits through the trailing re-check. Every within-state progress, timing or link change still dispatches. S1 is acceptable only if it keeps:
     - at most one status POST in flight;
     - no replay of facts already dispatched, including after a failed dispatch;
     - the startup dispatch;
     - the reconcile retry after a failed workflow preflight;
     - the current 503 behaviour under uncertainty.
- **Not contract-preserving:** refreshing status only on state transitions would remove required within-state progress, timing and links. It is a **change to required behaviour** and needs fresh owner approval. This review does not recommend it; the earlier suggestion is withdrawn.
- **Out of scope:** no debounce, latch, timer or other new mechanism is proposed.


- **P3. Retry granularity.**
  - A publication-stage failure, including a transient read error in the workflow preflight (`Pipeline.cpp:115-116`), can only be retried as a new attempt starting from `waiting` (`Graph.cpp:359-363`), which means a full rebuild.
  - This is safe and matches "Retry with a new rebuild revision". Whether a publication-only retry is wanted is the owner's call.
- **P4. Database uncertainty is sticky.**
  - After an uncertain write, all mutations are refused until restart (`Database.cpp:94-95,113-120`). Webhooks that arrive meanwhile are acknowledged and dropped; restart reconciliation reads them back over REST.
  - This is coherent with "never replay". Note it, but do not add a re-read mechanism unless the owner asks.
- **P5. A bound publication whose push step failed and whose feed cannot be verified has no exit.** This is by design ("no push" is never inferred). Whether a job cancelled before the push step reports that step as `skipped` (which leads to failure) or something else (which leads to stuck) is a GitHub fact this source cannot settle.

### E. Unmeasured scaling bounds to measure before acting

- **Whole-document rewrite per change.** Every committed change rewrites the whole graph document, hex-encoded so the statement is twice the size (`Database.cpp:9-18`, `GraphStore.cpp:40-41`). The document grows with history (receipts with indexes, assignments with dependency receipts).
  - The statement size will eventually exceed MariaDB's `max_allowed_packet`. The likely outcome is a lost connection, then `uncertain`, then the whole controller becomes read-only.
  - SSE also sends the full view, including receipts, on every commit.
  - **Measure:** `LENGTH(document)` and `max_allowed_packet` (owner action). A cheap mitigation if needed: `FROM_BASE64` instead of hex (1.33× instead of 2×).
- **Recursive tree reads.** `SourceReader::tree` reads `?recursive=1`, bounded at 20,000 entries (`SourceReader.cpp:167-169`) and in practice by the 2 MiB response limit (`HttpClient.cpp:75`). That caps the size of enrollable source repositories and, as the feed repository grows, also caps feed proof. **Measure:** the live feed repository's tree size.
- **`Connection: close` on every request** means one TLS handshake per GitHub call. This is part of the approved transport; I am not recommending pooling.

## 4. Kept on purpose, despite looking like overengineering

- **The write queue in `Database` and its pump loop.** It is required because `prepare` must see the state left by the previous commit. The pump avoids recursion when a no-SQL change completes synchronously.
- **`Database::write` marks uncertainty when a completion callback throws.** This fails closed on post-commit programming errors.
- **The preflight record.** It is both the write-ahead marker that lets startup safely fail attempts that never submitted, and the workflow commit that lets `accept` skip re-reading the workflow.
- **The dispatch run ID and the verified run identity.** These are two levels of trust, not duplicates.
- **The preflight blob check and the run-level blob check.** The branch can change between preflight and POST, so both are needed.
- **`completionReads`.** This is a defensible in-flight guard. Deleting it together with the evidence re-check it requires (`Pipeline.cpp:347-350`) is possible: GET coalescing and idempotent transitions absorb most duplicates. The cost would be rare duplicate feed HTTPS GETs and duplicate 409 log noise, so it is not worth changing.
- **GET coalescing in `GitHub`.** It collapses the same-repository preflight and selection reads for a burst of dispatches (for example, five core roots). Its one hazard is fixed at the caller (F3).
- **The token cache and its waiter list.**
- **The `connecting` single slot in `HttpClient`.** It is correct because the framework reports connect status before `onConnect`, synchronously (`SocketConnector.hpp:218-223,277-282`; the TLS wrapper calls the user's `onConnect` first at `tls/SocketConnector.hpp:70-71`).
- **Reading every enrolled repository on each request.** Dependents must be found from their selected-tag definitions. The coupling (one unreadable repository blocks the selection) is a consequence of that, not waste.
- **Sorting in `advance` by revision.** Dispatch remains all-eligible-at-once; the sort only gives a deterministic, oldest-first submission order under the transport bound. There is no reservation or ordering machinery.
- **The payload-size blocker, supersession, the `reactivated` identity, and frozen retry dependencies with `environment_from`.**
- **`publicStatus` returning 503 under uncertainty, `initialConnection`, SSE heartbeats, and `/api/graph` and `/api/status`** (used by the tests at `tests/lifecycle.py:54` and `tests/pipeline.py`).

**Checked and found clean:**
- No controller threads, registry or SDK client, persisted ready queue, or reservations.
- Every eligible independent node is dispatched in one committed transition.
- Joins require all prerequisites to be published.

## 5. Recommended correction checkpoints

1. **CP1, UI contract (F4).** UI, fixture and possibly one server line. No dependencies, minimal risk.
2. **CP2, enrollment guards (F1 + F3).** About net zero LOC: two comparisons replaced, plus the PUT commit used as the ref. Risk: the save → refresh signature change. Verify with the new race tests.
3. **CP3, status refresh reshape (S1 + S4).** About −15 LOC. Touches `GraphStore`, `Pipeline` and `main`. Risk: subtle "not replayed" behaviour; keep the existing status tests and add a POST-count test.
4. **CP4, small deletions (S2, S3).** About −10 LOC. Independent of the others.
5. **CP5, run discovery scope (F2).** Needs an owner decision on (a) alone or (a)+(b). Update the fixture to serve more than 100 runs outside the window.
6. **Owner decisions:** P1 (build retirement), P3 (retry granularity). P2 preserves required within-state progress; its costs remain unmeasured.
7. **Measurements (owner-run, read-only):** document length and `max_allowed_packet`; `total_count` of dispatch runs for the worker publication workflow and the largest source build workflow; the feed repository's tree size.

**Unresolved framework fact:** `SocketAcceptor.hpp:206-215` calls the status callback once per resolved local address. The startup work in `main.cpp:303-318` (webhook listen, status refresh, startup reconciliation) would then run once per address if the configured host resolves to more than one. The default `127.0.0.1` binds one address. Whether the live host does, and whether server status still compares equal to `OK` when the `NO_RETRY` bit is set, cannot be settled from what I read.

## 6. Overall verdict

Yes, the implementation is problem-aligned.
- **Sound core:** `Graph.cpp` is a compact, pure state machine, with exactly one place where authority is committed before dispatch. The trust checks sit at real boundaries.
- **What actually stands in the way of a clean solution:**
  - The enrollment generation guard (F1).
  - The status-change filter split across `GraphStore` and `Pipeline` (S1).
  - Recovery queries scoped to all of a workflow's history (F2).
- **Smaller dead code and duplicate checks:** S2, S3, S4.
- **Unmeasured but most consequential long term:** the whole-document rewrite per webhook, which grows with history.

# Follow-up checkpoint and limits

## Current checkpoint

- **What I read:** my review read the working tree at `ea38ff6abcadf93118c1b0d12024a37604f5a3a4`. You told me its source tree matches main `dd09c70`; I did not verify that. In this follow-up I read no source and used no tools.
- **Supplied verification, not mine:**
  - Final release `a11d98b7b3c401168e36a1029218631ba01d0f6e`, built from source `e78923e78a41a20bdc4d9a55533339ee8a0d96e6`.
  - Matching source and merge trees.
  - Binary SHA256 `3c0aafd9…4139`, PID 229265, active, with matching public and loopback assets.
  - Generation 3116 and all 100 executions, with attempt, assignment and run IDs preserved.
  - No active work at restart, and session 20 published.
  - Same listeners, workflow pins and runtime libraries.
  - All of this comes from the deploying agent. I have not tested or inspected the live deployment or the backup.
- **Effect of the CSS delta:** the only stated source change is the `.row-select:hover` → `.row-select:not(.chevron):hover` selector in `ui/style.css`, a file my review did not examine. No finding, deletion, policy item or scaling note depends on `style.css`. F4 concerns field names in `ui/app.js` and the server response, not styling. **The CSS delta changes no findings.** That conclusion relies on the supplied diff being the complete source delta.

## Corrected P2

**P2. Status refresh cadence versus the shared feed lock.**

- **Required behaviour (contract):** public status must carry useful within-state progress, timing and links, not only state transitions. Dispatching status on public changes within a state is therefore required behaviour, not overhead to remove.
- **Facts from source, as read at `ea38ff6`:**
  - A status refresh is triggered on every commit that `GraphStore::change` classifies as public (`main.cpp:173-176`). It is also triggered by the trailing generation re-check after a status POST (`Pipeline.cpp:606,616-618`), which fires on non-public commits as well.
  - The status job checks out the feed and pushes while holding the same per-feed concurrency group as publication (`core-ci-status.yml:18-22,37-42`; `core-ci-publish.yml:77-80`).
  - The workflow-level group cancels older pending status runs (`core-ci-status.yml:11-13`). This bounds the backlog but leaves cancelled runs behind.
- **Unmeasured (no data):**
  - Status runs per execution.
  - How long a status run holds the feed lock.
  - The added queueing delay for publications.
  - The number of cancelled status runs.
  - These are owner-run, read-only measurements from GitHub run history.
- **Contract-preserving options:**
  1. **Accept the current cadence** if the measured publication delay is acceptable.
  2. **Assess S1.** S1 removes only status dispatches whose public facts did not change, such as those triggered by preflight or dispatch-ID commits through the trailing re-check. Every within-state progress, timing or link change still dispatches. S1 is acceptable only if it keeps:
     - at most one status POST in flight;
     - no replay of facts already dispatched, including after a failed dispatch;
     - the startup dispatch;
     - the reconcile retry after a failed workflow preflight;
     - the current 503 behaviour under uncertainty.
- **Not contract-preserving:** refreshing status only on state transitions would remove required within-state progress, timing and links. It is a **change to required behaviour** and needs fresh owner approval. This review does not recommend it; the earlier suggestion is withdrawn.
- **Out of scope:** no debounce, latch, timer or other new mechanism is proposed.

## Remaining review status

- All original findings (F1–F4, S1–S4, C, P1–P5, E, and the framework note) are **review findings from a read-only source review**. Each still needs validation by the implementing agent against `e78923e` before anyone acts on it.
- **No fixes and no policy changes have been approved or applied.** P1, the corrected P2, P3 and the F2 (a)/(a)+(b) choice remain owner decisions.
- I have not re-read or revalidated any finding against the deployed source in this follow-up. Their continued applicability rests on the supplied statement that only `ui/style.css` changed.
- The section E measurements and the GitHub run-count facts (F2) are still unmeasured.