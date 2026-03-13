# Plan: Fix E2E PDU Layout Across All ECUs

**Date**: 2026-03-05
**Scope**: 12 E2E-protected messages across CVC, FZC, RZC
**ASIL Impact**: D (highest affected)
**Status**: DONE (Phases 1-8 complete, 120 tests pass)

---

## 1. Problem Statement

All E2E-protected messages from FZC and RZC have a **PDU byte layout mismatch** between firmware and DBC/spec. The CVC heartbeat also has a layout bug. E2E protection is effectively **decorative** — it either destroys application data (CVC) or uses reversed byte positions (FZC/RZC) that no receiver can validate.

### Root Cause: Two Incompatible E2E Implementations

| Property | Shared BSW E2E (`E2E.h/E2E.c`) | Custom FZC/RZC E2E |
|----------|---------------------------------|---------------------|
| **Byte 0** | `[AliveCounter:4 \| DataId:4]` | CRC-8 |
| **Byte 1** | CRC-8 | `[xxxx \| AliveCounter:4]` (no DataId in frame) |
| **CRC input** | bytes 2..N-1 + DataId | bytes 1..N-1 + DataId in seed |
| **CRC init** | `0xFF` | FZC: `0xFF ^ dataId`, RZC: `dataId` |
| **Alive counter** | Per E2E-state (one per config) | FZC: shared across all TX, RZC: per-PDU array |
| **Used by** | CVC (TX: heartbeat, vehicle state, torque, steer, brake) | FZC (TX: brake fault, motor cutoff, lidar), RZC (TX: motor status, current, temp) |

**DBC and CAN message matrix spec**: matches shared BSW E2E layout (byte 0 = counter+dataid, byte 1 = CRC).

### What's Broken (12 of 16 E2E messages)

| CAN ID | Message | Sender | Bug | Impact |
|--------|---------|--------|-----|--------|
| 0x010 | CVC_Heartbeat | CVC | App data at bytes 0-2, E2E overwrites bytes 0-1 → destroys ECU_ID | ECU_ID shows garbage in CAN monitor |
| 0x011 | FZC_Heartbeat | FZC | No E2E at all (sends raw via PduR_Transmit) | FSR-013/014 violated — no CRC/alive on ASIL C message |
| 0x012 | RZC_Heartbeat | RZC | No E2E at all (sends raw via PduR_Transmit) | FSR-013/014 violated |
| 0x200 | Steering_Status | FZC | No E2E (TODO in code) | FSR-013 violated — ASIL D message unprotected |
| 0x201 | Brake_Status | FZC | No E2E (TODO in code) | FSR-013 violated — ASIL D message unprotected |
| 0x210 | Brake_Fault | FZC | Custom E2E — reversed bytes vs DBC | CVC E2E_Check always fails → false safe-default trigger |
| 0x211 | Motor_Cutoff_Req | FZC | Custom E2E — reversed bytes vs DBC | CVC E2E_Check always fails |
| 0x220 | Lidar_Distance | FZC | Custom E2E — reversed bytes vs DBC | CVC E2E_Check always fails |
| 0x300 | Motor_Status | RZC | Custom E2E — reversed bytes vs DBC | CVC E2E_Check always fails |
| 0x301 | Motor_Current | RZC | Custom E2E — reversed bytes vs DBC | CVC E2E_Check always fails |
| 0x302 | Motor_Temperature | RZC | Custom E2E — reversed bytes vs DBC | CVC E2E_Check always fails |
| 0x303 | Battery_Status | RZC | No E2E (correct — QM message per spec) | N/A — not a bug |

**Messages that are CORRECT** (4 of 16):

| CAN ID | Message | Sender | Status |
|--------|---------|--------|--------|
| 0x001 | EStop_Broadcast | CVC | Correct — uses shared BSW E2E |
| 0x100 | Vehicle_State | CVC | Correct — uses shared BSW E2E |
| 0x101 | Torque_Request | CVC | Correct — uses shared BSW E2E |
| 0x102 | Steer_Command | CVC | Correct — uses shared BSW E2E |
| 0x103 | Brake_Command | CVC | Correct — uses shared BSW E2E |

### Visual Symptom

The "spiky" FZC/RZC signals seen in the CAN monitor (previously attributed to hardware) are caused by the DBC decoding bytes at wrong positions. The CAN monitor reads byte 2 as ECU_ID per DBC, but firmware puts `vehicle_state` there. Hardware connections are likely fine.

