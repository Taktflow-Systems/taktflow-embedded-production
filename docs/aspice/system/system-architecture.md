---
document_id: SYSARCH
title: "System Architecture"
version: "1.0"
status: draft
aspice_process: SYS.3
date: 2026-02-21
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

## Lessons Learned Rule

Every architecture element in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/system/lessons-learned/`](lessons-learned/). One file per element. File naming: `SYSARCH-<element-name>.md`.


# System Architecture

<!-- DECISION: ADR-001 -- Zonal architecture over domain-based -->

## 1. Purpose and Scope

This document defines the system architecture for the Taktflow Zonal Vehicle Platform per Automotive SPICE 4.0 SYS.3 (System Architectural Design). It decomposes the system into architectural elements, defines their interfaces and interactions, specifies the communication architecture, and allocates system requirements (SYS-001 through SYS-056) to architecture elements.

The system architecture bridges the gap between system requirements (SYS.2, document SYSREQ) and software architecture (SWE.2). It serves as the primary reference for understanding how the system is structured, how data flows between elements, and how safety mechanisms are distributed.

### 1.1 Scope

The architecture covers:

- 7 ECU nodes: 4 physical (CVC, FZC, RZC, SC) and 3 simulated (BCM, ICU, TCU)
- CAN 2.0B communication network at 500 kbps
- Raspberry Pi 4 edge gateway with CAN-to-cloud bridge
- AWS IoT Core cloud telemetry and Grafana dashboards
- AUTOSAR-like BSW layer shared across STM32 ECUs
- Safety architecture with diverse redundancy (ST + TI)
- Power distribution and kill relay system

### 1.2 Design Philosophy

The architecture follows a **zonal design pattern** -- the modern E/E architecture approach adopted by Tesla, VW, and BMW. ECUs are organized by physical vehicle zone (front, rear, central) rather than by functional domain (powertrain, chassis, body). A central vehicle computer coordinates all zones, and an independent safety controller provides diverse monitoring.

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** Zonal architecture is the correct choice for a modern E/E platform portfolio piece; it demonstrates awareness of industry trajectory (ICAS1/VW, Tesla CCM, BMW Neue Klasse). **Why:** Zonal reduces wiring harness complexity and aligns ECU placement with physical vehicle zones, which is the dominant OEM direction post-2025. **Tradeoff:** Gains industry relevance and natural FFI boundaries per zone; loses the simplicity of domain-based decomposition where functional grouping is more intuitive for a bench-scale demonstrator. **Alternative:** Domain-based architecture (powertrain ECU, chassis ECU, body ECU) would have been simpler to wire but carries no resume differentiation and is being phased out by major OEMs. Ref: ADR-001.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-001 -->

## 2. Referenced Documents

| Document ID | Title | Version | Relevance |
|-------------|-------|---------|-----------|
| ITEM-DEF | Item Definition | 1.0 | System boundary, functions, interfaces, environment |
| HARA | Hazard Analysis and Risk Assessment | 1.0 | Hazardous events, ASIL assignments |
| SG | Safety Goals | 1.0 | 8 safety goals, safe states, FTTI |
| FSC | Functional Safety Concept | 1.0 | 23 safety mechanisms, FFI, degradation concept |
| FSR | Functional Safety Requirements | 1.0 | 25 functional safety requirements |
| TSR | Technical Safety Requirements | 1.0 | 51 technical safety requirements, ECU allocation |
| SYSREQ | System Requirements Specification | 1.0 | 56 system requirements (SYS-001 to SYS-056) |
| STKR | Stakeholder Requirements | 1.0 | 32 stakeholder requirements |
| SSR | Software Safety Requirements | 0.1 | Per-ECU software safety requirements |
| HSR | Hardware Safety Requirements | 0.1 | Per-ECU hardware safety requirements |
| HSI | Hardware-Software Interface | 0.1 | Pin mapping, peripheral configuration |

## 3. System Element Decomposition

### 3.1 Top-Level Decomposition

```
Taktflow Zonal Vehicle Platform
|
+-- Physical ECU Layer
|   +-- CVC (Central Vehicle Computer) -- STM32G474RE
|   +-- FZC (Front Zone Controller) -- STM32G474RE
|   +-- RZC (Rear Zone Controller) -- STM32G474RE
|   +-- SC (Safety Controller) -- TMS570LC43x
|
+-- Simulated ECU Layer
|   +-- BCM (Body Control Module) -- Docker/vCAN
|   +-- ICU (Instrument Cluster Unit) -- Docker/vCAN
|   +-- TCU (Telematics Control Unit) -- Docker/vCAN
|
+-- Edge Gateway Layer
|   +-- Raspberry Pi 4 + CANable 2.0
|
+-- Cloud Layer
|   +-- AWS IoT Core
|   +-- AWS Timestream
|   +-- Grafana Dashboard
|
+-- Network Layer
|   +-- CAN Bus (physical, 500 kbps)
|   +-- vCAN Bus (virtual, CI/CD mode)
|   +-- MQTT/TLS (gateway to cloud)
|   +-- SOME/IP Bridge (CAN to simulated ECUs)
|
+-- Power Layer
    +-- 12V Main Supply
    +-- Kill Relay Assembly
    +-- Voltage Regulators (5V, 3.3V)
