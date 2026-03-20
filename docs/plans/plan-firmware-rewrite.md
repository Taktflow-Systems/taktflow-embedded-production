# Plan: Firmware Rewrite — Clean BSW + SWC Architecture

**Status**: PLANNING
**Date**: 2026-03-20
**Goal**: Scrap accumulated hacks, rewrite firmware with proper top-down architecture.

---

## Why Rewrite

Current firmware has these structural problems (found during HIL bringup):

| Problem | Root Cause | Impact |
|---------|-----------|--------|
| SWCs bypass Com with direct PduR_Transmit | Hand-written E2E in SWCs | Com cycle time throttle doesn't apply to 60% of TX frames |
| E2E DataIDs hardcoded in SWCs | SWCs written before DBC had E2E attributes | Wrong DataIDs cause E2E flicker on monitor |
| IoHwAb_Init(NULL) → all sensor reads fail | No bench-mode IoHwAb config | All signal values = 0 on hardware bench |
| Shared E2E alive counter across PDUs | Single `FzcCom_TxAlive` for all TX | Counter jumps unpredictably, E2E receivers reject frames |
| Bus-off recovery as inline hack in timer | Quick fix for USB hub power issue | Extra Com_MainFunction_Tx calls, 40% rate overshoot |
| Debug counters in production BSW modules | Instrumentation left in code | Code bloat, maintenance burden |
| ThreadX timer fires 141/s instead of 100/s | Unknown — possibly SysTick drift | All timing assumptions wrong |

---

## Architecture Principles

1. **DBC is the single source of truth** — every CAN ID, signal, E2E DataID, cycle time comes from DBC. No hardcoded values in SWC code.

2. **All TX goes through Com** — no direct PduR_Transmit from SWCs. E2E protection happens in Com layer (AUTOSAR standard), not in SWCs.

3. **IoHwAb has bench mode** — when no sensors are connected, IoHwAb returns configurable test values. No NULL config workaround.

4. **Per-PDU E2E state** — each PDU has its own alive counter. No shared counters.

5. **Bus-off recovery in Can MCAL** — not in application timer callbacks. Can_MainFunction_BusOff handles recovery.

6. **SWC code is thin** — reads from RTE, computes, writes to RTE. No BSW API calls except Rte_Read/Rte_Write.

7. **Codegen generates everything** — Com_Cfg, E2E_Cfg, Rte_Cfg, CanIf_Cfg all from DBC+ARXML. SWC skeletons generated with correct port names.

---

## Phase 1: BSW Stack Cleanup (Com + E2E)

### 1.1 Move E2E into Com layer

**Current**: SWC builds raw PDU → E2E_Protect → PduR_Transmit
**Target**: SWC → Com_SendSignal → Com packs PDU → Com applies E2E → PduR_Transmit

Changes:
- `Com_MainFunction_Tx`: after packing signals, apply E2E_Protect per PDU before PduR_Transmit
- E2E config per PDU in `Com_TxPduConfigType`: add `e2e_protected`, `e2e_data_id`, `e2e_counter_bit`, `e2e_crc_bit`
- Codegen: populate E2E fields from DBC attributes
- Remove all `PduR_Transmit` calls from SWC code

### 1.2 Per-PDU E2E alive counter

**Current**: Shared `FzcCom_TxAlive` across all PDUs
**Target**: `com_e2e_alive[COM_MAX_PDUS]` array, one counter per PDU

Changes:
- Add `static uint8 com_e2e_alive[COM_MAX_PDUS]` in Com.c
- E2E_Protect uses `com_e2e_alive[pdu_id]` and increments per send
- Remove `FzcCom_TxAlive`, `RzcCom_TxAlive`

### 1.3 Com cycle time enforcement (DONE)

Already implemented in this session. Verify it works after Phase 1.1 (all TX through Com).

---

## Phase 2: SWC Simplification

### 2.1 Remove all direct PduR/CanIf/E2E calls from SWCs

Files to clean:
- `Swc_FzcCom.c`: remove `PduR_Transmit` for brake_fault, motor_cutoff, lidar
- `Swc_RzcCom.c`: remove `PduR_Transmit` for motor_status, motor_current, motor_temp, battery_status
- `Swc_Heartbeat.c`: remove `PduR_Transmit`, `E2E_Protect` — heartbeat goes through Com like everything else
- `Swc_CvcCom.c`: remove any direct PduR calls

After cleanup, SWCs only call:
- `Rte_Read()` to get inputs
- Compute
- `Rte_Write()` to set outputs
- `Com_SendSignal()` for TX triggers (or Com does it automatically via RTE binding)

### 2.2 Automatic Com→RTE binding

**Current**: SWC calls `Com_SendSignal(signal_id, &value)` manually
**Target**: Com RX automatically writes to RTE (via `RteSignalId` in signal config). Com TX reads from RTE and packs automatically.

The `COM_RTE_SIGNAL_NONE` sentinel in `Com_SignalConfigType.RteSignalId` is already there. Set it to actual RTE signal IDs in the codegen.

