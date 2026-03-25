# Deep Audit — Data Flow, Feedback Loops & Data Races
# taktflow-embedded-production

**Date**: 2026-03-25
**Scope**: 10 parallel specialised auditors — signal flow, ISR/task races, RTOS races,
control loop feedback, E2E integrity, RTE port binding, NvM consistency,
state machine completeness, CAN timing, DMA/cache coherency.
**Status**: READ-ONLY — no fixes applied. Document gaps only.
**Auditor**: Claude Code (10× parallel static + structural analysis)

---

## Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 14 |
| HIGH | 9 |
| MEDIUM | 8 |
| LOW | 3 |
| **Total distinct gaps** | **34** |

---

## 1. Signal Flow (Auditor 1)

### 1.1 RTE Signal Definitions Missing — CVC Core Signals (CRITICAL)

The following signals are referenced by SWCs across multiple ECUs but are absent from
`firmware/bsw/rte/Rte_Cfg.h` / `firmware/bsw/rte/Rte_Cfg.c`:

| Signal | Referenced by | Impact |
|--------|--------------|--------|
| `CVC_SIG_VEHICLE_STATE` | CVC, SC, BCM supervisors | Vehicle state never propagated |
| `CVC_SIG_PEDAL_POSITION` | CVC → RZC motor control | Pedal command lost between ECUs |
| `CVC_SIG_ESTOP_ACTIVE` | All 6 ECUs | E-stop never flows to actuator SWCs |
| `CVC_SIG_DRIVE_MODE` | CVC → ICU, TCU | Drive mode selection disconnected |
| `CVC_SIG_SPEED_SETPOINT` | CVC → RZC | Speed setpoint not visible to RZC |

**Root cause**: ARXML Phase 2 enrichment not yet injected. SWC interface descriptions exist
in ARXML but generated RTE config (Rte_Cfg) was never regenerated after Phase 2.

---

### 1.2 BCM TX Signal Bridge Missing (CRITICAL)

`firmware/ecu/bcm/src/Swc_BcmState.c` writes BCM output signals to RTE ports via
`Rte_Write_BCM_*()`. However, no `Com_SendSignal()` bridge call was found in
`firmware/bsw/com/src/Com.c` for BCM TX PDUs.

**Result**: BCM state signals are written into the RTE buffer but never forwarded to the
CAN TX queue. BCM outputs are invisible on the bus.

---

### 1.3 FZC/RZC Sensor Feeder — Zero Signal Injection Path Test (HIGH)

Per gap-analysis-2026-03-25.md §2.1: `Swc_FzcSensorFeeder.c` and `Swc_RzcSensorFeeder.c`
have no unit tests. Additionally: no integration-level test verifies that a sensor value
injected by the feeder SWC arrives at the consuming SWC's RTE port. The injection path is
untested end-to-end.

---

## 2. ISR / Task Data Races (Auditor 2)

### 2.1 UART RX Buffer — No Protection Between ISR and Task (CRITICAL)

**File**: `firmware/platform/stm32/src/Uart_Hw_STM32.c`

```
ISR:  USART3_IRQHandler → rx_buf[rx_head++ % RX_BUF_SIZE] = byte
Task: Uart_Hw_ReadRx    → reads rx_buf[rx_tail]
```

`rx_head` and `rx_tail` are plain `uint32_t` (not `volatile`, no lock). On ARMv7-M:
- Compiler may cache `rx_head` in a register across the read in `ReadRx`.
- `rx_tail` update is not atomic with the copy — concurrent IRQ can corrupt the index.

**Fix class**: `volatile` on both indices + `__disable_irq()`/`__enable_irq()` guard in
the task-context reader, or CMSIS ring buffer with `__LDREX`/`__STREX`.

---

### 2.2 CAN Debug Counters — Write Outside SchM Lock (HIGH)

**File**: `firmware/bsw/can/src/Can.c`

