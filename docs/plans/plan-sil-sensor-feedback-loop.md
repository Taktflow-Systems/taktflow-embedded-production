# Plan: SIL Sensor Feedback Loop

**Status**: ALL PHASES DONE
**Implemented**: 2026-03-03

## Context

After fixing the CVC 0x100 Vehicle_State CAN TX (E2E, FaultMask, TorqueLimit), testing revealed that **no fault scenario triggers SAFE_STOP** on the live SIL dashboard. Root cause: the entire SIL sensor feedback loop is broken.

**The problem**: POSIX HAL stubs (Adc, Spi, Pwm) return hardcoded "OK"/zero values. The plant-sim computes realistic physics (overcurrent, steering deviation, brake failure) but this data never reaches ECU firmware sensor APIs. SWC fault detection reads from IoHwAb -> MCAL stubs -> always returns OK -> no faults detected -> no CAN fault messages -> CVC never enters SAFE_STOP.

**Additional firmware bugs found during investigation**:
1. `Swc_Brake.c`: `Brake_Position = brake_cmd` -- no actual sensor read, fault detection is impossible
2. `Swc_FzcSafety.c`: does NOT trigger `motor_cutoff` for steering faults -- no path to CVC SAFE_STOP
3. `Swc_FzcSafety_MainFunction` was missing from FZC Rte_Cfg runnable table (dead code)

**Scope**: SIL only for injection infrastructure; firmware bugfixes (Swc_Brake, FzcSafety) apply to both SIL + real HW. Compatible with parallel hardware integration.

## Architecture

### Real Hardware Flow
```
Physical sensor -> MCAL driver -> IoHwAb -> SWC fault detection -> RTE -> Com TX -> CAN
```

### SIL Flow (after fix)
```
Plant-sim physics -> Virtual sensor CAN (0x400/0x401) -> Sensor feeder -> MCAL injection -> IoHwAb -> SWC fault detection -> RTE -> Com TX -> CAN
```

Steps from IoHwAb onward are **identical** between real HW and SIL. Only the sensor source differs.

## Virtual Sensor CAN Protocol

**0x400 -- FZC Virtual Sensors** (plant-sim -> FZC, 10ms cyclic, no E2E)

| Byte | Content | Type | Range | Mapping |
|------|---------|------|-------|---------|
| 0-1 | steering_angle | uint16 LE | 0-16383 | 14-bit SPI angle format |
| 2-3 | brake_position | uint16 LE | 0-1000 | ADC counts (maps to duty %) |
| 4-5 | brake_current | uint16 LE | 0-65535 | mA |
| 6-7 | reserved | - | 0 | - |

**0x401 -- RZC Virtual Sensors** (plant-sim -> RZC, 10ms cyclic, no E2E)

| Byte | Content | Type | Range | Mapping |
|------|---------|------|-------|---------|
| 0-1 | motor_current | uint16 LE | 0-65535 | mA |
| 2-3 | motor_temp | uint16 LE | 0-2000 | 0.1 C (0-200.0 C) |
| 4-5 | battery_voltage | uint16 LE | 0-65535 | mV |
| 6-7 | reserved | - | 0 | - |

## Phases

| Phase | Name | Status | Scope |
|-------|------|--------|-------|
| 1 | MCAL injection APIs + IoHwAb | DONE | SIL infra + both |
| 2 | Plant-sim virtual sensor TX | DONE | SIL only |
| 3 | FZC sensor feeder + CAN wiring | DONE | SIL only |
| 4 | Swc_Brake real sensor read | DONE | Both (bugfix) |
| 5 | Steering fault -> motor cutoff | DONE | Both (bugfix) |
| 6 | RZC sensor feeder + CAN wiring | DONE | SIL only |
| 7 | Disable plant-sim direct ECU TX | DONE | SIL cleanup |
| 8 | Verification + deploy | DONE (local) | - |

---

### Phase 1: MCAL Injection APIs + IoHwAb — DONE

