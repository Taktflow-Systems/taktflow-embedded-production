---
document_id: FMEA
title: "Failure Mode and Effects Analysis"
version: "1.0"
status: draft
iso_26262_part: 9
aspice_process: SYS.2
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


# Failure Mode and Effects Analysis

<!-- DECISION: ADR-003 — Zonal safety mechanism allocation -->

## 1. Purpose

This document presents the system-level Failure Mode and Effects Analysis (FMEA) for the Taktflow Zonal Vehicle Platform per ISO 26262-9:2018, Clause 8. The FMEA systematically identifies potential failure modes of each hardware component and subsystem, evaluates the local and system-level effects of each failure, assesses severity, documents the detection method and recommended mitigation, and provides a reference to the Risk Priority Number (RPN) for prioritization.

The FMEA is a bottom-up inductive analysis: starting from individual component failures, it traces the propagation of effects upward through the system hierarchy to the vehicle-level safety goals. This complements the top-down deductive methods (FTA) and the dependent failure analysis (DFA) documented separately.

## 2. Scope

### 2.1 Item Under Analysis

The item is the Taktflow Zonal Vehicle Platform as defined in the Item Definition (ITEM-DEF v1.0). The analysis covers:

- **CVC** (STM32G474RE): Central Vehicle Computer -- pedal input, vehicle state machine, CAN master
- **FZC** (STM32G474RE): Front Zone Controller -- steering servo, brake servo, lidar, buzzer
- **RZC** (STM32G474RE): Rear Zone Controller -- motor driver, current/temperature sensing, encoder
- **SC** (TMS570LC43x): Safety Controller -- independent CAN monitor, heartbeat, kill relay
- **CAN bus**: Shared communication infrastructure (500 kbps, 7+1 nodes)
- **Power supply**: 12V rail, 5V and 3.3V derived rails

### 2.2 Exclusions

The following elements are excluded from this FMEA:

| Element | Reason |
|---------|--------|
| BCM, ICU, TCU (Docker) | QM-rated simulated ECUs with no safety function allocation |
| Raspberry Pi gateway | Outside item safety boundary (telemetry only) |
| AWS cloud infrastructure | No vehicle control authority |
| Development PC | Not part of the delivered system |

## 3. References

| Document ID | Title | Version | Relevance |
|-------------|-------|---------|-----------|
| ITEM-DEF | Item Definition | 1.0 | System boundary, interfaces, functions |
| HARA | Hazard Analysis and Risk Assessment | 1.0 | Hazardous events, ASIL assignments |
| SG | Safety Goals | 1.0 | Safety goals, safe states, FTTI |
| FSC | Functional Safety Concept | 1.0 | Safety mechanisms, allocation |
| DFA | Dependent Failure Analysis | 1.0 | Common cause and cascading failures |
| ISO 26262-9:2018 | Safety Analyses | -- | FMEA methodology (Clause 8) |
| ISO 26262-5:2018 | HW Level Development | -- | HW failure rate data, diagnostic coverage |
| IEC 60812:2018 | FMEA Methodology | -- | General FMEA process guidance |

## 4. Methodology

### 4.1 Analysis Process

The FMEA follows the process prescribed in IEC 60812 and ISO 26262-9 Clause 8:

1. **System decomposition**: Decompose the item into functional blocks, components, and interfaces.
2. **Failure mode identification**: For each component, identify all credible failure modes using hardware failure mode databases (MIL-HDBK-338B), component datasheets, and engineering judgment.
3. **Effect analysis**: For each failure mode, determine the local effect (on the immediate subsystem) and the system-level effect (on vehicle behavior and safety goals).
4. **Severity assessment**: Assign a severity rating (1-10) based on the potential harm to the vehicle occupant and third parties, aligned with the HARA severity classifications.
5. **Detection assessment**: Document the safety mechanism or diagnostic function that detects the failure mode.
6. **Recommended action**: Specify the mitigation, safe state transition, or design improvement.
7. **RPN reference**: Provide a cross-reference to the quantitative Risk Priority Number calculation (to be completed during detailed hardware FMEA in Phase 5).

### 4.2 Severity Scale

The severity scale maps to the HARA severity classifications and safety goals:

| Severity | Description | HARA Alignment | Safety Goal Impact |
|----------|-------------|----------------|-------------------|
| 10 | Hazardous without warning -- potential fatality | S3, C3 | Violation of ASIL D safety goal |
| 9 | Hazardous with warning -- life-threatening, critical function lost | S3, C2 | Violation of ASIL C/D safety goal |
| 8 | Very high -- system inoperable, safety function seriously degraded | S3, C1 or S2, C3 | Potential violation of ASIL B/C safety goal |
| 7 | High -- system performance severely affected | S2, C2 | Degraded safety function, ASIL A/B impact |
| 6 | Moderate -- system operable but comfort/convenience impacted | S2, C1 | Degradation warning, no immediate hazard |
| 5 | Low -- reduced performance noticed by operator | S1, C2 | Operator-noticeable degradation |
| 4 | Very low -- minor effect noticed by discriminating operator | S1, C1 | Minor inconvenience |
| 3 | Minor -- slight effect, annoyance to operator | S0-S1 | No safety impact, cosmetic |
| 2 | Very minor -- item barely affected | S0 | Negligible effect |
| 1 | None -- no discernible effect | S0 | No effect |

### 4.3 Participants

| Role | Responsibility |
|------|----------------|
| FSE Lead | FMEA facilitation, severity/detection consistency, safety goal linkage |
| System Engineer | System-level effect analysis, interface failure propagation |
| HW Engineer | Component failure modes, failure rate data, detection mechanisms |
| SW Engineer | Software diagnostic coverage, safety mechanism implementation |
| Test Engineer | Detection method validation, test coverage assessment |

## 5. FMEA Tables

### 5.1 CVC Subsystem (Central Vehicle Computer -- STM32G474RE)

The CVC is the vehicle brain: it reads dual pedal sensors, manages the vehicle state machine, drives the OLED display, and serves as the CAN bus master for torque, steering, and brake commands.

