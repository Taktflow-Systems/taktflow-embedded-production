# PROCESS â€” SIL/HIL Divergence Preflight

**Date**: 2026-03-10  
**Scope**: Lessons from repository scan on where SIL confidence can diverge from HIL reality.

## What We Learned
1. The highest-risk failures were not random defects; they clustered at architecture boundaries.
2. MCAL stubs carrying both simulation dynamics and fault injection became a hidden behavior layer.
3. Platform-guarded safety behavior (`PLATFORM_POSIX`) created policy differences between SIL and target runs.
4. Green SIL status did not guarantee bench readiness when recovery semantics and relay-state contracts drifted.
5. Comments and code can silently diverge (example class: relay state meaning vs recovery condition).

## Why This Matters
- HIL applies physical constraints and timing simultaneously, exposing layered assumptions that SIL can tolerate.
- A mature-looking process stack (traceability/tests/CI) can still hide bench-critical gaps if equivalence is not enforced.

## Actionable Rules
1. MCAL boundary is a safety boundary, not a convenience boundary.
2. Safety decisions must be platform-equivalent unless deviation is explicitly justified and tested.
3. Recovery conditions must use unambiguous signal semantics (document + code + tests aligned).
4. Every bench-critical safety claim requires an executable preflight check.

## Immediate Backlog
- Add nightly HIL-preflight static audit with severity gating.
- Fix recovery gate relay-state condition inconsistencies.
- Fix brake feedback ordering and steering plausibility reference selection.
- Reduce/contain platform-specific branches in safety-critical logic.
- Calibrate timing and plausibility constants with measured hardware data.

## Success Signal
Preflight nightly trends toward zero critical findings, and each resolved finding has code+test evidence linked to the change.