**Files modified:**
- `firmware/shared/bsw/mcal/posix/Spi_Posix.c` -- added `Spi_Posix_InjectAngle(uint16 angle)` with injection priority over UDP override and default oscillation
- `firmware/shared/bsw/ecual/IoHwAb.h` -- added `BrakePositionAdcGroup` to `IoHwAb_ConfigType`, declared `IoHwAb_ReadBrakePosition()`
- `firmware/shared/bsw/ecual/IoHwAb.c` -- implemented `IoHwAb_ReadBrakePosition()` using ADC read + scaling (raw * 1000 / 4095)
- `firmware/shared/bsw/test/test_IoHwAb_asild.c` -- added `BrakePositionAdcGroup = 3u` to test config

### Phase 2: Plant-Sim Virtual Sensor TX — DONE

**File:** `gateway/plant_sim/simulator.py`
- [x] Added `_tx_fzc_virtual_sensors()` -> CAN 0x600 at 10ms (steering angle, brake position, brake current)
- [x] Added `_tx_rzc_virtual_sensors()` -> CAN 0x601 at 10ms (motor current, motor temp, battery voltage)
- [x] Added both to 10ms TX schedule

### Phase 3: FZC Sensor Feeder + CAN Wiring — DONE

**Files (6-file wiring pattern):**
- [x] `firmware/fzc/include/Fzc_Cfg.h` -- added `FZC_COM_RX_VIRT_SENSORS (4u)`, signal IDs 14-16, ADC group/channel constants
- [x] `firmware/fzc/cfg/Com_Cfg_Fzc.c` -- added shadow buffers, 3 signal config entries (IDs 14-16), RX PDU entry
- [x] `firmware/fzc/src/main.c` -- added CAN 0x600 to CanIf RX table, PduR routing, IoHwAb BrakePositionAdcGroup, sensor feeder init
- [x] `firmware/fzc/cfg/Rte_Cfg_Fzc.c` -- added sensor feeder runnable (prio 11), also fixed missing `Swc_FzcSafety_MainFunction` (was dead code!)
- [x] `firmware/fzc/src/Swc_FzcSensorFeeder.c` -- **NEW** module (reads Com -> injects Spi/Adc via POSIX APIs)
- [x] `firmware/fzc/include/Swc_FzcSensorFeeder.h` -- **NEW** header

### Phase 4: Swc_Brake Real Sensor Read — DONE (bugfix, both SIL + HW)

**File:** `firmware/fzc/src/Swc_Brake.c`
- [x] Replaced `Brake_Position = brake_cmd` with `IoHwAb_ReadBrakePosition()` call + scaling (ADC counts / 10 -> 0-100%)

### Phase 5: Steering Fault -> Motor Cutoff — DONE (bugfix, both SIL + HW)

**File:** `firmware/fzc/src/Swc_FzcSafety.c`
- [x] Added `#include "Com.h"`
- [x] Added Step 2b: steering fault triggers `Rte_Write(FZC_SIG_MOTOR_CUTOFF, 1u)` + `Com_SendSignal(FZC_COM_TX_MOTOR_CUTOFF, ...)`

### Phase 6: RZC Sensor Feeder + CAN Wiring — DONE

**Files (6-file wiring pattern):**
- [x] `firmware/rzc/include/Rzc_Cfg.h` -- added `RZC_COM_RX_VIRT_SENSORS (2u)`, signal IDs 18-20, ADC group/channel constants
- [x] `firmware/rzc/cfg/Com_Cfg_Rzc.c` -- added shadow buffers, 3 signal config entries (IDs 18-20), RX PDU entry
- [x] `firmware/rzc/src/main.c` -- added CAN 0x601 to CanIf RX table, PduR routing, sensor feeder init
- [x] `firmware/rzc/cfg/Rte_Cfg_Rzc.c` -- added sensor feeder runnable (prio 6, between Com RX and Motor SWC)
- [x] `firmware/rzc/src/Swc_RzcSensorFeeder.c` -- **NEW** module (reads Com -> injects Adc via POSIX API)
- [x] `firmware/rzc/include/Swc_RzcSensorFeeder.h` -- **NEW** header

### Phase 7: Disable Plant-Sim Direct ECU TX — DONE

