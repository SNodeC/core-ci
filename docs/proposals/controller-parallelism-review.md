# core-ci parallelism review: diagnosis and reduction-first plan

**Bottom line:** the long chain comes from core-ci's own feed gating, not GitHub. Only one publication can exist at a time for the whole graph, because every format shares one feed. That one slot covers validation, tool and SDK setup, signing, push and the wait for HTTPS. The first gate is held by the controller; GitHub then repeats it for the whole workflow. The smallest fix is to delete code, not add it: drop the controller's feed gate, move GitHub's feed lock down to the privileged job, and take the HTTPS wait out of that lock. Push stays fast-forward-only and an uncertain writer is still never released. No new states, queue, threads or timers are needed.

I only read files. Nothing was edited, built, run or pushed. I read controller sources, worker workflows and scripts, tests, `WORK_LOG.md` and `docs/improvements/README.md`. You asked for the report as text, so I didn't write a plan file.

## Verified findings (ranked by impact)

**F1. One publication at a time, graph-wide. It starts at dispatch and ends only after controller proof.**
- `Graph::publicationReady` (`src/Graph.cpp:485-498`) refuses any node while another node on the same feed is `publishing`.
- The feed identity is `(feed repository_id, branch)` (`Graph.cpp:56-60`). `PipelineConfig` has one feed for deb, rpm, ipk and apk, so this is a global single writer.
- The same rule is repeated in `Graph::validate` (`Graph.cpp:580, 613-616`), `Graph::restore` (`Graph.cpp:159, 164-165`) and `Pipeline::advance` via `publicationSelected` (`src/Pipeline.cpp:262, 265-268`).
- The slot is taken when `publishing` is committed, before the workflow exists. It is released only by `published` or `failed`, after the run finishes, the webhook arrives and FeedProof's 5–7 requests return (`Pipeline.cpp:787-815`, `src/FeedProof.cpp:29-113`). So the slot spans validation (about 2 min), the privileged job (42–153 s), the HTTPS wait and controller proof.
- Twenty publications therefore run strictly one after another.
- What it protects: no feed write that isn't actually shared. Integrity is already enforced at the write itself: GitHub's feed concurrency group plus a fast-forward-only push with no force or rebase (`publish.py:127-143`). The controller gate is policy layered on top. When a writer is uncertain (the push-403 case) it blocks the whole feed.

**F2. GitHub repeats the same lock, also over validation.**
- `core-ci-publish.yml:24-27` puts the concurrency group at workflow level, so the unprivileged `validate` job (`:43-77`) is inside the lock too.
- Inside the privileged job, these all happen while the lock is held:
  - tool install (`:101-106`)
  - SDK download, extraction, defconfig and make queries again (`sign_openwrt.py:28-29` → `sdk.py:12-85`)
  - package signing, index generation and signing, and page rendering
  - `wait_for_https`, which polls Pages for up to 600 s (`publish.py:146-166`, called at `:249`)
- The validation run (`validate.py:63-67`) does not mutate the feed.

**F3. Built work is picked for publication in hash order.** `advance` walks `std::map<id, Node>`, where `id` is a sha256 digest, and takes the first ready node (`Pipeline.cpp:264-268`). That is neither FIFO nor critical-path order. This fits core IPK r128: its build started 11:06:54 but its publication only started 11:29:16 — roughly four unrelated publications of about 5 min each went first while its dependents waited. The mechanism is verified; that it explains this specific case is an inference. Once F1 is removed, the order no longer matters, so no priority policy is needed.

**F4. An eligibility update can be dropped when commits overlap.**
- `GraphStore::commit` builds its compare-and-swap guard from the generation at the moment it is called (`src/GraphStore.cpp:206`, guard at `:41-45`). The database runs writes in FIFO order later (`src/Database.cpp:28-42`).
- If an evidence write from another webhook is already queued, `advance`'s commit fails with "conditional mutation did not match". `advance` then returns 503 and has no re-evaluation (`Pipeline.cpp:286-289`).
- Nothing re-runs `advance` until the next terminal transition or a manual reconcile, so eligible dependents sit idle.
- `change()` retries once, but with the same stale node (`Pipeline.cpp:914-918`). If the earlier write touched the same node, `validate` rejects it ("evidence regressed"). A `built` or `published` transition can then be lost even though its run has finished and sends no more webhooks.
- `record()` already re-derives on conflict (`Pipeline.cpp:637-644`); `advance` and `change` don't.
- How often this happens is unproven. It would show up in the controller log as `GitHub webhook completed: {"status":503,…"conditional mutation did not match…"}`.

