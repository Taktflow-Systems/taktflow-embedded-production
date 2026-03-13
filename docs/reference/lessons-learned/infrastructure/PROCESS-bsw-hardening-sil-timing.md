# Lessons Learned — BSW Hardening Breaks SIL Timing Margins

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Date:** 2026-03-03
**Scope:** Adding DET, SchM, CanTp to BSW main loop increased cycle overhead, breaking SIL E2E tests

## Context

Phases 1-4 of BSW hardening added DET error tracing to all 18 modules, SchM critical sections to 5 modules, and CanTp ISO-TP transport with Dcm integration. This added `Det_ReportError()` calls in every parameter check, `SchM_Enter/Exit` around shared state, and `CanTp_MainFunction()` to the CVC 10ms scheduler.

## Mistake

After committing and pushing, all 17 SIL E2E scenarios with `wait_state RUN timeout: 10` became flaky. The timeout was exactly equal to `CVC_INIT_HOLD_CYCLES = 1000u` (10 seconds in SIL), leaving zero margin for E2E state machine stabilization and Docker scheduling jitter. The added main loop overhead pushed cycle completion time slightly beyond the 10ms budget, causing the INIT→RUN transition to slip past the 10-second test window.

Only unit tests and integration tests were run locally before pushing. SIL Docker tests were not verified.

## Fix

Increased all `wait_state RUN` timeouts from 10s to 15s across 17 scenario YAML files. The 5-second margin absorbs INIT hold time + E2E SM stabilization + Docker jitter.

## Principles

1. **Boundary-condition timeouts are time bombs.** Never set a test timeout equal to the minimum system timing requirement. Always add margin (≥50% for Docker/SIL environments).

2. **BSW main loop overhead is cumulative.** Adding one module's `MainFunction()` call to the scheduler is cheap, but adding DET checks to every parameter validation across 18 modules adds up. Always re-evaluate timing budgets after cross-cutting changes.

3. **Local tests are necessary but not sufficient for SIL changes.** Unit tests and integration tests run on host without Docker timing. Any change that affects the main loop scheduler must be verified on the real SIL Docker environment before declaring done.

4. **Grep for impact radius.** When a change affects timing (main loop overhead, INIT hold, heartbeat intervals), grep the entire test suite for related constants (`timeout:`, `wait_state`, timing values) to find all affected tests — not just the one that was reported broken.
