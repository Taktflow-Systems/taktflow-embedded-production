# Signal Path Audit — Virtual Sensor Chain

**Date**: 2026-03-17
**Purpose**: Document the complete signal path for each virtual sensor, verify each hop.

## RZC Battery Voltage (0x601 → IoHwAb → Swc_Battery → 0x303)

### Expected Path

| Hop | From | To | ID/Value | File | Verified |
|-----|------|----|----------|------|----------|
| 1 | Plant-sim | vcan0 | CAN 0x601, bytes 4-5 = voltage_mV (uint16 LE) | `simulator.py:_tx_rzc_virtual_sensors()` | YES — 5000mV on bus |
| 2 | vcan0 | Can_Posix | `Can_MainFunction_Read()` reads socketCAN | `Can_Posix.c` | ? |
| 3 | Can_Posix | CanIf | `CanIf_RxIndication(0x601, data, 8)` | `CanIf.c` | ? |
| 4 | CanIf RX table | Match | `0x601 → PDU 22 (RZC_COM_RX_RZC_VIRTUAL_SENSORS)` | `main.c:canif_rx_config[]` | YES — config verified |
| 5 | CanIf | E2E check | `Rzc_E2eRxCheck(22, data, 8) → E_OK` (default case) | `Swc_RzcCom.c:238` | YES — code verified |
| 6 | CanIf | PduR | `PduR_CanIfRxIndication(22, pdu_info)` | `CanIf.c` | ? |
| 7 | PduR routing | Match | `{22, PDUR_DEST_COM, 22}` | `main.c:rzc_pdur_routing[]` | YES — config verified |
| 8 | PduR | Com | `Com_RxIndication(22, pdu_info)` | `PduR.c` | ? |
| 9 | Com RX buf | Copy | `com_rx_pdu_buf[22][0..7] = data` | `Com.c:Com_RxIndication` | ? |
| 10 | Com signal unpack | Signal 170 | `sig[170].PduId == 22, BitPos=32, BitSize=16` → battery_mV | `Com_Cfg_Rzc.c` | YES — config verified |
| 11 | Com shadow buf | Read | `Com_ReceiveSignal(170, &val) → val = battery_mV` | `Com.c` | FAIL — returns 0 |
| 12 | Sensor feeder | IoHwAb | `IoHwAb_Inject_SetSensorValue(5, val)` | `Swc_RzcSensorFeeder.c` | N/A (dep on 11) |
| 13 | IoHwAb | Swc_Battery | `IoHwAb_ReadBatteryVoltage(&raw) → raw = val` | `IoHwAb_Posix.c` | N/A |
| 14 | Swc_Battery | Dem | `Dem_ReportErrorStatus(9, FAILED)` if avg < 8000mV | `Swc_Battery.c:183` | N/A |
| 15 | Dem | CAN 0x500 | `PduR_Transmit(5, pdu)` → DTC_Broadcast | `Dem.c:289` | N/A |

### Failure Point

**Hop 11 fails**: `Com_ReceiveSignal(170)` returns 0. Hops 1-10 need verification.

### Verification Plan

For each unverified hop, add a `fprintf(stderr, ...)` trace and check:

1. **Hop 2**: Does `Can_MainFunction_Read()` receive the 0x601 frame?
   - Check: `Can_Posix.c` → add trace in `Can_Hw_Read()`
2. **Hop 3**: Does CanIf receive it?
   - Check: `CanIf.c:CanIf_RxIndication()` → add trace for canId == 0x601
3. **Hop 6/8**: Does PduR/Com get called?
   - Check: `Com.c:Com_RxIndication()` → add trace for pduId == 22
4. **Hop 9-10**: Does Com unpack signal 170?
   - Check: `Com.c:Com_RxIndication()` → trace signal unpack loop

## RZC Motor Current (0x601 → IoHwAb → Swc_CurrentMonitor → DTC)

| Hop | From | To | ID/Value |
|-----|------|----|----------|
| 1 | Plant-sim | vcan0 | CAN 0x601, bytes 0-1 = current_mA (uint16 LE) |
| 2-9 | Same as battery | Same | PDU 22 → Com signal 168 |
| 10 | Com | Sensor feeder | `Com_ReceiveSignal(168, &val)` |
| 11 | Sensor feeder | IoHwAb | `IoHwAb_Inject_SetSensorValue(MOTOR_CURRENT, val)` |
| 12 | IoHwAb | Swc_CurrentMonitor | `IoHwAb_ReadMotorCurrent(&val)` |
| 13 | Swc_CurrentMonitor | Dem | `Dem_ReportErrorStatus(RZC_DTC_OVERCURRENT, FAILED)` if > threshold |
| 14 | Dem | CAN 0x500 | DTC 0x00E301 broadcast |

Same failure point — Hop 10 returns 0 because Hop 9 fails.

## FZC Steering Angle (0x600 → IoHwAb → Swc_Steering → plausibility check)

| Hop | From | To | ID/Value |
|-----|------|----|----------|
| 1 | Plant-sim | vcan0 | CAN 0x600, bytes 0-1 = steer_raw (uint16 LE, 14-bit SPI format) |
| 2-9 | Same pattern | Same | FZC PDU 20 (FZC_COM_RX_FZC_VIRTUAL_SENSORS) → Com signal 165 |
| 10 | Com | Sensor feeder | `Com_ReceiveSignal(165, &val)` |
| 11 | Sensor feeder | IoHwAb | `IoHwAb_Inject_SetSensorValue(STEERING, val)` |
| 12 | IoHwAb | Swc_Steering | `IoHwAb_ReadSteeringAngle(&raw)` |
| 13 | Swc_Steering | plausibility | `|cmd - actual| > threshold → fault` |

Same root cause — Com signal not receiving from virtual sensor PDU.

## Root Cause Hypothesis

The `Com_RxIndication(22, ...)` is called but the signal unpack loop iterates over ALL signals (0-217). For each signal where `sig.PduId == 22`, it unpacks. Signal 170 has `PduId = RZC_COM_RX_RZC_VIRTUAL_SENSORS = 22`. Should match.

**BUT**: the Com config was generated with `RZC_COM_RX_RZC_VIRTUAL_SENSORS = 22`. The generated `Com_Cfg_Rzc.c` uses this define. If the generated file uses the define (not a literal 22), AND the define resolves correctly at compile time, then `sig[170].PduId = 22`. The `Com_RxIndication(22)` loop should find it.

**ROOT CAUSE FOUND (2026-03-17)**:
`COM_MAX_PDUS = 16u` in `Com.h`. PDU 22 (0x601) >= 16 → `Com_RxIndication`
returns `DET_E_PARAM_VALUE` immediately. Frame silently dropped.
Also `com_rx_pdu_buf[16]` — any write to index >= 16 is out-of-bounds.

**Fix**: Increased `COM_MAX_PDUS` to 48, `COM_MAX_SIGNALS` to 256 with `#ifndef` guards.
This affects ALL ECUs — every PDU >= 16 was broken across the entire system.
