# Plan: SAFE_STOP Safe-State Actuation (State-Gated CAN TX)

**Date**: 2026-03-02
**Status**: DONE (Phases 1-2), Phase 3 pending CI verification
**Predecessor**: `plan-confirmation-read-safe-stop.md` (ALL DONE)

## Context

The confirmation-read pattern (plan-confirmation-read-safe-stop.md) is deployed and working correctly — it confirms genuine faults before committing to SAFE_STOP. However, post-deploy CAN traces still show a persistent SAFE_STOP loop:

**Root cause chain:**
1. CVC has no planner SWC — `Swc_CvcCom_TransmitSchedule` hardcoded `brake=0, steer=0` on every TX cycle
2. FZC receives `brake=0` via CAN 0x103 → RTE freshness is OK (E_OK), but...
3. FZC's `Swc_Brake` detects command timeout via `Rte_Read` freshness, not value — but once FZC has already latched a fault, the latch persists for 500ms (50 fault-free cycles) even with fresh data
4. FZC sends `Brake_Fault=3` (LATCHED) + `Motor_Cutoff=1` persistently via CAN 0x210/0x211
5. CVC confirmation-read correctly confirms these as genuine → enters SAFE_STOP
6. `BswM_Action_SafeStop` was an empty stub — did nothing
7. CVC continues sending `brake=0` → FZC never clears latch → infinite fault loop

**Fix**: State-gate the CvcCom TX to send safe-state values (`brake=100%`, `steer=0`, `torque=0`) when vehicle state is SAFE_STOP or SHUTDOWN.

## Phases

| Phase | Name | Status |
|-------|------|--------|
| 1 | TDD — Write test expectations | DONE |
| 2 | Implement state-gated TX + BswM body | DONE |
| 3 | Verify (tests + MISRA + CI) | IN PROGRESS |
| 4 | Deploy to VPS | PENDING |

## Files Modified

| # | File | Change |
|---|------|--------|
| 1 | `firmware/cvc/test/test_Swc_CvcCom_asild.c` | Capture Com_SendSignal values, mock VehicleState_GetState, 4 new tests |
| 2 | `firmware/cvc/src/Swc_CvcCom.c` | `#include "Swc_VehicleState.h"`, `CVC_SAFE_BRAKE_CMD`, state-gate brake=100 in SAFE_STOP/SHUTDOWN |
| 3 | `firmware/cvc/src/main.c` | Update BswM_Action_SafeStop comment (no functional change) |

## Design Details

### State-gate in `Swc_CvcCom_TransmitSchedule`

Reads `Swc_VehicleState_GetState()` every TX cycle. If state >= `CVC_STATE_SAFE_STOP`, overrides brake to 100% (max). Steer and torque remain 0 (already safe defaults).

| Signal | Normal (RUN/INIT) | Safe-State (SAFE_STOP/SHUTDOWN) |
|--------|-------------------|----------------------------------|
| steer_angle (Com 6, sint16) | 0 (center) | 0 (center) |
| brake_pressure (Com 7, uint8) | 0 (no brake) | 100 (max brake) |

### BswM_Action_SafeStop

Updated comment to explain that cyclic TX handles actuation. TODO:POST-BETA for DTC reporting.
