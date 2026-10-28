# Claude implementation prompt

Implementation status: applied locally and verified. The owner approved the measured
revised bounds: 3,572 C++ lines, 15 UI lines, 3 test-registration lines and 180 test lines.
These supersede the initial estimates in the generated prompt below. The final implementation
has 3,571 C++ lines, +12 UI lines, +3 registration lines and +131 test lines. Retirement is
explicit only; no live execution was changed. Commit, push and merge are now owner-authorized; deployment remains unchanged.

The implementation consolidates repeated inspection-response builders into one local callback,
and replaces the pending counter with a transient `{inspected, unresolved}` result. Two route
registrations share identical JSON parsing/response handling while keeping separate Pipeline
and graph actions. These reductions preserve one authority and replace duplicated code.

TASK: Implement the agreed "unconfirmed publication dispatch" correction plus explicit
operator retirement in /home/voc/projects/snodec/core-ci. Branch rewrite/slim-graph-controller,
HEAD 032cb34. This is a local fix with focused verification only. NO commit, push, deploy,
restart, GitHub mutation, or retire/retry of r169 (execution 1a7d0b93…:169), now or as a test.

READ FIRST: AGENTS.md; docs/proposals/unconfirmed-publication-recovery.md (the agreed design;
it overrides anything here that looks broader); src/Pipeline.cpp, Pipeline.h, Graph.cpp,
Graph.h, GraphStore.cpp, main.cpp (routes at 265-280); ui/app.js 167-187;
tests/pipeline.py (fixture 161-290, cases 400-440, 510-525, 655-710);
tests/ui_enrollment.py (fixture 80-140).

WORKSPACE RULES
- Leave these dirty files alone: README.md, deployment/sandbox/README.md,
  docs/deployment/README.md, and WORK_LOG.md unrelated hunks (append only; 682 unrelated added lines are interleaved elsewhere).
- docs/proposals/unconfirmed-publication-recovery.md is owned by this task: update its status
  section and keep its content. Keep the docs/improvements/ and other docs/proposals/ files.
- No checkout, reset, stash or extra source checkout. Don't touch .qtcreator/ or
  .local-credentials/, don't inspect any private/credential directory, don't edit the framework.
- Persistent build: build/codex/slim-controller. Temporary checks:
  /tmp/codex/build/core-ci/unconfirmed-dispatch-fix/<case>.
- Style: braces on every compound statement, one return at the end of each function, no
  avoidable break/continue, changed lines at most 160 chars, the committed canonical format
  files, and the format-check target must pass for src/ and tests/.
- Don't add any queue, persisted state/flag, cache, timer, thread, retry, pagination or
  recovery loop. Don't log request bodies, headers, tokens or raw response bodies. Defer
  HttpClient send()==false classification and the status-0 wording (no HttpClient changes).

BUDGET: src/ is currently 3510 physical lines.
- Core: +8 to +12 lines (cap 3522).
- Retirement: +15 to +25 lines (cumulative cap 3547).
- UI: +8 to +15 lines (ui/app.js).
- Tests: core +60 to +100, retirement +40 to +60.
Try for less. If a part can't fit, stop that part and report the exact measured overrun and
the alternatives below. Don't invent an architecture to compress code.

PART A — CORE DIAGNOSTICS (Pipeline.cpp, GraphStore.cpp)
A1 Dispatch logging (Pipeline::dispatch, the uncertain branch now at 152-153). Log
   {"error":"dispatch outcome is uncertain; assignment retained","execution":id,
    "stage":"publication"|"build","status":response.status,"submitted":response.submitted,
    "transport":response.error}.
   response.error is only our fixed HttpClient/GitHub strings; never log response.body.
   Classification is unchanged: assignment kept, nothing replayed. Optionally give the
   definite-reject branch the same fields if it costs at most 1 line; the persisted
   failure reason stays unchanged.
