# Execution evidence and live activity

The execution state machine remains waiting, building, publishing, published and failed.
`activity` explains what is currently known within those states. In particular, building
can mean a completed build waiting for the feed writer; publishing can mean an assigned
workflow that is still queued on GitHub. Neither state by itself proves a running job.

## Recorded facts

Each attempt has one `evidence` JSON object in MariaDB. Its optional `build` and
`publication` members contain facts from the verified assigned attempt-1 GitHub run:

| Field | Meaning |
| --- | --- |
| `run_id` | Assigned run, bound to the immutable run identity |
| `status` | Verified GitHub run status |
| `job_id`, `job_status` | Unique assigned package or privileged publisher job, when available |
| `started_at` | GitHub job start, accepted only for an in-progress or completed job |
| `completed_at`, `conclusion` | GitHub job completion facts, accepted only for a completed job |

The controller reads the attempt-specific jobs endpoint after verifying the repository,
controller actor, workflow blob and assignment title. It reuses the selected job for artifact
verification or publication-failure classification; it does not issue a second jobs request
for those consumers. A queued run's `run_started_at` is not treated as an actual job start.
Public GitHub evidence may remain incomplete, particularly before a queued run has its
assignment title. An unverified run never supplies a queued/running label.

Reports merge facts monotonically: delayed queued/in-progress observations cannot replace
completion, nulls cannot erase facts, and established job IDs/timestamps/conclusions cannot
change. Conflicting facts leave existing evidence and assignments intact. Terminal history
can receive missing evidence without changing its outcome. The known failed-build outcome
and its completion evidence commit together. No uncertain mutation is automatically retried.

`GraphStore::view()` and the existing local SSE stream expose `evidence`, `build_job`,
`publication_job` and the derived `activity`. `build_job` continues to expose previously
recorded successful-build timing when no early observation exists. The UI uses activity
labels while preserving the raw machine state in technical details. Expected timestamps
are Pending; missing facts for terminal work are Not recorded. Durations use recorded
start/completion times; no elapsed-time timer or polling loop is introduced. Publisher job
start/finish is distinct from `receipt.publication_verified_utc`, which requires verified
feed publication. Compiler/test summaries retain their existing validated provenance.

The GitHub status-page integration can whitelist these public facts from the existing graph
view. It should consume this evidence, rather than implement a second timing/status collector.

## Database setup

Fresh installations use `sql/schema.sql`. For an existing installation, take the documented
backup and stop core-ci. Use the administrator's protected connection configuration to add
this column before starting the new controller:

```sh
mariadb --defaults-extra-file=/secure/database-admin.cnf core_ci <<'SQL'
ALTER TABLE attempts
    ADD COLUMN evidence LONGTEXT NOT NULL DEFAULT '{}' CHECK (JSON_VALID(evidence));
SQL
```

Historical rows start with empty evidence; their sources, assignments, results and package
identities remain unchanged. Existing timestamps are not fabricated. The runtime account
still has no DDL privilege, and the application does not alter the database automatically.
Do not apply this ALTER twice. Reconnection still uses SNode.C's normal connection behavior.

The GitHub job fields and attempt endpoint are documented in
[GitHub's workflow-jobs REST API](https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt).
Notification actions are documented under
[workflow_job webhooks](https://docs.github.com/en/webhooks/webhook-events-and-payloads#workflow_job).
No workflow credentials, signing keys, worker hooks or GitHub dispatch protocol change.