```

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** The 4-physical + 3-simulated ECU topology is a pragmatic decision that maximizes architectural completeness while keeping BOM cost under control. **Why:** 4 physical ECUs (CVC, FZC, RZC, SC) cover the safety-critical path end-to-end; 3 simulated ECUs (BCM, ICU, TCU) demonstrate body/diagnostics/telematics without additional hardware spend, and share the same BSW stack via POSIX CAN backend. **Tradeoff:** Gains full 7-ECU network topology and UDS/body-control demonstration; loses physical CAN timing fidelity on simulated nodes (Docker scheduling jitter). **Alternative:** All 7 physical ECUs would cost ~$150+ more in Nucleo/LaunchPad boards and add wiring complexity with marginal safety benefit since BCM/ICU/TCU are QM-rated.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-002 -->

### 3.2 Detailed Element Decomposition

#### 3.2.1 CVC -- Central Vehicle Computer (ASIL D)

| Sub-Element | Type | Interface | Function |
|-------------|------|-----------|----------|
| STM32G474RE MCU | Processor | -- | Vehicle brain, state machine, pedal processing |
| AS5048A Pedal Sensor 1 | Sensor | SPI1, CS=PA4 | Primary pedal position (14-bit angle) |
| AS5048A Pedal Sensor 2 | Sensor | SPI1, CS=PA15 | Redundant pedal position (14-bit angle) |
| SSD1306 OLED Display | Actuator | I2C1, addr=0x3C | Operator status display (128x64) |
| E-Stop Button | Sensor | GPIO PC13, EXTI | Emergency stop input (active low, HW debounce) |
| TJA1051T/3 CAN Transceiver | Interface | FDCAN1 | CAN bus physical layer |
| TPS3823 External Watchdog | Safety HW | GPIO (WDI toggle) | Last-resort firmware hang detection |
| Status LEDs | Indicator | GPIO PB0 (green), PB1 (red) | Local ECU health indication |
| AUTOSAR-like BSW | Software | Internal | MCAL, CanIf, PduR, Com, Dcm, Dem, WdgM, BswM, RTE |
| Application SWCs | Software | Via RTE | Swc_Pedal, Swc_VehicleState, Swc_Dashboard, Swc_EStop, Swc_Heartbeat |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** STM32G474RE is a solid choice for the CVC role: 170 MHz Cortex-M4F with FDCAN, adequate flash (512 KB), and strong ST ecosystem/tooling. Dual AS5048A sensors on shared SPI1 with separate CS lines is the standard redundant-sensor pattern for ASIL D pedal sensing (SYS-001, SYS-002). **Why:** G4 series offers FDCAN (future CAN FD path), mixed-signal peripherals, and wide automotive temp range availability; Nucleo-G474RE is $15 and readily available. **Tradeoff:** Gains affordable ASIL D-capable platform with CAN FD headroom; the STM32G4 is not an automotive-qualified part (AEC-Q100) in Nucleo form, which is acceptable for a portfolio demonstrator but would need replacement with an automotive-grade STM32 for production. **Alternative:** STM32F446RE (cheaper, no FDCAN) or STM32H7 (overkill for this workload, higher cost/power).
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-003 -->

#### 3.2.2 FZC -- Front Zone Controller (ASIL D)

| Sub-Element | Type | Interface | Function |
|-------------|------|-----------|----------|
| STM32G474RE MCU | Processor | -- | Steering, braking, obstacle detection |
| Steering Servo (Metal Gear) | Actuator | TIM2_CH1 PWM, 50 Hz | Steering angle control |
| Brake Servo (Metal Gear) | Actuator | TIM2_CH2 PWM, 50 Hz | Brake force application |
| AS5048A Steering Angle Sensor | Sensor | SPI2, CS=PB12 | Steering position feedback (14-bit angle) |
| TFMini-S Lidar | Sensor | USART1, 115200 baud | Forward distance measurement (0.1-12 m, 100 Hz) |
| Piezo Buzzer | Actuator | GPIO | Audible warning patterns |
| TJA1051T/3 CAN Transceiver | Interface | FDCAN1 | CAN bus physical layer |
| TPS3823 External Watchdog | Safety HW | GPIO (WDI toggle) | Last-resort firmware hang detection |
| AUTOSAR-like BSW | Software | Internal | Shared BSW stack (same as CVC) |
| Application SWCs | Software | Via RTE | Swc_Steering, Swc_Brake, Swc_Lidar, Swc_FzcSafety, Swc_Heartbeat |

#### 3.2.3 RZC -- Rear Zone Controller (ASIL D)

| Sub-Element | Type | Interface | Function |
|-------------|------|-----------|----------|
| STM32G474RE MCU | Processor | -- | Motor control, current/temp/battery monitoring |
| 12V Brushed DC Motor | Actuator | Via BTS7960 | Propulsion motor |
| BTS7960 H-Bridge Driver | Actuator | TIM3_CH1/CH2 (RPWM/LPWM), GPIO PB0/PB1 (R_EN/L_EN) | Motor direction and speed control, 20 kHz PWM |
| ACS723 Current Sensor | Sensor | ADC1_CH1 | Motor current measurement (0-30A, galvanic isolation) |
| NTC Thermistor (Motor) | Sensor | ADC1_CH2 | Motor winding temperature (-40C to +125C) |
| NTC Thermistor (Winding) | Sensor | ADC1_CH3 | Secondary winding temperature |
| Battery Voltage Divider | Sensor | ADC1_CH4 | Battery voltage measurement (0-16V range) |
| Motor Encoder | Sensor | TIM4 (quadrature, PB6/PB7) | Motor speed and direction feedback |
| TJA1051T/3 CAN Transceiver | Interface | FDCAN1 | CAN bus physical layer |
| TPS3823 External Watchdog | Safety HW | GPIO (WDI toggle) | Last-resort firmware hang detection |
| AUTOSAR-like BSW | Software | Internal | Shared BSW stack (same as CVC) |
| Application SWCs | Software | Via RTE | Swc_Motor, Swc_CurrentMonitor, Swc_TempMonitor, Swc_Battery, Swc_RzcSafety, Swc_Heartbeat |

#### 3.2.4 SC -- Safety Controller (ASIL D)

| Sub-Element | Type | Interface | Function |
|-------------|------|-----------|----------|
| TMS570LC43x MCU (Lockstep Cortex-R5) | Processor | -- | Independent safety monitor |
| DCAN1 Controller | Interface | Listen-only mode (TEST reg bit 3) | CAN bus monitoring without transmission |
| SN65HVD230 CAN Transceiver | Interface | DCAN1 | CAN bus physical layer (3.3V compatible) |
| Kill Relay Circuit | Actuator | GIO_A0 -> IRLZ44N MOSFET -> 30A Relay | Energize-to-run actuator power cutoff |
| 1N4007 Flyback Diode | Protection | Across relay coil | Relay coil inductive spike suppression |
| Fault LED Panel (4 LEDs) | Indicator | GIO_A1 (CVC), GIO_A2 (FZC), GIO_A3 (RZC), GIO_A4 (System) | Per-ECU fault indication |
| TPS3823 External Watchdog | Safety HW | GIO_B0 (WDI toggle) | Last-resort firmware hang detection |
| ESM (Error Signaling Module) | Safety HW | Internal | Lockstep CPU comparison error detection |
| Minimal Firmware (~400 LOC) | Software | Direct register access (no AUTOSAR BSW) | Heartbeat monitor, cross-plausibility, kill relay, self-test |

**Note**: The SC intentionally does NOT use the AUTOSAR-like BSW stack. It runs a minimal, independent firmware generated by HALCoGen. This is a deliberate architectural decision for diverse software architecture per ISO 26262.

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** TMS570LC43x with lockstep Cortex-R5F is the textbook choice for a diverse safety controller. The decision to keep it on a completely independent software stack (~400 LOC, no AUTOSAR BSW, direct register access via HALCoGen) is the strongest FFI argument in the entire architecture. **Why:** Diverse vendor (TI vs ST), diverse CPU architecture (Cortex-R5F lockstep vs Cortex-M4F single-core), diverse compiler (TI CGT vs ARM GCC), and diverse software design (minimal firmware vs layered AUTOSAR) — this eliminates systematic common-cause failures per ISO 26262-9 Annex D. **Tradeoff:** Gains TUV-certified safety manual, hardware lockstep with ESM, and complete software independence; loses code reuse with the zone controllers and requires maintaining a second toolchain (Code Composer Studio + HALCoGen). **Alternative:** Using a second STM32 with the same BSW stack would save toolchain effort but would fail the diverse redundancy argument — an assessor would rightly question common-cause vulnerability. Ref: SYS-022 through SYS-027.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-004 -->

#### 3.2.5 BCM -- Body Control Module (QM, Simulated)

| Sub-Element | Type | Interface | Function |
|-------------|------|-----------|----------|
| Linux Process (Docker) | Runtime | SocketCAN (vcan0 or CAN bridge) | Simulated ECU execution |
| Can_Posix MCAL | Software | POSIX socket API | SocketCAN abstraction (same API as STM32 MCAL) |
| AUTOSAR-like BSW | Software | Internal | Shared BSW stack compiled for Linux |
| Swc_Lights | SWC | Via RTE | Auto headlight logic (speed > 0 -> lights on) |
| Swc_Indicators | SWC | Via RTE | Turn signals from steering angle, hazard on emergency |
| Swc_DoorLock | SWC | Via RTE | Auto-lock at speed |

CAN Messages: 0x400 (light status), 0x401 (indicator state), 0x402 (door lock status)

#### 3.2.6 ICU -- Instrument Cluster Unit (QM, Simulated)

| Sub-Element | Type | Interface | Function |
|-------------|------|-----------|----------|
| Linux Process (Docker) | Runtime | SocketCAN | Simulated ECU execution |
| Can_Posix MCAL | Software | POSIX socket API | SocketCAN abstraction |
| Swc_Dashboard | SWC | Via RTE | ncurses terminal UI: speedometer, temp, voltage, warnings |
| Swc_DtcDisplay | SWC | Via RTE | Active/stored DTC display |

CAN Messages: Receives ALL CAN messages (listen-only consumer). Sends: 0x7FF (DTC acknowledgments) <!-- TODO: Add 0x7FF to CAN matrix if not already present -->

#### 3.2.7 TCU -- Telematics Control Unit (QM, Simulated)

| Sub-Element | Type | Interface | Function |
|-------------|------|-----------|----------|
| Linux Process (Docker) | Runtime | SocketCAN | Simulated ECU execution |
| Can_Posix MCAL | Software | POSIX socket API | SocketCAN abstraction |
| UDS Server | SWC | Via Dcm BSW module | ISO 14229 diagnostic service handler |
| DTC Store | SWC | Via Dem BSW module | DTC storage (max 64 DTCs) |
| OBD-II PID Handler | SWC | Via Dcm BSW module | Mode 01 PIDs (RPM, speed, temp, load) |

CAN Messages: Receives 0x7DF/0x7E0-0x7E6 (UDS requests). Sends: 0x7E8-0x7EE (UDS responses)

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** Running BCM, ICU, and TCU as Docker containers with SocketCAN (Can_Posix MCAL) is an elegant approach: the same BSW C code compiles for both STM32 target and Linux host, proving the AUTOSAR portability abstraction works. **Why:** Demonstrates AUTOSAR-like BSW portability (same CanIf/PduR/Com stack, different MCAL backend), enables full 7-ECU network in CI/CD without hardware, and keeps QM-rated functions off physical boards. **Tradeoff:** Gains CI testability and BSW portability proof; loses deterministic CAN timing (Linux scheduling is not real-time) and physical sensor/actuator interaction for body functions. **Alternative:** Running simulated ECUs as bare Linux processes (no Docker) would reduce overhead slightly but lose the reproducible container environment for CI.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-005 -->

#### 3.2.8 Edge Gateway -- Raspberry Pi 4

| Sub-Element | Type | Interface | Function |
|-------------|------|-----------|----------|
| Raspberry Pi 4 (2GB) | Hardware | USB, Ethernet, Wi-Fi | Edge compute platform |
| CANable 2.0 | Interface | USB (gs_usb/candleLight firmware) | CAN bus listener |
| CAN Listener (python-can) | Software | SocketCAN (can0) | CAN message reception and decoding |
| Cloud Publisher (paho-mqtt) | Software | MQTT/TLS port 8883 | Telemetry batching and publishing |
| Motor Health ML Model | Software | scikit-learn (Random Forest) | Motor health scoring (1 Hz inference) |
| CAN Anomaly ML Model | Software | scikit-learn (Isolation Forest) | CAN bus anomaly detection (1 Hz inference) |
| Fault Injection GUI | Software | tkinter/Flask | Demo scenario trigger buttons |
| SAP QM Mock API | Software | Flask REST | Quality notification and 8D report generation |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** Raspberry Pi 4 as edge gateway is appropriate for a bench-scale demonstrator: it provides real Linux, USB-CAN via CANable 2.0, and enough compute for scikit-learn inference at 1 Hz. **Why:** Pi 4 is universally available, well-documented, and supports python-can natively; CANable 2.0 with gs_usb firmware gives a standards-compliant CAN interface without driver complexity. **Tradeoff:** Gains rapid prototyping and ML inference capability; not automotive-grade (no AEC-Q, no extended temp range, no deterministic OS). Production equivalent would be an NXP S32G or NVIDIA Jetson with AUTOSAR Adaptive. **Alternative:** NXP S32G GoldBox ($500+) would be production-representative but excessive for a portfolio demo. Ref: SYS-042, SYS-043.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-006 -->

#### 3.2.9 Cloud Layer

| Sub-Element | Type | Interface | Function |
|-------------|------|-----------|----------|
| AWS IoT Core | Service | MQTT broker | Message routing, X.509 auth |
| AWS IoT Rules | Service | SQL rules engine | Message filtering and routing |
| AWS Timestream | Service | Time-series DB | Telemetry storage |
| Grafana | Service | HTTP dashboard | Real-time visualization and alerting |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-007 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** AWS IoT Core + Timestream + Grafana is a well-established cloud telemetry stack with X.509 mutual TLS auth and native MQTT support. **Why:** AWS IoT Core handles device certificate management and MQTT routing; Timestream provides purpose-built time-series storage; Grafana adds alerting and visualization without vendor lock-in on the dashboard layer. **Tradeoff:** Gains production-grade cloud infrastructure with automotive IoT precedent (AWS has OEM partnerships); creates AWS dependency for the cloud layer. Per vendor-independence rules, the MQTT/TLS interface is standard and the Pi gateway could point to Azure IoT Hub or a self-hosted broker with configuration change only. **Alternative:** Self-hosted Mosquitto + InfluxDB + Grafana on Netcup VPS would eliminate AWS dependency but lose managed scaling and X.509 device provisioning. Ref: SYS-042.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-007 -->

#### 3.2.10 Network Elements

| Element | Type | Specification |
|---------|------|---------------|
| CAN Bus (Physical) | Wired bus | CAN 2.0B, 500 kbps, 11-bit IDs, 120 ohm termination |
| vCAN Bus (Virtual) | Software bus | Linux vcan0, for SIL testing and Docker ECUs |
| CAN Bridge (PC) | USB-CAN adapter | CANable 2.0, bridges real CAN to Docker simulated ECUs |
| MQTT Channel | Wireless | MQTT v3.1.1 over TLS 1.2, port 8883, X.509 certificates |
| SOME/IP Bridge | Software bridge | vsomeip on Pi, CAN <-> SOME/IP for Adaptive concepts |

#### 3.2.11 Power System

| Element | Specification | Consumers |
|---------|--------------|-----------|
| 12V Bench Supply | 12V DC, 5A continuous | Motor, servos, relay coil |
| Kill Relay (30A) | Energize-to-run, 12V coil, N-MOSFET driven | Gates 12V to motor driver and servos |
| Buck Converter 12V->5V | 5V, 3A output | TFMini-S lidar, Raspberry Pi (via USB-C) |
| Buck Converter 12V->3.3V | 3.3V, 1A output | Sensor power (shared rail) |
| Nucleo Onboard LDO | 3.3V from USB or 7-12V Vin | STM32 MCU logic power |
| LaunchPad Onboard LDO | 3.3V from USB or Vin | TMS570 MCU logic power |

## 4. System Architecture Diagram

### 4.1 Full System Block Diagram

```
                                    +---------------------------+
                                    |       AWS CLOUD           |
                                    |  IoT Core -> Timestream   |
                                    |       -> Grafana          |
                                    +------------+--------------+
                                                 |
                                            MQTT/TLS
                                            port 8883
                                                 |
                        +------------------------+------------------------+
                        |      RASPBERRY PI 4 -- Edge Gateway             |
                        |  +------------+  +----------+  +-------------+  |
                        |  | CAN        |  | ML       |  | SAP QM      |  |
                        |  | Listener   |  | Inference|  | Mock API    |  |
                        |  | (python-   |  | (scikit- |  | (Flask)     |  |
                        |  |  can)      |  |  learn)  |  |             |  |
                        |  +------+-----+  +----------+  +-------------+  |
                        |         |                                        |
                        |    CANable 2.0 (USB-CAN, gs_usb)                |
                        +----------+--------------------------------------+
                                   |
  =========+==========+==========+=+==========+==========+======== CAN Bus
            |          |          |            |          |       500 kbps
            |          |          |            |          |
   +--------+---+ +----+-----+ +-+--------+ +-+--------+ |
   |    CVC     | |   FZC    | |   RZC    | |    SC     | |
   | STM32G474  | | STM32G474| | STM32G474| | TMS570   | |
   |            | |          | |          | | LC43x    | |
   | [Pedal x2] | | [Steer  ]| | [Motor  ]| |          | |
   | [E-Stop  ] | | [Brake  ]| | [H-Brdg ]| | [CAN    ]| |
   | [OLED    ] | | [Lidar  ]| | [Curr.  ]| | [listen ]| |
   | [State   ] | | [Angle  ]| | [Temp   ]| | [only   ]| |
   | [Machine ] | | [Buzzer ]| | [Encoder]| | [Kill   ]| |
   |            | |          | | [Battery]| | [Relay  ]| |
   +-----+------+ +----+-----+ +----+-----+ | [LEDs  ]| |
         |             |            |        +----+-----+ |
         |             |            |             |       |
   FDCAN1+TJA1051     FDCAN1      FDCAN1    DCAN1+       |
                     +TJA1051    +TJA1051   SN65HVD230    |
                                                          |
                                             CANable 2.0 (PC)
                                                  |
                              +-------------------+-------------------+
                              |                   |                   |
                        +-----+-----+       +-----+-----+      +-----+-----+
                        |    BCM    |       |    ICU    |      |    TCU    |
                        |  (Docker) |       |  (Docker) |      |  (Docker) |
                        | Lights    |       | Dashboard |      | UDS Diag  |
                        | Indicators|       | Warnings  |      | DTC Store |
                        | Door Lock |       | DTC View  |      | OBD-II    |
                        +-----------+       +-----------+      +-----------+
                           QM                  QM                  QM
