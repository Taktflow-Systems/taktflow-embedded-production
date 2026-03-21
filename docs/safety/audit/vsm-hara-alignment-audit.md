# Vehicle State Machine — HARA Alignment Audit

**Date:** 2026-03-21
**Scope:** Full chain HARA → SG → TSR → SSR → Swc_VehicleState.c

## Result: 85% ALIGNED — 2 Critical Defects

### Critical Defects

| # | Issue | TSR Says | Code Does | Impact |
|---|-------|----------|-----------|--------|
| **1** | SC_KILL override broken | Any state + SC_KILL → SHUTDOWN | INIT/RUN + SC_KILL → SAFE_STOP | SC cannot force system shutdown |
| **2** | NVM state persistence missing | SSR-CVC-019: persist state + faults to NVM | Not implemented | Reset-washing vulnerability |

### Major Findings

| # | Issue | Status |
|---|-------|--------|
| 3 | Motor/brake/steering fault treated as CRITICAL (→SAFE_STOP) | Correct per HARA ASIL D, but TSR-035 table is ambiguous |
| 4 | CREEP_FAULT event not in TSR-035 table | Code correct (SG-001), TSR gap |
| 5 | Motor cutoff 30ms debounce may exceed 50ms FTTI | Needs WCET analysis |

### State Transition Audit

| Transition | TSR-035 | Code | Status |
|-----------|---------|------|--------|
| INIT→RUN (self-test pass) | Required | Implemented | **ALIGNED** |
| INIT→SAFE_STOP (self-test fail) | Required | Implemented | **ALIGNED** |
| INIT→SHUTDOWN (SC_KILL) | Required | Goes to SAFE_STOP | **MISALIGNED** |
| RUN→DEGRADED (minor fault) | Required | Implemented | **ALIGNED** |
| RUN→SAFE_STOP (critical fault) | Required | Implemented | **ALIGNED** |
| RUN→SHUTDOWN (SC_KILL) | Required | Goes to SAFE_STOP | **MISALIGNED** |
| DEGRADED→RUN (5s clear) | Required | Implemented | **ALIGNED** |
| SAFE_STOP→SHUTDOWN (stopped) | Required | Implemented | **ALIGNED** |
| SAFE_STOP→INIT (recovery) | Not in TSR | Implemented | **EXTRA** (defensible) |
| Any→SHUTDOWN (SC_KILL) | Required | PARTIAL | **MISALIGNED** |

### HARA → Code Traceability

| HARA HE | Safety Goal | Code Event | Transition | Status |
|---------|------------|------------|------------|--------|
| HE-001 (unintended accel) | SG-001 | PEDAL_FAULT_DUAL | RUN→SAFE_STOP | **ALIGNED** |
| HE-004 (loss of steering) | SG-003 | STEERING_FAULT | RUN→SAFE_STOP | **ALIGNED** |
| HE-005 (loss of braking) | SG-004 | BRAKE_FAULT | RUN→SAFE_STOP | **ALIGNED** |
| HE-012 (SC failure) | SG-008 | SC_KILL | Should be →SHUTDOWN | **MISALIGNED** |
| HE-017 (creep from rest) | SG-001 | CREEP_FAULT | RUN→SAFE_STOP | **ALIGNED** (TSR gap) |

## Required Fixes

1. **SC_KILL → SHUTDOWN**: Change transition table lines 89, 109, 178 in Swc_VehicleState.c
2. **NVM persistence**: Implement Swc_VehicleState_PersistToNvm per SSR-CVC-019
3. **TSR-035 update**: Add CREEP_FAULT event, clarify fault severity classification
4. **WCET analysis**: Verify motor cutoff 30ms debounce fits SG-001 50ms FTTI

## HITL Review Required

- [ ] Confirm SC_KILL → SHUTDOWN is the correct interpretation (not SC_KILL → SAFE_STOP → SHUTDOWN)
- [ ] Confirm NVM persistence is needed for bench (or defer to production)
- [ ] Confirm CREEP_FAULT → SAFE_STOP severity is correct
- [ ] Confirm 30ms motor cutoff debounce acceptable for FTTI budget
