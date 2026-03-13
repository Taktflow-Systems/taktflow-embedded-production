# Lessons Learned — Stale CAN Shadow Buffers Causing Spurious SAFE_STOP

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-03-01 to 2026-03-02
**Scope:** CVC enters SAFE_STOP within 2 cycles of restart due to stale fault data in Com shadow buffers
**Result:** Root cause identified and fixed — brake_fault/motor_cutoff TX changed from event-driven to cyclic

---

## 1. Root Cause: Event-Driven TX Leaves Stale Data in Receiver

FZC sent brake_fault (CAN 0x210) and motor_cutoff (CAN 0x211) as **event-driven** frames — only transmitted when fault was active:

```c
/* OLD: only sends when fault != 0 */
if (rteVal != (uint32)FZC_BRAKE_NO_FAULT) {
    FzcCom_TxPendBrakeFault = TRUE;
}
```

When the fault cleared, FZC stopped sending these frames. CVC's AUTOSAR Com shadow buffers retained the **last received value** — which was the fault value. After CVC restart (or any scenario where Com shadow buffers reset to 0 then receive old data), the stale fault data propagated through:

```
Com shadow buffer (stale bf=3, mc=1)
  → BridgeRxToRte (copies every 10ms)
    → RTE (brake_fault=3, motor_cutoff=1)
      → ConfirmFault (3-cycle debounce PASSES because value is consistently non-zero)
        → MOTOR_CUTOFF event → DEGRADED
          → BRAKE_FAULT event → SAFE_STOP
```

**Lesson:** In CAN communication, event-driven TX is only safe if the receiver has a timeout mechanism to invalidate stale data. If the receiver blindly trusts shadow buffer values, safety-critical signals MUST be sent cyclically so the receiver always has fresh data.

---

## 2. The Fix: Cyclic TX for Safety-Critical Status

Changed brake_fault and motor_cutoff from event-driven to cyclic (always send current status):

```c
/* NEW: always sends current value, even when 0 */
(void)Rte_Read(FZC_SIG_BRAKE_FAULT, &rteVal);
for (i = 0u; i < 8u; i++) { txBuf[i] = 0u; }
txBuf[2] = (uint8)rteVal;
(void)Swc_FzcCom_E2eProtect(txBuf, 8u, FZC_E2E_BRAKE_STATUS_DATA_ID);
(void)PduR_Transmit(FZC_COM_TX_BRAKE_FAULT, &pdu_info);
```

**Lesson:** For any signal that drives a safety state transition, prefer cyclic TX over event-driven. The bandwidth cost (8 bytes per 10ms frame) is trivial compared to the safety risk of stale data.

---

## 3. Diagnostic Logging Was Essential

The bug was invisible through static analysis alone. Multiple hypotheses were eliminated before adding `SIL_DIAG` fprintf logging:

| Hypothesis | How eliminated |
|-----------|---------------|
| FZC brake timeout fires before first CVC command | Already fixed (FirstCmdReceived guard) |
| E2E alive counter poisoning from stale data | FZC doesn't do E2E on RX (disabled due to separate bug) |
| Rte_Read returning E_NOT_OK | SIL Rte_Read always returns E_OK |
| CVC confirmation-read rejecting stale data | Confirmation-read passes because Com fresh read returns same stale value |

The breakthrough came from per-cycle diagnostic logging:

```
[VSM] c=1 st=0 ped=0 es=0 fzc=1 rzc=1 mc=0 bf=0 sf=0   ← clean
[VSM] c=2 st=0 ped=0 es=0 fzc=1 rzc=1 mc=1 bf=3 sf=0   ← stale fault appears!
[VSM] !! mc ONSET c=2 st=0 mc=1 bf=3 fzc=1 rzc=1
```

Fault values appeared at cycle 2 — before CVC even reached RUN state. This proved the data was stale, not from a live fault.

**Lesson:** When debugging CAN-layer data integrity issues, add per-cycle state dumps early. Static analysis of C source can't reveal runtime data flow through Com shadow buffers.

---

## 4. Docker stdout Buffering Trap

