# Integration Audit — Taktflow Embedded Production

**Date:** 2026-03-27
**Scope:** BSW stack, codegen pipeline, test framework, SIL/HIL infrastructure, safety architecture
**Auditor:** Claude Code (automated structural analysis)
**Status:** SNAPSHOT — reflects repo state at audit date

---

## Table of Contents

1. [Repository Overview](#1-repository-overview)
2. [BSW Stack Architecture](#2-bsw-stack-architecture)
3. [ECU Application Layer](#3-ecu-application-layer)
4. [Platform Abstraction Layer](#4-platform-abstraction-layer)
5. [Codegen Pipeline (DBC → ARXML → C)](#5-codegen-pipeline)
6. [Test Framework & Verification](#6-test-framework--verification)
7. [SIL / HIL Infrastructure](#7-sil--hil-infrastructure)
8. [Gateway & Edge Services](#8-gateway--edge-services)
9. [Safety Architecture (ISO 26262 / ASPICE)](#9-safety-architecture)
10. [CI/CD Gates](#10-cicd-gates)
11. [Key Findings & Observations](#11-key-findings--observations)

---

## 1. Repository Overview

### 1.1 Topology

```
taktflow-embedded-production/          ← this repo (submodule in parent workspace)
├── firmware/
│   ├── bsw/                           — AUTOSAR-like BSW stack (platform-independent)
│   ├── ecu/{cvc,fzc,rzc,sc,bcm,icu,tcu}/  — Per-ECU SWCs + mains
│   ├── platform/{stm32,stm32f4,stm32l5,tms570,posix,qnx}/  — MCAL implementations
│   └── lib/vendor/                    — Wrapped vendor libraries
├── gateway/                           — Edge services (CAN bridge, cloud, plant sim, ML)
├── tools/
│   ├── arxml/                         — DBC→ARXML converter + SWC extractor
│   ├── arxmlgen/                      — Jinja2 C code generator
│   ├── misra/                         — MISRA C:2012 cppcheck config
│   └── ci/                            — CI/CD scripts
├── test/
│   ├── unit/bsw/                      — Per-module Unity tests
│   ├── integration/bsw/               — BSW dataflow integration
│   ├── framework/                     — 11 cross-stack integration suites (ASIL C/D)
│   ├── sil/                           — SIL scenario scripts
│   ├── hil/                           — HIL scenario scripts
│   ├── mil/                           — MIL scenarios
│   └── pil/                           — PIL scenarios
├── docker/                            — Dockerfiles + compose for SIL
├── docs/                              — ASPICE, ISO 26262, plans, lessons learned
├── gateway/taktflow_vehicle.dbc       — SINGLE SOURCE OF TRUTH for CAN matrix
├── arxml/TaktflowSystem.arxml         — AUTOSAR ARXML model (generated from DBC)
└── model/ecu_sidecar.yaml             — ECU metadata (ASIL, RTE mappings)
```

### 1.2 ECU Inventory

| ECU | Processor | ASIL | Type | Role |
|-----|-----------|------|------|------|
| **CVC** | STM32G474RE | D | Physical | Central vehicle controller — mode management, pedal arbitration, heartbeat coordination |
| **FZC** | STM32G474RE | D | Physical | Front zone — steering servo, brake actuation, lidar proximity |
| **RZC** | STM32G474RE | C | Physical | Rear zone — motor torque, battery, encoder, temperature monitoring |
| **SC** | TMS570LC43x | D | Physical | Safety controller — lockstep, heartbeat timeout, torque cutoff, relay control |
| **BCM** | POSIX / Docker | QM | Virtual | Body control — lights, door locks, indicators |
| **ICU** | POSIX / Docker | QM | Virtual | Instrument cluster — dashboard display, DTC readout |
| **TCU** | POSIX / Docker | ASIL B | Virtual | Transmission control — UDS server, OBD-II PIDs, gear sequencing |

---

## 2. BSW Stack Architecture

### 2.1 Confirmed Module Inventory

Verified by presence of headers in `firmware/bsw/`:

#### MCAL Layer (`firmware/bsw/mcal/`)
| Module | Header | Purpose |
|--------|--------|---------|
| Can | `Can/include/Can.h` | CAN controller driver (TX/RX, ISR hooks) |
| Adc | `Adc/include/Adc.h` | Analog-to-digital conversion |
| Dio | `Dio/include/Dio.h` | Digital I/O |
| Gpt | `Gpt/include/Gpt.h` | General-purpose timer |
| Pwm | `Pwm/include/Pwm.h` | PWM output (motor, servo) |
| Spi | `Spi/include/Spi.h` | SPI bus (OLED, sensor peripherals) |
| Uart | `Uart/include/Uart.h` | UART serial |

#### ECUAL Layer (`firmware/bsw/ecual/`)
| Module | Header | Purpose |
|--------|--------|---------|
| CanIf | `CanIf/include/CanIf.h` | CAN interface — ID routing, PDU buffering, RX dispatch |
| PduR | `PduR/include/PduR.h` | PDU router — Com ↔ CanIf ↔ CanTp ↔ Dcm ↔ Xcp |
| IoHwAb | `IoHwAb/include/IoHwAb.h` | I/O hardware abstraction (ADC/DIO/PWM wrappers) |
| IoHwAb_Posix | `IoHwAb/include/IoHwAb_Posix.h` | POSIX stub for SIL |
| IoHwAb_Hil | `IoHwAb/include/IoHwAb_Hil.h` | HIL injection variant |
| IoHwAb_Inject | `IoHwAb/include/IoHwAb_Inject.h` | Fault injection interface |

#### Services Layer (`firmware/bsw/services/`)
| Module | Header | Purpose |
|--------|--------|---------|
| Com | `Com/include/Com.h` | Signal packing/unpacking, E2E, RX timeout |
| E2E | `E2E/include/E2E.h` | E2E Profile P01 — CRC-8 + alive counter |
| E2E_Sm | `E2E/include/E2E_Sm.h` | E2E state machine (OK, ERROR, INIT) |
| Dcm | `Dcm/include/Dcm.h` | Diagnostic communication manager (UDS) |
| Dem | `Dem/include/Dem.h` | Diagnostic event manager (DTC storage) |
| Det | `Det/include/Det.h` | Default error tracer |
| BswM | `BswM/include/BswM.h` | BSW mode manager (mode state machine + actions) |
| CanSM | `CanSM/include/CanSM.h` | CAN state manager (bus-off detection + recovery) |
| CanTp | `CanTp/include/CanTp.h` | ISO-TP transport (segmented UDS/XCP frames) |
| FiM | `FiM/include/FiM.h` | Function inhibition manager (DEM-driven inhibition) |
| NvM | `NvM/include/NvM.h` | Non-volatile memory service |
| SchM | `SchM/include/SchM.h` | Scheduler manager (cyclic task dispatch) |
| WdgM | `WdgM/include/WdgM.h` | Watchdog manager (alive, deadline, program flow) |
| Xcp | `Xcp/include/Xcp.h` | XCP calibration protocol |
| Sil | `Sil/include/Sil_Time.h` | SIL timing service (POSIX clock shim) |

#### RTE (`firmware/bsw/rte/`)
| Module | Header | Purpose |
|--------|--------|---------|
| Rte | `rte/include/Rte.h` | Runtime environment — type-safe inter-SWC signal routing |

#### OS Bootstrap (`firmware/bsw/os/bootstrap/`)
| Module | Header | Purpose |
|--------|--------|---------|
| Os | `include/Os.h` | OS abstraction API |
| Os_TaskMap | `include/Os_TaskMap.h` | Task-to-runnable binding table |
| Os_Port | `port/include/Os_Port.h` | Port interface |
| Os_Port_TaskBinding | `port/include/Os_Port_TaskBinding.h` | Task binding types |

#### Base Types (`firmware/bsw/include/`)
- `Std_Types.h` — AUTOSAR standard types (Std_ReturnType, uint8, uint16…)
- `Platform_Types.h` — Platform-independent type aliases
- `ComStack_Types.h` — Communication stack types (PduInfoType, PduIdType…)
- `Compiler.h` — Compiler abstraction macros

### 2.2 Signal Data Flow (RX path)

```
Hardware (CAN controller)
    ↓  interrupt
Can_RxISR()                      [firmware/bsw/mcal/Can/]
    ↓  CanIf_RxIndication()
CanIf                            [firmware/bsw/ecual/CanIf/]
    ↓  PduR_RxIndication()
PduR                             [firmware/bsw/ecual/PduR/]
    ↓  Com_RxIndication()
Com  (+E2E_Check if configured)  [firmware/bsw/services/Com/]
    ↓  Rte_Write_<signal>()
RTE                              [firmware/bsw/rte/]
    ↓  Rte_Read_<signal>()
SWC (application)                [firmware/ecu/<ecu>/src/]
```

TX path is the mirror image (SWC → Rte_Write → Com → PduR → CanIf → Can_Write → HW).

### 2.3 Scheduler Architecture

`SchM` dispatches cyclic runnables in the main loop tick (10 ms base). Each ECU's `main.c` calls `SchM_MainFunction()` after BSW init. `Os_TaskMap` binds runnables to task slots. Platform-specific OS port (`Os_Port_Stm32.c`, `Os_Port_Tms570.c`, `Os_Port_Posix.c`) provides the tick source.

SC (TMS570) does **not** use the AUTOSAR BSW stack — it runs bare-metal with its own minimal scheduler (`sc_main.c`).

---

## 3. ECU Application Layer

### 3.1 Confirmed SWC Files by ECU

#### CVC (`firmware/ecu/cvc/src/`)
| File | Function |
|------|----------|
| `main.c` | BSW init, self-test, 10 ms tick loop |
| `Swc_Pedal.c` | Dual-channel ADC pedal read + discrepancy fault |
| `Swc_VehicleState.c` | Vehicle state machine (INIT → READY → DRIVE → FAULT) |
| `Swc_EStop.c` | Emergency stop logic |
| `Swc_Heartbeat.c` | CVC heartbeat TX, monitors FZC/RZC/SC |
| `Swc_Dashboard.c` | SSD1306 OLED display driver |
| `Swc_CvcCom.c` | CAN communication dispatcher |
| `Swc_CvcDcm.c` | UDS diagnostic service handlers |
| `Swc_Nvm.c` | Non-volatile configuration storage |
| `Swc_CanMonitor.c` | Real-time CAN frame monitor |
| `Swc_Scheduler.c` | SWC-level cyclic task orchestration |
| `Swc_SelfTest.c` | POST (power-on self-test) |
| `Swc_Watchdog.c` | Hardware watchdog kick |
| `Ssd1306.c` | OLED driver (SPI) |

#### FZC (`firmware/ecu/fzc/src/`)
`Swc_Steering.c`, `Swc_Brake.c`, `Swc_Lidar.c`, `Swc_Heartbeat.c`, `Swc_FzcCom.c`, `Swc_FzcDcm.c`, `Swc_FzcNvm.c`, `Swc_FzcScheduler.c`, `Swc_FzcSafety.c`, `Swc_FzcSensorFeeder.c`, `Swc_FzcCanMonitor.c`, `Swc_Buzzer.c`, `main.c`

#### RZC (`firmware/ecu/rzc/src/`)
`Swc_Motor.c`, `Swc_Battery.c`, `Swc_Encoder.c`, `Swc_TempMonitor.c`, `Swc_CurrentMonitor.c`, `Swc_RzcCom.c`, `Swc_RzcDcm.c`, `Swc_RzcNvm.c`, `Swc_RzcSafety.c`, `Swc_RzcScheduler.c`, `Swc_RzcSelfTest.c`, `Swc_Heartbeat.c`, `Swc_RzcSensorFeeder.c`, `main.c`

#### SC (`firmware/ecu/sc/src/`) — bare-metal, no AUTOSAR BSW
`sc_main.c`, `sc_can.c`, `sc_e2e.c`, `sc_esm.c`, `sc_heartbeat.c`, `sc_led.c`, `sc_monitoring.c`, `sc_plausibility.c`, `sc_relay.c`, `sc_selftest.c`, `sc_state.c`, `sc_watchdog.c`

#### BCM (`firmware/ecu/bcm/src/`)
`bcm_main.c`, `Swc_BcmMain.c`, `Swc_BcmCan.c`, `Swc_DoorLock.c`, `Swc_Lights.c`, `Swc_Indicators.c`

#### ICU (`firmware/ecu/icu/src/`)
`icu_main.c`, `Swc_Dashboard.c`, `Swc_DtcDisplay.c`

#### TCU (`firmware/ecu/tcu/src/`)
`tcu_main.c`, `Swc_DataAggregator.c`, `Swc_DtcStore.c`, `Swc_Obd2Pids.c`, `Swc_UdsServer.c`

### 3.2 SWC Interface Pattern

All SWCs follow `Swc_<Module>_Init()` / `Swc_<Module>_Main()` — init called once at startup, main called every 10 ms by scheduler. Access to signals exclusively via `Rte_Read_<ECU>_<signal>()` / `Rte_Write_<ECU>_<signal>()`. No SWC calls Com, CanIf, or MCAL directly.

### 3.3 Generated Config Files (per ECU `cfg/`)

All files under `firmware/ecu/*/cfg/` carry `/* GENERATED -- DO NOT EDIT */` headers and are produced by `tools/arxmlgen/`. Categories per ECU:

- `Com_Cfg_<ECU>.c/.h` — Signal packing tables, TX PDU periodicity, E2E DataID map
- `Rte_Cfg_<ECU>.c/.h` — RTE buffer pool and signal routing
- `Rte_Wrapper_<ECU>.c` — Type-safe `Rte_Read_*` / `Rte_Write_*` wrappers
- `CanIf_Cfg_<ECU>.c/.h` — CAN ID → PDU handle routing
- `PduR_Cfg_<ECU>.c/.h` — PDU routing paths
- `E2E_Cfg_<ECU>.c/.h` — E2E Profile P01 parameters (CRC offset, counter bit, DataID)
- `CanTp_Cfg_<ECU>.c/.h` — ISO-TP channel configuration
- `Dcm_Cfg_<ECU>.c/.h` — UDS DID table

**Rule (CLAUDE.md / development-discipline.md):** ECU `main.c` uses `extern` to reference these structs. Static local copies of CanIf/PduR/Com config are forbidden (check: `grep -rn "static.*ConfigType.*config" firmware/ecu/*/src/main.c` must be zero).

---

## 4. Platform Abstraction Layer

### 4.1 Confirmed Platform Ports

| Platform | Directory | Targets | Notes |
|----------|-----------|---------|-------|
| STM32 (G4) | `firmware/platform/stm32/` | CVC, FZC physical boards | `Can_Hw_STM32.c`, `Adc_Hw_STM32.c`, `Dio_Hw_STM32.c`, `Gpt_Hw_STM32.c`, `Pwm_Hw_STM32.c`, `Spi_Hw_STM32.c`, `Uart_Hw_STM32.c`, `Os_Port_Stm32.c/.S`, `cvc_hw_stm32.c`, `fzc_hw_stm32.c`, `rzc_hw_stm32.c` |
| STM32F4 | `firmware/platform/stm32f4/` | RZC F4 variant, diag builds | `Can_Hw_STM32F4.c`, `rzc_f4_hw_stm32f4.c`, diagnostic test binaries |
| STM32L5 | `firmware/platform/stm32l5/` | L5 experiment (Cortex-M33, TrustZone) | `Os_Port_Stm32L5.c/.S`, own `Makefile.stm32l5` |
| TMS570 | `firmware/platform/tms570/` | SC safety controller | `sc_hw_tms570.c`, `Os_Port_Tms570.c/.S`, `can_loopback_test.c` |
| POSIX | `firmware/platform/posix/` | SIL (Docker/Linux), unit tests | `Can_Posix.c` (SocketCAN), `Adc_Posix.c`, `Dio_Posix.c`, `Gpt_Posix.c`, `Pwm_Posix.c`, `Spi_Posix.c`, `Uart_Posix.c`, `Os_Port_Posix.c`, per-ECU hw shims |
| QNX (stub) | `firmware/platform/qnx/` | Future RTOS target | `Can_Qnx.c`, `sc_hw_qnx.c` — stub implementations |

### 4.2 ThreadX Integration (STM32 G4)

`firmware/platform/stm32/src/` contains `tx_initialize_low_level.S`, `tx_stubs.c`, `stm32g4xx_it_threadx.c`, `stm32g4xx_hal_timebase_tim.c` and `include/tx_user.h`. This reflects the ThreadX OS port experiment (Cortex-M4, G474RE) as a scheduled background workstream alongside the bare-metal scheduler.

### 4.3 Platform Selection (Build)

```bash
make -f firmware/platform/stm32/Makefile.stm32 build       # STM32 G4 (CVC, FZC, RZC)
make -f firmware/platform/tms570/Makefile.tms570 build      # TMS570 (SC)
make -f firmware/platform/posix/Makefile.posix build        # POSIX SIL
make -f firmware/platform/stm32l5/Makefile.stm32l5 build    # STM32L5 experiment
make -f firmware/platform/stm32f4/Makefile.stm32f4 build    # STM32F4 diag
```

---

## 5. Codegen Pipeline

### 5.1 Single Source of Truth

```
gateway/taktflow_vehicle.dbc
    │  (CAN message matrix: 32 messages, 180+ signals)
    ↓
tools/arxml/dbc2arxml.py
    │  (~500 lines — creates AUTOSAR SYSTEM/SIGNAL/I-PDU elements,
    │   assigns E2E DataIDs, outputs ARXML)
    ↓
arxml/TaktflowSystem.arxml  +  model/ecu_sidecar.yaml
    │  (AUTOSAR R22-11 model + ECU-specific metadata: ASIL, port bindings)
    ↓
python -m tools.arxmlgen
    │  (Jinja2 templates — one pass per ECU per module type)
    ↓
firmware/ecu/{cvc,fzc,rzc,bcm,icu,tcu}/cfg/
    ├── Com_Cfg_*.c/.h
    ├── Rte_Cfg_*.c/.h
    ├── Rte_Wrapper_*.c
    ├── CanIf_Cfg_*.c/.h
    ├── PduR_Cfg_*.c/.h
    ├── E2E_Cfg_*.c/.h
    ├── CanTp_Cfg_*.c/.h
    └── Dcm_Cfg_*.c/.h
```

**Additional codegen tools:**
- `tools/arxml/swc_extractor.py` — Extract SWC port definitions from ARXML for skeleton generation
- `tools/arxml/codegen.py` — Secondary codegen entry point / utility

### 5.2 Generator Architecture (`tools/arxmlgen/`)

Core modules (confirmed by docs at `docs/arxmlgen/`):
- `reader.py` — Parse ARXML + sidecar → in-memory `SignalModel`, `PduModel`, `EcuModel`
- `model.py` — Domain model (dataclasses)
- `engine.py` — Jinja2 template rendering driver
- `config.py` — Generator configuration (from `project.yaml`)
- `generators/com_cfg.py` → `Com_Cfg_*.c/.h`
- `generators/rte_cfg.py` → `Rte_Cfg_*.c/.h`
- `generators/rte_wrapper.py` → `Rte_Wrapper_*.c`
- `generators/canif_cfg.py` → `CanIf_Cfg_*.c/.h`
- `generators/pdur_cfg.py` → `PduR_Cfg_*.c/.h`
- `generators/e2e_cfg.py` → `E2E_Cfg_*.c/.h` (unique DataID per message, P01 parameters)
- `generators/cantp_cfg.py` → `CanTp_Cfg_*.c/.h`
- `generators/swc_skeleton.py` → `Swc_<Module>.c` (first-time skeleton only)
- `generators/cfg_header.py` → common defines + metadata comment block

Reference: `docs/arxmlgen/architecture.md`, `docs/arxmlgen/api-reference.md`, `docs/arxmlgen/arxml-coverage.md`, `docs/arxmlgen/test-report.md`

### 5.3 Generator Test Suite

Located at `tools/arxmlgen/tests/`:
- `test_com_generator.py` — Signal table generation correctness
- `test_rte_generator.py` — RTE wrapper consistency
- `test_canif_generator.py` — CAN ID routing validity
- `test_pdur_generator.py` — PDU fan-out paths
- `test_e2e_generator.py` — DataID uniqueness, CRC/counter parameters
- `test_model_integrity.py` — ARXML → model conversion correctness
- `test_quality.py` — Lint checks on generated code output
- `test_rte_wrapper_generator.py` — Type-safety validation
- `test_cfg_header_generator.py` — Header macro consistency

CI gate: all generator tests must pass before generated configs are committed.

### 5.4 Update Procedure

```bash
# 1. Edit DBC (never generated C directly)
# gateway/taktflow_vehicle.dbc

# 2. Regenerate ARXML
python tools/arxml/dbc2arxml.py gateway/taktflow_vehicle.dbc arxml/

# 3. Regenerate C configs
python -m tools.arxmlgen

# 4. Commit in separate commit
# git commit -m "chore(codegen): regenerate configs from DBC vX.Y"
```

---

## 6. Test Framework & Verification

### 6.1 Unit Tests (`test/unit/bsw/`)

Confirmed test files (Unity framework, GCC host build):

**Handwritten unit tests:**
`test_Can_asild.c`, `test_Can_Posix_asild.c`, `test_CanIf_asild.c`, `test_CanSM_asild.c`, `test_CanTp_asild.c`, `test_Com_asild.c`, `test_Com_TxAutoPull_asild.c`, `test_Dcm_qm.c`, `test_Dem_asilb.c`, `test_Det_asild.c`, `test_Det_Callout_Sil_asild.c`, `test_Dio_asild.c`, `test_E2E_asild.c`, `test_E2E_Sm_asild.c`, `test_Gpt_asild.c`, `test_IoHwAb_asild.c`, `test_IoHwAb_Posix_asild.c`, `test_IoHwAb_Hil_asild.c`, `test_PduR_asild.c`, `test_Pwm_asild.c`, `test_Rte_asild.c`, `test_SchM_asild.c`, `test_Spi_asild.c`, `test_Uart_qm.c`, `test_WdgM_asild.c`, `test_Adc_asila.c`, `stubs_com_test.c`

**Generated test files** (from arxmlgen test generator):
`test_BswM_asild.c`, `test_CanSM_full_generated.c`, `test_Com_signals_generated.c`, `test_Com_negative_generated.c`, `test_Dem_generated.c`, `test_Det_generated.c`, `test_E2E_messages_generated.c`, `test_E2E_SM_full_generated.c`, `test_E2E_negative_generated.c`, `test_FiM_generated.c`, `test_Rte_generated.c`, `test_VSM_transitions_generated.c`, `test_WdgM_generated.c`, `test_XCP_security_generated.c`

**BSW dataflow integration:**
`test/integration/bsw/test_bsw_dataflow.c`

### 6.2 Framework Integration Tests (`test/framework/`)

11 confirmed suites — all files exist in both `test/framework/` and `test/framework/src/`:

| File | ASIL | What it Verifies |
|------|------|-----------------|
| `test_int_can_busoff_asild.c` | D | CanSM bus-off detection, `Can_Hw_IsBusOff()`, recovery sequence |
| `test_int_e2e_chain_asild.c` | D | E2E protect → Com TX → CanIf → loopback → E2E check round-trip |
| `test_int_e2e_faults_asild.c` | D | Corrupted CRC / stale counter / missing message → DEM event |
| `test_int_heartbeat_loss_asild.c` | D | SC detects missing CVC heartbeat within FTTI budget → safe state |
| `test_int_safe_state_asild.c` | D | All fault paths → torque 0%, steer neutral, warning latched |
| `test_int_wdgm_supervision_asild.c` | D | WdgM alive counter, deadline, program flow supervision |
| `test_int_overcurrent_chain_asild.c` | D | Motor overcurrent → DEM event → actuator shutdown → BswM |
| `test_int_can_matrix_asilc.c` | C | All 32 CAN messages routed per DBC matrix (CanIf → PduR) |
| `test_int_signal_routing_asilc.c` | C | Signal flow: SWC → RTE → Com → CAN boundary |
| `test_int_dem_to_dcm_asilc.c` | C | DEM fault events readable via UDS (Dcm) |
| `test_int_bswm_mode_asilc.c` | C | BSW mode transitions (startup → run → shutdown → safe) |

### 6.3 xIL Verification Pyramid

```
Level 7: Manual / Field Test
Level 6: HIL  — test/hil/         (real hardware, USB-CAN adapter, Raspberry Pi bench)
Level 5: PIL  — test/pil/         (real MCU firmware + simulated plant via CAN/UART)
Level 4: SIL  — test/sil/         (Docker vECUs + vcan0 + plant_sim_py)
Level 3: MIL  — test/mil/         (Python plant model, Python BSW model)
Level 2: Integration Tests         (test/framework/ — real BSW modules, vcan0)
Level 1: Unit Tests                (test/unit/bsw/ — Unity, mocked MCAL)
```

**Development discipline (`.claude/rules/development-discipline.md`):** Each layer must reach 100% before starting the next. Never skip layers.

---

## 7. SIL / HIL Infrastructure

### 7.1 SIL (Software-in-the-Loop)

**Docker compose:** `docker/docker-compose.sil.yml` — 7 containers (one per ECU) + plant simulator, all connected via `vcan0` SocketCAN bridge.

**Build target per container:**
```bash
make -f firmware/platform/posix/Makefile.posix build ECU=<ecu>
```

**Scenario scripts (`test/sil/scenarios/`):**
- `scenario_ignition_on.py` — Ignition → heartbeat sync across all ECUs
- `scenario_drive_creep.py` — Pedal input → motor torque → vehicle acceleration
- `scenario_brake_emergency.py` — Brake request → FZC servo → safe-state check
- `scenario_heartbeat_loss_recovery.py` — CVC timeout → SC cutoff → recovery

**Clean environment before SIL:**
```bash
sudo killall -9 cvc_posix fzc_posix rzc_posix 2>/dev/null
```
(See discipline rule §4 — zombie processes inflate CAN frame rates.)

### 7.2 HIL (Hardware-in-the-Loop)

**Infrastructure:** Raspberry Pi bench with USB-CAN adapter, real STM32G4 boards, physical CAN bus at 500 kbit/s.

**Scenario scripts (`test/hil/`):**
- HIL-specific equivalents of SIL scenarios with real timing verification
- `hil_heartbeat_timeout.py` — Validates SC physical relay cutoff within FTTI

**Setup guide:** `docs/guides/usb-can-adapter-setup.md`

**CI nightly:** `hil-preflight-nightly` (3 AM UTC) → `hil-nightly` (4 AM UTC).

### 7.3 Plant Simulator (`gateway/plant_sim_py/`)

Modular Python plant with per-subsystem models:
- `motor_model.py` — Electric motor torque/speed dynamics
- `battery_model.py` — Battery SoC, voltage, current
- `brake_model.py` — Hydraulic/regen braking deceleration
- `steering_model.py` — Steering servo position response
- `lidar_model.py` — Proximity distance simulation
- `simulator.py` — Integration loop, publishes signals to vcan0

---

## 8. Gateway & Edge Services

### 8.1 Confirmed Gateway Modules

| Module | Key Files | Technology | Purpose |
|--------|-----------|------------|---------|
| `can_gateway` | `main.py`, `decoder.py`, `mqtt_publisher.py` | Python + python-can + paho-mqtt | CAN ↔ MQTT bridge, per-signal topic publish |
| `cloud_connector` | `main.py`, `bridge.py`, `buffer.py`, `health.py` | Python + AWS IoT SDK | TLS MQTT to AWS IoT Core, buffered upload, health heartbeat |
| `fault_inject` | `app.py`, `scenarios.py`, `test_runner.py`, harnesses | Python | YAML-driven fault scenario executor for SIL/HIL |
| `ml_inference` | `detector.py`, `train_anomaly.py` | Python + PyTorch | Anomaly detection on CAN signal streams |
| `plant_sim_py` | see §7.3 | Python | Vehicle dynamics simulation |
| `sap_qm_mock` | `app.py`, `odata_router.py`, `dtc_connector.py`, `defect_catalog.py` | Python (FastAPI) | SAP Quality Management OData API mock — DTC → SAP defect mapping |
| `godot_bridge` | `bridge.py` | Python | 3D visualization bridge to Godot engine via MQTT/WebSocket |
| `ws_bridge` | `bridge.py` | Python | WebSocket ↔ CAN bridge |
| `lib` | `dbc_encoder.py` | Python | Shared DBC signal encode/decode library |

### 8.2 DBC Encoder Library

`gateway/lib/dbc_encoder.py` is the shared signal codec used by all gateway services. Tested by `gateway/tests/test_dbc_encoder.py`. Plant sim tested by `gateway/tests/test_plant_sim.py`.

### 8.3 Cloud Connector Architecture

`cloud_connector/bridge.py` — CAN → AWS IoT Core (TLS MQTT)
`cloud_connector/buffer.py` — Ring buffer for offline resilience
`cloud_connector/health.py` — Connectivity health monitor
Tests: `tests/test_bridge.py`, `tests/test_buffer.py`, `tests/test_health.py`

---

## 9. Safety Architecture

### 9.1 ISO 26262 Coverage

Documentation tree under `docs/safety/` and `docs/aspice/`:

| Part | Key Artifacts | Status |
|------|--------------|--------|
| Part 2 (Safety Management) | `safety-plan.md`, `safety-case.md` | draft |
| Part 3 (Concept) | `item-definition.md`, `hara.md`, `safety-goals.md`, `functional-safety-concept.md` | draft/active |
| Part 4 (System) | `safety-validation-report.md` | planned |
| Part 5 (HW) | `hw-requirements.md`, `hw-design.md`, `hardware-metrics.md` | draft |
| Part 6 (SW) | `sw-architecture.md`, `bsw-architecture.md`, `SWR-*.md` (8 files) | draft/active |
| Part 9 (Analysis) | `fmea.md`, `dfa.md`, `asil-decomposition.md`, `misra-deviation-register.md`, `heartbeat-ftti-budget.md` | active |

ASIL D scope: CVC, FZC, SC — unintended acceleration, steering loss, heartbeat supervision.
ASIL C scope: RZC — motor overcurrent, temperature runaway.
ASIL B scope: TCU.
QM: BCM, ICU.

### 9.2 HARA → Code Traceability Chain

```
Hazard (HARA)
  → Safety Goal (ASIL D/C/B assignment)
    → Technical Safety Req (TSR-XXX in docs/aspice/system/system-requirements.md)
      → SW Safety Req (SWR-<ECU>-YYY in docs/aspice/software/sw-requirements/SWR-*.md)
        → BSW module / SWC implementation
          → Unit test (test/unit/bsw/test_*.c)
            → Integration test (test/framework/test_int_*.c)
              → SIL scenario (test/sil/scenarios/*.py)
                → HIL scenario (test/hil/scenarios/*.py)
                  → Test result (PASS/FAIL + metric)
```

Traceability matrix: `docs/aspice/traceability/traceability-matrix.md`
CI enforcement: `.github/workflows/traceability.yml` validates links on every push.

### 9.3 Fail-Closed Fault Reactions

| Fault Trigger | Detection | BSW Path | Safe Reaction |
|---------------|-----------|----------|---------------|
| CVC heartbeat timeout | SC `sc_heartbeat.c` | Bare-metal timer | Relay cutoff, torque 0%, steer neutral |
| E2E CRC mismatch | `E2E_Check()` → Com | Com → DEM event | Signal discarded, timeout counter |
| Motor overcurrent | RZC `Swc_CurrentMonitor.c` | DEM → BswM | Actuator shutdown, CVC notification |
| Pedal channel discrepancy | CVC `Swc_Pedal.c` | Det → DEM | Pedal position forced to 0 (release) |
| WdgM deadline miss | `WdgM_MainFunction()` | BswM mode request | SAFE_STOP mode transition |
| Bus-off condition | `CanSM_MainFunction()` | CanSM recovery | Bus-off recovery sequence (controller reset) |
| FiM inhibition | `FiM_GetFunctionPermission()` | DEM-driven | Unsafe function blocked at SWC entry |

### 9.4 MISRA C:2012 Compliance

Tool: cppcheck 2.13+ with MISRA addon
Config: `tools/misra/misra.json`
Suppressions: `tools/misra/suppressions.txt`
Deviations register: `docs/safety/analysis/misra-deviation-register.md`
CI gate: **0 violations allowed** — `--error-exitcode=1` blocks merge on any violation.
Tool qualification: `docs/aspice/verification/tool-qualification/tool-qual-cppcheck.md`

### 9.5 ASPICE Process Coverage

| Process Area | Document(s) | Status |
|-------------|-------------|--------|
| SYS.1 | `stakeholder-requirements.md` | draft |
| SYS.2 | `system-requirements.md`, `system-architecture.md`, `interface-control-doc.md`, `can-message-matrix.md` | draft |
| SWE.1 | `SWR-CVC/FZC/RZC/SC/BCM/ICU/TCU/BSW.md` (8 files) | planned |
| SWE.2 | `sw-architecture.md`, `bsw-architecture.md`, `vecu-architecture.md` | draft |
| SWE.3 | Firmware source (implementation) | active |
| SWE.4 | `unit-test-plan.md`, `unit-test-report.md` | planned |
| SWE.5 | `integration-test-plan.md`, `integration-test-report.md`, `sil-report.md`, `pil-report.md`, `hil-report.md` | planned |
| SWE.6 | `sw-verification-plan.md`, `release-notes.md` | planned |
| SYS.4/5 | `system-integration-report.md`, `system-verification-report.md` | planned |
| MAN.3 | `execution-roadmap.md`, `weekly-status-*.md`, `risk-register.md`, `issue-log.md`, `decision-log.md` | active |
| SUP.1 | `qa-plan.md` | draft |
| SUP.8 | `cm-strategy.md`, baselines, change-requests | draft |

Reference: `docs/INDEX.md` (master document register), `docs/aspice/plans/aspice-plans-overview.md`

---

## 10. CI/CD Gates

### 10.1 Pipeline Summary

| Workflow | Trigger | Blocking? | Purpose |
|----------|---------|-----------|---------|
| Lint & Build | Push / PR | Yes | Clang-format, shellcheck, basic compile |
| Unit & Integration Tests | Push / PR | Yes | Unity unit tests + framework integration suites |
| MISRA Analysis | Push / PR | Yes | cppcheck MISRA C:2012 (0 violations) |
| Traceability Check | Push / PR | Yes | Bidirectional req ↔ code ↔ test link validation |
| Full SIL Nightly | Nightly 2AM UTC | Informational | 7-ECU Docker SIL, all scenarios |
| HIL Preflight Nightly | Nightly 3AM UTC | Informational | Convergence checks before full HIL run |
| HIL Nightly | Nightly 4AM UTC | Informational | Raspberry Pi bench, physical CAN |

### 10.2 Qualified Tools

| Tool | Qualification Doc |
|------|------------------|
| GCC | `tool-qual-gcc.md` |
| Unity (test framework) | `tool-qual-unity.md` |
| cppcheck (MISRA) | `tool-qual-cppcheck.md` |
| gcov (coverage) | `tool-qual-gcov.md` |

### 10.3 Commit Discipline

Per `.claude/rules/development-discipline.md`:
- Generated configs committed in a **separate** `chore(codegen):` commit
- Debug `fprintf` traces removed before merge to `main`
- Volatile debug counters (`g_dbg_*`) used for permanent instrumentation
- HITL-LOCK blocks never edited

---

## 11. Key Findings & Observations

### 11.1 Strengths

1. **DBC-first codegen** eliminates hand-edit drift between CAN matrix and BSW config. 32 messages and 180+ signals all flow from a single `.dbc` file.

2. **Real MCAL headers confirmed** for all 7 MCAL drivers (Can, Adc, Dio, Gpt, Pwm, Spi, Uart) across 3 physical platforms (STM32G4, TMS570) and POSIX shim — genuine platform abstraction, not stubs.

3. **SC bare-metal isolation** is correct: TMS570 Safety Controller has no AUTOSAR BSW stack. Its 12 source files (`sc_*.c`) implement minimal lockstep supervision, keeping the safety-critical path auditable.

4. **IoHwAb injection variants** (`IoHwAb_Inject.h`, `IoHwAb_Hil.h`, `IoHwAb_Posix.h`) allow the same SWC code to run in SIL, HIL, and fault-injection mode without `#ifdef` in application code.

5. **11 ASIL-tagged integration test suites** in `test/framework/` cover every key safety chain (E2E, heartbeat, bus-off, overcurrent, safe-state, watchdog) — they are the highest-confidence automated verification layer.

6. **Generated test files** (14 `*_generated.c` files) keep BSW test coverage in sync with the codegen model automatically.

### 11.2 Gaps / Items to Verify

1. **SC port coverage**: `firmware/platform/qnx/` is confirmed (stub). `tms570/src/` contains only `sc_hw_tms570.c`, `Os_Port_Tms570.c/.S`, and `can_loopback_test.c`. Verify TMS570 build is green in CI before claiming SC layer done (see `platform-port-checklist.md`).

2. **CAN driver for STM32**: `Can_Hw_STM32.c` exists but the `Makefile.stm32` `Can` MCAL header is `firmware/bsw/mcal/Can/include/Can.h` — confirm the STM32 FDCAN driver satisfies the full platform port checklist (TX ISR, async CB, ring buffer, no sync `canFrameSent()`).

3. **Duplicate framework test files**: `test/framework/*.c` and `test/framework/src/*.c` appear to be duplicates (same 11 filenames). Confirm which location the Makefile builds from to avoid stale tests.

4. **SIL scenario scripts**: `test/sil/scenarios/` Python files confirmed to exist by the exploration but not individually listed here — verify all 4+ scenario files are present and passing nightly.

5. **STM32L5 / ThreadX experiment**: `firmware/platform/stm32l5/` and ThreadX stubs in `stm32/src/` (from recent commits) are experimental. Ensure they are excluded from production CI paths and gated behind a feature flag or separate Makefile target.

6. **ASPICE gap**: 7 of 10 planned SWE/SYS documents are still `planned` status. Gap-fill plan exists (`docs/plans/Taktflow_Gap_Filling_Plan_v1.docx`) — track against `execution-roadmap.md` milestones.

7. **POSIX CAN for BCM/ICU/TCU**: These virtual ECUs use `Can_Posix.c` but their `platform_posix/` cfg dirs should be verified to exist alongside the physical ECU cfg dirs.

---

*Generated by automated structural analysis. For human review, cross-reference against `docs/INDEX.md` and run `make test` to confirm current test status.*

---

## 12. AI Integration Planning

**Purpose:** Map the architecture for where AI/ML capabilities can and cannot be integrated, given the safety constraints of an ISO 26262 ASIL D zonal vehicle platform.

### 12.1 Existing AI: Anomaly Detection Gateway (`gateway/ml_inference/`)

The codebase already has a deployed ML inference layer, running entirely outside the safety boundary:

| Attribute | Detail |
|-----------|--------|
| Model | Isolation Forest (scikit-learn, joblib) |
| Input | 4 MQTT topics: Motor_Current_Phase_mA, Motor_Temperature, Motor_Status_MotorSpeed_RPM, Battery_Status_BatteryVoltage_mV |
| Window | 10 samples @ 10 Hz = 1-second rolling buffer |
| Features | `[current_mean, current_std, motor_temp, rpm, battery_voltage]` |
| Output | Anomaly score 0–1, published to `taktflow/anomaly/score` |
| DTC trigger | Score > 0.7 → soft DTC `0xE601` on `taktflow/alerts/dtc/0xE601` |
| Reset | `taktflow/command/reset` clears buffers, score → 0.0 |
| Training | `train_anomaly.py` — synthetic normal/fault distribution, auto-train fallback |
| Deployment | Docker container (`gateway/ml_inference/Dockerfile`), isolated from ECU firmware |

**Safety constraint:** This is a QM-level advisory layer. It publishes soft DTC alerts over MQTT — it does NOT directly command actuators, modify CAN frames, or interact with the ASIL D safety path. This is correct: AI output feeds human/operator dashboards and logging, not the control loop.

### 12.2 Safety Boundary for AI Integration

The fundamental constraint comes from ISO 26262 and the HARA:

| Zone | ASIL | AI Permitted? | Rationale |
|------|------|---------------|-----------|
| SC firmware (TMS570) | D | **Never** | Lockstep bare-metal. No OS, no dynamic allocation, no non-deterministic behavior. |
| CVC/FZC/RZC firmware (STM32G4) | D/C | **Never in control loop** | ASIL D FTTIs (50ms) require deterministic cyclic functions. AI inference latency is uncharacterized and untestable to ASIL D. |
| SWC logic (`Swc_*.c`) | D/C | **No** | SWCs are MISRA C:2012, statically analyzable. AI would violate MISRA rules (dynamic allocation, floating point, etc.) |
| BSW stack | D/C | **No** | Same as SWC logic. BSW modules are deterministic by design. |
| POSIX/Docker SIL ECUs | QM | **Read-only advisory** | AI can observe signals; cannot command actuators or modify BSW state. |
| Gateway services | QM | **Yes, with isolation** | `gateway/ml_inference/` — correct pattern. CAN telemetry in, MQTT advisory out. |
| Codegen tools | QM (tools) | **Yes** | AI can assist codegen, template generation, ARXML validation, DBC linting. |
| Test generation | QM (tools) | **Yes** | AI can generate unit/integration test cases from signal definitions. |
| Documentation | — | **Yes** | AI can generate/maintain safety docs, HARA cross-reference, traceability matrices. |

### 12.3 Integration Point Catalog

#### Zone A — Off-Limits (Safety-Critical Firmware)

These areas must never contain AI inference, LLM output, or ML model calls:

- `firmware/bsw/` — BSW stack. Any change here requires ISO 26262 Part 6 SW verification including MC/DC coverage.
- `firmware/ecu/{cvc,fzc,rzc,sc}/src/` — SWC and SC source. ASIL D/C implementation.
- `firmware/platform/` — MCAL implementations. Hardware register access, ISR handlers.
- `firmware/ecu/*/cfg/` — Generated configs. Changing these requires DBC change + codegen rerun, not hand-edit.

**Why not ASIL D AI?** The HARA identifies 4 ASIL D hazards requiring 50ms FTTI (HE-001 unintended acceleration, HE-004 loss of steering, HE-005 loss of braking, HE-017 unintended motion from rest). ML inference cannot be proven to complete within 50ms with 100% reliability, and no current ML model can satisfy ASIL D systematic capability (SC) requirements (ISO 26262-6 Table 9).

#### Zone B — Gateway / Edge (Safe Integration Points)

These are the correct homes for AI integration:

| Integration | Location | Pattern | Feasibility |
|-------------|----------|---------|-------------|
| **Motor anomaly detection** (existing) | `gateway/ml_inference/` | Subscribe MQTT → infer → publish score | ✅ Deployed |
| **Steering + brake anomaly extension** | `gateway/ml_inference/detector.py` | Add steering_angle + brake_force to feature vector | ✅ Extend existing |
| **CAN matrix health monitor** | `gateway/can_gateway/` | Statistical deviation from expected signal ranges (from DBC) | ✅ New service |
| **Predictive maintenance alerts** | `gateway/cloud_connector/` | Time-series trend model on motor temp / current over sessions | ✅ Publishable to Grafana |
| **Lidar false-negative filter** | `gateway/plant_sim_py/lidar_model.py` | ML-based obstacle confidence in simulation | ✅ SIL only, not HIL |
| **Fault injection planning** | `gateway/fault_inject/scenarios.py` | LLM-assisted scenario generation from HARA hazardous events | ✅ Offline tool |
| **Log analysis / triage** | CI or post-session | Classify DTC patterns, suggest root cause | ✅ Offline only |

#### Zone C — Development Tools (AI-Assisted Workflow)

AI can significantly accelerate these without any safety risk:

| Tool Area | AI Opportunity | Files Involved |
|-----------|---------------|----------------|
| **DBC validation** | LLM checks signal naming conventions, E2E DataID uniqueness, missing CycleTime | `tools/pipeline/step1_validate_dbc.py` |
| **ARXML generation** | AI-assisted DBC→ARXML mapping for new messages | `tools/arxml/dbc2arxml.py` |
| **Test case generation** | Generate `test_Com_signals_generated.c` style tests from signal table | `tools/arxmlgen/generators/` |
| **HARA cross-reference** | AI validates that new DBC signals are covered by at least one safety goal | `docs/safety/concept/hara.md` |
| **Traceability matrix** | AI checks HARA→SG→TSR→SWR→test link completeness | `tools/trace/gen_traceability_matrix.py` |
| **Lessons learned classification** | AI tags new incidents against existing lessons learned patterns | `docs/lessons-learned/` |
| **HIL scenario authoring** | LLM generates YAML HIL scenarios from HARA hazardous event descriptions | `test/hil/scenarios/*.yaml` |

### 12.4 AI-Assisted Codegen Pipeline Extension

The most impactful near-term AI integration is inside the existing codegen pipeline (`tools/arxmlgen/`), which is already tool-level QM:

```
DBC (modified)
    ↓
[AI step — optional, QM tool]
  • Validate new signals against HARA HE coverage
  • Suggest E2E DataID if missing
  • Check naming convention per docs/standards/naming-conventions.md
  • Generate initial SWR ticket outline for new signal
    ↓
tools/arxml/dbc2arxml.py  [existing]
    ↓
tools/arxmlgen/  [existing generators]
    ↓
firmware/ecu/*/cfg/  [generated C configs]
    ↓
[AI step — optional, QM tool]
  • Generate initial unit test stubs for new signals in test_Com_signals_generated.c
  • Verify generated E2E DataID against HARA signal path audit
```

**Implementation note:** Any AI step must be implemented as a separate pre/post-processing script — never modify the Jinja2 templates or generator Python directly with AI-generated logic. The generators are part of the qualified toolchain; AI augmentation lives outside them.

### 12.5 ML Anomaly Detector — Recommended Enhancements

Building on the existing `gateway/ml_inference/` module:

| Enhancement | Approach | Priority |
|-------------|----------|---------|
| Add steering + brake signals to feature vector | Extend `SensorBuffers` with 2 more deques; add MQTT subscriptions for `FZC_Steering_Angle` and `FZC_Brake_Force` | High |
| Session-baseline normalization | Before each drive session, capture 5s idle baseline; re-fit scaler per session | Medium |
| Online retraining | Accumulate labeled normal sessions; retrain IsolationForest weekly via CI scheduled job | Medium |
| FTTI-aware alert severity | Map anomaly score threshold to SG FTTI: score 0.5 = advisory, 0.8 = operator alert, never → actuator command | High |
| Explainability output | Include SHAP feature contribution in MQTT payload for operator diagnostics | Low |
| Integration with DEM | Bridge MQTT DTC alerts to gateway `diagnostics/` service → surface in ICU display | Medium |

### 12.6 Constraints Summary for AI Integrators

1. **AI output is QM advisory only.** AI inference results must never directly modify actuator commands, BSW state, or safety-critical CAN signals. The path is always: AI → MQTT/log → human operator or QM display.

2. **Do not add AI to the ASIL D FTTI path.** The 50ms budget for SG-001/SG-004 (acceleration + braking) leaves no margin for inference latency. The SC (TMS570) must reach safe state independently without any ML input.

3. **Generated config files are off-limits for AI editing.** `firmware/ecu/*/cfg/` files carry `/* GENERATED -- DO NOT EDIT */` headers. If AI-assisted codegen is needed, fix the generator (`tools/arxmlgen/`) or the DBC, then run `python -m tools.arxmlgen`.

4. **HITL-LOCK blocks must not be touched.** Safety documentation in `docs/safety/` contains `<!-- HITL-LOCK START:id -->` blocks that are human-reviewer-owned. AI must not edit, reformat, or move content inside these markers.

5. **MISRA compliance required for firmware.** Any AI-assisted code generation targeting `firmware/` must pass `tools/misra/` cppcheck scan. AI-generated C tends to use dynamic allocation, variadic macros, and implicit casts — all flagged by MISRA C:2012.

6. **CAN signal authorization.** New MQTT topics carrying AI output must not reuse existing DBC signal names. Use prefix `taktflow/anomaly/` or `taktflow/ai/` namespaces to prevent confusion with real vehicle signals.

7. **Plant simulator is the right AI sandbox.** `gateway/plant_sim_py/` is the correct place to experiment with AI-enhanced simulation (e.g., learned plant dynamics, lidar noise model). Changes there have zero impact on firmware or safety certification artifacts.

---

*AI integration section added 2026-03-27. For questions on safety boundaries, refer to `docs/safety/concept/hara.md` (ASIL assignments) and `docs/safety/concept/safety-goals.md` (FTTI budgets).*