```

### 4.2 Data Flow Diagram

```
  Driver Input                     Environment
      |                                |
      v                                v
  +--------+  torque req (CAN)   +--------+  motor cmd (CAN)  +--------+
  |  CVC   |-------------------->|  RZC   |<------------------|  CVC   |
  | Pedal  |  steer cmd (CAN)    | Motor  |  current fb (CAN) | State  |
  | E-Stop |-------------------->| Current|------------------>| Machine|
  | State  |  brake cmd (CAN)    | Temp   |  temp fb (CAN)    |        |
  | OLED   |--+  vehicle state   | Battery|  speed fb (CAN)   |        |
  +--------+  |  (CAN broadcast) | Encoder|  battery fb (CAN) +--------+
      |       |                  +--------+       |
      |       |                       |           |
      |       |  +--------+          |           |
      |       +->|  FZC   |<---------+-----------+
      |          | Steer  |
      |          | Brake  |  angle fb (CAN)
      |          | Lidar  |-----------------> CVC
      |          | Buzzer |  distance (CAN)
      |          +--------+-----------------> CVC
      |               |
      |    heartbeats  |    heartbeats
      |    (CAN 50ms)  |    (CAN 50ms)
      v               v
  +--------+  listens to ALL CAN messages
  |   SC   |  (DCAN1 silent mode)
  | Monitor|
  | Kill   |---> Kill Relay ---> 12V actuator power
  | Relay  |
  | LEDs   |
  +--------+
      |
  Independent of CAN (GPIO-driven):
  Fault LEDs, Kill Relay, Ext. Watchdog