Then `Com_MainFunction_Tx` automatically reads fresh values from RTE and packs them — no SWC intervention needed for cyclic TX.

### 2.3 Thin SWC pattern

After Phase 2.1 and 2.2, each SWC becomes:

```c
void Swc_Steering_MainFunction(void)
{
    /* Read sensor */
    uint16 angle = 0;
    IoHwAb_ReadSteeringAngle(&angle);

    /* Compute (control law, fault detection) */
    /* ... */

    /* Write to RTE — Com picks it up automatically */
    Rte_Write(FZC_SIG_STEER_ANGLE, angle);
    Rte_Write(FZC_SIG_STEER_FAULT, fault);
}
```

No Com_SendSignal, no PduR_Transmit, no E2E_Protect in SWC code.

---

## Phase 3: IoHwAb Bench Mode

### 3.1 IoHwAb config with test values

```c
typedef struct {
    /* ... existing fields ... */
    boolean     BenchMode;          /**< TRUE = use test values, not HW */
    uint16      TestSteerAngle;     /**< Bench: simulated steer angle */
    uint16      TestBrakePos;       /**< Bench: simulated brake position */
    uint16      TestBatteryMV;      /**< Bench: simulated battery voltage */
    sint16      TestMotorTempDC;    /**< Bench: simulated motor temp */
} IoHwAb_ConfigType;
```

When `BenchMode = TRUE`, all reads return the test values. When `FALSE`, reads from real hardware.

### 3.2 Bench config in experiment Makefiles

Each ThreadX experiment provides a static config:

```c
static const IoHwAb_ConfigType bench_config = {
    .BenchMode = TRUE,
    .TestSteerAngle = 1500,   /* 15.0° */
    .TestBatteryMV = 12600,   /* 12.6V */
    .TestMotorTempDC = 350,   /* 35.0°C */
    /* ... */
};
IoHwAb_Init(&bench_config);
```

---

## Phase 4: Can MCAL Bus-Off Recovery

### 4.1 Move recovery to Can_MainFunction_BusOff

**Current**: Inline FDCAN re-init in timer callback
**Target**: `Can_MainFunction_BusOff()` called every 10ms, handles recovery internally

```c
void Can_MainFunction_BusOff(void)
{
    if (Can_Hw_IsBusOff()) {
        Can_Hw_Stop();
        Can_Hw_Init(500000);
        Can_Hw_Start();
        /* Don't call Com — just restart hardware */
    }
}
```

### 4.2 Remove bus-off hacks from experiments

Delete all inline FDCAN re-init code from `app_threadx.c` and `main.c`.

---

## Phase 5: Codegen Completeness

### 5.1 Generate E2E DataIDs for ALL E2E-protected PDUs

Currently missing: Brake_Fault, Motor_Cutoff TX DataIDs in Fzc_Cfg.h.

Fix `tools/arxmlgen/generators/cfg_header.py` to emit `#define <ECU>_E2E_<MSG>_DATA_ID` for every E2E-protected PDU (TX and RX).

### 5.2 Generate Com→RTE signal bindings

Populate `Com_SignalConfigType.RteSignalId` from ARXML port mappings instead of `COM_RTE_SIGNAL_NONE`.

### 5.3 Generate SWC skeletons (optional)

Generate `Swc_<Name>.c` with correct `Rte_Read`/`Rte_Write` calls from ARXML ports. Developer fills in the compute logic.

---

## Phase 6: Verification

### 6.1 HIL CAN rate test

Run `test/hil/test_can_rates.py` — verify every message matches DBC GenMsgCycleTime within ±30%.

### 6.2 E2E verification

Each PDU has its own alive counter incrementing monotonically. No jumps or shared counters.

### 6.3 Signal value verification

With IoHwAb bench mode: all signals show configured test values, not zero.

### 6.4 CVC reaches RUN

With proper heartbeat rates and E2E, CVC VSM transitions INIT → RUN within 5 seconds.

### 6.5 SIL regression

After rewrite, run full 16-scenario SIL suite. Must pass >= 14/16.

---

## Estimated Effort

| Phase | Scope | Effort |
|-------|-------|--------|
| 1 | BSW Com+E2E refactor | 4-6 hours |
| 2 | SWC cleanup (7 ECUs × 2-3 SWCs each) | 4-6 hours |
| 3 | IoHwAb bench mode | 1-2 hours |
| 4 | Can bus-off recovery | 1 hour |
| 5 | Codegen fixes | 2-3 hours |
| 6 | Verification | 2-3 hours |
| **Total** | | **14-21 hours** |

---

## Order of Execution

1. Phase 5 first (codegen) — generate correct configs
2. Phase 1 (Com+E2E) — route all TX through Com
3. Phase 2 (SWC cleanup) — simplify SWCs
4. Phase 3 (IoHwAb bench) — get non-zero values
5. Phase 4 (bus-off recovery) — clean Can MCAL
6. Phase 6 (verify) — HIL + SIL tests
