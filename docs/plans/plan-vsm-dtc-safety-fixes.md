# Plan: VSM + DTC Safety Fixes (SG-001, SG-006, SAFE_STOP Flapping)

**Date**: 2026-03-04
**Status**: DONE — All phases coded, compilation verified, BSW + integration tests pass. Deploy to Netcup + SIL E2E remaining.
**Safety**: SG-001 (ASIL D), SG-006 (ASIL A), VSM recovery (ASIL D)

## Context

Three related firmware bugs found during SIL E2E testing:

1. **SG-006 Battery Drain** — DTC 0xE401 broadcasts correctly but VSM stays in RUN (should transition to DEGRADED/LIMP)
2. **SG-001 Overcurrent** — Vehicle enters SAFE_STOP correctly but DTC 0xE301 is never broadcast on CAN
3. **SAFE_STOP Flapping** — After any fault, VSM oscillates RUN↔SAFE_STOP every ~1 second

All three are system integration gaps: components work individually, cross-cutting wiring is missing.

## Root Causes

### Bug 1: Battery — VSM has no battery input
- `Swc_VehicleState.c:433-441` reads 8 RTE signals — battery is absent
- No `CVC_EVT_BATTERY_*` events defined (Cvc_Cfg.h:251-265)
- Transition table (78-181) has no battery entries
- RZC correctly detects and broadcasts via Dem, but CVC never consumes it

### Bug 2: Overcurrent — Dem_MainFunction never called on RZC/FZC
- CVC calls `Dem_MainFunction()` in 100ms task (main.c:465)
- RZC and FZC do NOT — RZC main.c 100ms task only has `WdgM_MainFunction()`
- RZC doesn't call `Dem_SetEcuId()` — broadcasts with ECU ID 0x00 instead of 0x03
- DTC code table in Dem.c is CVC-centric — RZC event 0 maps to `0xC20100` not `0xE301`

### Bug 3: SAFE_STOP Flapping — no fault latching + stale init counter
Two problems combine:
1. **No fault latching** (lines 634-658): Recovery checks instantaneous signals, not latched faults. When SAFE_STOP cuts the motor, current drops, signal clears, recovery starts
2. **init_hold_counter not reset** (line 648): Recovery sets state=INIT but doesn't reset `init_hold_counter`. Since it's already >= `CVC_INIT_HOLD_CYCLES` from boot, INIT→RUN fires immediately (1 cycle). Full flap cycle: ~500ms recovery + ~10ms INIT→RUN + ~30ms fault redetection ≈ 600ms

---

## Implementation Phases

### Phase 1: SAFE_STOP Fault Latching (Safety-Critical)

**Priority**: Highest — affects ALL fault scenarios, ASIL D

**Files to modify:**

| File | Changes |
|------|---------|
| `firmware/cvc/include/Cvc_Cfg.h` | Add latch constants, increase recovery cycles |
| `firmware/cvc/src/Swc_VehicleState.c` | Add latch arrays, modify ConfirmFault + recovery logic, reset init counter |

**Cvc_Cfg.h changes:**

After line 210 (`CVC_SAFE_STOP_RECOVERY_CYCLES`):
```c
/* Increase recovery hold time: 200 × 10ms = 2 seconds */
#define CVC_SAFE_STOP_RECOVERY_CYCLES   200u

/* Fault unlatch debounce: 300 × 10ms = 3 seconds of sustained clear */
#define CVC_FAULT_UNLATCH_CYCLES        300u

/* Latch indices — one per fault that can trigger SAFE_STOP */
#define CVC_LATCH_IDX_ESTOP             0u
#define CVC_LATCH_IDX_SC_KILL           1u
#define CVC_LATCH_IDX_MOTOR_CUTOFF      2u
#define CVC_LATCH_IDX_BRAKE             3u
#define CVC_LATCH_IDX_STEERING          4u
#define CVC_LATCH_IDX_PEDAL_DUAL        5u
#define CVC_LATCH_IDX_CAN_DUAL          6u
#define CVC_LATCH_COUNT                 7u
```

**Swc_VehicleState.c changes:**