```

## 5. Communication Architecture

### 5.1 CAN Bus Topology

| Parameter | Value |
|-----------|-------|
| Standard | CAN 2.0B (11-bit standard IDs) |
| Bit Rate | 500 kbps |
| Topology | Linear bus, daisy-chain wiring |
| Termination | 120 ohm at each physical end (CVC end + SC end) |
| Wire | 22 AWG twisted pair (CAN_H, CAN_L) |
| Physical Nodes | CVC, FZC, RZC, SC (4 physical) |
| Virtual Nodes | BCM, ICU, TCU (via CAN bridge) + Pi gateway |
| Total Nodes | 8 (4 physical + 3 bridged + 1 gateway) |
| Transceivers | TJA1051T/3 (STM32 ECUs, 3.3V, CAN FD capable) |
| SC Transceiver | SN65HVD230 (3.3V, classic CAN compatible) |
| Controllers | FDCAN in classic mode (STM32G474), DCAN (TMS570) |
| SC CAN Mode | Listen-only (DCAN TEST register bit 3 = 1) |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-008 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** CAN 2.0B at 500 kbps with 11-bit standard IDs is the appropriate bus choice for this system's message count (24 types) and data rates. **Why:** 500 kbps is the standard automotive CAN speed for powertrain/chassis networks; CAN 2.0B with 11-bit IDs is sufficient since 24 message types are well within the 2048-ID address space; 24% bus utilization leaves ample headroom below the 50% design target. **Tradeoff:** Gains proven automotive reliability, universal tool support (Vector, PEAK, candump), and simple wiring; foregoes CAN FD bandwidth (up to 8 Mbps data phase) which is not needed at this message rate. The FDCAN controllers on STM32G474 are configured in classic CAN mode, preserving the upgrade path to CAN FD if message payloads grow. **Alternative:** CAN FD at 2/5 Mbps would demonstrate a more modern bus but adds complexity in transceiver selection and offers no benefit at 24% utilization. Ethernet/SOME/IP would be production-relevant for zonal gateways but adds $50+ per node in PHY hardware. Ref: SYS-031.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-008 -->

### 5.2 CAN ID Allocation Strategy

CAN IDs are assigned by safety priority (lower CAN ID = higher arbitration priority):

| ID Range | Priority | Category | ASIL |
|----------|----------|----------|------|
| 0x001 | Highest | E-Stop broadcast | B |
| 0x010-0x012 | Very High | Heartbeat messages (CVC, FZC, RZC) | C |
| 0x100-0x1FF | High | Control commands (torque, steer, brake, vehicle state) | D |
| 0x200-0x2FF | Medium | Status feedback (motor, steering, lidar, brake) | A-D |
| 0x300-0x3FF | Normal | Sensor data (current, temp, battery, speed) | A-QM |
| 0x400-0x4FF | Low | Body functions (lights, indicators, door lock) | QM |
| 0x500-0x5FF | Lowest | DTC broadcast | QM |
| 0x7DF | Standard | UDS functional request (ISO 14229) | QM |
| 0x7E0-0x7E6 | Standard | UDS physical requests (per ECU) | QM |
| 0x7E8-0x7EE | Standard | UDS physical responses (per ECU) | QM |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-009 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** CAN ID allocation by safety priority (lower ID = higher arbitration priority = higher ASIL) is the textbook approach and ensures safety-critical messages always win bus arbitration. E-stop at 0x001, heartbeats at 0x010-0x012, ASIL D control commands at 0x100-0x1FF. **Why:** CAN arbitration is non-destructive and bitwise — lower ID always wins. Placing ASIL D messages in the lowest ID ranges guarantees worst-case latency for safety messages regardless of bus load. **Tradeoff:** Gains deterministic priority inversion protection for safety messages; the ID allocation is somewhat rigid (large gaps between ranges), but this is intentional for future expansion. **Alternative:** Priority-based allocation is the only defensible approach for a mixed-ASIL CAN network per ISO 26262-4. Random or sequential ID assignment would be an audit finding. Ref: SYS-033.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-009 -->

### 5.3 CAN Message Overview Table

| CAN ID | Message Name | Sender | Receiver(s) | DLC | Cycle (ms) | E2E | ASIL | Description |
|--------|-------------|--------|-------------|-----|-----------|-----|------|-------------|
| 0x001 | E-Stop Broadcast | CVC | ALL | 4 | Event (10 ms repeat) | Yes | B | Emergency stop command, highest priority |
| 0x010 | CVC Heartbeat | CVC | SC, ALL | 4 | 50 | Yes | C | CVC alive counter, mode, fault status |
| 0x011 | FZC Heartbeat | FZC | SC, ALL | 4 | 50 | Yes | C | FZC alive counter, mode, fault status |
| 0x012 | RZC Heartbeat | RZC | SC, ALL | 4 | 50 | Yes | C | RZC alive counter, mode, fault status |
| 0x100 | Vehicle State | CVC | FZC, RZC, BCM, ICU | 6 | 10 | Yes | D | Vehicle state, fault mask, torque/speed limits |
| 0x101 | Torque Request | CVC | RZC | 8 | 10 | Yes | D | Torque %, direction, pedal position |
| 0x102 | Steer Command | CVC | FZC | 8 | 10 | Yes | D | Steering angle command (degrees) |
| 0x103 | Brake Command | CVC | FZC | 8 | 10 | Yes | D | Brake force % (0-100) |
| 0x200 | Steering Status | FZC | CVC, SC | 8 | 20 | Yes | D | Actual angle, commanded angle, fault status |
| 0x201 | Brake Status | FZC | CVC, SC | 8 | 20 | Yes | D | Brake position, servo current, fault status |
| 0x210 | Brake Fault | FZC | CVC, SC | 4 | Event | Yes | D | Brake fault type, commanded vs actual |
| 0x211 | Motor Cutoff Req | FZC | CVC | 4 | Event | Yes | D | Emergency motor cutoff request |
| 0x220 | Lidar Distance | FZC | CVC, ICU | 8 | 10 | Yes | C | Distance (cm), signal strength, zone |
| 0x300 | Motor Status | RZC | CVC, SC, ICU | 8 | 20 | Yes | D | Speed (RPM), direction, enable status |
| 0x301 | Motor Current | RZC | SC, CVC, ICU | 8 | 10 | Yes | C | Current (mA), direction, enable status |
| 0x302 | Motor Temperature | RZC | CVC, ICU | 6 | 100 | Yes | A | Winding temp 1, winding temp 2, derating % |
| 0x303 | Battery Status | RZC | CVC, ICU | 4 | 1000 | No | QM | Battery voltage, SOC estimate |
| 0x400 | Light Status | BCM | ICU | 4 | 100 | No | QM | Headlights, tail lights, fog |
| 0x401 | Indicator State | BCM | ICU | 4 | 100 | No | QM | Left, right, hazard |
| 0x402 | Door Lock Status | BCM | ICU | 2 | Event | No | QM | Lock state per door |
| 0x500 | DTC Broadcast | Any | TCU, ICU | 8 | Event | No | QM | DTC number, status, ECU source |
| 0x7DF | UDS Func Request | Tester | TCU | 8 | On-demand | No | QM | ISO 14229 functional addressing |
| 0x7E0-0x7E6 | UDS Phys Request | Tester | Per ECU | 8 | On-demand | No | QM | ISO 14229 physical addressing |
| 0x7E8-0x7EE | UDS Response | Per ECU | Tester | 8 | On-demand | No | QM | ISO 14229 response |

**Total CAN messages: 24 defined types** (excluding per-ECU UDS addressing variants).

### 5.4 CAN Bus Utilization Estimate

| Message Category | Messages | Avg. DLC | Cycle (ms) | Bits/msg | Bits/sec |
|------------------|----------|----------|-----------|----------|----------|
| E-Stop (event-driven) | 1 | 4 | N/A (event) | 83 | ~0 (rare) |
| Heartbeats (3 ECUs) | 3 | 4 | 50 | 83 | 4,980 |
| Control (4 msgs) | 4 | 8 | 10 | 131 | 52,400 |
| Status (4 msgs) | 4 | 8 | 20 | 131 | 26,200 |
| Sensor data (3 msgs) | 2 | 8 | 10-100 | 131 | ~18,340 |
| Motor temp | 1 | 6 | 100 | 115 | 1,150 |
| Battery | 1 | 4 | 1000 | 83 | 83 |
| Body (3 msgs) | 3 | 4 | 100 | 83 | 2,490 |
| **Total continuous** | | | | | **~105,643** |

**Bus utilization**: ~120,000 / 500,000 = **24%** (well below the 50% design target; see CAN message matrix for detailed per-message calculation).

### 5.5 E2E Protection Overview

| Property | Value |
|----------|-------|
| CRC Polynomial | CRC-8/SAE-J1850 (0x1D, init 0xFF) |
| CRC Coverage | Data payload bytes + Data ID |
| Alive Counter | 4-bit (0-15, wraps), increments by 1 per TX |
| Data ID | 4-bit, unique per message type |
| E2E Header Position | Bytes 0-1 of CAN payload |
| Hamming Distance | 4 (for payloads up to 8 bytes) |
| Error Detection | All 1-3 bit errors, most multi-bit burst errors |

**E2E-protected messages**: All messages with ASIL A or higher (0x001, 0x010-0x012, 0x100-0x103, 0x200-0x201, 0x210-0x211, 0x220, 0x300-0x302).

**Non-E2E messages**: QM messages (0x303, 0x400-0x402, 0x500, UDS).

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-010 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** CRC-8/SAE-J1850 with 4-bit alive counter and 4-bit data ID in a 2-byte E2E header is a well-balanced choice for CAN 2.0 frames where payload space is limited to 8 bytes. Hamming distance of 4 detects all 1-3 bit errors. **Why:** SAE J1850 polynomial is automotive-standard; the 2-byte header leaves 6 bytes for data in an 8-byte CAN frame, which is sufficient for all defined messages. AUTOSAR E2E Profile 1 uses this exact scheme. **Tradeoff:** Gains standard E2E detection (corruption, loss, delay, repetition, insertion) with minimal overhead; CRC-8 has weaker detection than CRC-32 for multi-bit burst errors, but CAN's built-in CRC-15 already provides the first layer of integrity. The 4-bit alive counter wraps at 15, which is adequate for 50 ms cycle times (750 ms detection window). **Alternative:** CRC-32 (AUTOSAR E2E Profile 4) would provide stronger burst detection but consumes 4 more payload bytes, leaving only 2 bytes for data — not viable on CAN 2.0. Ref: SYS-032.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-010 -->

**Receiver behavior on E2E failure**:
1. First failure: Use last valid value
2. Second failure: Use last valid value (warning logged)
3. Third consecutive failure: Substitute safe default (zero torque, full brake, center steering)

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-011 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** The 3-strikes E2E failure policy (use-last, use-last-with-warning, substitute-safe-default) is a sensible graduated response that avoids nuisance safe-stops from transient CAN errors while still reaching the safe state within FTTI. **Why:** A single CAN bit error should not trigger emergency braking; 3 consecutive failures at 10 ms cycle = 30 ms detection, well within typical FTTI budgets (50-200 ms for steering/braking). **Tradeoff:** Gains robustness against transient EMI; the 2-cycle "use last valid" window means the system operates on stale data briefly. At 10 ms cycle time, this is 20 ms of stale data — acceptable for the mechanical response time of servos and motors. **Alternative:** Immediate safe-default on first failure would be more conservative but would cause frequent nuisance stops on a bench setup with imperfect CAN wiring. Ref: SYS-032, FSC SM-004.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-011 -->

### 5.6 MQTT Topics (Gateway to Cloud)

| Topic | Direction | Content | Rate |
|-------|-----------|---------|------|
| `vehicle/telemetry` | Pi -> Cloud | JSON: all ECU status, sensor values | 1 msg / 5 sec |
| `vehicle/dtc/new` | Pi -> Cloud | New DTC event with freeze-frame | Event-driven |
| `vehicle/dtc/soft` | Pi -> Cloud | ML-predicted soft DTC (P1A40) | Event-driven |
| `vehicle/alerts` | Pi -> Cloud | Anomaly detection alert | Event-driven |
| `vehicle/config` | Cloud -> Pi | Configuration updates | On-demand |

### 5.7 SOME/IP Bridge (Docker Simulated ECUs)

The 3 simulated ECUs (BCM, ICU, TCU) use CAN as their primary communication interface via the BSW stack with a POSIX SocketCAN MCAL backend (`Can_Posix.c`). This allows the same BSW stack code to run on both STM32 and Linux targets.

Additionally, vsomeip (BMW open-source SOME/IP implementation) provides service-oriented communication as a demonstration of AUTOSAR Adaptive concepts:

- BCM offers `LightService` (SOME/IP)
- ICU offers `DisplayService` (SOME/IP)
- SOME/IP-SD (Service Discovery) runs on the local Docker network
- The Pi gateway acts as a CAN-to-SOME/IP bridge when needed

The SOME/IP layer is supplementary and does not carry safety-critical data.

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-012 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** Positioning SOME/IP as supplementary (non-safety, demonstration only) is the right call. It demonstrates AUTOSAR Adaptive awareness without burdening the safety case. **Why:** vsomeip with Service Discovery shows service-oriented communication competence (a key AUTOSAR Adaptive skill), but routing safety-critical data through SOME/IP would require qualifying the vsomeip stack to ASIL D — an enormous effort for no safety benefit. **Tradeoff:** Gains AUTOSAR Adaptive portfolio keyword and service-oriented demo; the SOME/IP path adds latency and a Linux dependency that would not exist in a pure CAN architecture. **Alternative:** Omitting SOME/IP entirely would simplify the system but lose a valuable AUTOSAR Adaptive talking point.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-012 -->

## 6. System State Machine

### 6.1 State Definitions

| State | ID | Description | Motor | Steering | Braking | Monitoring |
|-------|----|-------------|-------|----------|---------|------------|
| OFF | 0 | System powered down, kill relay open | Disabled | Disabled | Disabled | Off |
| INIT | 1 | BSW initialization, self-test, CAN startup | Disabled | Disabled | Disabled | Self-test |
| RUN | 2 | Normal operation, all functions active | 100% torque, 100% speed | Full range, 30 deg/s rate | Full authority | All active |
| DEGRADED | 3 | Reduced performance, non-critical fault | 75% torque, 50% speed | Full range, 15 deg/s rate | Full authority | Warning |
| LIMP | 4 | Minimal function, critical non-safety fault | 30% torque, 20% speed | +/-15 deg, 7.5 deg/s rate | Full authority | Continuous warning |
| SAFE_STOP | 5 | Controlled shutdown sequence | Disabled (0%) | Return to center | Full brake applied | Emergency warning |
| SHUTDOWN | 6 | Kill relay open, system de-energized | Power cut | Power cut | Mechanical hold | LED last state |

### 6.2 State Transition Table

| From | To | Trigger | Guard Condition | Action |
|------|----|---------|-----------------|--------|
| OFF | INIT | Ignition on (power applied) | 12V and 3.3V rails stable | BSW_Init(), CAN init, self-test start |
| INIT | RUN | Self-test passed | All BSW modules initialized, CAN operational, all heartbeats received | Enable actuators, energize kill relay (SC) |
| INIT | SAFE_STOP | Self-test failed | Any self-test step fails | Log DTC, display fault on OLED |
| RUN | DEGRADED | Minor fault detected | Single sensor fault, intermittent CAN error, temp warning | Reduce limits, display warning, buzzer single beep |
| RUN | SAFE_STOP | Critical fault detected | E-stop, dual sensor failure, brake fault, SC override | Motor off, brakes applied, buzzer fast beep |
| DEGRADED | RUN | All faults cleared | 5 sec fault-free + manual acknowledge | Restore full limits |
| DEGRADED | LIMP | Persistent fault (5 sec) | Fault not cleared within 5 seconds | Further reduce limits, buzzer slow beep |
| DEGRADED | SAFE_STOP | Critical fault detected | Additional fault during degraded operation | Motor off, brakes applied |
| LIMP | SAFE_STOP | Critical fault detected | Any additional fault in LIMP mode | Motor off, brakes applied |
| SAFE_STOP | SHUTDOWN | Controlled shutdown complete | Vehicle speed = 0, all outputs disabled | Open kill relay, persist DTCs |
| Any | SAFE_STOP | E-stop pressed | GPIO PC13 rising edge confirmed (NC button) | Immediate motor off, brakes, CAN broadcast 0x001 |
| Any | SHUTDOWN | SC kill relay opened | SC detected safety violation | Hardware power removal to actuators |

**Transition rules**:
1. Forward-only progression (except DEGRADED -> RUN recovery).
2. No skip-back: LIMP cannot go directly to RUN (must pass through DEGRADED first).
3. SHUTDOWN is irreversible without full power cycle.
4. E-stop overrides from any state to SAFE_STOP.
5. SC kill relay overrides from any state to SHUTDOWN (hardware authority).

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-013 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** The 7-state machine with forward-only progression and two override paths (E-stop to SAFE_STOP, SC kill to SHUTDOWN) is well-designed for ISO 26262 safe-state arguments. The DEGRADED and LIMP states provide graduated degradation before reaching SAFE_STOP, which is the expected pattern for ASIL D systems. **Why:** Forward-only prevents oscillation between RUN and fault states; the single recovery path (DEGRADED -> RUN after 5s fault-free + manual acknowledge) requires both automatic and human confirmation. **Tradeoff:** Gains a deterministic, provably-safe state progression that an assessor can follow; the 5-second recovery timer plus manual acknowledge is conservative but prevents premature return to full operation. LIMP -> RUN requires cycling through DEGRADED first, adding operator friction. **Alternative:** A simpler 3-state model (RUN/FAULT/OFF) would be easier to implement but would not demonstrate graduated degradation — a key differentiator for ASIL D systems. Ref: SYS-029, SYS-030, FSC SM-022.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-013 -->

### 6.3 State Transition Diagram (ASCII)

```
                                +-------+
                   Power On     |  OFF  |
                 +------------->+-------+
                 |                  |
                 |           ignition on
                 |                  v
                 |              +-------+
                 |              | INIT  |
                 |              +---+---+
                 |                  |
                 |        +---------+---------+
                 |        |                   |
                 |   self-test OK        self-test FAIL
                 |        |                   |
                 |        v                   |
                 |    +-------+               |
                 |    |  RUN  |               |
                 |    +---+---+               |
                 |        |                   |
                 |  +-----+------+            |
                 |  |            |            |
                 |  minor     critical        |
                 |  fault     fault/E-stop    |
                 |  |            |            |
                 |  v            |            |
          +------+------+       |            |
   faults |  DEGRADED   |       |            |
   clear  +------+------+       |            |
   + 5s   |      |             |            |
   recov. |   persistent       |            |
   +------+   fault (5s)       |            |
                 |              |            |
                 v              |            |
             +------+          |            |
             | LIMP |          |            |
             +--+---+          |            |
                |              |            |
             critical          |            |
             fault             |            |
                |              |            |
                v              v            v
            +--------+<-------+<-----------+
            | SAFE   |
            | STOP   |<--- E-Stop from ANY state
            +---+----+
                |
           speed = 0
                |
                v
            +--------+
            |SHUTDOWN|<--- SC Kill Relay from ANY state
            +--------+
                |
           power cycle
                |
                v
            +-------+
            |  OFF  |
            +-------+