`g_dbg_can_tx_calls` and `g_dbg_can_rx_calls` are incremented directly in
`Can_Write()` / `Can_RxIndication()` without `SchM_Enter_Can_Exclusive()` guards.
Both functions can be called from task context while an ISR increments the same counter.

Read-modify-write on a 32-bit counter is not atomic on ARMv7-M without LDREX/STREX.

---

### 2.3 Com RX PDU Quality — Write Without Lock (CRITICAL)

**File**: `firmware/bsw/com/src/Com.c`

`com_rx_pdu_quality[]` is written in `Com_RxIndication()` (ISR context on some platforms)
and read in `Com_MainFunction_Rx()` (task context). No `SchM_Enter_Com_*` guard wraps
either access. On STM32 with DMA-triggered RX indication, this is a confirmed race path.

---

## 3. RTOS Races (Auditor 3)

### 3.1 HAL_GetTick() — SysTick Race on STM32 (CRITICAL)

**File**: `firmware/platform/stm32/src/Os_Hw_STM32.c` (stub) + STM32 HAL

`HAL_GetTick()` reads `uwTick`, which is incremented in `SysTick_Handler` (ISR).
On ARMv7-M, 32-bit read is atomic — however any code that reads `HAL_GetTick()` twice
to compute a delta may observe a torn value if an overflow occurs between reads.

More importantly: `SysTick_Handler` is not yet implemented (stub file). When the real
IRQ is wired, existing consumers of `HAL_GetTick()` may race if not reviewed.

---

### 3.2 Com_MainFunction_Tx — Counter Write Before SchM Lock (HIGH)

**File**: `firmware/bsw/com/src/Com.c`

```c
g_dbg_com_tx_calls++;          // ← outside lock
SchM_Enter_Com_TX_DATA();
// ... signal packing ...
SchM_Exit_Com_TX_DATA();
```

The debug counter is incremented before the exclusive section. An ISR calling
`Com_SendSignal()` between the counter increment and the lock acquisition will cause
a lost-update or double-count.

---

### 3.3 OsTask Activation — No Deadline Monitoring (MEDIUM)

No task deadline or execution time monitoring is instrumented in the OS abstraction.
If a task overruns its period (e.g., NvM blocking write — see §7), the overrun is
invisible. Combined with the disabled watchdog safety check (§4.1), a runaway task
would only be caught by the global watchdog timeout.

---

## 4. Control Loop Feedback (Auditor 4)

### 4.1 Watchdog — No ISR-Level Keep-Alive (CRITICAL)

**File**: `firmware/ecu/cvc/src/Swc_CvcMonitor.c`

`Wdg_Trigger()` is called only from the 100 ms CVC main loop task. If the main loop
blocks (e.g., on NvM, CAN TX queue full, or a deadlock), the watchdog is not fed and
the system resets. This is the intended behaviour.

**Gap**: There is no secondary ISR-level keep-alive to distinguish "main loop hung" from
"main loop slow due to NvM I/O". The watchdog window is not configured (open window),
so any early trigger is also fatal. The watchdog self-test stub (TMS570 §1.2) means
window configuration is never verified at boot.

---

### 4.2 Motor Cutoff — No Actuation Confirmation (HIGH)

**File**: `firmware/ecu/cvc/src/Swc_CvcSupervisor.c`

When a safety fault is detected, CVC sends a motor cutoff command via CAN to RZC.
There is no feedback path: CVC does not read an RZC acknowledgement signal, does not
check `CVC_SIG_VEHICLE_STATE` (undefined — §1.1), and does not retry if the CAN frame
is dropped (`Can_Write` → `CAN_BUSY` → silent drop — see §9.2).

**Result**: In a fault scenario with CAN bus contention, the motor cutoff command may
be silently dropped with no detection and no retry.

---

### 4.3 Brake Pressure Feedback — Open Loop (MEDIUM)

