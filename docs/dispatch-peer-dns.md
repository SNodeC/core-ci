# Dispatch stalls caused by peer-name reverse DNS

The 2026-11-03 SNodeC selection committed 178 executions, including 76 authorized
core builds. GitHub showed no package workflow runs. All 76 dispatch outcomes
were unresolved: 41 connections ended before a response and 35 before the HTTP
request. No HTTP status was received; this is not an HTTP 0 server response.

An isolated executable using the actual https() helper on vchrist reproduced the
problem with harmless authenticated-invalid GET requests: 1 and 5 parallel calls
completed in 0.222 and 0.413 seconds, 20 in 5.626 seconds, 40 in 16.086 seconds,
and all 76 failed after 89.263 seconds. A separate Python 76-connection read probe
received all 76 HTTP 401 responses in 0.323 seconds. These are probe timings, not
package-build timings or a controller-overhead benchmark.

A trace of the isolated native executable shows five-second event-loop gaps
before peer-address construction. SNode.C ConfigAddress::getSocketAddress uses
getnameinfo with NI_NAMEREQD unless numeric reverse names are selected. Both
local and remote addresses are constructed before TLS proceeds. A test-only libc
interposer forcing numeric getnameinfo formatting, without changing TLS, gave
76 HTTP 401 results in 1.406 seconds on vchrist. This isolates reverse lookup
from GitHub execution, assignment size, concurrency policy and database work.

Use the installed public Local::setNumericReverse and Remote::setNumericReverse
settings for outgoing HTTPS clients and both HTTP/TLS listeners. This changes
endpoint display names only. Forward DNS, SNI, hostname verification, CA checks,
HTTP methods, bounded GET reissue, no-POST-retry and immutable assignments stay
unchanged. No framework modification, timeout increase, cache, queue or new
controller scheduling mechanism is needed. The isolated regression executable
interposes libc getnameinfo to count forbidden nonnumeric calls and returns
EAI_NONAME rather than contacting DNS; before the fix it detects such calls.

The owner explicitly requested an empty database for the next run. Preserve the
interrupted database as evidence before replacing only the SNodeC database. The
VOC instance and its database must remain untouched. This does not turn missing
GitHub runs into proof of rejection or introduce dispatch replay.