| ID | Component | Failure Mode | Local Effect | System Effect | S | Detection Method | Recommended Action / Mitigation | RPN Ref |
|----|-----------|-------------|--------------|---------------|---|-----------------|--------------------------------|---------|
| FM-CVC-001 | Pedal sensor 1 (AS5048A) | Stuck at maximum output (full scale) | CVC reads one sensor at full throttle | If sensor 2 disagrees: plausibility fault detected (SM-001). If sensor 2 also fails high: unintended full throttle command sent to RZC. Unintended acceleration -- **violates SG-001 (ASIL D)** | 10 | SM-001: Dual sensor plausibility check (|S1-S2| > 5%, 2 consecutive cycles). SM-002: RZC overcurrent cutoff. SM-003: SC cross-plausibility (torque vs current). | Dual sensor plausibility within 20 ms. On disagreement: torque = 0, safe stop. Single sensor stuck high is detected; dual stuck high is a common cause failure addressed in DFA (CCF-002). | RPN-001 |
| FM-CVC-002 | Pedal sensor 1 (AS5048A) | Stuck at zero (minimum output) | CVC reads one sensor as zero throttle | If sensor 2 is healthy and reports non-zero: plausibility fault detected. If both stuck at zero: vehicle cannot accelerate (loss of throttle input). Loss of drive function -- **impacts SG-002 (ASIL B)** | 5 | SM-001: Dual sensor plausibility check. SM-006: Motor controller health monitoring (expected torque vs actual). | On plausibility fault: transition to DEGRADED. Both sensors stuck at zero is fail-safe (no unintended acceleration). Log DTC, inform operator via OLED. | RPN-002 |
| FM-CVC-003 | Pedal sensor 1 (AS5048A) | Reading drift / offset error | CVC sensor 1 reading deviates from true position by a slow-growing offset | If drift exceeds plausibility threshold: detected by SM-001. If drift is within threshold but causes incorrect torque mapping: incorrect torque request, proportional to drift magnitude. Incorrect torque -- **impacts SG-001 (ASIL D)** | 8 | SM-001: Plausibility check (5% threshold detects drift > 5%). SM-003: SC cross-plausibility (torque request vs actual current, 20% threshold). Range check: sensor reading within 0.5V-4.5V equivalent. | Tighten plausibility threshold for drift detection. Implement running-average comparison between sensors. Sensor recalibration at INIT. Residual risk: drift within 5% is undetected but causes proportionally small torque error. | RPN-003 |
| FM-CVC-004 | Pedal sensor 2 (AS5048A, redundant) | Stuck at any value | CVC reads disagreement between sensor 1 and sensor 2 | Plausibility check fails immediately (SM-001). System transitions to DEGRADED or SAFE_STOP depending on the magnitude of disagreement. Single-point failure detected -- no immediate hazard if sensor 1 is healthy. | 3 | SM-001: Dual sensor plausibility check. Detection within 20 ms (2 control cycles). | Log DTC for sensor 2. Transition to DEGRADED mode (single sensor operation with reduced confidence). Schedule maintenance. If sensor 1 subsequently fails, system goes to SAFE_STOP. | RPN-004 |
| FM-CVC-005 | Both pedal sensors (AS5048A x2) | Agree on wrong value (common cause) | Both sensors report the same incorrect angle due to shared root cause (SPI bus fault, shared power rail corruption, shared EMI event) | CVC plausibility check passes (both agree). Incorrect torque command sent to RZC. Unintended acceleration or incorrect speed -- **violates SG-001 (ASIL D)**. This is a common cause failure. | 10 | SM-003: SC cross-plausibility (torque request vs actual motor current). SM-002: RZC independent overcurrent cutoff (hardware limit). SM-005: Kill relay (ultimate backup). CAN E2E: detects SPI bus corruption that manifests as CAN message errors. | Primary mitigation: SC cross-plausibility provides independent detection (different vendor, different architecture). Secondary: RZC overcurrent cutoff provides hardware-level protection. DFA analysis: see CCF-002 (shared SPI bus) and CCF-004 (shared power supply). Residual risk: latency of SC detection (60 ms) exceeds SG-001 FTTI (50 ms) -- SM-001/SM-002 provide faster primary coverage. | RPN-005 |
| FM-CVC-006 | E-stop button | Stuck open (cannot close / activate) | E-stop button does not register when pressed by operator | Loss of emergency stop function. Operator's last-resort safety intervention unavailable. All other safety mechanisms must function independently. **Impacts SG-008 (ASIL C)** | 8 | SM-023: E-stop GPIO monitoring (CVC verifies E-stop circuit during INIT self-test: brief activation + readback). Periodic E-stop circuit health check during RUN mode (monitor GPIO pin state for stuck-at-high, check interrupt controller configuration). | E-stop self-test at startup (operator prompted to press and release). Redundant E-stop path: SC monitors CAN for E-stop message; if CVC fails to broadcast, SC heartbeat timeout provides backup. Use normally-closed (NC) contact wiring so wire break = safe state (E-stop activated). | RPN-006 |
| FM-CVC-007 | E-stop button | Stuck closed (permanently activated) | CVC detects continuous E-stop assertion | System enters SAFE_STOP immediately and cannot exit. Permanent shutdown -- vehicle inoperable but in safe state. Availability loss only. | 2 | SM-023: E-stop state monitoring. CVC detects stuck-closed during INIT (E-stop asserted before operator action). | Operator-visible: OLED displays "E-STOP STUCK" diagnostic. DTC logged. Physical inspection required. Fail-safe condition -- no hazard. | RPN-007 |
| FM-CVC-008 | OLED display (SSD1306) | Display failure (blank, garbled, or stuck) | Operator loses visual status and warning information | Loss of driver information. No direct safety impact (OLED is a QM-rated information function). Operator may not see fault warnings or vehicle state. | 2 | I2C ACK/NACK detection: CVC checks I2C transaction status. Display checksum: CVC periodically reads display RAM back and compares. | Redundant warning paths: FZC buzzer (audible), SC fault LEDs (visual), ICU dashboard (simulated). OLED failure is not safety-critical because multiple independent warning channels exist. DTC logged for OLED fault. | RPN-008 |
| FM-CVC-009 | CVC CAN transmitter (FDCAN + TJA1051T) | CAN TX failure (no messages sent) | CVC can no longer transmit torque commands, steering commands, brake commands, or heartbeat messages to the bus | FZC: no brake/steering commands received -- FZC auto-brakes on CAN timeout (SM-012). RZC: no torque commands -- RZC defaults to zero torque on CAN timeout. SC: CVC heartbeat missing -- SC opens kill relay after timeout. All actuators go to safe state. **Impacts SG-001, SG-003, SG-004 (ASIL D)** | 7 | SM-019: SC heartbeat monitoring (CVC heartbeat timeout = 150 ms). SM-012: FZC auto-brake on CAN timeout (100 ms). SM-007: RZC torque command timeout. | All receiving ECUs implement timeout-based safe defaults. SC provides independent system-level detection. CVC CAN TX failure results in a coordinated safe state transition. Residual risk: 100-200 ms latency before detection (exceeds 50 ms FTTI for SG-001). Primary mitigation: SM-001/SM-002 handle pedal/motor faults faster; CAN TX loss is a gradual loss-of-communication scenario. | RPN-009 |
| FM-CVC-010 | CVC CAN receiver (FDCAN + TJA1051T) | CAN RX failure (no messages received) | CVC cannot receive feedback from FZC (steering angle, brake status) or RZC (motor current, speed, temperature) | CVC operates blind: no feedback on actuator status. CVC cannot detect RZC/FZC faults through CAN. CVC detects RX timeout internally and transitions to DEGRADED. SC provides independent monitoring. **Impacts SG-002, SG-003, SG-004 (ASIL B-D)** | 6 | CVC internal CAN RX timeout counters (one per expected message). Alive counter staleness detection (part of E2E, SM-004). CVC self-diagnostic: if no messages received from any node for > 200 ms, CVC assumes bus failure and transitions to SAFE_STOP. | CVC transitions to SAFE_STOP on persistent RX silence. SC continues independent monitoring of all ECUs. Redundant detection: SC detects the same failure through heartbeat anomaly (CVC may send but not receive, so CVC heartbeat continues but CVC commands become stale). | RPN-010 |
| FM-CVC-011 | CVC MCU (STM32G474RE) | Software hang (infinite loop, deadlock, stack overflow) | CVC stops executing all functions: no sensor reading, no CAN transmission, no state machine updates | All downstream ECUs lose CAN commands from CVC. FZC auto-brakes (SM-012), RZC defaults to zero torque. SC detects CVC heartbeat timeout and opens kill relay. System enters SHUTDOWN. **Impacts all safety goals** | 9 | SM-020: External watchdog (TPS3823) forces hardware reset within 1.6 seconds. SM-019: SC heartbeat monitoring detects CVC silence within 150 ms. SM-012: FZC auto-brake on CAN timeout. | Multi-layer detection: (1) External watchdog resets CVC. (2) SC detects heartbeat loss and opens kill relay. (3) FZC/RZC implement independent timeout-based safe defaults. After CVC reset: full INIT self-test before returning to RUN. DTC logged for watchdog reset cause. | RPN-011 |
| FM-CVC-012 | CVC SPI bus (SPI1) | SPI bus failure (MISO stuck, clock lost, bus short) | Both pedal sensors become unreadable simultaneously | CVC cannot read either pedal sensor. This is equivalent to both sensors failing (common cause via shared bus). CVC detects SPI communication errors (CRC/parity error on AS5048A response, timeout). Torque request set to zero. **Impacts SG-001 (ASIL D), SG-002 (ASIL B)** | 9 | SPI transaction timeout detection (< 1 ms per transaction). AS5048A parity bit verification on every read. SM-001: Both sensors return invalid data -- plausibility check fails on invalid readings. CVC detects SPI bus error and sets torque = 0 within one control cycle (10 ms). | SPI bus failure is a single point of failure for pedal input. Mitigation: CVC detects SPI errors within 10 ms and transitions to SAFE_STOP. SC cross-plausibility (SM-003) provides independent backup. Design consideration: separate SPI buses for each sensor would eliminate this common cause (addressed in DFA CCF-002). Current design accepts this as a detectable single point. | RPN-012 |

### 5.2 FZC Subsystem (Front Zone Controller -- STM32G474RE)

The FZC controls steering (servo + angle sensor), braking (servo), obstacle detection (lidar), and audible warnings (buzzer).

