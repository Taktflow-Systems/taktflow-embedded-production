---
document_id: GAP-HIL
title: "Gap Analysis: DIY HIL Bench vs Professional dSPACE / Vector Systems"
version: "1.0"
status: draft
date: 2026-03-01
related:
  - plans/master-plan.md
  - plans/plan-hardware-bringup.md
  - aspice/verification/xil/hil-report.md
---

## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.

# Gap Analysis: DIY HIL Bench vs Professional dSPACE / Vector Systems

## 1. Purpose

This document compares our $577 DIY bench setup against professional automotive HIL systems (dSPACE SCALEXIO at $150k-$500k, Vector VT System + CANoe at $80k-$250k). It identifies 8 capability gaps, evaluates their severity, and provides concrete, budget-aware closure strategies.

**Goal**: Determine which gaps are worth closing with hardware/software, which are best documented as known limitations, and define the implementation order to maximize portfolio credibility within the remaining ~$1,000 budget.

---

## 2. What Defines "HIL" (vs Just a Bench)

A bench becomes HIL when it satisfies these 5 pillars:

| # | Pillar | Definition |
|---|--------|------------|
| P1 | **Closed-loop plant model** | ECU commands an actuator, plant model computes physics response, feeds updated sensor data back to ECU — causal feedback loop |
| P2 | **Deterministic real-time execution** | Plant model step completes within a bounded time, every cycle, no OS jitter |
| P3 | **Electrical signal fidelity** | Test system produces voltages/currents identical to real sensors at the ECU's input pins |
| P4 | **Fault insertion** | Hardware relay matrix that can inject wire-level electrical faults (open, short-to-GND, short-to-VBAT) under software control, mid-scenario |
| P5 | **Automated test execution with traceability** | Test sequences linked to requirements, producing traceable pass/fail evidence |

Our SIL already satisfies P1 (plant-sim + Docker vECUs). The physical bench with real sensors/actuators partially satisfies P1 and P3. The gaps are in P2, P4, P5, and several supporting capabilities.

---

## 3. What We Have

### Hardware ($577 BOM, oscilloscope purchased separately)

| Category | Components | Portfolio Function |
|----------|------------|-------------------|
| **MCUs** | 3x STM32G474RE Nucleo-64, 1x TMS570LC43x LaunchPad | 4 physical ECUs (CVC, FZC, RZC, SC) |
| **CAN bus** | 3x TJA1051T/3, 1x SN65HVD230, 2x USB-CAN (Ecktron UCAN + Waveshare), 120R terminators, CM chokes, TVS diodes | CAN 2.0B at 500 kbps, E2E protected |
| **Sensors** | 3x AS5048A (SPI, 14-bit angle), 1x ACS723 (Hall current, +/-20A), 3x NTC 10k, 1x TFMini-S (UART lidar), 1x quadrature encoder | Real sensor inputs |
| **Actuators** | 1x BTS7960 H-bridge (43A), 1x 12V DC motor, 2x MG996R servo | Real actuator outputs |
| **Safety HW** | 5x TPS3823 external watchdog, 1x 30A kill relay + IRLZ44N driver, 1x E-stop button (NC) | Independent safety chain |
| **Power** | Lab PSU 0-30V/5A, LM2596 buck converters, fuses, Schottky protection | Multi-rail power distribution |
| **Gateway** | Raspberry Pi 4 (4GB), MicroSD, USB-C PSU | Edge gateway + plant model host |
| **Test equipment** | Rigol DS1054Z (50 MHz, 4 ch), 2x USB-CAN adapters | Signal measurement + CAN analysis |

### Software (Already Implemented)

| Component | Description |
|-----------|-------------|
| **Plant simulator** | Python, 5 physics models (motor, brake, steering, battery, lidar), feeds CAN frames |
| **Fault injection** | REST API (`fault-inject` Docker service), 19 SIL scenarios defined in YAML |
| **SIL platform** | 16-service Docker Compose, deployed live at `sil.taktflow-systems.com` |
| **ML inference** | Isolation Forest anomaly detection on motor telemetry |
| **DBC file** | `gateway/taktflow.dbc` — 31 CAN messages, signal-level definitions |
| **Verdict checker** | `verdict_checker.py` — automated pass/fail for SIL scenarios |
| **CI pipelines** | 4 workflows: unit tests (7-ECU matrix), MISRA (blocking), SIL nightly (19 scenarios), traceability |

---

## 4. What Professional Systems Provide

### dSPACE SCALEXIO ($150k-$500k per bench)

| Capability | Implementation |
|------------|---------------|
| Real-time processing | Intel Xeon 3.5 GHz on dedicated RTOS, sub-µs deterministic |
| I/O boards | HighFlex FPGA-based: analog in/out (0-10V, 16-bit), digital I/O, PWM, frequency channels |
| Sensor simulation | FPGA-driven resolver/encoder/angle emulation with exact electrical waveforms |
| Fault insertion | Integrated relay matrix (FIU) per I/O channel: open, short-GND, short-VBAT, cross-wire |
| Load simulation | Programmable actuator load boards matching motor impedance |
| Bus interfaces | DS1521: 8x CAN FD, 3x Automotive Ethernet, 2x FlexRay, 3x LIN |
| Plant models | ASM library: engine, drivetrain, vehicle dynamics, battery, ADAS sensor models |
| Test automation | AutomationDesk (ISO 26262 certified), step-based + CAPL scripting |
| Calibration | ControlDesk + XCP/A2L for live ECU variable access |
| Traceability | SYNECT: test cases linked to DOORS requirements, coverage reports |