A2 Status dispatch logging (refreshStatus 577-579). Same fields plus "generation":generation.
   Keep the statusSent/refreshing coalescing exactly as it is.
A3 Unbound run search (inspect, runId-null branch 487-514). Classify the listing BEFORE any
   binding:
   - "unreadable": status != 200, non-empty error, body not an object.
   - "incomplete": workflow_runs not an array; total_count not an unsigned number;
     workflow_runs.size() > 100; total_count != workflow_runs.size(); or any entry that
     lacks an object actor, an unsigned id, or a string display_title. The result is
     "incomplete" whether 0 or 1 entries matched; never bind from an incomplete page.
   - On a complete list, count matches (actor id == controller_app_bot_id and
     display_title == "Publish "/"Build " + label):
     - 0 → "no matching run". This is not proof of non-submission.
     - more than 1 → "ambiguous".
     - exactly 1 → read(id) through the existing path.
   Keep unchanged: known-run-ID/dispatch-ID reads (516), full accept() validation (actor,
   repository, path, ref, preflight commit/blob, attempt one), and webhook binding without
   any listing.
A4 Inspect outcomes. Every inspect callback body has {"execution","assignment_preserved":true,
   "inspection":<outcome>}, where outcome is one of: "runs read", "run unreadable",
   "jobs unreadable", or the A3 values. Jobs-read failure (now silently 202 at 478) must
   report "jobs unreadable". "runs read" means only that the reads finished and accept()
   was invoked. It does not claim that record/complete graph commits succeeded.
A5 Reconcile aggregation (519-553). Replace the shared pending counter with one transient
   shared struct {pending, unresolved array}. This replaces the existing
   completion-counting mechanism; it is not a new queue. Result:
   {200, {"inspected":count,"unresolved":[{"execution","inspection"}...]}}. Every outcome
   other than "runs read" goes into unresolved. Don't call anything "reconciled" or
   "resolved". Keep the restored-start preflight-absent failure path unchanged. grep
   README/docs/ui/tests for "reconciled" and update only what this task owns, reporting
   anything else.
A6 Activity (GraphStore::view 93-99). Replace the default "assigned; awaiting GitHub status"
   with "publication run unconfirmed" or "build run unconfirmed". It means no verified run
   binding yet; it does not mean the request was submitted, and it also covers the normal
   short dispatch window. No change to blockers, state or the public view schema.

PART B — EXPLICIT RETIREMENT (owner-approved policy relaxation; operator action only)
B1 Graph.cpp/Graph.h: add one predicate and one transition.
   bool retirable(const Json& node): the latest attempt has state == "publishing",
     publication_run is null, dispatch.publication is absent or null, and
     evidence.publication is absent or empty. Don't require or forbid preflight. Don't
     require superseded_by == "".
   void retire(Json& state, const std::string& id, const std::string& attemptId):
     require(latest attempt id == attemptId && retirable(node), ...), then call the existing
     failed(node, attemptId, "operator abandoned unconfirmed dispatch; remote outcome unknown").
     Keep the assignment, preflight and all history. No new state value or flag.
B2 GraphStore::view: add row["retirable"] = graph::retirable(node) before dispatch/preflight
   are stripped. It's derived, never persisted, and not added to publicView. This is the UI's
   ONLY eligibility source; there's no parallel policy in JS.
B3 Pipeline::retire(execution, attempt, callback): mirror Pipeline::retry through change().
   Atomic: a stale or changed attempt is rejected by the graph guard (409 via the existing
   change() mapping). No GitHub call and no remote cancellation.
B4 main.cpp: POST /api/work/retire, visible next to /api/work/retry, using the same
   parse/error pattern. Require body.execution (string), body.attempt (string) and
   body.acknowledge_unknown_outcome == true (a boolean true, nothing else); otherwise 422.
   Mutation authority is gated exactly like retry. No new auth system.
