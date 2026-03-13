# Plan: Confirmation-Read Pattern for SAFE_STOP Transitions

**Date**: 2026-03-02
**Status**: IN PROGRESS

## Context

CVC transitions to SAFE_STOP immediately when any external fault signal (brake_fault, motor_cutoff, steering_fault) reads non-zero from the RTE buffer. This causes false SAFE_STOP transitions because:

1. RTE buffers can contain stale data from the previous cycle
2. A single spurious CAN frame triggers an irreversible safety transition
3. No verification of fault signal freshness, E2E status, or consistency

## Design

Per-fault debounce counters with confirmation reads from Com shadow buffer and E2E status checks before committing to SAFE_STOP.

- **Threshold**: 3 consecutive 10ms cycles = 30ms (within 50-100ms safety response requirement)
- **E-stop**: NO debounce (ASIL D immediate response)
- **CAN timeout**: Already debounced via `CVC_HB_MAX_MISS=3`

| Signal | Confirmation Level |
|--------|-------------------|
| brake_fault | Debounce + Com read + E2E check |
| motor_cutoff | Debounce + Com read |
| steering_fault | Debounce only (not bridged via Com) |
| e-stop | IMMEDIATE (no debounce) |

## Phases

| Phase | Name | Status |
|-------|------|--------|
| 1 | TDD — Write test expectations | DONE |
| 2 | Implement ConfirmFault | DONE |
| 3 | Verify (tests + MISRA) | DONE (tests: 47/47 pass, MISRA: pending CI) |

## Files Modified

| File | Change |
|------|--------|
| `firmware/cvc/test/test_Swc_VehicleState_asild.c` | Mock Com_ReceiveSignal + GetRxStatus, 9 new tests, update existing |
| `firmware/cvc/src/Swc_VehicleState.c` | Include, debounce counters, ConfirmFault helper, replace Step 4 |