```

### 6.4 Per-State Operational Limits

| Parameter | RUN | DEGRADED | LIMP | SAFE_STOP | SHUTDOWN |
|-----------|-----|----------|------|-----------|----------|
| Max Torque | 100% | 75% | 30% | 0% | 0% (power cut) |
| Max Speed | 100% | 50% | 20% | 0% | 0% (power cut) |
| Steering Range | +/-45 deg | +/-45 deg | +/-15 deg | Return-to-center | No power |
| Steering Rate | 30 deg/s | 15 deg/s | 7.5 deg/s | Controlled to center | No power |
| Braking | Driver command | Driver command | Driver command | Full brake applied | Mechanical hold |
| Buzzer | Silent | Single beep | Slow repeat | Fast repeat | Off |
| OLED | Normal display | Warning + limits | LIMP warning | STOP warning | Last frame |

### 6.5 E-Stop and SC Override Paths

**E-Stop Path (CVC-initiated, software)**:
1. E-stop button press detected on CVC GPIO PC13 (< 1 ms)
2. CVC sets internal torque to zero (< 1 ms)
3. CVC broadcasts E-stop CAN message 0x001 (< 2 ms)
4. RZC receives and disables motor (< 10 ms)
5. FZC receives and applies full brake + centers steering (< 10 ms)
6. Total: < 12 ms from button press to safe state

**SC Override Path (hardware-independent)**:
1. SC detects safety violation (heartbeat timeout, cross-plausibility, self-test failure)
2. SC drives GIO_A0 LOW (< 1 ms)
3. N-MOSFET turns off, relay coil de-energizes (< 5 ms)
4. Relay contacts open, 12V removed from motor driver and servos (< 10 ms)
5. Total: < 16 ms from detection to actuator power removal

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-014 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** The dual safe-state paths (software E-stop via CAN at <12 ms, hardware SC kill relay at <16 ms) provide genuine defense-in-depth. The E-stop path is faster (CAN broadcast) but depends on functioning CVC and CAN bus; the SC path is independent of both (GPIO-driven relay). **Why:** ISO 26262 requires that the safe state be reachable even when the primary safety mechanism fails. The SC kill relay is the last-resort path — it operates on hardware signals (heartbeat timeout, GPIO) that are independent of CAN bus integrity and zone controller firmware. **Tradeoff:** Gains two independent paths to safe state with different failure modes; the SC path (16 ms) is slightly slower than the E-stop path (12 ms) due to relay mechanical response time, but this is well within any reasonable FTTI. **Alternative:** A single-path E-stop (CAN only) would be faster but would be a single point of failure — CAN bus fault could prevent safe-stop. Ref: SYS-024, SYS-028, SG-001 through SG-008.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-014 -->

## 7. Power Architecture

### 7.1 Power Domain Diagram

```
   12V Bench Supply (5A)
          |
          |  12V Main Rail
          +------+------+------+------+------+
          |      |      |      |      |      |
          |   Buck     Buck   Nucleo Nucleo Nucleo
          |   12V->5V  12V->  LDO    LDO    LDO
          |    |       3.3V   (CVC)  (FZC)  (RZC)
          |    |        |      |      |      |
          |   5V Rail  3.3V   3.3V   3.3V   3.3V
          |    |       Rail    |      |      |
          |    |        |      |      |      |
          |  TFMini-S  Sensors CVC    FZC    RZC
          |  Pi (USB)  CAN     MCU    MCU    MCU
          |            xcvrs
          |
          +--[ Kill Relay (30A, Energize-to-Run) ]
          |
          |  12V Actuator Rail (GATED by Kill Relay)
          |
          +------+------+------+
          |      |      |      |
       BTS7960  Steer  Brake  Relay
       Motor    Servo  Servo  Coil
       Driver                  |
                          SC GIO_A0
                            |
                         IRLZ44N
                         MOSFET
                            |
                          GND
```

```
   LaunchPad Power (independent path):

   USB or separate 3.3V supply
          |
     TMS570LC43x
     (SC MCU)
          |
     SN65HVD230
     (CAN transceiver)
          |
     TPS3823
     (ext. watchdog)
          |
     Fault LEDs (x4)