- [x] `gateway/plant_sim/simulator.py` -- removed TX of 0x300 (Motor_Status), 0x301 (Motor_Current), 0x302 (Motor_Temp), 0x303 (Battery_Status)
- [x] `firmware/rzc/src/Swc_RzcCom.c` -- removed `#ifndef PLATFORM_POSIX` guard from `Swc_RzcCom_TransmitSchedule()` so RZC sends its own telemetry in SIL
- [x] Kept 0x220 (Lidar) and 0x500 (DTC) in plant-sim -- separate concerns

### Phase 8: Verification — DONE (local)

- [x] BSW unit tests: 22/22 passed, 0 failures
- [x] Integration tests: 11/11 passed, 0 failures
- [ ] Deploy to Netcup + full E2E verification (requires user action)

## File Impact Summary

| File | Change | Scope |
|------|--------|-------|
| `shared/bsw/mcal/posix/Spi_Posix.c` | Add injection API | SIL only |
| `shared/bsw/ecual/IoHwAb.h` | Add BrakePositionAdcGroup + ReadBrakePosition | Both |
| `shared/bsw/ecual/IoHwAb.c` | Implement ReadBrakePosition | Both |
| `shared/bsw/test/test_IoHwAb_asild.c` | Add BrakePositionAdcGroup to test | Test |
| `gateway/plant_sim/simulator.py` | Add 0x400/0x401 TX, remove 0x300-303 | SIL only |
| `fzc/src/main.c` | CanIf RX 0x400, PduR, sensor feeder init | SIL wiring |
| `fzc/cfg/Com_Cfg_Fzc.c` | RX PDU + 3 signals | Config |
| `fzc/include/Fzc_Cfg.h` | Constants | Config |
| `fzc/cfg/Rte_Cfg_Fzc.c` | Sensor feeder + FzcSafety runnables | Config |
| `fzc/src/Swc_FzcSensorFeeder.c` | **NEW** sensor feeder | SIL only |
| `fzc/include/Swc_FzcSensorFeeder.h` | **NEW** header | SIL only |
| `fzc/src/Swc_Brake.c` | Real sensor read (bugfix) | Both |
| `fzc/src/Swc_FzcSafety.c` | Motor cutoff on steer fault (bugfix) | Both |
| `rzc/src/main.c` | CanIf RX 0x401, PduR, sensor feeder init | SIL wiring |
| `rzc/cfg/Com_Cfg_Rzc.c` | RX PDU + 3 signals | Config |
| `rzc/include/Rzc_Cfg.h` | Constants | Config |
| `rzc/cfg/Rte_Cfg_Rzc.c` | Sensor feeder runnable | Config |
| `rzc/src/Swc_RzcSensorFeeder.c` | **NEW** sensor feeder | SIL only |
| `rzc/include/Swc_RzcSensorFeeder.h` | **NEW** header | SIL only |
| `rzc/src/Swc_RzcCom.c` | Remove PLATFORM_POSIX guard on TX | Both |

**20 files** total (4 new files, 16 modifications).

## Key Design Decisions

1. **CAN-based injection** (not UDP/shared memory) -- consistent with existing bus architecture, all containers already share vcan0
2. **Sensor feeders are separate SWCs** (not integrated into existing Com bridges) -- clean separation, easy to disable/remove, clearly SIL-only
3. **No E2E on virtual sensor CAN** -- SIL-only diagnostic aids, not safety-critical inter-ECU messages. TODO:HARDWARE for real sensor integration
4. **IoHwAb_ReadBrakePosition + Swc_Brake fix apply to real HW too** -- these are genuine firmware bugs, not SIL hacks
5. **FzcSafety was dead code** -- discovered missing runnable table entry during Phase 3, fixed

## Notes

- All MCAL injection code guarded by `#ifdef PLATFORM_POSIX` -- zero impact on real hardware builds
- Virtual sensor CAN IDs (0x600, 0x601) only transmitted by plant-sim -- not present on real CAN bus without plant-sim
- Compatible with parallel hardware integration -- real sensor drivers will replace MCAL stubs when hardware arrives
