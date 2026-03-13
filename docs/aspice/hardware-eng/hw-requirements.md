---
document_id: HWREQ
title: "Hardware Requirements"
version: "1.0"
status: draft
aspice_process: HWE.1
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


# Hardware Requirements

## 1. Purpose

This document specifies the hardware requirements (HWR) for the Taktflow Zonal Vehicle Platform per Automotive SPICE 4.0 HWE.1 (Hardware Requirements Analysis). Hardware requirements define the physical, electrical, and environmental characteristics that the hardware platform shall meet to support the system functions and hardware safety requirements (HSR).

Hardware requirements are distinct from hardware safety requirements (HSR): HWRs cover the full hardware specification including non-safety aspects (power, connectors, environmental limits), while HSRs focus specifically on safety-critical hardware features and diagnostic coverage.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| HSR | Hardware Safety Requirements | 1.0 |
| TSR | Technical Safety Requirements | 1.0 |
| SYSARCH | System Architecture | 1.0 |
| PIN-MAP | Pin Mapping | 1.0 |
| BOM | Bill of Materials | 1.0 |
| HWDES | Hardware Design | 1.0 |
| SYSREQ | System Requirements Specification | 1.0 |

## 3. Requirement Conventions

### 3.1 Requirement Structure

Each requirement follows the format:

- **ID**: HWR-NNN (sequential)
- **Title**: Descriptive name
- **Traces up**: HSR-xxx, TSR-xxx, or SYS-xxx
- **Verification method**: Inspection, analysis, test, or measurement
- **Status**: draft | reviewed | approved | verified

### 3.2 Requirement Categories

- **General (HWR-001 to HWR-005)**: Operating conditions, environmental limits
- **Power (HWR-006 to HWR-012)**: Voltage regulation, power distribution, current budgets
- **CAN Bus (HWR-013 to HWR-018)**: Physical layer, bus topology, transceivers
- **Sensor Interfaces (HWR-019 to HWR-023)**: SPI, UART, ADC specifications
- **Actuator Interfaces (HWR-024 to HWR-026)**: PWM, motor driver, servo drive
- **Safety Hardware (HWR-027 to HWR-029)**: Watchdog, relay, E-stop
- **Protection (HWR-030 to HWR-033)**: Reverse polarity, overcurrent, ESD, overvoltage

---

## 4. General Requirements

### HWR-001: Operating Voltage Range

- **Traces up**: SYS-031, TSR-032
- **Verification method**: Measurement (bench supply voltage sweep)
- **Status**: draft

The system shall operate from a 12V DC nominal power supply with a permissible input voltage range of 9.6V to 14.4V (12V +/-20%). The system shall remain functional across this entire range without degradation of safety functions. Below 9.6V, the system shall enter a controlled brown-out shutdown sequence. Above 14.4V, overvoltage protection circuitry shall clamp the input to prevent damage.

**Rationale**: The +/-20% range accounts for bench supply regulation tolerance and cable voltage drop. Indoor bench operation does not experience the full automotive voltage range (6V-16V cranking/load dump).

---

### HWR-002: Operating Temperature Range

- **Traces up**: SYS-053
- **Verification method**: Inspection (indoor lab environment verification)
- **Status**: draft

The system shall operate within an ambient temperature range of 10 degrees C to 40 degrees C. This corresponds to indoor laboratory/bench conditions. Component derating shall be applied based on the upper temperature limit (40 degrees C). No conformal coating or extended temperature components are required.

**Rationale**: The platform operates exclusively on an indoor bench. The 10-40 degrees C range eliminates the need for automotive-grade temperature components (-40 to +125 degrees C), significantly reducing cost while remaining suitable for the portfolio demonstration.

---

### HWR-003: EMC Design Practices (Bench-Level)

- **Traces up**: SYS-031
- **Verification method**: Inspection (schematic and PCB layout review)
- **Status**: draft

The hardware design shall follow basic EMC practices suitable for bench-level operation:

1. **Ground plane**: All protoboard/PCB assemblies shall use a continuous ground plane or ground wire grid with maximum 20 mm spacing.
2. **Decoupling**: Every IC power pin shall have a 100 nF ceramic capacitor placed within 10 mm of the pin.
3. **CAN bus filtering**: Common-mode chokes (minimum 100 uH) on all CAN bus connections at each ECU node.
4. **Signal routing**: High-speed signals (SPI, CAN) shall be separated from power traces by at least 5 mm.
5. **Cable shielding**: CAN bus cables shall use twisted pair. Sensor cables longer than 30 cm shall use shielded cable.

No formal EMC pre-compliance testing (per CISPR 25 or ISO 11452) is required for the bench prototype. EMC design practices are preventive only.

**Rationale**: Bench-level EMC practices prevent self-interference between CAN bus, SPI, and ADC signals without the cost and effort of formal EMC testing.

---

### HWR-004: Connector and Wiring Standard

- **Traces up**: SYS-031
- **Verification method**: Inspection (visual hardware inspection)
- **Status**: draft

All inter-board wiring shall meet the following specifications:

1. **Wire gauge**: Minimum 22 AWG for signal lines, minimum 18 AWG for power lines carrying more than 1A, minimum 16 AWG for motor power lines (up to 25A).
2. **Connectors**: Dupont 2.54 mm headers for Nucleo/LaunchPad connections. Screw terminals for power distribution (12V, GND, motor power). JST-XH or equivalent locking connectors for sensor cables.
3. **Color coding**: Red = power (12V, 5V, 3.3V), Black = ground, Yellow = CAN_H, Green = CAN_L, Blue = SPI, White = UART, Orange = PWM.
4. **Strain relief**: All cables attached to the mounting plate shall have strain relief (cable tie or clip) within 50 mm of the connector.
5. **Labeling**: Each cable shall be labeled at both ends with the signal name and destination ECU.

