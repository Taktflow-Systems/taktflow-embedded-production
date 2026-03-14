# Taktflow Embedded Production

The production firmware repository for the Taktflow zonal vehicle platform. This is where the full toolchain lives: DBC -> ARXML -> generated C configs, platform-specific MCAL builds, HIL infrastructure, and the complete ISO 26262 ASIL D development artifacts.

The companion public repo ([taktflow-embedded](https://github.com/nhuvaoanh123/taktflow-embedded)) contains the showcase version. This one is the working engineering environment.

---

## What this is

7-ECU drive-by-wire platform developed against ISO 26262 ASIL D. 4 physical ECUs on STM32G474RE and TMS570LC43x hardware. 3 simulated ECUs sharing the same BSW on Linux/Docker. All ECUs compile from the same C source -- platform differences are isolated to the MCAL layer.

The CAN network is the single source of truth: everything flows from gateway/taktflow.dbc through ARXML generation into the C configs. No hand-editing of generated files.

HIL bench is running: all 4 physical ECUs on real hardware, sensor values injected over CAN via the IoHwAb override layer, closed-loop plant-sim on Raspberry Pi at 100 Hz.

**Live SIL demo**: [sil.taktflow-systems.com](https://sil.taktflow-systems.com) -- all 7 ECUs in Docker with real-time telemetry, fault injection, and anomaly detection.

---

## Numbers from source

| What | Count |
|------|-------|
| Unit tests | 1,783 -- 0 failures |
| C source files | 225 (firmware, excluding test files) |
| BSW modules | 37 (MCAL x8, ECUAL x3, Services x14, RTE x1, OS x11) |
| CAN messages | 35 (in gateway/taktflow.dbc) |
| Safety requirements | 482 (SG x8, FSR x26, TSR x52, SSR x82, HSR x25, SWR x201 + STK/SYS) |
| Traceability links | 482 requirements fully traced -- SG 100%, FSR 100%, TSR 100%, SSR 98%, SWR 93% |
| HIL test scenarios | 26 -- all passed on physical bench |
| Implementation plans | 51 in docs/plans/ |
| ASPICE documents | 67 in docs/aspice/ |

---

## Toolchain -- DBC-first codegen

All CAN configuration is generated. The pipeline:

```
gateway/taktflow.dbc          <- single source of truth for CAN signals
        |
        v tools/arxml/dbc2arxml.py
arxml/TaktflowSystem.arxml
        |
        v tools/arxmlgen/  (+ model/ecu_sidecar.yaml)
firmware/ecu/*/cfg/           <- GENERATED -- never hand-edit
  Com_Cfg.c / Com_Cfg.h
  CanIf_Cfg.c / CanIf_Cfg.h
  PduR_Cfg.c / PduR_Cfg.h
  E2E_Cfg.c / E2E_Cfg.h
  Rte_Cfg.c / Rte_Cfg.h
  runnable tables, signal tables
```

If a generated file is wrong: fix the DBC, ARXML, sidecar, or generator -- not the output.

---

## Platform abstraction

The same SWC source (firmware/ecu/*/src/) compiles for three targets:

| Target | Build | Usage |
|--------|-------|-------|
| STM32G474RE | make -f firmware/platform/stm32/Makefile.stm32 build | CVC, FZC, RZC physical ECUs |
| TMS570LC43x | make -f firmware/platform/tms570/Makefile.tms570 build | SC safety controller |
| POSIX (Linux) | make -f firmware/platform/posix/Makefile.posix build | SIL / Docker / unit tests |

#ifdef PLATFORM_POSIX appears only in the MCAL layer. SWC code has no platform guards.

---

## HIL setup

Physical bench with no peripherals connected. Sensor injection via CAN:

```
Raspberry Pi (plant-sim @ 100 Hz)
        | CAN 0x600 / 0x601 - virtual sensor frames
        v
CAN bus (500 kbps)
        |
        +-- CVC STM32G474RE  --- IoHwAb_Hil.c overrides pedal/encoder reads
        +-- FZC STM32G474RE  --- IoHwAb_Hil.c overrides steering/brake/lidar reads
        +-- RZC STM32G474RE  --- IoHwAb_Hil.c overrides motor current/temp reads
        +-- SC  TMS570LC43x  --- monitors heartbeats, controls kill relay
```

`PLATFORM_HIL=1` compile flag enables the IoHwAb override layer. The same binary otherwise runs identically to SIL. 26 HIL scenarios validated on this bench -- all passed.

WS5 (SC torque-current LUT calibration) is BLOCKED -- electronic load procured, not yet connected.

---

## Test results -- SIL

19 automated scenarios on vcan0 in Docker. All pass in CI (sil-nightly.yml). Scenario files in test/sil/scenarios/, run by test/sil/verdict_checker.py.

| ID | Scenario | Requirements |
|----|----------|-------------|
| SIL-001 | Normal system startup -- INIT -> RUN | TSR-001, SSR-CVC-001 |
| SIL-002 | Pedal ramp -- torque tracks pedal input | TSR-003, SSR-CVC-003 |
| SIL-003 | Emergency stop -- SAFE_STOP on CVC command | TSR-005, SSR-CVC-007 |
| SIL-004 | CAN busoff recovery -- FZC busoff -> reconnect | TSR-020, SSR-FZC-016 |
| SIL-005 | Watchdog timeout -- CVC WdgM miss -> DEM event | TSR-008, SSR-CVC-012 |
| SIL-006 | Battery undervoltage -- DEGRADED/LIMP mode | TSR-046, SSR-RZC-001 |
| SIL-007 | Motor overcurrent -- DTC 0xE301, cutoff | TSR-007, SSR-RZC-004 |
| SIL-008 | Sensor disagreement -- FZC redundant check fail | TSR-012, SSR-FZC-003 |
| SIL-009 | E2E CRC corruption -- receiver rejects frame | TSR-022, SSR-CVC-008 |
| SIL-010 | Motor overtemperature -- derating on 0x301 | TSR-009, SSR-RZC-006 |
| SIL-011 | Steering sensor failure -- FZC SAFE_STOP | TSR-011, SSR-FZC-002 |
| SIL-012 | Multiple simultaneous faults -- priority handling | TSR-050, SSR-CVC-015 |
| SIL-013 | Recovery from SAFE_STOP -- re-init sequence | TSR-005, SSR-CVC-016 |
| SIL-014 | 10-minute sustained load -- no memory leak, no drift | SWR-CVC-018 |
| SIL-015 | Power cycle -- NvM persistence across reset | SWR-CVC-020 |
| SIL-016 | Gateway telemetry -- MQTT publish to AWS IoT | SWR-GW-001 |
| SIL-017 | ML anomaly detection -- Isolation Forest flags fault | SWR-GW-005 |
| SIL-018 | SAP QM handshake -- quality message exchange | SWR-GW-008 |

Each scenario file carries: verifies: (requirement IDs), aspice: SWE.6, iso_reference:, and setup/steps/verdicts/teardown blocks.

---

## Test results -- HIL

26 scenarios on can0 with physical ECUs. All passed. Scenario files in test/hil/scenarios/, run by test/hil/hil_runner.py on the Pi.

**Category 1 -- Closed-loop plant-sim dynamics (7 tests, ASIL B-D)**

| ID | What is verified |
|----|-----------------|
| HIL-001 | Motor current on 0x301 reacts to torque command on 0x101 |
| HIL-002 | Motor temperature rises under sustained load |
| HIL-003 | Battery voltage sags under motor load |
| HIL-004 | Steering angle tracks command with rate limit |
| HIL-005 | Brake position tracks command with rate limit |
| HIL-006 | Lidar distance responds to simulated obstacles |
| HIL-007 | Vehicle state machine: INIT -> RUN -> DEGRADED -> SAFE_STOP |

**Category 2 -- Heartbeat & liveness (5 tests, ASIL QM-C)**

| ID | What is verified |
|----|-----------------|
| HIL-010 | CVC heartbeat 0x010 at 50 ms +/-10% |
| HIL-011 | FZC heartbeat 0x011 at 50 ms +/-10% |
| HIL-012 | RZC heartbeat 0x012 at 50 ms +/-10% |
| HIL-013 | ICU reads ECU health from heartbeats, reflects on 0x360 |
| HIL-014 | BCM receives 0x100, timeout -> SHUTDOWN |

**Category 3 -- Simulated ECU behavior (6 tests, ASIL QM)**

| ID | What is verified |
|----|-----------------|
| HIL-020 | BCM headlight auto-on in RUN, auto-off in SHUTDOWN |
| HIL-021 | BCM turn indicator at 1.5 Hz, hazard overrides |
| HIL-022 | BCM body status 0x360 at 100 ms with alive counter |
| HIL-023 | ICU gauge reception: speed, torque, temp, battery |
| HIL-024 | TCU UDS session control + DTC read via 0x7DF |
| HIL-025 | TCU UDS Read DID: vehicle speed, battery, state |

**Category 4 -- Fault injection via MQTT (5 tests, ASIL B-D)**

| ID | What is verified |
|----|-----------------|
| HIL-030 | Overcurrent injection -> RZC DTC 0xE301, motor cutoff |
| HIL-031 | Steering fault injection -> FZC DTC 0xD001, SAFE_STOP |
| HIL-032 | Brake fault injection -> FZC DTC 0xE202, SAFE_STOP |
| HIL-033 | Battery undervoltage injection -> DEGRADED/LIMP |
| HIL-034 | Motor overtemp injection -> DTC, derating on 0x301 |

**Category 5 -- E2E protection & CAN integrity (3 tests, ASIL D)**

| ID | What is verified |
|----|-----------------|
| HIL-040 | E2E CRC-8 + alive counter present on 0x100 |
| HIL-041 | Corrupted CAN frame rejected by receiver |
| HIL-042 | Alive counter freeze -> receiver falls back to safe default |

---

## BSW stack

```
Application SWCs  (Swc_Pedal, Swc_Motor, Swc_Steering, Swc_Brake, ...)
       | Rte_Read / Rte_Write
RTE   (signal buffers, port connections, runnable scheduling)
       |
Services  (Com, Dcm, Dem, WdgM, BswM, E2E, CanTp, Det, NvM, SchM, Sil_Time)
       |
ECU Abstraction  (CanIf, PduR, IoHwAb)
       |
MCAL  (Can, Spi, Adc, Pwm, Dio, Gpt, Uart)
       |
OS  (scheduler, tasks, alarms, resources, events -- bare-metal or POSIX shim)
       |
Hardware  (STM32G474RE / TMS570LC43x / POSIX)
```

### Safety Controller

Runs on TMS570 lockstep. No AUTOSAR, no RTOS -- flat C, minimal LOC, auditable. Monitors heartbeats from CVC/FZC/RZC, cross-checks torque vs. current plausibility, drives an energize-to-run kill relay, fed by external TPS3823 watchdog.

---

## Build & test

```bash
# POSIX build (SIL / unit tests)
make -f firmware/platform/posix/Makefile.posix build

# Run all 1,783 unit tests
make test

# STM32 firmware
make -f firmware/platform/stm32/Makefile.stm32 build

# TMS570 safety controller
make -f firmware/platform/tms570/Makefile.tms570 build

# Regenerate CAN configs from DBC
python -m tools.arxmlgen

# MISRA C:2012 analysis
make misra

# SIL environment (Docker)
docker compose -f docker/docker-compose.sil.yml up
```

---

## Structure

```
firmware/
  bsw/                   -- BSW stack (37 modules, shared across all ECUs)
    mcal/                 -- Can, Spi, Adc, Pwm, Dio, Gpt, Uart
    ecual/                -- CanIf, PduR, IoHwAb (+ HIL and POSIX variants)
    services/             -- Com, Dcm, Dem, WdgM, BswM, E2E, CanTp, Det, NvM, SchM
    rte/                  -- Runtime Environment
    os/                   -- OS abstraction (scheduler, tasks, alarms, events)
  ecu/{cvc,fzc,rzc,sc,bcm,icu,tcu}/
    src/                  -- SWC source
    include/              -- SWC headers
    cfg/                  -- GENERATED configs (do not edit)
    test/                 -- Unit tests (Unity)
  platform/{stm32,tms570,posix}/  -- Platform MCAL implementations + makefiles
  lib/vendor/             -- Wrapped third-party libraries
gateway/                  -- Raspberry Pi edge services
  plant_sim/              -- Closed-loop physics (motor, brake, steering, battery, lidar)
  can_gateway/            -- CAN <-> MQTT bridge
  cloud_connector/        -- AWS IoT Core
  fault_inject/           -- SIL fault injection REST API
  ml_inference/           -- Isolation Forest anomaly detection
  taktflow.dbc            -- CAN source of truth
arxml/                    -- Generated ARXML (from DBC)
model/                    -- ECU sidecar config (ecu_sidecar.yaml)
tools/
  arxml/                  -- dbc2arxml.py converter
  arxmlgen/               -- ARXML -> C config generator
  ci/                     -- CI scripts, HIL preflight audit
  misra/                  -- MISRA C:2012 checker config
  trace/                  -- Requirements traceability tools
docker/                   -- SIL/HIL container orchestration
test/
  sil/                    -- SIL scenarios + verdict_checker.py
  hil/                    -- HIL runner + 26 YAML scenarios + can_helpers.py
  mil/, pil/              -- MIL and PIL infrastructure
docs/
  safety/                 -- ISO 26262 (concept, analysis, requirements, validation)
  aspice/                 -- ASPICE 4.0 work products (67 documents)
  plans/                  -- 51 implementation plans
  lessons-learned/        -- Post-fix notes
hardware/                 -- BOM, pin mappings, wiring log, schematics
scripts/                  -- Deploy, debug, HIL launch utilities
```

---

## What this does not claim

- STM32G474RE Nucleo boards are development hardware, not automotive-grade silicon
- ISO 26262 documentation is in draft -- not externally audited or reviewed
- "ASIL D" is the process and architecture intent, not a certified product claim
- This is a portfolio and development environment, not a production vehicle system

---

## License

Copyright Taktflow Systems 2026. All rights reserved.
