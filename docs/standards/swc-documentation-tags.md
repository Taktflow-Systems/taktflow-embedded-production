# SWC Documentation Tags Standard

**Status**: DRAFT — review before enforcement
**Date**: 2026-03-18
**Applies to**: All files in `firmware/ecu/*/src/Swc_*.c` and `firmware/ecu/*/include/Swc_*.h`

## Purpose

Every SWC file must carry metadata that answers:
1. **Who owns it?** — when it breaks, who to call
2. **What requirement does it satisfy?** — traceability for ASPICE SWE.3 / ISO 26262 Part 6
3. **What data does it consume and produce?** — interface contract
4. **What are the constraints?** — timing, ASIL, resource budget
5. **What is the failure behavior?** — safe state, DTC, degradation

## Required Tags

### File-level (in file header comment)

| Tag | Purpose | Standard | Example |
|-----|---------|----------|---------|
| `@file` | File name | Doxygen standard | `@file Swc_Battery.c` |
| `@brief` | One-line description | Doxygen standard | `@brief Battery voltage monitoring and SOC estimation` |
| `@owner` | Responsible role/person | ASPICE SWE.1 (responsibility) | `@owner RZC Application` |
| `@ecu` | Target ECU | AUTOSAR system mapping | `@ecu RZC` |
| `@asil` | Safety integrity level | ISO 26262 Part 6 §5.4.3 | `@asil QM` or `@asil C` |
| `@satisfies` | Requirements this file implements | Doxygen built-in, ISO 26262 Part 6 §8.4.4 | `@satisfies SSR-RZC-006, SSR-RZC-007` |
| `@period` | Cyclic execution period | AUTOSAR TIMING-EVENT | `@period 10ms` |
| `@wcet` | Worst-case execution time budget | ISO 26262 Part 6 §7.4.14 | `@wcet 200us` |

### Data flow tags (in file header or function header)

| Tag | Purpose | Standard | Example |
|-----|---------|----------|---------|
| `@consumes` | R-port signals read by this SWC | AUTOSAR R-PORT-PROTOTYPE | `@consumes RZC_SIG_TEMP_FAULT (from Swc_TempMonitor)` |
| `@produces` | P-port signals written by this SWC | AUTOSAR P-PORT-PROTOTYPE | `@produces RZC_SIG_BATTERY_MV → Swc_RzcCom → CAN 0x303` |

### Call chain tags (on EVERY function)

| Tag | Purpose | Standard | Example |
|-----|---------|----------|---------|
| `@calledby` | Who calls this function, and when | Call graph traceability | `@calledby Rte_MainFunction every 10ms (Rte_Cfg_Rzc.c entry 4)` |
| `@calls` | What this function calls (BSW APIs, other SWCs) | Call graph traceability | `@calls IoHwAb_ReadBatteryVoltage, Rte_Write, Dem_ReportErrorStatus` |
| `@triggers` | What event starts execution | AUTOSAR TIMING-EVENT | `@triggers TIMING-EVENT 10ms (ARXML: Swc_Battery_InternalBehavior)` |
| `@notifies` | What downstream is affected when this runs | Event chain | `@notifies Swc_RzcCom (reads RZC_SIG_BATTERY_MV next 20ms cycle)` |
| `@task` | OS task this runs in, with priority | AUTOSAR OS task mapping | `@task Task_10ms priority=3 (0=lowest)` |
| `@deadline` | Must complete within this time | ISO 26262 timing constraint | `@deadline 1ms (period=10ms, utilization budget=10%)` |
| `@order` | Execution order within same task/period | Runnable scheduling | `@order 4 of 12 in Task_10ms (after CurrentMonitor, before RzcCom)` |

### Contract tags (on EVERY function)

| Tag | Purpose | Standard | Example |
|-----|---------|----------|---------|
| `@pre` | Precondition — must be true BEFORE call | ISO 26262 Part 6 §8.4.3 | `@pre Swc_Battery_Init() called, IoHwAb initialized` |
| `@post` | Postcondition — guaranteed true AFTER call | ISO 26262 Part 6 §8.4.3 | `@post RZC_SIG_BATTERY_MV updated with 4-sample average` |
| `@invariant` | Must always hold during execution | ISO 26262 Part 6 §8.4.3 | `@invariant 0 <= battery_mV <= 20000` |
| `@safe_state` | What happens on failure | ISO 26262 Part 4 §6 | `@safe_state Report DTC, hold last known good value` |