B5 Retry semantics: unchanged. The retirement's acknowledgement covers a later explicit
   /api/work/retry (a new rebuild revision). Don't parse failure strings and don't add a
   second acknowledgement. Nothing retries automatically.
B6 Late facts: unchanged code paths. accept()/observe may still record authenticated
   evidence on the retired attempt. complete() must not publish, verify a receipt for, or
   flip a failed attempt. Verify this; don't add code for it.
B7 ui/app.js (around 181): when execution.retirable && graph.mutations_allowed, show
   "Retire unconfirmed publication", disabled while busy. confirm() text must say:
   - the remote GitHub outcome is unknown and may still exist or have published;
   - this does not cancel anything remotely;
   - it records an operator decision, not a GitHub failure;
   - any later retry is a separate explicit action creating a new rebuild revision.
   Then POST {execution: execution.id, attempt: execution.attempt,
   acknowledge_unknown_outcome: true}.
B8 If B1 to B4 exceed +25 lines, STOP and report the measured lines and these options:
   (a) fold retire into Pipeline::retry with an attempt parameter (saves about 7 lines,
       but mixes two actions);
   (b) owner approval for the overrun.
   Don't choose either yourself.
   Accepted residual: retiring during the normal in-flight dispatch window is possible. A
   later 200 still records the dispatch ID on the failed attempt (the 136-141 guard checks
   only the attempt id). That is evidence retention, covered by the acknowledgement. Don't
   add process memory to prevent it.

TESTS (existing harnesses and public fixtures only; test-side controls allowed)
tests/pipeline.py: add choices plus synthetic-GitHub controls.
- "unconfirmed-dispatch": the publication POST is read, then the connection closes without
  a response and no run is created. Check:
  - the attempt stays publishing with no dispatch id;
  - the log carries status 0 / submitted true / a transport string and no body;
  - retry returns 409;
  - reconcile → unresolved "no matching run";
  - activity is "publishing: publication run unconfirmed";
  - restart sends no additional publication POST.
  Variant: the response is lost AFTER the run was created → reconcile binds exactly through
  full accept().
- Listing variants:
  - complete with one match → binds;
  - total_count > size with one match → "incomplete", no read of that run;
  - malformed entry → "incomplete";
  - two matches → "ambiguous";
  - 500 listing → "unreadable";
  - jobs 500 → "jobs unreadable".
  Also check the status-dispatch log has generation and that coalescing is unchanged.
- "retire":
  - a missing or false acknowledgement → 422;
  - a stale attempt → 409;
  - a bound run, a dispatch id, or publication evidence → 409 and retirable false;
  - success → failed with the exact reason, assignment/preflight/history kept, and no
    GitHub POST or cancel;
  - no automatic retry;
  - a late authenticated workflow_run/job report is recorded but the state stays failed,
    with no feed/receipt reads;
  - an explicit retry afterwards → 200 with a new revision.
- Keep the existing cases passing unchanged: preflight-publish-known, preflight-publish-absent,
  cold, recovery. Confirm the cold listing (260-263) stays complete under A3 (total_count ==
  entries, at most 100); if not, adjust only the fixture and explain why.
tests/ui_enrollment.py: give the "assigned" row the new activity text and retirable: true,
plus one non-retirable publishing row. Assert the button is shown or absent only from
`retirable`, the confirm text, and the exact POST body (add a fixture handler for
/api/work/retire).
Don't run the full suite. Run only the touched/new cases, ui_enrollment, and the format-check
target. The cold fixture's 611 request count is an existing fixture measurement, not
performance acceptance.

REPORT (and append to WORK_LOG.md after line 682; update the proposal's status section)
- Production, test and documentation changes listed separately, with file:line.
- Exact src/ line count (before 3510 → after), UI lines and test lines against the budgets.
- The commands run and their actual results, failures included verbatim. Local proof is not
  live acceptance; r169 is untouched.
- Any deviation, any part stopped under B8, and any remaining owner choice.