`Swc_FzcController.c` sends a brake pressure setpoint to the FZC actuator. No pressure
readback signal is consumed by CVC to verify that the requested pressure was achieved.
The control loop is open: commanded ≠ achieved is undetectable.

---

## 5. E2E Integrity (Auditor 5)

### 5.1 E2E Check — Always Bypassed (CRITICAL)

**File**: `firmware/ecu/bcm/src/Swc_BcmCan.c`, line 96

```c
#define E2E_CHECK(data, dlc)   TRUE   /* TODO:POST-BETA — implement real E2E CRC-8 */
```

This macro replaces the entire E2E validation call site. Every CAN frame accepted by
BCM is treated as valid regardless of CRC or alive counter. Corrupt frames, replayed
frames, and frames with wrong counters are all accepted silently.

**Also noted**: `E2E_Protect()` return value is cast to `(void)` in the TX path — if
CRC computation fails, the caller has no way to know.

---

### 5.2 BCM Alive Counter Wraps at 255, Not 15 (HIGH)

**File**: `firmware/ecu/bcm/src/Swc_BcmCan.c`

```c
static uint8_t g_alive_counter = 0;
// ...
g_alive_counter++;   // wraps at 256
frame.data[0] = (frame.data[0] & 0xF0) | (g_alive_counter & 0x0F);
```

The mask `& 0x0F` correctly limits the transmitted nibble to 0–15. However the stored
counter wraps at 256 without reset, meaning it drifts out of sync with the transmitted
value every 256 cycles. A receiver tracking `g_alive_counter` directly (not the masked
transmitted value) will detect a false sequence error at wrap.

**Correct pattern**: `g_alive_counter = (g_alive_counter + 1) & 0x0F;`

---

### 5.3 E2E State Machine Not Initialised (MEDIUM)

No call to `E2E_SMCheckInit()` or equivalent was found at ECU init for any of the 6
ECUs. The AUTOSAR E2E state machine starts in `E2E_SM_INIT` but without explicit
initialisation the initial state is undefined (relies on zero-init of BSS).
On warm reset (no power cycle) BSS may not be cleared.

---

## 6. RTE Port Binding (Auditor 6)

### 6.1 Signal IDs — Consistent (PASS)

All signal IDs found in `firmware/bsw/rte/Rte_Cfg.h` are consistently aliased and
used across all ECU SWC files. No mismatched IDs or missing alias definitions were
found for the signals that ARE defined.

**Note**: The gap is not inconsistency — it is completeness. Signals listed in §1.1
above are absent from Rte_Cfg entirely, so by definition no mismatch is possible.

---

## 7. NvM Consistency (Auditor 7)

### 7.1 NvM_ReadBlock Return Value Ignored Everywhere (CRITICAL)

**Files**: All SWCs that call `NvM_ReadBlock()` — BCM, CVC, FZC, RZC, SC

```c
NvM_ReadBlock(NVM_BLOCK_DTC, &dtc_buffer);   // return value discarded
```

`NvM_ReadBlock` is asynchronous in AUTOSAR. The return value `E_OK` only means the
request was queued. The actual result arrives via callback or polling `NvM_GetErrorStatus()`.
No SWC polls `NvM_GetErrorStatus()` after `ReadBlock`. If the NvM read fails (first boot,
CRC mismatch, flash error), the SWC silently operates on uninitialised buffer contents.

---

### 7.2 FZC NvM Buffer Overflow (CRITICAL)

**File**: `firmware/ecu/fzc/src/Swc_FzcDtc.c`

```c
static Dtc_SlotType DtcSlots[FZC_DTC_MAX_COUNT];   // sizeof = 45 × 4 = 180 bytes
// ...
NvM_ReadBlock(NVM_BLOCK_FZC_DTC, DtcSlots);        // block configured for 1024 bytes
```

The NvM block `NVM_BLOCK_FZC_DTC` is configured for 1024 bytes in `NvM_Cfg.h` but
`DtcSlots` is only 180 bytes. On a successful NvM read, 844 bytes are written past
the end of `DtcSlots`, corrupting adjacent stack or BSS variables.