```

### 7.2 Power Domain Description

**12V Main Rail (always live)**: Directly from the bench supply. Powers buck converters, Nucleo board Vin inputs, and the kill relay coil circuit. This rail is NOT gated by the kill relay.

**12V Actuator Rail (gated by kill relay)**: Only powered when the SC actively energizes the kill relay. Supplies the BTS7960 motor driver, steering servo, and brake servo. When the kill relay opens, this rail loses power and all actuators stop.

**5V Rail**: From 12V->5V buck converter. Powers the TFMini-S lidar (5V power, 3.3V logic) and the Raspberry Pi (via USB-C cable). Always live (not gated by kill relay).

**3.3V Rail**: From 12V->3.3V buck converter and Nucleo onboard LDOs. Powers all STM32 MCUs, CAN transceivers (TJA1051T/3), angle sensors (AS5048A), current sensor (ACS723), and NTC thermistors. Always live.

**SC Power (independent)**: The TMS570LC43x LaunchPad is powered independently (via USB or separate Vin) to ensure the safety controller remains operational regardless of main 12V rail status. The SC's CAN transceiver (SN65HVD230), external watchdog (TPS3823), and fault LEDs are powered from this independent path.

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-015 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** Independent SC power is essential for the safety argument. If the SC shared the same 12V rail as the zone controllers, a main supply fault could disable both the monitored system and the monitor simultaneously — a dependent failure that violates ISO 26262-9 DFA requirements. **Why:** The SC must remain operational when the main 12V rail fails or is intentionally cut by the kill relay. USB power from the LaunchPad's debug connector or a separate supply achieves this. **Tradeoff:** Gains true power domain independence for the safety monitor; adds one more power cable to the bench setup. The CAN bus transceiver still shares the physical CAN wires with the zone controllers, so a CAN bus short could still affect SC monitoring — but the SC is listen-only and the kill relay is GPIO-driven, not CAN-driven. **Alternative:** A battery-backed SC power rail would be even more robust but is overkill for a bench demonstrator.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-015 -->

### 7.3 Energize-to-Run Kill Relay Design

| Parameter | Value |
|-----------|-------|
| Relay | 30A automotive relay, 12V coil |
| Drive Circuit | SC GIO_A0 (3.3V) -> 10k gate resistor -> IRLZ44N N-MOSFET gate |
| Flyback Protection | 1N4007 diode across relay coil (cathode to 12V) |
| Fail-Safe Behavior | SC power loss -> MOSFET off -> relay de-energizes -> actuator power cut |
| Re-energize | Only after full power cycle + SC startup self-test pass |
| Relay Test | Performed during INIT state: energize for 200 ms, verify actuator voltage present, then proceed |

**Why energize-to-run**: Any failure of the SC (power loss, firmware hang, lockstep CPU error, watchdog timeout, GPIO fault) results in the relay coil losing drive current. The relay spring returns the contacts to the open (safe) position. This is the preferred fail-safe pattern for ISO 26262 safety-critical actuator control.

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-016 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** Energize-to-run is the gold-standard fail-safe relay pattern. The IRLZ44N logic-level MOSFET driven from SC GIO_A0 (3.3V) is a clean low-side switching design. Flyback protection with 1N4007 is correct for inductive relay coil. **Why:** In the energize-to-run pattern, the default state (no power, no firmware, any fault) is safe — relay open, actuators de-energized. This means every conceivable SC failure mode results in the safe state without requiring any active response. An assessor will immediately recognize this as the ISO 26262 recommended approach for fail-safe actuator cutoff. **Tradeoff:** Gains inherent fail-safety (no single fault can prevent safe-state entry); the relay must be actively held closed during normal operation, consuming ~200 mA relay coil current continuously. Also, the 30A relay contacts could weld if subjected to inrush current from the motor driver — a 30A relay with adequate inrush margin should be specified. **Alternative:** A de-energize-to-run (normally-closed) relay would save continuous coil current but would require active intervention to open — any SC failure mode would leave actuators powered, which is unsafe. Ref: SYS-024, SM-005.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-016 -->

## 8. Safety Architecture

### 8.1 Safety Mechanism Allocation Matrix

The 23 safety mechanisms from the Functional Safety Concept (FSC) are allocated across the 4 physical ECUs. P = Primary implementer, S = Secondary/backup, M = Monitor only.

| SM-ID | Safety Mechanism | CVC | FZC | RZC | SC | Safety Goals |
|-------|-----------------|-----|-----|-----|----|-------------|
| SM-001 | Dual pedal sensor plausibility check | P | -- | -- | -- | SG-001 |
| SM-002 | Motor current monitoring + overcurrent cutoff | -- | -- | P | -- | SG-001, SG-006 |
| SM-003 | Cross-plausibility (torque req vs actual current) | -- | -- | -- | P | SG-001 |
| SM-004 | CAN E2E protection (CRC-8 + alive counter + data ID) | P | P | P | M | SG-001 to SG-008 |
| SM-005 | Kill relay -- energize-to-run pattern | -- | -- | -- | P | SG-001, SG-003, SG-004, SG-008 |
| SM-006 | Motor controller health monitoring | -- | -- | P | -- | SG-002 |
| SM-007 | CAN alive monitoring for RZC | P | -- | -- | S | SG-002 |
| SM-008 | Steering angle feedback monitoring | -- | P | -- | -- | SG-003 |
| SM-009 | Steering rate limiting (30 deg/s max) | -- | P | -- | -- | SG-003 |
| SM-010 | Steering angle limits (+/-43 deg software) | -- | P | -- | -- | SG-003 |
| SM-011 | Brake command monitoring (PWM feedback) | -- | P | -- | -- | SG-004 |
| SM-012 | Auto-brake on CAN timeout (100 ms) | -- | P | -- | -- | SG-004 |
| SM-013 | Motor cutoff as backup deceleration | P | -- | P | S | SG-004 |
| SM-014 | Brake command plausibility | -- | P | -- | -- | SG-005 |
| SM-015 | Motor temperature monitoring and derating | -- | -- | P | -- | SG-006 |
| SM-016 | Motor current limiting (software + BTS7960 HW) | -- | -- | P | -- | SG-006 |
| SM-017 | Lidar distance monitoring (graduated response) | -- | P | -- | -- | SG-007 |
| SM-018 | Lidar signal plausibility check | -- | P | -- | -- | SG-007 |
| SM-019 | Heartbeat monitoring (3 ECU timeouts) | -- | -- | -- | P | SG-008 |
| SM-020 | External watchdog per ECU (TPS3823) | P | P | P | P | SG-008 |
| SM-021 | SC self-test and lockstep CPU monitoring | -- | -- | -- | P | SG-008 |
| SM-022 | Vehicle state machine management | P | S | S | M | SG-001 to SG-008 |
| SM-023 | E-stop broadcast (CAN ID 0x001) | P | S | S | M | SG-001, SG-008 |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-017 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** All 23 safety mechanisms from the FSC are allocated to physical ECUs with clear P/S/M roles. The SC holds primary responsibility for cross-system monitoring (SM-003, SM-005, SM-019, SM-021) while zone controllers handle their local safety mechanisms. This separation ensures the monitor is independent from the monitored system. **Why:** The P/S/M allocation pattern makes it unambiguous which ECU owns each safety mechanism and which provides backup — an assessor can trace each safety goal through the allocation matrix to verify completeness. Every safety goal (SG-001 through SG-008) has at least one primary and one monitoring element. **Tradeoff:** Gains audit-ready clarity and defense-in-depth; the SC's monitoring scope is broad (heartbeats, cross-plausibility, kill relay, self-test) which makes the SC firmware more complex than a simple watchdog. Mitigation: SC firmware is kept at ~400 LOC with direct register access, minimizing systematic failure risk. **Alternative:** Distributing monitoring across zone controllers (peer monitoring) would reduce SC complexity but would lose the independent-monitor argument that is central to the ISO 26262 safety case. Ref: FSC SM-001 through SM-023, SG-001 through SG-008.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-017 -->

### 8.2 Freedom From Interference (FFI)

Per ISO 26262-4, elements of different ASIL levels sharing resources must demonstrate freedom from interference across spatial, temporal, and communication domains.

#### 8.2.1 Spatial FFI (Memory Isolation)

| ECU | ASIL Mix | FFI Mechanism |
|-----|----------|---------------|
| CVC | D (pedal, state) + QM (OLED) | MPU-enforced memory partitioning: safety-critical SWCs in protected region, QM display SWC in separate region. Stack canary monitoring. |
| FZC | D (brake) + C (lidar) + QM (buzzer) | MPU-enforced partitioning: ASIL D brake SWC isolated from ASIL C lidar SWC and QM buzzer SWC. Separate stack regions. |
| RZC | D (motor) + A (current/temp) | MPU-enforced partitioning: ASIL D motor control isolated from ASIL A monitoring. Independent ADC channels per function. |
| SC | C/D (all functions) | Single ASIL allocation (no mix). Minimal firmware (~400 LOC). Lockstep cores provide hardware redundancy. |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-018 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** MPU-enforced memory partitioning on each STM32G474 is the correct spatial FFI approach for mixed-ASIL SWCs on the same MCU. The Cortex-M4 MPU supports up to 8 regions, which is sufficient for the 2-3 partition scheme described (safety-critical + QM). **Why:** ISO 26262-6 Section 7 highly recommends (++) spatial isolation at ASIL D. Hardware MPU enforcement is stronger than software-only protection (stack canaries, bounds checking) because it catches all access violations at the hardware level. **Tradeoff:** Gains hardware-enforced spatial FFI that satisfies the ISO 26262 ++ recommendation; MPU configuration adds initialization complexity and the 8-region limit on Cortex-M4 constrains granularity. Cortex-M4 MPU also requires power-of-2 aligned regions, which may waste some RAM. **Alternative:** Full hardware separation (one MCU per ASIL level) would be strongest but would double the ECU count and BOM cost. Software-only isolation (no MPU) would be insufficient at ASIL D — an assessor would flag it. Ref: SYS-052, ISO 26262-6 Section 7.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-018 -->

#### 8.2.2 Temporal FFI (Timing Isolation)

| Mechanism | Description |
|-----------|-------------|
| Cooperative Scheduling | Fixed-priority task scheduling with WCET budget per runnable |
| WCET Budgets | Safety-critical runnables must complete within 80% of scheduling deadline |
| Priority Assignment | ASIL D runnables at highest priority, QM at lowest. No priority inversion possible. |
| Watchdog Gating | External watchdog (TPS3823) only fed when all runnables complete. Hang in any runnable triggers reset. |
| Independent Tick Sources | SysTick timer for scheduling, independent from application timers |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-019 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** Cooperative scheduling with WCET budgets and watchdog gating is a pragmatic temporal FFI approach for a bare-metal AUTOSAR-like system without an RTOS. The 80% WCET deadline margin provides headroom for worst-case jitter. **Why:** A cooperative scheduler avoids the complexity of preemptive RTOS qualification (SafeRTOS or similar would require TCL2/TCL3 tool qualification at ASIL D). The external watchdog (TPS3823) acts as the independent temporal monitor — if any runnable exceeds its budget and blocks the main loop, the watchdog triggers a reset. **Tradeoff:** Gains simplicity and avoids RTOS qualification burden; cooperative scheduling cannot preempt a stuck runnable — if a safety-critical runnable hangs, the QM runnable behind it is also delayed. Mitigation: the external watchdog provides the hard backstop. A weakness is that priority inversion is prevented by design (no blocking primitives) rather than by OS mechanism — this works only because there are no blocking calls in the runnables. **Alternative:** SafeRTOS (TUV-certified, ~$5k license) would provide preemptive scheduling with formal WCET guarantees, but adds cost and qualification overhead for a portfolio project. Ref: SYS-053.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-019 -->

#### 8.2.3 Communication FFI (CAN Bus Isolation)

| Mechanism | Description |
|-----------|-------------|
| CAN ID Priority | ASIL D messages have lowest CAN IDs (highest arbitration priority) |
| E2E Protection | CRC-8 + alive counter on all safety messages. Detects corruption, loss, delay, repetition. |
| SC Listen-Only | SC cannot transmit on CAN, preventing bus corruption from safety monitor |
| CAN ID Filtering | Each ECU's CAN acceptance filter configured to receive only relevant messages |
| Bus Utilization | 24% bus load ensures adequate bandwidth for safety messages under worst-case arbitration |
| Babbling Node Protection | CAN error counters + bus-off recovery with attempt limiting (3 attempts per 10 sec) |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-020 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** Communication FFI is well-addressed: priority-based CAN IDs, E2E protection, SC listen-only mode, per-ECU acceptance filtering, and babbling-node protection. The SC listen-only mode is particularly strong — it guarantees the safety monitor cannot corrupt the CAN bus by design (DCAN TEST register bit 3). **Why:** ISO 26262-4 requires demonstration that communication faults between mixed-ASIL elements cannot violate safety goals. Each mechanism addresses a specific fault type: priority handles arbitration contention, E2E handles corruption/loss/delay, listen-only prevents SC-induced bus faults, filtering reduces unnecessary processing. **Tradeoff:** Gains comprehensive communication FFI coverage; the SC's inability to transmit means it can only signal faults via the kill relay (hardware) and fault LEDs (visual), not via CAN messages to other ECUs. This is intentional — adding SC CAN TX would create a new interference path. **Alternative:** Allowing the SC to transmit a "fault detected" CAN message would enable faster software-level response from zone controllers, but would compromise the listen-only isolation argument. Ref: SYS-025, SYS-034.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-020 -->

### 8.3 Diverse Redundancy Summary

| Dimension | Zone Controllers (CVC, FZC, RZC) | Safety Controller (SC) |
|-----------|-----------------------------------|------------------------|
| Vendor | STMicroelectronics (ST) | Texas Instruments (TI) |
| CPU Architecture | ARM Cortex-M4F (single core) | ARM Cortex-R5F (dual lockstep cores) |
| Compiler | ARM GCC (arm-none-eabi-gcc) | TI ARM CGT (Code Composer Studio) |
| BSW Architecture | AUTOSAR-like layered BSW (~2,500 LOC) | Minimal direct-register firmware (~400 LOC) |
| CAN Mode | Normal (TX + RX via FDCAN) | Listen-only (RX only via DCAN TEST mode) |
| Safety HW | External watchdog (TPS3823) | Lockstep CPU + ESM + external watchdog (TPS3823) |
| Safety Manual | Generic Cortex-M4 failure rates | TUV-certified TMS570 safety manual |
| Development Tool | STM32CubeIDE + CubeMX | Code Composer Studio + HALCoGen |

This diverse redundancy architecture ensures that a systematic fault (design error, compiler bug, silicon errata) affecting the STM32 zone controllers does not simultaneously affect the TMS570 safety controller. The two independent paths to the safe state (software-controlled via CVC state machine + hardware-controlled via SC kill relay) provide defense-in-depth.

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-021 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** The diversity matrix covers all 7 dimensions (vendor, CPU architecture, compiler, BSW, CAN mode, safety HW, development tools) — this is a comprehensive diverse redundancy argument that would satisfy ISO 26262-9 Annex D CCF analysis. **Why:** Systematic faults are the primary concern at ASIL D (random hardware faults are addressed by lockstep + SPFM/LFM metrics). By ensuring no shared systematic design element between the zone controllers and the safety controller, a compiler bug, silicon errata, or BSW design error in the STM32 path cannot propagate to the TMS570 path. **Tradeoff:** Gains a textbook-quality diverse redundancy argument with 7-dimension independence; maintaining two completely separate toolchains (STM32CubeIDE + ARM GCC for ST, Code Composer Studio + HALCoGen + TI CGT for TI) doubles the build infrastructure effort and requires competency in both ecosystems. **Alternative:** Using the same vendor with different MCU families (e.g., STM32G4 + STM32H7) would share the compiler and HAL, weakening the diversity argument. A third-party safety MCU from Infineon (AURIX TC3xx) would provide equally strong diversity but at higher cost ($50+ per board). Ref: ISO 26262-9 Annex D, DFA.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-021 -->

## 9. Requirement Allocation Table

The following table maps all 56 system requirements to their allocated architecture elements.

### 9.1 Drive-by-Wire Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-001 | Dual Pedal Position Sensing | CVC (SPI1, AS5048A x2) | D |
| SYS-002 | Pedal Sensor Plausibility Monitoring | CVC (Swc_Pedal) | D |
| SYS-003 | Pedal-to-Torque Mapping | CVC (Swc_Pedal) | D |
| SYS-004 | Motor Torque Control via CAN | CVC (Com TX), RZC (Swc_Motor, BTS7960) | D |
| SYS-005 | Motor Current Monitoring | RZC (ADC1_CH1, ACS723, Swc_CurrentMonitor) | A |
| SYS-006 | Motor Temperature Monitoring and Derating | RZC (ADC1_CH2/CH3, NTC, Swc_TempMonitor) | A |
| SYS-007 | Motor Direction Control and Plausibility | RZC (Swc_Motor, TIM4 encoder) | C |
| SYS-008 | Battery Voltage Monitoring | RZC (ADC1_CH4, Swc_Battery) | QM |
| SYS-009 | Encoder Feedback for Speed Measurement | RZC (TIM4 quadrature, Swc_Motor) | QM |

### 9.2 Steering Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-010 | Steering Command Reception and Servo Control | FZC (FDCAN1 RX, TIM2_CH1, Swc_Steering) | D |
| SYS-011 | Steering Angle Feedback Monitoring | FZC (SPI2, AS5048A, Swc_Steering) | D |
| SYS-012 | Steering Return-to-Center on Fault | FZC (TIM2_CH1, Swc_Steering) | D |
| SYS-013 | Steering Rate and Angle Limiting | FZC (Swc_Steering) | C |

### 9.3 Braking Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-014 | Brake Command Reception and Servo Control | FZC (FDCAN1 RX, TIM2_CH2, Swc_Brake) | D |
| SYS-015 | Brake System Monitoring | FZC (Swc_Brake, timer capture feedback) | D |
| SYS-016 | Auto-Brake on CAN Communication Loss | FZC (Swc_Brake, autonomous) | D |
| SYS-017 | Emergency Braking from Obstacle Detection | FZC (Swc_Lidar, Swc_Brake) | C |

### 9.4 Obstacle Detection Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-018 | Lidar Distance Sensing | FZC (USART1, TFMini-S, Swc_Lidar) | C |
| SYS-019 | Graduated Obstacle Response | FZC (Swc_Lidar, Swc_Brake, buzzer) | C |
| SYS-020 | Lidar Sensor Plausibility Checking | FZC (Swc_Lidar) | C |

### 9.5 Safety Monitoring Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-021 | Heartbeat Transmission by Zone ECUs | CVC, FZC, RZC (Com TX, 50 ms) | C |
| SYS-022 | Heartbeat Timeout Detection by Safety Controller | SC (heartbeat_monitor.c, DCAN1 RX) | C |
| SYS-023 | Cross-Plausibility Check (Torque vs. Current) | SC (can_monitor.c, lookup table) | C |
| SYS-024 | Kill Relay Control (Energize-to-Run) | SC (relay.c, GIO_A0, MOSFET, relay) | D |
| SYS-025 | Safety Controller CAN Listen-Only Mode | SC (DCAN1 TEST register) | C |
| SYS-026 | Safety Controller Lockstep CPU Monitoring | SC (TMS570 lockstep, ESM) | D |
| SYS-027 | External Watchdog Monitoring (All Physical ECUs) | CVC, FZC, RZC, SC (TPS3823, GPIO toggle) | D |

### 9.6 Emergency Stop Requirement

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-028 | E-Stop Detection and Broadcast | CVC (PC13 GPIO, CAN TX 0x001), FZC + RZC (CAN RX) | B |

### 9.7 State Management Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-029 | Vehicle State Machine | CVC (Swc_VehicleState, BswM) | D |
| SYS-030 | Coordinated Mode Management via BswM | CVC (BswM master), FZC + RZC (BswM slaves) | D |

### 9.8 CAN Communication Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-031 | CAN Bus Configuration | CVC, FZC, RZC (FDCAN1), SC (DCAN1), CAN transceivers | D |
| SYS-032 | E2E Protection on Safety-Critical Messages | CVC, FZC, RZC (E2E module in BSW), SC (E2E in firmware) | D |
| SYS-033 | CAN Message Priority Assignment | CAN ID allocation table (Section 5.2) | D |
| SYS-034 | CAN Bus Loss Detection per ECU | CVC, FZC, RZC, SC (CAN error counters, RX timeout) | C |

### 9.9 Body Control Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-035 | Automatic Headlight Control | BCM (Swc_Lights, Docker) | QM |
| SYS-036 | Turn Indicator and Hazard Light Control | BCM (Swc_Indicators, Docker) | QM |

### 9.10 Diagnostics Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-037 | UDS Diagnostic Session Control | TCU (UDS server, Dcm module) | QM |
| SYS-038 | UDS Read and Clear DTC Services | TCU (UDS server, Dem module) | QM |
| SYS-039 | UDS Read/Write Data by Identifier | TCU (UDS server, Dcm module) | QM |
| SYS-040 | UDS Security Access | TCU (UDS server, seed-key) | QM |
| SYS-041 | DTC Storage and Persistence | All ECUs (Dem module), TCU (DTC store) | QM |

### 9.11 Telemetry/Cloud Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-042 | MQTT Telemetry to AWS IoT Core | Pi Gateway (cloud_publisher.py, paho-mqtt) | QM |
| SYS-043 | Edge ML Anomaly Detection | Pi Gateway (ml_inference.py, scikit-learn) | QM |

### 9.12 Operator Interface Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-044 | OLED Status Display | CVC (Swc_Dashboard, I2C1, SSD1306) | QM |
| SYS-045 | Audible Warning via Buzzer | FZC (Swc_FzcSafety, GPIO buzzer) | QM |
| SYS-046 | Fault LED Panel on Safety Controller | SC (led_panel.c, GIO_A1-A4) | C |

### 9.13 Interface Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-047 | SPI Interface -- Pedal and Steering Sensors | CVC (SPI1), FZC (SPI2), AS5048A sensors | D |
| SYS-048 | UART Interface -- Lidar Sensor | FZC (USART1, DMA), TFMini-S | C |
| SYS-049 | ADC Interface -- Current, Temp, Voltage | RZC (ADC1, 4 channels, DMA) | A |
| SYS-050 | PWM Interface -- Motor and Servos | RZC (TIM3), FZC (TIM2) | D |

### 9.14 Non-Functional Requirements

| SYS-ID | Title | Element(s) | ASIL |
|--------|-------|-----------|------|
| SYS-051 | MISRA C Compliance | All firmware (CVC, FZC, RZC, SC, BCM, ICU, TCU) | D |
| SYS-052 | Static RAM Only -- No Dynamic Allocation | All firmware | D |
| SYS-053 | WCET Within Deadline Margin | CVC, FZC, RZC (10 ms loop < 8 ms WCET) | D |
| SYS-054 | Flash Memory Utilization | All physical ECUs (< 80% flash usage) | QM |
| SYS-055 | Bidirectional Traceability Chain | All documents and code (process requirement) | D |
| SYS-056 | SAP QM Mock Integration | Pi Gateway (sap_qm_mock/app.py) | QM |

### 9.15 Allocation Summary by Element

| Element | SYS Requirements Allocated | Count |
|---------|---------------------------|-------|
| CVC | SYS-001 to SYS-004, SYS-021, SYS-027 to SYS-034, SYS-044, SYS-047, SYS-051 to SYS-055 | 22 |
| FZC | SYS-010 to SYS-021, SYS-027, SYS-031 to SYS-034, SYS-045, SYS-047, SYS-048, SYS-050, SYS-051 to SYS-055 | 24 |
| RZC | SYS-004 to SYS-009, SYS-021, SYS-027, SYS-031 to SYS-034, SYS-049, SYS-050, SYS-051 to SYS-055 | 20 |
| SC | SYS-022 to SYS-027, SYS-031, SYS-032, SYS-034, SYS-046, SYS-051 to SYS-055 | 15 |
| BCM | SYS-035, SYS-036, SYS-051 | 3 |
| ICU | SYS-041, SYS-051 | 2 |
| TCU | SYS-037 to SYS-041, SYS-051 | 6 |
| Pi Gateway | SYS-042, SYS-043, SYS-056 | 3 |
| CAN Bus | SYS-031, SYS-032, SYS-033, SYS-034 | 4 |

## 10. System Integration Strategy

### 10.1 Integration Order (Bottom-Up)

The system is integrated in a bottom-up sequence aligned with the V-model and ASPICE SYS.4:

```
Phase 1: MCAL Drivers (per ECU)
  |  Verify: CAN TX/RX, SPI read, ADC conversion, PWM output, GPIO toggle
  |