Initial diagnostic logging used `printf` (stdout), which produced **no output** in Docker containers. Root cause: C `printf` to stdout is **fully buffered** when there's no TTY (standard Docker behavior). `fprintf(stderr, ...)` is unbuffered by default.

```c
/* WRONG — buffered, no output in Docker */
#define VSM_DIAG(fmt, ...) (void)printf("[VSM] " fmt "\n", ##__VA_ARGS__)

/* CORRECT — unbuffered, immediate output */
#define VSM_DIAG(fmt, ...) (void)fprintf(stderr, "[VSM] " fmt "\n", ##__VA_ARGS__)
```

**Lesson:** Always use `stderr` for diagnostic logging in Docker/embedded SIL environments. Alternatively, call `setlinebuf(stdout)` or `fflush(stdout)` after each print, but `stderr` is simpler and more portable.

---

## 5. SIL_DIAG Variables Must Be Guarded

Adding diagnostic variables that are only used inside `VSM_DIAG()` macros causes `-Werror=unused-variable` in non-DIAG builds:

```c
/* WRONG — prev unused when SIL_DIAG not defined */
uint8 prev = current_state;
current_state = next_state;
VSM_DIAG("%s -> %s", names[prev], names[current_state]);

/* CORRECT — guard the variable */
#ifdef SIL_DIAG
uint8 prev = current_state;
#endif
current_state = next_state;
#ifdef SIL_DIAG
VSM_DIAG("%s -> %s", names[prev], names[current_state]);
#endif
```

**Lesson:** When adding diagnostic logging with `#ifdef`-guarded macros, always guard any variables that are ONLY used by those macros. The no-op `((void)0)` expansion doesn't reference the variable.

---

## 6. AUTOSAR Com Confirmation-Read Doesn't Protect Against Stale Data

The CVC VehicleState module uses a 3-cycle confirmation-read pattern before acting on faults:
1. RTE value non-zero for 3 consecutive cycles
2. Fresh read from Com shadow buffer confirms non-zero
3. E2E status check passes

This was designed to filter noise and transient faults. But it **does not protect against stale data** because:
- The Com fresh read returns the same stale value (shadow buffer never updated)
- E2E status reports OK (last received frame had valid E2E, and no new frame arrived to invalidate it)
- 3-cycle debounce passes because the stale value is consistently non-zero

**Lesson:** Confirmation-read patterns protect against transient noise but NOT against persistent stale data. For stale data protection, you need either: (a) cyclic TX so fresh data always arrives, or (b) a Com-level timeout that invalidates shadow buffers when no frame arrives within the expected period.

---

## 7. Debugging Timeline: What Worked, What Didn't

| Step | Time | Result |
|------|------|--------|
| Static analysis of all SAFE_STOP triggers | ~2h | Eliminated obvious causes but couldn't find root cause |
| Add SIL_DIAG logging to VehicleState + Heartbeat | 30min | First real data — but stdout buffering hid it |
| Fix stderr for Docker | 10min | Got output — clean boot, but no fault onset visible |
| Add fault onset + confirm-read detail logging | 20min | **Breakthrough** — stale mc=1 bf=3 at cycle 2 |
| Trace back through Com → shadow buffer → FzcCom TX | 30min | Root cause confirmed — event-driven TX |
| TDD tests + fix + CI + deploy + verify | 30min | Fix verified on VPS |

**Total: ~4 hours** from "reset still triggers SAFE_STOP" to verified fix.

**Lesson:** For data-flow bugs in multi-ECU systems, invest in runtime observability (diagnostic logging) early. Static analysis of individual source files can't reveal cross-ECU data staleness.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Event-driven CAN TX | Unsafe for safety-critical status — receiver retains stale values when sender stops |
| Cyclic TX | Always use for signals that drive safety transitions — bandwidth cost is trivial |
| Com shadow buffers | Retain last value indefinitely — no automatic invalidation on timeout |
| Confirmation-read | Protects against transient noise, NOT against persistent stale data |
| Docker diagnostic logging | Use `stderr` (unbuffered), never `stdout` (fully buffered without TTY) |
| SIL_DIAG guard | Variables used only by `#ifdef`-guarded macros must also be guarded |
| Debug approach | Per-cycle state dumps > static analysis for cross-ECU data flow bugs |
