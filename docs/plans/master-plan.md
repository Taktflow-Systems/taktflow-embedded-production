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

# Master Plan: Zonal Vehicle Platform — ASIL D Portfolio

**Status**: IN PROGRESS (Phases 0-17 DONE)
**Created**: 2026-02-20
**Updated**: 2026-02-25
**Goal**: Hire-ready automotive functional safety + cloud + ML portfolio

---

## Full Setup (Before Walkthrough)

```text
      DRIVER INPUT
           |
           v
   ______________________
  /|                    |\        +-----------------------------+
 /_|  SMART VEHICLE      | \      | Edge Gateway (Raspberry Pi)|
|   | (7 ECU Platform)   |  |---->| CAN ingest + ML inference  |
| O | CVC/FZC/RZC/SC     | O|     | DTC + Soft DTC generation  |
|___| BCM/ICU/TCU        |__|     +---------------+-------------+
                                         | MQTT/TLS
                                         v
                               +-----------------------------+
                               | Cloud Platform              |
                               | IoT Core + Rules + Storage |
                               | Grafana + Alerting          |
                               +------+----------------------+
                                      |
                     +----------------+------------------+
                     |                                   |
                     v                                   v
     +-----------------------------+       +-----------------------------+
     | Quality System (SAP QM)     |       | Business/Finance Layer      |
     | Q-Meldung, 8D, CAPA         |       | Cost of quality, warranty,  |
     | corrective action workflow  |       | service KPIs, management BI |
     +-----------------------------+       +-----------------------------+
```

### Itemized Setup

1. Vehicle layer: zonal car platform running safety + control + diagnostics.
2. Edge layer: local gateway does protocol handling and ML inference.
3. Cloud layer: telemetry, rules, and real-time observability.
4. Quality layer: SAP QM notifications and 8D corrective-action lifecycle.
5. Business layer: quality cost, warranty/service metrics, and leadership reporting.

## Quick Walkthrough (How This Works)

This platform is a 7-ECU zonal system on CAN:

1. Driver input enters through **CVC** (dual pedal sensors + vehicle state logic).
2. CVC sends motion commands over CAN to **FZC** (steering/brake) and **RZC** (motor).
3. FZC and RZC execute control and send feedback (angle, current, temp, speed, status).
4. **SC (TMS570 Safety Controller)** independently monitors heartbeats and plausibility; if unsafe, it opens kill relay and forces safe state.
5. Simulated ECUs (**BCM, ICU, TCU**) run in Docker and consume/produce CAN traffic for body functions, dashboard, and diagnostics.
6. Raspberry Pi gateway logs CAN, runs ML inference, and publishes telemetry/alerts to cloud dashboards.
7. Faults become DTCs, then can flow into quality workflow (SAP QM mock + 8D report path).

Development and validation run in three modes:

- Full mixed mode: 4 physical ECUs + 3 simulated ECUs
- Pure software mode: all virtual ECUs on `vcan0`
- Partial hardware mode: any subset physical, rest simulated

## Example Full Workflow (One Fault Scenario)

Scenario: motor overcurrent during driving.

1. CVC sends torque request to RZC.
2. RZC drives motor and reads current sensor.
3. Current exceeds threshold for debounce window.
4. RZC triggers protection: derate or cut motor output, reports fault to Dem/DTC path.
5. ICU shows warning; TCU stores/serves DTC via UDS (`0x19`, `0x14` etc.).
6. SC still supervises global safety; if heartbeat/plausibility fails, SC opens kill relay.
7. Gateway detects anomaly pattern, publishes cloud alert.
8. Cloud pipeline creates quality notification (SAP QM mock), links to 8D workflow.
9. Test evidence is captured across xIL levels (SIL/PIL/HIL) and added to safety/verification reports.

---

## Architecture Overview

### Zonal Controller Architecture (Modern E/E) — 7 ECUs (4 Physical + 3 Simulated)

Unlike traditional domain-based architectures (one ECU per function), this project uses a **zonal architecture** — the modern approach adopted by Tesla, VW, BMW. ECUs are organized by physical vehicle zone with a central computer on top.

The architecture uses **7 ECU nodes**: 4 run on real hardware with real sensors and actuators, and 3 run as software-simulated ECUs (Docker containers or native processes using virtual CAN). This mirrors real automotive development where virtual ECUs (vECUs) are used alongside physical prototypes — a standard practice at Tier 1 suppliers using tools like Vector CANoe and dSPACE VEOS.

```
                                 ┌──────────────────────┐
                                 │      AWS CLOUD       │
                                 │  IoT Core → Grafana  │
                                 └──────────┬───────────┘
                                            │ MQTT
┌───────────────────────────────────────────┐
│  RASPBERRY PI 4 — Edge Gateway            │
│  • CAN telemetry → Cloud                  │
│  • Edge ML inference (anomaly, health)    │
│  • Fault injection GUI (demo scenarios)   │
│  • CAN data logging for ML training       │
│              CANable 2.0 (USB-CAN)        │
└───────────────────┬───────────────────────┘
                    │
════════════════════╪═══════════════ CAN Bus (500 kbps) ════════════
    │               │              │              │
┌───┴───────┐ ┌─────┴─────┐ ┌─────┴─────┐ ┌─────┴──────┐
│  CENTRAL  │ │FRONT ZONE │ │ REAR ZONE │ │  SAFETY    │
│  VEHICLE  │ │CONTROLLER │ │CONTROLLER │ │ CONTROLLER │
│  COMPUTER │ │           │ │           │ │            │
│ STM32G474 │ │ STM32G474 │ │ STM32G474 │ │  TMS570    │
│           │ │           │ │           │ │  LC43x     │
│• Pedal ×2 │ │• Steering │ │• Motor    │ │            │
│  (AS5048A)│ │  servo    │ │  + PWM    │ │• CAN listen│
│• E-stop   │ │• Brake    │ │• H-bridge │ │  (silent)  │
│• OLED     │ │  servo    │ │• Current  │ │• Heartbeat │
│• Vehicle  │ │• Lidar    │ │  sensor   │ │  monitor   │
│  state    │ │  (TFMini) │ │• Temp     │ │• Kill relay│
│  machine  │ │• Steering │ │  sensors  │ │• Fault LEDs│
│• CAN      │ │  angle    │ │• Encoder  │ │• Ext WDT   │
│  master   │ │  sensor   │ │• Battery  │ │• Lockstep  │
│           │ │• Buzzer   │ │  voltage  │ │  cores     │
└───────────┘ └───────────┘ └───────────┘ └────────────┘
  PHYSICAL      PHYSICAL      PHYSICAL      PHYSICAL
                    │
                    │ CAN bridge (CANable 2.0 USB-CAN on PC)
                    │
        ┌───────────┼────────────────────────┐
        │           │                        │
  ┌─────┴─────┐ ┌───┴───────┐ ┌─────────────┴─┐
  │   BODY    │ │INSTRUMENT │ │  TELEMATICS    │
  │  CONTROL  │ │  CLUSTER  │ │   CONTROL     │
  │  MODULE   │ │   UNIT    │ │    UNIT       │
  │  (BCM)    │ │  (ICU)    │ │   (TCU)       │
  │           │ │           │ │               │
  │• Lights   │ │• Gauges   │ │• UDS diag     │
  │• Indicators│ │• Warnings │ │• DTC storage  │
  │• Door lock│ │• DTC view │ │• OBD-II PIDs  │
  │• Hazards  │ │• Dash UI  │ │• Fault logging│
  └───────────┘ └───────────┘ └───────────────┘
   SIMULATED     SIMULATED      SIMULATED
   (Docker)      (Docker)       (Docker)
```

### ECU Summary Table

| ECU | Role | Type | Hardware / Runtime | ASIL |
|-----|------|------|--------------------|------|
| Central Vehicle Computer (CVC) | Vehicle brain, pedal input, state machine | **Physical** | STM32G474RE Nucleo | D (SW) |
| Front Zone Controller (FZC) | Steering, braking, lidar, ADAS | **Physical** | STM32G474RE Nucleo | D (SW) |
| Rear Zone Controller (RZC) | Motor, current, temp, battery | **Physical** | STM32G474RE Nucleo | D (SW) |
| Safety Controller (SC) | Independent safety monitor | **Physical** | TI TMS570LC43x LaunchPad | D (HW lockstep) |
| Body Control Module (BCM) | Lights, indicators, door locks | **Simulated** | Docker + vcan / CAN bridge | QM |
| Instrument Cluster Unit (ICU) | Dashboard gauges, warnings, DTCs | **Simulated** | Docker + vcan / CAN bridge | QM |
| Telematics Control Unit (TCU) | UDS diagnostics, OBD-II, DTC storage | **Simulated** | Docker + vcan / CAN bridge | QM |

**Diverse redundancy**: CVC/FZC/RZC use STM32 (ST). Safety Controller uses TMS570 (TI). Different vendor, different architecture = real ISO 26262 diverse redundancy.

### Simulated ECU Details

All 3 simulated ECUs are written in **C** (same codebase structure as physical ECUs) compiled for Linux with a POSIX SocketCAN abstraction layer. They run as Docker containers on the development PC, connected to the real CAN bus via a second CANable 2.0 adapter, or to a virtual CAN bus (vcan0) for pure-software testing.

**BCM — Body Control Module (QM)**
- Receives: vehicle state from CVC, speed from RZC, brake status from FZC
- Sends: light status (0x400), indicator state (0x401), door lock status (0x402)
- Logic: auto headlights at speed > 0, hazard lights on emergency brake, turn signals from steering angle
- Adds: realistic comfort-domain CAN traffic, multi-domain network demonstration

**ICU — Instrument Cluster Unit (QM)**
- Receives: ALL CAN messages (listen-only consumer)
- Sends: DTC acknowledgments (0x7FF)
- Output: Terminal UI (ncurses) or web dashboard showing speedometer, tachometer, temp gauge, warning lights
- Adds: visual demo output, proves CAN message decoding across full network

**TCU — Telematics Control Unit (QM)**
- Receives: diagnostic requests (UDS 0x7DF/0x7E0-0x7E6), all DTC broadcasts
- Sends: UDS responses (0x7E8-0x7EE), DTC storage confirmations
- Logic: UDS service handler (0x10 DiagSession, 0x22 ReadDataByID, 0x14 ClearDTCs, 0x19 ReadDTCs)
- Adds: UDS/OBD-II diagnostics stack — massive resume keyword, every OEM cares about this

### CAN Bus Topology

