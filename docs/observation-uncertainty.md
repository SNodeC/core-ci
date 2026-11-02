# Bounded observation reads and unresolved evidence

This change is not a fix for an identified root cause. The cause of the interrupted
recovery's historical no-response outcomes remains unknown. Recovery after an
arbitrary outage is not guaranteed: this is bounded best-effort observation.
No workflow, pin, wire contract, publication authority, bootstrap or listener
configuration changes. No SNode.C changes are required.

## Transport policy

Each `https()` GET owns an independent budget of at most three transport attempts.
A connect ERROR, TLS end before an HTTP request, or connection end before a complete
HTTP response may start a new identical GET directly from the final callback.
There is no delay, timer, polling, shared budget or whole-observation restart.
Native retry, retryOnFatal and reconnect remain explicitly false. TLS CA and
hostname verification stay enabled and unchanged on every attempt.

The first final event for each attempt consumes its completion callback. Later
response, parse-error or disconnect callbacks cannot change its classification,
repeat delivery or start another reissue. Each call delivers at most one final
result; this does not guarantee that every request eventually completes.

Every parser-reported response is final, with its numeric status preserved.
403, 429, 503 and Retry-After never authorize reissue. Connect FATAL/DISABLED,
client setup and HTTP delivery/parse failures are final. POSTs are never retried,
including installation-token minting and workflow dispatch. Signing/publication
mutations and database operations are never retried. A token failure, including a synchronous signing failure, after an
attempt has been selected records read uncertainty immediately; it gets no token
retry even if its cause might be transient.

SNode.C synthesizes status zero for an unanswered request at disconnect. The
controller classifies that as no response, never as server "HTTP 0". TLS failure
causes are unavailable through these public callbacks; the diagnostic says so.
`State::what()` is not used: its public API accepts arbitrary error strings, so
stable secret-free contents cannot be established for every connect origin.

The installed parser rejects status 600 before reporting a response. The observed
result is a terminal parse failure after one connection, not a reported 600 and
not an eligible transport reissue. The controller preserves statuses exposed by
the response callback; it cannot recover a rejected status from an unexposed
parser result. Rejection of out-of-range 600 alone is not evidence of a framework
defect ([HTTP status semantics](https://www.rfc-editor.org/rfc/rfc9110.html#section-15)).

## Safe failure context

Transport diagnostics contain the method, HTTPS host, an API path without its
query only for api.github.com, a controller-written phase, and the attempt count.
Other hosts expose the origin only, without signed paths or ports. Response
status text is converted to a number rather than echoed verbatim. Headers,
bodies, tokens, queries, signed URLs and remote parser/exception messages are
never used as diagnostics. JSON errors in token(), api() and checked() use the
fixed label "Invalid JSON" plus the exception id, without exception what().
Existing controller-written semantic errors remain controller-written.

Each failed transport attempt uses application().error(), which is enabled at
runtime log level 2 (ERR). Earlier failures remain logged after eventual success.
The fixture verifies the actual log file at that level, using the existing
`--enforce-log-file` setting solely in the test. Production logging configuration
is untouched; retention/delivery follows its existing logging destination.
Only the final outcome of an exhausted call reaches graph uncertainty. Dispatch
acknowledgements retain the safe context as an execution fact without replay.

## Durable uncertainty and clearance

The existing attempt uncertainty object is `{at, stage, kind, reason}`. Its meaning
is: **Latest recorded failure still awaiting the required successful check;
verified facts retained.** It does not say that the remote job failed, or that
the latest completed status read failed.

`kind` is the check level required for clearance, not the latest failure's phase
(the phase is in reason). Early token/run/transport/parse errors use null observed
facts through applyEvidence and record `read`, without invented run facts or
readiness scanning. Artifact/receipt failures after a valid run read record
`evidence`. A later read failure keeps an existing same-stage evidence requirement
while updating a changed reason. It never downgrades that requirement. A
successful status-only read clears only same-stage read uncertainty. Validated
build or publication completion clears evidence uncertainty through the existing
transition. Invalid receipt identity remains unresolved evidence and cannot
release dependents. Receipt identity rejection records that diagnostic in Graph;
the caller may receive successful Store commit status. That status means the
observation was recorded, not that publication was verified. The uncertainty
remains visible in the graph/UI.

Every diagnostic write/clear checks the committed latest attempt id and its exact
awaited stage (Building/build or Publishing/publish). A dispatch acknowledgement
may change other facts without suppressing a diagnostic. Old attempts, Built,
Failed, Published, retired work and late build errors during Publishing gain no
new uncertainty. Webhooks select the attempt's current assignment stage before
observation, so a late build notification cannot become a publish observation.
Failed/retired history retains its uncertainty. Retry remains
restricted to definite Failed work and creates a new attempt, preserving history.

Legacy objects without kind are evidence-level; without stage they refer to the
current guarded awaited stage. This interpretation is used only when comparing
or clearing; there is no migration or normalization write. Equal normalized
stage/kind/reason preserves the entire existing object byte-for-byte. `at` means
**first recorded for the current reason**, not last checked. A changed reason or
a new failure after clearance gets a new at. Strengthening read to evidence for
the same reason preserves its first-recorded at. Identical failures cause no UPDATE,
revision increment or SSE message.

## Authority and ordering

The single-writer assumption remains: one active controller, one MariaDB
connection. Its deque of command sequences keeps each read's appended UPDATE in
the same sequence; that sequence drains before the next, with autocommit enabled.
Diagnostic writes and clearances therefore follow Store commit order, not request
start order. No lock, revision guard, sequence field or cache is added.

Store remains the only commit/dispatch/SSE boundary. Failed or uncertain commits
report a plain attempt-correlated error, without SSE, commands, recursive error
writes or retries. Valid publication and exact dependent authority still commit
together, then dispatch immediately. No readiness logic or immutable input changes.

There is no polling. If a decisive observation exhausts its attempts, its stage
stays awaited and its uncertainty persists until a later webhook observation,
the next startup recovery pass, or an owner-initiated Reconcile supplies the
required successful check. There is no additional startup pass or dispatch replay.

## Presentation and validation

The existing activity line shows `Unresolved <stage> <read|evidence>: <reason>`,
with full detail and meaning in its tooltip. Expanded facts label at as first
recorded for the current reason. Badge, columns, layout and job timestamps retain
their meanings. SSE continues to carry committed field updates only.

Focused checks use local trusted TLS fixtures, isolated real MariaDB, public
Controller/Graph/Store boundaries and Chromium with changing SSE data. Test-only
linker redirection of the HTTPS function verifies real token/API JSON parsing;
there is no production injection hook. Installed Valgrind checks native client
teardown without framework or controller lifetime wrappers. Exact commands and
results are recorded in WORK_LOG.md.

Live acceptance is not executed by this task. No deployment, restart, GitHub
mutation, webhook redelivery, reconciliation, build, tag or pin change is allowed.
The future clean single-restart interruption acceptance remains outstanding.
