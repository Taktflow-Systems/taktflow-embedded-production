# Lessons Learned: CAN Silence Detection Fix

**Date:** 2026-03-03
**Scope:** RZC, FZC, SC, Com BSW — intermittent SAFE_STOP in SIL

## 2026-03-03 — RZC CAN silence counter never reset

**Context**: Normal driving in SIL caused intermittent SAFE_STOP.
**Mistake**: `Safety_CanSilenceCounter` in `Swc_RzcSafety.c` was incremented every 10ms cycle but had no external API to reset it. After 200ms (20 cycles) it permanently latched CAN loss, disabling the motor.
**Fix**: Added `Swc_RzcSafety_NotifyCanRx()` to reset the counter, called from `Swc_RzcCom_Receive()`.
**Principle**: Every "silence/timeout counter" pattern must have a paired "notify/reset" API wired into the receive path. If a counter only goes up, it will always latch.

## 2026-03-03 — FZC CAN monitor was dead code

**Context**: `Swc_FzcCanMonitor` was correctly implemented with `Init()`, `NotifyRx()`, `Check()` APIs — but never wired into `main.c`, the RTE runnable table, or called from `Swc_FzcCom_Receive()`.
**Fix**: Added `Init()` call in `main.c`, registered `Check` as 10ms runnable, called `NotifyRx()` from FZC Com receive.
**Principle**: New modules need 3 wiring points: init, periodic scheduler, and data source. Missing any one makes the module dead code.

## 2026-03-03 — SC heartbeat timeout too tight for Docker

**Context**: SC `SC_HB_TIMEOUT_TICKS=10` (100ms) + `SC_HB_CONFIRM_TICKS=3` (30ms) works on bare metal but not in Docker where CPU scheduling jitter can cause 150ms+ stalls.
**Fix**: Added `#ifdef PLATFORM_POSIX` guard: 30u (300ms) timeout, 5u (50ms) confirm. Used `#ifndef` wrapper so unit tests can override.
**Principle**: Safety timing constants need platform-specific tuning. SIL containers have 3-5x the jitter of bare metal.

## 2026-03-03 — Com BSW had no RX deadline invalidation

**Context**: AUTOSAR Com specifies `ComRxDataTimeoutAction = REPLACE` to zero-fill shadow buffers when RX timeout expires. Our Com module had the config fields (`TimeoutMs`) but no `Com_MainFunction_Rx()` to enforce them.
**Fix**: Implemented `Com_MainFunction_Rx()` with per-PDU deadline counter, wired into all 3 ECU schedulers between CAN RX and SWC runnables.
**Principle**: Config without enforcement is false confidence. If a timeout value exists in the config struct, there must be runtime code that uses it.