```
Development Modes:

MODE 1: Full Hardware + Simulated (Demo Mode)
  Real CAN Bus ─── CVC ─── FZC ─── RZC ─── SC ─── CANable(Pi) ─── CANable(PC)
                                                                         │
                                                           Docker: BCM, ICU, TCU

MODE 2: Pure Software (CI/CD Mode)
  vcan0 ─── vCVC ─── vFZC ─── vRZC ─── vSC ─── BCM ─── ICU ─── TCU ─── Gateway
  (All 7 ECUs + gateway as processes, no hardware needed)

MODE 3: Partial Hardware (Development Mode)
  Real CAN Bus ─── [1-4 physical ECUs] ─── CANable(PC)
                                              │
                              Docker: remaining ECUs simulated
```

### Additional Systems

| System | Hardware | Purpose |
|--------|----------|---------|
| Edge Gateway | Raspberry Pi 4 (2GB) | Cloud telemetry, ML inference, fault injection GUI |
| CAN Analyzer (Pi) | CANable 2.0 (USB-CAN) | Bus monitoring, SocketCAN interface for Pi |
| CAN Bridge (PC) | CANable 2.0 (USB-CAN) | Bridge real CAN bus to Docker simulated ECUs |
| Cloud | AWS IoT Core + Grafana | Real-time dashboard, data lake, alerts |
| vECU Runtime | Docker + SocketCAN | Run simulated ECUs with same C source code |

### 16 Demo Scenarios (12 Safety + 3 Simulated ECU + 1 SAP QM)

| # | Scenario | Trigger | Observable Result | ECUs Involved |
|---|----------|---------|-------------------|---------------|
| 1 | Normal driving | Pedal input | Motor spins, ICU shows speed, BCM lights on | CVC, RZC, ICU, BCM |
| 2 | Pedal sensor disagreement | Dual sensor mismatch | Limp mode, ICU warning light | CVC, RZC, ICU |
| 3 | Pedal sensor failure | Sensor disconnected | Motor stops, ICU fault display | CVC, RZC, ICU |
| 4 | Object detected | Lidar < threshold | Brake engages, BCM hazard lights | FZC, CVC, RZC, BCM |
| 5 | Motor overcurrent | Current sensor trip | Motor stops, DTC stored in TCU | RZC, CVC, TCU |
| 6 | Motor overtemp | Temp sensor trip | Motor derates then stops, ICU temp warning | RZC, CVC, ICU |
| 7 | Steering fault | Angle sensor lost | Servo returns to center, ICU steering warning | FZC, CVC, ICU |
| 8 | CAN bus loss | Bus disconnected | Safety Controller kills system | SC, all |
| 9 | ECU hang | Missing heartbeat | Safety Controller kills system | SC, all |
| 10 | E-stop pressed | Button pressed | Broadcast stop, everything stops, BCM hazards | CVC, all, BCM |
| 11 | ML anomaly alert | Abnormal current pattern | Cloud dashboard alarm fires | Pi, AWS |
| 12 | CVC vs Safety disagree | Injected conflict | Safety Controller wins (kill relay) | CVC, SC |
| 13 | UDS diagnostic session | TCU receives 0x10 request | TCU responds, reads live data via 0x22 | TCU, all |
| 14 | DTC read/clear cycle | TCU 0x19/0x14 service | TCU lists stored faults, clears on command | TCU |
| 15 | Night driving mode | Speed > 0 in BCM | BCM auto-enables headlights, ICU shows icon | BCM, ICU, RZC |
| 16 | DTC → SAP QM workflow | Motor overcurrent DTC | Q-Meldung created, 8D report auto-generated | RZC, TCU, Pi, SAP QM |

---

## Hardware Bill of Materials

| # | Item | Qty | Unit $ | Total $ |
|---|------|-----|--------|---------|
| | **MCUs** | | | |
| 1 | STM32G474RE Nucleo-64 | 3 | $20 | $60 |
| 2 | TI TMS570LC43x LaunchPad (LAUNCHXL2-570LC43) | 1 | $62 | $62 |
| 3 | Raspberry Pi 4 Model B (2GB) | 1 | $45 | $45 |
| | **CAN Bus** | | | |
| 4 | Adafruit CAN Pal (TJA1051T/3 transceiver) | 4 | $4 | $16 |
| 5 | SN65HVD230 breakout (for TMS570) | 1 | $5 | $5 |
| 6 | CANable 2.0 (USB-CAN, for Pi) | 1 | $35 | $35 |
| 6b | CANable 2.0 (USB-CAN, for PC — CAN bridge to simulated ECUs) | 1 | $35 | $35 |
| 7 | 22 AWG twisted pair wire (25 ft) | 1 | $10 | $10 |
| 8 | 120 ohm resistors (bus termination) | 2 | $0.10 | $1 |
| | **Sensors** | | | |
| 9 | AS5048A magnetic angle sensor + magnet | 3 | $15 | $45 |
| 10 | TFMini-S lidar (UART, 0.1-12m, 100Hz) | 1 | $25 | $25 |
| 11 | ACS723 current transducer | 1 | $8 | $8 |
| 12 | NTC 10K thermistors | 3 | $2 | $6 |
| | **Actuators** | | | |
| 13 | 12V brushed DC motor with encoder | 1 | $25 | $25 |
| 14 | BTS7960 H-bridge motor driver | 1 | $10 | $10 |
| 15 | Metal gear servo (brake) | 1 | $20 | $20 |
| 16 | Metal gear servo (steering) | 1 | $20 | $20 |
| | **Safety Hardware** | | | |
| 17 | TPS3823-33DBVT external watchdog IC | 4 | $1.50 | $6 |
| 18 | 30A automotive relay (kill relay) | 1 | $8 | $8 |
| 19 | Industrial E-stop mushroom button | 1 | $8 | $8 |
| 20 | IRLZ44N logic-level MOSFET (relay driver) | 1 | $1 | $1 |
| 21 | 1N4007 flyback diode | 1 | $0.10 | $1 |
| | **UI / Indicators** | | | |
| 22 | SSD1306 OLED 128x64 (I2C) | 1 | $4 | $4 |
| 23 | LEDs (red + green, for fault panel) | 8 | $0.20 | $2 |
| 24 | Piezo buzzer | 1 | $2 | $2 |
| | **Power** | | | |
| 25 | 12V/5A bench power supply | 1 | $20 | $20 |
| 26 | Buck converters (12V→5V, 12V→3.3V) | 3 | $4 | $12 |
| 27 | Raspberry Pi USB-C power supply | 1 | $10 | $10 |
| 28 | MicroSD card 32GB (for Pi) | 1 | $10 | $10 |
| | **Infrastructure** | | | |
| 29 | Mounting board (acrylic or aluminum) | 1 | $20 | $20 |
| 30 | Protoboards + standoffs | 4 | $5 | $20 |
| 31 | Wire kit + JST/Molex connectors | 1 | $25 | $25 |
| 32 | Heat shrink + cable labels | 1 | $10 | $10 |
| | **Test Equipment (optional)** | | | |
| 33 | Rigol DS1054Z oscilloscope | 1 | $400 | $400 |
| | | | **Total** | **~$977** |
| | | | **Without scope** | **~$577** |

**Budget**: $2,000 — leaves ~$1,023 for shipping, spares, and upgrades (e.g., RPLidar A1 at $100 for 360-degree scanning).

---

## Software Toolchains (All Free)

| MCU | IDE | HAL / Drivers | Debug |
|-----|-----|---------------|-------|
| STM32G474 | STM32CubeIDE | STM32 HAL + CubeMX | Onboard ST-LINK/V3 |
| TMS570LC43x | Code Composer Studio | HALCoGen (TUV-certified process) | Onboard XDS110 |
| Simulated ECUs | VS Code + GCC | POSIX SocketCAN | Docker, GDB |
| Raspberry Pi | VS Code + Python | python-can, paho-mqtt, scikit-learn | SSH |

## Software Architecture — AUTOSAR-like Layered BSW

All physical ECUs (CVC, FZC, RZC) follow an AUTOSAR Classic-inspired layered architecture. This is not a certified AUTOSAR stack — it's an architectural pattern that demonstrates understanding of the BSW structure.

```
┌─────────────────────────────────────────────────────────┐
│  APPLICATION LAYER (SWCs — Software Components)         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│  │Swc_Pedal │ │Swc_Steer │ │Swc_Motor │ │Swc_Safety│  │
│  │          │ │          │ │          │ │          │  │
│  │Rte_Read()│ │Rte_Read()│ │Rte_Read()│ │Rte_Read()│  │
│  │Rte_Write│ │Rte_Write│ │Rte_Write│ │Rte_Write│  │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘  │
├───────┼────────────┼────────────┼────────────┼─────────┤
│  RTE (Runtime Environment)                              │
│  Signal routing: SWC ports ←→ BSW services              │
│  Implemented as: function pointer table + signal buffer │
├─────────────────────────────────────────────────────────┤
│  BSW SERVICES LAYER                                     │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌──────┐ ┌──────┐ ┌──────┐  │
│  │ Com │ │ Dcm │ │ Dem │ │ WdgM │ │ BswM │ │ NvM  │  │
│  │     │ │     │ │     │ │      │ │      │ │(stub)│  │
│  └──┬──┘ └──┬──┘ └──┬──┘ └──┬───┘ └──┬───┘ └──┬───┘  │
├─────┼───────┼───────┼───────┼────────┼────────┼───────┤
│  ECU ABSTRACTION LAYER                                  │
│  ┌──────┐ ┌──────┐ ┌──────┐                            │
│  │CanIf │ │ PduR │ │IoHwAb│                            │
│  └──┬───┘ └──┬───┘ └──┬───┘                            │
├─────┼────────┼────────┼────────────────────────────────┤
│  MCAL (Microcontroller Abstraction Layer)               │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐    │
│  │ Can │ │ Spi │ │ Adc │ │ Pwm │ │ Gpt │ │ Dio │    │
│  └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘    │
├─────┼───────┼───────┼───────┼───────┼───────┼────────┤
│  HARDWARE (STM32G474RE / TMS570LC43x)                  │
└─────────────────────────────────────────────────────────┘
```

### BSW Module Summary

