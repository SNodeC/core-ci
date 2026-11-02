# Local startup-recovery activation and blocked controlled-chain acceptance

2026-11-02: startup recovery implementation committed as
1e76e9aedf7bba0560b09c8373dbb8ad8c9d5e10 on feat/graph-controller in
/home/voc/projects/snodec/core-ci-new. Not pushed or merged. Production remains
2348 C++ physical lines; prior focused checks are in startup-recovery.md.

The owner authorized local activation, one fresh core-triggered full chain, then
a deliberate stop during active builds and restart only after GitHub finished
those assigned builds. No manual reconciliation or completion redelivery.

## Completed

Gracefully stopped old PID 412713 and its owned MariaDB, then started the tested
recovery binary as detached PID 487201 with the unchanged sandbox database,
GitHub identities/pins, certificate and secret/key PATHS. No data reset/migration,
workflow/feed changes, key inspection or production SNodeC resource operation.

Binary: /tmp/codex/build/core-ci/startup-recovery-20261102/core-ci.
SHA256: 185c18b622e7db3c6c2535fc2ba44ff9131f21e08a7da38ff58b36f2af260777.
Management localhost:8080 and native TLS 0.0.0.0:8443 listen. A local HTTPS GET
with real hostname gw-home.vchrist.at returned 404 with certificate verification
successful (curl verify result 0). Controller is left running locally.

Created exactly one sandbox demo-core tag v2.1.20 at approved source
fc3acdb94779dafb879c183143c20e19b435e766. Tag POST succeeded; no second tag or
repeat mutation. Read-only observers and a one-shot active-build interruption
driver were armed. The driver could stop only after five matching core runs
existed and at least one actual job was in progress.

## Verified blocker, before the planned outage

GitHub push delivery 3851760462609776640 at 10:35:35.454 UTC returned status 502,
failed to connect to host. Its duration was 0.85s. This was BEFORE any controlled
interruption, with the new controller listening locally.

Public HTTPS address https://gw-home.vchrist.at:8443/github/webhook was also
unreachable from Jupiter: curl connection failure, HTTP code 000. Public DNS
resolved to 212.241.96.69. Jupiter currently has only LAN IPv4 10.20.99.86 on
wlp2s0f0 (plus loopback). The documented existing home forwarding route is WAN
TCP 8443 -> 192.168.1.239:8443. That documentation is historical; current router
configuration was not inspected or changed. A reachable local listener does not
establish public GitHub reachability across the current network arrangement.

At 10:38:59 UTC verified: zero persisted v2.1.20 executions and zero matching
GitHub core runs. Thus the source event never selected work. No build-completion
event was redelivered/retriggered, no manual reconciliation/retry occurred, and
the planned active-build interruption was NOT performed. This is not a passed
restart-recovery full-chain acceptance.

Stopped only diagnostic observers/interruption driver, preserving raw evidence
under /tmp/codex/build/core-ci/restart-chain-20261102. The live controller and
original assignments remain intact. Automatic startup observation can update
unfinished history by design; no assignments were deleted/rebound/replayed.

## Needed to continue

Restore the existing public forwarding route to the current machine, or supply
an approved existing reachable callback route. No tunnel, network change or new
infrastructure was invented. Once reachability is restored, select the existing
v2.1.20 tag exactly once (no selection exists), then perform the authorized stop,
wait for assigned GitHub builds, restart, and observe automatic completion.
Creating another source tag or replaying completion notifications is unnecessary.
