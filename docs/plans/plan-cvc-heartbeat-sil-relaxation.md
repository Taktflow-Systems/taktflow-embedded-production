# Plan: CVC Heartbeat Timeout Relaxation for SIL

**Date:** 2026-03-03
**Status:** DONE

## Context

After the CAN silence detection fix (plan-can-silence-fix.md), SAFE_STOP is eliminated. However, CVC logs show persistent FZC/RZC heartbeat flapping:

- **96 FZC timeouts** and **35 RZC timeouts** in 20 minutes of uptime
- Pattern: `RUN → LIMP → DEGRADED → RUN` repeating every few seconds
- Root cause: E2E SM thresholds tuned for bare metal (deterministic 10ms ticks) are too tight for Docker CPU scheduling jitter (50-150ms stalls possible)

Same class of issue as the SC heartbeat fix (Phase 2 of plan-can-silence-fix.md), just on the CVC side.

## Root Cause Analysis

CVC uses E2E State Machine to monitor FZC/RZC heartbeats (polled every 50ms):

| Parameter | FZC (current) | RZC (current) | Problem |
|-----------|---------------|---------------|---------|
| Window | 4 slots (200ms) | 6 slots (300ms) | Small observation window |
| MaxErrValid | 1 | 2 | FZC: 2 missed 50ms slots = INVALID (100ms) |
| MinOkInit | 2 | 3 | — |
| MinOkInvalid | 3 | 3 | Recovery requires 150ms clean |

In Docker, a single CPU scheduling stall of ~100ms causes 2 consecutive missed heartbeat slots, exceeding FZC's `MaxErrValid=1`. RZC is slightly more tolerant but still flaps.

## Fix

Add `#ifdef PLATFORM_POSIX` / `#ifndef` guards to E2E SM constants in `Cvc_Cfg.h`, following the exact pattern used for SC in `sc_cfg.h`. ~3× relaxation for SIL:

| Parameter | FZC bare metal | FZC SIL | RZC bare metal | RZC SIL |
|-----------|---------------|---------|---------------|---------|
| Window | 4 (200ms) | 8 (400ms) | 6 (300ms) | 12 (600ms) |
| MaxErrValid | 1 | 3 | 2 | 5 |
| MinOkInit | 2 | 2 | 3 | 3 |
| MinOkInvalid | 3 | 3 | 3 | 3 |

Logic: wider window + higher error tolerance absorbs Docker jitter. MinOk values stay the same (don't weaken the "declare healthy" threshold, just widen the "declare sick" tolerance).

## Phases

| Phase | Name | Status |
|-------|------|--------|
| 0 | Add `#ifndef`/`PLATFORM_POSIX` guards to `Cvc_Cfg.h` | DONE |
| 1 | Verify tests pass locally | DONE |
| 2 | Commit, push, deploy, verify no flapping | DONE |

## Phase 0: Modify `Cvc_Cfg.h`

File: `firmware/cvc/include/Cvc_Cfg.h`

Wrapped each E2E SM constant with `#ifndef` + `#ifdef PLATFORM_POSIX` guards:

```c
/* E2E SM Configuration — FZC */
#ifndef CVC_E2E_SM_FZC_WINDOW
  #ifdef PLATFORM_POSIX
    #define CVC_E2E_SM_FZC_WINDOW        8u   /* 8 × 50ms = 400ms — Docker jitter margin */
  #else
    #define CVC_E2E_SM_FZC_WINDOW        4u   /* 4 × 50ms = 200ms */
  #endif
#endif
/* ... same pattern for all 8 constants */
```

Also wrapped `CVC_INIT_HOLD_CYCLES` — Docker boot is slower, relaxed to 10s (1000 cycles) for SIL.

**No test changes needed** — `test_Swc_Heartbeat_asilc.c` already blocks `Cvc_Cfg.h` with `#define CVC_CFG_H` and provides its own constants.

## Phase 1: Verify Tests

Run CVC heartbeat tests, all CVC tests, MISRA on `Cvc_Cfg.h`.

## Phase 2: Deploy & Verify

- Commit + push
- Deploy to Netcup
- Check CVC logs for 5+ minutes — expect 0 LIMP transitions

## Files Modified

| File | Change |
|------|--------|
| `firmware/cvc/include/Cvc_Cfg.h` | `#ifndef`/`PLATFORM_POSIX` guards on E2E SM constants + INIT hold |
| `docs/plans/plan-cvc-heartbeat-sil-relaxation.md` | This plan |
