---
document_id: HSR
title: "Hardware Safety Requirements"
version: "1.0"
status: draft
iso_26262_part: 5
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

## Lessons Learned Rule

Every requirement (HSR-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/safety/lessons-learned/`](../lessons-learned/). One file per requirement (HSR-NNN). File naming: `HSR-NNN-<short-title>.md`.


# Hardware Safety Requirements

## 1. Purpose

This document specifies the hardware safety requirements (HSR) for the Taktflow Zonal Vehicle Platform, derived from the technical safety requirements (TSR) per ISO 26262-5 Clause 6. Hardware safety requirements define what the hardware on each ECU shall provide to support the implementation of the safety functions. Each HSR is allocated to a specific ECU and specifies physical characteristics, circuit requirements, diagnostic coverage, and failure mode behavior.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| TSR | Technical Safety Requirements | 1.0 |
| FSR | Functional Safety Requirements | 1.0 |
| SG | Safety Goals | 1.0 |
| FSC | Functional Safety Concept | 1.0 |
| HSI | Hardware-Software Interface Specification | 0.1 |
| FMEA | Failure Mode and Effects Analysis | 0.1 |
| HW-METRICS | Hardware Architectural Metrics | 0.1 |

## 3. Requirement Conventions

### 3.1 Requirement Structure

Each requirement follows the format:

- **ID**: HSR-{ECU}-NNN (e.g., HSR-CVC-001)
- **Title**: Descriptive name
- **ASIL**: Inherited from parent TSR
- **Traces up**: TSR-xxx
- **Diagnostic coverage**: Percentage and method of fault detection
- **Status**: draft | reviewed | approved | verified

### 3.2 Naming Convention

HSR IDs are prefixed by the allocated ECU:
- `HSR-CVC-NNN` — Central Vehicle Computer
- `HSR-FZC-NNN` — Front Zone Controller
- `HSR-RZC-NNN` — Rear Zone Controller
- `HSR-SC-NNN` — Safety Controller

### 3.3 ASIL Inheritance

Each HSR inherits the ASIL of its parent TSR. Where an HSR traces to multiple TSRs, it inherits the highest ASIL.

### 3.4 Diagnostic Coverage Targets

Per ISO 26262-5 Table 5 (hardware architectural metrics):

| ASIL | SPFM Target | LFM Target |
|------|-------------|------------|
| D | >= 99% | >= 90% |
| C | >= 97% | >= 80% |
| B | >= 90% | >= 60% |
| A | >= 60% | >= 60% |

---

## 4. CVC — Central Vehicle Computer Hardware Safety Requirements

### HSR-CVC-001: Dual AS5048A Pedal Sensor SPI Bus

- **ASIL**: D
- **Traces up**: TSR-001, TSR-002
- **Diagnostic coverage**: 99% — dual redundant sensors with independent chip-selects; SPI CRC per transaction; mutual plausibility comparison
- **Status**: draft

The CVC hardware shall provide two AS5048A 14-bit magnetic angle sensors on the SPI1 bus for pedal position sensing. The hardware shall meet the following requirements:

1. **SPI bus**: SPI1 (PA5 = SCK, PA6 = MISO, PA7 = MOSI) with 1 MHz clock capability.
2. **Chip-selects**: Two independent GPIO chip-select lines — PA4 for sensor 1 and PA15 for sensor 2 — with 10k ohm pull-up resistors to 3.3V to ensure deselection on MCU reset.
3. **Sensor mounting**: Both sensors shall be mounted on the same pedal shaft with a known angular offset (calibratable). The magnetic target (diametric magnet) shall be common to both sensors.
4. **Power supply**: Each sensor shall be powered from the 3.3V rail with a local 100 nF decoupling capacitor within 10 mm of the sensor VDD pin.
5. **Separation**: The SPI signal traces for each sensor shall be routed on separate PCB layers or with at least 2 mm clearance to reduce coupling between sensor 1 and sensor 2 data paths.
6. **Failure modes addressed**: Sensor drift (plausibility check), stuck sensor (individual diagnostics), SPI bus failure (CRC + retry), common-cause magnet failure (range check on both sensors).

**Rationale**: Dual redundant sensors with independent chip-selects provide 99% SPFM for pedal position sensing at ASIL D. The pull-up resistors ensure sensors are deselected during MCU startup/reset, preventing spurious SPI traffic.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-CVC-001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies dual AS5048A sensors on SPI1 with independent chip-selects, pull-ups, decoupling, and trace separation. ASIL D is correct per TSR-001/TSR-002. The 99% SPFM claim is reasonable given dual redundancy with mutual plausibility. The 2 mm trace separation between sensor data paths and the separate PCB layers reduce common-cause coupling. The shared diametric magnet is a potential single point of failure for both sensors -- the range check on both sensors addresses this partially, but this should be explicitly listed in the FMEA as a common-cause item. Traces to TSR-001 and TSR-002 are consistent. No critical gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-CVC-001 -->

---

### HSR-CVC-002: CVC External Watchdog (TPS3823)

- **ASIL**: D
- **Traces up**: TSR-032
- **Diagnostic coverage**: 85% — detects MCU hang, clock failure, firmware infinite loop; does not detect incorrect computation with correct timing
- **Status**: draft

The CVC hardware shall include a TPS3823DBVR external watchdog IC with the following configuration:

1. **Timeout**: External capacitor on CT pin to set timeout period of 1.6 seconds (C = 100 nF per TPS3823 datasheet, tolerance +/- 20%).
2. **WDI connection**: WDI (watchdog input) pin connected to a dedicated MCU GPIO (configured by software).
3. **RESET connection**: RESET output (active-low, push-pull) connected to the STM32G474RE NRST pin with a 100 nF debounce capacitor.
4. **Power supply**: VDD connected to the 3.3V rail with a 100 nF decoupling capacitor.
5. **Power-on reset**: The TPS3823 shall provide a minimum 200 ms power-on reset pulse (per datasheet) to ensure the MCU starts cleanly.
6. **Independence**: The TPS3823 internal oscillator is independent of the MCU clock, providing timing diversity.

**Rationale**: The TPS3823 is a separate IC with its own oscillator, providing true clock-domain independence from the STM32. The push-pull RESET output drives the MCU NRST pin directly (1:1 connection per ECU; wire-OR not needed). Note: if wire-OR with multiple reset sources is ever required, use the TPS3824 (open-drain variant) instead.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-CVC-002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies TPS3823 external watchdog with 1.6s timeout, independent oscillator, and open-drain reset. ASIL D is correct per TSR-032. The 85% DC is appropriate -- the TPS3823 detects hang/clock faults but not incorrect-but-timely computation. The 200 ms POR pulse meets STM32G474 minimum reset pulse requirement. HSR-A-002 correctly flags the +/-20% timeout tolerance -- worst case 1.92s should be validated against system timing requirements. The clock independence from the MCU eliminates a common-cause failure mode. Traces to TSR-032 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-CVC-002 -->

> **Correction (2026-02-27):** The HITL comment above references "open-drain reset" — this was corrected to **push-pull** in the requirement text. The TPS3823 has a push-pull RESET output; the TPS3824 is the open-drain variant. No functional impact (1:1 connection per ECU, wire-OR not needed).

---

### HSR-CVC-003: CVC E-Stop Input Circuit

- **ASIL**: B
- **Traces up**: TSR-033
- **Diagnostic coverage**: 99% — hardware debounce + software confirmation; fail-safe wiring (normally-closed button to ground with pull-up)
- **Status**: draft

The CVC hardware shall provide an E-stop button input circuit on GPIO PC13 with the following characteristics:

1. **Button type**: Normally-closed (NC) momentary push button. In the resting state, the button holds PC13 LOW via connection to ground. Pressing the button opens the circuit, and the pull-up drives PC13 HIGH. This is fail-safe: a broken wire or disconnected button appears as E-stop activated (HIGH).
2. **Note on polarity**: The interrupt shall be configured for the transition that represents button PRESS (NC button opening = rising edge on PC13). The software ISR shall handle the correct polarity.
3. **Pull-up resistor**: 10k ohm internal pull-up enabled on PC13.
4. **Hardware debounce**: RC filter on PC13 — R = 10k ohm series, C = 100 nF to ground. Time constant = 1 ms, sufficient to filter switch bounce (typical: 1-10 ms).
5. **ESD protection**: TVS diode (3.3V bidirectional) on PC13 to protect against electrostatic discharge from the external button wiring.
6. **Wiring**: The E-stop button shall be connected via a shielded cable if the button is more than 30 cm from the CVC PCB.

**Rationale**: Normally-closed fail-safe wiring ensures any wire break or disconnection results in E-stop activation rather than E-stop unavailability. The RC debounce removes contact bounce before it reaches the GPIO, reducing software debounce load.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-CVC-003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies NC fail-safe E-stop wiring with hardware RC debounce, pull-up, ESD protection, and shielded cable. ASIL B is correct per TSR-033. The 99% DC claim is justified by the NC fail-safe wiring (wire break = E-stop activated) plus hardware debounce plus software confirmation. The TVS diode on PC13 is important given that the E-stop button may be externally mounted with long wiring exposed to ESD. The 1 ms RC time constant is appropriate for switch debounce. The note on polarity (rising edge for NC button press) is important for implementation clarity. Traces to TSR-033 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-CVC-003 -->

---

### HSR-CVC-004: CVC CAN Transceiver

- **ASIL**: D
- **Traces up**: TSR-022, TSR-038
- **Diagnostic coverage**: 90% — bus-off detection via CAN controller error counters; no hardware-level CAN message corruption detection (covered by E2E in software)
- **Status**: draft

The CVC hardware shall include a CAN transceiver (TJA1051T/3 or equivalent ISO 11898-2 compliant) connecting the STM32G474RE FDCAN1 peripheral to the CAN bus:

1. **TXD/RXD**: Connected to FDCAN1 TX (PB9) and RX (PB8).
2. **Termination**: 120 ohm termination resistor at each end of the CAN bus (CVC at one end, RZC or SC at the other end).
3. **Common-mode filter**: A common-mode choke (minimum 100 uH) on CANH/CANL lines for EMI suppression.
4. **ESD protection**: TVS diodes on CANH and CANL (rated for ISO 7637 transient).
5. **Standby pin**: The transceiver STB pin shall be connected to a GPIO to enable software-controlled sleep mode. Default state: active (STB = LOW).
6. **Bus fault tolerance**: The transceiver shall tolerate permanent short of CANH or CANL to ground or battery voltage without damage (per ISO 11898-2).

**Rationale**: A compliant CAN transceiver provides the physical layer for all safety-critical CAN communication. Common-mode filtering and ESD protection ensure reliable operation in the automotive EMI environment.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-CVC-004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies ISO 11898-2 compliant CAN transceiver with termination, common-mode filtering, ESD protection, and bus fault tolerance. ASIL D is correct per TSR-022/TSR-038. The 90% DC is appropriate -- error counters detect bus-level faults, but message-level corruption is handled by E2E in software. The standby pin (STB) control enables power management. The bus fault tolerance requirement (CANH/CANL short to GND or VBAT) is important for automotive robustness. HSR-A-004 correctly flags the need for EMC testing. Traces to TSR-022 and TSR-038 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-CVC-004 -->

---

### HSR-CVC-005: CVC OLED Display Interface

- **ASIL**: B
- **Traces up**: TSR-043
- **Diagnostic coverage**: 60% — I2C acknowledge check detects disconnection; no detection of pixel-level display faults
- **Status**: draft

The CVC hardware shall include an SSD1306 128x64 OLED display connected via I2C1 (PB6 = SCL, PB7 = SDA):

1. **I2C pull-ups**: 4.7k ohm pull-up resistors on SCL and SDA to 3.3V.
2. **I2C address**: SSD1306 default address 0x3C (7-bit).
3. **Power supply**: Display powered from the 3.3V rail with a 10 uF bulk capacitor for display inrush.
4. **Reset pin**: SSD1306 RST connected to a GPIO for hardware reset capability.

**Rationale**: The OLED provides the primary visual warning channel for the operator. I2C acknowledge checking provides basic disconnection detection but not display content verification (which would require a camera-based readback not feasible on this platform).

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-CVC-005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies SSD1306 OLED display on I2C1 with pull-ups, bulk capacitor, and hardware reset. ASIL B is correct per TSR-043. The 60% DC is honest -- I2C ACK only detects disconnection, not display content faults. The rationale correctly acknowledges the camera-based readback limitation. The hardware reset pin enables recovery from SSD1306 lockup without MCU reset. The 10 uF bulk capacitor addresses OLED inrush current. This is a secondary warning channel alongside the SC fault LEDs (HSR-SC-003). Traces to TSR-043 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-CVC-005 -->

---

## 5. FZC — Front Zone Controller Hardware Safety Requirements

### HSR-FZC-001: FZC Steering Angle Sensor SPI Bus

- **ASIL**: D
- **Traces up**: TSR-010
- **Diagnostic coverage**: 95% — SPI CRC per transaction, range check, rate-of-change check; single sensor (no dual redundancy, compensated by command-vs-feedback comparison)
- **Status**: draft

The FZC hardware shall provide one AS5048A 14-bit magnetic angle sensor on SPI1 for steering angle feedback:

1. **SPI bus**: SPI1 (PA5 = SCK, PA6 = MISO, PA7 = MOSI) with 1 MHz clock.
2. **Chip-select**: PA4 as GPIO chip-select with 10k ohm pull-up to 3.3V.
3. **Sensor mounting**: Sensor mounted on the steering servo output shaft. The diametric magnet shall be mechanically secured to prevent detachment under vibration.
4. **Power supply**: 3.3V with 100 nF decoupling capacitor within 10 mm of sensor VDD.
5. **Cable routing**: SPI signals routed away from servo power lines (minimum 10 mm separation) to reduce EMI coupling from the servo motor.

**Rationale**: A single steering angle sensor relies on command-vs-feedback comparison (software diagnostic) to detect servo faults. The 95% diagnostic coverage accounts for the residual risk of simultaneous servo and sensor fault (addressed by SC cross-plausibility as a system-level backup).

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-FZC-001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies single AS5048A on SPI1 for steering angle with SPI CRC, pull-up, and EMI routing separation from servo power. ASIL D is correct per TSR-010. The 95% DC is appropriate for a single sensor with SPI CRC and rate-of-change check, but below the 99% SPFM target for ASIL D. The rationale correctly acknowledges this gap and delegates it to command-vs-feedback comparison plus SC cross-plausibility. The 10 mm separation from servo power lines is important for EMI resilience. Consider whether a second sensor would be more appropriate for ASIL D -- the current single-sensor design relies heavily on software diagnostics. Traces to TSR-010 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-FZC-001 -->

---

### HSR-FZC-002: FZC Brake Servo PWM Circuit

- **ASIL**: D
- **Traces up**: TSR-015
- **Diagnostic coverage**: 85% — PWM timer feedback provides output verification; no servo-level force feedback sensor
- **Status**: draft

The FZC hardware shall provide a PWM output for the brake servo actuator:

1. **PWM output**: TIM2 channel 1 output on PA0, configured for 50 Hz servo PWM.
2. **Timer capture input**: TIM2 channel 2 input on PA1 (or equivalent), connected to the TIM2 CH1 output via a resistor divider (for feedback measurement of actual PWM output).
3. **Servo power**: Servo powered from the 6V regulated rail (separate from MCU 3.3V). A dedicated servo power regulator shall source at least 2A continuous.
4. **Protection**: A 3A fast-blow fuse on the servo power line to protect against servo short circuit. A flyback diode across the servo power lines for inductive load protection.
5. **Signal level**: PWM output at 3.3V logic level (compatible with standard servo signal input).

**Rationale**: Timer capture feedback enables software verification that the PWM peripheral is producing the commanded output. Separate power rails for servo and MCU prevent servo faults from affecting MCU operation.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-FZC-002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies brake servo PWM on TIM2 with timer capture feedback, separate 6V power rail (2A), fuse, and flyback protection. ASIL D is correct per TSR-015. The 85% DC is appropriate -- timer capture verifies PWM output but not servo mechanical response. The separate 6V power rail prevents servo faults from affecting MCU operation (spatial FFI for power). The 3A fuse provides overcurrent protection for servo short circuit. The resistor divider for capture feedback is a practical solution for PWM output verification. Traces to TSR-015 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-FZC-002 -->

---

### HSR-FZC-003: FZC Lidar UART Interface

- **ASIL**: C
- **Traces up**: TSR-018
- **Diagnostic coverage**: 80% — UART frame checksum detects data corruption; timeout detects disconnection; no physical-layer signal quality measurement
- **Status**: draft

The FZC hardware shall provide a UART interface for the TFMini-S lidar sensor:

1. **UART**: UART2 (PA2 = TX, PA3 = RX) configured for 115200 baud, 8N1.
2. **Voltage levels**: TFMini-S operates at 3.3V logic levels (LVTTL compatible with STM32). No level shifter required.
3. **Power supply**: TFMini-S requires 5V power (100 mA typical). A dedicated 5V rail from the input supply shall be provided with a 470 uF bulk capacitor for sensor startup inrush and a 100 nF decoupling capacitor.
4. **Connector**: A keyed 4-pin connector (VCC, GND, TX, RX) to prevent reverse polarity connection.
5. **ESD protection**: TVS diodes on RX and TX lines.
6. **Cable length**: Maximum cable length 1 meter; for longer runs, a shielded cable shall be used.

**Rationale**: The TFMini-S native 3.3V logic eliminates the need for level shifting, simplifying the interface. The 5V power rail must be clean and adequately decoupled to prevent lidar measurement errors from power supply noise.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-FZC-003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies TFMini-S UART interface at 115200 baud with 5V power, bulk capacitor, keyed connector, and ESD protection. ASIL C is correct per TSR-018. The 80% DC is appropriate -- UART checksum detects data corruption and timeout detects disconnection, but there is no physical-layer signal quality measurement. The keyed 4-pin connector prevents reverse polarity connection, which is a good practice. The 470 uF bulk capacitor for startup inrush is important given the TFMini-S current draw. The 1-meter cable length limit is appropriate for 115200 baud without shielding. Traces to TSR-018 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-FZC-003 -->

---

### HSR-FZC-004: FZC External Watchdog (TPS3823)

- **ASIL**: D
- **Traces up**: TSR-032
- **Diagnostic coverage**: 85% — same as HSR-CVC-002
- **Status**: draft

The FZC hardware shall include a TPS3823DBVR external watchdog IC with the same configuration as HSR-CVC-002: CT capacitor for 1.6 second timeout, WDI connected to dedicated MCU GPIO, RESET connected to STM32G474RE NRST with 100 nF debounce capacitor, VDD on 3.3V rail with 100 nF decoupling.

**Rationale**: Each ECU requires an independent external watchdog for ASIL D compliance. The identical configuration across ECUs simplifies BOM and schematic review.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-FZC-004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies TPS3823 external watchdog matching HSR-CVC-002. ASIL D is correct per TSR-032. The identical configuration across all ECUs is a good design decision for BOM simplification and consistent behavior. The 85% DC is consistent with the CVC watchdog. No unique FZC-specific concerns beyond the CVC review. Traces to TSR-032 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-FZC-004 -->

---

### HSR-FZC-005: FZC CAN Transceiver

- **ASIL**: D
- **Traces up**: TSR-022, TSR-038
- **Diagnostic coverage**: 90% — same as HSR-CVC-004
- **Status**: draft

The FZC hardware shall include a CAN transceiver identical in specification to HSR-CVC-004, connecting FDCAN1 to the CAN bus. The FZC shall not be a bus termination node (no 120 ohm termination resistor on FZC) unless FZC is physically at one end of the bus topology.

**Rationale**: Identical CAN transceiver specification across all ECUs ensures bus compatibility and simplifies system-level EMC testing.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-FZC-005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies CAN transceiver matching HSR-CVC-004, with a note that FZC should not have termination unless at bus end. ASIL D is correct per TSR-022/TSR-038. The 90% DC is consistent with the CVC CAN transceiver. The termination note is important for correct bus topology -- the bus should only have two termination resistors at the physical endpoints. Traces to TSR-022 and TSR-038 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-FZC-005 -->

---

### HSR-FZC-006: FZC Steering Servo PWM Circuit

- **ASIL**: D
- **Traces up**: TSR-012, TSR-013
- **Diagnostic coverage**: 80% — PWM timer output control provides disable capability; no servo position feedback at hardware level (software reads AS5048A)
- **Status**: draft

The FZC hardware shall provide a PWM output for the steering servo actuator:

1. **PWM output**: TIM1 channel 1 output on PA8, configured for 50 Hz servo PWM.
2. **Servo power**: Servo powered from a dedicated 6V regulated rail (separate from MCU 3.3V), capable of sourcing at least 2A continuous.
3. **Protection**: 3A fast-blow fuse on servo power. Flyback diode for inductive protection.
4. **PWM disable**: The TIM1 output enable (CCER register) and the GPIO alternate function can be independently controlled by software to provide three-level PWM disable (duty = 0, timer output disable, GPIO force LOW).
5. **Signal level**: 3.3V logic level PWM compatible with standard servo input.

**Rationale**: Three-level PWM disable (duty, timer, GPIO) provides defense-in-depth for the most critical actuator safety function (steering). Separate servo power rail prevents servo faults from corrupting MCU power.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-FZC-006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies steering servo PWM on TIM1 with three-level disable (duty, timer output, GPIO force), separate 6V power, fuse, and flyback. ASIL D is correct per TSR-012/TSR-013. The 80% DC is lower than ideal for ASIL D -- it reflects the lack of servo position feedback at the hardware level (software reads AS5048A). The three-level PWM disable is excellent defense-in-depth: even if one disable mechanism fails, two others remain. The separate 6V power rail is critical for steering servo independence. Traces to TSR-012 and TSR-013 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-FZC-006 -->

---

### HSR-FZC-007: FZC Buzzer Circuit

- **ASIL**: B
- **Traces up**: TSR-044
- **Diagnostic coverage**: 60% — GPIO output drive detectable by software readback; no acoustic level feedback
- **Status**: draft

The FZC hardware shall provide a piezo buzzer driven by a GPIO pin:

1. **Buzzer type**: Active piezo buzzer (self-oscillating, driven by DC voltage), minimum 85 dB at 10 cm.
2. **Drive circuit**: GPIO drives an N-channel MOSFET gate (e.g., 2N7002) which switches the buzzer connected between 3.3V and MOSFET drain. A 10k ohm gate pulldown resistor ensures the buzzer is OFF when GPIO is floating (MCU reset).
3. **Flyback diode**: Schottky diode across the buzzer terminals for inductive kickback protection.

**Rationale**: Active buzzer simplifies the drive circuit (no PWM tone generation required). The MOSFET driver with pulldown ensures the buzzer defaults to OFF on MCU reset or power-up, preventing nuisance alarms during initialization.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-FZC-007 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies active piezo buzzer with MOSFET driver, gate pulldown (OFF on reset), and flyback protection. ASIL B is correct per TSR-044. The 60% DC is honest -- GPIO readback confirms output state but cannot verify acoustic output. The 10k ohm gate pulldown ensuring OFF during MCU reset prevents nuisance alarms. The active buzzer design avoids software PWM tone generation complexity. The 85 dB minimum at 10 cm is sufficient for operator warning in a lab/workshop environment. Traces to TSR-044 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-FZC-007 -->

---

## 6. RZC — Rear Zone Controller Hardware Safety Requirements

### HSR-RZC-001: RZC ACS723 Motor Current Sensing Circuit

- **ASIL**: A
- **Traces up**: TSR-006
- **Diagnostic coverage**: 95% — ACS723 provides galvanically isolated current measurement; BTS7960 built-in overcurrent provides diverse hardware backup
- **Status**: draft

The RZC hardware shall include an ACS723LLCTR-20AB-T Hall-effect current sensor in the motor supply path:

1. **Placement**: In series with the motor supply line between the BTS7960 output and the motor. The sensor shall measure bidirectional current (positive = forward, negative = reverse).
2. **Output**: Analog voltage output proportional to current (sensitivity: 100 mV/A for ACS723LLCTR-20AB-T 20A variant, zero-current output: VCC/2 = 1.65V), connected to ADC1 channel 1 (PA0).
3. **Decoupling**: 100 nF capacitor on VCC pin and 1 nF capacitor on the output pin (for noise filtering without excessive bandwidth reduction, maintaining 80 kHz bandwidth).
4. **ADC reference**: ADC VREF+ connected to a stable 3.3V reference (internal or external, accuracy +/- 1%).
5. **PCB layout**: The ACS723 current path shall be on a wide trace (minimum 2 mm for 25A capacity) and the signal output trace shall be routed on a separate layer from the power trace.
6. **Diverse backup**: The BTS7960 built-in current sense (IS pins) provides a secondary current measurement path with lower accuracy but independent from the ACS723.

**Rationale**: Galvanic isolation between the high-current motor path and the MCU ADC input protects the MCU from motor faults. The ACS723 + BTS7960 combination provides diverse hardware for current monitoring.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-RZC-001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies ACS723 Hall-effect current sensor in the motor supply path with ADC connection, decoupling, PCB layout constraints, and BTS7960 IS pins as diverse backup. ASIL A is correct per TSR-006. The 95% DC is well-justified by galvanic isolation plus diverse BTS7960 backup. The PCB layout requirement (2 mm trace width for 25A, signal on separate layer from power) is important for noise immunity. HSR-A-003 correctly flags ACS723 temperature drift vs. the 20% cross-plausibility threshold. HSR-O-002 addresses the need to measure the actual sensitivity vs. temperature curve. The 1 nF output capacitor maintains 80 kHz bandwidth which is more than adequate for 1 kHz sampling. Traces to TSR-006 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-RZC-001 -->

---

### HSR-RZC-002: RZC NTC Temperature Sensing Circuit

- **ASIL**: A
- **Traces up**: TSR-008
- **Diagnostic coverage**: 90% — NTC open-circuit reads as very cold (below -30 C), short-circuit reads as very hot (above 150 C); both detected by software range check
- **Status**: draft

The RZC hardware shall include an NTC thermistor circuit for motor temperature measurement:

1. **NTC type**: 10k ohm NTC at 25 degrees C, beta = 3950 (B25/85), physically mounted on or near the motor winding.
2. **Voltage divider**: NTC in series with a 10k ohm precision resistor (1% tolerance) between 3.3V and ground, with the midpoint connected to ADC1 channel 2 (PA1).
3. **Filtering**: 100 nF capacitor across the ADC input for noise filtering.
4. **Failure modes**: Open-circuit NTC: ADC reads near VCC (3.3V), equivalent to below -30 degrees C (software detects as sensor fault). Short-circuit NTC: ADC reads near GND, equivalent to above 150 degrees C (software detects as sensor fault).
5. **Thermal coupling**: The NTC shall be in direct thermal contact with the motor case or winding, secured with thermal adhesive or clip.

**Rationale**: The voltage divider topology with a fixed precision resistor provides a simple, reliable temperature measurement. Both open and short circuit failure modes produce out-of-range readings detectable by software, achieving 90% diagnostic coverage.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-RZC-002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies NTC thermistor circuit with voltage divider (10k/10k), filtering, and open/short circuit failure mode analysis. ASIL A is correct per TSR-008. The 90% DC is appropriate -- both open-circuit (reads very cold) and short-circuit (reads very hot) failure modes are detectable by software range check (SSR-RZC-006). The 1% tolerance precision resistor ensures accurate temperature conversion. HSR-O-003 (NTC beta calibration) is correctly referenced as an open item. The thermal coupling requirement (adhesive/clip) is important for measurement accuracy. Traces to TSR-008 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-RZC-002 -->

---

### HSR-RZC-003: RZC External Watchdog (TPS3823)

- **ASIL**: D
- **Traces up**: TSR-032
- **Diagnostic coverage**: 85% — same as HSR-CVC-002
- **Status**: draft

The RZC hardware shall include a TPS3823DBVR external watchdog IC with the same configuration as HSR-CVC-002: CT capacitor for 1.6 second timeout, WDI connected to dedicated MCU GPIO, RESET connected to STM32G474RE NRST with 100 nF debounce capacitor, VDD on 3.3V rail with 100 nF decoupling.

**Rationale**: Independent external watchdog per ECU for ASIL D compliance.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-RZC-003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies TPS3823 external watchdog matching HSR-CVC-002. ASIL D is correct per TSR-032. Identical configuration across all ECUs for BOM simplification and consistent behavior. The 85% DC is consistent with the CVC watchdog. For the RZC specifically, the watchdog timeout (1.6s) is significantly longer than the motor overcurrent detection requirement (10 ms debounce from SSR-RZC-004) -- the watchdog is a last-resort safety mechanism, not the primary motor protection. Traces to TSR-032 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-RZC-003 -->

---

### HSR-RZC-004: RZC BTS7960 Motor Driver Interface

- **ASIL**: C
- **Traces up**: TSR-005, TSR-040
- **Diagnostic coverage**: 88% — enable line disable provides guaranteed motor cutoff; PWM disable provides secondary path; BTS7960 built-in overcurrent/thermal shutdown provides tertiary protection; residual: driver stuck-on fault (reliance on kill relay)
- **Status**: draft

The RZC hardware shall interface with the BTS7960 H-bridge motor driver:

1. **PWM inputs**: RPWM on TIM1 channel 1 (PA8), LPWM on TIM1 channel 2 (PA9). Timer configured for complementary output with hardware dead-time insertion (minimum 10 us dead-time).
2. **Enable lines**: R_EN on GPIO PB0, L_EN on GPIO PB1. Both lines shall have 10k ohm pull-down resistors to ground, ensuring the motor driver is disabled on MCU reset or GPIO floating state.
3. **Diagnostic outputs**: BTS7960 IS_R and IS_L (current sense) connected to ADC channels for diverse current measurement backup. R_IS on ADC1 channel 3 (PA3), L_IS on ADC1 channel 4 (PA4).
4. **Motor connections**: BTS7960 output connected to the DC motor via appropriately rated wiring (minimum 18 AWG for 25A continuous).
5. **Power supply**: BTS7960 VCC connected to the 12V bus via the kill relay. BTS7960 logic VCC connected to 3.3V or 5V as specified by the module.
6. **Heat dissipation**: BTS7960 module mounted with adequate airflow or heatsinking for 25A continuous operation at 40 degrees C ambient.

**Rationale**: Pull-down resistors on enable lines ensure the motor driver defaults to disabled on any MCU fault (reset, hang, GPIO floating). This is the most critical fail-safe hardware feature for SG-001 (prevent unintended acceleration).

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-RZC-004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies BTS7960 H-bridge interface with PWM dead-time, enable line pull-downs (fail-safe disabled), diagnostic IS outputs, and power path via kill relay. ASIL C is correct per TSR-005/TSR-040. The 88% DC is appropriate given the multiple diagnostic paths (enable line control, PWM disable, BTS7960 built-in overcurrent/thermal). The 10k ohm pull-down resistors on R_EN and L_EN are the most critical hardware safety feature -- they ensure the motor driver defaults to OFF on any MCU fault. The residual risk (driver stuck-on) is correctly identified and covered by the kill relay. The BTS7960 IS pins on separate ADC channels provide diverse current measurement independent of the ACS723. Traces to TSR-005 and TSR-040 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-RZC-004 -->

---

### HSR-RZC-005: RZC CAN Transceiver

- **ASIL**: D
- **Traces up**: TSR-022, TSR-038
- **Diagnostic coverage**: 90% — same as HSR-CVC-004
- **Status**: draft

The RZC hardware shall include a CAN transceiver identical in specification to HSR-CVC-004, connecting FDCAN1 to the CAN bus. If the RZC is at a physical end of the bus, it shall include a 120 ohm termination resistor.

**Rationale**: Identical CAN transceiver specification across all ECUs ensures bus compatibility.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-RZC-005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies CAN transceiver matching HSR-CVC-004, with conditional termination if RZC is at bus endpoint. ASIL D is correct per TSR-022/TSR-038. The 90% DC is consistent with other ECU CAN transceivers. The conditional termination note is important for correct bus topology. No unique RZC-specific CAN concerns. Traces to TSR-022 and TSR-038 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-RZC-005 -->

---

### HSR-RZC-006: RZC Motor Encoder Interface

- **ASIL**: C
- **Traces up**: TSR-040
- **Diagnostic coverage**: 75% — quadrature decoding detects direction; count rate validates speed; does not detect encoder failure (addressed by direction mismatch timeout in software)
- **Status**: draft

The RZC hardware shall provide a quadrature encoder interface for motor speed and direction sensing:

1. **Timer**: TIM3 configured in encoder mode (channels 1 and 2 for quadrature A and B inputs).
2. **Encoder connections**: TIM3 CH1 (PA6) and TIM3 CH2 (PA7) connected to the motor encoder outputs.
3. **Input filtering**: Hardware input filter enabled on TIM3 (ICF = 0x0F, maximum filter) to reject noise pulses shorter than the encoder minimum pulse width.
4. **Pull-up resistors**: 10k ohm pull-up resistors on both encoder inputs for open-collector encoder outputs.
5. **ESD protection**: TVS diodes on both encoder inputs.

**Rationale**: Hardware encoder mode in TIM3 provides zero-CPU-overhead direction and speed measurement. Input filtering prevents false counts from electrical noise near the motor.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-RZC-006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies quadrature encoder interface on TIM3 with hardware input filtering, pull-ups, and ESD protection. ASIL C is correct per TSR-040. The 75% DC is appropriate -- quadrature decoding detects direction and count rate validates speed, but encoder failure (disconnection, stuck) is detected only by the software direction mismatch timeout in SSR-RZC-015. The maximum hardware input filter (ICF=0x0F) rejects noise from the motor, which is critical given the physical proximity of encoder to motor. Pull-ups on both inputs handle open-collector encoder outputs. Traces to TSR-040 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-RZC-006 -->

---

### HSR-RZC-007: RZC Battery Voltage Monitoring

- **ASIL**: QM
- **Traces up**: TSR-009 (motor protection context)
- **Diagnostic coverage**: 70% — resistor divider provides voltage scaling; ADC accuracy limited by resistor tolerance
- **Status**: draft

The RZC hardware shall provide battery voltage measurement:

1. **Voltage divider**: 47k ohm / 10k ohm resistor divider from the 12V battery bus to ADC1 channel 5 (PA5), providing approximately 2.1V at 12V input (within 3.3V ADC range).
2. **Filtering**: 100 nF capacitor on the ADC input.
3. **Overvoltage protection**: Zener diode (3.3V) across the ADC input to clamp voltage spikes.

**Rationale**: Battery voltage monitoring enables detection of overvoltage (above 14.4V) and undervoltage (below 10V) conditions that may affect motor driver and servo operation.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-RZC-007 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies battery voltage monitoring with resistor divider (47k/10k), filtering, and zener overvoltage clamp. ASIL QM is correct -- battery voltage monitoring supports motor protection context but is not itself a safety function. The 70% DC is appropriate for a QM requirement. The 47k/10k divider provides approximately 2.1V at 12V input, well within the 3.3V ADC range. The zener clamp protects the ADC input from voltage transients. Traces to TSR-009 (motor protection context) are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-RZC-007 -->

---

## 7. SC — Safety Controller Hardware Safety Requirements

### HSR-SC-001: SC Kill Relay Circuit

- **ASIL**: D
- **Traces up**: TSR-029
- **Diagnostic coverage**: 99% — energize-to-run design provides fail-safe on any SC failure; GPIO readback verifies output state; residual risk: relay contacts welded closed (PMHF contribution documented in FMEA)
- **Status**: draft

The SC hardware shall provide a kill relay circuit controlled by GIO_A0 with the following characteristics:

1. **Relay type**: Automotive-grade relay rated for 12V coil, 30A contact (minimum), with gold-flashed contacts for low contact resistance. SPST-NO (single-pole, single-throw, normally-open) configuration.
2. **Energize-to-run**: The relay coil is energized when GIO_A0 = HIGH (via N-channel MOSFET). When the coil is energized, the relay contacts close, connecting the 12V power bus to the motor driver and servo circuits. When the coil is de-energized (GIO_A0 = LOW, SC power loss, or MOSFET failure), the relay contacts open, disconnecting actuator power. This is inherently fail-safe.
3. **MOSFET driver**: N-channel MOSFET (e.g., IRFZ44N, VDS >= 55V, RDS_on < 22 mohm) with gate connected to GIO_A0 via 100 ohm series resistor, drain connected to relay coil low side, source connected to ground. A 10k ohm pull-down resistor on the gate ensures the MOSFET is OFF (relay open) when GIO_A0 is floating or the SC is not powered.
4. **Flyback diode**: 1N4007 (or equivalent fast recovery diode) across the relay coil for inductive kickback protection. The diode shall limit the coil de-energization time to less than 5 ms.
5. **Relay dropout time**: The relay shall open within 10 ms of coil de-energization (verify with actual relay datasheet; typical automotive relay: 5-8 ms).
6. **Contact monitoring**: Optional — a relay auxiliary contact or a voltage divider on the load side of the relay can provide software readback of the relay state. If implemented, this provides detection of a welded-contact failure.
7. **Power path**: The relay shall be rated for the maximum system current (sum of motor driver + servo + auxiliary loads, estimated 30A peak).

**Rationale**: The energize-to-run pattern is the gold standard for safety-critical power relays. Any failure of the SC (power loss, firmware hang, GPIO fault, MOSFET failure) results in the relay opening (safe state). The 10k ohm gate pulldown ensures the MOSFET is OFF during power-up sequencing before the SC firmware initializes GIO_A0.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-SC-001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies the kill relay circuit with energize-to-run design, MOSFET driver with gate pulldown, flyback diode, contact monitoring, and 30A rating. ASIL D is correct per TSR-029. The 99% DC is well-justified by the energize-to-run design (any SC failure = relay opens) plus GPIO readback. The residual risk (relay contacts welded closed) is correctly identified and documented in HSR-A-001 with FMEA reference. The 10k ohm gate pulldown is critical for power-up safety. The optional contact monitoring (item 6) should be made mandatory given ASIL D -- HSR-O-006 tracks this. The 10 ms relay dropout time should be verified on actual hardware (HSR-O-001). Traces to TSR-029 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-SC-001 -->

---

### HSR-SC-002: SC External Watchdog (TPS3823)

- **ASIL**: D
- **Traces up**: TSR-032
- **Diagnostic coverage**: 85% — same function as HSR-CVC-002 but for TMS570LC43x platform
- **Status**: draft

The SC hardware shall include a TPS3823DBVR external watchdog IC with the following configuration:

1. **Timeout**: CT capacitor for 1.6 second timeout (100 nF, same as other ECUs).
2. **WDI connection**: Connected to a dedicated TMS570LC43x GIO pin.
3. **RESET connection**: RESET output connected to the TMS570LC43x nRST pin with 100 nF debounce capacitor.
4. **Power supply**: VDD on the 3.3V rail with 100 nF decoupling.
5. **Independence from lockstep**: The TPS3823 provides an independent reset mechanism for faults not detected by the TMS570 lockstep CPU (e.g., clock failure affecting both cores simultaneously, or an oscillator fault that causes both cores to fail identically).

**Rationale**: The external watchdog complements the TMS570 lockstep. Lockstep detects computation errors (cycle-by-cycle comparison). The TPS3823 detects hang conditions (no toggle within timeout). Together they cover complementary fault classes.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-SC-002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies TPS3823 external watchdog for the TMS570LC43x with the same timeout and configuration as other ECUs. ASIL D is correct per TSR-032. The 85% DC is consistent with other ECU watchdogs. The rationale correctly explains the complementary fault coverage: lockstep detects computation errors, TPS3823 detects hang/clock faults. Item 5 is important -- the TPS3823 catches faults that affect both lockstep cores simultaneously (e.g., common clock failure). The TMS570 nRST connection should be verified against TI reference design for compatibility with the lockstep reset behavior. Traces to TSR-032 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-SC-002 -->

---

### HSR-SC-003: SC Fault LED Circuit

- **ASIL**: B
- **Traces up**: TSR-045
- **Diagnostic coverage**: 60% — GPIO output drive verified by software readback; LED burnout not detected; CAN-independent warning channel
- **Status**: draft

The SC hardware shall provide 4 fault indicator LEDs with the following configuration:

1. **LED assignments**: GIO_A1 = CVC fault (red), GIO_A2 = FZC fault (red), GIO_A3 = RZC fault (red), GIO_A4 = system fault (amber).
2. **Drive circuit**: Each LED driven by a dedicated GIO pin through a current-limiting resistor (330 ohm for 10 mA at 3.3V). No MOSFET driver needed — TMS570 GIO pins can source 10 mA directly.
3. **LED type**: Standard 3 mm or 5 mm through-hole LEDs with minimum 100 mcd luminous intensity.
4. **Default state**: All LEDs OFF at power-up (GIO pins initialized to LOW).
5. **Independence**: The fault LEDs are driven directly by SC GPIO pins and do not depend on CAN communication, providing a CAN-independent warning channel.

**Rationale**: Direct GPIO-driven LEDs ensure the operator receives visual fault indication even during total CAN bus failure. This is the only warning channel that operates independently of the CAN bus.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-SC-003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies 4 fault LEDs on dedicated GIO pins with current-limiting resistors and default OFF state. ASIL B is correct per TSR-045. The 60% DC is honest -- GPIO readback confirms output state but LED burnout is undetectable without hardware current sensing on the LED path. The CAN-independent warning channel is architecturally important -- this is the only operator indication that works during total CAN bus failure. The direct GPIO drive (no MOSFET) is appropriate for 10 mA LED current within TMS570 GIO pin capabilities. Traces to TSR-045 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-SC-003 -->

---

### HSR-SC-004: SC DCAN1 Listen-Only Mode Configuration

- **ASIL**: D
- **Traces up**: TSR-022, TSR-038
- **Diagnostic coverage**: 95% — listen-only mode prevents bus corruption; CAN controller error counters detect bus faults; E2E in software detects message-level faults
- **Status**: draft

The SC hardware shall configure DCAN1 for listen-only (silent) mode:

1. **DCAN module**: Use DCAN1 (not DCAN4 due to HALCoGen v04.07.01 mailbox bug) connected via the edge connector pins.
2. **Listen-only mode**: DCAN1 TEST register bit 3 (Silent mode) shall be set. In this mode, the DCAN module receives CAN frames but does not transmit or acknowledge frames on the bus. This ensures the SC cannot corrupt the CAN bus.
3. **CAN transceiver**: A CAN transceiver (TJA1051T/3 or equivalent) connecting DCAN1 TX/RX to the CAN bus. The TX line from the TMS570 shall be connected to the transceiver TXD even in listen-only mode (the DCAN module drives TX recessive in silent mode).
4. **Bus connection**: The SC shall be connected to the same CAN bus as CVC, FZC, and RZC. If the SC is at a physical end of the bus, it shall include a 120 ohm termination resistor.
5. **Independence**: The SC CAN interface is electrically independent from the zone ECU CAN interfaces (no shared components other than the bus wires and termination).

**Rationale**: Listen-only mode is critical for SC independence: the SC monitors the bus without the ability to corrupt it. This is a key architectural feature for ASIL D independent monitoring. Using DCAN1 (not DCAN4) avoids the known HALCoGen mailbox bug.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-SC-004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies DCAN1 in listen-only (silent) mode with CAN transceiver, bus connection, and independence from zone ECU CAN interfaces. ASIL D is correct per TSR-022/TSR-038. The 95% DC is appropriate -- listen-only mode prevents bus corruption, error counters detect bus faults, and E2E covers message-level faults. The note about using DCAN1 (not DCAN4) due to the HALCoGen mailbox bug is critical project knowledge. The electrical independence (no shared components except bus wires/termination) is a key FFI argument. The TX line still connected to the transceiver TXD is correct -- DCAN drives TX recessive in silent mode, so the transceiver is needed for proper bus electrical behavior. Traces to TSR-022 and TSR-038 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-SC-004 -->

---

### HSR-SC-005: SC TMS570LC43x Lockstep CPU Configuration

- **ASIL**: D
- **Traces up**: TSR-050, TSR-051
- **Diagnostic coverage**: 99% — lockstep CPU provides cycle-by-cycle comparison of CPU outputs; ESM provides hardware error signaling; PBIST covers RAM; flash CRC covers program memory
- **Status**: draft

The SC hardware shall utilize the TMS570LC43x lockstep CPU safety features:

1. **Lockstep mode**: The TMS570LC43x operates in lockstep by default — two ARM Cortex-R5F cores execute the same instructions, and a hardware comparator checks outputs every clock cycle. This is a hardware feature; no software configuration is needed to enable lockstep.
2. **Error Signaling Module (ESM)**: The ESM shall be configured to generate an NMI (non-maskable interrupt) or reset on lockstep comparison error. The ESM error output pin (nERROR) shall be connected to an external fault indicator or left as a test point.
3. **PBIST**: The Programmable Built-In Self-Test engine shall be used during startup for RAM testing. PBIST covers March13N algorithm over all RAM banks.
4. **ECC**: ECC (Error Correcting Code) shall be enabled on flash and RAM memories. Single-bit errors shall be corrected automatically; double-bit errors shall trigger ESM.
5. **Power supply**: The TMS570LC43x requires 1.2V core and 3.3V I/O supply rails. Both shall include adequate decoupling (100 nF per power pin, plus 10 uF bulk) per TI reference design.
6. **Clock**: External oscillator (16 MHz typical) with PLL for system clock (up to 300 MHz). Crystal specification per TI recommendation.

**Rationale**: The TMS570LC43x lockstep architecture is specifically designed for ISO 26262 ASIL D safety applications. The cycle-by-cycle comparison provides the highest achievable hardware diagnostic coverage for computation errors (99% per ISO 26262-5 Table D.7). This is the primary justification for using the TMS570 as the Safety Controller.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-SC-005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies TMS570LC43x lockstep CPU with ESM, PBIST, ECC, and power/clock per TI reference design. ASIL D is correct per TSR-050/TSR-051. The 99% DC is the highest in the system and correctly references ISO 26262-5 Table D.7 for lockstep CPU coverage. The lockstep mode being enabled by default (hardware feature) eliminates the risk of misconfiguration. The ESM-to-NMI configuration ensures immediate response to lockstep comparison errors. PBIST with March13N algorithm provides comprehensive RAM testing at startup. ECC on flash and RAM provides runtime memory protection with single-bit correction and double-bit detection. Traces to TSR-050 and TSR-051 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-SC-005 -->

---

### HSR-SC-006: SC Power Supply Independence

- **ASIL**: D
- **Traces up**: TSR-029, TSR-030
- **Diagnostic coverage**: 90% — independent power regulator ensures SC operates when zone ECU power fails; brown-out detection on 3.3V rail
- **Status**: draft

The SC hardware shall have a power supply path that is independent of the kill relay:

1. **SC power path**: The SC 3.3V regulator shall be powered directly from the 12V battery bus, NOT through the kill relay. This ensures the SC remains powered when the kill relay is open (safe state), enabling DTC logging and fault LED indication after relay de-energization.
2. **Regulator**: A dedicated 3.3V linear regulator (e.g., LM1117-3.3) for the SC, separate from the zone ECU regulators.
3. **Brown-out detection**: The TMS570LC43x internal supply monitor shall be enabled to detect undervoltage on the 1.2V core rail and 3.3V I/O rail. Undervoltage shall trigger ESM and lockstep-like reset behavior.
4. **Reverse polarity protection**: A series Schottky diode or P-channel MOSFET on the 12V input to the SC regulator.
5. **Kill relay isolation**: The kill relay shall only control power to the motor driver and servo circuits, never to the SC. The SC shall be powered as long as the 12V battery is connected.

**Rationale**: SC power independence is essential: the SC must remain operational to log DTCs and indicate faults via LEDs after it has opened the kill relay. If the SC lost power when the relay opened, it could not maintain the safe state or provide post-fault diagnostics.

<!-- HITL-LOCK START:COMMENT-BLOCK-HSR-SC-006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies SC power supply independence from the kill relay, with dedicated 3.3V regulator, brown-out detection, reverse polarity protection, and kill relay isolation. ASIL D is correct per TSR-029/TSR-030. The 90% DC is appropriate -- independent power with brown-out detection provides high coverage, with the residual being complete battery disconnection (which is outside the system boundary). The key architectural insight is that the SC must remain powered after opening the kill relay -- otherwise it could not maintain fault indication or log DTCs. The reverse polarity protection (Schottky or MOSFET) prevents damage from incorrect battery connection during initial hardware assembly. Traces to TSR-029 and TSR-030 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSR-SC-006 -->

---

## 8. Requirements Traceability Summary

### 8.1 TSR to HSR Mapping

| TSR | HSRs Derived |
|-----|-------------|
| TSR-001 | HSR-CVC-001 |
| TSR-002 | HSR-CVC-001 |
| TSR-005 | HSR-RZC-004 |
| TSR-006 | HSR-RZC-001 |
| TSR-008 | HSR-RZC-002 |
| TSR-010 | HSR-FZC-001 |
| TSR-012 | HSR-FZC-006 |
| TSR-013 | HSR-FZC-006 |
| TSR-015 | HSR-FZC-002 |
| TSR-018 | HSR-FZC-003 |
| TSR-022 | HSR-CVC-004, HSR-FZC-005, HSR-RZC-005, HSR-SC-004 |
| TSR-029 | HSR-SC-001, HSR-SC-006 |
| TSR-030 | HSR-SC-006 |
| TSR-032 | HSR-CVC-002, HSR-FZC-004, HSR-RZC-003, HSR-SC-002 |
| TSR-033 | HSR-CVC-003 |
| TSR-038 | HSR-CVC-004, HSR-FZC-005, HSR-RZC-005, HSR-SC-004 |
| TSR-040 | HSR-RZC-004, HSR-RZC-006 |
| TSR-043 | HSR-CVC-005 |
| TSR-044 | HSR-FZC-007 |
| TSR-045 | HSR-SC-003 |
| TSR-050 | HSR-SC-005 |
| TSR-051 | HSR-SC-005 |

### 8.2 HSR Count per ECU

| ECU | HSR Count | ASIL D Count | ASIL C Count | ASIL B Count | ASIL A Count | QM Count |
|-----|-----------|-------------|-------------|-------------|-------------|----------|
| CVC | 5 | 3 | 0 | 1 | 0 | 0 |
| FZC | 7 | 4 | 1 | 1 | 0 | 0 |
| RZC | 7 | 3 | 2 | 0 | 2 | 1 |
| SC | 6 | 5 | 0 | 1 | 0 | 0 |
| **Total** | **25** | **15** | **3** | **3** | **2** | **1** |

Note: HSR-FZC-006 counts as 1 requirement despite tracing to two TSRs.

### 8.3 ASIL Distribution

| ASIL | Count | Percentage |
|------|-------|-----------|
| D | 15 | 60% |
| C | 3 | 12% |
| B | 3 | 12% |
| A | 2 | 8% |
| QM | 1 | 4% |
| **Total** | **25** | |

Note: One additional HSR (HSR-RZC-007) is QM as battery voltage monitoring supports motor protection but is not itself a safety function.

### 8.4 Diagnostic Coverage Summary

| HSR | ASIL | DC | Method |
|-----|------|-----|--------|
| HSR-CVC-001 | D | 99% | Dual redundant sensors, SPI CRC, plausibility |
| HSR-CVC-002 | D | 85% | External WDT with independent oscillator |
| HSR-CVC-003 | B | 99% | NC fail-safe wiring, HW debounce, SW confirm |
| HSR-CVC-004 | D | 90% | Error counters, bus-off detection |
| HSR-CVC-005 | B | 60% | I2C ACK check |
| HSR-FZC-001 | D | 95% | SPI CRC, range/rate check, command-vs-feedback |
| HSR-FZC-002 | D | 85% | Timer capture feedback |
| HSR-FZC-003 | C | 80% | UART checksum, timeout |
| HSR-FZC-004 | D | 85% | External WDT |
| HSR-FZC-005 | D | 90% | Error counters |
| HSR-FZC-006 | D | 80% | 3-level PWM disable |
| HSR-FZC-007 | B | 60% | GPIO readback |
| HSR-RZC-001 | A | 95% | Galvanic isolation, diverse backup (BTS7960 IS) |
| HSR-RZC-002 | A | 90% | Open/short circuit detection via range check |
| HSR-RZC-003 | D | 85% | External WDT |
| HSR-RZC-004 | C | 88% | Enable pulldowns, PWM dead-time, BTS7960 protection |
| HSR-RZC-005 | D | 90% | Error counters |
| HSR-RZC-006 | C | 75% | Quadrature decode, noise filter |
| HSR-RZC-007 | QM | 70% | Voltage divider with overvoltage clamp |
| HSR-SC-001 | D | 99% | Energize-to-run, GPIO readback |
| HSR-SC-002 | D | 85% | External WDT, independent oscillator |
| HSR-SC-003 | B | 60% | GPIO readback |
| HSR-SC-004 | D | 95% | Listen-only mode, error counters |
| HSR-SC-005 | D | 99% | Lockstep CPU, ESM, PBIST, ECC |
| HSR-SC-006 | D | 90% | Independent power regulator, brown-out detection |

## 9. Open Items and Assumptions

### 9.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| HSR-A-001 | The kill relay contacts do not weld closed under normal operating currents (< 30A) | Residual risk: relay stuck-closed addressed in FMEA with PMHF contribution |
| HSR-A-002 | The TPS3823 timeout tolerance (+/- 20%) is acceptable for the application | Worst-case timeout: 1.92 seconds (1.6s + 20%). This must be longer than the MCU maximum expected loop time. |
| HSR-A-003 | ACS723 sensitivity drift over temperature is within the 20% cross-plausibility threshold | If temperature drift exceeds 20%, false cross-plausibility faults could occur |
| HSR-A-004 | Standard PCB layout practices (ground plane, decoupling) are sufficient for EMI at 500 kbps CAN | No EMC testing has been performed at this design stage |

### 9.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| HSR-O-001 | Verify kill relay dropout time on actual hardware | HW Engineer | Hardware integration |
| HSR-O-002 | Measure ACS723 sensitivity vs. temperature curve | HW Engineer | Hardware integration |
| HSR-O-003 | Perform FMEDA analysis for all hardware elements per ISO 26262-5 | Safety Engineer | Before Phase 4 |
| HSR-O-004 | Calculate SPFM, LFM, PMHF for each ECU hardware design | Safety Engineer | Before Phase 4 |
| HSR-O-005 | Verify TPS3823 power-on reset pulse duration meets STM32 requirements | HW Engineer | Hardware integration |
| HSR-O-006 | Design and verify relay contact monitoring circuit (HSR-SC-001, item 6) | HW Engineer | Detailed design |
| HSR-O-007 | Perform EMC pre-compliance test on CAN bus at 500 kbps | Test Engineer | System integration |

## 10. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete HSR specification: 25 requirements across 4 ECUs (CVC: 5, FZC: 7, RZC: 7, SC: 6), diagnostic coverage analysis, full traceability |