| ID | Component | Failure Mode | Local Effect | System Effect | S | Detection Method | Recommended Action / Mitigation | RPN Ref |
|----|-----------|-------------|--------------|---------------|---|-----------------|--------------------------------|---------|
| FM-FZC-001 | Steering servo | Stuck (mechanical jam, gear failure, bearing seizure) | Servo does not respond to PWM commands. Steering angle remains at last position. | Loss of steering control. Vehicle cannot change direction. During a turn, vehicle travels straight instead of following curve. **Violates SG-003 (ASIL D)** | 9 | SM-008: Steering angle feedback monitoring (command vs actual position, 5-degree threshold, 50 ms window). FZC detects position error exceeding threshold and declares steering fault. | FZC commands servo PWM off (prevent motor burnout). CVC reduces speed (DEGRADED). If stuck during turn: SC detects anomalous behavior through heartbeat fault status. Kill relay provides ultimate backup. Residual risk: servo jam is not preventable; mitigation is detection and speed reduction. | RPN-013 |
| FM-FZC-002 | Steering servo | Oscillation / runaway (control loop instability, PWM glitch, EMI on signal line) | Servo oscillates rapidly around target position or slews to mechanical limit | Erratic steering -- vehicle weaves unpredictably or steers to full lock. **Violates SG-003 (ASIL D)**. Life-threatening lane departure into oncoming traffic. | 10 | SM-008: Rate-of-change check (max 360 deg/s). SM-009: Steering rate limiting (30 deg/s software clamp). SM-010: Angle limits (-45 to +45 degrees). FZC detects oscillation via rate-of-change threshold and commands servo to center. | SM-009 rate limiting prevents abrupt steering. SM-010 angle limits prevent mechanical overtravel. On oscillation detection: FZC disables servo PWM entirely and commands center via separate GPIO. CVC transitions to SAFE_STOP. | RPN-014 |
| FM-FZC-003 | Steering angle sensor (AS5048A on SPI2) | Failure (stuck, disconnected, magnet displaced) | FZC loses position feedback for closed-loop steering control | FZC cannot verify steering position. Open-loop servo control is unreliable. FZC transitions to return-to-center on sensor fault (SM-008). **Impacts SG-003 (ASIL D)** | 7 | SM-008: Sensor range check (valid range: -45 to +45 degrees), timeout detection, parity check on SPI response. FZC detects invalid sensor data within 10 ms. | FZC commands return-to-center (0 degrees) using calibrated open-loop PWM. CVC reduces speed. System transitions to DEGRADED. If center position cannot be confirmed within 200 ms, FZC disables servo entirely. DTC logged. | RPN-015 |
| FM-FZC-004 | Brake servo | Stuck (mechanical jam, gear failure, linkage disconnection) | Servo does not apply braking force when commanded | Loss of braking capability. Vehicle cannot decelerate via brake actuator. **Violates SG-004 (ASIL D)** | 10 | SM-011: Brake command monitoring (command vs servo current draw). Expected current draw when brake applied -- absence of current indicates servo not actuating. FZC detects brake fault within 20 ms. | SM-013: Motor cutoff as backup deceleration (CVC commands RZC torque = 0, H-bridge disabled). SM-005: SC kill relay opens, removing all drive power. Brake servo stuck is a critical failure; motor cutoff provides alternative deceleration through friction and back-EMF. | RPN-016 |
| FM-FZC-005 | Brake servo | Uncommanded activation (PWM glitch, FZC software fault, CAN message corruption) | Brake servo applies force without a brake command from CVC | Unintended braking. Vehicle decelerates unexpectedly. Risk of rear-end collision from following traffic. **Impacts SG-005 (ASIL A)** | 6 | SM-014: Brake command plausibility (FZC validates incoming CAN brake command against vehicle state -- reject brake in INIT/OFF mode). SM-004: E2E protection detects CAN corruption. FZC detects invalid brake activation within 10 ms. | FZC releases brake on plausibility fault. CVC notified via CAN. If brake activation is due to FZC software fault: SC detects anomalous heartbeat status and may open kill relay. System transitions to DEGRADED. | RPN-017 |
| FM-FZC-006 | TFMini-S lidar | Stuck at maximum range (12 m) or returning maximum constantly | FZC reads maximum distance regardless of actual obstacle position | False "all clear" -- obstacle in path is not detected. Emergency braking does not trigger. **Violates SG-007 (ASIL C)** | 8 | SM-018: Stuck sensor detection (distance does not change by > 1 cm over 50 consecutive samples = 500 ms). Signal strength check (low strength + constant value indicates fault). Timeout monitoring for UART frames. | On stuck detection: FZC substitutes safe default distance (0 cm = obstacle present), triggering protective braking. System transitions to DEGRADED with speed limiting. DTC logged. Residual risk: 500 ms detection latency for stuck sensor. | RPN-018 |
| FM-FZC-007 | TFMini-S lidar | Stuck at minimum range (0 cm) or returning near-zero constantly | FZC reads zero distance regardless of actual environment | Phantom braking -- vehicle brakes for non-existent obstacle. Unexpected deceleration. **Impacts SG-005 (ASIL A)** | 5 | SM-018: Stuck sensor detection (constant value over 500 ms). Signal strength check. SM-014: Brake command plausibility (repeated emergency brake without speed reduction confirms false trigger). | On false positive detection: FZC suppresses emergency brake. System transitions to DEGRADED (lidar function degraded). Operator warned via buzzer. Low severity because phantom braking is fail-safe (vehicle stops). | RPN-019 |
| FM-FZC-008 | FZC CAN interface (FDCAN + TJA1051T) | CAN failure (TX and/or RX) | FZC cannot receive steering/brake commands from CVC or transmit status/heartbeat | No steering or brake commands received. FZC activates auto-brake on CAN timeout (SM-012). FZC heartbeat missing -- SC detects and opens kill relay. **Impacts SG-003, SG-004 (ASIL D)** | 8 | SM-019: SC heartbeat monitoring (FZC heartbeat timeout = 150 ms). SM-012: FZC internal CAN timeout triggers auto-brake (100 ms). CVC detects FZC status message timeout. | FZC autonomous auto-brake provides immediate safe action without requiring CAN communication. SC kill relay provides system-level backup. FZC CAN failure results in vehicle stopping. | RPN-020 |
| FM-FZC-009 | FZC MCU (STM32G474RE) | Software hang (infinite loop, deadlock, stack overflow) | FZC stops executing all functions: no servo control, no lidar reading, no CAN | Steering and braking functions completely lost. Steering servo holds last PWM (may drift). Brake servo holds last position. **Violates SG-003, SG-004 (ASIL D)** | 10 | SM-020: External watchdog (TPS3823) forces FZC hardware reset within 1.6 seconds. SM-019: SC heartbeat monitoring detects FZC silence within 150 ms. CVC detects FZC status timeout. SM-005: SC kill relay opens on heartbeat loss. | Multi-layer detection: watchdog reset (1.6 s) and SC heartbeat (150 ms). SC kill relay removes power from servos. Servo PWM loss causes servo to coast to neutral (passive return). CVC transitions to SAFE_STOP on FZC heartbeat loss. | RPN-021 |
| FM-FZC-010 | Buzzer (piezo, GPIO-driven) | Failure (open circuit, driver fault) | FZC cannot produce audible warning tones | Loss of audible warning channel. Operator may not hear fault warnings or obstacle alerts. No direct vehicle control impact. | 3 | FZC buzzer self-test at INIT (brief tone). GPIO output feedback readback (if available on pin). During operation: buzzer failure is not directly detectable without hardware feedback circuit. | Redundant warning channels: OLED display (CVC), fault LEDs (SC), ICU dashboard (simulated). Buzzer failure is not safety-critical because multiple independent warning channels exist. DTC logged if self-test fails. | RPN-022 |

### 5.3 RZC Subsystem (Rear Zone Controller -- STM32G474RE)

The RZC controls the DC motor (BTS7960 H-bridge), monitors motor current (ACS723), motor/winding temperature (NTC x2), motor speed (encoder), and battery voltage.