| Module | AUTOSAR Name | What It Does | Complexity |
|--------|-------------|-------------|------------|
| `Can` | CAN Driver (MCAL) | Raw FDCAN peripheral access | Wraps STM32 HAL |
| `Spi` | SPI Driver (MCAL) | Raw SPI peripheral access | Wraps STM32 HAL |
| `Adc` | ADC Driver (MCAL) | Raw ADC DMA scan | Wraps STM32 HAL |
| `Pwm` | PWM Driver (MCAL) | Timer-based PWM output | Wraps STM32 HAL |
| `Gpt` | GPT Driver (MCAL) | System tick, timing | Wraps STM32 HAL |
| `Dio` | DIO Driver (MCAL) | Digital I/O (GPIO) | Wraps STM32 HAL |
| `CanIf` | CAN Interface | HW-independent CAN API, PDU routing up | ~200 LOC |
| `PduR` | PDU Router | Routes PDUs between Com, Dcm, CanIf | ~150 LOC |
| `IoHwAb` | I/O HW Abstraction | Sensor/actuator access for SWCs | ~100 LOC |
| `Com` | Communication | Signal packing/unpacking, I-PDU groups, deadlines | ~400 LOC |
| `Dcm` | Diagnostic Comm Mgr | UDS service dispatch, session management | ~500 LOC |
| `Dem` | Diagnostic Event Mgr | DTC storage, status bits, fault debouncing | ~300 LOC |
| `WdgM` | Watchdog Manager | Supervised entities, alive/deadline monitoring | ~200 LOC |
| `BswM` | BSW Mode Manager | ECU state/mode management, rule-based actions | ~150 LOC |
| `NvM` | NV Memory Manager | Stub — DTC persistence placeholder | ~50 LOC (stub) |
| `Rte` | Runtime Environment | Signal buffer, port connections, runnable scheduling | ~300 LOC |

**Total shared BSW: ~2,500 LOC** — reused across all 3 STM32 ECUs.

### POSIX Portability (Simulated ECUs)

The 3 simulated ECUs (BCM, ICU, TCU) use the **same C codebase** as physical ECUs, compiled for Linux with a POSIX MCAL abstraction layer. No SOME/IP — pure BSW + SocketCAN.
- **Can_Posix.c** replaces Can.c (STM32 MCAL) with Linux SocketCAN backend
- **CanIf, PduR, Com, Dcm, Dem** all reused unchanged on Linux
- BCM/ICU/TCU use Rte_Read/Rte_Write just like physical ECUs

```
Physical ECUs (AUTOSAR Classic)          Simulated ECUs (Same BSW, POSIX MCAL)
CVC ──┐                                 BCM ──┐
FZC ──┤── CAN 2.0B (500 kbps) ──────────┤── SocketCAN (vcan0 or CAN bridge)
RZC ──┤                                 ICU ──┤
SC  ──┘                                 TCU ──┘
```

---

## Phase Table

| Phase | Name | Status |
|-------|------|--------|
| 0 | Project Setup & Architecture Docs | DONE |
| 1 | Safety Concept (HARA, Safety Goals, FSC) | DONE |
| 2 | Safety Analysis (FMEA, DFA, Hardware Metrics) | DONE |
| 3 | Requirements & System Architecture | DONE |
| 4 | CAN Protocol & HSI Design | DONE |
| 5 | Shared BSW Layer (16 AUTOSAR-like modules) | DONE |
| 6 | CVC Firmware (6 SWCs, 88 tests) | DONE |
| 7 | FZC Firmware (steering, braking, lidar) | DONE |
| 8 | RZC Firmware (motor control, current, battery) | DONE |
| 9 | Safety Controller (TMS570 independent monitor) | DONE |
| 10 | BCM, ICU, TCU Firmware (simulated ECUs) | DONE |
| 11 | POSIX Port + Docker SIL (7 ECUs containerized) | DONE |
| 12 | DBC File + Plant Simulator (physics models) | DONE |
| 13 | CAN-to-MQTT Gateway + WebSocket Bridge | DONE |
| 14 | Live Telemetry Dashboard (/embedded) | DONE |
| 15 | SAP QM Mock API (OData endpoints) | DONE |
| 16 | Edge ML Anomaly Detection + Fault Injection | DONE |
| 17 | VPS Deployment + Live Demo | DONE |
| 18 | Physical Hardware Build + HIL Testing | PENDING |

---

## Phase 0: Project Setup & Architecture Docs (DONE)

- [x] Repository scaffold
- [x] .claude/rules/ — 28 rule files (embedded + ISO 26262)
- [x] .claude/hooks/ — lint-firmware, protect-files
- [x] .claude/skills/ — security-review, plan-feature, firmware-build
- [x] Git Flow branching (main → develop)
- [x] CLAUDE.md, PROJECT_STATE.md
- [x] Hardware feasibility research (STM32G474, TMS570, CAN bus, sensors)

### DONE Criteria
- [x] All rules in place
- [x] Git Flow configured
- [x] Hardware architecture verified feasible
- [x] Ready to start safety lifecycle

---

## Phase 1: Safety Concept (DONE)

- [x] Item definition (system boundary, functions, interfaces, environment)
  - [x] System: Zonal vehicle platform (7 ECUs: 4 physical + 3 simulated + edge gateway)
  - [x] Functions: drive-by-wire (pedal → motor), steering, braking, distance sensing
  - [x] Interfaces: CAN bus (500 kbps), lidar (UART), sensors (SPI/ADC), actuators (PWM/GPIO)
  - [x] Environment: indoor demo platform, 12V power, controlled conditions
- [x] Hazard Analysis and Risk Assessment (HARA)
  - [x] Identify all operational situations (6 situations)
  - [x] Identify all hazardous events (16 hazardous events)
  - [x] Rate each: Severity, Exposure, Controllability
  - [x] Assign ASIL per hazardous event
- [x] Safety goals (8 safety goals: SG-001 to SG-008)
- [x] Safe states definition (4 safe states per safety goal)
- [x] FTTI estimation per safety goal
- [x] Functional safety concept
  - [x] 23 safety mechanisms (SM-001 to SM-023)
  - [x] Warning and degradation concept
  - [x] Operator warning strategy (OLED + buzzer + LEDs)
- [x] Safety plan
- [x] Functional safety requirements (25 FSRs: FSR-001 to FSR-025)

### Files
- `docs/safety/concept/item-definition.md`
- `docs/safety/concept/hara.md`
- `docs/safety/concept/safety-goals.md`
- `docs/safety/concept/functional-safety-concept.md`
- `docs/safety/requirements/functional-safety-reqs.md`
- `docs/safety/plan/safety-plan.md`

### DONE Criteria
- [x] All hazardous events identified and rated
- [x] Every safety goal has a safe state and FTTI
- [x] Functional safety concept covers all safety goals

---

## Phase 2: Safety Analysis (DONE)

- [x] System-level FMEA (50 failure modes across all components)
  - [x] CVC failures (pedal sensor, OLED, CAN TX/RX, E-stop)
  - [x] FZC failures (steering servo, brake servo, lidar, angle sensor, buzzer)
  - [x] RZC failures (motor driver, current sensor, temp sensor, encoder, battery)
  - [x] SC failures (CAN listen, heartbeat logic, kill relay, watchdog)
  - [x] CAN bus failures (open, short, stuck, delayed, corrupted)
  - [x] Power failures (12V loss, 5V loss, 3.3V loss, ground fault)
- [x] FMEDA — failure rate classification, diagnostic coverage
  - [x] TMS570 failure rates from TI safety manual
  - [x] STM32G474 failure rates from ST safety manual
- [x] SPFM and LFM calculation per ECU (all exceed targets)
- [x] PMHF estimation (5.2 FIT total < 10 FIT target)
- [x] Dependent Failure Analysis (DFA)
  - [x] Common cause: 6 CCFs identified with mitigations
  - [x] Cascading: 5 CFs identified with mitigations
  - [x] Beta-factor analysis, independence arguments
- [x] ASIL decomposition decisions (2 decompositions documented)

### Files
- `docs/safety/analysis/fmea.md`
- `docs/safety/analysis/dfa.md`
- `docs/safety/analysis/hardware-metrics.md`
- `docs/safety/analysis/asil-decomposition.md`

### DONE Criteria
- [x] Every component has failure modes analyzed
- [x] SPFM, LFM, PMHF numbers calculated
- [x] DFA covers all cross-ECU dependencies

---

## Phase 3: Requirements & System Architecture (DONE)

- [x] Stakeholder requirements (32 STK requirements: STK-001 to STK-032)
- [x] System requirements (56 SYS requirements: SYS-001 to SYS-056)
- [x] Technical Safety Requirements (51 TSR: TSR-001 to TSR-051)
- [x] TSR allocation to zonal ECUs
- [x] Software Safety Requirements (81 SSR across 4 ECUs)
  - [x] CVC: SSR-CVC-001..023
  - [x] FZC: SSR-FZC-001..024
  - [x] RZC: SSR-RZC-001..017
  - [x] SC: SSR-SC-001..017
- [x] Hardware Safety Requirements (25 HSR across 4 ECUs)
- [x] System architecture document (10 element categories, 24 CAN messages, state machine)
- [x] Software architecture per ECU (modules, interfaces, MPU config, task scheduling)
- [x] BSW architecture (16 modules with full API signatures)
- [x] Per-ECU SW requirements (197 SWR across 10 documents)
  - [x] SWR-CVC: 35 reqs | SWR-FZC: 32 reqs | SWR-RZC: 30 reqs | SWR-SC: 26 reqs
  - [x] SWR-BCM: 12 reqs | SWR-ICU: 10 reqs | SWR-TCU: 15 reqs | SWR-BSW: 27 reqs
  - [x] SWR-GW: 6 reqs (gateway: MQTT, CAN acquisition, ML, SAP QM)
  - [x] SWR-ALL: 4 reqs (cross-cutting: MISRA, no malloc, WCET, flash budget)
- [x] Traceability matrix (475 total traced: SG → FSR → TSR → SSR → SWR → module → test)
- [x] CI traceability enforcement (`trace-gen.py --check` blocks merge on broken links or untested SWR)
- [x] Suspect-link detection (`suspect-links.py` reports downstream impact on PRs)

### Files
- `docs/aspice/system/stakeholder-requirements.md`
- `docs/aspice/system/system-requirements.md`
- `docs/safety/requirements/technical-safety-reqs.md`
- `docs/safety/requirements/sw-safety-reqs.md`
- `docs/safety/requirements/hw-safety-reqs.md`
- `docs/aspice/system/system-architecture.md`
- `docs/aspice/software/sw-architecture/sw-architecture.md`
- `docs/aspice/software/sw-architecture/bsw-architecture.md`
- `docs/aspice/software/sw-requirements/SWR-CVC.md`
- `docs/aspice/software/sw-requirements/SWR-FZC.md`
- `docs/aspice/software/sw-requirements/SWR-RZC.md`
- `docs/aspice/software/sw-requirements/SWR-SC.md`
- `docs/aspice/software/sw-requirements/SWR-BCM.md`
- `docs/aspice/software/sw-requirements/SWR-ICU.md`
- `docs/aspice/software/sw-requirements/SWR-TCU.md`
- `docs/aspice/software/sw-requirements/SWR-BSW.md`
- `docs/aspice/software/sw-requirements/SWR-GW.md`
- `docs/aspice/software/sw-requirements/SWR-ALL.md`
- `docs/aspice/traceability/traceability-matrix.md`
- `scripts/trace-gen.py` (954-line V-model traceability generator, pure stdlib)
- `scripts/suspect-links.py` (change impact detection for PRs)
- `.github/workflows/traceability.yml` (CI: blocking check + suspect-link analysis)