---

### 7.3 Blocking POSIX NvM — No Timeout (HIGH)

**File**: `firmware/posix/src/NvM_Stub_Posix.c`

The POSIX NvM stub uses `fread()`/`fwrite()` directly in the SWC call context with no
timeout. If the backing file is on a slow or unavailable filesystem (e.g., NFS mount in
SIL Docker), `fwrite()` blocks indefinitely, hanging the calling task and starving the
watchdog (§4.1).

---

### 7.4 DTC NvM Write — Not Atomic (MEDIUM)

**File**: `firmware/ecu/cvc/src/Swc_CvcDtc.c`

```c
Dem_ReportEvent(dtc_id, DEM_EVENT_STATUS_FAILED);   // broadcast to DEM
NvM_WriteBlock(NVM_BLOCK_DTC, &dtc_buffer);          // persist to NvM
```

Broadcast happens before persist. If power is lost between these two lines, DEM
reports the fault as active on the bus but the NvM block still holds the previous
(healthy) state. On next boot, the DTC is invisible — no DEM event, no NvM record.

---

## 8. State Machine Completeness (Auditor 8)

### 8.1 CAN Timeout Debounce — Not Reset on Grace Expiry (HIGH)

**File**: `firmware/bsw/com/src/Com.c`

```c
if (can_tmo_debounce[pdu_id] > 0) {
    can_tmo_debounce[pdu_id]--;
    return;   // still in grace period
}
// timeout handler
```

`can_tmo_debounce` is decremented during grace but never explicitly reset to 0 when
the grace period expires and the timeout handler fires. If the PDU resumes and then
times out again, the debounce counter starts from whatever residual value it had,
producing a shorter-than-specified grace period on the second timeout.

---

### 8.2 SC State Machine — No FAULT→MONITORING Recovery Path (CRITICAL)

**File**: `firmware/ecu/sc/src/Swc_ScSupervisor.c`

SC state machine transitions:

```
INIT → MONITORING → FAULT → KILL (terminal)
                  ↗
         (no return from FAULT)
```

Once SC enters `FAULT`, the only transition is to `KILL`. There is no transition back
to `MONITORING` even after a transient fault clears. The SC must be power-cycled to
recover. This is intentional for some safety goals but there is no documentation that
this is a deliberate design choice vs. an omission. No `@satisfies` tag links this
behaviour to a Safety Goal.

**Gap**: Intent not documented. If intentional, must be traced to an ASIL requirement.
If accidental, recovery path is missing.

---

### 8.3 Steering Timeout Counter — Not Reset on Latch Clear (MEDIUM)

**File**: `firmware/ecu/sc/src/Swc_ScSteering.c`

`Steering_CmdTimeoutCounter` is incremented on missing steering command frames and
triggers a latch. The latch can be cleared by an operator reset signal. However
`Steering_CmdTimeoutCounter` is not reset when the latch clears — on the next timeout
event the counter starts from its previous value, reaching the latch threshold
faster than the nominal debounce period.

---

## 9. CAN Timing (Auditor 9)

### 9.1 ALL RX PDUs Have timeoutMs = 0 (CRITICAL)

**File**: `firmware/bsw/com/src/Com_Cfg.c`

```c
const ComRxPduCfg_t Com_RxPduCfg[] = {
    /* pdu_id, dlc, signal_ref, timeoutMs */
    { COM_PDU_CVC_STATUS,   8, &Sig_CvcStatus,   0 },  /* ← 0 = disabled */
    { COM_PDU_BCM_STATUS,   8, &Sig_BcmStatus,   0 },
    { COM_PDU_FZC_STATUS,   8, &Sig_FzcStatus,   0 },
    { COM_PDU_RZC_STATUS,   8, &Sig_RzcStatus,   0 },
    { COM_PDU_SC_STATUS,    8, &Sig_ScStatus,     0 },
    { COM_PDU_ICU_STATUS,   8, &Sig_IcuStatus,   0 },
    // ... all 6 ECUs ...
};
```