| ID | Component | Failure Mode | Local Effect | System Effect | S | Detection Method | Recommended Action / Mitigation | RPN Ref |
|----|-----------|-------------|--------------|---------------|---|-----------------|--------------------------------|---------|
| FM-RZC-001 | Motor driver (BTS7960) | Short-through (high-side and low-side FETs conduct simultaneously) | Shoot-through current through H-bridge. Motor may spin uncontrolled or at full power in one direction. Potential BTS7960 thermal damage. | Motor runs at full uncontrolled power. Unintended acceleration. **Violates SG-001 (ASIL D)** | 9 | SM-002: RZC overcurrent cutoff (ACS723 > 25A for > 10 ms triggers H-bridge disable). BTS7960 built-in overcurrent shutdown at ~43A (hardware backup). SM-003: SC cross-plausibility (torque request = 0 but current > threshold). SM-005: Kill relay. | RZC disables both enable lines (R_EN = LOW, L_EN = LOW) on overcurrent. If software detection fails, BTS7960 hardware overcurrent shuts down driver. SC kill relay removes power entirely. Multiple independent layers. | RPN-023 |
| FM-RZC-002 | Motor driver (BTS7960) | Open circuit (FET failure, enable pin disconnection, wiring fault) | Motor receives no current. H-bridge outputs are high-impedance. | Motor stops producing torque. Loss of drive function. Vehicle decelerates through friction. **Impacts SG-002 (ASIL B)** | 5 | SM-006: Motor controller health monitoring (PWM active but no current = open circuit). RZC detects zero current despite non-zero PWM within 50 ms. SM-007: CVC alive monitoring detects anomalous RZC status. | RZC broadcasts motor fault DTC. CVC transitions to DEGRADED. Motor open circuit is fail-safe (no unintended motion). Controlled stop initiated. | RPN-024 |
| FM-RZC-003 | Current sensor (ACS723) | Stuck low (output = 0A reading regardless of actual current) | RZC reads zero motor current even when motor is drawing significant current | Motor overcurrent goes undetected by software. Only BTS7960 hardware overcurrent protection remains. Prolonged overcurrent causes motor heating, potential fire. **Impacts SG-006 (ASIL A), SG-001 (ASIL D via SM-002 loss)** | 8 | SM-003: SC cross-plausibility (torque request > 0 but reported current = 0 is implausible). RZC plausibility: PWM active + encoder shows rotation + current = 0 is inconsistent. BTS7960 hardware overcurrent at ~43A provides last-resort protection. | RZC implements current-plausibility: if PWM > 20% and encoder shows rotation but current reads 0 for > 100 ms, declare sensor fault. Transition to DEGRADED with reduced torque limit. BTS7960 hardware protection covers catastrophic overcurrent. | RPN-025 |
| FM-RZC-004 | Current sensor (ACS723) | Stuck high (output = maximum reading regardless of actual current) | RZC reads high current even when motor is idle or lightly loaded | False overcurrent detection. RZC disables motor unnecessarily. Nuisance trip -- loss of motor function. **Impacts SG-002 (ASIL B)** | 3 | RZC plausibility: high current reading + zero PWM duty cycle = implausible (sensor fault, not real overcurrent). SM-006: motor controller health check detects inconsistency. | On implausible sensor reading: RZC logs sensor fault DTC. System transitions to DEGRADED with reduced torque limit. False overcurrent is fail-safe (motor disabled). | RPN-026 |
| FM-RZC-005 | Temperature sensor 1 (NTC thermistor, motor body) | Open circuit (NTC disconnected, wire break) | RZC ADC reads 0V (NTC pull-up to 3.3V with NTC open = ADC sees high resistance = near 3.3V, depending on circuit; or 0V if NTC is in high-side configuration) -- reads as extreme temperature | Overtemperature not detected if reading maps to cold temperature. OR false overtemperature if reading maps to hot temperature. Behavior depends on NTC circuit topology. **Impacts SG-006 (ASIL A)** | 6 | SM-015: Temperature range check (reading < -30C or > 150C = sensor fault). Dual NTC cross-plausibility: if sensor 1 and sensor 2 disagree by > 20C for > 1 second, declare sensor fault. | On sensor fault: RZC uses remaining healthy NTC sensor. If both NTC sensors fail: RZC transitions to DEGRADED with reduced torque limit (de-rate to 50%). DTC logged. | RPN-027 |
| FM-RZC-006 | Temperature sensor 2 (NTC thermistor, motor winding) | Short circuit (NTC shorted, wire short to ground) | RZC ADC reads near 0V, which maps to very high temperature | False overtemperature detection. RZC unnecessarily de-rates or disables motor. Nuisance trip. | 2 | SM-015: Dual NTC cross-plausibility (sensor 1 at 25C, sensor 2 at 150C = implausible). ADC range check. | On plausibility failure: RZC uses remaining healthy NTC. False overtemperature is fail-safe (motor de-rated or disabled). DTC logged. | RPN-028 |
| FM-RZC-007 | Motor encoder (timer input capture) | Failure (encoder cable disconnected, encoder IC fault, magnet misalignment) | RZC cannot measure motor speed or position | Speed feedback lost. Open-loop motor control only. Speed-dependent safety functions degraded (e.g., speed limiting in DEGRADED/LIMP modes). **Impacts SG-002 (ASIL B)** | 5 | RZC encoder plausibility: if PWM > 0 and current > threshold but encoder count unchanged for > 200 ms, declare encoder fault. Timer input capture timeout detection. | On encoder fault: RZC transitions to DEGRADED. Speed estimated from current and voltage (rough approximation). Torque limited. DTC logged. CVC reduces speed command. SC continues independent monitoring. | RPN-029 |
| FM-RZC-008 | Motor (DC brushed motor) | Winding short circuit (turn-to-turn short, insulation breakdown) | Increased current draw, reduced torque, localized heating in shorted turns | Overcurrent triggers SM-002 protection. Motor performance degrades. Potential smoke or fire if sustained. **Impacts SG-006 (ASIL A)** | 7 | SM-002: Overcurrent cutoff (ACS723, 25A threshold, 10 ms debounce). SM-015: Temperature monitoring (NTC sensors detect elevated winding temperature). SM-003: SC cross-plausibility (torque request low but current high). | SM-002 triggers motor disable on overcurrent. SM-015 de-rates motor on temperature rise. If winding short develops gradually, temperature monitoring provides early warning before overcurrent threshold. System transitions to SAFE_STOP. | RPN-030 |
| FM-RZC-009 | RZC CAN interface (FDCAN + TJA1051T) | CAN failure (TX and/or RX) | RZC cannot receive torque commands from CVC or transmit status/heartbeat | No torque commands received. RZC defaults to zero torque on CAN timeout (safe default). RZC heartbeat missing -- SC detects and opens kill relay. **Impacts SG-001, SG-002 (ASIL B-D)** | 7 | SM-019: SC heartbeat monitoring (RZC heartbeat timeout = 150 ms). CVC detects RZC status message timeout. SM-007: CAN alive monitoring. | RZC CAN timeout safe default: torque = 0, H-bridge disabled. SC kill relay removes power. Motor stops. Fail-safe design: loss of communication = motor off. | RPN-031 |
| FM-RZC-010 | RZC MCU (STM32G474RE) | Software hang (infinite loop, deadlock, stack overflow) | RZC stops executing all functions: PWM outputs freeze at last value, no CAN, no protection functions | Motor PWM may freeze at last duty cycle. Motor continues running at last speed. Overcurrent protection lost (software-based). **Violates SG-001 (ASIL D), SG-006 (ASIL A)** | 9 | SM-020: External watchdog (TPS3823) forces RZC hardware reset within 1.6 seconds. SM-019: SC heartbeat monitoring detects RZC silence within 150 ms. SM-005: Kill relay removes motor power. BTS7960 hardware overcurrent at ~43A (hardware protection survives software hang). | Multi-layer: (1) BTS7960 hardware overcurrent (immediate, independent of software). (2) SC heartbeat + kill relay (150 ms + relay dropout). (3) External watchdog reset (1.6 s). Critical gap: PWM may freeze at high duty cycle for up to 150 ms before SC intervenes. SM-002 hardware overcurrent provides partial coverage. | RPN-032 |

### 5.4 SC Subsystem (Safety Controller -- TMS570LC43x)

The SC is the independent safety monitor: CAN listen-only, heartbeat monitoring, cross-plausibility, kill relay control, lockstep CPU, external watchdog.