### Fault/DTC tags

| Tag | Purpose | Standard | Example |
|-----|---------|----------|---------|
| `@reports` | DTC codes reported by this SWC | AUTOSAR Dem | `@reports RZC_DTC_BATTERY (0x00E401)` |
| `@detects` | Fault conditions detected | ISO 26262 FMEA | `@detects undervoltage (<8V), overvoltage (>16V)` |

### Traceability tags (for test files)

| Tag | Purpose | Standard | Example |
|-----|---------|----------|---------|
| `@verifies` | Requirements verified by this test | Doxygen built-in, ASPICE SWE.4 | `@verifies SSR-RZC-006` |
| `@covers` | Code coverage target | ASPICE SWE.4 | `@covers Swc_Battery_MainFunction` |

## Example: Complete SWC File Header

```c
/**
 * @file    Swc_Battery.c
 * @brief   Battery voltage monitoring — 4-sample average, hysteresis, DTC
 *
 * @owner       RZC Application
 * @ecu         RZC
 * @asil        QM
 * @satisfies   SSR-RZC-006 (battery voltage monitoring)
 *              SSR-RZC-007 (battery undervoltage detection)
 * @period      10ms (cyclic via Rte_MainFunction)
 * @wcet        50us (measured on STM32F446RE @ 180MHz)
 *
 * @consumes    IoHwAb_ReadBatteryVoltage (from Swc_RzcSensorFeeder on SIL)
 * @produces    RZC_SIG_BATTERY_MV       (uint16, 0-20000 mV)  → Swc_RzcCom → CAN 0x303
 *              RZC_SIG_BATTERY_STATUS   (uint8, enum)          → Swc_RzcSafety, Swc_RzcCom
 *              RZC_SIG_BATTERY_SOC      (uint8, 0-100 %)       → Swc_RzcCom → CAN 0x303
 * @reports     RZC_DTC_BATTERY (0x00E401) when avg_mV < 8000 for 3 cycles
 * @detects     undervoltage (<8000mV), overvoltage (>16000mV)
 * @safe_state  Hold last known good SOC, report DTC, set status to critical
 */
```

## Example: Complete Function Header (with call chain)

```c
/**
 * @brief   Cyclic battery monitoring — read, average, detect, report
 *
 * @calledby    Rte_MainFunction every 10ms (Rte_Cfg_Rzc.c entry 8, task 0x02)
 * @calls       IoHwAb_ReadBatteryVoltage — reads ADC (SIL: from Swc_RzcSensorFeeder)
 *              Dem_ReportErrorStatus     — when avg < 8000mV or avg > 16000mV
 *              Rte_Write                 — RZC_SIG_BATTERY_MV, _STATUS, _SOC
 * @notifies    Swc_RzcCom (reads BATTERY_MV next 200ms cycle → packs CAN 0x303)
 *              Swc_RzcSafety (reads BATTERY_STATUS next 10ms cycle → fault mask)
 *              CVC Swc_VehicleState (receives CAN 0x303 → Com→RTE auto-bind → reads status)
 *
 * @pre         Swc_Battery_Init() called. IoHwAb initialized. SensorFeeder running.
 * @post        RZC_SIG_BATTERY_MV = 4-sample moving average of IoHwAb voltage.
 *              RZC_SIG_BATTERY_STATUS = threshold evaluation with hysteresis.
 *              DTC reported if DISABLE_LOW or DISABLE_HIGH for 3 consecutive calls.
 * @invariant   0 <= Batt_Voltage_mV <= 20000
 *              0 <= Batt_Soc <= 100
 *              Batt_AvgIndex < RZC_BATT_AVG_WINDOW
 */
void Swc_Battery_MainFunction(void)
```

## Example: Full Call Chain for One Safety Path