### DONE Criteria
- [x] Every safety goal traces to TSR → SSR → architecture element
- [x] Traceability matrix complete — 475 requirements, 0 broken links, 0 untested, SWR 100%
- [x] CI enforcement green — `trace-gen.py --check` blocks merge on gaps (ADR-013)

---

## Phase 4: CAN Protocol & HSI Design (DONE)

- [x] CAN message matrix (31 message types, 16 E2E-protected, 24% bus load)
  - [x] CVC → FZC: steer request, brake request (10 ms cycle)
  - [x] CVC → RZC: torque request (10 ms cycle)
  - [x] CVC → ALL: vehicle state, E-stop broadcast
  - [x] FZC → CVC: steering angle, brake status, lidar distance (20 ms cycle)
  - [x] RZC → CVC: motor status, current, temp, battery voltage (20 ms cycle)
  - [x] ALL → SC: heartbeat (alive counter, 50 ms cycle)
  - [x] FZC → CVC/RZC: emergency brake request (event-driven)
  - [x] BCM → ALL: light status, indicator state, door lock status (100 ms cycle)
  - [x] ICU → CVC: DTC acknowledgment (event-driven)
  - [x] TCU ↔ ALL: UDS request/response (0x7DF/0x7E0-0x7E6 → 0x7E8-0x7EE, on-demand)
- [x] E2E protection design (CRC-8 SAE J1850, alive counter, data ID per safety message)
- [x] Interface Control Document (22 interfaces with register-level configs)
- [x] Hardware-Software Interface per ECU (full HSI specification)
  - [x] CVC: pin mapping, peripheral config, memory map, MPU config
  - [x] FZC: pin mapping, peripheral config, memory map, MPU config
  - [x] RZC: pin mapping, peripheral config, memory map, MPU config
  - [x] SC: pin mapping, DCAN1 config, lockstep, ESM
- [x] Simulated ECU CAN interface specification (vECU architecture)
  - [x] POSIX SocketCAN abstraction layer (Can_Posix.c, Gpt_Posix.c, etc.)
  - [x] Docker networking and container structure
  - [x] CAN bridge configuration: CANable on PC ↔ real CAN bus
  - [x] vcan0 setup for pure-software CI/CD mode
  - [x] GitHub Actions CI/CD workflow
- [x] Hardware requirements (33 HWR: HWR-001 to HWR-033)
- [x] Hardware design (per-ECU circuit designs with ASCII schematics)
- [x] Bill of Materials (74 line items, $537/$937 total, 3-phase procurement)
- [x] Pin mapping (53 pins across 4 ECUs with conflict checks)

### Files
- `docs/aspice/system/can-message-matrix.md`
- `docs/aspice/system/interface-control-doc.md`
- `docs/safety/requirements/hsi-specification.md`
- `docs/aspice/hardware-eng/hw-requirements.md`
- `docs/aspice/hardware-eng/hw-design.md`
- `docs/aspice/software/sw-architecture/vecu-architecture.md`
- `hardware/pin-mapping.md`
- `hardware/bom.md`

### DONE Criteria
- [x] Every CAN message defined with ID, signals, timing, E2E protection
- [x] HSI complete for all 4 physical ECUs
- [x] Pin assignments verified against Nucleo/LaunchPad schematics
- [x] Simulated ECU CAN interface specified

---

## Phase 5: Shared BSW Layer (AUTOSAR-like)

Build the reusable BSW modules first — these are shared across all 3 STM32 ECUs.

### 5a: MCAL Drivers (wrapping STM32 HAL)
- [x] `bsw/mcal/Can.c` — FDCAN1 init, TX, RX (20 tests)
- [x] `bsw/mcal/Spi.c` — SPI init, transfer (14 tests)
- [x] `bsw/mcal/Adc.c` — ADC DMA scan (13 tests)
- [x] `bsw/mcal/Pwm.c` — Timer PWM output (14 tests)
- [x] `bsw/mcal/Dio.c` — Digital I/O (12 tests)
- [x] `bsw/mcal/Gpt.c` — System tick, timing (14 tests)
- [x] Platform abstraction headers: Platform_Types.h, Std_Types.h, ComStack_Types.h, Compiler.h

### 5b: ECU Abstraction Layer
- [x] `bsw/ecual/CanIf.c` — hardware-independent CAN interface (9 tests)
- [x] `bsw/ecual/PduR.c` — PDU Router (8 tests)
- [x] `bsw/ecual/IoHwAb.c` — I/O Hardware Abstraction (19 tests)

### 5c: BSW Services
- [x] `bsw/services/Com.c` — Communication module (9 tests)
- [x] `bsw/services/Dcm.c` — Diagnostic Communication Manager (14 tests)
- [x] `bsw/services/Dem.c` — Diagnostic Event Manager (8 tests)
- [x] `bsw/services/WdgM.c` — Watchdog Manager (8 tests)
- [x] `bsw/services/BswM.c` — BSW Mode Manager (14 tests)
- [x] `bsw/services/E2E.c` — E2E Protection (23 tests)

### 5d: RTE (Runtime Environment)
- [x] `bsw/rte/Rte.c` — Runtime Environment (14 tests)
  - [x] Signal buffer with copy semantics
  - [x] Runnable scheduling with priority-ordered dispatch
  - [x] WdgM checkpoint integration
- [ ] RTE configuration per ECU (CVC, FZC, RZC) — done in Phase 6-8

### Files
- `firmware/shared/bsw/mcal/` — Can.c, Spi.c, Adc.c, Pwm.c, Dio.c, Gpt.c
- `firmware/shared/bsw/ecual/` — CanIf.c, PduR.c, IoHwAb.c
- `firmware/shared/bsw/services/` — Com.c, Dcm.c, Dem.c, WdgM.c, BswM.c, E2E.c
- `firmware/shared/bsw/rte/` — Rte.c, Rte_Cfg_Cvc.c, Rte_Cfg_Fzc.c, Rte_Cfg_Rzc.c
- `firmware/shared/bsw/include/` — all headers, Platform_Types.h, Std_Types.h, ComStack_Types.h

### DONE Criteria
- [x] All 16 BSW modules implemented with TDD (test-first)
- [x] 212 unit tests across original 16 modules (all @verifies tagged)
  - Note: Uart.c (15 tests) added during Phase 7, Can_Posix.c (14 tests) added during Phase 10 — total BSW now 18 modules / 241 tests
- [x] Can → CanIf → PduR → Com chain verified in unit tests
- [x] Dcm responds to UDS requests (0x10, 0x22, 0x3E) with NRC handling
- [x] Dem stores DTCs with counter-based debouncing
- [x] WdgM feeds watchdog only when supervised entities are healthy
- [x] Rte_Read / Rte_Write pass signals with copy semantics
- [x] BswM forward-only mode state machine (STARTUP → RUN → DEGRADED → SAFE_STOP → SHUTDOWN)
- [x] E2E CRC-8 + alive counter + Data ID protection
- [x] IoHwAb wraps all MCAL calls with engineering unit conversion
- [ ] BSW compiles for STM32 target (deferred to hardware-in-hand)
- [x] Same BSW links into CVC, FZC, RZC projects (all 6 ECUs now use shared BSW)

---

## Phase 6: Firmware — Central Vehicle Computer (STM32G474) ✅ DONE

- [x] Application SWCs (Software Components)
  - [x] `Swc_Pedal.c` — dual sensor read, plausibility check, stuck detect, torque map, ramp limit, mode limit (482 LOC, 25 tests)
  - [x] `Swc_VehicleState.c` — 6-state machine, 11 events, const transition table (356 LOC, 20 tests)
  - [x] `Swc_Dashboard.c` — OLED display with 200ms refresh, fault resilience (314 LOC, 8 tests)
  - [x] `Swc_EStop.c` — debounce, latch, 4× CAN broadcast, fail-safe (158 LOC, 10 tests)
  - [x] `Swc_Heartbeat.c` — 50ms TX, alive counter, FZC/RZC timeout detect, recovery (216 LOC, 15 tests)
  - [x] `Ssd1306.c` — I2C OLED driver, 5x7 font, init/clear/cursor/string (338 LOC, 10 tests)
- [x] `Cvc_Cfg.h` — unified config: 31 RTE signals, 8 TX/6 RX PDUs, 18 DTCs, E2E IDs, enums (185 LOC)
- [x] RTE configuration: `Rte_Cfg_Cvc.c` — 31 signals, 8 runnables with priorities (97 LOC)
- [x] Com configuration: `Com_Cfg_Cvc.c` — 17 signals, 8 TX + 6 RX PDUs (118 LOC)
- [x] Dcm configuration: `Dcm_Cfg_Cvc.c` — 4 DIDs (ECU ID, HW/SW ver, state), callbacks (122 LOC)
- [x] `main.c` — BSW init sequence, self-test, 1ms/10ms/100ms tick loop (338 LOC)
- [ ] MISRA compliance pass (deferred to CI with target toolchain)
- [ ] CubeMX hardware configuration (deferred to hardware-in-hand)

### Files (23 files, ~5,930 LOC)
- `firmware/cvc/src/` — main.c, Swc_Pedal.c, Swc_VehicleState.c, Swc_Dashboard.c, Swc_EStop.c, Swc_Heartbeat.c, Ssd1306.c
- `firmware/cvc/include/` — Cvc_Cfg.h, Swc_Pedal.h, Swc_VehicleState.h, Swc_Dashboard.h, Swc_EStop.h, Swc_Heartbeat.h, Ssd1306.h
- `firmware/cvc/cfg/` — Rte_Cfg_Cvc.c, Com_Cfg_Cvc.c, Dcm_Cfg_Cvc.c
- `firmware/cvc/test/` — test_Swc_Pedal.c, test_Swc_VehicleState.c, test_Swc_EStop.c, test_Swc_Heartbeat.c, test_Swc_Dashboard.c, test_Ssd1306.c

### DONE Criteria
- [x] Dual pedal reading with plausibility check, stuck detect, zero-torque latch
- [x] State machine: 6 states, 11 events, all transitions tested (20 tests)
- [x] CAN messages: E-stop broadcast, heartbeat TX/RX, vehicle state, torque, steering, brake
- [x] OLED displays vehicle state, speed, pedal position, fault mask
- [x] E-stop: debounce, permanent latch, 4× broadcast, fail-safe on read error
- [x] Dem: 18 DTCs defined, reported on all fault types
- [x] 88 unit tests with full @verifies traceability tags
- [x] No malloc, no banned functions, all static allocation

---

## Phase 7: Firmware — Front Zone Controller (STM32G474) ✅ DONE