---

## 2. Requirements Violated

| Req ID | ASIL | Title | Violation |
|--------|------|-------|-----------|
| **FSR-013** | **D** | CAN E2E Protection on Safety Messages | FZC/RZC heartbeats have no E2E. FZC steer/brake status have no E2E. FZC/RZC data messages use reversed byte layout. |
| **FSR-014** | **C** | ECU Heartbeat Transmission | All 3 heartbeats have wrong PDU layout vs spec. Spec says E2E bytes 0-1, app data bytes 2+. |
| **FSR-015** | **C** | Heartbeat Timeout Detection (SC) | SC E2E check on FZC/RZC heartbeats always fails (no E2E on TX side). |
| **SSR-CVC-008** | **D** | CAN E2E Protection — TX Side | CVC heartbeat puts ECU_ID at byte 1, E2E_Protect overwrites with CRC. |
| **SSR-CVC-010** | **D** | CAN E2E Protection — RX Side | CVC extracts FZC/RZC alive from wrong bit positions in Com config. |
| **SSR-CVC-011** | **C** | CVC Heartbeat Transmission | PDU layout doesn't match spec. |

---

## 3. Data ID Alignment

**Spec** (CAN message matrix) vs **firmware defines**:

| Message | Spec Data ID | Firmware Define | Current Value | Correct? | Fix |
|---------|-------------|-----------------|---------------|----------|-----|
| EStop_Broadcast | 0x01 | `CVC_E2E_ESTOP_DATA_ID` | 0x01 | Yes | — |
| CVC_Heartbeat | 0x02 | `CVC_E2E_HEARTBEAT_DATA_ID` | 0x02 | Yes | — |
| FZC_Heartbeat | 0x03 | `FZC_E2E_HEARTBEAT_DATA_ID` | 0x11 | **NO** | → 0x03 |
| RZC_Heartbeat | 0x04 | `RZC_E2E_HEARTBEAT_DATA_ID` | 0x04 | Yes | — |
| Vehicle_State | 0x05 | `CVC_E2E_VEHICLE_STATE_DATA_ID` | 0x05 | Yes | — |
| Torque_Request | 0x06 | `CVC_E2E_TORQUE_DATA_ID` | 0x06 | Yes | — |
| Steer_Command | 0x07 | `CVC_E2E_STEER_DATA_ID` | 0x07 | Yes | — |
| Brake_Command | 0x08 | `CVC_E2E_BRAKE_DATA_ID` | 0x08 | Yes | — |
| Steering_Status | 0x09 | (missing — no E2E) | — | **NO** | Add 0x09 |
| Brake_Status | 0x0A | (missing — no E2E) | — | **NO** | Add 0x0A |
| Brake_Fault | 0x0B | FZC custom E2E constant | TBD | **TBD** | → 0x0B |
| Motor_Cutoff_Req | 0x0C | FZC custom E2E constant | TBD | **TBD** | → 0x0C |
| Lidar_Distance | 0x0D | FZC custom E2E constant | TBD | **TBD** | → 0x0D |
| Motor_Status | 0x0E | RZC custom E2E constant | TBD | **TBD** | → 0x0E |
| Motor_Current | 0x0F | RZC custom E2E constant | TBD | **TBD** | → 0x0F |
| Motor_Temperature | 0x00 | RZC custom E2E constant | TBD | **TBD** | → 0x00 |

---

## 4. Strategy: Migrate All to Shared BSW E2E

**Decision**: Replace FZC/RZC custom E2E (`Swc_FzcCom_E2eProtect`, `Swc_RzcCom_E2eProtect`) with calls to the shared BSW `E2E_Protect()` / `E2E_Check()` from `E2E.h`.

**Rationale**:
- Shared BSW E2E matches the DBC and CAN message matrix spec
- CVC already uses it successfully for all its TX messages
- Single implementation = single place to maintain, test, and audit
- Eliminates the CRC computation differences (init seed, input range)
- Eliminates the byte layout reversal bug at the source