**F5. Status refresh queues ahead of execution dispatch and keeps re-firing.**
- `onChanged` runs before each commit's continuation (`GraphStore.cpp:277-280`). It calls `refreshStatus()` (`src/main.cpp:158-161`), which enqueues a GitHub read and then a dispatch (`Pipeline.cpp:384-426`). Those requests land in the shared FIFO ahead of the `advance`/dispatch requests from the same commit.
- `refreshing` only coalesces while the controller's own HTTP request is in flight. It then re-fires whenever the generation moved, and every evidence-only commit moves it. During a busy session that becomes a near-continuous stream of status dispatches.
- Each status job takes the same feed lock (`core-ci-status.yml:19-22`, FIFO with publishers) and pushes, which triggers a Pages build. Pages builds then delay the publishers' `wait_for_https`, which runs inside the lock.
- `main.cpp:163` also builds the full `view()` on every commit, even with no SSE receivers. That view computes blockers, dispatch inputs and JSON dumps for all 140 nodes, and it runs before the continuation. Your 119–160 ms full-GET measurement is a rough upper bound for it, not a measurement of it.

**F6. All GitHub calls go through one connection-per-request FIFO.** `HttpClient` allows one request in flight, sends `Connection: close` and does a new TCP+TLS handshake each time (`src/HttpClient.cpp:24, 130-133, 137-171`).
- This serializes token minting, dispatch preflight, POST, inspect, job and artifact reads, FeedProof, and status refresh.
- It protects only this client design, which matches each operation to `requests.front()`. It protects no graph mutation.
- Fan-out after one `published` costs roughly 3 round trips per dispatch, one after another.
- You deferred the per-origin client refactor. I'm not prescribing it; F6 should be measured first (CP0).

**F7. Already parallel and correct.**
- `advance` dispatches every eligible waiting build in one commit (`Pipeline.cpp:269-275`), after commit and before dispatch (`:286-301`).
- Eligibility is per node over exact prerequisites (`Graph::blockers`, `Graph.cpp:204-227`), so independent targets are only coupled through F1. A join waits for every prerequisite to be published.
- Webhooks are acknowledged before processing (`Pipeline.cpp:160, 204`).
- Tokens, reads and workflow proofs are coalesced (`src/GitHub.cpp:251-295`, `Pipeline.cpp:309-364`).

**Hypotheses (not verified):**
- **H1:** Pages throughput may be the bound on `wait_for_https`. If the feed site is built from a branch rather than a custom Actions workflow, GitHub applies a soft limit of 10 builds per hour, and status pushes use up that budget.
- **H2:** The existing tests require `queue: max` (`tests/worker_provenance.py:503`), and live runs accept the key. It still has to be proven live that several pending jobs in the group are kept in FIFO order and not cancelled.
- **H3:** The shared HTTP FIFO in F6 contributes about a second at fan-out. It is not proven to dominate the 13 s gap.

## Q1–Q2: serialization boundaries

| Boundary | Code | What it protects | Kind of cost |
|---|---|---|---|
| One event-loop thread | AGENTS.md; `Pipeline::event` scan `:178-200` | In-memory graph; needed | CPU, small |
| Database FIFO, one connection | `Database.cpp:28-141` | Transaction atomicity on one connection; needed | Local async wait, ms |
| Generation compare-and-swap | `GraphStore.cpp:41-45, 206` | Stale validation and external writers; needed. Missing re-evaluation (F4) is a defect | Policy |
| `onChanged` before continuation | `GraphStore.cpp:277-280`, `main.cpp:158-174` | Nothing; a side effect of ordering | CPU (view) plus queuing (status) |
| Shared HTTP FIFO | `HttpClient.cpp` | Only the client design | Async wait (F6) |
| Dispatch preflight (ref read, approved blob) | `Pipeline.cpp:366-382, 434-441` | Prevents dispatching an unapproved workflow that would get secrets; needed per dispatch | 1 round trip, coalesced per repo |
| Inspect after dispatch | `Pipeline.cpp:477-496` | Run identity binding; needed (could also come from the webhook) | 1 round trip |
| Artifact list before `built` | `Pipeline.cpp:719-785` | Artifact identity; needed | 1 round trip |
| FeedProof before `published` | `FeedProof.cpp` | Independent proof that an edge is satisfied; needed on the dependent path only | About 6 round trips plus HTTPS |
| Controller feed gate | F1 | Policy only; integrity is enforced at the write | Policy gate, minutes |
| Workflow-wide GitHub lock | `core-ci-publish.yml:24-27` | Validation needs no lock; the feed-mutating section does | Lock held too wide |
| `wait_for_https` inside the lock | `publish.py:249` | Writes nothing; later deployments still serve retained receipts | Async wait inside the lock |
| Status job in the feed lock | `core-ci-status.yml:19-22` | Needed while it pushes the same branch; its frequency is not needed | Contention |

## Q3: what can overlap, and the smallest feed critical section