- [x] Application SWCs — **6 SWCs** (plan had 5 — added Swc_Buzzer)
  - [x] `Swc_Steering.c` — servo control via Rte_Write(Pwm), angle feedback via Rte_Read(Spi), rate limiting, angle limits, return-to-center on fault (30 tests)
  - [x] `Swc_Brake.c` — servo control via Rte_Write(Pwm), brake force mapping, auto-brake on Com Rx timeout (22 tests)
  - [x] `Swc_Lidar.c` — TFMini-S frame parser via Rte_Read(Uart), distance filtering, threshold check (warning/brake/emergency) (25 tests)
  - [x] `Swc_FzcSafety.c` — local plausibility checks, sensor timeout detection, Dem_ReportErrorStatus on failures (8 tests)
  - [x] `Swc_Heartbeat.c` — alive counter TX via Com (12 tests)
  - [x] **`Swc_Buzzer.c`** — audible warning output for proximity/fault alerts (8 tests)
- [x] Total: 105 SWC tests + Uart.c in shared BSW (15 tests developed for FZC lidar) = **120 total attributable**
- [x] MCAL extension: `Uart.c` — USART DMA RX for TFMini-S lidar (moved to shared BSW MCAL, not FZC-specific)
- [x] RTE configuration: `Rte_Cfg_Fzc.c`
- [x] Com configuration: `Com_Cfg_Fzc.c`
- [x] Dcm configuration: **`Dcm_Cfg_Fzc.c`** (added — not in original plan)
- [x] `main.c` — BSW init, RTE init, 10ms tick loop
- [ ] MISRA compliance pass (deferred to CI with target toolchain)
- [ ] CubeMX hardware configuration (deferred to hardware-in-hand)

### Files
- `firmware/fzc/src/` — Swc_Steering.c, Swc_Brake.c, Swc_Lidar.c, Swc_FzcSafety.c, Swc_Heartbeat.c, Swc_Buzzer.c, main.c
- `firmware/fzc/include/` — headers for all SWCs, Fzc_Cfg.h
- `firmware/fzc/cfg/` — Rte_Cfg_Fzc.c, Com_Cfg_Fzc.c, Dcm_Cfg_Fzc.c
- `firmware/fzc/test/` — test files for all 6 SWCs
- `firmware/shared/bsw/mcal/Uart.c` — shared MCAL (lidar UART went into shared BSW, not FZC-specific)

### DONE Criteria
- [x] Steering servo tracks angle command through Rte → Pwm
- [x] Brake servo responds to brake request via Com Rx
- [x] Lidar reads distance, triggers emergency brake at threshold
- [x] Return-to-center on steering sensor fault, DTC stored via Dem
- [x] Auto-brake on Com Rx timeout detection
- [x] Buzzer audible warning on proximity/fault events
- [x] 105 SWC unit tests with full @verifies traceability tags
- [x] No malloc, no banned functions, all static allocation

---

## Phase 8: Firmware — Rear Zone Controller (STM32G474) ✅ DONE

- [x] Application SWCs — **7 SWCs** (plan had 6 — added Swc_Encoder)
  - [x] `Swc_Motor.c` — torque request via Rte_Read(Com) → PWM via Rte_Write(Pwm), ramp limiting, direction control (28 tests)
  - [x] `Swc_CurrentMonitor.c` — overcurrent detection via Rte_Read(Adc), cutoff threshold, filtering, Dem_ReportErrorStatus on trip (18 tests)
  - [x] `Swc_TempMonitor.c` — derating curve via Rte_Read(Adc), over-temp shutdown, Dem_ReportErrorStatus (12 tests)
  - [x] `Swc_Battery.c` — voltage monitoring via Rte_Read(Adc), SOC estimate (simple voltage table) (8 tests)
  - [x] `Swc_RzcSafety.c` — motor safety checks, emergency brake response via Com Rx, safe state transition via BswM (10 tests)
  - [x] `Swc_Heartbeat.c` — alive counter TX via Com (10 tests)
  - [x] **`Swc_Encoder.c`** — motor encoder feedback, speed/position calculation (15 tests)
- [x] Total: **101 SWC tests**
- [x] RTE configuration: `Rte_Cfg_Rzc.c`
- [x] Com configuration: `Com_Cfg_Rzc.c`
- [x] Dcm configuration: **`Dcm_Cfg_Rzc.c`** (added — not in original plan)
- [x] `main.c` — BSW init, RTE init, 10ms tick loop
- [ ] MISRA compliance pass (deferred to CI with target toolchain)
- [ ] CubeMX hardware configuration (deferred to hardware-in-hand)

### Files
- `firmware/rzc/src/` — Swc_Motor.c, Swc_CurrentMonitor.c, Swc_TempMonitor.c, Swc_Battery.c, Swc_RzcSafety.c, Swc_Heartbeat.c, Swc_Encoder.c, main.c
- `firmware/rzc/include/` — headers for all SWCs, Rzc_Cfg.h
- `firmware/rzc/cfg/` — Rte_Cfg_Rzc.c, Com_Cfg_Rzc.c, Dcm_Cfg_Rzc.c
- `firmware/rzc/test/` — test files for all 7 SWCs

### DONE Criteria
- [x] Motor responds to torque request via full BSW stack (Com → Rte → Swc_Motor → Rte → Pwm)
- [x] Overcurrent protection cuts motor within 10 ms, DTC stored via Dem
- [x] Over-temp derating and shutdown working, DTC stored via Dem
- [x] Emergency brake command stops motor via Com Rx
- [x] Battery voltage reported over CAN via Com TX
- [x] Encoder feedback for speed/position measurement
- [x] 101 SWC unit tests with full @verifies traceability tags
- [x] No malloc, no banned functions, all static allocation

---

## Phase 9: Firmware — Safety Controller (TMS570LC43x) ✅ DONE

**Note**: The Safety Controller does NOT use the AUTOSAR-like BSW stack. It runs a minimal, independent bare-metal firmware. This is intentional — the Safety Controller must be simple, auditable, and architecturally independent from the zone controllers it monitors. Diverse software architecture = real ISO 26262 principle.

- [x] **10 source modules + 2 config headers**, bare-metal (no AUTOSAR)
  - [x] `sc_e2e.c` — E2E CRC + alive counter validation for safety messages (9 tests)
  - [x] `sc_can.c` — CAN listen-only mode, message reception and dispatch (11 tests)
  - [x] `sc_heartbeat.c` — alive counter tracking per ECU, timeout detection (11 tests)
  - [x] `sc_plausibility.c` — cross-plausibility checks (torque vs current, etc.) (10 tests)
  - [x] `sc_relay.c` — kill relay control via GPIO → MOSFET → relay, energize-to-run pattern (11 tests)
  - [x] `sc_led.c` — fault LED panel, per-ECU health indicators (4 tests)
  - [x] `sc_watchdog.c` — external watchdog feed, safety-gated (4 tests)
  - [x] `sc_esm.c` — Error Signaling Module, lockstep CPU error handling (6 tests)
  - [x] `sc_selftest.c` — startup self-test sequence (10 tests)
  - [x] `sc_main.c` — init, main loop, state machine (no unit test — integration-tested)
  - [x] Config: `sc_types.h`, `sc_cfg.h`
- [x] Total: **76 tests**, 26 SWRs, 17 SSRs
- [x] Safety Controller state machine (INIT → SELFTEST → MONITORING → FAULT → KILL)

### Files
- `firmware/sc/src/` — sc_main.c, sc_can.c, sc_heartbeat.c, sc_plausibility.c, sc_relay.c, sc_led.c, sc_watchdog.c, sc_esm.c, sc_selftest.c, sc_e2e.c
- `firmware/sc/include/` — sc_types.h, sc_cfg.h, headers for all modules
- `firmware/sc/test/` — test files for all modules (except sc_main.c)

### DONE Criteria
- [x] Detects missing heartbeat within 100 ms
- [x] Kills system on CAN silence or ECU hang
- [x] LED panel shows per-ECU health (green/red)
- [x] External watchdog resets Safety Controller if it hangs
- [x] Lockstep CPU error detection via ESM module
- [x] E2E validation on all received safety messages
- [x] Startup self-test before entering monitoring mode
- [x] 76 unit tests with full @verifies traceability tags
- [x] No AUTOSAR dependency — fully independent bare-metal code

---

## Phase 10: Simulated ECUs — BCM, ICU, TCU (Docker + SocketCAN) ✅ DONE

Simulated ECUs use **pure BSW + SocketCAN** — no SOME/IP or vsomeip. Same C codebase as physical ECUs, compiled for Linux with a POSIX MCAL abstraction layer. SOME/IP was dropped as unjustified for portfolio value.

### 10.0: POSIX MCAL Abstraction
- [x] `Can_Posix.c` — SocketCAN backend with same `Can_Write()` / `Can_MainFunction_Read()` API as STM32 MCAL (14 tests)
- [x] `Gpt_Posix.c` — POSIX timer abstraction
- [x] `Dio_Posix.c` — GPIO simulation via shared memory / flags
- [x] `Adc_Posix.c` — ADC simulation stub
- [x] `Pwm_Posix.c` — PWM simulation stub
- [x] `Spi_Posix.c` — SPI simulation stub
- [x] `Can_Posix.h` — shared header
- [x] Compile-time switch: `#ifdef PLATFORM_POSIX` vs `#ifdef PLATFORM_STM32`
- [x] Entire BSW stack (CanIf, PduR, Com, Dcm, Dem) reused unchanged on Linux
- [x] 7 files + header + 14 tests

### 10.1: Docker Infrastructure
- [x] `docker/Dockerfile.vecu` — Ubuntu base, build-essential, can-utils, SocketCAN headers
- [x] `docker/docker-compose.yml` — all 3 simulated ECUs + vcan setup
- [x] `Makefile.posix` — build target for all 3 simulated ECUs for Linux
- [x] `scripts/vecu-start.sh` — create vcan0, bring up, launch containers
- [x] `scripts/vecu-stop.sh` — tear down containers and vcan

### 10.2: Body Control Module (BCM)
- [x] `Swc_Lights.c` — auto headlight logic (speed > 0 → lights on) via Rte_Read(vehicle_speed)
- [x] `Swc_Indicators.c` — turn signals from steering angle, hazard on emergency
- [x] `Swc_DoorLock.c` — lock state management, auto-lock at speed
- [x] `bcm_main.c` — BSW init (Can_Posix), RTE init, 100 ms main loop
- [x] `Rte_Cfg_Bcm.c` — port mappings, runnable schedule
- [x] **21 tests, 12 SWRs**