**What gets deleted**:
- `Swc_FzcCom_E2eProtect()` and `FzcCom_CalcCrc8()` in `Swc_FzcCom.c`
- `Swc_FzcCom_E2eCheck()` in `Swc_FzcCom.c`
- `Swc_RzcCom_E2eProtect()` and `RzcCom_Crc8()` in `Swc_RzcCom.c`
- `Swc_RzcCom_E2eCheck()` in `Swc_RzcCom.c`
- `FzcCom_TxAlive` shared counter (replaced by per-config E2E state)
- `RzcCom_TxAlive[]` array (replaced by per-config E2E state)

**What gets added**:
- `#include "E2E.h"` in `Swc_FzcCom.c` and `Swc_RzcCom.c`
- Per-message `E2E_ConfigType` + `E2E_StateType` (static) for each TX message
- `E2E_Protect()` calls before `PduR_Transmit()` / `Com_SendSignal()`
- `E2E_Check()` calls in RX path (replacing custom check)

---

## 5. Phased Implementation

### Phase 1: Fix CVC Heartbeat TX PDU Layout

**Status**: DONE (38/38 tests pass)

**Files**:
- `firmware/cvc/src/Swc_Heartbeat.c` — fix PDU construction
- `firmware/cvc/test/test_Swc_Heartbeat_asilc.c` — update test assertions FIRST (TDD)

**Changes**:
```c
/* BEFORE (buggy) */
pdu[0] = alive_counter;        /* E2E overwrites */
pdu[1] = CVC_ECU_ID_CVC;       /* E2E overwrites — DESTROYED! */
pdu[2] = (uint8)vehicle_state;

/* AFTER (correct) */
/* Bytes 0-1 reserved for E2E_Protect (counter+dataid, CRC) */
pdu[2] = CVC_ECU_ID_CVC;                    /* ECU_ID — byte 2 per DBC */
pdu[3] = (uint8)(vehicle_state & 0x0Fu);     /* OperatingMode — byte 3 low nibble */
```

Keep `alive_counter` variable for `Rte_Write` diagnostic but stop writing it to PDU byte 0.

**Test changes** (write FIRST):
- `test_Heartbeat_TX_data_includes_ecu_id_and_state`: check `mock_com_send_data[2]` for ECU_ID, `mock_com_send_data[3] & 0x0F` for state
- `test_Heartbeat_Alive_counter_increments/wraps`: remove PDU byte[0] assertions (E2E manages byte 0)
- `test_Heartbeat_E2E_Protect_with_real_config`: verify bytes 0-1 are E2E overhead

**Affected tests**: ~10 of 39 CVC heartbeat tests

---

### Phase 2: Add E2E to FZC Heartbeat + Fix PDU Layout

**Status**: DONE (27/27 tests pass)

**Files**:
- `firmware/fzc/src/Swc_Heartbeat.c` — add E2E_Protect, fix layout
- `firmware/fzc/include/Fzc_Cfg.h` — fix Data ID (0x11 → 0x03)
- `firmware/fzc/test/test_Swc_Heartbeat_asilc.c` — update tests FIRST (TDD)

**Changes**:
1. Add `#include "E2E.h"`
2. Add static E2E config + state:
   ```c
   static const E2E_ConfigType hb_e2e_config = {
       FZC_E2E_HEARTBEAT_DATA_ID,  /* 0x03 per spec */
       15u,                         /* MaxDeltaCounter */
       8u                           /* DataLength */
   };
   static E2E_StateType hb_e2e_state;
   ```
3. Init: `hb_e2e_state.Counter = 0u;`
4. Fix PDU construction:
   ```c
   for (i = 0u; i < 8u; i++) { tx_data[i] = 0u; }
   /* Bytes 0-1: reserved for E2E */
   tx_data[2] = FZC_ECU_ID;
   tx_data[3] = (uint8)(((fault_mask & 0x0Fu) << 4u) | (vehicle_state & 0x0Fu));
   (void)E2E_Protect(&hb_e2e_config, &hb_e2e_state, tx_data, 8u);
   ```
5. Keep `PduR_Transmit` for send, keep `Hb_AliveCounter` for Rte_Write diagnostic

**Test changes** (write FIRST):
- Update byte position defines: `HB_BYTE_ECU_ID=2`, combined `HB_BYTE_STATE_FAULT=3`
- Add E2E mock expectations (`E2E_Protect` called with DataId=0x03)
- ~15 of 27 FZC heartbeat tests affected

---

### Phase 3: Add E2E to RZC Heartbeat + Fix PDU Layout

**Status**: DONE (21/21 tests pass)