| ID | Component | Failure Mode | Local Effect | System Effect | S | Detection Method | Recommended Action / Mitigation | RPN Ref |
|----|-----------|-------------|--------------|---------------|---|-----------------|--------------------------------|---------|
| FM-SC-001 | SC CAN interface (DCAN1 + SN65HVD230) | CAN interface failure (DCAN peripheral fault, transceiver failure, connector fault) | SC cannot receive any CAN messages from zone ECUs | SC loses all monitoring capability: no heartbeats, no cross-plausibility data. SC blind to system state. **Impacts SG-008 (ASIL C)** | 8 | SC internal DCAN self-test at startup (loop-back mode). During operation: SC monitors for CAN silence -- if no messages received from any node for > 200 ms (abnormal on a healthy bus), SC assumes CAN interface fault. SC transitions to fail-safe: opens kill relay. | SC CAN failure = fail-safe (relay opens). Design: SC monitors total bus silence as a self-diagnostic. If SC CAN is faulty but bus is healthy, SC sees silence and kills relay. Conservative but safe. DTC logging via fault LEDs only (no CAN available). | RPN-033 |
| FM-SC-002 | SC heartbeat logic | Logic error -- false fault detection (incorrect timeout calculation, software bug in heartbeat counter) | SC incorrectly determines that a healthy ECU has timed out | False kill relay activation. System enters SHUTDOWN unnecessarily. Vehicle stops abruptly. Loss of availability. | 4 | SC firmware unit testing with 100% MC/DC coverage for heartbeat logic (ASIL D verification). Runtime: zone ECU heartbeat status available on CAN for independent verification by CVC. If CVC sees healthy ECU but SC kills relay, CVC logs "SC false trigger" DTC. | Software verification: formal review + unit testing of heartbeat logic. Debounce timer (50 ms) reduces false triggers from CAN jitter. Heartbeat timeout set to 3x heartbeat interval (150 ms) with margin. False kill is fail-safe (safe but unavailable). | RPN-034 |
| FM-SC-003 | SC heartbeat logic | Logic error -- missed fault (incorrect threshold, software bug fails to detect timeout) | SC fails to detect that a zone ECU has stopped sending heartbeats | SC does not open kill relay when it should. Zone ECU fault goes undetected by SC. Other safety mechanisms (CVC timeout, external watchdog) must catch the fault. **Impacts SG-008 (ASIL C)** | 9 | SC lockstep comparison (SM-021) detects computation errors in heartbeat logic. Periodic self-test: SC intentionally skips its own heartbeat counter for one cycle and verifies it triggers an internal fault flag. CVC independently monitors zone ECU heartbeats as backup. | Multi-layer: CVC also monitors FZC/RZC heartbeats. External watchdogs on each ECU provide independent hang detection. SC lockstep detects CPU computation errors. Residual risk: systematic software fault in heartbeat logic that passes lockstep (both cores execute same bug). Mitigation: independent code review, 100% MC/DC coverage testing. | RPN-035 |
| FM-SC-004 | Kill relay (mechanical relay + MOSFET driver) | Stuck closed (relay contacts welded, MOSFET gate-source short) | SC commands relay open (de-energize) but contacts remain closed. Motor power not interrupted. | SC cannot force safe state. Vehicle continues running despite critical fault. All other safety mechanisms at the zone ECU level must function. **Violates SG-008 (ASIL C)**. This is the most critical single-point failure in the safety architecture. | 10 | SM-005: Kill relay self-test at INIT (energize, verify actuator power present via feedback; de-energize, verify actuator power absent). Relay contact feedback circuit: voltage divider on relay output side, read by SC ADC, confirms relay state. | Kill relay self-test at every startup. Periodic relay test during RUN (brief open-close cycle during idle periods, with operator warning). Relay feedback circuit provides runtime monitoring. If feedback indicates relay stuck closed: SC illuminates all fault LEDs, broadcasts fault (if CAN available), system enters permanent fault state requiring physical relay replacement. Select relay with adequate contact rating (2x maximum load current) to prevent welding. | RPN-036 |
| FM-SC-005 | Kill relay | Stuck open (relay coil failed, MOSFET failed open, wiring disconnection) | Kill relay cannot close. No actuator power available. Motor and servos cannot be powered. | System cannot exit SHUTDOWN state. Vehicle permanently inoperable until relay/wiring repaired. No safety hazard (safe state maintained). | 3 | SM-005: Kill relay self-test at INIT (energize, verify actuator power present). If power not detected after energize: relay stuck open. SC fault LED panel shows system fault. | DTC logged. Physical inspection required. Fail-safe condition: relay stuck open = no motion possible. Availability issue only. | RPN-037 |
| FM-SC-006 | SC lockstep CPU (Cortex-R5 dual-core comparator) | Lockstep compare error (SEU in one core, transient fault, manufacturing defect) | TMS570 ESM (Error Signaling Module) triggers immediate CPU reset. SC firmware restarts. | SC resets, briefly losing monitoring capability. During reset (~50 ms): no heartbeat monitoring, no cross-plausibility. Kill relay behavior during reset depends on GPIO state: de-energize-to-run pattern means reset = relay opens = safe state. | 5 | Hardware lockstep comparator (every clock cycle). ESM generates NMI on mismatch. SC external watchdog (TPS3823) provides backup if ESM fails. | Lockstep error = SC reset = relay opens (safe state, by design). After reset: SC performs full self-test before re-energizing relay. Transient lockstep errors are expected (SEU rate in automotive environment); system recovers automatically. Persistent lockstep errors indicate hardware damage. | RPN-038 |
| FM-SC-007 | External watchdog (TPS3823) | Watchdog IC failure (no reset output on timeout, oscillator failure) | SC software hang is not detected by external watchdog. SC remains hung. | SC remains in last state indefinitely. If SC was monitoring normally when it hung: relay remains energized (last state was "run"). SC monitoring stops -- subsequent zone ECU faults go undetected by SC. **Impacts SG-008 (ASIL C)** | 6 | TPS3823 self-test at startup: SC intentionally delays WDI toggle beyond timeout and verifies reset occurs. If no reset: TPS3823 declared faulty. Runtime: no direct detection of TPS3823 failure during operation (latent fault). | TPS3823 failure is a latent fault. Mitigation: startup self-test catches most failure modes. Select TPS3823 for its proven automotive reliability. CVC independently monitors SC heartbeat (SC transmits periodic CAN status if CAN is functional). Residual risk: SC hang + TPS3823 failure is a double-point failure with very low probability. | RPN-039 |
| FM-SC-008 | SC MCU (TMS570LC43x) | Software hang (infinite loop, deadlock, incorrect program flow) | SC stops executing: no heartbeat monitoring, no cross-plausibility, no watchdog toggle | SC external watchdog (TPS3823) resets SC within 1.6 seconds. During hang: relay state frozen. If relay was energized: system runs unmonitored for up to 1.6 seconds. After reset: SC re-initializes and re-evaluates system state. **Impacts SG-008 (ASIL C)** | 8 | SM-021: Lockstep detects CPU computation errors (but not all software logic faults). SM-020: TPS3823 external watchdog resets SC within 1.6 seconds. CVC monitors SC CAN status message (if SC was transmitting). Zone ECU external watchdogs provide independent local protection. | SC hang is detected and recovered by TPS3823 within 1.6 seconds. During hang: zone ECUs continue operating with their own local safety mechanisms. SC re-initialization: full self-test, relay test, CAN bus scan before re-energizing relay. Gap: 1.6 seconds of unmonitored operation exceeds all FTTI values. Mitigation: primary safety mechanisms on zone ECUs (SM-001, SM-002, SM-008, SM-011) provide FTTI-compliant coverage independently of SC. | RPN-040 |

### 5.5 CAN Bus Infrastructure

The CAN bus is the shared communication medium for all inter-ECU messages including safety-critical torque, steering, brake, and heartbeat messages.