Phase 2: BSW Stack (shared)
  |  Verify: Can -> CanIf -> PduR -> Com chain sends/receives CAN message
  |  Verify: Dcm responds to UDS 0x10, Dem stores/retrieves DTC
  |  Verify: WdgM feeds watchdog only when entities healthy
  |  Verify: Rte_Read/Rte_Write pass signals between SWC and Com
  |
Phase 3: Application SWCs (per ECU)
  |  Verify: Swc_Pedal plausibility check, Swc_Motor torque control
  |  Verify: Swc_Steering closed-loop, Swc_Brake servo control
  |  Verify: Swc_Lidar graduated response
  |
Phase 4: Single ECU Integration (per ECU)
  |  Verify: Full application + BSW stack on target MCU
  |  Verify: WCET measurement, watchdog feed, self-test
  |
Phase 5: Dual ECU Integration (pairwise)
  |  Verify: CVC <-> RZC torque command chain
  |  Verify: CVC <-> FZC steer/brake command chain
  |  Verify: CVC/FZC/RZC <-> SC heartbeat monitoring
  |
Phase 6: Full Physical ECU Integration (4 ECUs)
  |  Verify: All 4 physical ECUs on CAN bus simultaneously
  |  Verify: E-stop chain, kill relay response, state machine transitions
  |  Verify: All safety mechanisms (SM-001 to SM-023)
  |
Phase 7: Simulated ECU Integration (Docker)
  |  Verify: BCM, ICU, TCU via CAN bridge or vcan0
  |  Verify: UDS diagnostic chain, DTC flow, dashboard display
  |
Phase 8: Full System Integration (7 ECUs + Pi + Cloud)
  |  Verify: End-to-end: pedal -> CVC -> RZC -> motor -> ICU display
  |  Verify: Fault -> DTC -> cloud -> Grafana -> SAP QM mock
  |  Verify: All 16 demo scenarios
  |
Phase 9: Endurance and Regression
     Verify: 30-minute continuous run, no watchdog resets, no DTC drift
     Verify: All xIL levels agree (MIL vs SIL vs PIL vs HIL comparison)
