# Phase 1: pure core

Implemented the owner-approved SPEC.md through the I1 clarification (SHA-256
26dec55f5ebf6518061a7e1f0266f3a5a52c8831e2d4a8ac1c8122381f3c6b68).
The SPEC changes are separate commits, applied only with the supplied hash-checked
scripts/patch. No archive implementation was used as a reference.

## Architecture and scope

The immutable C++ pipeline owns package, target and feed definitions. Its builder
validates references, source-repository overlap and cycles, and produces a
name-tied topological order per target. SemVer precedence compares arbitrarily
large numeric identifiers without integer conversion; build metadata is retained
for dispatch but ignored for precedence.

Engine state is limited to releases, unit assignments/publications/failures and
revision counters, feed queues/slots, and monotonic event time. Readiness, settled
state, desired inputs and blockers are derived together. classify() implements
section 4.7 and ignored events do not advance time. The engine owns no I/O,
clock, random generator, JSON, thread or persisted ready queue. Retry/resume
re-emissions keep the existing assignment and are exempt from I1; queued
publications are revalidated before scheduling. Status renders text and DOT.

No persistence, SNode.C adapters, real pipeline, workflows, installation or live
acceptance are part of this phase. I5 replay remains Phase 2 work.

## Verification

Configuration: GNU C++ 16.2.0, C++20, Debug, STL only for core; all core/test
compilation uses -Wall -Wextra -Wpedantic -Werror. Commands from the repo root:

```sh
mkdir -p build/codex/phase-1-debug/tmp
TMPDIR="$PWD/build/codex/phase-1-debug/tmp" cmake -S . -B build/codex/phase-1-debug -DCORECI_WITH_SNODEC=OFF -DCMAKE_BUILD_TYPE=Debug
TMPDIR="$PWD/build/codex/phase-1-debug/tmp" cmake --build build/codex/phase-1-debug --parallel 2
cmake --build build/codex/phase-1-debug --target format-check
ctest --test-dir build/codex/phase-1-debug --output-on-failure
```

Final CTest: 11 passed, 0 failed, 0 skipped; real time 69.19 seconds. The randomized
test recorded seed=12639937, runs=10000. It covers I1-I4, caps and uncapped builds,
accepted/ignored events, failures, retries, resume, supersession, and successful
quiescence against independently tracked latest tags and published input values.
S10 also explicitly covers retry/resume while an upstream release is unsettled.
All scenarios compare complete command lists for their applied events.

Formatting checks passed. All eight canonical configuration/module copies are
byte-identical to the local baseline. Every production C++ line is at most 160
characters. tests/Check.h is 28 lines (limit 60). Review found no competing state
or scheduling authority. No sanitizer or clang-tidy execution is claimed. The canonical .clang-format,
.clang-tidy and .cmake-format.py each retain a baseline blank line at EOF;
git diff --check reports those three only. Other staged paths pass the check.

## Accounting and files outside the SPEC layout

Production C++: +980/-0 physical lines. Test C++: +1305/-0 physical lines.
CMakeLists.txt: +70/-0 build lines. The core is a fresh implementation explicitly
authorized by Phase 1; reduction/reuse of the archived implementation is excluded.
Canonical tooling: +754/-0 lines, separate from controller and test code.
Documentation changes are reported separately in the phase handover.

The following extras are deviations from the SPEC layout, included to satisfy the
previously supplied global formatting/development instructions and subject to
owner review:

- .clang-format: canonical C++ formatting rules.
- .clang-format-ignore: canonical formatting exclusions.
- .cmake-format.py: canonical CMake formatting rules.
- .clang-tidy: canonical analysis configuration; optional analyze target.
- .clangd: canonical editor configuration. For this nested build, start clangd
  with --compile-commands-dir=build/codex/phase-1-debug.
- cmake/clang-format.cmake: mandatory byte copy; assumes src/, retained unchanged.
- cmake/cmake-format.cmake: mandatory byte copy; retained unchanged.
- cmake/format.cmake: mandatory byte copy; retained unchanged.
- AGENTS.md: records build layout and the top-level formatting integration, which
  discovers core/ and tests/ instead of the copied modules' src/ assumption.
- WORK_LOG.md: this decision/verification record, required by the supplied global
  engineering instructions.

No functional deviations from the amended SPEC are intended. No additional
checkout or temporary source workspace was created. The ignored persistent Debug
build remains available for development. Phase 2 has not started.