**Rationale**: Standardized wiring reduces integration errors and simplifies troubleshooting during hardware bring-up.

---

### HWR-005: Mounting and Physical Layout

- **Traces up**: SYS-031
- **Verification method**: Inspection (physical layout review)
- **Status**: draft

All ECUs and peripheral circuits shall be mounted on a common base plate (minimum 400 mm x 300 mm plywood, acrylic, or aluminum sheet):

1. **ECU spacing**: Minimum 50 mm between Nucleo/LaunchPad boards to allow airflow and cable routing.
2. **CAN bus routing**: CAN bus wiring shall follow a linear topology along one edge of the base plate, with stub cables to each ECU not exceeding 100 mm.
3. **Motor mounting**: The DC motor and BTS7960 module shall be mounted at one end of the base plate with the motor output shaft accessible for encoder and load attachment.
4. **Safety hardware**: The kill relay, E-stop button, and SC fault LED panel shall be mounted in an accessible location on the base plate edge closest to the operator.
5. **Power supply**: The bench PSU shall be placed off the base plate with a single power cable entry point.
6. **Total CAN bus length**: Maximum 2 meters end-to-end for the linear bus.

**Rationale**: A structured physical layout minimizes CAN bus stub lengths and provides clear operator access to safety-critical controls (E-stop, fault LEDs, kill relay indicator).

---

## 5. Power Requirements

### HWR-006: 12V Main Power Rail

- **Traces up**: SYS-031, HSR-SC-006
- **Verification method**: Measurement (voltage at each ECU under full load)
- **Status**: draft

The 12V main power rail shall be supplied by a benchtop DC power supply rated for at least 12V / 10A continuous output. The rail shall distribute power to:

1. Kill relay coil circuit (via SC MOSFET driver)
2. Buck converter (12V to 5V)
3. Buck converter (12V to 3.3V)
4. Nucleo board Vin inputs (3 boards, via onboard LDO)
5. TMS570 LaunchPad (via separate path, NOT through kill relay)

The 12V rail shall use 16 AWG wiring from the PSU to the distribution point, with individual 18 AWG drops to each consumer. A 10A fuse shall protect the main 12V rail at the PSU output.

**Rationale**: The 10A rating provides headroom for the motor (up to 25A peak through the kill relay path) plus all auxiliary loads. The main fuse prevents catastrophic failure if a short circuit occurs.

---

### HWR-007: 12V Actuator Power Rail (Kill Relay Gated)

- **Traces up**: HSR-SC-001, HSR-RZC-004, TSR-029
- **Verification method**: Measurement (voltage at BTS7960 and servos with relay open/closed)
- **Status**: draft

The 12V actuator power rail shall be gated by the kill relay (energize-to-run, HSR-SC-001). This rail shall supply:

1. BTS7960 motor driver VCC (up to 43A peak, 25A continuous rated)
2. Steering servo power (via 6V regulator, 2A continuous)
3. Brake servo power (via 6V regulator, 2A continuous)

The actuator rail shall be isolated from the MCU power rails. When the kill relay is open (de-energized), no voltage shall be present on the actuator rail. The total current capacity of the kill relay contacts shall be at least 30A.

**Rationale**: Gating actuator power through the kill relay ensures that SC safety actions (relay de-energize) physically remove power from all actuators simultaneously, regardless of software state on zone ECUs.

---

### HWR-008: 5V Power Rail

- **Traces up**: HSR-FZC-003, SYS-048
- **Verification method**: Measurement (5V rail voltage under load)
- **Status**: draft

A 5V regulated rail shall be provided by an LM2596-based buck converter module (or equivalent) with the following specifications:

1. **Input**: 12V main rail (NOT gated by kill relay)
2. **Output**: 5.0V +/-2% (4.9V to 5.1V)
3. **Current capacity**: 3A continuous minimum
4. **Ripple**: Less than 50 mV peak-to-peak
5. **Decoupling**: 470 uF electrolytic capacitor at output plus 100 nF ceramic

Consumers on the 5V rail:

| Consumer | Current (typical) | Current (peak) |
|----------|-------------------|----------------|
| TFMini-S lidar | 100 mA | 140 mA |
| Raspberry Pi 4 (via USB-C) | 600 mA | 1200 mA |
| **Total** | **700 mA** | **1340 mA** |

**Rationale**: The LM2596 buck converter is widely available, inexpensive, and provides sufficient efficiency (up to 92%) for the 5V loads. The 3A rating provides headroom for peak Pi consumption.

---

### HWR-009: 3.3V Sensor Power Rail

- **Traces up**: HSR-CVC-001, HSR-FZC-001, HSR-RZC-001
- **Verification method**: Measurement (3.3V rail voltage and noise)
- **Status**: draft

A 3.3V regulated rail shall be provided by an LM2596-based buck converter module with the following specifications:

1. **Input**: 12V main rail (NOT gated by kill relay)
2. **Output**: 3.3V +/-2% (3.234V to 3.366V)
3. **Current capacity**: 1A continuous minimum
4. **Ripple**: Less than 30 mV peak-to-peak
5. **Decoupling**: 220 uF electrolytic at output plus 100 nF ceramic

Consumers on the 3.3V rail:

| Consumer | Current (typical) |
|----------|-------------------|
| 3x AS5048A angle sensors | 3 x 14 mA = 42 mA |
| ACS723 current sensor | 10 mA |
| 4x TPS3823 watchdog ICs | 4 x 35 uA = 0.14 mA |
| 3x TJA1051T/3 CAN transceivers (standby share) | 3 x 5 mA = 15 mA |
| SN65HVD230 CAN transceiver | 6 mA |
| NTC voltage dividers (3x) | 3 x 0.33 mA = 1 mA |
| **Total** | **~74 mA** |