Same pattern as Phase 2 but for RZC.

**Files**:
- `firmware/rzc/src/Swc_Heartbeat.c`
- `firmware/rzc/include/Rzc_Cfg.h` — Data ID stays 0x04 (already correct per spec)
- `firmware/rzc/test/test_Swc_Heartbeat_asilc.c`

**Affected tests**: ~12 of 21 RZC heartbeat tests

---

### Phase 4: Fix CVC Heartbeat RX Com Signal Bit Positions

**Status**: DONE (config-only, 38/38 CVC tests pass)

**Files**:
- `firmware/cvc/cfg/Com_Cfg_Cvc.c` — fix RX signal bit positions
- `test/integration/test_int_heartbeat_loss_asild.c` — verify heartbeat loss detection still works

**Changes**:
```c
/* BEFORE (wrong — matches old firmware layout) */
{  9u,    0u,     8u, COM_UINT8, CVC_COM_RX_FZC_HB, &sig_rx_fzc_hb_alive  },
{ 10u,    8u,     8u, COM_UINT8, CVC_COM_RX_FZC_HB, &sig_rx_fzc_hb_ecu_id },
{ 11u,    0u,     8u, COM_UINT8, CVC_COM_RX_RZC_HB, &sig_rx_rzc_hb_alive  },
{ 12u,    8u,     8u, COM_UINT8, CVC_COM_RX_RZC_HB, &sig_rx_rzc_hb_ecu_id },

/* AFTER (correct — E2E layout: byte 0=[counter|dataid], 1=CRC, 2=ECU_ID) */
{  9u,    4u,     4u, COM_UINT8, CVC_COM_RX_FZC_HB, &sig_rx_fzc_hb_alive  },
{ 10u,   16u,     8u, COM_UINT8, CVC_COM_RX_FZC_HB, &sig_rx_fzc_hb_ecu_id },
{ 11u,    4u,     4u, COM_UINT8, CVC_COM_RX_RZC_HB, &sig_rx_rzc_hb_alive  },
{ 12u,   16u,     8u, COM_UINT8, CVC_COM_RX_RZC_HB, &sig_rx_rzc_hb_ecu_id },
```

**Risk**: CVC heartbeat monitoring uses `fzc_alive != fzc_last_alive` comparison. After fix, alive counter changes from 8-bit (0-255) to 4-bit (0-15). The comparison logic still works — it just detects changes in a smaller range. No code change needed in `Swc_Heartbeat.c` RX path.

---

### Phase 5: Migrate FZC Data Messages to Shared BSW E2E

**Status**: DONE (13/13 tests pass)

**Files**:
- `firmware/fzc/src/Swc_FzcCom.c` — replace custom E2E with shared BSW
- `firmware/fzc/include/Fzc_Cfg.h` — add Data ID defines per spec
- `firmware/fzc/test/test_Swc_FzcCom_asild.c` — update tests FIRST (TDD)

**Messages affected** (5):

| CAN ID | Message | Current State | Fix |
|--------|---------|--------------|-----|
| 0x200 | Steering_Status | No E2E (TODO) | Add E2E_Protect with DataId=0x09 |
| 0x201 | Brake_Status | No E2E (TODO) | Add E2E_Protect with DataId=0x0A |
| 0x210 | Brake_Fault | Custom E2E (reversed) | Replace with E2E_Protect, DataId=0x0B |
| 0x211 | Motor_Cutoff_Req | Custom E2E (reversed) | Replace with E2E_Protect, DataId=0x0C |
| 0x220 | Lidar_Distance | Custom E2E (reversed) | Replace with E2E_Protect, DataId=0x0D |

**Changes**:
1. Delete `FzcCom_CalcCrc8()`, `Swc_FzcCom_E2eProtect()`, `Swc_FzcCom_E2eCheck()`
2. Delete `FzcCom_TxAlive` shared counter
3. Add per-message E2E config + state (5 configs, 5 states):
   ```c
   static const E2E_ConfigType steer_e2e_cfg = { 0x09u, 15u, 8u };
   static E2E_StateType steer_e2e_state;
   /* ... same for brake, brake_fault, motor_cutoff, lidar */
   ```