Add module-static latch arrays after line 210:
```c
static uint8  fault_latched[CVC_LATCH_COUNT];
static uint16 fault_unlatch_count[CVC_LATCH_COUNT];
```

Initialize in `Swc_VehicleState_Init()` (after line 257):
```c
for (i = 0u; i < CVC_LATCH_COUNT; i++)
{
    fault_latched[i]       = FALSE;
    fault_unlatch_count[i] = 0u;
}
```

Add latch-set in `Swc_VehicleState_OnEvent()` when entering SAFE_STOP (after line 319, when `next_state == CVC_STATE_SAFE_STOP`):
```c
if (next_state == CVC_STATE_SAFE_STOP)
{
    /* Latch the fault that caused SAFE_STOP */
    uint8 latch_idx = 0xFFu;
    switch (event)
    {
        case CVC_EVT_ESTOP:              latch_idx = CVC_LATCH_IDX_ESTOP;        break;
        case CVC_EVT_SC_KILL:            latch_idx = CVC_LATCH_IDX_SC_KILL;      break;
        case CVC_EVT_MOTOR_CUTOFF:       latch_idx = CVC_LATCH_IDX_MOTOR_CUTOFF; break;
        case CVC_EVT_BRAKE_FAULT:        latch_idx = CVC_LATCH_IDX_BRAKE;        break;
        case CVC_EVT_STEERING_FAULT:     latch_idx = CVC_LATCH_IDX_STEERING;     break;
        case CVC_EVT_PEDAL_FAULT_DUAL:   latch_idx = CVC_LATCH_IDX_PEDAL_DUAL;  break;
        case CVC_EVT_CAN_TIMEOUT_DUAL:   latch_idx = CVC_LATCH_IDX_CAN_DUAL;    break;
        default: break;
    }
    if (latch_idx != 0xFFu)
    {
        fault_latched[latch_idx]       = TRUE;
        fault_unlatch_count[latch_idx] = 0u;
    }
}
```

Replace SAFE_STOP recovery logic (lines 633-658) with latch-aware version:
```c
/* ---- Step 5: SAFE_STOP recovery with fault latching ---- */
if (current_state == CVC_STATE_SAFE_STOP)
{
    /* Map latch indices to their raw signal values */
    const uint32 raw_signals[CVC_LATCH_COUNT] = {
        estop_active,   /* ESTOP       */
        sc_relay_kill,  /* SC_KILL     */
        motor_cutoff,   /* MOTOR_CUTOFF */
        brake_fault,    /* BRAKE       */
        steering_fault, /* STEERING    */
        pedal_fault,    /* PEDAL_DUAL  */
        (fzc_comm == CVC_COMM_TIMEOUT && rzc_comm == CVC_COMM_TIMEOUT) ? 1u : 0u  /* CAN_DUAL */
    };

    uint8 all_latches_clear = TRUE;
    uint8 li;

    for (li = 0u; li < CVC_LATCH_COUNT; li++)
    {
        if (fault_latched[li] == TRUE)
        {
            if (raw_signals[li] == 0u)
            {
                fault_unlatch_count[li]++;
                if (fault_unlatch_count[li] >= CVC_FAULT_UNLATCH_CYCLES)
                {
                    fault_latched[li]       = FALSE;
                    fault_unlatch_count[li] = 0u;
                    VSM_DIAG("UNLATCH idx=%u after %u cycles", (unsigned)li, (unsigned)CVC_FAULT_UNLATCH_CYCLES);
                }
                else
                {
                    all_latches_clear = FALSE;
                }
            }
            else
            {
                fault_unlatch_count[li] = 0u;  /* Reset: fault still active */
                all_latches_clear = FALSE;
            }
        }
    }

    /* Standard recovery only after ALL latches cleared */
    if (all_latches_clear == TRUE)
    {
        /* Check instantaneous signals (defense in depth) */
        if ((estop_active == 0u) &&
            (motor_cutoff == 0u) &&
            (brake_fault == 0u) &&
            (steering_fault == 0u) &&
            (pedal_fault == 0u) &&
            (sc_relay_kill == 0u) &&
            (fzc_comm == CVC_COMM_OK) &&
            (rzc_comm == CVC_COMM_OK))
        {
            safe_stop_clear_count++;
            if (safe_stop_clear_count >= CVC_SAFE_STOP_RECOVERY_CYCLES)
            {
                safe_stop_clear_count  = 0u;
                init_hold_counter      = 0u;  /* BUG FIX: force full INIT hold */
                current_state          = CVC_STATE_INIT;
                self_test_pass_pending = TRUE;
                (void)BswM_RequestMode(CVC_ECU_ID_CVC, BSWM_STARTUP);
                VSM_DIAG("SAFE_STOP -> INIT (recovery, latches clear)");
            }
        }
        else
        {
            safe_stop_clear_count = 0u;
        }
    }
    else
    {
        safe_stop_clear_count = 0u;
    }
}
```