**May run concurrently, across all publications:**
- builds
- the `validate` job: artifact download, SDK verification, metadata and provenance checks
- the privileged job's tool install and SDK preparation
- package-file signing (rpm `--addsign`, apk `adbsign`), which depends only on artifact bytes and the key
- HTTPS confirmation
- controller FeedProof
- dependent builds

**Must serialize per feed (the true critical section):**
1. Check out the feed branch head inside the lock.
2. Add the pre-signed package files with `add_new`, which refuses any existing identity.
3. Regenerate indexes over all retained packages and sign them (`apt_indexes`, `rpm_indexes`, `ipkg-make-index`/`usign`, `apk mkndx`).
4. Render the pages.
5. Commit.
6. Fast-forward-only push.

Status jobs share this same section.

## Q4: recommended reduction and alternatives

**Recommended, in order:**
- **CP1.** Delete the controller feed gate. Publication readiness becomes "built, verified, within dispatch bounds". Add re-evaluation on a definite (non-uncertain) compare-and-swap conflict:
  - in `advance`: run `advance` again from current state;
  - in `change`: re-derive the transition instead of re-committing the stale node, following the existing `record()` pattern.

  Run `onChanged` after the continuation, and build `view()` only when there are SSE receivers.
- **CP2.** Move `concurrency:` from workflow level to the privileged `publish` job. Split `publish.py --phase push` (token, then push) from a new `--phase confirm` (`wait_for_https`). Run confirm in a third, unlocked job that has no environment and no secrets.
  - The locked job keeps the name `Publish <label>` and the step "Publish checked feed update", so `matchesJob` and push-step evidence stay unchanged.
  - The confirm job must not be named `Publish …`.
- **CP3.** Reduce status refresh. Dispatch only on commits that change an attempt state or a selection, not on evidence-only commits. Optionally keep at most one status run outstanding, cleared by that run's standard `workflow_run` webhook. Owner decision; no timer.
- **CP4 (only if CP0 measurements show the lock is still dominated by setup).** Split the privileged job into `Sign <label>` (environment, unlocked: tools, SDK, package signing) and `Publish <label>` (environment, locked: steps 1–6 of the critical section).

**Alternatives and tradeoffs:**
- **B, conservative:** the controller blocks a feed only while a *completed-but-unverified* writer exists. Still parallel while things go well, but one uncertain writer stalls everything (the 403 case).
- **C:** separate feeds or branches per format. The controller already keys feeds by repo and branch, but this is a deployment and Pages change.
- **Rejected:**
  - a second dispatch with a "prepared" state (adds a state);
  - lock-free optimistic pushes with rebuild on rejection (a retry loop);
  - a ready queue or priority policy.

## Q5: how the short section keeps its guarantees

- **Feed checkout:** taken inside the job-level lock that every feed writer, publishers and status jobs, holds from checkout to push. So it is the current head.
- **Indexes over retained packages:** regenerated from that checkout, so they include every earlier writer's packages, uncertain ones included if their push landed. `add_new` and per-revision paths mean no different bytes ever appear under an existing package identity.
- **Signing:** index signatures are made inside the section; package-file signatures can be made before it (CP4).
- **Concurrent status writers:** they take the same lock and push fast-forward-only.
- **Checked push:** unchanged. A stale push is rejected and never forced or rebased, and nothing retries. A rejected or 403 push leaves the node `publishing`, i.e. uncertain, exactly as today (`publicationResult` fails a node only when the push was verifiably skipped).
- **Uncertain writers:** an uncertain node is never released. Its dependents stay blocked because only verified publication satisfies an edge. What changes is that it no longer blocks unrelated writers. **This is an owner decision**, because it reinterprets "uncertain-writer exclusion"; alternative B keeps the literal reading.

## Q6: reusing verified tooling

- Do **not** pass tools from `validate`: that runner processes untrusted artifacts.
- Acceptable options:
  - (a) Cache the official SDK *archive* keyed by its sha256 and re-hash it after restore against freshly fetched official metadata. A mismatch fails, with no fallback. This saves download time only.
  - (b) Skip the duplicated defconfig and make queries in the privileged job, because the same run's `validate` already verified the environment, and extract only the host tools. This removes a deliberate double check (the comment at `sign_openwrt.py:27`): owner decision.
  - (c) Under CP4, hand host tools from the trusted `Sign` job to `Publish` as an artifact, with per-file sha256 values passed in job outputs and checked before execution. Same trust level as today.
- Artifact, package, signature and key checks all stay. Measure the 79 s breakdown (download, extract, defconfig) before choosing.

## Q7: local reaction and measurements

The graph is committed before network work wherever it can be. The `built` and `published` transitions must wait for artifact and feed verification. Unnecessary delay comes from F4, F5 and F6.