**Rationale**: The external 3.3V rail supplements the Nucleo onboard LDO (which powers the MCU). Sensors and CAN transceivers receive power from the external rail to avoid overloading the Nucleo LDO (typically rated at 500 mA).

---

### HWR-010: CVC Power Budget

- **Traces up**: SYS-001, SYS-029
- **Verification method**: Measurement (CVC total current draw)
- **Status**: draft

The CVC (STM32G474RE Nucleo-64) power budget:

| Domain | Supply | Consumer | Current |
|--------|--------|----------|---------|
| MCU logic | Nucleo onboard 3.3V LDO | STM32G474RE (170 MHz, all peripherals active) | 80 mA |
| SPI sensors | External 3.3V rail | 2x AS5048A (14 mA each) + pull-ups | 35 mA |
| I2C display | External 3.3V rail | SSD1306 OLED (25 mA typical at 50% pixels) | 25 mA |
| CAN transceiver | External 3.3V rail | TJA1051T/3 (active) | 70 mA |
| Watchdog | External 3.3V rail | TPS3823 | 0.035 mA |
| Status LEDs | MCU 3.3V | 2x LED (10 mA each) | 20 mA |
| **Total** | | | **~230 mA** |

The Nucleo board shall be powered via USB (5V) or external Vin (7-12V). Brown-out detection shall be enabled at the 2.7V threshold on the STM32G474RE (BOR level 1).

---

### HWR-011: FZC Power Budget

- **Traces up**: SYS-010, SYS-014, SYS-018
- **Verification method**: Measurement (FZC total current draw)
- **Status**: draft

The FZC (STM32G474RE Nucleo-64) power budget:

| Domain | Supply | Consumer | Current |
|--------|--------|----------|---------|
| MCU logic | Nucleo onboard 3.3V LDO | STM32G474RE (170 MHz) | 80 mA |
| SPI sensor | External 3.3V rail | AS5048A (steering angle) | 14 mA |
| CAN transceiver | External 3.3V rail | TJA1051T/3 | 70 mA |
| Watchdog | External 3.3V rail | TPS3823 | 0.035 mA |
| Buzzer driver | MCU 3.3V via MOSFET | 2N7002 gate + buzzer (active) | 30 mA |
| Servo power | 12V actuator rail via 6V regulator | Steering MG996R + Brake MG996R | 2 x 500 mA = 1000 mA peak |
| Lidar | External 5V rail | TFMini-S | 100 mA |
| **Total (logic)** | | | **~194 mA** |
| **Total (servo peak)** | | | **~1000 mA** (separate rail) |

The servo power draw is on the 12V actuator rail (via 6V regulator), not on the MCU power domain.

---

### HWR-012: RZC Power Budget

- **Traces up**: SYS-004, SYS-005, SYS-006
- **Verification method**: Measurement (RZC total current draw)
- **Status**: draft

The RZC (STM32G474RE Nucleo-64) power budget:

| Domain | Supply | Consumer | Current |
|--------|--------|----------|---------|
| MCU logic | Nucleo onboard 3.3V LDO | STM32G474RE (170 MHz) | 80 mA |
| CAN transceiver | External 3.3V rail | TJA1051T/3 | 70 mA |
| Watchdog | External 3.3V rail | TPS3823 | 0.035 mA |
| Current sensor | External 3.3V rail | ACS723 | 10 mA |
| NTC dividers (2x) | External 3.3V rail | Resistor dividers | 0.66 mA |
| Battery divider | External 3.3V rail | Resistor divider | 0.07 mA |
| Encoder pull-ups | External 3.3V rail | 2x 10k to 3.3V | 0.66 mA |
| Motor driver | 12V actuator rail | BTS7960 (25A continuous) | 25,000 mA peak |
| **Total (logic)** | | | **~162 mA** |
| **Total (motor peak)** | | | **~25 A** (separate rail) |

The motor power draw is on the 12V actuator rail (gated by kill relay), completely separate from MCU power.

---

## 6. CAN Bus Requirements

### HWR-013: CAN Physical Layer Compliance

- **Traces up**: SYS-031, HSR-CVC-004, HSR-FZC-005, HSR-RZC-005, HSR-SC-004
- **Verification method**: Measurement (CAN bus waveform on oscilloscope)
- **Status**: draft

The CAN physical layer shall comply with ISO 11898-2 (high-speed CAN):

1. **Differential voltage**: CAN_H dominant = 3.5V (nominal), CAN_L dominant = 1.5V (nominal), differential = 2.0V. Recessive: both lines at 2.5V.
2. **Bit rate**: 500 kbps +/-0.1% (crystal/PLL accuracy on STM32 and TMS570).
3. **Sample point**: 80% of bit time (10 Tq: 1 sync + 7 NTSEG1 + 2 NTSEG2).
4. **Bit timing**: STM32 FDCAN prescaler=34 (170 MHz/34=5 MHz), TMS570 DCAN BRP=15 (75 MHz/15=5 MHz), both at 10 Tq per bit for 500 kbps with 80% sample point.
5. **Bus impedance**: 60 ohm (two 120 ohm terminators in parallel at each end).
6. **Maximum propagation delay**: Within CAN 2.0B specification for 500 kbps at 2 m bus length (well within limits).

---

### HWR-014: CAN Bus Topology and Termination

- **Traces up**: SYS-031
- **Verification method**: Measurement (termination resistance end-to-end)
- **Status**: draft

The CAN bus shall use a linear (daisy-chain) topology:

1. **Node order**: CVC --- FZC --- RZC --- SC (physical wiring order on base plate).
2. **Termination**: 120 ohm resistor at each end of the bus (at CVC node and at SC node). Total bus termination: 60 ohm as measured between CAN_H and CAN_L with all nodes powered off.
3. **Stub length**: Maximum 100 mm from the main bus line to each ECU's CAN transceiver pins.
4. **Total bus length**: Maximum 2 meters end-to-end.
5. **Additional nodes**: The Raspberry Pi (via CANable 2.0) shall be connected as a T-tap at any point on the main bus, with stub length not exceeding 100 mm.
6. **PC CAN bridge**: A second CANable 2.0 on a development PC connects as a T-tap for CAN analysis and Docker ECU bridging, stub not exceeding 100 mm.

**Rationale**: Linear topology with short stubs is the simplest CAN bus implementation and is standard for short bus lengths.

---

### HWR-015: CAN Wire and Cable Specification

- **Traces up**: SYS-031
- **Verification method**: Inspection (cable type verification)
- **Status**: draft

CAN bus wiring shall use:

1. **Wire type**: 22 AWG twisted pair (1 twist per 25 mm minimum).
2. **Conductor**: Stranded copper (not solid core), for mechanical flexibility.
3. **Insulation**: PVC or silicone rated for at least 60 degrees C.
4. **Color**: Yellow = CAN_H, Green/Blue = CAN_L (per SAE J1939 convention).
5. **Shielding**: Optional for bench length (2 m). If used, shield connected to chassis ground at one end only.

---

### HWR-016: STM32 CAN Transceiver (TJA1051T/3)

- **Traces up**: HSR-CVC-004, HSR-FZC-005, HSR-RZC-005, SYS-031
- **Verification method**: Test (CAN message TX/RX at 500 kbps)
- **Status**: draft

Each STM32-based ECU (CVC, FZC, RZC) shall use a TJA1051T/3 CAN transceiver (NXP) or equivalent breakout module:

1. **Supply voltage**: 3.3V (VCC = 3.0V to 3.6V range per datasheet).
2. **I/O voltage**: 3.3V compatible TXD/RXD (no level shifting needed for STM32 3.3V logic).
3. **CAN FD capability**: The TJA1051T/3 supports CAN FD bit rates up to 5 Mbps. Only classic CAN at 500 kbps is used in this project, but the transceiver provides upgrade headroom.
4. **Standby mode**: STB pin active-low. For always-active operation, tie STB to GND (active). If software-controlled sleep is desired, connect STB to a GPIO.
5. **FDCAN connections**: TXD connected to FDCAN1_TX, RXD connected to FDCAN1_RX (pin assignments per PIN-MAP document).
6. **ESD rating**: +/-8 kV HBM on CAN bus pins per IEC 61000-4-2.
7. **Fault tolerance**: Survives permanent short of CANH or CANL to GND or VBAT per ISO 11898-2.
8. **Decoupling**: 100 nF ceramic capacitor on VCC pin within 10 mm.

---

### HWR-017: TMS570 CAN Transceiver (SN65HVD230)

- **Traces up**: HSR-SC-004, SYS-031
- **Verification method**: Test (CAN RX at 500 kbps in listen-only mode)
- **Status**: draft

The SC (TMS570LC43x) shall use an SN65HVD230 CAN transceiver (TI):

1. **Supply voltage**: 3.3V (VCC = 3.0V to 3.6V range per datasheet).
2. **I/O voltage**: 3.3V logic compatible with TMS570 DCAN1 pins.
3. **Speed**: Supports up to 1 Mbps. Used at 500 kbps.
4. **Slope control**: Rs pin connected to GND for maximum speed (no slope limiting at 500 kbps).
5. **DCAN connections**: TXD connected to DCAN1TX, RXD connected to DCAN1RX (via edge connector pins on LaunchPad).
6. **Listen-only note**: In DCAN silent mode, the TMS570 drives TXD recessive (high). The transceiver transmits a recessive level on CAN_H/CAN_L, which has no effect on bus arbitration.
7. **Decoupling**: 100 nF ceramic capacitor on VCC pin.

**Rationale**: The SN65HVD230 is a TI part, maintaining vendor consistency with the TMS570 (both TI). It is 3.3V native, eliminating the need for level shifting.

---

### HWR-018: CAN Bus ESD and Transient Protection

- **Traces up**: HSR-CVC-004, SYS-031
- **Verification method**: Inspection (schematic review for TVS diode placement)
- **Status**: draft

Each CAN bus connection shall include ESD/transient protection:

1. **TVS diodes**: Bidirectional TVS diode array (e.g., PESD1CAN or NUP2105L) on CAN_H and CAN_L at each ECU node, rated for minimum +/-24V standoff voltage and +/-8 kV ESD (IEC 61000-4-2).
2. **Common-mode choke**: Minimum 100 uH common-mode inductance on CAN_H/CAN_L pair at each ECU node, placed between the transceiver and the bus connector.
3. **Placement**: TVS diodes shall be placed between the common-mode choke and the bus connector (closest to the bus wires).

**Rationale**: TVS diodes and common-mode chokes provide a standard protection scheme for CAN bus nodes. While the bench environment has low ESD risk, the protection demonstrates best practice and prevents damage from accidental static discharge during hardware handling.

---

## 7. Sensor Interface Requirements

### HWR-019: SPI Interface for AS5048A Angle Sensors

- **Traces up**: HSR-CVC-001, HSR-FZC-001, TSR-001, TSR-010, SYS-047
- **Verification method**: Test (SPI read of AS5048A angle register at 1 MHz)
- **Status**: draft

The SPI interface for AS5048A magnetic angle sensors shall meet:

1. **SPI mode**: Mode 1 (CPOL=0, CPHA=1) per AS5048A datasheet.
2. **Clock speed**: 1 MHz (well within the AS5048A maximum of 10 MHz). Lower speed improves noise immunity on protoboard wiring.
3. **Word size**: 16-bit per transaction (command/response frame).
4. **Chip-select**: Active-low, GPIO-controlled (not hardware NSS). 10k ohm pull-up resistor on each CS line to 3.3V ensures sensor deselection on MCU reset.
5. **MOSI**: Connected (AS5048A requires command write before data read). Master-out line driven by STM32.
6. **MISO**: Connected. Master-in line reads sensor response. No pull-up needed (AS5048A drives MISO when selected).
7. **CVC configuration**: SPI1 bus with 2 AS5048A sensors (CS1 = PA4, CS2 = PA15). MOSI = PA7, MISO = PA6, SCK = PA5.
8. **FZC configuration**: SPI1 bus with 1 AS5048A sensor (CS = PA4). MOSI = PA7, MISO = PA6, SCK = PA5.
9. **Decoupling**: 100 nF ceramic capacitor on each AS5048A VDD pin, within 10 mm.
10. **Cable length**: SPI cable from MCU to sensor shall not exceed 300 mm at 1 MHz. For longer runs, reduce SPI clock to 500 kHz.

---

### HWR-020: UART Interface for TFMini-S Lidar

- **Traces up**: HSR-FZC-003, TSR-018, SYS-048
- **Verification method**: Test (UART reception of TFMini-S 9-byte frame at 115200 baud)
- **Status**: draft

The UART interface for the TFMini-S lidar sensor shall meet:

1. **Baud rate**: 115200 bps (TFMini-S default).
2. **Frame format**: 8 data bits, no parity, 1 stop bit (8N1).
3. **Logic levels**: 3.3V LVTTL (TFMini-S output is 3.3V compatible with STM32 inputs). No level shifter required.
4. **Power**: TFMini-S requires 5V power (100 mA typical, 140 mA peak). Power from the external 5V rail, not from the Nucleo 5V pin.
5. **FZC pin assignment**: USART2 (PA2 = TX to sensor, PA3 = RX from sensor). DMA reception configured on RX channel for background frame capture.
6. **Connector**: 4-pin keyed connector (VCC 5V, GND, TX, RX) to prevent reverse connection. TFMini-S wire colors: Red = 5V, Black = GND, White = TX (sensor output), Green = RX (sensor input).
7. **Cable length**: Maximum 1 meter. Shielded cable for runs exceeding 500 mm.
8. **ESD protection**: TVS diodes (3.3V bidirectional) on RX and TX lines.

---

### HWR-021: ADC Interface for Current Sensing (ACS723)

- **Traces up**: HSR-RZC-001, TSR-006, SYS-049
- **Verification method**: Test (ADC reading at known current values)
- **Status**: draft

The ADC interface for the ACS723 Hall-effect current sensor shall meet:

1. **ADC channel**: ADC1 channel 1 (PA0) on RZC.
2. **Resolution**: 12-bit (4096 counts over 0-3.3V range).
3. **Sample rate**: Minimum 1 kHz (1 ms conversion period) for current monitoring.
4. **Conversion time**: Less than 5 us per sample (ADC clock = 170 MHz / 4 = 42.5 MHz, 12.5 ADC cycles = 0.29 us per conversion).
5. **ACS723 output**: Analog voltage, sensitivity = 100 mV/A (ACS723LLCTR-20AB-T, 20A variant), zero-current output = VCC/2 = 1.65V for bidirectional measurement.
6. **ADC input impedance**: ACS723 output impedance is approximately 1k ohm. STM32 ADC input requires less than 50k ohm source impedance at the specified sample time.
7. **Filtering**: 1 nF capacitor on ACS723 output pin (bandwidth filter, 80 kHz) plus 100 nF capacitor at ADC input (anti-aliasing, 1.6 kHz cutoff with 1k source impedance).
8. **ADC reference**: VREF+ connected to VDDA (3.3V) with 1 uF + 100 nF decoupling.

---

### HWR-022: ADC Interface for Temperature Sensing (NTC)

- **Traces up**: HSR-RZC-002, TSR-008, SYS-049
- **Verification method**: Test (ADC reading at room temperature vs. calculated value)
- **Status**: draft

The ADC interface for NTC thermistor temperature measurement shall meet:

1. **ADC channels**: ADC1 channel 2 (PA1) = motor winding NTC, ADC1 channel 3 (PA2) = board/secondary NTC on RZC.
2. **NTC specification**: 10k ohm at 25 degrees C, beta (B25/85) = 3950.
3. **Voltage divider**: 10k ohm precision resistor (1% tolerance) in series with NTC. NTC between ADC input and GND, fixed resistor between ADC input and 3.3V.
4. **ADC range**: At 25 degrees C, NTC = 10k, divider output = 1.65V (midscale). At 0 degrees C, NTC ~ 32.6k, output ~ 0.78V. At 100 degrees C, NTC ~ 0.68k, output ~ 3.07V.
5. **Filtering**: 100 nF capacitor on each ADC input.
6. **Sample rate**: 10 Hz minimum (100 ms period), consistent with thermal time constant.
7. **Failure detection**: Open NTC reads as VCC (3.3V) = below -30 degrees C equivalent. Short NTC reads as GND (0V) = above 150 degrees C equivalent. Both detectable by software range check.

---

### HWR-023: ADC Interface for Battery Voltage Monitoring

- **Traces up**: HSR-RZC-007, TSR-009, SYS-049
- **Verification method**: Test (ADC reading at known battery voltage)
- **Status**: draft

The ADC interface for battery voltage monitoring shall meet:

1. **ADC channel**: ADC1 channel 4 (PA3) on RZC.
2. **Voltage divider**: 47k ohm upper / 10k ohm lower, scaling 12V to approximately 2.1V (within 3.3V ADC range). Maximum input voltage before ADC clamp: 18.8V.
3. **Resistor tolerance**: 1% for both divider resistors, providing +/-2% measurement accuracy.
4. **Filtering**: 100 nF capacitor across the lower resistor (ADC input to GND).
5. **Overvoltage protection**: 3.3V Zener diode (BZX84C3V3) across the ADC input to clamp voltage spikes.
6. **Sample rate**: 1 Hz minimum (1000 ms period), consistent with slow battery voltage changes.
7. **Measurement range**: 0V to 18V input (0V to 3.16V at ADC).

---

## 8. Actuator Interface Requirements

### HWR-024: PWM Interface for Servos (Steering and Brake)

- **Traces up**: HSR-FZC-002, HSR-FZC-006, TSR-012, TSR-015, SYS-050
- **Verification method**: Test (PWM waveform on oscilloscope, servo response)
- **Status**: draft

The PWM interface for MG996R metal-gear servos shall meet:

1. **PWM frequency**: 50 Hz (20 ms period), standard hobby servo specification.
2. **Pulse width range**: 1.0 ms (0 degrees) to 2.0 ms (180 degrees). Effective range for the application: 1.0 ms to 2.0 ms centered at 1.5 ms (90 degrees = center).
3. **Resolution**: Timer configured for at least 0.5 us resolution (better than 0.1 degree angular resolution).
4. **FZC pin assignments**: Steering servo = TIM2_CH1 (PA0), Brake servo = TIM2_CH2 (PA1). Both channels on the same timer for synchronized update.
5. **Voltage level**: 3.3V PWM output from STM32 (compatible with MG996R signal input which accepts 3.0V-5.0V logic).
6. **Servo power**: 4.8V to 7.2V (use 6V regulated rail from 12V actuator bus). Current: 500 mA typical, 2.5A stall per servo.
7. **Protection**: 3A fast-blow fuse per servo power line. Flyback diode (1N5819 Schottky) across each servo power connector.

---

### HWR-025: BTS7960 H-Bridge Motor Driver Interface

- **Traces up**: HSR-RZC-004, TSR-005, SYS-050
- **Verification method**: Test (motor rotation in both directions at varying PWM duty)
- **Status**: draft

The BTS7960 H-bridge motor driver interface shall meet:

1. **PWM inputs**: RPWM and LPWM, driven by STM32 timer outputs. PWM frequency = 20 kHz (above audible range to prevent motor whine).
2. **Enable lines**: R_EN and L_EN, driven by STM32 GPIO. 10k ohm pull-down resistors on both lines to GND ensure motor driver is disabled on MCU reset or GPIO floating.
3. **Logic voltage**: 3.3V compatible (BTS7960 module accepts 3.3V or 5V logic).
4. **Motor voltage**: 12V from actuator rail (kill relay gated).
5. **Motor current**: Rated for 43A peak, 25A continuous. The application motor draws up to 25A peak (stall).
6. **Current sense**: BTS7960 IS_R and IS_L pins connected to ADC channels for backup current monitoring (primary current sense via external ACS723).
7. **Dead-time**: Minimum 10 us dead-time between direction changes (both RPWM and LPWM at 0% for at least 10 us). Enforced by both hardware timer dead-time insertion and software guards.
8. **Heat dissipation**: BTS7960 module includes onboard heatsink. Mount with adequate airflow (natural convection sufficient at 40 degrees C ambient for 25A continuous).

---

### HWR-026: Motor Encoder Interface

- **Traces up**: HSR-RZC-006, TSR-040, SYS-009
- **Verification method**: Test (encoder count increments during motor rotation)
- **Status**: draft

The quadrature encoder interface for motor speed and direction measurement shall meet:

1. **Timer mode**: TIM4 configured in encoder mode (channels 1 and 2 for quadrature A and B).
2. **RZC pin assignments**: TIM4_CH1 (PB6) = Encoder A, TIM4_CH2 (PB7) = Encoder B.
3. **Input filtering**: Hardware digital filter on TIM4 inputs (ICF = 0x0F, maximum filtering, 8 samples at fDTS/32). Rejects noise pulses shorter than encoder minimum pulse width.
4. **Pull-up resistors**: 10k ohm pull-up to 3.3V on both encoder inputs (required for open-collector encoder outputs).
5. **ESD protection**: TVS diodes (3.3V bidirectional) on both encoder inputs.
6. **Encoder specification**: 11 PPR (pulses per revolution) or higher. With quadrature decoding (4x), provides 44 counts per revolution minimum.
7. **Maximum RPM**: At 10,000 RPM with 11 PPR encoder, frequency = 10000 * 11 / 60 = 1833 Hz. Well within TIM4 input capture capability at 170 MHz.
8. **Cable**: Shielded cable from encoder to RZC if distance exceeds 200 mm.

---

## 9. Safety Hardware Requirements

### HWR-027: External Watchdog (TPS3823) per ECU

- **Traces up**: HSR-CVC-002, HSR-FZC-004, HSR-RZC-003, HSR-SC-002, TSR-032
- **Verification method**: Test (verify MCU reset on watchdog timeout)
- **Status**: draft

Each physical ECU (CVC, FZC, RZC, SC) shall include a TPS3823DBVR external watchdog IC:

1. **Part number**: TPS3823DBVR (TI), SOT-23-5 package, available on breakout boards.
2. **Timeout period**: 1.6 seconds, set by external capacitor CT = 100 nF (per TPS3823 datasheet Table 1). Tolerance: +/-20% (range 1.28s to 1.92s).
3. **WDI (watchdog input)**: Connected to a dedicated MCU GPIO output. Toggle between HIGH and LOW at a rate faster than the timeout period.
4. **RESET output**: Active-low, push-pull. Connected to the MCU hardware reset pin (NRST on STM32, nRST on TMS570). 100 nF debounce capacitor from RESET to GND. Note: TPS3823 is push-pull; use TPS3824 if open-drain (wire-OR) is needed.
5. **MR (manual reset) input**: Tied to VDD (no manual reset required).
6. **VDD**: Connected to 3.3V rail with 100 nF decoupling capacitor.
7. **Power-on reset**: TPS3823 provides a minimum 200 ms power-on reset pulse, ensuring the MCU starts from a known state.
8. **Independent oscillator**: The TPS3823 uses an internal RC oscillator, independent of the MCU clock. This provides timing diversity for clock-fault detection.