### Vector VT System + CANoe ($80k-$250k per bench)

| Capability | Implementation |
|------------|---------------|
| Stimulation modules | VT2004: 4 analog channels with built-in fault relays per channel |
| Serial interfaces | VT2710: PSI5, SENT, SPI, I2C, UART with fault injection |
| Relay matrix | VT2820: 20 relay channels for signal switching and fault injection |
| Real-time engine | VT6011/VT6051: hard real-time I/O control via PCIe to host |
| Load simulation | VT6104: actuator load simulation (resistive, inductive, capacitive) |
| Rest-bus simulation | CANoe: full network simulation for CAN/LIN/FlexRay/Ethernet, CAPL scripting |
| Diagnostics | UDS (ISO 14229), OBD-II, KWP2000 protocol support |
| Test framework | vTESTstudio: structured test design with requirement traceability |
| Calibration | CANape: XCP/A2L live measurement and calibration |

---

## 5. Full Pillar Comparison

| Pillar | dSPACE/Vector | Our Setup | Status |
|--------|--------------|-----------|--------|
| **P1: Closed-loop plant model** | Simulink ASM on dedicated RTOS, sub-ms step | Python plant-sim, 5 models, over CAN | **Covered** (different architecture, same topology) |
| **P2: Deterministic RT** | Xeon RTOS / FPGA, sub-µs jitter | STM32 bare-metal (deterministic) + RPi Linux (1-5ms jitter) | **Gap** — RPi plant model not RT |
| **P3: Electrical signal fidelity** | 16-bit DAC, FPGA sensor emulation | Real sensors on real MCU pins | **Different approach** — real > simulated for this use case |
| **P4: Fault insertion** | Hardware FIU relay matrix, software-controlled | Software fault injection via CAN | **Gap** — no wire-level electrical faults |
| **P5: Automated test + traceability** | AutomationDesk / vTESTstudio + DOORS | Python scripts + verdict_checker | **Gap** — no structured traceability |

---

## 6. Gap Register

### GAP-1: No Hardware Fault Insertion Unit (FIU)

**Severity**: High — ISO 26262 Part 4 requires fault injection testing for safety validation.

**What we can do now**: Software fault injection over CAN (corrupt frames, timeout, E2E counter mismatch, bus-off simulation). Covers communication-level faults.

**What we cannot do**: Wire-level electrical faults under software control — open circuit, short to GND, short to VBAT, stuck-at-voltage. These test ECU analog input fault detection, wiring harness faults, and sensor failure modes.

**Professional approach**: dSPACE HighFlex boards with integrated FIU relays ($5,000+ per module). Vector VT2820 relay module (20 channels, ~$3,000+). NI PXI FIU modules ($5,000-$15,000).

**DIY closure strategy**: Arduino Mega + relay module matrix + Python REST control.

| Item | Qty | Cost |
|------|-----|------|
| Arduino Mega 2560 (clone) | 1 | $12 |
| 8-channel optocoupler relay board (5V, active-low) | 1 | $8 |
| 2-channel relay board (short-to-GND, short-to-VBAT bus) | 1 | $3 |
| 2.54mm IDC harness connectors | 1 set | $10 |
| Enclosure + terminal blocks | 1 | $15 |
| **Total** | | **~$50** |

**How it works**: Each ECU harness wire passes through a normally-closed relay contact (normal operation). Opening the relay = open-circuit fault. A second relay per node shorts the fault bus to GND or VBAT. Arduino sketch exposes serial commands (`OPEN 3`, `SHORT_GND 5`, `NORMAL ALL`). Python wrapper over `pyserial` exposes REST endpoint that integrates with existing test framework.

**Limitations vs professional**:
- Relay switching: ~5-10ms (professional FIU: ~100µs) — adequate for 100ms FTTI scenarios
- No programmable stuck-at-voltage (only hard open/close)
- Max 10A/30VDC per relay channel (sufficient for ECU harness signals, not power lines)
- Relay bounce creates transients — add RC snubbers on inductive lines

**Effort**: 3-5 days (Arduino sketch + Python wrapper + wiring harness + integration test)