Proposed measurements, logs only, no new evidence fields:
- webhook receipt, with `X-GitHub-Delivery` added at `main.cpp:107`, plus type, action, run ID and scan time
- commit: enqueue, start and finish time, generation, outcome including conflicts, database queue depth
- `onChanged` duration
- `advance`: compute time and counts
- each HTTP request: enqueue, connect and response time, queue depth, method and path (no headers)
- dispatch POST response compared with the run's `created_at` read by `inspect`
- FeedProof start and end
- each status dispatch's generation

GitHub job `started_at`/`completed_at` values are already recorded as evidence. Pages visibility is measured by an external probe comparing `status/controller.json` generation and receipt bytes against controller state. "Millisecond reaction" stays a target until these numbers exist.

## Q8: status contention versus execution authority

Status refresh (F5) is derivative work. It competes in three places: the controller's HTTP queue, the GitHub feed lock, and the Pages build budget. It has no execution authority. CP3 bounds it without a scheduler. The live SSE UI stays immediate; static pages are expected to lag.

## Changes summary

- **Behaviour changes:** concurrent publication dispatch per feed; restore accepts several `publishing` nodes; re-evaluation on conflict; status trigger policy.
- **New states:** none. **New evidence fields:** none.
- **Obsolete gates to delete:**
  - `Graph.cpp:489-496`, `:580, 614-615` (feed part), `:159, 164-165`
  - `Pipeline.cpp:262, 265-268`
  - workflow-level `concurrency` (`core-ci-publish.yml:24-27`), moved to the job
- **Consumers affected:**
  - `Graph::restore`/`validate`, `Pipeline::advance`/`change`, `GraphStore::commit`, `main.cpp` `onChanged`
  - `publish.py`, `core-ci-publish.yml`
  - tests: `GraphTest.cpp:179-192`, `tests/pipeline.py:176` (asserts exactly one `publishing`), `worker_provenance.py:501-504`
  - docs: deployment README, status-templates README, `WORK_LOG.md`
- **Activation:** selections pin `deployment.publication_workflow` `blob_sha` and `ref`. A changed workflow at the same ref will fail the dispatch proof for waiting selections that pinned the old workflow. Publish it on a **new publication ref** or activate between sessions (owner decision).

**Owner decisions needed:**
1. Write-boundary exclusion (CP1) versus alternative B.
2. The workflow reshape and ref strategy.
3. Status trigger policy.
4. Re-derive on conflict.
5. SDK duplication: option (a), (b) or none.
6. CP4 at all.

## Estimates (growth needs approval)

| Checkpoint | Production | Tests |
|---|---|---|
| CP0 logs | +40–70 | +0–15 |
| CP1 | −20 deleted, +15–35 added; net −5 to +15 | +70–130 |
| CP2 (workflow YAML + Python) | +20–40 | +15–30 |
| CP3 | +10–40 | +30–60 |
| **CP0–3 total** | **about +65 to +165 net** | **+115–235** |
| CP4 (separate approval) | +100–180 | +80–150 |

## Tests and live acceptance

**Public-boundary tests:**
- **Graph:** two built nodes on the same feed both publish in one change; restore accepts that state; an uncertain writer doesn't block an independent writer, while its dependents and a join with one uncertain prerequisite stay blocked.
- **`tests/pipeline.py` (fake GitHub):**
  - one advance sends N publication POSTs, each after its own durable `publishing` commit;
  - an evidence webhook interleaved with FeedProof still gets dependents dispatched with no further events (F4 regression);
  - `request_trace` shows execution dispatch before status dispatch;
  - status dispatches are bounded;
  - restart with three `publishing` nodes reconciles all of them.
- **Worker:** `validate` and confirm are outside the lock; the publish job holds it; `push` doesn't wait; `confirm` checks bytes; still no force push.

**Live acceptance (VOC-Experiments sandbox, needs your authorization):** a fresh tag at the unchanged source commit. Prove that:
- `validate` job intervals overlap;
- locked publish and status job intervals never overlap;
- no pending job is cancelled (H2);
- every join's dispatch comes after its last prerequisite's `publication_verified_utc`;
- feed history is linear and fast-forward only, and the client tests verify the signed indexes;
- controller logs show receipt → commit → POST in ms.

Report any uncertain writer you see; fault injection needs your separate authorization.

## Expected critical path and refuting observations

With one feed, the critical path becomes the sum of lock-held time over all publications, plus per-tier build and validation time. I'm not promising a wall-clock figure.

The diagnosis is refuted if any of these hold:
- validations don't overlap after CP1–2, or publish jobs still start about one controller reaction after the previous run ends (GitHub concurrency doesn't behave as assumed);
- lock-held time is small but the chain stays long (the limit is runners or build time);
- the logs never show 503 conflicts from `advance` (F4 is only theoretical);
- the HTTP queue wait dominates receipt-to-POST time (revisit the deferred client work);
- `wait_for_https` stays at minutes with fewer status pushes (Pages throughput, H1).