**Quantity required**: 4 (one per physical ECU).

---

### HWR-028: Kill Relay Assembly

- **Traces up**: HSR-SC-001, TSR-029, SYS-024
- **Verification method**: Test (relay open/close timing, contact resistance)
- **Status**: draft

The kill relay assembly shall meet:

1. **Relay type**: Automotive-grade SPST-NO (normally-open) relay. Contact rating: minimum 30A at 14VDC. Coil voltage: 12V DC. Coil resistance: 70-120 ohm typical.
2. **MOSFET driver**: IRLZ44N N-channel MOSFET (or equivalent logic-level MOSFET, VGS(th) less than 2V for direct 3.3V gate drive). VDS rating: 55V minimum. RDS(on): less than 22 mohm. Drain to relay coil low-side, source to GND.
3. **Gate resistor**: 100 ohm series resistor between SC GIO_A0 and MOSFET gate (limits gate current during switching).
4. **Gate pull-down**: 10k ohm resistor from gate to GND. Ensures MOSFET is OFF (relay open) when GIO_A0 is floating, high-impedance, or during SC power-up.
5. **Flyback diode**: 1N4007 (or equivalent, 1A, 1000V reverse voltage) across the relay coil, cathode to 12V positive side. Limits coil de-energization voltage spike and ensures relay dropout within 10 ms.
6. **Relay mounting**: Relay mounted on the base plate with screw terminals for 12V actuator rail input and output connections.
7. **Dropout time**: Relay contacts shall open within 10 ms of coil de-energization. Verify with actual relay on oscilloscope.
8. **Contact monitoring** (optional): A voltage divider (47k/10k) on the actuator-side of the relay contacts, connected to an ADC input on the SC, can provide relay state readback (actuator rail voltage present/absent).

---

### HWR-029: E-Stop Button Circuit

- **Traces up**: HSR-CVC-003, TSR-033, SYS-028
- **Verification method**: Test (E-stop activation and GPIO detection)
- **Status**: draft

The E-stop (emergency stop) button circuit shall meet:

1. **Button type**: Panel-mount, normally-closed (NC) push button with locking mushroom head (red color, per IEC 60947-5-5). Minimum contact rating: 1A at 250VAC / 2A at 30VDC.
2. **Wiring (fail-safe)**: NC button contact wired between GPIO PC13 and GND. In the resting (not pressed) state, PC13 is held LOW through the closed button contact. Pressing the button opens the contact, and the internal pull-up drives PC13 HIGH. A broken or disconnected wire causes PC13 to go HIGH (E-stop activated) -- this is inherently fail-safe.
3. **Pull-up**: STM32G474RE internal pull-up on PC13 (approximately 40k ohm). No external pull-up required.
4. **Hardware debounce**: External RC filter on PC13. Series resistor R = 10k ohm, capacitor C = 100 nF from PC13 to GND. Time constant = 1 ms, sufficient to suppress switch bounce.
5. **ESD protection**: 3.3V bidirectional TVS diode on PC13 (protects against static from button wiring).
6. **Interrupt**: PC13 configured as EXTI (external interrupt), rising-edge triggered (button press = rising edge due to NC wiring).
7. **Button location**: Mounted on the base plate edge, within immediate reach of the operator.
8. **Cable**: If the button is more than 300 mm from the CVC board, use shielded cable with the shield connected to GND at the CVC end only.

---

## 10. Protection Requirements

### HWR-030: Reverse Polarity Protection

- **Traces up**: HSR-SC-006, SYS-031
- **Verification method**: Test (reverse polarity connection, verify no damage)
- **Status**: draft

Reverse polarity protection shall be provided at the main 12V power entry point:

1. **Method**: Series Schottky diode (e.g., SB560, 5A 60V Schottky, forward drop approximately 0.45V at 5A) on the 12V positive rail, before the distribution bus. Alternatively, a P-channel MOSFET reverse polarity protection circuit may be used for lower voltage drop.
2. **Voltage drop**: Maximum 0.5V at full load (10A). This reduces the effective minimum operating voltage to 9.1V (from 9.6V input with 0.5V drop).
3. **Rating**: The protection device shall be rated for at least 10A continuous and 60V reverse voltage.
4. **Location**: At the 12V power entry point on the base plate, before any other circuitry.

**Rationale**: Reverse polarity protection prevents damage from accidentally connecting the bench supply with reversed polarity. Schottky diode is simplest; MOSFET solution has lower drop but higher complexity.

---

### HWR-031: Overcurrent Protection (Fuses)

- **Traces up**: HSR-RZC-004, SYS-031
- **Verification method**: Inspection (fuse rating verification)
- **Status**: draft

Fuse protection shall be provided at the following locations:

| Location | Fuse Rating | Type | Purpose |
|----------|-------------|------|---------|
| 12V main rail (PSU output) | 10A | Blade fuse (ATC/ATO) | Protect PSU and main wiring |
| 12V actuator rail (after kill relay) | 30A | Blade fuse (ATC/ATO) | Protect relay contacts and motor wiring |
| Steering servo power | 3A | Glass tube (fast-blow) | Protect servo and wiring from short |
| Brake servo power | 3A | Glass tube (fast-blow) | Protect servo and wiring from short |
| 5V rail (buck converter output) | 3A | Resettable PTC (polyfuse) | Protect lidar and Pi from overcurrent |