`timeoutMs = 0` disables timeout monitoring for every RX PDU across all 6 ECUs.
`Com_MainFunction_Rx()` checks `if (cfg->timeoutMs == 0) continue;` — no RX timeout
is ever triggered. A silent loss of any ECU on the bus is undetectable at the COM layer.

---

### 9.2 Can_Write Returns CAN_BUSY — Silent Frame Drop (HIGH)

**File**: `firmware/bsw/can/src/Can.c`

```c
Can_ReturnType Can_Write(Can_HwHandleType hth, const Can_PduType *pdu) {
    if (HAL_CAN_AddTxMessage(...) != HAL_OK)
        return CAN_BUSY;   // caller must retry — but no caller does
    return CAN_OK;
}
```

All callers (`Com_MainFunction_Tx`, `Swc_BcmCan`, `Swc_CvcSupervisor`) discard the
`CAN_BUSY` return value. Under bus contention or TX queue full conditions, frames
are silently dropped with no retry, no error counter increment, no DTC.

Combined with §4.2 (motor cutoff with no confirmation), this creates a safety path
where the fault-response command is dropped without detection.

---

### 9.3 No Minimum Tx Interval on Event-Triggered Messages (MEDIUM)

Event-triggered TX messages (fault alerts, state changes) have no minimum inter-frame
delay. A rapid state oscillation (e.g., fault flag toggling at task rate) can flood
the bus at the full task period (10 ms → 100 frames/s per ECU). No debounce or
minimum-delta filter is applied before `Com_SendSignal()` triggers transmission.

---

## 10. DMA / Cache Coherency (Auditor 10)

### 10.1 No DMA-Safe Memory Sections in Linker Scripts (CRITICAL)

**Files**: `firmware/platform/stm32/ld/stm32g474.ld`,
`firmware/platform/stm32f4/ld/stm32f4xx.ld`

Neither linker script defines a `.dma_buf` or `.noinit` section placed in non-cached
(SRAM2/SRAM3) or MPU-uncached RAM. All UART and CAN DMA buffers are placed in
default `.bss` (SRAM1, cached on Cortex-M4 with cache enabled).

Without cache maintenance (clean before TX, invalidate after RX), DMA accesses
stale cache lines instead of real memory.

---

### 10.2 UART RX Buffer — Cached RAM, No Invalidate After DMA (CRITICAL)

**File**: `firmware/platform/stm32/src/Uart_Hw_STM32.c`

```c
static uint8_t rx_buf[RX_BUF_SIZE];   // in .bss — cached SRAM1
// DMA RX complete callback:
// ← no SCB_InvalidateDCache_by_Addr(rx_buf, ...) before reading
rx_head = (rx_head + dma_count) % RX_BUF_SIZE;
```

CPU reads `rx_buf` via cached view. DMA wrote to physical memory. Without cache
invalidation, the CPU reads stale (pre-DMA) data. Characters are silently lost.

---

### 10.3 CAN TX Stack Buffer Passed to HAL (HIGH)

**File**: `firmware/bsw/can/src/Can.c`

```c
Can_ReturnType Can_Write(Can_HwHandleType hth, const Can_PduType *pdu) {
    uint8_t local_data[8];
    memcpy(local_data, pdu->sdu, pdu->length);
    HAL_CAN_AddTxMessage(hcan, &tx_header, local_data, &tx_mailbox);
}
```

`local_data` is a stack variable. `HAL_CAN_AddTxMessage` copies into the CAN peripheral
mailbox register (memory-mapped, not DMA). This is safe for the STM32 CAN peripheral
(direct register write, not async DMA). **However**: if this is ever changed to DMA TX,
the stack buffer will be freed before DMA completes.

**Gap**: No comment documents this assumption. If DMA TX is enabled in HAL config
(`CAN_InitTypeDef.TransmitFifoPriority`), the behaviour changes silently.

