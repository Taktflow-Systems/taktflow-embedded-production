# Plan: Fault Signal Path Hardening

**Date**: 2026-03-09
**Branch**: `fix/sil-bugfixes`
**Priority**: High — signal-path bugs masked by DTC belt-and-suspenders injection

## Problem Statement

Audit of all 11 fault injection scenarios revealed that 5 scenarios rely on direct DTC injection (CAN 0x500) as the primary detection path. The **signal-based** detection path — the one that would operate in production — has encoding bugs that were masked by this fallback.

### Bugs Found

| # | Component | File | Issue | Severity |
|---|-----------|------|-------|----------|
| 1 | **Motor_Status 0x300 encoding** | `gateway/plant_sim/simulator.py` | MotorFaultStatus was at byte 4 instead of byte 7 per DBC | **Fixed** |
| 2 | **RZC Motor_Current 0x301 encoding** | `firmware/rzc/src/Swc_RzcCom.c:580` | OvercurrentFlag written as `pdu[4] = overcurrent` — clobbers direction/enable bits, wrong bit position | **Open** |
| 3 | **Motor thermal model** | `gateway/plant_sim/motor_model.py` | Heat uses latched 28A current when `_hw_disabled`, causing 146°C spike | **Fixed** |

### Paths Verified OK

| Signal Path | DBC bitPos | Plant-Sim byte | FZC/RZC Com bitPos | Status |
|-------------|-----------|----------------|-------------------|--------|
| Steering_Status SteerFaultStatus | 48 (byte 6) | payload[6] | FZC sig 4: bitPos=48 | OK |
| Brake_Status BrakeFaultStatus | 48 (byte 6) | payload[6] | FZC sig 6: bitPos=16 (separate PDU) | OK |
| Motor_Status MotorFaultStatus | 56 (byte 7) | payload[7] | CVC sig 21: bitPos=56 | **Fixed** |
| Battery_Status BattVoltage | 16 (byte 2) | payload[2-3] | CVC sig: bitPos=16 | OK |

## Phases

### Phase 1: Fix RZC Motor_Current encoding — IN PROGRESS

**File**: `firmware/rzc/src/Swc_RzcCom.c` line 580

**Current** (wrong):
```c
pdu[4] = (uint8)overcurrent;  /* clobbers dir/enable bits, wrong position */
```

**Fix** (match DBC OvercurrentFlag at bit 34 = byte 4 bit 2):
```c
pdu[4] = (uint8)((current_ma >> 16u) & 0x03u)      /* CurrentMA bits [17:16] if needed */
       | (uint8)(overcurrent << 2u);                 /* OvercurrentFlag at bit 34 */
```

Actually, per DBC and plant-sim reference encoding:
```c
/* Byte 4: direction(1) | enable(1) | overcurrent(1) — bits [32..34] */
pdu[4] = (uint8)motor_dir
       | (uint8)(motor_enable << 1u)
       | (uint8)(overcurrent << 2u);
```

**DONE criteria**: CVC signal 21 reads non-zero MotorFaultStatus within 30ms of overcurrent injection (without DTC belt-and-suspenders).

### Phase 2: Add DBC-alignment comments — PENDING

Add byte-layout comments to all plant-sim TX functions matching the format already used in `_tx_motor_status()`. This prevents future drift.

Files:
- `gateway/plant_sim/simulator.py`: `_tx_motor_current`, `_tx_motor_temp`, `_tx_steering_status`, `_tx_brake_status`, `_tx_battery_status`

### Phase 3: Lesson learned — PENDING

Append to `docs/lessons-learned/web/ui-safety.md`.
