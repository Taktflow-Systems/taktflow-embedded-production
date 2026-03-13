# Lessons Learned — RZC Heartbeat 5x Over-Transmit

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-28 to 2026-03-01
**Scope:** RZC heartbeat CAN frame transmitted at 5x the intended rate (every ~10ms instead of 50ms)
**Result:** Root cause identified and fixed, verified on VPS SIL demo (CVC=92, FZC=93, RZC=93 frames/5s — all equal)

---

## 1. Root Cause: Signal ID vs PDU ID Confusion in Com_SendSignal

The AUTOSAR `Com_SendSignal(SignalId, data)` API takes a **signal index** as its first parameter, not a PDU ID. The signal-to-PDU mapping is defined in `Com_Cfg_Rzc.c`:

| Signal Index | Signal Name | Mapped PDU |
|-------------|-------------|------------|
| 0 | hb_alive_counter | PDU 0 (heartbeat, CAN 0x012) |
| 1 | hb_ecu_id | PDU 0 (heartbeat) |
| 2 | hb_fault_mask | PDU 0 (heartbeat) |
| 3-7 | motor_status signals | PDU 1 (motor_status, CAN 0x300) |
| 8-9 | motor_current signals | PDU 2 (motor_current, CAN 0x301) |
| 10-12 | motor_temp signals | PDU 3 (motor_temp, CAN 0x302) |
| 13-14 | battery_status signals | PDU 4 (battery_status, CAN 0x303) |

**The bug:** Three RZC SWCs (`Swc_CurrentMonitor`, `Swc_TempMonitor`, `Swc_Battery`) called `Com_SendSignal()` with PDU ID constants as the first argument:

```c
/* Swc_CurrentMonitor.c — WRONG */
(void)Com_SendSignal(RZC_COM_TX_MOTOR_CURRENT, tx_data);  /* RZC_COM_TX_MOTOR_CURRENT = 2u */
```

Signal index 2 = `hb_fault_mask`, which belongs to PDU 0 (heartbeat). This set `com_tx_pending[0] = TRUE` — marking the heartbeat PDU for transmission.

Since `Swc_CurrentMonitor` runs at 1ms and `Com_MainFunction_Tx()` runs at 10ms, every 10ms cycle found the heartbeat PDU pending and transmitted it. The heartbeat's intended 50ms period was overridden — resulting in 5x over-transmit.

**Lesson:** `Com_SendSignal` takes a **signal ID**, not a PDU ID. The signal config table maps signals to PDUs internally. Using the wrong ID type silently corrupts unrelated PDUs. Always verify signal IDs against `Com_Cfg_*.c` signal tables.

---

## 2. The Fix: Remove Direct Com_SendSignal from SWCs

The architectural fix was to **remove** `Com_SendSignal` calls from the three SWCs entirely. In the AUTOSAR pattern used by this project, SWCs write sensor data to RTE signals, and a dedicated communication SWC (`Swc_RzcCom`) handles CAN TX by reading RTE signals and calling `Com_SendSignal` at the correct rate.

**Files changed:**
- `Swc_CurrentMonitor.c` — removed `#include "Com.h"`, `CM_SendCanFrame()` function, all CAN TX logic
- `Swc_TempMonitor.c` — removed `#include "Com.h"`, CAN broadcast section, alive counter
- `Swc_Battery.c` — removed `#include "Com.h"`, CAN broadcast section, alive counter

Each SWC now ends with: `/* CAN TX handled by Swc_RzcCom (reads RTE signals, sends via Com) */`

**Lesson:** In AUTOSAR, SWCs should not call `Com_SendSignal` directly. The communication path is: SWC → RTE → Com SWC → Com → CanIf → CAN. Mixing layers (SWC calling Com directly) bypasses the intended scheduling and creates timing coupling.

---

## 3. Secondary Bug: Microsecond vs Millisecond Timing in main.c

`Main_Hw_GetTick()` returns **microseconds** (via `clock_gettime(CLOCK_MONOTONIC)` on POSIX), but the main loop thresholds were written as if it returned milliseconds:

```c
/* WRONG — treats microseconds as milliseconds */
if ((tick_ms - last_1ms) >= 1u)   /* fires every 1us, not 1ms */

/* CORRECT — 1ms = 1000us */
if ((tick_us - last_1ms_us) >= 1000u)
```

This affected both RZC and FZC `main.c`. CVC already had correct thresholds but inconsistent variable names.

**Fix:** Changed all three ECU main files:
- Thresholds: `1u → 1000u`, `10u → 10000u`, `100u → 100000u`
- Variable names: `tick_ms → tick_us`, `last_1ms → last_1ms_us`, etc.

**Lesson:** When porting from bare-metal (where SysTick period IS the tick unit) to POSIX (where `clock_gettime` returns nanoseconds/microseconds), the tick unit changes. Variable names must reflect the unit (`_us`, `_ms`) to prevent this class of bug. Name the unit, not just the concept.

