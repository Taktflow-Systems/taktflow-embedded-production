# Plan â€” HIL Preflight Divergence Closure

**Date**: 2026-03-10  
**Scope**: Close SIL/HIL divergence risks before physical bench demos and recurring HIL execution.

## Objective
Establish a strict preflight gate so hardware runs are blocked when known architecture-divergence risks are present.

## Problem Statement
SIL can remain green while hardware behavior diverges when:
- simulation behavior leaks into MCAL boundaries,
- safety logic differs by platform guards,
- timing constants are Docker-tuned only,
- recovery semantics are inconsistent with relay state contracts.

## Closure Workstreams

### WS1 — Safety Path Equivalence — DONE
- Removed `#ifdef PLATFORM_POSIX` guards from safety-critical paths in
  `Swc_VehicleState.c` (HIL-PF-002/003) and `Swc_FzcSafety.c` (HIL-PF-004).
- Grace period constants now platform-conditional in headers (0 on bare metal),
  code paths are identical across platforms.
- ESM guard documented with waiver (HIL-PF-008).

### WS2 — MCAL Boundary Purity — DONE
- `IoHwAb_Inject.h` / `IoHwAb_Posix.c` / `IoHwAb_Hil.c` provide the unified
  sensor injection API — SWC code has zero `#ifdef` and zero direct MCAL calls.
- `Swc_FzcSensorFeeder` and `Swc_RzcSensorFeeder` call only `IoHwAb_Inject_SetSensorValue`
  and `IoHwAb_Inject_SetEncoderValue` — never `Spi_Posix_InjectAngle` or any MCAL stub.
- `Spi_Posix_InjectAngle()` and its steering injection state removed from `Spi_Posix.c`
  (dead code: no callers in SWC layer).
- `Spi_Posix.c` reduced to thin transport stub with only default oscillation defaults
  and UDP pedal override socket (direct fault-injection path, separate from SensorFeeder).

### WS3 — State/Recovery Correctness — DONE
- Fixed relay-state semantic mismatch (HIL-PF-005, CRITICAL):
  recovery latch, instantaneous guard, and tests all aligned to 0=killed, 1=OK.
- CI tests pass with correct relay semantics.

### WS4 — Actuation Logic Correctness — DONE
- Brake feedback: moved `IoHwAb_ReadBrakePosition` before deviation check
  so comparison uses current-cycle sensor data (HIL-PF-006).
- Steering plausibility: now compares previous rate-limited output vs actual
  feedback instead of raw command angle (HIL-PF-007).

### WS5 — Timing and Calibration — BLOCKED
- Heartbeat/grace/timeout constants validated on physical ECU bench — DONE.
  ECUs running at 1x timing confirm firmware constants are correct.
- TODO:HARDWARE Create calibration dataset for SC torque-current plausibility LUT.
  Electronic load procured (BOM complete), not yet connected to bench.
  **Blocked on**: electronic load physically connected to bench.

## Execution Order
1. Blockers first: recovery semantics, E-stop/relay bench proof, safety-path equivalence.
2. Actuation logic fixes and tests.
3. MCAL boundary refactor for simulation/injection split.
4. Timing/LUT calibration and correlation baselines.

## Deliverables
- HIL preflight audit script and nightly workflow.
- Updated tests for recovery/brake/steering logic.
- Documentation updates for state and relay semantics.
- Correlation report template (SIL vs HIL).

## Exit Criteria
- Preflight nightly has no critical findings.
- E-stop bench test demonstrates relay drop and torque collapse within target latency.
- Recovery path tests pass with consistent relay-state semantics.
- Safety logic parity policy is enforced in CI.
- LUT and timeout values are backed by measured hardware data, not placeholders.