### 10.3: Instrument Cluster Unit (ICU)
- [x] `Swc_Dashboard.c` — ncurses terminal UI: speedometer, temp gauge, battery, warnings
- [x] `Swc_DtcDisplay.c` — display active/stored DTCs from TCU via Rte_Read
- [x] `icu_main.c` — BSW init (Can_Posix), RTE init, 50 ms main loop
- [x] `Rte_Cfg_Icu.c` — port mappings (subscribe to ALL signal groups)
- [x] **24 tests, 10 SWRs, ncurses UI**

### 10.4: Telematics Control Unit (TCU)
- [x] `Swc_UdsServer.c` — UDS (ISO 14229) service handler: 0x10, 0x22, 0x14, 0x19, 0x3E, 0x7F
- [x] `Swc_DtcStore.c` — DTC storage (in-memory, max 64 DTCs, ISO 14229 status bits)
- [x] `Swc_Obd2Pids.c` — OBD-II PID handler: RPM (0x0C), speed (0x0D), temp (0x05), load (0x04)
- [x] `tcu_main.c` — BSW init (Can_Posix), event-driven main loop
- [x] `Rte_Cfg_Tcu.c` — port mappings
- [x] **35 tests, 15 SWRs**

### Files
- `firmware/bcm/src/`, `firmware/bcm/include/`, `firmware/bcm/cfg/`, `firmware/bcm/test/`
- `firmware/icu/src/`, `firmware/icu/include/`, `firmware/icu/cfg/`, `firmware/icu/test/`
- `firmware/tcu/src/`, `firmware/tcu/include/`, `firmware/tcu/cfg/`, `firmware/tcu/test/`
- `firmware/shared/bsw/mcal/Can_Posix.c`, `firmware/shared/bsw/mcal/Can_Posix.h`
- `firmware/shared/bsw/mcal/Gpt_Posix.c`, `Dio_Posix.c`, `Adc_Posix.c`, `Pwm_Posix.c`, `Spi_Posix.c`
- `docker/Dockerfile.vecu`, `docker/docker-compose.yml`
- `Makefile.posix`
- `scripts/vecu-start.sh`, `scripts/vecu-stop.sh`

### DONE Criteria
- [x] All 3 simulated ECUs build and run on Linux
- [x] BCM sends light/indicator/lock messages on vcan0
- [x] ICU displays live dashboard from all CAN traffic (ncurses)
- [x] TCU responds to UDS requests (0x10, 0x22, 0x19, 0x14)
- [x] Docker-compose brings up all 3 with single command
- [ ] CAN bridge mode connects simulated ECUs to real CAN bus — TODO:HARDWARE
- [x] 80 SWC tests across 3 ECUs + 14 POSIX MCAL tests = **94 total**
- [x] No SOME/IP — pure BSW + SocketCAN

---

## Phase 11: Edge Gateway — Raspberry Pi + Cloud + ML

### 11a: CAN Interface + Data Logging
- [ ] CANable 2.0 setup (flash candleLight firmware, gs_usb driver)
- [ ] SocketCAN configuration (`ip link set can0 up type can bitrate 500000`)
- [ ] python-can listener (Notifier + Listener pattern)
- [ ] CAN data logger (CSV/Parquet format for ML training)
- [ ] CAN message decoder (parse all message IDs per CAN matrix)

### 11b: Cloud Telemetry
- [ ] AWS IoT Core setup (thing, certificate, policy)
- [ ] MQTT publisher (paho-mqtt or AWS IoT SDK, TLS on port 8883)
- [ ] Telemetry schema (JSON: ECU status, sensor values, fault flags)
- [ ] Publish rate: 1 message per 5 seconds (batched, stays within free tier)
- [ ] AWS Timestream ingestion (time-series storage)
- [ ] Grafana dashboard
  - [ ] Per-ECU health status indicators
  - [ ] Motor current / temperature time series
  - [ ] Battery voltage gauge
  - [ ] Lidar distance graph
  - [ ] Anomaly score display
  - [ ] Fault event log

### 11c: Edge ML Models
- [ ] Data collection: run normal + fault scenarios, log CAN data
- [ ] Model 1: Motor Health Score (Random Forest)
  - [ ] Features: current mean/variance, temp trend, current-to-torque ratio
  - [ ] Train on PC (scikit-learn), export with joblib
  - [ ] Deploy on Pi, inference at 1 Hz
- [ ] Model 2: CAN Bus Anomaly Detection (Isolation Forest)
  - [ ] Features: message frequency per ID, payload byte distributions, timing jitter
  - [ ] Train on normal baseline, detect injected anomalies
  - [ ] Deploy on Pi, inference at 1 Hz
- [ ] Alert pipeline: anomaly score > threshold -> MQTT alert -> Grafana alarm
- [ ] Predictive quality trigger:
  - [ ] If Motor Health Score < 40%, generate **Soft DTC** `P1A40` (`Predictive Component Degradation`)
  - [ ] Publish Soft DTC event to MQTT topic `vehicle/dtc/soft`
  - [ ] Mark event severity as early-warning (non-hard-fault) with confidence score and feature snapshot

### 11d: SAP QM Integration (Simulated)

Demonstrates the full chain: **sensor fault -> DTC (Dem) -> CAN -> cloud -> SAP Quality Notification**.
Also demonstrates predictive chain: **ML health degradation -> Soft DTC -> cloud -> SAP QM early notification -> 8D initiation before hardware failure**.

This bridges embedded engineering with enterprise business processes — showing understanding of how vehicle field failures flow into the OEM's quality management system.

- [ ] SAP QM mock API (`gateway/sap_qm_mock/`)
  - [ ] Flask REST API simulating SAP QM BAPI endpoints
  - [ ] `POST /api/qm/notification` — create quality notification (Q-Meldung)
  - [ ] `GET /api/qm/notifications` — list all notifications
  - [ ] `PATCH /api/qm/notification/{id}` — update status (open → in process → completed)
  - [ ] In-memory storage (SQLite for persistence across restarts)
- [ ] DTC → Quality Notification mapping
  - [ ] DTC triggers notification automatically when cloud receives fault MQTT message
  - [ ] Soft DTC triggers notification automatically when cloud receives predictive MQTT message (`vehicle/dtc/soft`)
  - [ ] Notification types:
    - [ ] Q1 (Customer complaint): overcurrent, overtemp → field return
    - [ ] Q2 (Internal): sensor plausibility failure → production quality
    - [ ] Q2 (Internal): **predictive component degradation** from ML Soft DTC (`P1A40`)
    - [ ] Q3 (Supplier complaint): CAN transceiver fault → supplier issue
  - [ ] Notification content maps DTC data to SAP fields:

    | DTC Field | SAP QM Field | Example |
    |-----------|-------------|---------|
    | DTC number (3 bytes) | Defect code (QMNUM) | P0480 (Motor overcurrent) |
    | ECU ID | Equipment number (EQUNR) | RZC-001 |
    | Timestamp | Notification date (QMDAT) | 2026-02-21 |
    | Fault status | Priority (PRIOK) | 1 (safety-critical) |
    | Sensor values at fault | Long text (QMTXT) | "Current: 15.2A, Threshold: 10A" |
    | Safety goal reference | Catalog code (FECOD) | SG-003 (unintended acceleration) |

- [ ] SAP QM Dashboard (web UI)
  - [ ] Quality notification list (table: ID, type, DTC, ECU, status, date)
  - [ ] Notification detail view (full DTC context, sensor values, safety goal)
  - [ ] 8D report template auto-generated from DTC data
    - [ ] D1: Team (auto: "Zonal Platform Safety Team")
    - [ ] D2: Problem description (from DTC + sensor context)
    - [ ] D3: Containment (from safe state in safety concept)
    - [ ] D4: Root cause (placeholder — user fills in)
    - [ ] D5-D8: Corrective action workflow
  - [ ] Statistics: notifications per ECU, per DTC type, trend over time
- [ ] Integration pipeline:
  - [ ] MQTT topic `vehicle/dtc/new` → Lambda/rule → SAP QM mock API
  - [ ] MQTT topic `vehicle/dtc/soft` → Lambda/rule → SAP QM mock API (auto-create Q2 early warning)
  - [ ] Grafana panel linking to SAP QM dashboard for cross-reference
  - [ ] Traceability: DTC/Soft DTC → quality notification → 8D report → corrective action
### 11e: Fault Injection GUI
- [ ] Python GUI (tkinter or web-based Flask)
  - [ ] Buttons per demo scenario (1-16)
  - [ ] Live CAN bus status display
  - [ ] Scenario result logging
  - [ ] SAP QM notification feed (shows new Q-Meldung when DTC fires)
- [ ] CAN message injection (simulate faults from Pi)

### Files
- `gateway/can_listener.py`
- `gateway/cloud_publisher.py`
- `gateway/ml_inference.py`
- `gateway/fault_injector.py`
- `gateway/sap_qm_mock/app.py` — Flask API simulating SAP QM
- `gateway/sap_qm_mock/models.py` — notification data model
- `gateway/sap_qm_mock/dtc_mapping.py` — DTC → Q-notification mapping rules
- `gateway/sap_qm_mock/templates/` — dashboard HTML templates
- `gateway/sap_qm_mock/eight_d.py` — 8D report generator
- `gateway/models/` — trained ML model files
- `gateway/config.py` — CAN IDs, MQTT topics, thresholds, SAP QM config
- `gateway/requirements.txt`
- `docs/aspice/system/cloud-architecture.md`

### DONE Criteria
- [ ] Pi receives all CAN messages in real-time
- [ ] Grafana dashboard shows live telemetry
- [ ] Motor health model produces scores
- [ ] Anomaly detector flags injected faults
- [ ] DTC automatically creates SAP QM notification via mock API
- [ ] **Soft DTC from ML score < 40% automatically creates Q2 SAP QM notification**
- [ ] SAP QM dashboard shows notification list with DTC context
- [ ] 8D report template auto-generated from DTC data
- [ ] Fault injection GUI triggers demo scenarios and shows Q-Meldung feed

---

## Phase 12: Verification — xIL Testing + Unit Tests + Coverage

This phase implements the full **xIL (x-in-the-Loop) testing strategy** used in automotive systems engineering. Each level adds hardware fidelity, progressively validating from pure software to full system.