---

## 4. TDD Caught the Fix Direction

Per project rules (TDD enforced), tests were updated **first** to define the desired behavior:

```c
/* BEFORE (wrong — expected SWC to call Com_SendSignal) */
TEST_ASSERT_EQUAL(1, mock_com_send_count);

/* AFTER (correct — SWC should NOT call Com_SendSignal) */
TEST_ASSERT_EQUAL(0, mock_com_send_count);
/* Verify data goes to RTE instead */
TEST_ASSERT_EQUAL(expected_value, mock_rte_values[RZC_SIG_MOTOR_CURRENT_MA]);
```

Tests updated in:
- `test_Swc_CurrentMonitor_asila.c`
- `test_Swc_TempMonitor_asila.c`
- `test_Swc_Battery_qm.c`

Only after tests expressed the correct behavior were the source files modified.

**Lesson:** TDD prevents "fix the code to match the test" anti-pattern. By writing the test expectation first (`mock_com_send_count == 0`), the fix direction is locked in before touching production code.

---

## 5. Cascading Type Mismatch: Dio_FlipChannel

Changing `Dio_FlipChannel` from `void` to `uint8` return type (in a prior IoHwAb update) exposed 5 test files with stale mock signatures:

```c
/* Stale mock — void return */
void Dio_FlipChannel(uint8 ChannelId) { (void)ChannelId; }

/* Correct mock — uint8 return */
uint8 Dio_FlipChannel(uint8 ChannelId) { (void)ChannelId; return STD_HIGH; }
```

Files affected: `test_WdgM_asild.c`, `test_int_heartbeat_loss_asild.c`, `test_int_safe_state_asild.c`, `test_int_overcurrent_chain_asild.c`, `test_int_wdgm_supervision_asild.c`.

Additionally, `WdgM.c` line 113 needed a `(void)` cast to satisfy MISRA Rule 17.7 (return value of non-void function shall be used).

**Lesson:** When changing a shared API's return type, grep for ALL mock implementations across unit and integration test files. Mock signatures must match exactly — the compiler catches type mismatches, but only if all files are compiled together.

---

## 6. Verification: candump on VPS

The fix was verified on the live SIL demo (VPS at `sil.taktflow-systems.com`) using `candump`:

```bash
# Before fix (RZC 5x over-transmit)
docker exec sil-cvc candump -t A vcan0 | grep -c "012"  # ~92 frames/5s
docker exec sil-fzc candump -t A vcan0 | grep -c "011"  # ~93 frames/5s
docker exec sil-rzc candump -t A vcan0 | grep -c "012"  # ~460 frames/5s  ← 5x!

# After fix (all equal)
docker exec sil-cvc candump -t A vcan0 | grep -c "010"  # 92 frames/5s
docker exec sil-fzc candump -t A vcan0 | grep -c "011"  # 93 frames/5s
docker exec sil-rzc candump -t A vcan0 | grep -c "012"  # 93 frames/5s  ← fixed
```

Expected: ~100 frames/5s at 50ms period. Measured ~92-93 frames/5s accounts for startup delay and timing jitter in Docker — within acceptable tolerance.

**Lesson:** Always verify CAN timing fixes with `candump` frame counting on the actual SIL environment, not just unit tests. Unit tests verify logic; `candump` verifies real-time behavior.

---

## 7. Debugging Approach That Worked

1. **Symptom observation**: `candump` showed RZC heartbeat (0x012) at ~460 frames/5s vs CVC/FZC at ~93
2. **Hypothesis**: Something besides `Swc_Heartbeat` is triggering heartbeat PDU transmission
3. **Signal table analysis**: Mapped all `Com_SendSignal` calls to their actual signal→PDU mappings
4. **Found the collision**: Three SWCs using PDU IDs where signal IDs were expected
5. **Confirmed**: Signal 2 (`hb_fault_mask`) maps to heartbeat PDU → every SWC call marks heartbeat for TX

**Lesson:** For AUTOSAR Com layer bugs, always start by mapping signal IDs to PDU IDs in the config table. The signal config is the Rosetta Stone — without it, `Com_SendSignal(2u, ...)` could mean anything.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Com_SendSignal API | First param is **signal ID**, not PDU ID — verify against Com_Cfg signal table |
| AUTOSAR layering | SWCs write RTE signals; a Com SWC handles CAN TX — don't bypass layers |
| Timing units | Name variables with units (`_us`, `_ms`) — prevents microsecond/millisecond confusion |
| TDD discipline | Update test expectations FIRST, then fix code — locks in the fix direction |
| Mock maintenance | When changing a shared API signature, grep and update ALL test mocks |
| MISRA 17.7 | `(void)` cast when intentionally discarding a return value |
| SIL verification | `candump` frame counting on the live Docker environment is the ground truth |
| Root cause analysis | Map signal IDs → PDU IDs in Com_Cfg to find unintended PDU activation |