**Total recovery time after fix**: 3s unlatch + 2s recovery hold + 10s INIT hold = ~15s minimum (vs ~600ms flapping before)

---

### Phase 2: Dem_MainFunction on RZC/FZC + DTC Code Remapping

**Priority**: High — enables DTC broadcast for all zone controller faults

**Files to modify:**

| File | Changes |
|------|---------|
| `firmware/shared/bsw/services/Dem.c` | Guard PduR_Transmit when broadcast not configured |
| `firmware/shared/bsw/services/Dem.h` | Add `Dem_SetBroadcastPduId()` declaration |
| `firmware/rzc/src/main.c` | Add Dem_SetEcuId + Dem_SetDtcCode + Dem_MainFunction + CanIf TX entry |
| `firmware/fzc/src/main.c` | Same as RZC with FZC-specific IDs |
| `firmware/cvc/src/main.c` | Add Dem_SetBroadcastPduId call + CanIf TX entry for 0x500 |
| `firmware/rzc/include/Rzc_Cfg.h` | Add RZC_COM_TX_DTC_BROADCAST if needed |
| `firmware/fzc/include/Fzc_Cfg.h` | Add FZC_COM_TX_DTC_BROADCAST if needed |

**Dem.h** — add declaration:
```c
void Dem_SetBroadcastPduId(PduIdType TxPduId);
```

**Dem.c** — add static variable + API + guard:
```c
static PduIdType dem_broadcast_pdu_id = 0xFFu;  /* Unconfigured */

void Dem_SetBroadcastPduId(PduIdType TxPduId)
{
    dem_broadcast_pdu_id = TxPduId;
}
```

In `Dem_MainFunction()`, replace `PduR_Transmit(0x500u, ...)` with:
```c
if (dem_broadcast_pdu_id != 0xFFu)
{
    (void)PduR_Transmit(dem_broadcast_pdu_id, &pdu_info);
}
```

**RZC main.c** — after `Dem_Init(NULL_PTR)`:
```c
Dem_SetEcuId(0x03u);                              /* RZC ECU ID */
Dem_SetBroadcastPduId(RZC_COM_TX_DTC_BROADCAST);  /* CanIf TX index for 0x500 */

/* Remap RZC DTC codes from CVC-centric defaults */
Dem_SetDtcCode(RZC_DTC_OVERCURRENT,    0x00E301u);
Dem_SetDtcCode(RZC_DTC_OVERTEMP,       0x00E302u);
Dem_SetDtcCode(RZC_DTC_BATTERY,        0x00E401u);
/* ... other RZC DTCs as needed */
```

In RZC 100ms task: add `Dem_MainFunction();`

Add CAN 0x500 to RZC CanIf TX config array.

**FZC main.c** — same pattern with `Dem_SetEcuId(0x02u)` and FZC-specific DTC codes.

**CVC main.c** — add `Dem_SetBroadcastPduId()` call after existing `Dem_SetEcuId(0x10u)`.

---

### Phase 3: Battery VSM Wiring (SG-006)

**Priority**: Medium — new signal path, most files touched

**Files to modify (6-file wiring pattern):**