All fuses shall be accessible for replacement without disassembling the base plate.

**Rationale**: Distributed fusing provides protection at each power domain, limiting the scope of any short-circuit failure. The motor fuse (30A) allows the BTS7960 to operate at its rated current while still protecting against catastrophic shorts.

---

### HWR-032: ESD Protection at External Interfaces

- **Traces up**: HSR-CVC-003, HSR-FZC-003, HWR-018
- **Verification method**: Inspection (TVS diode placement on schematic)
- **Status**: draft

ESD protection TVS diodes shall be placed on all external-facing signal lines:

| Interface | Pin(s) | TVS Type | Standoff Voltage |
|-----------|--------|----------|-----------------|
| CAN bus (all nodes) | CAN_H, CAN_L | PESD1CAN or NUP2105L | 24V |
| E-stop button | PC13 | 3.3V bidirectional TVS | 3.3V |
| TFMini-S UART | PA2 (TX), PA3 (RX) | 3.3V bidirectional TVS | 3.3V |
| Motor encoder | PB6 (A), PB7 (B) | 3.3V bidirectional TVS | 3.3V |

TVS diodes are not required on SPI interfaces when the sensor is co-located with the MCU on the same protoboard (short trace, no external cable).

---

### HWR-033: Overvoltage Protection on ADC Inputs

- **Traces up**: HSR-RZC-007, SYS-049
- **Verification method**: Inspection (Zener diode placement on schematic)
- **Status**: draft

Overvoltage protection shall be provided on all ADC inputs that connect to external circuits:

| ADC Input | Pin | Protection |
|-----------|-----|------------|
| Battery voltage divider output | PA3 (RZC) | 3.3V Zener diode (BZX84C3V3) to GND |
| ACS723 current sensor output | PA0 (RZC) | 3.3V Zener diode (BZX84C3V3) to GND |
| NTC divider outputs | PA1, PA2 (RZC) | Not required (bounded by resistor divider, max output = 3.3V) |

The Zener diode clamp voltage (3.3V nominal, 3.5V maximum at 5 mA) is within the STM32G474RE absolute maximum ADC input rating (VDDA + 0.3V = 3.6V).

**Rationale**: The battery voltage divider can produce voltages above 3.3V if the battery voltage exceeds the designed range (e.g., load dump on the bench supply). The Zener clamp prevents ADC input damage.

---

## 11. Requirements Traceability Summary

### 11.1 HWR to HSR/TSR/SYS Mapping

| HWR | Traces Up To |
|-----|-------------|
| HWR-001 | SYS-031, TSR-032 |
| HWR-002 | SYS-053 |
| HWR-003 | SYS-031 |
| HWR-004 | SYS-031 |
| HWR-005 | SYS-031 |
| HWR-006 | SYS-031, HSR-SC-006 |
| HWR-007 | HSR-SC-001, HSR-RZC-004, TSR-029 |
| HWR-008 | HSR-FZC-003, SYS-048 |
| HWR-009 | HSR-CVC-001, HSR-FZC-001, HSR-RZC-001 |
| HWR-010 | SYS-001, SYS-029 |
| HWR-011 | SYS-010, SYS-014, SYS-018 |
| HWR-012 | SYS-004, SYS-005, SYS-006 |
| HWR-013 | SYS-031, HSR-CVC-004, HSR-FZC-005, HSR-RZC-005, HSR-SC-004 |
| HWR-014 | SYS-031 |
| HWR-015 | SYS-031 |
| HWR-016 | HSR-CVC-004, HSR-FZC-005, HSR-RZC-005, SYS-031 |
| HWR-017 | HSR-SC-004, SYS-031 |
| HWR-018 | HSR-CVC-004, SYS-031 |
| HWR-019 | HSR-CVC-001, HSR-FZC-001, TSR-001, TSR-010, SYS-047 |
| HWR-020 | HSR-FZC-003, TSR-018, SYS-048 |
| HWR-021 | HSR-RZC-001, TSR-006, SYS-049 |
| HWR-022 | HSR-RZC-002, TSR-008, SYS-049 |
| HWR-023 | HSR-RZC-007, TSR-009, SYS-049 |
| HWR-024 | HSR-FZC-002, HSR-FZC-006, TSR-012, TSR-015, SYS-050 |
| HWR-025 | HSR-RZC-004, TSR-005, SYS-050 |
| HWR-026 | HSR-RZC-006, TSR-040, SYS-009 |
| HWR-027 | HSR-CVC-002, HSR-FZC-004, HSR-RZC-003, HSR-SC-002, TSR-032 |
| HWR-028 | HSR-SC-001, TSR-029, SYS-024 |
| HWR-029 | HSR-CVC-003, TSR-033, SYS-028 |
| HWR-030 | HSR-SC-006, SYS-031 |
| HWR-031 | HSR-RZC-004, SYS-031 |
| HWR-032 | HSR-CVC-003, HSR-FZC-003, HWR-018 |
| HWR-033 | HSR-RZC-007, SYS-049 |

### 11.2 HWR Count by Category

| Category | HWR Range | Count |
|----------|-----------|-------|
| General | HWR-001 to HWR-005 | 5 |
| Power | HWR-006 to HWR-012 | 7 |
| CAN Bus | HWR-013 to HWR-018 | 6 |
| Sensor Interfaces | HWR-019 to HWR-023 | 5 |
| Actuator Interfaces | HWR-024 to HWR-026 | 3 |
| Safety Hardware | HWR-027 to HWR-029 | 3 |
| Protection | HWR-030 to HWR-033 | 4 |
| **Total** | | **33** |

---

## 12. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete HWR specification: 33 requirements across 7 categories, full traceability to HSR/TSR/SYS |