```

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-023 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** The 9-phase bottom-up integration strategy follows the V-model and aligns with ASPICE SYS.4 (System Integration and Test). Starting from MCAL drivers and building up through BSW, SWCs, single-ECU, dual-ECU, full-physical, simulated, full-system, and endurance is the textbook approach. **Why:** Bottom-up integration catches interface defects early (Phase 2-3 catches CAN/BSW stack issues before the system is wired together) and each phase builds on proven foundations from the previous phase. The xIL comparison (MIL vs SIL vs PIL vs HIL) in Phase 9 demonstrates model-code equivalence per ISO 26262-6 back-to-back testing. **Tradeoff:** Gains systematic, auditable integration with clear verification criteria at each phase; the 9-phase approach is time-intensive and requires dedicated test fixtures at each level. **Alternative:** Big-bang integration (wire everything up and debug) would be faster initially but would make fault isolation nearly impossible on a 7-ECU CAN network. Ref: ASPICE SYS.4, SYS.5.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-023 -->

### 10.2 Integration Test Points

| Test Point | Verified At | Method |
|------------|-------------|--------|
| CAN message delivery (all IDs) | Phase 5-6 | CAN analyzer trace (candump) |
| E2E protection (CRC, alive counter) | Phase 2-3 | Fault injection (corrupt CRC, repeat counter) |
| Heartbeat timeout -> kill relay | Phase 5-6 | Halt one ECU, measure relay response time |
| E-stop -> all ECUs stop | Phase 6 | Press E-stop, verify motor off + brake applied < 12 ms |
| State machine transitions | Phase 4-6 | Inject faults, verify state progression |
| Lidar -> emergency brake | Phase 4-6 | Move target into emergency zone, verify brake response |
| DTC -> cloud -> SAP QM | Phase 8 | Trigger overcurrent, verify Q-Meldung in SAP mock |
| ML anomaly detection | Phase 8 | Inject abnormal current pattern, verify cloud alert |

### 10.3 Hardware-in-the-Loop (HIL) Approach

The HIL test bench consists of:

1. **Physical ECUs**: 4 Nucleo/LaunchPad boards on a mounting plate
2. **CAN bus**: Daisy-chain wired, 120 ohm terminators at each end
3. **Real sensors**: AS5048A on pedal fixture, TFMini-S pointing at movable target
4. **Real actuators**: DC motor (free-spinning or with flywheel), servos (unloaded)
5. **Plant simulation**: Python plant model on PC sends simulated sensor feedback via CAN bridge for scenarios requiring dynamic response (closed-loop motor control)
6. **Fault injection**: Pi or PC sends corrupted CAN messages, halts heartbeats, disconnects sensors
7. **Measurement**: CAN trace logging (candump), oscilloscope for timing verification, power analyzer for current measurement

## 11. Resource Estimates

### 11.1 Per-ECU Resource Table

| Resource | CVC (STM32G474RE) | FZC (STM32G474RE) | RZC (STM32G474RE) | SC (TMS570LC43x) |
|----------|-------------------|--------------------|--------------------|-------------------|
| **Flash Available** | 512 KB | 512 KB | 512 KB | 4 MB |
| **Flash Estimate** | ~80 KB (16%) | ~90 KB (18%) | ~70 KB (14%) | ~16 KB (0.4%) |
| **RAM Available** | 128 KB | 128 KB | 128 KB | 512 KB |
| **RAM Estimate** | ~20 KB (16%) | ~22 KB (17%) | ~18 KB (14%) | ~4 KB (0.8%) |
| **CPU Clock** | 170 MHz | 170 MHz | 170 MHz | 300 MHz |
| **CPU Load Estimate** | < 5% | < 5% | < 5% | < 1% |
| **WCET (10 ms loop)** | < 2 ms | < 2 ms | < 2 ms | < 0.5 ms |
| **Peripherals Used** | FDCAN1, SPI1, I2C1, GPIO (5) | FDCAN1, SPI2, USART1, TIM2, GPIO (3) | FDCAN1, TIM3 (PWM), TIM4 (encoder), ADC1 (4ch), GPIO (4) | DCAN1, GIO (6), RTI |
| **CAN TX Messages** | 5 (0x001, 0x010, 0x100-0x103) | 5 (0x011, 0x200, 0x201, 0x210-0x211, 0x220) | 5 (0x012, 0x300-0x303) | 0 (listen-only) |
| **CAN RX Messages** | ~10 (all status + heartbeats) | ~6 (control commands + state) | ~4 (torque + state + E-stop) | ALL (listen to everything) |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-024 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** Resource utilization is extremely low across all ECUs: 14-18% flash, 14-17% RAM, <5% CPU on the STM32s, and <1% on the TMS570. This provides massive headroom (well above the SYS-054 requirement of <80% flash). WCET estimates of <2 ms in a 10 ms loop give 80% margin, satisfying the 80% WCET budget rule from temporal FFI. **Why:** Low utilization is desirable for safety: it ensures no resource contention, leaves room for future features and safety patches, and simplifies the WCET argument. The SC at 0.4% flash and 0.8% RAM with <0.5 ms WCET confirms the minimal-firmware design intent. **Tradeoff:** Gains large safety margins and future-proofing; the STM32G474RE (512 KB flash, 128 KB RAM, 170 MHz) is arguably over-specified for the current workload — a smaller STM32G431 (128 KB flash, 32 KB RAM) would suffice, but the G474 was chosen for FDCAN and the Nucleo board availability. **Alternative:** Downgrading to STM32G431 would save ~$3 per board but would tighten flash margins if the BSW stack grows significantly. Ref: SYS-053, SYS-054.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-024 -->

### 11.2 CAN Bandwidth per ECU

| ECU | TX Bandwidth (bits/sec) | RX Bandwidth (bits/sec) | Total |
|-----|------------------------|------------------------|-------|
| CVC | ~52,700 (5 msgs, 10 ms avg) | ~13,100 | ~65,800 |
| FZC | ~18,400 (5 msgs, 10-20 ms) | ~39,300 | ~57,700 |
| RZC | ~15,700 (5 msgs, 10-1000 ms) | ~26,200 | ~41,900 |
| SC | 0 (listen-only) | ~105,600 (all traffic) | ~105,600 |
| BCM | ~2,490 (3 msgs, 100 ms) | ~13,100 | ~15,590 |

### 11.3 Simulated ECU Resources

| Resource | BCM (Docker) | ICU (Docker) | TCU (Docker) |
|----------|-------------|-------------|-------------|
| RAM | ~10 MB container | ~15 MB container | ~12 MB container |
| CPU | < 1% of host | < 2% of host | < 1% of host |
| Disk | ~50 MB image | ~50 MB image | ~50 MB image |
| Network | SocketCAN (vcan0 or can bridge) | SocketCAN | SocketCAN |

### 11.4 Edge Gateway Resources

| Resource | Raspberry Pi 4 |
|----------|---------------|
| RAM Available | 2 GB |
| RAM Estimate | ~200 MB (Python, ML models, Flask) |
| CPU | Quad-core Cortex-A72 @ 1.5 GHz |
| CPU Load | < 20% (CAN + ML inference at 1 Hz) |
| Storage | 32 GB MicroSD |
| Network | Wi-Fi (MQTT to cloud), USB (CANable 2.0) |

## 12. Traceability

### 12.1 System Requirements to Architecture Elements

Complete forward traceability from system requirements to the architecture elements that implement them is provided in Section 9 (Requirement Allocation Table). Every SYS requirement (SYS-001 through SYS-056) is allocated to at least one architecture element.

### 12.2 Architecture Elements to System Requirements

Reverse traceability from architecture elements to the system requirements they implement:

| Architecture Element | Allocated SYS Requirements |
|---------------------|---------------------------|
| CVC (STM32G474RE) | SYS-001, SYS-002, SYS-003, SYS-004, SYS-021, SYS-027, SYS-028, SYS-029, SYS-030, SYS-031, SYS-032, SYS-033, SYS-034, SYS-044, SYS-047, SYS-050, SYS-051, SYS-052, SYS-053, SYS-054, SYS-055 |
| FZC (STM32G474RE) | SYS-010, SYS-011, SYS-012, SYS-013, SYS-014, SYS-015, SYS-016, SYS-017, SYS-018, SYS-019, SYS-020, SYS-021, SYS-027, SYS-031, SYS-032, SYS-033, SYS-034, SYS-045, SYS-047, SYS-048, SYS-050, SYS-051, SYS-052, SYS-053, SYS-054, SYS-055 |
| RZC (STM32G474RE) | SYS-004, SYS-005, SYS-006, SYS-007, SYS-008, SYS-009, SYS-021, SYS-027, SYS-031, SYS-032, SYS-033, SYS-034, SYS-049, SYS-050, SYS-051, SYS-052, SYS-053, SYS-054, SYS-055 |
| SC (TMS570LC43x) | SYS-022, SYS-023, SYS-024, SYS-025, SYS-026, SYS-027, SYS-031, SYS-032, SYS-034, SYS-046, SYS-051, SYS-052, SYS-053, SYS-054, SYS-055 |
| BCM (Docker) | SYS-035, SYS-036, SYS-051 |
| ICU (Docker) | SYS-041, SYS-051 |
| TCU (Docker) | SYS-037, SYS-038, SYS-039, SYS-040, SYS-041, SYS-051 |
| Pi Gateway | SYS-042, SYS-043, SYS-056 |

### 12.3 Safety Goal to Architecture Element Mapping

| Safety Goal | ASIL | Primary Element | Secondary Element(s) | Kill Relay (SC) |
|-------------|------|----------------|---------------------|----------------|
| SG-001 (Unintended acceleration) | D | CVC (pedal plausibility) | RZC (overcurrent cutoff) | SC (cross-plausibility) |
| SG-002 (Loss of drive torque) | B | RZC (motor health) | CVC (alive monitoring) | SC (heartbeat timeout) |
| SG-003 (Steering movement) | D | FZC (angle feedback) | CVC (command validation) | SC (kill relay backup) |
| SG-004 (Loss of braking) | D | FZC (brake monitoring) | CVC (motor cutoff) | SC (kill relay backup) |
| SG-005 (Unintended braking) | A | FZC (brake plausibility) | -- | -- |
| SG-006 (Motor protection) | A | RZC (current/temp) | -- | -- |
| SG-007 (Obstacle detection) | C | FZC (lidar monitoring) | -- | -- |
| SG-008 (Safety monitoring) | C | SC (heartbeat, self-test) | CVC (E-stop) | SC (kill relay) |

### 12.4 Traceability Completeness Check

| Check | Result |
|-------|--------|
| All 56 SYS requirements allocated to at least one element? | YES |
| All architecture elements have at least one SYS requirement? | YES |
| All 8 safety goals mapped to architecture elements? | YES |
| All 23 safety mechanisms allocated to ECUs? | YES (Section 8.1) |
| All CAN messages assigned IDs and E2E status? | YES (Section 5.3) |
| No orphan architecture elements (no requirements)? | YES |

<!-- HITL-LOCK START:COMMENT-BLOCK-ARCH-022 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** The traceability completeness check (all 56 SYS requirements allocated, all elements have requirements, all 8 safety goals mapped, all 23 safety mechanisms allocated, all CAN messages assigned) demonstrates full bidirectional traceability per ASPICE SYS.3 and ISO 26262-4. **Why:** An ASPICE assessor evaluating SYS.3 will check that every system requirement traces to at least one architecture element and every element traces back to at least one requirement. The completeness table provides immediate audit evidence. **Tradeoff:** Gains clear audit-readiness; maintaining this traceability table as the system evolves requires discipline — any added requirement or element must update both the allocation tables (Section 9) and this summary. **Alternative:** Tool-based traceability (DOORS, Polarion) would automate consistency checks, but for a document-driven portfolio project, the manual table is sufficient and demonstrates the traceability skill itself. Ref: SYS-055, ASPICE SYS.3 BP5.
<!-- HITL-LOCK END:COMMENT-BLOCK-ARCH-022 -->

## 13. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete system architecture: 10 architecture element categories, full CAN message matrix (24 messages), system state machine (7 states, 12 transitions), power architecture with kill relay, safety architecture (23 SM allocation, FFI, diverse redundancy), requirement allocation for all 56 SYS requirements, integration strategy, resource estimates |

