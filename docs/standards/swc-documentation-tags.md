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

### Interface tags (in file header or function header)

| Tag | Purpose | Standard | Example |
|-----|---------|----------|---------|
| `@consumes` | R-port signals read by this SWC | AUTOSAR R-PORT-PROTOTYPE | `@consumes IoHwAb_ReadBatteryVoltage` |
| `@produces` | P-port signals written by this SWC | AUTOSAR P-PORT-PROTOTYPE | `@produces RZC_SIG_BATTERY_MV, RZC_SIG_BATTERY_STATUS` |
| `@triggers` | What event triggers execution | AUTOSAR TIMING-EVENT / INIT-EVENT | `@triggers Rte_MainFunction 10ms cyclic` |

### Contract tags (in function header)

| Tag | Purpose | Standard | Example |
|-----|---------|----------|---------|
| `@pre` | Precondition | ISO 26262 Part 6 §8.4.3 | `@pre Swc_Battery_Init() called, IoHwAb initialized` |
| `@post` | Postcondition | ISO 26262 Part 6 §8.4.3 | `@post RZC_SIG_BATTERY_MV updated with 4-sample average` |
| `@invariant` | Must always hold | ISO 26262 Part 6 §8.4.3 | `@invariant 0 <= battery_mV <= 20000` |
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

## Example: Complete SWC Header

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
 * @produces    RZC_SIG_BATTERY_MV       (uint16, 0-20000 mV)
 *              RZC_SIG_BATTERY_STATUS   (uint8, enum: 0=crit_UV..4=crit_OV)
 *              RZC_SIG_BATTERY_SOC      (uint8, 0-100 %)
 * @reports     RZC_DTC_BATTERY (0x00E401) when avg_mV < 8000 for 3 cycles
 * @detects     undervoltage (<8000mV), overvoltage (>16000mV)
 * @safe_state  Hold last known good SOC, report DTC, set status to critical
 *
 * @pre         Swc_Battery_Init() called. IoHwAb initialized.
 * @post        RTE signals updated. DTC reported if threshold exceeded.
 * @invariant   0 <= Batt_Voltage_mV <= 20000
 *              0 <= Batt_Soc <= 100
 */
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