4. In `Swc_FzcCom_TransmitSchedule()`: replace `Swc_FzcCom_E2eProtect(...)` with `E2E_Protect(&cfg, &state, pdu, len)`
5. In `Swc_FzcCom_Receive()`: replace `Swc_FzcCom_E2eCheck(...)` with `E2E_Check(&cfg, &state, pdu, len)`
6. Fix all TX PDU construction to put app data at bytes 2+ (not bytes 0+)

**Critical**: FZC Steering_Status (0x200) and Brake_Status (0x201) currently have **no E2E at all** — adding it is a safety improvement, not just a layout fix.

**Affected tests**: ~13 FZC Com tests

---

### Phase 6: Migrate RZC Data Messages to Shared BSW E2E

**Status**: DONE (6/6 tests pass)

**Files**:
- `firmware/rzc/src/Swc_RzcCom.c` — replace custom E2E with shared BSW
- `firmware/rzc/include/Rzc_Cfg.h` — add Data ID defines per spec
- `firmware/rzc/test/test_Swc_RzcCom_asild.c` — update tests FIRST (TDD)

**Messages affected** (3 — Battery_Status is QM, no E2E needed):

| CAN ID | Message | Current State | Fix |
|--------|---------|--------------|-----|
| 0x300 | Motor_Status | Custom E2E (reversed) | Replace with E2E_Protect, DataId=0x0E |
| 0x301 | Motor_Current | Custom E2E (reversed) | Replace with E2E_Protect, DataId=0x0F |
| 0x302 | Motor_Temperature | Custom E2E (reversed) | Replace with E2E_Protect, DataId=0x00 |

**Changes**:
1. Delete `RzcCom_Crc8()`, `Swc_RzcCom_E2eProtect()`, `Swc_RzcCom_E2eCheck()`
2. Delete `RzcCom_TxAlive[]` per-PDU array and `RzcCom_RxAlive[]`, `RzcCom_RxFailCount[]`
3. Add per-message E2E config + state (3 configs, 3 states)
4. Replace calls in `Swc_RzcCom_TransmitSchedule()` and `Swc_RzcCom_Receive()`
5. Fix all TX PDU construction to put app data at bytes 2+

**Affected tests**: ~6 RZC Com tests

---

### Phase 7: Fix CVC RX E2E Check for FZC/RZC Data Messages

**Status**: DONE (15/15 tests pass)

**Files**:
- `firmware/cvc/src/Swc_CvcCom.c` — update RX table Data IDs
- `firmware/cvc/cfg/Com_Cfg_Cvc.c` — fix RX signal bit positions for FZC/RZC data messages
- `firmware/cvc/test/test_Swc_CvcCom_asild.c` — update tests FIRST (TDD)

**Changes**:
1. Verify `CvcCom_RxTable` Data IDs match spec (0x0B for brake fault, 0x0F for motor current, etc.)
2. Fix Com signal bit positions for all FZC/RZC RX messages (app data at bytes 2+, not bytes 0+)
3. CVC E2E_Check will now **start passing** for FZC/RZC messages — this is the desired behavior

**Affected tests**: ~15 CVC Com tests + 5 integration tests

---

### Phase 8: Build All + Run Full Test Suite

**Status**: DONE (120/120 unit tests pass)

**Checklist**:
- [x] CVC Heartbeat tests — 38/38 pass
- [x] FZC Heartbeat tests — 27/27 pass
- [x] RZC Heartbeat tests — 21/21 pass
- [x] CVC Com tests — 15/15 pass
- [x] FZC Com tests — 13/13 pass
- [x] RZC Com tests — 6/6 pass
- [ ] `make test` — SC passes (should pass unchanged — SC already uses correct layout)
- [ ] Integration tests pass (`test/integration/`)
- [ ] MISRA check passes
- [ ] Deploy to SIL (Netcup) — `docker compose build --no-cache`
- [ ] Verify CAN monitor shows correct signals:
  - ECU_ID constant (CVC=1, FZC=2, RZC=3)
  - E2E_AliveCounter cycles 0-15
  - E2E_CRC8 changes each frame
  - OperatingMode shows vehicle state
  - No more "spiky" FZC/RZC signals
- [ ] Run full E2E SIL test suite — all scenarios green
- [ ] Flash to physical STM32s — verify on CAN bus

---

## 6. Test Impact Analysis