```
┌─────────────────────────────────────────────────────────────────┐
│  xIL Testing Pyramid                                           │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  HIL — Hardware-in-the-Loop                              │   │
│  │  All 4 physical ECUs + simulated plant + fault injection │   │
│  │  Validates: full system safety, real CAN timing          │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  PIL — Processor-in-the-Loop                             │   │
│  │  1-4 physical ECUs + rest simulated on PC                │   │
│  │  Validates: real-time behavior on target CPU             │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  SIL — Software-in-the-Loop                              │   │
│  │  All 7 ECUs as Docker containers + vcan0                 │   │
│  │  Validates: control logic, CAN protocol, state machines  │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  MIL — Model-in-the-Loop                                 │   │
│  │  Python plant model + control algorithm (no firmware)    │   │
│  │  Validates: algorithm design, system dynamics            │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  Unit Tests                                               │   │
│  │  Individual functions, isolated, mocked dependencies     │   │
│  │  Validates: correctness of each function                 │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 12a: MIL — Model-in-the-Loop (Python)

Plant models simulating physical dynamics. No firmware, no CAN — pure algorithm validation.

- [ ] `test/mil/plant_motor.py` — DC motor model
  - [ ] Electrical: V = IR + L(dI/dt) + Ke*ω (back-EMF)
  - [ ] Mechanical: J(dω/dt) = Kt*I - B*ω - T_load (inertia, friction, load)
  - [ ] Thermal: dT/dt = (I²R - h*A*(T-T_amb)) / (m*Cp) (heat generation vs dissipation)
  - [ ] Inputs: PWM duty → voltage, load torque
  - [ ] Outputs: current, speed (RPM), temperature
- [ ] `test/mil/plant_steering.py` — steering servo model
  - [ ] Angle response: first-order with rate limit and saturation
  - [ ] Return-to-center spring force
  - [ ] Inputs: PWM command
  - [ ] Outputs: actual angle (degrees)
- [ ] `test/mil/plant_vehicle.py` — simplified vehicle dynamics
  - [ ] Speed from motor torque: F = T/r, a = F/m, v += a*dt
  - [ ] Braking deceleration model
  - [ ] Lidar: object at configurable distance, closing rate
- [ ] `test/mil/controller_pedal.py` — pedal plausibility algorithm (pure Python)
- [ ] `test/mil/controller_motor.py` — motor control algorithm (ramp, derating)
- [ ] MIL test scenarios:
  - [ ] Normal operation: pedal → torque → motor model → speed
  - [ ] Overcurrent: load increase → current rises → cutoff triggers
  - [ ] Thermal runaway: sustained high load → temp rises → derating → shutdown
  - [ ] Emergency brake: lidar object → brake model → deceleration
- [ ] MIL test report with plots (matplotlib): current vs time, temp vs time, speed vs torque

### 12b: Unit Tests — Coverage & Gap Analysis

**Note**: 1,459 unit tests pass across all ECUs (updated 2026-02-25, CI run green). Phase 12b focuses on measuring and improving coverage.

- [x] Test framework: Unity (all C modules), run on host (x86)
- [x] Current test counts (2026-02-25, 0 failures):
  - BSW: 443 tests (18 modules, including hardened boundary/fault tests)
  - CVC: 215 tests (6 SWCs)
  - FZC: 200 tests (6 SWCs)
  - RZC: 181 tests (7 SWCs)
  - SC: 145 tests (9 modules)
  - BCM: 67 tests (3 modules)
  - ICU: 58 tests (2 modules)
  - TCU: 90 tests (3 modules)
  - Integration: 60 tests (cross-ECU)
  - **Total: 1,459 tests — all passing**
- [ ] Coverage measurement:
  - [ ] Statement coverage (target: 100% for safety-critical paths)
  - [ ] Branch coverage (target: 100% for ASIL D modules)
  - [ ] MC/DC coverage (target: 100% for ASIL D safety paths)
  - [ ] Generate per-module coverage reports
- [x] Static analysis:
  - [x] cppcheck on all firmware sources — 0 violations, CI blocking
  - [x] MISRA C:2012 full check (mandatory + required rules) — `.github/workflows/misra.yml`
  - [x] All deviations documented: DEV-001 (Rule 11.5), DEV-002 (Rule 11.8) in deviation register
- [ ] Gap identification:
  - [ ] Identify modules below coverage targets
  - [ ] Write additional tests for uncovered branches/conditions
  - [ ] Prioritize safety-critical paths (ASIL D modules first)
- [ ] Coverage report (statement, branch, MC/DC for safety-critical)

### 12c: SIL — Software-in-the-Loop (Docker + vcan)

All 7 ECUs compiled for Linux, running on PC with virtual CAN. No hardware needed.

- [ ] `test/sil/run_sil.sh` — brings up vcan0, launches all 7 ECU containers + plant simulator
- [ ] Plant simulator process (`test/sil/plant_sim.py`)
  - [ ] Reads motor commands from vcan0 (torque request PDU)
  - [ ] Runs motor/vehicle plant model from MIL
  - [ ] Writes simulated sensor values back to vcan0 (current, temp, speed, lidar distance)
  - [ ] Closed-loop: firmware controls plant, plant feeds back to firmware
- [ ] SIL test scenarios (automated, pytest):
  - [ ] All 16 demo scenarios run in SIL mode
  - [ ] Verify: correct state transitions, DTC storage, CAN message timing
  - [ ] Verify: safety mechanisms trigger at correct thresholds
  - [ ] Regression: run full suite in CI (GitHub Actions) — no hardware needed
- [x] SIL scenario YAML definitions (19 scenarios in `test/sil/scenarios/`):
  - [x] SIL-001..015: firmware scenarios (startup, faults, recovery, endurance)
  - [x] SIL-016: gateway MQTT telemetry + CAN acquisition (SWR-GW-001, SWR-GW-002)
  - [x] SIL-017: gateway ML anomaly detection (SWR-GW-003, SWR-GW-004)
  - [x] SIL-018: gateway SAP QM DTC forwarding + 8D report (SWR-GW-005, SWR-GW-006)
  - [x] SIL-019: cross-cutting analysis verification (SWR-ALL-001..004)
- [ ] SIL timing analysis: measure loop execution time, CAN latency on vcan

### 12d: PIL — Processor-in-the-Loop (partial hardware)

Real MCU executing firmware, but with simulated sensors/actuators from PC.

- [ ] PIL configuration: 1 STM32 (e.g., CVC) on real CAN bus ↔ PC running plant sim + remaining ECUs
- [ ] CAN bridge: CANable on PC bridges real CAN ↔ plant simulator
- [ ] PIL test scenarios:
  - [ ] Pedal → CVC (real MCU) → torque request on CAN → plant sim responds with motor feedback
  - [ ] Verify: real-time 10ms loop timing met on STM32
  - [ ] Verify: CAN message timing matches spec (jitter < 1ms)
  - [ ] Verify: WdgM feeds watchdog correctly under real CPU load
- [ ] PIL report: compare SIL results vs PIL results (any timing differences?)

### Files
- `test/mil/` — Python plant models, MIL test scripts, plots
- `test/sil/` — SIL runner, plant simulator, docker-compose-sil.yml
- `test/pil/` — PIL configuration, CAN bridge scripts
- `firmware/*/test/` — Unity unit tests per ECU
- `gateway/tests/` — pytest for Python modules
- `docs/aspice/verification/xil/mil-report.md`
- `docs/aspice/verification/unit-test/unit-test-report.md`
- `docs/aspice/verification/xil/sil-report.md`
- `docs/aspice/verification/xil/pil-report.md`
- `docs/aspice/verification/unit-test/coverage-report.md`
- `docs/aspice/verification/unit-test/static-analysis-report.md`

### DONE Criteria
- [ ] MIL: plant models produce physically plausible outputs, control algorithms validated
- [ ] Unit tests: every safety module has tests, MC/DC coverage documented
- [ ] SIL: all 19 scenarios pass in pure-software mode, CI-ready
- [ ] PIL: at least 1 ECU validated on real MCU with plant simulation
- [x] Zero MISRA mandatory violations (CI green, blocking)
- [x] Traceability CI green — 475 reqs, 0 broken links, 0 untested
- [ ] All tests pass across all xIL levels

---

## Phase 13: HIL — Hardware Assembly + Integration Testing

Full **HIL (Hardware-in-the-Loop)** — all 4 physical ECUs with real sensors/actuators, plus fault injection from PC/Pi simulating environmental conditions.

- [ ] Mount 4 ECU boards on platform
- [ ] CAN bus wiring (daisy chain: CVC → FZC → RZC → SC, 120Ω terminators at CVC and SC)
- [ ] Power distribution (12V supply → buck converters → 5V/3.3V rails)
- [ ] Sensor/actuator wiring per HSI spec
  - [ ] Dual pedal sensors → CVC SPI
  - [ ] OLED → CVC I2C
  - [ ] E-stop button → CVC GPIO
  - [ ] Steering servo + brake servo → FZC PWM
  - [ ] TFMini-S lidar → FZC UART
  - [ ] Steering angle sensor → FZC SPI
  - [ ] Motor + H-bridge → RZC PWM
  - [ ] Current sensor → RZC ADC
  - [ ] Thermistors → RZC ADC
  - [ ] Kill relay → SC GPIO (via MOSFET)
  - [ ] Fault LEDs → SC GPIO
  - [ ] External watchdogs → each ECU GPIO
- [ ] Pi + CANable connected to CAN bus
- [ ] PC + CANable connected to CAN bus (CAN bridge for simulated ECUs)
- [ ] Flash all physical firmware
- [ ] Launch simulated ECUs via docker-compose
- [ ] **HIL test execution:**
  - [ ] Integration test: CAN messages flowing between all 7 ECUs (4 physical + 3 simulated)
  - [ ] End-to-end test: pedal → CVC → RZC → motor spins → ICU shows speed
  - [ ] Safety chain test: fault → SC detects → kill relay opens → motor stops → BCM hazards
  - [ ] Diagnostics test: UDS request from PC → TCU responds with DTCs
  - [ ] Cloud test: Pi → AWS → Grafana shows live data from all 7 ECUs
  - [ ] **Fault injection via CAN**: inject corrupted messages, missing heartbeats, wrong alive counters
  - [ ] **Timing validation**: compare HIL CAN timing vs SIL vs PIL — document differences
  - [ ] **Endurance test**: 30-minute continuous run, monitor for drift, memory leaks, watchdog resets
- [ ] **HIL vs SIL comparison report**: document which failures are only caught at HIL level (e.g., EMC, timing jitter, ADC noise)

### Files
- `docs/aspice/verification/xil/hil-report.md`
- `docs/aspice/verification/integration-test/integration-test-report.md`
- `docs/aspice/verification/xil/xil-comparison-report.md` — MIL vs SIL vs PIL vs HIL comparison
- Photos of assembled platform

### DONE Criteria
- [ ] All 7 ECUs (4 physical + 3 simulated) + Pi communicating on CAN bus
- [ ] End-to-end pedal-to-motor working
- [ ] Safety chain (heartbeat → timeout → kill) working
- [ ] Simulated ECUs responding to CAN traffic (BCM lights, ICU dashboard, TCU UDS)
- [ ] Cloud dashboard receiving live data
- [ ] No communication errors in 30-minute endurance run
- [ ] xIL comparison report documents test coverage per level

---

## Phase 14: Demo Scenarios + Video + Portfolio Polish

### 14a: Demo Scenarios
- [ ] Execute and record all 16 demo scenarios
- [ ] Each scenario: setup → trigger → observable result → CAN trace
- [ ] Document results in system test report

### 14b: Video
- [ ] Record each scenario on video (10-15 sec each)
- [ ] Create demo reel (3-5 min combined)
- [ ] Include: platform overview, normal operation, fault scenarios, cloud dashboard, ML alerts

### 14c: Safety Case
- [ ] Safety case document (claims, arguments, evidence)
- [x] Final traceability verification — 475 reqs, 0 gaps, CI-enforced

### 14d: Portfolio Polish
- [ ] apps-web-overview.md — portfolio landing page
  - [ ] Project overview + zonal architecture diagram
  - [ ] Photo of assembled platform
  - [ ] Safety lifecycle summary (HARA → safety case)
  - [ ] Links to all work products
  - [ ] Demo video embed
  - [ ] Skills/standards demonstrated
  - [ ] BOM + cost breakdown
- [ ] Merge develop → release/1.0.0 → main
- [ ] Tag v1.0.0

### Files
- `docs/aspice/verification/system-verification/system-verification-report.md`
- `docs/safety/plan/safety-case.md`
- `apps-web-overview.md`
- `media/` — demo video or YouTube link

### DONE Criteria
- [ ] All 16 scenarios demonstrated and recorded
- [ ] Every fault results in correct safe state
- [ ] Safety case references all evidence
- [x] Traceability matrix 100% complete (CI-enforced, 475 reqs, SWR 100%)
- [ ] README is portfolio-ready
- [ ] Tagged v1.0.0 on main

---

## Skill Coverage Matrix

| Skill Area | Where Demonstrated | Resume Keywords |
|------------|-------------------|-----------------|
| **AUTOSAR Classic** | Shared BSW layer (MCAL, CanIf, PduR, Com, Dcm, Dem, WdgM, BswM, RTE) | AUTOSAR, BSW, MCAL, RTE, SWC, Com, Dcm, Dem |
| **POSIX MCAL Portability** | Same C codebase on STM32 (bare-metal) and Linux (Docker/SocketCAN) | cross-compilation, SocketCAN, POSIX, portability |
| Embedded C (MISRA) | All firmware: CVC, FZC, RZC, SC, BCM, ICU, TCU | MISRA C, state machines, defensive programming |
| Automotive Safety MCU | Safety Controller (TMS570) | TMS570, lockstep, HALCoGen, Cortex-R5 |
| Automotive Safety Process | All safety docs | ISO 26262, ASIL D, HARA, FMEA, DFA, safety case |
| Zonal Architecture | System design (7 ECUs, 4 physical + 3 simulated) | E/E architecture, zonal controllers, SDV |
| CAN Bus Protocol | CAN matrix, E2E, all firmware | CAN 2.0B, E2E protection, CRC, alive counter |
| Sensor Integration | AS5048A, TFMini-S, ACS723, NTC | SPI, UART, ADC, lidar, hall-effect, ToF |
| Motor Control | RZC firmware | PWM, H-bridge, current monitoring, encoder |
| ASPICE Process | All documentation | ASPICE 4.0, V-model, traceability |
| Virtual ECU / SIL | BCM, ICU, TCU (Docker + SocketCAN) | vECU, SIL testing, Docker, SocketCAN, CI/CD |
| UDS Diagnostics | Dcm + Dem in BSW, TCU | UDS (ISO 14229), OBD-II, DTC management, ReadDataByID |
| **MIL Testing** | Python plant models (motor, steering, vehicle dynamics) | MIL, plant model, control algorithm validation |
| **SIL Testing** | All 7 ECUs in Docker + vcan, automated test suite | SIL, virtual ECU, CI/CD, regression testing |
| **PIL Testing** | Real MCU + simulated plant via CAN bridge | PIL, processor-in-the-loop, real-time validation |
| **HIL Testing** | Full hardware + fault injection via CAN | HIL, fault injection, python-can, SocketCAN, endurance testing |
| **xIL Comparison** | MIL vs SIL vs PIL vs HIL coverage analysis | systems engineering, V-model verification, test strategy |
| Edge Computing | Raspberry Pi gateway | Edge ML, gateway, python-can |
| Machine Learning | Motor health, anomaly detection | scikit-learn, Isolation Forest, Random Forest, predictive maintenance |
| Cloud IoT | AWS pipeline | AWS IoT Core, MQTT, Timestream, Grafana |
| Automotive Cybersecurity | CAN anomaly detection | ISO/SAE 21434, intrusion detection |
| **SAP QM / Quality Management** | DTC → Q-Meldung pipeline, 8D report generation | SAP QM, quality notification, 8D process, BAPI |
| **End-to-End Traceability** | Sensor fault → DTC → cloud → SAP QM → 8D → corrective action | Field quality, warranty analysis, closed-loop quality |
| **Traceability Tooling** | Custom trace-gen.py (475 reqs, 8 V-model levels), CI enforcement, suspect-link detection | Requirements engineering, V-model, traceability matrix, CI/CD, ISO 26262 Part 8 |
| Containerization | Simulated ECU runtime | Docker, docker-compose, Linux containers |

---

## Doubts & Open Questions

These are honest uncertainties that need to be resolved during implementation. Flagging them now prevents surprises later.

| # | Doubt | Phase | Impact | Resolution Needed |
|---|-------|-------|--------|-------------------|
| D1 | **19.5 days is optimistic**. TMS570 toolchain (HALCoGen + CCS), vsomeip Docker build, AWS IoT setup, and ML model training each have learning curves. Realistic estimate with learning: 25-30 working days. | All | Schedule | Accept that timeline is aspirational. Track actual velocity after Phase 5. **Resolved — Phases 0-10 completed in ~4 days AI-assisted.** |
| D2 | **BSW ~2,500 LOC may underestimate**. Com with signal packing + timeouts + E2E integration alone could be 500+ LOC. Real total might be 3,500-4,000 LOC. | 5 | Schedule | Start with minimal Com (pack/unpack only), add E2E and timeouts incrementally. **Resolved — BSW ~4,500 LOC across 18 modules. Concern valid but managed.** |
| D3 | **Simulated ECU communication approach** (see Phase 10). Plan claimed BSW reuse + SOME/IP + direct CAN manager simultaneously. | 10 | Architecture | **Resolved — pure BSW + SocketCAN. SOME/IP dropped entirely.** |
| D4 | **MIL plant model parameters**. Motor model equations are correct, but parameter values (J, B, Ke, Kt, R, L) require the actual motor datasheet. A $25 hobby motor may not have a detailed datasheet — may need to measure parameters experimentally. | 12 | Accuracy | Buy motor early (Phase 0/1), measure parameters if datasheet is incomplete. *(Still open — relevant for Phase 12)* |
| D5 | **Docker + SocketCAN on Windows**. WSL2 USB passthrough for CANable is finicky (requires usbipd-win). May need to develop simulated ECUs directly on Raspberry Pi or a Linux machine. | 10 | Dev environment | Test WSL2 + usbipd-win early. Fallback: develop on Pi. **Resolved — Docker infra targets Linux. Dev on WSL2 or Pi.** |
| D6 | **vsomeip build complexity in Docker**. vsomeip has Boost dependencies and a non-trivial CMake build. Docker image may take significant effort to get right. | 10 | Schedule | **Dropped entirely. Not justified for portfolio value.** SOME/IP replaced by pure BSW + SocketCAN. |
| D7 | **HARA for a demo platform**. Real HARA requires operational situations analysis, but this is an indoor bench demo. Some Severity/Exposure ratings will feel forced. | 1 | Credibility | Be transparent: document that S/E/C ratings assume the platform controls a real vehicle. This is standard practice in academic/portfolio HARA. **Resolved — Completed. Transparent assumptions documented.** |
| D8 | **FMEDA failure rates**. TI publishes TMS570 safety manual with failure rates. ST publishes STM32 safety manuals for some variants — G474 may not have one (it's not an ASIL-certified MCU). | 2 | Completeness | Use generic Cortex-M4 failure rates from literature. Flag the assumption. **Resolved — Completed. Generic rates used, flagged in metrics doc.** |
| D9 | **AWS free tier sustainability**. Even at 1 msg/5sec, Timestream charges per write. May exceed free tier after ~2 weeks of continuous testing. | 11 | Cost | Budget $3-5/month for AWS. Use local InfluxDB as fallback. *(Still open — relevant for Phase 11)* |
| D10 | **SAP QM mock vs real SAP**. The mock API demonstrates the pipeline but won't impress someone who actually works with SAP QM daily. It's a Flask REST API, not RFC/BAPI. | 11 | Credibility | Frame it as "integration architecture demonstration" not "SAP integration". Show the data mapping and 8D workflow, not the API itself. *(Still open — relevant for Phase 11)* |

---

## Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| TMS570 toolchain learning curve | 1-2 days lost | Start with LED toggle + CAN loopback. Safety Controller firmware is only ~400 lines. **Mitigated — bare-metal C, no HALCoGen/CCS dependency in portfolio code.** |
| CAN bit timing mismatch | Bus doesn't work | Use online CAN bit timing calculator. Test 2-node first, add nodes one at a time. |
| AURIX scope creep temptation | Timeline blown | AURIX is a stretch goal ONLY. Ship with TMS570 first. **Mitigated — shipped with TMS570.** |
| AWS free tier exceeded | Unexpected cost | Batch messages to 1/5sec. Budget $3/month worst case. |
| Sensor wiring errors | Debug time | Follow HSI spec exactly. Test each sensor standalone before integration. |
| ML model insufficient training data | Weak demo | Generate synthetic data from fault injection. 30 min of CAN logging = thousands of samples. |
| CAN bridge timing jitter | Simulated ECUs have delayed responses vs physical | Acceptable for QM-rated simulated ECUs. Document latency in test report. |
| SocketCAN not available on Windows | Dev PC is Windows | Use WSL2 with USB passthrough for CANable, or develop on Raspberry Pi directly. Docker Desktop supports WSL2 backend. |
| UDS implementation scope creep | TCU takes too long | Limit to 5 core services (0x10, 0x22, 0x14, 0x19, 0x3E). No security access (0x27) for portfolio scope. |
| AUTOSAR BSW over-engineering | Spend too long on BSW perfection | Keep modules simplified (~2,500 LOC total). Not a certified stack — demonstrate architecture understanding, not production completeness. |
| vsomeip build complexity | Docker image build issues | Pin vsomeip version, pre-build in base image, fallback to raw SOME/IP over UDP if vsomeip too heavy. **Eliminated — SOME/IP dropped entirely. Pure BSW + SocketCAN.** |