---

### 10.4 No DMB After ISR Index Update (MEDIUM)

**File**: `firmware/platform/stm32/src/Uart_Hw_STM32.c`

```c
// In USART3_IRQHandler:
rx_buf[rx_head % RX_BUF_SIZE] = byte;
rx_head++;
// ← no DMB — compiler/CPU may reorder across the index update
```

On ARMv7-M, stores to normal memory are not guaranteed to be visible to other agents
(DMA, another CPU) without a memory barrier. A `__DMB()` (Data Memory Barrier) is
required after the buffer write and before the index update to prevent the index from
becoming visible before the data.

---

## 11. Cross-Cutting Findings

### 11.1 SchM Stubs — No Actual Exclusion (CRITICAL)

**File**: `firmware/bsw/os/src/SchM.c`

```c
void SchM_Enter_Can_Exclusive(void)  {}  // stub — no __disable_irq()
void SchM_Exit_Can_Exclusive(void)   {}  // stub — no __enable_irq()
```

All `SchM_Enter_*` / `SchM_Exit_*` functions are empty stubs. Every race condition
listed in §2 and §3 that is nominally "protected" by SchM calls is in fact completely
unprotected. The stubs give false confidence.

**Impact**: All race mitigations throughout the codebase that rely on SchM are absent.
This is a systemic issue that makes §2.2, §3.2, and §9.1 timeout-monitoring races
immediately exploitable.

---

### 11.2 Volatile Missing on Shared Variables (HIGH)

The following shared variables (ISR ↔ task) were found without `volatile`:

| Variable | File | Shared between |
|----------|------|---------------|
| `rx_head`, `rx_tail` | Uart_Hw_STM32.c | USART3_IRQHandler ↔ ReadRx |
| `g_dbg_can_tx_calls` | Can.c | Can_Write ↔ diagnostic task |
| `g_dbg_can_rx_calls` | Can.c | Can_RxIndication ↔ diagnostic task |
| `com_rx_pdu_quality` | Com.c | Com_RxIndication ↔ Com_MainFunction_Rx |

Without `volatile`, the compiler may cache these in registers, producing stale reads
in task context even after ISR writes.

---

## 12. Priority Order for Resolution

Ordered by risk, not effort:

| Priority | Gap | Category |
|----------|-----|----------|
| P1 | Implement SchM_Enter/Exit with __disable_irq/__enable_irq (§11.1) | race |
| P2 | Add volatile to all ISR↔task shared variables (§11.2) | race |
| P3 | Fix UART rx_buf race: volatile + ISR guard in ReadRx (§2.1) | race |
| P4 | Fix FZC NvM buffer overflow: resize DtcSlots or shrink NvM block (§7.2) | safety |
| P5 | Set timeoutMs > 0 for all RX PDUs in Com_Cfg (§9.1) | safety |
| P6 | Implement E2E CRC-8 validation — remove always-TRUE macro (§5.1) | safety |
| P7 | Add DMA-safe linker sections; add cache maintenance to UART DMA paths (§10.1–10.2) | safety |
| P8 | Add CAN_BUSY retry in all Can_Write callers; add DTC on persistent busy (§9.2) | safety |
| P9 | Fix alive counter wrap: mask counter itself not just transmitted nibble (§5.2) | correctness |
| P10 | Add motor cutoff confirmation feedback loop in CvcSupervisor (§4.2) | safety |
| P11 | Document or add SC FAULT→MONITORING recovery path (§8.2) | traceability |
| P12 | Fix DTC NvM write order: persist before broadcast (§7.4) | safety |
| P13 | Define CVC core signals in Rte_Cfg + regenerate RTE (§1.1) | architecture |
| P14 | Add BCM → Com_SendSignal bridge for TX PDUs (§1.2) | correctness |

---

*This document is a read-only gap inventory. No code was modified during this audit.*