```
SG-001: Prevent unintended motor movement during overcurrent

Swc_RzcSensorFeeder_MainFunction()
  @calledby  Rte_MainFunction 10ms
  @calls     Rte_Read(RZC_SIG_RZC_VIRTUAL_SENSORS_VSENSOR_MOTOR_CURRENT)
             IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_MOTOR_CURRENT)
  @notifies  Swc_CurrentMonitor (reads IoHwAb next 1ms cycle)

Swc_CurrentMonitor_MainFunction()
  @calledby  Rte_MainFunction 1ms
  @calls     IoHwAb_ReadMotorCurrent()
             Rte_Write(RZC_SIG_OVERCURRENT)
             Dem_ReportErrorStatus(RZC_DTC_OVERCURRENT)
  @notifies  Swc_Motor (reads RZC_SIG_OVERCURRENT next 10ms cycle)
             Dem_MainFunction (broadcasts DTC on CAN 0x500 next 100ms cycle)

Swc_Motor_MainFunction()
  @calledby  Rte_MainFunction 10ms
  @calls     Rte_Read(RZC_SIG_OVERCURRENT)
             Rte_Read(RZC_SIG_TEMP_FAULT)
             Rte_Write(RZC_SIG_MOTOR_FAULT)
             Motor_DisableOutputs()
  @notifies  Swc_RzcCom (reads RZC_SIG_MOTOR_FAULT next 20ms → CAN 0x300 byte 7)

Swc_RzcCom_TransmitSchedule()
  @calledby  main.c 10ms task
  @calls     Rte_Read(RZC_SIG_MOTOR_FAULT)
             E2E_Protect()
             PduR_Transmit(RZC_COM_TX_MOTOR_STATUS)
  @notifies  CVC (receives CAN 0x300 via Com→RTE auto-bind)

CVC Swc_CvcCom_BridgeRxToRte()
  @calledby  main.c 10ms task
  @calls     Com_ReceiveSignal(CVC_COM_SIG_MOTOR_STATUS_MOTOR_FAULT_STATUS)
             Rte_Write(CVC_SIG_MOTOR_FAULT_RZC)
  @notifies  Swc_VehicleState (reads CVC_SIG_MOTOR_FAULT_RZC next 10ms cycle)

CVC Swc_VehicleState_MainFunction()
  @calledby  Rte_MainFunction 10ms
  @calls     Rte_Read(CVC_SIG_MOTOR_FAULT_RZC)
             Swc_VehicleState_OnEvent(CVC_EVT_MOTOR_CUTOFF)
  @notifies  Vehicle enters SAFE_STOP → CAN 0x100 VehicleState=4
```

## Example: Test File Header

```c
/**
 * @file    test_Swc_Battery_qm.c
 * @brief   Unit tests for Swc_Battery
 *
 * @owner       RZC Test
 * @verifies    SSR-RZC-006, SSR-RZC-007
 * @covers      Swc_Battery_Init, Swc_Battery_MainFunction
 * @asil        QM (test code, not deployed)
 */
```

## CI Enforcement

1. **Tag presence check**: every `Swc_*.c` must have `@owner`, `@ecu`, `@asil`, `@satisfies`
2. **Tag consistency check**: `@ecu` matches directory path (`firmware/ecu/rzc/` → `@ecu RZC`)
3. **Traceability check**: every `@satisfies SSR-*` must exist in requirements document
4. **Interface check**: every `@produces` signal must have a corresponding `Rte_Write` call in the file
5. **DTC check**: every `@reports` DTC code must match `Dem_SetDtcCode` in main.c

## Standards References

- [Doxygen \satisfies / \verifies commands](https://www.doxygen.nl/manual/commands.html)
- [AUTOSAR SWC Modeling Requirements (R22-11)](https://www.autosar.org/fileadmin/standards/R22-11/CP/AUTOSAR_RS_SWCModeling.pdf)
- [AUTOSAR C++14 Rule A2-7-3: Documentation for all declarations](https://www.mathworks.com/help/bugfinder/ref/autosarc14rulea273.html)
- [ISO 26262 Part 6: Software development](https://autosar.io/en/insights/aspice-vs-iso26262)
- [ASPICE SWE.3: Software detailed design and unit construction](https://www.mathworks.com/discovery/automotive-spice.html)
- [TI Hercules MCU C Coding Guidelines (Doxygen headers)](https://software-dl.ti.com/hercules/hercules_public_sw/HerculesMCU_C_CodingGuidelines.pdf)
- [ISO 26262/ASPICE Traceability Guide (Sodiuswillert)](https://www.sodiuswillert.com/en/blog/traceability-standards-regulations-in-the-automotive-industry)