| ID | Component | Failure Mode | Local Effect | System Effect | S | Detection Method | Recommended Action / Mitigation | RPN Ref |
|----|-----------|-------------|--------------|---------------|---|-----------------|--------------------------------|---------|
| FM-CAN-001 | CAN bus wiring | Open circuit (wire break on CAN_H or CAN_L) | Bus reflections, increased error rate. If both wires break: total communication loss. | If single wire: degraded communication (error frames increase, some messages lost, bus eventually reaches error-passive state). If both wires: total loss -- all ECUs isolated. **Impacts SG-001 through SG-008** | 9 | SM-004: E2E protection detects message loss/corruption via alive counter gaps. SM-019: SC heartbeat monitoring detects communication silence. CVC/FZC/RZC CAN error counters (bus-off detection). | CAN error counters provide node-level diagnostics. SM-012: FZC auto-brakes on timeout. SM-005: SC kill relay on heartbeat timeout. All ECUs implement safe defaults on CAN loss. Proper bus routing, strain relief, and connector quality reduce wire break probability. | RPN-041 |
| FM-CAN-002 | CAN bus wiring | Short to ground (CAN_H or CAN_L shorted to GND, or CAN_H shorted to CAN_L) | Bus stuck in dominant state. No communication possible (bus permanently at dominant level). | Total communication loss. All ECU transmissions blocked. All timeout-based safety mechanisms activate. **Impacts SG-001 through SG-008** | 9 | Same as FM-CAN-001. CAN controller detects bus-stuck-dominant condition. SM-019: SC detects total silence. | Same mitigation as FM-CAN-001. Bus stuck dominant is equivalent to total communication loss -- all timeout-based safe defaults activate. Hardware mitigation: proper CAN bus routing with physical separation from power lines, shielded cables. | RPN-042 |
| FM-CAN-003 | CAN transceiver (single node -- TJA1051T or SN65HVD230) | Transceiver failure (stuck dominant, stuck recessive, or internal fault) | Single node disconnected from bus, or single node jams bus in dominant state | Single node lost: that ECU's functions impaired; other ECUs functional. Stuck dominant: node becomes babbling (jams bus for all). **Impacts: per-node failure = moderate; babbling = severe (FM-CAN-005)** | 6 | CAN controller error counters on each node. Bus-off detection: node enters bus-off after 256 error frames (automatic CAN protocol). SM-019 + SM-004 detect consequence (missing messages). | Single node loss: detected via heartbeat timeout; other ECUs continue. Stuck-dominant babbling: CAN protocol bus-off mechanism removes the babbling node after 256 errors (~2 ms at 500 kbps). SC in listen-only mode is immune to bus-off. | RPN-043 |
| FM-CAN-004 | CAN messages | Message corruption (bit errors due to EMI, ground noise, marginal timing) | CAN CRC detects most single- and multi-bit errors. If CAN CRC passes but data is wrong (extremely rare): E2E CRC catches residual errors. | If both CAN CRC and E2E CRC pass on corrupted data (probability < 10^-9 per message): wrong data accepted. Incorrect torque/steering/brake command executed. **Potentially impacts all safety goals** | 8 | CAN hardware CRC-15 (Hamming distance 6, detects up to 5-bit errors in 130-bit frame). SM-004: E2E CRC-8 (Hamming distance 4 for 8-byte payload, detects additional data errors). Alive counter detects message repetition/delay. Data ID detects message masquerading. | Dual-layer CRC (CAN CRC-15 + E2E CRC-8) provides combined detection probability > 99.9999%. Alive counter detects stuck/repeated messages. Range checking on received values (torque 0-100%, angle -45 to +45 degrees) catches out-of-range corrupted values. Residual risk: extremely low probability of undetected corruption. | RPN-044 |
| FM-CAN-005 | CAN bus | Bus overload / babbling node (single ECU floods bus with high-priority messages) | CAN bus saturated. Lower-priority messages delayed or lost. Safety-critical messages may be blocked if babbling node uses higher CAN ID priority. | Safety-critical messages (torque, brake, heartbeat) delayed. If delay exceeds timeout: timeout-based safe defaults activate. **Impacts SG-001 through SG-008** | 6 | CAN controller transmit-failure counters (messages stuck in TX buffer). CVC monitors message cycle-time deviations. SM-019: SC detects abnormal bus activity pattern (high message rate from single ID). CAN protocol bus-off mechanism (128 error frames = bus-off). | CAN ID priority assignment: safety-critical messages use highest priority IDs (lowest numerical CAN ID). E-stop: 0x001. Heartbeats: 0x010-0x013. Torque commands: 0x100-0x1FF. CAN bus-off mechanism automatically disables babbling node. SC in listen-only mode remains functional during bus overload. | RPN-045 |

### 5.6 Power Supply Subsystem

The power supply provides 12V (motor, relay, servos), 5V (lidar, Pi), and 3.3V (MCUs, sensors, transceivers) to all system components.

| ID | Component | Failure Mode | Local Effect | System Effect | S | Detection Method | Recommended Action / Mitigation | RPN Ref |
|----|-----------|-------------|--------------|---------------|---|-----------------|--------------------------------|---------|
| FM-PWR-001 | 12V bench supply | Total supply loss (supply failure, fuse blown, cable disconnection) | No power to any system component | Total system shutdown. Kill relay opens (de-energize-to-run pattern: loss of power = relay opens = safe state). Motor stops (no power). Servos coast to neutral. All ECUs power down. | 8 | N/A (power loss = everything stops). Post-event: DTC log analysis (last logged event before power loss, if flash write completed). | Energize-to-run relay pattern ensures power loss is inherently safe. Motor stops, servos de-power. No uncontrolled motion after power loss. System requires full restart after power restoration. | RPN-046 |
| FM-PWR-002 | 5V buck converter | 5V rail failure (converter fault, inductor failure, capacitor degradation) | TFMini-S lidar and Raspberry Pi lose power | Lidar lost: no obstacle detection (FZC detects UART timeout via SM-018). Pi lost: no cloud telemetry or edge ML (QM, no safety impact). **Impacts SG-007 (ASIL C)** | 5 | SM-018: Lidar UART timeout detection (100 ms). FZC declares lidar fault and substitutes safe default (0 cm). Pi loss: gateway health check from cloud side (non-safety). | FZC transitions to DEGRADED on lidar loss (speed limited). Core ECU functions (12V/3.3V) unaffected. Lidar loss triggers safe default (assume obstacle present). | RPN-047 |
| FM-PWR-003 | 3.3V regulator (per-ECU: Nucleo onboard or buck converter) | 3.3V rail failure (regulator fault, input undervoltage) | Affected MCU loses power. MCU resets or enters undefined state. All MCU functions cease. | Effect depends on which ECU loses 3.3V: CVC loss = FM-CVC-011 equivalent; FZC loss = FM-FZC-009 equivalent; RZC loss = FM-RZC-010 equivalent; SC loss = relay opens (safe). **Impacts all safety goals for the affected ECU** | 9 | MCU brown-out detection (BOD) triggers reset before entering undefined state. SM-020: External watchdog detects MCU inactivity. SM-019: SC detects heartbeat timeout. | Each ECU has independent 3.3V regulation (separate failure domains). SC 3.3V loss = relay opens = safe state. Zone ECU 3.3V loss = detected by SC within 150 ms. Brown-out detection prevents undefined MCU behavior. | RPN-048 |
| FM-PWR-004 | Ground plane / wiring | Ground loop / noise injection (poor star grounding, common impedance coupling) | ADC readings corrupted by noise. Sensor values fluctuate. Digital communication may be affected. | ADC noise affects current sensor (ACS723), temperature sensors (NTC), and battery voltage readings. Possible false overcurrent or false temperature readings. **Impacts SG-006 (ASIL A)** | 4 | ADC software filtering (running average, median filter) rejects noise spikes. Sensor plausibility checks (SM-015, SM-016) detect out-of-range values. Dual NTC cross-plausibility detects single sensor noise. | Star ground topology on proto boards. Separate analog and digital ground paths. ADC input filtering (RC low-pass). Software filtering (3-sample median filter). Shielded cables for analog sensor connections. | RPN-049 |
| FM-PWR-005 | 12V supply / wiring | Reverse polarity (supply connected backwards) | Reverse voltage across all components. Potential permanent damage to unprotected ICs. | Catastrophic hardware damage. All ECUs, sensors, and actuators potentially destroyed. Complete system replacement required. | 8 | N/A (damage occurs before detection is possible). Prevention only. | Reverse polarity protection diode (Schottky) on 12V input. Fuse on 12V line (5A) limits damage propagation. Polarity-keyed connectors prevent incorrect connection. Board-level reverse protection on each ECU. | RPN-050 |

## 6. Failure Mode Summary Statistics