| File | Changes |
|------|---------|
| `firmware/cvc/include/Cvc_Cfg.h` | Add CVC_SIG_BATTERY_STATUS, CVC_COM_RX_BATTERY_STATUS, CVC_EVT_BATTERY_WARN/CRIT |
| `firmware/cvc/cfg/Com_Cfg_Cvc.c` | Add RX signal buffer + config for 0x303 byte 3 |
| `firmware/cvc/cfg/Rte_Cfg_Cvc.c` | Add RTE signal 32 (battery status, default=2=NORMAL) |
| `firmware/cvc/src/main.c` | Add CAN 0x303 to CanIf RX + PduR routing |
| `firmware/cvc/src/Swc_CvcCom.c` | Add 0x303 RX handler, bridge battery→RTE |
| `firmware/cvc/src/Swc_VehicleState.c` | Read battery, add events, expand transition table to 16 columns |

**Cvc_Cfg.h additions:**
```c
#define CVC_SIG_BATTERY_STATUS    32u
#define CVC_SIG_COUNT             33u  /* was 32 */

#define CVC_COM_RX_BATTERY_STATUS  7u  /* CAN 0x303 */

#define CVC_EVT_BATTERY_WARN      14u
#define CVC_EVT_BATTERY_CRIT      15u
#define CVC_EVT_COUNT             16u  /* was 14 */
```

**Transition table additions** (2 new columns per row):

| State | EVT_BATTERY_WARN | EVT_BATTERY_CRIT |
|-------|------------------|-------------------|
| INIT | INVALID | INVALID |
| RUN | DEGRADED | LIMP |
| DEGRADED | INVALID (already degraded) | LIMP |
| LIMP | INVALID | SAFE_STOP |
| SAFE_STOP | INVALID | INVALID |
| SHUTDOWN | INVALID | INVALID |

**VSM MainFunction additions:**
- Read `CVC_SIG_BATTERY_STATUS` after existing signal reads
- Add battery confirmation in ConfirmFault section (UV_warn=1 → EVT_BATTERY_WARN, critical_UV=0 → EVT_BATTERY_CRIT)
- Add battery normal check to DEGRADED→RUN fault-cleared condition (line 573-577)
- Add battery latch index `CVC_LATCH_IDX_BATTERY_CRIT 7u` + bump `CVC_LATCH_COUNT` to 8
- Add battery_status to recovery guard `raw_signals[]` array

---

## Verification

### Unit Tests (local GCC, run before deploy)

**Phase 1 tests:**
- `test_safe_stop_latch_persists`: Trigger SAFE_STOP, clear fault signal, verify state stays SAFE_STOP after 50 cycles (old window)
- `test_safe_stop_unlatch_after_timeout`: Clear fault for 300+ cycles, then 200 recovery cycles → verify INIT
- `test_safe_stop_unlatch_resets_on_reflicker`: Clear 200 cycles, re-assert 1 cycle, clear again → need fresh 300 cycles
- `test_init_hold_counter_reset`: After recovery, verify INIT holds for full CVC_INIT_HOLD_CYCLES

**Phase 2 tests:**
- `test_dem_broadcast_with_pdu_id`: Confirm DTC, call Dem_MainFunction(), verify PduR_Transmit called
- `test_dem_no_broadcast_without_config`: Don't set PDU ID, verify no transmit
- `test_dem_ecu_id_in_frame`: Verify byte 4 = configured ECU ID

**Phase 3 tests:**
- `test_battery_warn_to_degraded`: Battery status=1 for 3 cycles → DEGRADED
- `test_battery_crit_to_limp`: From RUN, battery status=0 for 3 cycles → LIMP
- `test_battery_crit_latches_in_safe_stop`: Verify battery latch set and unlatch requires 3s

### SIL E2E (deploy to Netcup after unit tests pass)

- SIL-006: Battery UV → DTC 0xE401 + DEGRADED + SAFE_STOP
- SIL-007: Overcurrent → DTC 0xE301 + SAFE_STOP
- All scenarios: Verify NO flapping after SAFE_STOP
- Reset button: Verify clean recovery without oscillation

---

## MISRA Impact

- New static arrays (`fault_latched`, `fault_unlatch_count`) — ensure `uint8`/`uint16` types, no VLA
- Switch statement in OnEvent latch-set — needs `default: break;` (already included)
- All new constants are `#define` with `u` suffix — MISRA compliant
- No new `malloc`, no new pointer arithmetic