**Reference**: [MDPI 2025 — Cost-effective embedded system for remote ECU testing](https://www.mdpi.com/2076-3417/15/23/12736)

---

### GAP-2: Plant Model Not Hard Real-Time

**Severity**: Medium — Affects plant model fidelity, but vehicle-level dynamics (10-100ms time constants) are tolerant of 1-5ms jitter.

**What we can do now**: Python plant-sim on RPi 4 with stock Linux kernel. Measured worst-case jitter: 1-5ms. CAN frame period at 500 kbps is ~0.5ms, so the plant model is adequate for 1kHz physics updates of slow dynamics (motor inertia, battery voltage, thermal).

**What we cannot do**: Sub-millisecond deterministic plant model steps (required for motor commutation, resolver emulation, power electronics switching simulation).

**Professional approach**: dSPACE SCALEXIO uses dedicated Xeon + RTOS with sub-µs jitter. FPGA boards handle sub-10µs signal processing independently.

**DIY closure strategy (2 tiers)**:

**Tier 1 — PREEMPT_RT kernel on RPi 4 ($0, 1-2 days)**:
- Flash PREEMPT_RT patched kernel (available at [github.com/remusmp/rpi-rt-kernel](https://github.com/remusmp/rpi-rt-kernel))
- Set plant-sim Python thread to `SCHED_FIFO` priority 80 via `ctypes`
- Isolate CPU core: `isolcpus=3` in `/boot/cmdline.txt`, pin process with `taskset`
- **Result**: Worst-case jitter drops from 1-5ms to ~100-200µs. Sufficient for 1kHz plant model.

**Tier 2 — Offload plant model to dedicated STM32 ($0, 1-2 weeks)**:
- Port motor physics model to C, run on a spare STM32G474RE with FreeRTOS
- Hardware timer drives 1ms step with ~1µs jitter
- Plant-model ECU communicates via CAN (same as other ECUs)
- RPi becomes test orchestrator and data logger only
- **Result**: Hard real-time plant model at 1kHz, deterministic. Can truthfully claim "hard real-time plant model on Cortex-M4F at 170 MHz."

**Recommended**: Do Tier 1 immediately (1 day). Document Tier 2 as the production-intent architecture. Implement one model (motor) on STM32 if time permits.

**Reference**: [LeMaRiva Tech — RPi 4B PREEMPT_RT performance](https://lemariva.com/blog/2019/09/raspberry-pi-4b-preempt-rt-kernel-419y-performance-test)

---

### GAP-3: No Analog Sensor Simulation

**Severity**: Medium — Limits ability to test ECU ADC fault detection (stuck-high, stuck-low, drift, out-of-range) without physically manipulating sensors.

**What we can do now**: Real sensors (AS5048A SPI angle, ACS723 Hall current, NTC thermistor) produce real electrical signals. Sensor plausibility can be tested by CAN-injected contradictory values (cross-check logic).

**What we cannot do**: Electrically simulate a failed sensor at the ADC pin (stuck voltage, slow drift, out-of-range condition) without disconnecting the real sensor and substituting a voltage source.

**Professional approach**: dSPACE HighFlex boards output 0-10V at 16-bit resolution per channel. Vector VT2004 provides 4 analog channels with built-in fault relays.

**DIY closure strategy**: MCP4728 quad DAC + RP2040 SPI emulator + UART emulator.

| Item | Qty | Cost |
|------|-----|------|
| Adafruit MCP4728 quad 12-bit I2C DAC breakout | 2 | $14 |
| RP2040 Raspberry Pi Pico (SPI slave emulation) | 1 | $4 |
| MCP6004 quad op-amp buffer (rail-to-rail) | 1 | $1.50 |
| Resistors, caps (anti-alias, voltage divider) | 1 set | $2 |
| Perfboard + headers | 1 | $5 |
| **Total** | | **~$30** |

**Architecture**:

```
Test orchestrator (RPi/PC)
    |
    | I2C (set DAC values)        UART (send fake lidar frames)
    |                              |
    v                              v
 MCP4728 DAC x2               RPi/Pico UART TX
 (8 analog channels)           (TFMini-S protocol)
    |                              |
    | 0-3.3V analog               | UART @ 115200
    v                              v
 ECU ADC pins                  FZC UART RX
 (NTC, ACS723, battery         (lidar input)
  voltage divider)

 RP2040 Pico (SPI slave mode)
    |
    | SPI (responds to CS/CLK with programmed angle)
    v
 CVC SPI bus
 (AS5048A emulation)
```

**Sensor-by-sensor coverage**:

| Sensor | Emulation Method | Notes |
|--------|-----------------|-------|
| NTC thermistor (ADC) | MCP4728 DAC output → voltage divider equivalent | Compute divider voltage from target temperature, feed to ADC pin |
| ACS723 current (ADC) | MCP4728 DAC output → ACS723-equivalent voltage | ACS723 outputs VCC/2 + 400mV/A. DAC replicates this. |
| Battery voltage (ADC) | MCP4728 DAC output → scaled voltage | Match the 47k/10k divider output range |
| AS5048A angle (SPI) | RP2040 as SPI slave, returns programmed 14-bit angle | Test orchestrator sets target angle via USB serial |
| TFMini-S lidar (UART) | Python script on RPi sends binary TFMini frames | Protocol documented: 9-byte frame at 115200 baud |
| Quadrature encoder (GPIO) | RP2040 PIO state machine generates A/B/Z pulses | Programmable frequency = programmable RPM |

**Limitations vs professional**:
- 12-bit DAC resolution (0.8mV step on 3.3V) vs 16-bit (professional). But STM32G474 ADC is also 12-bit, so DAC resolution matches ECU's own sensing resolution.
- MCP4728 I2C update rate ~1kHz — adequate for thermal and current simulation, not for high-frequency signals
- No closed-loop impedance matching for resistive sensors. Use MCP4131 digital potentiometer ($1.50) for true NTC resistance simulation if needed.

**Effort**: 3-5 days (DAC wiring, Python I2C driver via smbus2, NTC lookup table, SPI slave firmware on Pico, UART emulator script)

**Reference**: [Adafruit MCP4728](https://www.adafruit.com/product/4470), [Keysight — Emulating analog sensors](https://www.keysight.com/us/en/assets/7018-07101/article-reprints/5992-1336.pdf)

---

### GAP-4: No Actuator Load Simulation

**Severity**: Low — We use real actuators (motor, servos), which is actually stronger evidence than simulated loads for this platform.

**What we can do now**: Real motor (12V DC + BTS7960 H-bridge) and real servos (MG996R) produce real back-EMF, real inductive kickback, and real current draw. The BTS7960 IS+ current sense output provides real feedback. The ACS723 measures real motor current.

**What we cannot do**: Programmable load profiles (simulate stall at 5A, simulate no-load, simulate inductive ramp) without physically changing the mechanical load on the motor.

**Professional approach**: Vector VT6104 load module provides programmable resistive/inductive/capacitive loads. dSPACE power stage boards simulate motor impedance.

**DIY closure strategy**: ATORCH DL24P electronic load + Python control.

| Item | Qty | Cost |
|------|-----|------|
| ATORCH DL24P electronic load (150W, 0-20A, USB + Bluetooth) | 1 | $40 |
| **Total** | | **~$40** |

**How it works**: The DL24P supports CC (constant current), CV, CR (constant resistance), and CP modes. Python library ([github.com/Jay2k1/Electronic_load_DL24](https://github.com/Jay2k1/Electronic_load_DL24)) provides USB control. Script the load profile: ramp 0A → 10A over 5s (stall simulation), hold at 2A (steady state), release to 0A (no-load).

**Alternative DIY ($8)**: Classic MOSFET constant-current load — IRF540N + LM358 op-amp + 0.1R sense resistor + DAC setpoint. Handles 0-10A continuous with heatsink. 1 day to build. Well-documented on [Hackaday.io](https://hackaday.io/project/163589-diy-constant-current-load).

**Limitations vs professional**:
- Linear resistive load only — cannot simulate back-EMF regeneration or inductive kickback
- DL24P is a general-purpose tool, not synchronized to the test sequence
- No impedance spectrum matching (professional loads match motor L/R at frequency)

**Effort**: 1 day (DL24P: Python integration + test script)

---

### GAP-5: No Rest-Bus Simulation

**Severity**: Medium-High — Professional HIL only tests the ECU under test; all other nodes are simulated. Our bench has all 7 ECUs (4 physical + 3 Docker), but no ability to simulate absent nodes or inject specific message values from non-existent network participants.

**What we can do now**: All 7 ECUs run simultaneously on the CAN bus. The Docker-based BCM/ICU/TCU already act as "simulated nodes" with real CAN traffic.

**What we cannot do**: Simulate an arbitrary CAN node (e.g., a Battery Management System or a Transmission Controller) that our ECUs expect but don't have. Also cannot arbitrarily replace one of our real ECUs with a simulated stub for isolated testing.

**Professional approach**: Vector CANoe provides full rest-bus simulation — only the ECU under test is real, everything else is simulated with CAPL scripts, correct message timing, and DBC-driven signal encoding. dSPACE uses the same concept via its bus board + model.

**DIY closure strategy**: python-can + cantools + SocketCAN broadcast manager. **$0 cost.**

**Architecture**:

```
RPi / PC (test host)
    |
    | python-can (SocketCAN interface)
    v
CANable USB-CAN adapter
    |
    | CAN bus @ 500 kbps
    v
ECU under test (physical STM32)
```

**Key tools**:

| Tool | Source | Purpose |
|------|--------|---------|
| python-can | [github.com/hardbyte/python-can](https://github.com/hardbyte/python-can) | SocketCAN interface, `Bus.send_periodic()` uses kernel broadcast manager for zero-overhead cyclic TX |
| cantools | [github.com/cantools/cantools](https://github.com/cantools/cantools) | DBC parser, signal encode/decode, `NodeTester` class for absent-node simulation |
| CANdevStudio | [github.com/GENIVI/CANdevStudio](https://github.com/GENIVI/CANdevStudio) | Qt GUI for visual CAN node simulation (DBC import, drag-and-drop) |

**What we build**:
1. Load our existing `gateway/taktflow.dbc` into cantools
2. Python script creates cyclic transmitters for each "absent" node's messages at correct periods
3. REST endpoint allows test framework to modify signal values mid-test (e.g., set SOC=5% to trigger undervoltage response)
4. `python-can` broadcast manager runs in kernel space — handles 100+ cyclic frames with negligible CPU

**Example** (simulate a BMS node):
```python
import can, cantools

db = cantools.database.load_file('gateway/taktflow.dbc')
bus = can.interface.Bus(channel='can0', bustype='socketcan')

bms_msg = db.get_message_by_name('BMS_Status')
data = bms_msg.encode({'SOC': 80, 'BattVoltage': 48.0, 'BattTemp': 25})
task = bus.send_periodic(
    can.Message(arbitration_id=bms_msg.frame_id, data=data), 0.1
)

# Inject fault: SOC drops to 5%
data_fault = bms_msg.encode({'SOC': 5, 'BattVoltage': 44.0, 'BattTemp': 55})
task.modify_data(can.Message(arbitration_id=bms_msg.frame_id, data=data_fault))
```

**Limitations vs professional**:
- No FlexRay, LIN, or Automotive Ethernet simulation (only CAN 2.0B)
- No CAPL scripting (Python instead — functionally equivalent, different syntax)
- SocketCAN jitter on Linux ~100µs–1ms (vs CANoe's dedicated hardware timestamping)

**Effort**: 2-3 days (DBC validation, node simulator script, REST integration, test with one ECU isolated)

---

### GAP-6: No Automated Test Execution with Traceability

**Severity**: High — ISO 26262 Part 8 and ASPICE SWE.4-6 require traceable test execution. Our current SIL verdict_checker proves pass/fail but does not link results to requirement IDs.

**What we can do now**: `verdict_checker.py` evaluates 19 SIL scenarios against YAML-defined pass criteria. CI produces JUnit XML. But test cases are not tagged with requirement IDs, and reports don't show requirement coverage.

**What we cannot do**: Execute a test suite where each test case is linked to a specific SSR/SWR/TSR, and produce a report showing which requirements are verified, which are not, and what the pass/fail rate is per ASIL level.

**Professional approach**: dSPACE AutomationDesk (ISO 26262 certified) links test steps to DOORS requirements. Vector vTESTstudio provides structured test design with requirement traceability and HTML reports. Both cost $10k-$50k per seat.

**DIY closure strategy**: Robot Framework + CAN keyword library + Allure reports. **$0 cost.**

**Architecture**:

```
Robot Framework (.robot test files)
    |
    | [Tags]  SSR-042  ASIL-B
    |
    v
CANLibrary (python-can wrapper)
    |
    | send/receive CAN frames
    v
CANable USB-CAN → ECU under test

    |
    v
Allure Report Generator
    |
    v
HTML report grouped by requirement ID
```

**Key tools**:

| Tool | Source | Purpose |
|------|--------|---------|
| Robot Framework | [robotframework.org](https://robotframework.org/) | Keyword-driven test framework, Python-extensible |
| robotframework-can-uds-library | [github.com/Openwide-Ingenierie](https://github.com/Openwide-Ingenierie/robotframework-can-uds-library) | CAN + UDS keywords for Robot Framework |
| Allure Report | [allurereport.org](https://allurereport.org/) | Interactive HTML test reports with requirement grouping |
| pytest + pytest-bdd (alternative) | PyPI | `@pytest.mark.requirement("SSR-042")` tags, Allure adapter |

**Example Robot Framework test**:
```robot
*** Settings ***
Library    CANLibrary    channel=can0    bustype=socketcan

*** Test Cases ***
TC_042 Motor Overcurrent Triggers Safe State
    [Tags]    SSR-RZC-007    ASIL-B    SG-003
    [Documentation]    Verify RZC detects overcurrent >10A and de-energizes motor within 50ms
    Set Motor Setpoint    100    # 100% duty cycle
    Set Electronic Load    CC    12.0    # Force 12A draw (over 10A threshold)
    Wait For CAN Signal    RZC_FaultCode    ==    0x15    timeout=500ms
    Verify Motor PWM    ==    0    # Motor de-energized
    [Teardown]    Reset Electronic Load
```

**Traceability output**: Allure groups results by `[Tags]`, producing a view: "SSR-RZC-007: PASS (3 test cases)", "SG-003: 8/8 PASS". This maps directly to the traceability matrix in `aspice/traceability/traceability-matrix.md`.

**Limitations vs professional**:
- No DOORS integration (requirements in Markdown, not a database)
- No real-time synchronized test steps (Python-level timing, not µs-precise)
- No graphical test designer (text-based `.robot` files)
- Report format is HTML, not a certified tool output

**Effort**: 1 week (Robot Framework setup, CAN keyword library wrapper, template 10 test cases, Allure integration, CI pipeline addition)

**Reference**: [DornerWorks — Robot Framework for HIL](https://www.dornerworks.com/blog/robot-framework-tutorial/)

---

### GAP-7: No XCP/A2L Calibration Access

**Severity**: Medium — Professional calibration workflows (CANape, INCA) use XCP protocol to read/write ECU internal variables without recompiling. We currently have no way to inspect ECU state except via CAN messages.

**What we can do now**: ECU state is visible through CAN output messages (motor RPM, fault codes, temperatures). Debug builds can output extra data via UART/SWD.

**What we cannot do**: Live read/write of arbitrary ECU internal variables (PID gains, fault thresholds, calibration constants) without reflashing firmware.

**Professional approach**: Vector CANape ($10k+/seat) + XCP over CAN + A2L file from AUTOSAR toolchain. dSPACE ControlDesk provides similar XCP measurement and calibration.

**DIY closure strategy**: XCPlite (STM32 slave) + pyxcp (Python host) + A2L from ELF symbols. **$0 cost.**

**Key tools**:

| Tool | Source | Purpose |
|------|--------|---------|
| XCPlite | [github.com/vectorgrp/XCPlite](https://github.com/vectorgrp/XCPlite) | Minimal open-source XCP slave in C (by Vector Group, MIT license). Supports XCP over CAN and Ethernet. |
| pyxcp | [github.com/christoph2/pyxcp](https://github.com/christoph2/pyxcp) | Python XCP master. CAN via python-can. Read/write variables, DAQ streaming, A2L parsing. |
| OpenXCP | [github.com/shreaker/OpenXCP](https://github.com/shreaker/OpenXCP) | Alternative: both slave and master in one repo. |

**Architecture**:
```
pyxcp (PC/RPi)
    |
    | XCP over CAN (CRO/DTO messages)
    v
CANable USB-CAN
    |
    v
STM32G474RE (CVC/FZC/RZC)
    |
    | XCPlite slave library
    | Registered variables: KP_motor, fault_threshold, ...
    v
ECU RAM (live values)
```

**A2L file generation**: Extract symbol names and addresses from `.elf` via `arm-none-eabi-nm`. Python script generates minimal A2L (~50 lines for 10 parameters). Not as polished as AUTOSAR-toolchain-generated A2L, but functionally equivalent for pyxcp.

**Example usage**:
```python
import pyxcp

with pyxcp.Master('can', config={'channel': 'can0', 'bustype': 'socketcan'}) as xcp:
    xcp.connect()
    kp = xcp.readVariable('KP_motor')             # Read live from ECU RAM
    xcp.writeVariable('fault_threshold', 0.8)      # Calibrate on-the-fly
    xcp.startDAQ(['MotorRPM', 'BatteryCurrent'])   # Live streaming
```

**Limitations vs professional**:
- Manual A2L generation (professional: auto-generated from AUTOSAR system description)
- No ASAM MCD-3 / ODX integration
- No graphical measurement dashboard (pyxcp is CLI/script only — could add Grafana for visualization)

**Effort**: 2-3 weeks total (XCPlite integration: 3-5 days, A2L generator: 1-2 days, pyxcp host: 1-2 days, end-to-end testing: 2-3 days)

**Recommended**: Implement a minimal demo — one ECU (CVC), 5 variables (KP_motor, KI_motor, fault_threshold, pedal_deadzone, e2e_timeout). That's enough to demonstrate "XCP over CAN with pyxcp and XCPlite."

**Reference**: [CSS Electronics — A2L file explained](https://www.csselectronics.com/pages/a2l-file-asap2-intro-xcp-on-can-bus)

---

### GAP-8: No Deterministic Test Sequencing

**Severity**: Low — Python test scripts have 1-5ms timing uncertainty. For vehicle-level HIL scenarios with 100ms control loops and 50-200ms FTTI, this is adequate. Only matters for sub-ms injection timing.

**What we can do now**: Python scripts send CAN frames and measure response timing using `time.monotonic()`. `python-can` with SocketCAN records kernel-assigned hardware timestamps on received frames (µs resolution). This gives precise measurement of ECU response times even though injection timing has jitter.

**What we cannot do**: Guarantee that a fault injection CAN frame arrives at a specific CAN bus cycle (deterministic to the bit-time level).

**Professional approach**: AutomationDesk uses real-time synchronized sequencer on dedicated hardware. dSPACE FPGA boards handle sub-µs injection timing.

**DIY closure strategy**: Hardware timestamps + RP2040 PIO for µs GPIO timing. **$4 cost.**

**Architecture**:
```
Test orchestrator (Python on RPi/PC)
    |
    | "inject fault X at T+100ms" command
    v
RP2040 Pico (PIO state machine)
    |
    | Hardware timer, ~1µs jitter
    v
FIU relay GPIO / CAN transceiver
```

**What to implement**:
1. Add `time.monotonic()` timestamps to every test step in pytest fixtures
2. Use `python-can` `msg.timestamp` (kernel hardware timestamp) for all CAN event timing assertions
3. Write a `TimingAssertion` class: `assert_response_within(msg, threshold_ms)`
4. Log all events to SQLite with timestamps for post-run analysis
5. (Optional) RP2040 Pico as precision GPIO timer for relay FIU switching

**Limitations vs professional**:
- Injection timing: Python ~1-5ms jitter vs AutomationDesk sub-µs
- Adequate for FTTI > 50ms scenarios, insufficient for µs-level power electronics testing
- The RP2040 PIO escalation path gives ~10ns GPIO accuracy if needed

**Effort**: 2-3 days (timestamp infrastructure in pytest, timing assertion helpers, SQLite event logger)

---

## 7. Summary: Cost, Effort, Priority

| # | Gap | Cost | Effort | Priority | Rationale |
|---|-----|------|--------|----------|-----------|
| **5** | Rest-bus simulation | $0 | 2-3 days | **1 — Do first** | Highest ROI: free, enables isolated ECU testing, uses existing DBC |
| **6** | Automated test traceability | $0 | 1 week | **2 — High** | Converts ad-hoc scripts to ASPICE-compliant traceable test suite |
| **1** | Hardware FIU | ~$50 | 3-5 days | **3 — High** | Tangible hardware artifact, directly addresses ISO 26262 fault injection |
| **3** | Analog sensor simulation | ~$30 | 3-5 days | **4 — Medium** | High-visibility gap, demonstrates signal chain understanding |
| **2** | RT plant model (PREEMPT_RT) | $0 | 1-2 days | **5 — Medium** | Quick win, measurable jitter improvement |
| **4** | Actuator load simulation | ~$40 | 1 day | **6 — Low** | Nice-to-have, real loads already stronger evidence |
| **8** | Deterministic sequencing | $4 | 2-3 days | **7 — Low** | Timestamp infrastructure, adequate for FTTI > 50ms |
| **7** | XCP/A2L calibration | $0 | 2-3 weeks | **8 — Last** | High effort, impressive but not blocking; implement minimal demo |

**Total hardware cost to close all gaps**: ~$125
**Total effort**: ~5-7 weeks (sequential), ~3-4 weeks (parallelized)
**Remaining budget after all closures**: ~$875

---

## 8. After Closure: What We Can Claim

### Before gap closure (current state)

> "4-ECU bench with real sensors and actuators, CAN bus, software fault injection via REST API, Python plant model, and SIL test suite with 19 automated scenarios."

### After gap closure (all 8 gaps addressed)

> "DIY HIL test bench with closed-loop real-time plant model (PREEMPT_RT, 200µs worst-case jitter), 8-channel hardware fault insertion unit, analog sensor simulation board (MCP4728 DAC + RP2040 SPI/UART emulation), programmable electronic load, rest-bus simulation via python-can/cantools, automated test execution with Robot Framework linked to 548 safety requirements via Allure traceability reports, and XCP over CAN calibration access via XCPlite/pyxcp — covering the functional equivalent of dSPACE SCALEXIO / Vector VT System at 0.1% of the cost."

### Honest remaining limitations (always disclose)

| Limitation | Why it doesn't matter for portfolio |
|------------|-------------------------------------|
| Relay FIU switching 5-10ms, not 100µs | FTTI targets are 50-200ms; relay speed is adequate |
| RPi plant model jitter ~200µs, not sub-µs | Vehicle dynamics time constants are 10-100ms |
| 12-bit DAC, not 16-bit | Matches STM32 ADC resolution (also 12-bit) |
| No FlexRay/LIN/Automotive Ethernet | CAN-only platform by design |
| No certified tool output | Portfolio project, not production safety case |
| No DOORS integration | Requirements in Markdown with automated traceability CI |

---

## 9. Comparison to DIY Embedded Projects (Context)

For context, this analysis puts the project in perspective against the broader embedded community:

| Tier | Typical DIY Project | HIL Capability |
|------|-------------------|----------------|
| Beginner (90%) | Arduino blink, ESP32 weather station | None |
| Intermediate (9%) | Custom PCB, FreeRTOS, basic CAN via MCP2515 | None |
| Advanced DIY (1%) | Multi-layer PCB, Zephyr RTOS, OBD2 reverse engineering | None |
| **This project (before gap closure)** | **7-ECU AUTOSAR platform, ASIL D, MISRA C, 1067 tests** | **Partial (SIL + software fault injection)** |
| **This project (after gap closure)** | **+ FIU + sensor sim + RT plant + rest-bus + traceability** | **Full DIY HIL equivalent** |
| Professional (dSPACE/Vector) | Commercial product, $150k-$500k | Full production HIL |

No DIY embedded project found on GitHub or YouTube includes: MISRA compliance, a hardware FIU, rest-bus simulation, automated test traceability to safety requirements, or XCP calibration access. After gap closure, this project stands alone in that space.

---

## 10. Hardware Safety Cautions for Fault Injection Testing

**Read this section before powering on any FIU or sensor simulation hardware.**

### 10.1 Risk Classification

| Risk Level | Tests | Damage if wrong |
|---|---|---|
| **Safe** | Open circuit via relay, software CAN injection, DAC 0-3.3V to ADC pin, watchdog timeout test | None |
| **Low risk** | Short sensor to GND (with current limit), electronic load <5A, E-stop/kill relay test | Blown resistor (€0.02) |
| **DANGEROUS** | Short 12V to 3.3V MCU pin, motor stall >10s at full PWM, reverse polarity | **Dead Nucleo (€16) or LaunchPad (€40)** |

### 10.2 Mandatory Precautions

**RULE 1: No VBAT fault bus on MCU signal lines.**
Do NOT build a short-to-12V relay path on any wire connected to an MCU GPIO/ADC pin. 12V on a 3.3V pin kills the STM32 instantly. Only use short-to-GND on MCU-facing channels. Short-to-VBAT is only safe on actuator power lines (motor, relay coil, servo power).

**RULE 2: Current-limiting resistors on every FIU channel.**
Add a 100R resistor in series on every FIU relay channel that connects to an MCU pin. At 3.3V, this limits short-circuit current to 33mA — safe for all components. Use resistors from existing stock (#49, 10k or #52, 100R).

**RULE 3: Verify voltage with oscilloscope before triggering any fault.**
Before the first fault injection on any channel:
1. Power up with relay closed (normal path)
2. Probe the wire with oscilloscope
3. Confirm voltage matches expectation (3.3V signal, not 12V power)
4. Only then trigger the fault from the test script

The oscilloscope does not protect anything — it lets you see a miswiring before it destroys hardware.

**RULE 4: Hard timeout on every destructive test.**
Every test script that stresses power components must have a maximum duration enforced in code:
- Motor at >80% PWM: **max 5 seconds**
- Electronic load at >5A: **max 10 seconds**
- Any short-to-GND fault: **max 2 seconds**
- If the test framework crashes, the Arduino FIU must auto-restore all relays to normal after a serial timeout (e.g., 5s no heartbeat → `NORMAL ALL`)

**RULE 5: Start every test at minimum stress.**
- Electronic load: start at 1A, step up in 1A increments
- DAC output: start at mid-range (1.65V), not boundary values
- Motor PWM: start at 10% duty, not 100%
- Verify each step is safe before increasing

**RULE 6: Never hot-swap FIU wiring.**
Always power off the entire bench before connecting or disconnecting any FIU harness wire. A loose wire touching the wrong pin while powered can short 12V to 3.3V.

**RULE 7: Double-check polarity on every power-up.**
Before the first power-up of any session, verify:
- 12V PSU polarity (+ to + rail, - to GND rail)
- Buck converter output voltage (measure with multimeter before connecting to load)
- Servo power rail (6V, not 12V)
- SB560 Schottky diode direction on main power rail

### 10.3 Design Decision: Skip VBAT Fault Bus

The original GAP-1 design included a 2-channel relay board (#79) for short-to-GND and short-to-VBAT fault buses. **Recommendation: only build the short-to-GND bus.** Reasons:
- Short-to-VBAT on a miswired channel destroys MCU hardware
- Short-to-GND is safe with 100R current limiting
- Open-circuit + short-to-GND covers the most common wiring faults
- Short-to-VBAT testing can be done manually with the oscilloscope connected, one wire at a time, on actuator-only lines

This reduces item #79 to a 1-channel relay (GND bus only) or removes it entirely if the 8-channel board has a spare channel.

### 10.4 FIU Arduino Firmware Safety Features

The Arduino FIU sketch must implement:
- **Watchdog heartbeat**: If no serial command received for 5 seconds, auto-execute `NORMAL ALL` (restore all relays to closed/normal)
- **Mutual exclusion**: Never allow short-to-GND and short-to-VBAT on the same channel simultaneously
- **Channel lockout**: Channels marked as "MCU-facing" in firmware config cannot be switched to VBAT fault bus
- **Status LED**: Green = all normal, Red = any fault active, Blink = heartbeat timeout recovery

### 10.5 Per-Test-Type Checklist

**Before any open-circuit test:**
- [ ] Verify channel is correct (probe with oscilloscope)
- [ ] Test script has timeout
- [ ] ECU is running and CAN bus is active

**Before any short-to-GND test:**
- [ ] 100R current-limiting resistor is in the FIU channel
- [ ] Confirm wire is 3.3V signal, not 12V power
- [ ] Probe with oscilloscope first
- [ ] Test script timeout ≤ 2 seconds

**Before any electronic load test:**
- [ ] Start at 1A, not target current
- [ ] BTS7960 heatsink is mounted
- [ ] Motor is mechanically secured (will spin under load)
- [ ] Test script timeout ≤ 10 seconds
- [ ] ACS723 current reading matches load setpoint (cross-check)

**Before any DAC sensor simulation test:**
- [ ] DAC output voltage verified with multimeter before connecting to ECU
- [ ] DAC output range is within 0-3.3V (never exceeds MCU VDD)
- [ ] Op-amp buffer is powered from same 3.3V rail as MCU

---

## 11. References

### Professional HIL Systems
- [dSPACE SCALEXIO — Product Overview](https://www.dspace.com/en/ltd/home/products/hw/simulator_hardware/scalexio.cfm)
- [dSPACE AutomationDesk — ISO 26262 Certified](https://www.dspace.com/en/inc/home/products/sw/test_automation_software/automationdesk.cfm)
- [Vector VT System — Modular HIL Platform](https://www.vector.com/int/en/products/products-a-z/hardware/vt-system/)
- [Vector CANoe — Network Simulation](https://www.vector.com/int/en/products/products-a-z/software/canoe/)
- [NI — Using Fault Insertion Units for ECU Testing](https://www.ni.com/en/solutions/transportation/hardware-in-the-loop/using-fault-insertion-units--fius--for-electronic-testing.html)

### DIY Closure Solutions
- [python-can — GitHub](https://github.com/hardbyte/python-can)
- [cantools — GitHub](https://github.com/cantools/cantools)
- [CANdevStudio (GENIVI) — GitHub](https://github.com/GENIVI/CANdevStudio)
- [XCPlite (Vector Group, MIT) — GitHub](https://github.com/vectorgrp/XCPlite)
- [pyxcp — GitHub](https://github.com/christoph2/pyxcp)
- [OpenXCP — GitHub](https://github.com/shreaker/OpenXCP)
- [Robot Framework — Official](https://robotframework.org/)
- [robotframework-can-uds-library — GitHub](https://github.com/Openwide-Ingenierie/robotframework-can-uds-library)
- [Allure Report — Official](https://allurereport.org/)
- [ATORCH DL24 Python Library — GitHub](https://github.com/Jay2k1/Electronic_load_DL24)
- [Adafruit MCP4728 Quad DAC — Product](https://www.adafruit.com/product/4470)
- [RPi PREEMPT_RT Kernel — GitHub](https://github.com/remusmp/rpi-rt-kernel)
- [CSS Electronics — A2L File Explained](https://www.csselectronics.com/pages/a2l-file-asap2-intro-xcp-on-can-bus)

### Research Papers
- [MDPI 2025 — Cost-Effective Embedded System for Remote ECU Testing](https://www.mdpi.com/2076-3417/15/23/12736)
- [Keysight — Emulating Analog Input Sensors in Automotive](https://www.keysight.com/us/en/assets/7018-07101/article-reprints/5992-1336.pdf)
- [DornerWorks — Robot Framework for HIL Testing](https://www.dornerworks.com/blog/robot-framework-tutorial/)

### Price References
- dSPACE SCALEXIO full HIL: $150,000–$500,000 (single ECU bench)
- dSPACE system-of-systems HIL: $500,000–$2,000,000+
- Vector CANoe base license: ~10,000–40,000 EUR per seat
- Vector VT System hardware (8-10 modules): ~30,000–80,000 EUR
- Used dSPACE HIL on eBay: [listed at >$1.2M original](https://www.ebay.com/itm/127170835214)

---

## 11. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-03-01 | System | Initial gap analysis: 8 gaps identified, closure strategies with BOM, effort, and priority. Full comparison against dSPACE SCALEXIO and Vector VT System. |
| 1.1 | 2026-03-01 | System | Added section 10: Hardware safety cautions for fault injection testing. 7 mandatory rules, per-test checklists, FIU firmware safety features, VBAT fault bus risk decision. |