### 6.1 Count by Subsystem

| Subsystem | Failure Modes | Severity >= 8 | Severity >= 9 |
|-----------|--------------|---------------|---------------|
| CVC | 12 | 4 | 3 |
| FZC | 10 | 3 | 2 |
| RZC | 10 | 3 | 2 |
| SC | 8 | 3 | 1 |
| CAN Bus | 5 | 2 | 2 |
| Power Supply | 5 | 2 | 1 |
| **Total** | **50** | **17** | **11** |

### 6.2 Count by Severity

| Severity | Count | Percentage | Description |
|----------|-------|------------|-------------|
| 10 | 4 | 8% | Hazardous without warning (SG-001, SG-003, SG-004, SG-008 violation) |
| 9 | 7 | 14% | Hazardous with warning (critical function loss, software hang) |
| 8 | 6 | 12% | Very high (monitoring loss, severe degradation) |
| 7 | 4 | 8% | High (significant performance impact) |
| 6 | 4 | 8% | Moderate (degraded but operable) |
| 5 | 5 | 10% | Low (noticeable degradation) |
| 4 | 2 | 4% | Very low (minor effect) |
| 3 | 4 | 8% | Minor (annoyance, fail-safe nuisance) |
| 2 | 3 | 6% | Very minor (negligible) |
| 1 | 0 | 0% | None |
| **Unassigned (RPN pending)** | **11** | **22%** | Severity 5-8 range, non-critical |

### 6.3 Safety Goal Impact Summary

| Safety Goal | ASIL | Failure Modes Affecting | Most Critical FM |
|-------------|------|------------------------|-----------------|
| SG-001 (Prevent unintended acceleration) | D | FM-CVC-001, FM-CVC-003, FM-CVC-005, FM-CVC-012, FM-RZC-001, FM-RZC-010, FM-CAN-004 | FM-CVC-005 (both sensors agree on wrong value, S=10) |
| SG-002 (Prevent loss of drive torque) | B | FM-CVC-002, FM-RZC-002, FM-RZC-004, FM-RZC-007, FM-RZC-009 | FM-RZC-002 (motor driver open circuit, S=5) |
| SG-003 (Prevent unintended steering) | D | FM-FZC-001, FM-FZC-002, FM-FZC-003, FM-FZC-008, FM-FZC-009 | FM-FZC-002 (servo oscillation, S=10) |
| SG-004 (Prevent loss of braking) | D | FM-FZC-004, FM-FZC-008, FM-FZC-009 | FM-FZC-004 (brake servo stuck, S=10) |
| SG-005 (Prevent unintended braking) | A | FM-FZC-005, FM-FZC-007 | FM-FZC-005 (brake servo uncommanded, S=6) |
| SG-006 (Motor protection) | A | FM-RZC-003, FM-RZC-005, FM-RZC-006, FM-RZC-008, FM-PWR-004 | FM-RZC-003 (current sensor stuck low, S=8) |
| SG-007 (Obstacle detection) | C | FM-FZC-006, FM-FZC-007, FM-PWR-002 | FM-FZC-006 (lidar stuck at max, S=8) |
| SG-008 (Independent safety monitoring) | C | FM-SC-001 through FM-SC-008, FM-CVC-006, FM-CAN-001, FM-CAN-002 | FM-SC-004 (kill relay stuck closed, S=10) |

## 7. Critical Failure Modes (Severity >= 9)

The following failure modes have severity >= 9 and require the highest level of mitigation assurance.

| FM-ID | Component | Failure Mode | S | Safety Goal | Safety Mechanisms | Independence Layers |
|-------|-----------|-------------|---|-------------|-------------------|-------------------|
| FM-CVC-001 | Pedal sensor 1 | Stuck at max | 10 | SG-001 (D) | SM-001 (dual plausibility), SM-002 (overcurrent), SM-003 (cross-plausibility), SM-005 (kill relay) | 4 layers: CVC SW, RZC SW, RZC HW (BTS7960), SC HW (relay) |
| FM-CVC-005 | Both pedal sensors | Agree on wrong value (CCF) | 10 | SG-001 (D) | SM-003 (SC cross-plausibility), SM-002 (overcurrent), SM-005 (kill relay) | 3 layers: SC SW (different vendor), RZC HW (BTS7960), SC HW (relay) |
| FM-CVC-011 | CVC MCU | Software hang | 9 | All SGs | SM-020 (ext. WDT), SM-019 (SC heartbeat), SM-012 (FZC auto-brake), SM-005 (kill relay) | 4 layers: TPS3823 HW, SC SW, FZC SW, SC HW (relay) |
| FM-CVC-012 | CVC SPI bus | Bus failure | 9 | SG-001 (D) | SM-001 (detects SPI errors), SM-003 (SC cross-plausibility), SM-005 (kill relay) | 3 layers: CVC SW, SC SW (different vendor), SC HW (relay) |
| FM-FZC-002 | Steering servo | Oscillation | 10 | SG-003 (D) | SM-008 (rate check), SM-009 (rate limiting), SM-010 (angle limits), SM-005 (kill relay) | 3 layers: FZC SW, FZC SW (multiple checks), SC HW (relay) |
| FM-FZC-004 | Brake servo | Stuck | 10 | SG-004 (D) | SM-011 (brake monitoring), SM-013 (motor cutoff), SM-005 (kill relay) | 3 layers: FZC SW, CVC+RZC SW, SC HW (relay) |
| FM-FZC-009 | FZC MCU | Software hang | 10 | SG-003, SG-004 (D) | SM-020 (ext. WDT), SM-019 (SC heartbeat), SM-005 (kill relay) | 3 layers: TPS3823 HW, SC SW, SC HW (relay) |
| FM-RZC-001 | BTS7960 | Short-through | 9 | SG-001 (D) | SM-002 (overcurrent), BTS7960 HW protection, SM-003 (SC cross-plausibility), SM-005 (kill relay) | 4 layers: RZC SW, BTS7960 HW, SC SW, SC HW (relay) |
| FM-RZC-010 | RZC MCU | Software hang | 9 | SG-001, SG-006 | SM-020 (ext. WDT), SM-019 (SC heartbeat), SM-005 (kill relay), BTS7960 HW protection | 4 layers: TPS3823 HW, SC SW, SC HW (relay), BTS7960 HW |
| FM-SC-003 | SC heartbeat logic | Missed fault | 9 | SG-008 (C) | SM-021 (lockstep), CVC independent monitoring, per-ECU external watchdogs | 3 layers: TMS570 HW lockstep, CVC SW, TPS3823 HW |
| FM-SC-004 | Kill relay | Stuck closed | 10 | SG-008 (C) | Relay self-test at INIT, relay contact feedback circuit, zone ECU local safety mechanisms | 2 layers: relay feedback HW, zone ECU SW safety mechanisms |

## 8. Single-Point Failure Analysis

The following failure modes are identified as potential single-point failures (SPF) or residual faults (RF) per ISO 26262-5 Clause 8.

| FM-ID | Failure Mode | SPF/RF Classification | Justification | SPFM Contribution |
|-------|-------------|----------------------|---------------|-------------------|
| FM-SC-004 | Kill relay stuck closed | Residual fault | Relay is the ultimate safety backup. Stuck-closed relay means the backup is unavailable, but zone ECU local mechanisms still function. Self-test and feedback circuit provide detection. | RF (detected latent fault if feedback circuit functional) |
| FM-CVC-005 | Both sensors agree wrong | Residual fault | Common cause bypasses dual sensor redundancy. SC cross-plausibility provides diverse detection but with timing limitation (60 ms > 50 ms FTTI). RZC overcurrent provides partial coverage. | RF (partially detected by diverse mechanism) |
| FM-CAN-001 | CAN bus open circuit | Single-point detectable | Single CAN bus is shared infrastructure. All timeout mechanisms detect the consequence. SC kill relay provides safe state. | SPF-D (detected, safe state achieved) |

## 9. Recommended Actions Summary

### 9.1 Design Actions (Before Implementation)