| Test File | Total Tests | Tests to Update | Reason |
|-----------|-------------|----------------|--------|
| `cvc/test/test_Swc_Heartbeat_asilc.c` | 39 | ~10 | PDU byte positions for ECU_ID, state |
| `fzc/test/test_Swc_Heartbeat_asilc.c` | 27 | ~15 | Add E2E expectations, fix byte positions |
| `rzc/test/test_Swc_Heartbeat_asilc.c` | 21 | ~12 | Add E2E expectations, fix byte positions |
| `cvc/test/test_Swc_CvcCom_asild.c` | 15 | ~8 | RX Data IDs, signal bit positions |
| `fzc/test/test_Swc_FzcCom_asild.c` | 13 | ~13 | All — custom E2E replaced entirely |
| `rzc/test/test_Swc_RzcCom_asild.c` | 6 | ~6 | All — custom E2E replaced entirely |
| `test/integration/test_int_e2e_chain_asild.c` | 5 | ~3 | Byte positions in roundtrip assertions |
| `test/integration/test_int_heartbeat_loss_asild.c` | 5 | ~2 | Heartbeat alive counter extraction |
| `sc/test/test_sc_e2e_asild.c` | 16 | 0 | Already uses correct layout |
| `sc/test/test_sc_can_asild.c` | 17 | 0 | Already uses correct layout |
| `sc/test/test_sc_heartbeat_asilc.c` | 24 | 0 | Already uses correct layout |
| **Total** | **~188** | **~69** | |

---

## 7. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Breaking CVC heartbeat monitoring of FZC/RZC | Medium | High | Alive counter comparison (`!=`) works with 4-bit values. Test thoroughly. |
| SC heartbeat timeout on boot | Low | Medium | SC startup grace period already handles boot transients. |
| SIL false positives from stale Docker layers | High | Medium | Always `docker compose build --no-cache` after firmware changes. |
| FZC/RZC E2E_Protect link error (missing E2E.c) | Low | Low | E2E.c already in shared BSW, already compiled for CVC. Add to FZC/RZC Makefile includes. |
| CVC E2E_Check starts rejecting messages during migration | Medium | High | Deploy all ECUs simultaneously. Don't deploy CVC RX fix before FZC/RZC TX fix. |

---

## 8. Key Design Decisions

1. **Migrate to shared BSW E2E** — single implementation, matches DBC/spec, already proven on CVC.
2. **Per-message E2E state** — each TX message gets its own `E2E_ConfigType` + `E2E_StateType` (no shared alive counters).
3. **Keep PduR_Transmit for FZC/RZC** — don't migrate to Com_SendSignal. Just add `E2E_Protect()` before the PduR call.
4. **Data IDs per CAN message matrix** — 16 unique IDs (0x00-0x0F) assigned to 16 E2E-protected messages.
5. **Battery_Status (0x303) stays without E2E** — QM message per spec, no safety requirement.
6. **FaultStatus encoding**: `byte[3] = (fault_mask_lo << 4) | (vehicle_state & 0x0F)` — high nibble = fault, low nibble = mode. Matches DBC.

---

## 9. Cascading Failure Chain (Reference)

```
PDU layout mismatch (root cause)
 +-- CVC Heartbeat TX: E2E destroys ECU_ID (byte 1)
 |    +-- CAN monitor shows garbage ECU_ID (cycling 0->24x)
 |
 +-- FZC/RZC Heartbeat TX: No E2E at all
 |    +-- SC E2E check always fails -> unreliable heartbeat timeout (FSR-015)
 |    +-- CVC E2E check always fails -> safety bypass (SSR-CVC-010)
 |
 +-- FZC data messages (0x210, 0x211, 0x220): reversed E2E bytes
 |    +-- CVC E2E_Check always fails
 |    |    +-- After 3 failures: safe default (zero torque, full brake)
 |    +-- CAN monitor decodes wrong byte positions -> "spiky" values
 |
 +-- FZC missing E2E (0x200, 0x201): ASIL D messages unprotected
 |    +-- No CRC -> bit errors go undetected
 |    +-- No alive counter -> stale/lost messages go undetected
 |
 +-- RZC data messages (0x300-0x302): reversed E2E bytes
 |    +-- CVC E2E_Check always fails -> false safe-default substitution
 |    +-- CAN monitor shows corrupted motor telemetry
 |
 +-- CVC RX Com config: wrong bit positions for heartbeat signals
      +-- Alive counter extraction reads wrong bits -> false timeout
```
