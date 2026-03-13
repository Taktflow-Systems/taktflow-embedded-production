# Plan: CAN Silence Detection Fix

**Date:** 2026-03-03
**Status:** DONE

## Problem Statement

During normal driving in SIL, vehicle state intermittently transitions to SAFE_STOP without fault injection. Root-cause analysis found 3 defects and 1 hardening opportunity:

| # | Finding | Severity | ECU |
|---|---------|----------|-----|
| 1 | RZC `Safety_CanSilenceCounter` never reset on CAN RX — latches after 200ms | **Bug (P1)** | RZC |
| 2 | FZC `Swc_FzcCanMonitor` is dead code — never wired into scheduler or Com | **Bug (P2)** | FZC |
| 3 | SIL heartbeat core timeout (100ms) not relaxed for Docker CPU jitter | **Design (P3)** | SC |
| 4 | No `ComRxDataTimeoutAction` equivalent for stale shadow buffers | **Hardening (P4)** | Com BSW |

### Reference: SC Does It Right

SC (`sc_can.c`) resets `bus_silence_counter = 0u` inside `SC_CAN_Receive()` on any valid RX, before `SC_CAN_MonitorBus()` checks it. This is the correct pattern.

## Phase Table

| Phase | Name | Status |
|-------|------|--------|
| 0 | RZC: Add `NotifyCanRx` API and wire into Com RX path | DONE |
| 1 | FZC: Wire dead `Swc_FzcCanMonitor` into scheduler and Com RX path | DONE |
| 2 | SC: Relax heartbeat timeout for SIL | DONE |
| 3 | Com BSW: Add shadow buffer timeout invalidation | DONE |
| 4 | Verify: rebuild, run tests | DONE |

## Phase 0: RZC — Add `NotifyCanRx` and Wire It

- [x] Add `void Swc_RzcSafety_NotifyCanRx(void);` to header
- [x] Implement: resets `Safety_CanSilenceCounter = 0u`
- [x] Call from `Swc_RzcCom_Receive()` on each cycle where CAN data is processed
- [x] Add unit test: 15 cycles → NotifyCanRx → 15 more cycles → no latch
- [x] Add unit test: 21 cycles without NotifyCanRx → latch confirmed

## Phase 1: FZC — Wire Dead `Swc_FzcCanMonitor`

- [x] Add `Swc_FzcCanMonitor_Init()` call in `main.c`
- [x] Register `Swc_FzcCanMonitor_Check` in RTE runnable table (10ms, priority 3)
- [x] Call `Swc_FzcCanMonitor_NotifyRx()` from `Swc_FzcCom_Receive()`

## Phase 2: SC — Relax Heartbeat Timeout for SIL

- [x] `SC_HB_TIMEOUT_TICKS`: 10→30 for PLATFORM_POSIX (300ms, 3x relaxation)
- [x] `SC_HB_CONFIRM_TICKS`: 3→5 for PLATFORM_POSIX (50ms)
- [x] `SC_HB_STARTUP_GRACE_TICKS`: 500→1500 for PLATFORM_POSIX (15s)
- [x] Bare metal values unchanged
- [x] `#ifndef` guards allow unit tests to override values

## Phase 3: Com BSW — Shadow Buffer Timeout Invalidation

- [x] Per-PDU RX deadline counter `com_rx_timeout_cnt[]` in Com module
- [x] Reset counter on `Com_RxIndication()` — fresh data arrived
- [x] `Com_MainFunction_Rx()`: increment counter; if `(counter * 10ms) >= TimeoutMs`, zero-fill all shadow buffers (AUTOSAR REPLACE action)
- [x] Declared `Com_MainFunction_Rx()` in `Com.h`
- [x] Wired into CVC RTE scheduler (priority 8, after CAN RX)
- [x] Wired into FZC RTE scheduler (priority 10, after CAN RX)
- [x] Wired into RZC RTE scheduler (priority 6, after CAN RX)
- [x] All 3 ECU Com configs already had `TimeoutMs` configured for RX PDUs

## Phase 4: Verify

- [x] BSW tests: 19/19 modules, 0 failures (including 21 Com tests with 2 new Rx timeout tests)
- [x] RZC safety tests: 18/18 passed (includes NotifyCanRx tests)
- [x] SC heartbeat tests: 24/24 passed (with `#ifndef` override fix)
- [x] FZC CAN monitor tests: 4/4 passed
- [x] FZC Com tests: 13/13 passed (with NotifyRx mock stub)
- [x] RZC Com tests: 6/6 passed (with NotifyCanRx mock stub)
- [x] CVC VehicleState tests: 52/52 passed
- [x] CVC Heartbeat tests: 38/38 passed
- [x] CVC EStop tests: 17/17 passed
- [x] FZC Safety tests: 18/18 passed
- [x] All SC tests (main, can, relay, selftest, e2e, plausibility): all passed
- [x] MISRA: 0 violations on all modified files (Com.c, Swc_RzcSafety.c, Swc_RzcCom.c, Swc_FzcCom.c, sc_cfg.h)
- [ ] Deploy to Netcup and 5-min smoke test (requires user action)

## Files Modified

| File | Change |
|------|--------|
| `rzc/include/Swc_RzcSafety.h` | Added `Swc_RzcSafety_NotifyCanRx()` declaration |
| `rzc/src/Swc_RzcSafety.c` | Added `Swc_RzcSafety_NotifyCanRx()` implementation |
| `rzc/src/Swc_RzcCom.c` | Call `Swc_RzcSafety_NotifyCanRx()` in Receive |
| `rzc/cfg/Rte_Cfg_Rzc.c` | Added `Com_MainFunction_Rx`, adjusted priorities |
| `rzc/test/test_Swc_RzcSafety_asild.c` | 2 new NotifyCanRx tests |
| `rzc/test/test_Swc_RzcCom_asild.c` | Added mock stub for `NotifyCanRx` |
| `fzc/src/main.c` | Added `Swc_FzcCanMonitor_Init()` call |
| `fzc/src/Swc_FzcCom.c` | Call `Swc_FzcCanMonitor_NotifyRx()` in Receive |
| `fzc/cfg/Rte_Cfg_Fzc.c` | Added `Com_MainFunction_Rx`, `Swc_FzcCanMonitor_Check` |
| `fzc/test/test_Swc_FzcCom_asild.c` | Added mock stub for `NotifyRx` |
| `sc/include/sc_cfg.h` | `#ifndef` guarded PLATFORM_POSIX timeout values |
| `shared/bsw/services/Com.c` | Added `Com_MainFunction_Rx()` with deadline monitoring |
| `shared/bsw/services/Com.h` | Added `Com_MainFunction_Rx()` declaration |
| `shared/bsw/test/test_Com_asild.c` | 2 new Rx timeout tests |
| `cvc/cfg/Rte_Cfg_Cvc.c` | Added `Com_MainFunction_Rx` to scheduler |