| Priority | Action | Affected FM | Rationale |
|----------|--------|-------------|-----------|
| 1 | Implement relay contact feedback circuit on SC (ADC readback of relay output) | FM-SC-004 | Kill relay stuck-closed is the most critical residual fault. Feedback enables runtime monitoring. |
| 2 | Use normally-closed E-stop wiring (wire break = E-stop activated) | FM-CVC-006 | Ensures wire break fails safe (E-stop triggers), not dangerous (E-stop lost). |
| 3 | Select relay with adequate contact rating (minimum 2x max load current) | FM-SC-004 | Reduces probability of contact welding. |
| 4 | Implement star ground topology with separate analog/digital grounds | FM-PWR-004 | Reduces ADC noise coupling that could cause false sensor readings. |
| 5 | Add reverse polarity protection (Schottky diode) on 12V input | FM-PWR-005 | Prevents catastrophic hardware damage from incorrect wiring. |

### 9.2 Software Actions (During Implementation)

| Priority | Action | Affected FM | Rationale |
|----------|--------|-------------|-----------|
| 1 | Implement SM-001 dual pedal plausibility with 20 ms detection time | FM-CVC-001 to FM-CVC-005 | Primary protection for SG-001 (ASIL D). |
| 2 | Implement SM-002 overcurrent cutoff with 11 ms response | FM-RZC-001, FM-RZC-008, FM-RZC-010 | Hardware-backed motor protection. |
| 3 | Implement SM-012 auto-brake on CAN timeout | FM-FZC-008, FM-CAN-001, FM-CAN-002 | FZC autonomous braking on communication loss. |
| 4 | Implement SM-019 heartbeat monitoring with 150 ms timeout | FM-SC-003, FM-CVC-011, FM-FZC-009, FM-RZC-010 | System-level fault detection for all ECU hangs. |
| 5 | Implement SM-018 lidar plausibility (stuck, timeout, range, signal strength) | FM-FZC-006, FM-FZC-007 | Obstacle detection integrity. |
| 6 | Implement CAN E2E protection (CRC-8, alive counter, data ID) on all safety messages | FM-CAN-004, FM-CAN-005 | Communication integrity for ASIL D messages. |

### 9.3 Verification Actions (During Testing)

| Priority | Action | Affected FM | Method |
|----------|--------|-------------|--------|
| 1 | Fault injection: pedal sensor stuck-at faults (SPI manipulation, power disconnect) | FM-CVC-001 to FM-CVC-005 | Hardware fault injection, SIL simulation |
| 2 | Fault injection: CAN bus disconnect, short-to-ground, babbling node | FM-CAN-001 to FM-CAN-005 | CAN bus fault injection tool |
| 3 | Fault injection: FZC/RZC software hang (watchdog test, infinite loop injection) | FM-FZC-009, FM-RZC-010 | Software fault injection, debug breakpoint |
| 4 | Kill relay stuck-closed simulation (bypass relay feedback, verify zone ECU response) | FM-SC-004 | Hardware fault injection |
| 5 | Power supply fault injection (12V removal, 5V dropout, 3.3V brownout) | FM-PWR-001 to FM-PWR-003 | Power supply control via programmable supply |

## 10. RPN Calculation Note

The RPN (Risk Priority Number) values referenced in the FMEA table (RPN-001 through RPN-050) are placeholders for the quantitative RPN calculation to be performed during the detailed hardware FMEA in Phase 5 (hardware integration). The RPN is calculated as:

```
RPN = Severity (S) x Occurrence (O) x Detection (D)

Where:
  S = Severity rating (1-10, as assigned in this document)
  O = Occurrence rating (1-10, based on failure rate data from FMEDA)
  D = Detection rating (1-10, based on diagnostic coverage of safety mechanisms)
```

Occurrence (O) and Detection (D) ratings require component-specific failure rate data (FIT rates from manufacturer safety manuals, IEC 62380, or MIL-HDBK-217) and quantitative diagnostic coverage analysis, which are deliverables of the hardware-level FMEA (ISO 26262-5, Clause 8).

## 11. Traceability

### 11.1 FMEA to Safety Goals

| Safety Goal | ASIL | Failure Modes | Count |
|-------------|------|--------------|-------|
| SG-001 | D | FM-CVC-001, FM-CVC-003, FM-CVC-005, FM-CVC-012, FM-RZC-001, FM-RZC-010, FM-CAN-004 | 7 |
| SG-002 | B | FM-CVC-002, FM-RZC-002, FM-RZC-004, FM-RZC-007, FM-RZC-009 | 5 |
| SG-003 | D | FM-FZC-001, FM-FZC-002, FM-FZC-003, FM-FZC-008, FM-FZC-009 | 5 |
| SG-004 | D | FM-FZC-004, FM-FZC-008, FM-FZC-009 | 3 |
| SG-005 | A | FM-FZC-005, FM-FZC-007 | 2 |
| SG-006 | A | FM-RZC-003, FM-RZC-005, FM-RZC-006, FM-RZC-008, FM-PWR-004 | 5 |
| SG-007 | C | FM-FZC-006, FM-FZC-007, FM-PWR-002 | 3 |
| SG-008 | C | FM-CVC-006, FM-SC-001, FM-SC-002, FM-SC-003, FM-SC-004, FM-SC-005, FM-SC-006, FM-SC-007, FM-SC-008, FM-CAN-001, FM-CAN-002 | 11 |

### 11.2 FMEA to Safety Mechanisms

| Safety Mechanism | Failure Modes Detected | Count |
|------------------|----------------------|-------|
| SM-001 (Dual pedal plausibility) | FM-CVC-001 to FM-CVC-005, FM-CVC-012 | 6 |
| SM-002 (Overcurrent cutoff) | FM-CVC-001, FM-RZC-001, FM-RZC-008, FM-RZC-010 | 4 |
| SM-003 (SC cross-plausibility) | FM-CVC-001, FM-CVC-003, FM-CVC-005, FM-RZC-001, FM-RZC-003 | 5 |
| SM-004 (CAN E2E) | FM-CAN-003, FM-CAN-004, FM-CAN-005 | 3 |
| SM-005 (Kill relay) | FM-CVC-011, FM-FZC-009, FM-RZC-010, FM-CAN-001, FM-CAN-002 | 5 |
| SM-008 (Steering feedback) | FM-FZC-001, FM-FZC-002, FM-FZC-003 | 3 |
| SM-011 (Brake monitoring) | FM-FZC-004, FM-FZC-005 | 2 |
| SM-012 (Auto-brake on CAN timeout) | FM-CVC-009, FM-FZC-008, FM-CAN-001 | 3 |
| SM-015 (Temperature monitoring) | FM-RZC-005, FM-RZC-006, FM-RZC-008 | 3 |
| SM-018 (Lidar plausibility) | FM-FZC-006, FM-FZC-007, FM-PWR-002 | 3 |
| SM-019 (Heartbeat monitoring) | FM-CVC-009, FM-CVC-011, FM-FZC-009, FM-RZC-010, FM-SC-001 | 5 |
| SM-020 (External watchdog) | FM-CVC-011, FM-FZC-009, FM-RZC-010, FM-SC-008 | 4 |
| SM-021 (Lockstep) | FM-SC-003, FM-SC-006, FM-SC-008 | 3 |

### 11.3 Cross-Reference to DFA

| FMEA Entry | DFA Entry | Relationship |
|------------|-----------|-------------|
| FM-CVC-005 (both sensors agree wrong) | CCF-002 (same MCU vendor), CCF-005 (same compiler) | Common cause analysis required |
| FM-CVC-012 (SPI bus failure) | CCF-001 (shared CAN bus -- analogous shared bus pattern) | Shared resource failure |
| FM-CAN-001, FM-CAN-002 (CAN bus failure) | CCF-001 (shared CAN bus) | Primary shared resource failure |
| FM-PWR-001 (12V loss) | CCF-003 (shared power supply) | Common power failure |
| FM-PWR-003 (3.3V failure) | CCF-003 (shared power supply) | Derived power failure |
| FM-SC-004 + FM-RZC-010 | CF-001 (cascading failure) | Kill relay failure combined with motor hang |

## 12. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete system-level FMEA: 50 failure modes across 6 subsystems, severity assessment, detection methods, recommended actions, traceability to safety goals and safety mechanisms |

## 13. Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| FSE Lead | _________________ | __________ | __________ |
| System Engineer | _________________ | __________ | __________ |
| HW Engineer | _________________ | __________ | __________ |
| SW Engineer | _________________ | __________ | __________ |
| Safety Manager | _________________ | __________ | __________ |

