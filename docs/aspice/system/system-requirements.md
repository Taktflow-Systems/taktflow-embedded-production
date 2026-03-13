---
document_id: SYSREQ
title: "System Requirements Specification"
version: "1.0"
status: draft
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

## Lessons Learned Rule

Every requirement (SYS-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/system/lessons-learned/`](lessons-learned/). One file per requirement. File naming: `SYS-NNN-<short-title>.md`.


# System Requirements Specification

## 1. Purpose

This document specifies the system-level requirements for the Taktflow Zonal Vehicle Platform, derived from the stakeholder requirements (document STKR) per Automotive SPICE 4.0 SYS.2 (System Requirements Analysis). System requirements define the technical "what" at the system boundary — they are verifiable, traceable, and allocated to system elements in the system architecture (SYS.3).

System requirements bridge the gap between stakeholder needs (expressed in stakeholder language) and technical safety requirements / software requirements (expressed in engineering language). Every system requirement traces upward to at least one stakeholder requirement and downward to at least one technical safety requirement (TSR) or software requirement (SWR).

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| STKR | Stakeholder Requirements | 1.0 |
| ITEM-DEF | Item Definition | 1.0 |
| HARA | Hazard Analysis and Risk Assessment | 1.0 |
| SG | Safety Goals | 1.0 |
| FSR | Functional Safety Requirements | 1.0 |
| FSC | Functional Safety Concept | 1.0 |

## 3. Requirement Conventions

### 3.1 Requirement Structure

Each requirement follows this format:

- **ID**: SYS-NNN (sequential)
- **Title**: Descriptive name
- **Traces up**: STK-NNN (stakeholder requirement parent)
- **Traces down**: TSR-NNN / SWR-NNN (placeholder until TSC and SWE.1 phases)
- **Safety relevance**: ASIL level if safety-related, or QM if not
- **Verification method**: Inspection (I), Analysis (A), Test (T), or Demonstration (D)
- **Verification criteria**: Specific pass/fail criteria
- **Status**: draft | reviewed | approved | implemented | verified

### 3.2 Requirement Language

- "shall" = mandatory
- "should" = recommended
- "may" = optional

All SYS requirements use "shall" unless otherwise noted.

---

## 4. Functional System Requirements — Drive-by-Wire

### SYS-001: Dual Pedal Position Sensing

- **Traces up**: STK-005, STK-016
- **Traces down**: TSR-001, TSR-002, SWR-CVC-001, SWR-CVC-002
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: Both pedal sensors independently report angle values within 1% linearity across the 0-100% pedal travel range. Both sensors are readable within a single 10 ms control cycle.
- **Status**: draft

The system shall sense the operator pedal position using two independent AS5048A magnetic angle sensors connected to the CVC via SPI1 with separate chip-select lines (PA4 and PA15). Both sensor readings shall be acquired in every control cycle and made available to the pedal plausibility monitoring function.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct — undetected pedal position error directly causes unintended acceleration (S3/E4/C3 from HARA). Dual independent sensors with separate chip-selects on one SPI bus is a sound 1oo2D architecture at the sensing level. Verification criteria (1% linearity, 10 ms readout) are concrete and testable on bench. **Why:** Drive-by-wire pedal sensing is the primary operator input to the torque path — any single-point failure here violates the safety goal. **Tradeoff:** Dual sensors on the same SPI bus share a single bus fault domain; true independence would require separate SPI peripherals, but separate CS lines plus E2E on SPI frames provide adequate diagnostic coverage for this scale. **Alternative:** Two separate SPI buses (SPI1 + SPI2) would eliminate the shared-bus coupling factor, but consumes a second peripheral and the AS5048A CRC already covers data integrity on the wire.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS001 -->

---

### SYS-002: Pedal Sensor Plausibility Monitoring

- **Traces up**: STK-005, STK-016, STK-018
- **Traces down**: TSR-003, TSR-004, SWR-CVC-003, SWR-CVC-004, SWR-CVC-005, SWR-CVC-006
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: A simulated sensor disagreement of 6% of full scale (above the 5% threshold) persisting for 2 control cycles triggers a plausibility fault within 20 ms. A disagreement of 4% (below threshold) does not trigger a fault.
- **Status**: draft

The system shall compare the two pedal sensor readings every control cycle and detect a plausibility fault when the absolute difference exceeds a calibratable threshold (default: 5% of full-scale range) for 2 or more consecutive cycles. On detection, the system shall set the torque request to zero and latch the fault until manual reset.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct — this is the primary safety mechanism against pedal sensor single-point faults feeding into the torque path. The 5% threshold with 2-cycle debounce is a sensible engineering default that filters noise without delaying fault detection beyond FTTI. Verification criteria (6% triggers, 4% does not) are directly testable boundary conditions. Relates to SYS-001 (sensor input) and feeds SYS-003 (torque mapping). **Why:** Without plausibility monitoring, a drifting or stuck sensor could command unintended torque — this is the diagnostic coverage mechanism that brings SYS-001's dual-sensor architecture to ASIL D SPFM compliance. **Tradeoff:** Latching fault until manual reset is conservative (safe) but reduces availability; auto-recovery after sensor re-agreement would improve uptime but weakens the safety argument. **Alternative:** Triple-sensor voting (2oo3) would allow continued operation after one sensor fault, but adds cost and complexity disproportionate to this platform's risk profile.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS002 -->

---

### SYS-003: Pedal-to-Torque Mapping

- **Traces up**: STK-005
- **Traces down**: SWR-CVC-001, SWR-CVC-007
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: For a validated pedal input of 0%, the torque request is 0%. For a validated pedal input of 100%, the torque request equals the maximum permitted by the current operating mode. The mapping is monotonically increasing.
- **Status**: draft

The system shall convert a validated pedal position (average of both sensor readings when plausibility check passes) into a motor torque request using a calibratable mapping function. The mapping shall apply ramp-rate limiting (maximum increase rate: 50% per second) to prevent sudden torque transients. The maximum torque request shall be constrained by the current vehicle operating mode (RUN: 100%, DEGRADED: 75%, LIMP: 30%).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct — the mapping function sits directly in the torque command path. The 50%/s ramp rate limit and mode-dependent torque caps (100/75/30%) are well-specified, testable, and provide defence-in-depth against sudden torque transients. Verification criteria (0% in = 0% out, 100% in = mode max, monotonic) are concrete. **Why:** Even with valid pedal input, an unconstrained or non-monotonic mapping could produce hazardous torque behaviour — ramp limiting and mode capping are essential safety mechanisms. **Tradeoff:** The 50%/s ramp rate may feel sluggish under aggressive driving; a higher rate would improve responsiveness but increases the severity of a mapping fault. Current value is conservative and appropriate for a development platform. **Alternative:** A lookup table with interpolation (instead of a function) would be simpler to verify formally but less flexible for calibration tuning.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS003 -->

---

### SYS-004: Motor Torque Control via CAN

- **Traces up**: STK-005, STK-022
- **Traces down**: TSR-005, SWR-CVC-008, SWR-CVC-016, SWR-CVC-017, SWR-RZC-001, SWR-RZC-002, SWR-RZC-003, SWR-RZC-016, SWR-RZC-026
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: A torque request CAN message transmitted by CVC at 10 ms intervals results in a corresponding PWM duty cycle change at the RZC BTS7960 motor driver within 20 ms of transmission. The duty cycle is proportional to the requested torque within 2% accuracy.
- **Status**: draft

The system shall transmit the torque request from the CVC to the RZC via CAN at a fixed cycle time of 10 ms. The RZC shall translate the received torque request into PWM duty cycle for the BTS7960 motor driver and control the motor direction via RPWM/LPWM signals. The torque CAN message shall include E2E protection (CRC-8, alive counter, data ID).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct — this is the primary torque actuation path across the CAN bus, and corruption/loss of these messages directly impacts vehicle controllability. E2E protection (CRC-8, alive counter, data ID) covers AUTOSAR E2E Profile requirements for communication faults. Verification criteria (10 ms cycle, 20 ms end-to-end latency, 2% duty cycle accuracy) are measurable on target. **Why:** CAN is the only communication channel between CVC (pedal intent) and RZC (motor actuation) — without E2E protection, message corruption, loss, or delay could cause unintended torque. **Tradeoff:** 10 ms cycle time at ASIL D drives tight timing requirements on both CVC TX and RZC RX tasks; a longer cycle would ease WCET but worsen FTTI. **Alternative:** Direct SPI or UART link between CVC and RZC would eliminate CAN arbitration delay but would sacrifice the bus-based architecture that enables SC monitoring (SYS-023).
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS004 -->

---

### SYS-005: Motor Current Monitoring

- **Traces up**: STK-005, STK-016
- **Traces down**: TSR-006, TSR-007, SWR-RZC-005, SWR-RZC-006, SWR-RZC-007, SWR-RZC-008, SWR-RZC-027
- **Safety relevance**: ASIL A
- **Verification method**: Test
- **Verification criteria**: An applied motor current of 26A (above the 25A threshold) sustained for 10 ms results in motor driver disable within 20 ms. The ACS723 current reading accuracy is within 2A of the actual current over the 0-30A range.
- **Status**: draft

The system shall continuously monitor motor current via the ACS723 current sensor (ADC1_CH1 on RZC, sampled at 1 kHz minimum). The system shall disable the motor driver (BTS7960 R_EN and L_EN set LOW) within 10 ms of detecting that motor current exceeds the maximum rated threshold (calibratable, default: 25A) for a continuous debounce period of 10 ms.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL A is appropriate — motor overcurrent is a hardware protection concern (smoke/fire prevention) rather than a primary vehicle controllability hazard, and the BTS7960 has its own built-in overcurrent shutdown as a hardware safety mechanism. The ACS723 at 1 kHz sampling with 25A threshold and 10 ms debounce provides adequate software-level monitoring. Verification criteria (26A triggers, 2A accuracy) are testable on bench with a current source. **Why:** Current monitoring supplements the BTS7960's hardware protection and feeds the SC cross-plausibility check (SYS-023); ASIL A is sufficient because the hardware safety mechanism is the primary barrier. **Tradeoff:** Single current sensor means no sensor-level redundancy; a second sensor would improve diagnostic coverage but ASIL A does not demand it. **Alternative:** Hall-effect current sensing on the high-side (instead of ACS723 on low-side) would catch more fault modes but adds cost and PCB complexity.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS005 -->

---

### SYS-006: Motor Temperature Monitoring and Derating

- **Traces up**: STK-005, STK-016
- **Traces down**: TSR-008, TSR-009, SWR-ICU-004, SWR-RZC-009, SWR-RZC-010, SWR-RZC-011
- **Safety relevance**: ASIL A
- **Verification method**: Test
- **Verification criteria**: Simulated NTC readings corresponding to 60C, 80C, and 100C thresholds trigger derating to 75%, 50%, and 0% (motor off) respectively within 200 ms. Recovery occurs at 50C, 70C, and 90C respectively (10C hysteresis verified).
- **Status**: draft

The system shall continuously monitor motor temperature via NTC thermistors (ADC1_CH2 and ADC1_CH3 on RZC) and apply a progressive current derating curve: below 60C full current, 60-79C maximum 75% rated current, 80-99C maximum 50% rated current, at or above 100C motor disabled. Recovery shall include 10C hysteresis. Sensor readings outside -30C to 150C shall be treated as sensor fault (motor disabled).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL A is appropriate — thermal protection prevents hardware damage (motor/winding burnout) rather than addressing a direct vehicle controllability hazard; the BTS7960 has its own thermal shutdown as hardware backup. Progressive derating with 10C hysteresis is a standard industrial pattern that prevents oscillation. Out-of-range plausibility (-30C to 150C) with fail-safe default (motor disabled) is well-specified. **Why:** Thermal runaway damages hardware but the torque path safety mechanisms (SYS-002, SYS-023, SYS-024) independently handle controllability hazards. **Tradeoff:** 200 ms reaction time is slower than current monitoring (SYS-005) but thermal mass makes temperature changes inherently slow — faster reaction adds no safety benefit. **Alternative:** Bimetallic thermal cutoff directly on the motor would provide hardware-level backup independent of software, but adds BOM cost for marginal gain at ASIL A.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS006 -->

---

### SYS-007: Motor Direction Control and Plausibility

- **Traces up**: STK-005, STK-016
- **Traces down**: TSR-040, SWR-RZC-002, SWR-RZC-012, SWR-RZC-013, SWR-RZC-014, SWR-RZC-015
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: A commanded forward direction results in forward encoder count within 50 ms. Simultaneous RPWM and LPWM activation is physically prevented with at least 10 us dead-time verified by oscilloscope capture.
- **Status**: draft

The system shall verify that the motor rotation direction matches the commanded direction within 50 ms using encoder feedback. The system shall enforce a minimum dead-time of 10 microseconds between motor direction changes to prevent H-bridge shoot-through. If direction mismatch is detected, the motor driver shall be disabled and a DTC logged.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS007 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is defensible -- unintended direction reversal is hazardous but exposure is lower than the continuous torque path because direction changes are infrequent. Dead-time (10 us) prevents H-bridge shoot-through; oscilloscope verification is concrete. **Why:** Direction plausibility closes the gap between commanded and actual motor behavior; a stuck direction bit could cause reverse torque. Relates to SYS-009 (encoder input). **Tradeoff:** ASIL C rather than D is justified because SC cross-plausibility (SYS-023) independently detects torque anomalies from direction faults. **Alternative:** Hardware interlock via discrete logic would eliminate software dead-time dependency, but BTS7960 has internal shoot-through protection.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS007 -->

---

### SYS-008: Battery Voltage Monitoring

- **Traces up**: STK-005
- **Traces down**: SWR-ICU-005, SWR-RZC-003, SWR-RZC-017, SWR-RZC-018
- **Safety relevance**: QM
- **Verification method**: Test
- **Verification criteria**: Applied voltages of 9V and 16V on the battery input trigger undervoltage and overvoltage warnings respectively. A voltage below 8V or above 17V triggers motor disable.
- **Status**: draft

The system shall continuously monitor battery voltage via a resistor divider on ADC1_CH4 (RZC). The system shall report warning conditions at configurable thresholds (default: below 10V or above 15V) and shall disable the motor at critical thresholds (default: below 8V or above 17V) to protect the electronics and motor driver.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS008 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is appropriate -- battery voltage monitoring protects electronics from damage (undervoltage brownout, overvoltage stress) but does not directly address vehicle controllability hazards. Verification criteria (9V/16V warning, 8V/17V disable) are testable with a bench supply. **Why:** The kill relay (SYS-024) and watchdog (SYS-027) handle safety-critical power-loss scenarios; this is a hardware-protection function. **Tradeoff:** QM means no formal deviation process for threshold calibration, which is appropriate for values tuned during integration. **Alternative:** Elevating to ASIL A was considered if voltage monitoring were part of the safety concept for brownout-induced CPU corruption, but that role is filled by the TPS3823 watchdog.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS008 -->

---

### SYS-009: Encoder Feedback for Speed Measurement

- **Traces up**: STK-005, STK-014
- **Traces down**: SWR-ICU-002, SWR-RZC-004, SWR-RZC-012
- **Safety relevance**: QM
- **Verification method**: Test
- **Verification criteria**: Encoder pulses at a known motor speed produce a speed calculation within 5% of the actual motor speed.
- **Status**: draft

The system shall read motor encoder feedback on the RZC via GPIO interrupt to measure motor speed and direction. The encoder data shall be used for speed calculation, direction verification (SYS-007), and reporting to the CVC and telemetry systems via CAN.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS009 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is appropriate for speed measurement -- it is informational. However, encoder data also feeds SYS-007 (direction verification, ASIL C); the encoder hardware path will need to meet ASIL C when traced through SYS-007. The 5% accuracy criterion is testable with a reference tachometer. **Why:** Speed for telemetry/display is QM; direction verification has its own requirement (SYS-007) at the correct ASIL. **Tradeoff:** Keeping QM avoids safety process overhead on speed calculation while the underlying encoder hardware is qualified via SYS-007. **Alternative:** Merging speed and direction into a single ASIL C requirement was considered but would unnecessarily elevate the speed calculation algorithm.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS009 -->

---

## 5. Functional System Requirements — Steering

### SYS-010: Steering Command Reception and Servo Control

- **Traces up**: STK-006
- **Traces down**: TSR-012, SWR-FZC-001, SWR-FZC-008, SWR-FZC-026, SWR-FZC-028, SWR-FZC-032
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: A steering command CAN message specifying +20 degrees results in the steering servo reaching 20 +/- 1 degrees within 200 ms as measured by the AS5048A feedback sensor.
- **Status**: draft

The system shall receive steering angle commands from the CVC via CAN (10 ms cycle, E2E protected) and drive the steering servo to the commanded angle using PWM (TIM2_CH1 on FZC, 50 Hz servo frequency). The system shall implement closed-loop position control using the steering angle feedback sensor.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS010 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- unintended steering angle directly causes loss of vehicle controllability (S3/E4/C3). Closed-loop position control with AS5048A feedback is the right architecture for servo-based steer-by-wire. Verification criteria (+/- 1 degree, 200 ms settling) are measurable on a steering test rig. **Why:** The steering actuation path directly controls vehicle direction; like the torque path (SYS-004), it requires ASIL D. **Tradeoff:** 200 ms settling is generous for a servo but appropriate for a low-speed platform; tightening would stress the 50 Hz PWM cycle. **Alternative:** Stepper motor with position feedback would offer finer resolution but adds complexity for a hobby-servo-scale platform.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS010 -->

---

### SYS-011: Steering Angle Feedback Monitoring

- **Traces up**: STK-006, STK-016, STK-018
- **Traces down**: TSR-010, TSR-011, SWR-FZC-001, SWR-FZC-002, SWR-FZC-003
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: A simulated command-vs-feedback deviation of 6 degrees (above 5 degree threshold) persisting for 50 ms triggers a steering fault. A deviation of 4 degrees does not trigger a fault.
- **Status**: draft

The system shall continuously monitor the steering angle sensor (AS5048A on SPI2, FZC) and detect a steering fault when: (a) the command-vs-feedback difference exceeds 5 degrees for 50 ms, (b) the angle is outside -45 to +45 degrees, (c) the rate of change exceeds 360 degrees/second, or (d) SPI communication fails (CRC error, timeout, no response).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS011 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- this is the steering equivalent of pedal plausibility (SYS-002): the primary diagnostic coverage for the steering actuation path. Four independent fault detection criteria (deviation, range, rate, SPI failure) provide comprehensive coverage. Verification criteria (6 deg triggers, 4 deg does not) are clean boundary tests. **Why:** Without feedback monitoring, a jammed or disconnected servo would go undetected, leaving the vehicle in an uncontrollable steering state. **Tradeoff:** Single feedback sensor means a sensor fault forces return-to-center (SYS-012) rather than continued operation; dual sensors would improve availability but the servo form factor makes this impractical. **Alternative:** Dual AS5048A on the steering column would mirror the pedal architecture but physical constraints on the servo linkage make this infeasible.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS011 -->

---

### SYS-012: Steering Return-to-Center on Fault

- **Traces up**: STK-006, STK-017
- **Traces down**: TSR-012, TSR-013, SWR-FZC-004, SWR-FZC-005, SWR-FZC-006
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: On steering fault injection, the steering servo reaches 0 degrees (center) within 100 ms at a controlled rate. If center is not reached within 200 ms, PWM output is disabled (verified by oscilloscope).
- **Status**: draft

The system shall command the steering servo to the center position (0 degrees) at a controlled rate not exceeding the rate limit upon detection of a steering fault. If the servo does not reach center within 200 ms, the system shall disable the steering servo PWM output entirely.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS012 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- this is the defined safe state for the steering subsystem. The two-stage approach (controlled return-to-center, then PWM disable) provides graceful degradation with a hard fallback. The 200 ms timeout with oscilloscope verification is concrete and testable. **Why:** Straight-ahead is the only safe steering angle; any other default creates a turning hazard. Rate-limited return prevents a sudden steering jerk during fault recovery. **Tradeoff:** Disabling PWM entirely if center is not reached leaves steering unassisted but mechanically centered by spring return; acceptable for a low-speed platform. **Alternative:** Maintaining PWM at center indefinitely was considered but risks actuating a faulty servo; complete disable is the safer fallback.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS012 -->

---

### SYS-013: Steering Rate and Angle Limiting

- **Traces up**: STK-006, STK-016
- **Traces down**: TSR-014, SWR-FZC-007
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: A step command from 0 to 45 degrees is rate-limited to arrive in no less than 1.5 seconds (30 deg/s). A command of 50 degrees is clamped to 43 degrees (45 minus 2 degree margin).
- **Status**: draft

The system shall limit the steering angle command rate of change to a maximum of 30 degrees per second and enforce software angle limits of -45 to +45 degrees with a 2-degree margin inside mechanical stops. Any command exceeding these limits shall be clamped.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS013 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is defensible -- rate and angle limiting is a defence-in-depth mechanism supplementing the primary steering control (SYS-010, ASIL D) and feedback monitoring (SYS-011, ASIL D). The 2-degree margin inside mechanical stops protects against servo overtravel. Verification criteria (1.5s for 45 deg, clamping at 43 deg) are directly testable. **Why:** Even if the CVC commands an erratic steering profile, the FZC-side rate limiter prevents sudden transients that could cause loss of control at speed. **Tradeoff:** 30 deg/s rate limit may feel slow; the value is calibratable and appropriate for the platform's low-speed envelope. **Alternative:** Speed-dependent rate limiting would improve agility but requires a reliable speed signal from the RZC, adding a cross-ECU dependency.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS013 -->

---

## 6. Functional System Requirements — Braking

### SYS-014: Brake Command Reception and Servo Control

- **Traces up**: STK-007
- **Traces down**: TSR-015, SWR-FZC-002, SWR-FZC-009, SWR-FZC-026, SWR-FZC-032
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: A brake command of 80% force received via CAN results in the brake servo applying proportional PWM within 20 ms.
- **Status**: draft

The system shall receive brake commands from the CVC via CAN (10 ms cycle, E2E protected) and drive the brake servo to apply proportional braking force using PWM (TIM2_CH2 on FZC, 50 Hz servo frequency). Brake commands shall be range-validated (0% to 100%).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS014 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- loss of braking capability directly violates the safety goal for collision avoidance (S3/E4/C3). E2E-protected CAN with range validation and 20 ms response are well-specified. **Why:** Braking is the primary deceleration mechanism with no mechanical backup, making this path fully safety-critical. **Tradeoff:** Open-loop PWM-to-force mapping (no force feedback sensor) means actual force is assumed proportional to PWM; adding a force sensor would close the loop but is impractical on a servo-actuated brake. **Alternative:** Hydraulic brake-by-wire would provide better force control but is disproportionate for a development platform.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS014 -->

---

### SYS-015: Brake System Monitoring

- **Traces up**: STK-007, STK-016, STK-018
- **Traces down**: TSR-015, TSR-016, SWR-FZC-010, SWR-FZC-012, SWR-FZC-027
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: A simulated brake servo disconnection (no current draw when brake force commanded) triggers a brake fault within 20 ms. The CVC is notified via CAN and motor torque cutoff is commanded within 30 ms total.
- **Status**: draft

The system shall monitor the brake command path and detect a brake system fault when: (a) PWM output does not match the commanded value, (b) the brake servo draws no current when a non-zero force is commanded, (c) a brake command is received in INIT or OFF mode, or (d) the brake command value is outside 0-100%. On brake fault, the FZC shall notify the CVC and request motor torque cutoff.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS015 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- brake monitoring is the diagnostic coverage mechanism for the braking path, analogous to pedal plausibility (SYS-002) and steering monitoring (SYS-011). Four fault detection criteria provide comprehensive coverage; 20 ms detection + 10 ms CAN = 30 ms total is within FTTI. **Why:** Without monitoring, a disconnected or stuck servo would leave the vehicle unable to decelerate. Motor torque cutoff on brake fault is the correct fail-safe. **Tradeoff:** Current-draw monitoring for servo disconnection requires a shunt resistor on the servo power line, adding minor HW complexity. **Alternative:** Brake force feedback sensor would be more direct than current-draw inference but is unavailable on standard hobby servos.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS015 -->

---

### SYS-016: Auto-Brake on CAN Communication Loss

- **Traces up**: STK-007, STK-017
- **Traces down**: TSR-017, SWR-FZC-011
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: When CAN brake command messages are blocked for 100 ms, the FZC autonomously applies maximum braking force within 110 ms (100 ms timeout + 10 ms actuation). Brake remains applied until valid CAN communication is restored.
- **Status**: draft

The system shall autonomously apply maximum braking force if no valid brake command CAN message (passing E2E verification) is received from the CVC within 100 ms. The auto-brake shall remain applied until valid CAN communication is restored and the CVC explicitly commands brake release.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS016 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- autonomous braking on CAN loss ensures the vehicle stops even if the CVC is completely unresponsive. The 100 ms timeout balances false-positive avoidance against stopping distance. Requiring explicit CVC brake-release after recovery prevents accidental re-acceleration. **Why:** Without this, a CVC crash or CAN failure would leave the vehicle coasting uncontrolled; this is the FZC's independent safe-state action. **Tradeoff:** Maximum braking on CAN loss could be jarring; graduated braking would be gentler but adds complexity and delays the stop. Hard stop is the conservative choice. **Alternative:** Speed-dependent graduated braking was considered but requires speed data from the RZC, creating a cross-ECU dependency that defeats autonomous FZC action.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS016 -->

---

### SYS-017: Emergency Braking from Obstacle Detection

- **Traces up**: STK-007, STK-008
- **Traces down**: TSR-018, SWR-FZC-014
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: An obstacle placed at 19 cm (inside 20 cm emergency zone) triggers maximum braking within 30 ms of sensor reading. Motor cutoff is requested via CAN simultaneously.
- **Status**: draft

The system shall trigger emergency braking when the lidar sensor reports an obstacle within the emergency distance threshold (calibratable, default: 20 cm). Emergency braking shall apply maximum brake servo force and simultaneously request motor torque cutoff from the CVC via CAN. Emergency braking shall override any other brake command.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS017 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is appropriate -- emergency braking from obstacle detection is an additional safety layer beyond operator-commanded braking (SYS-014, ASIL D) and auto-brake on CAN loss (SYS-016, ASIL D). The TFMini-S is a consumer-grade sensor, so ASIL C is the ceiling without redundancy. **Why:** The 20 cm threshold with 30 ms response is achievable given the 100 Hz lidar update rate. **Tradeoff:** Single forward-facing lidar provides no lateral or rear coverage; multi-sensor fusion would improve safety but exceeds platform scope. **Alternative:** Ultrasonic backup sensor would provide diverse redundancy, but TFMini-S plausibility checking (SYS-020) provides adequate fault detection at ASIL C.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS017 -->

---

## 7. Functional System Requirements — Obstacle Detection

### SYS-018: Lidar Distance Sensing

- **Traces up**: STK-008
- **Traces down**: TSR-018, TSR-019, SWR-FZC-013
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: The TFMini-S reports distance to a target at 1 m within +/- 3 cm accuracy. Update rate is 100 Hz (+/- 5 Hz). Data is available to the application within 10 ms of sensor output.
- **Status**: draft

The system shall continuously read forward distance from the TFMini-S lidar sensor via UART (USART1 on FZC, 115200 baud, 100 Hz native update rate). The system shall parse the TFMini-S serial protocol and extract distance (cm) and signal strength values from each frame.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS018 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is correct -- the TFMini-S is a consumer-grade sensor; ASIL C is the maximum justifiable without automotive-qualified hardware or sensor redundancy. Verification criteria (+/- 3 cm at 1 m, 100 Hz, 10 ms latency) are datasheet-derived and bench-testable. **Why:** UART at 115200 baud with DMA is the standard TFMini-S interface; the 9-byte frame protocol with checksum provides basic integrity. **Tradeoff:** Single sensor with no redundancy means a failure defaults to "obstacle present" (SYS-020) -- safe but reduces availability. **Alternative:** Dual TFMini-S at diverse mounting angles would provide sensor-level redundancy but doubles BOM cost.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS018 -->

---

### SYS-019: Graduated Obstacle Response

- **Traces up**: STK-008
- **Traces down**: TSR-018, TSR-019, SWR-FZC-004, SWR-FZC-014
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: Obstacles at 100 cm, 50 cm, and 20 cm trigger warning (buzzer + CAN alert), partial braking (speed reduction request), and emergency braking (full brake + motor cutoff) respectively. All thresholds are configurable at compile time.
- **Status**: draft

The system shall implement a graduated response based on three configurable distance thresholds: warning zone (default 100 cm, buzzer and CAN alert), braking zone (default 50 cm, speed reduction request and partial braking), and emergency zone (default 20 cm, full emergency braking and motor cutoff request). The thresholds shall maintain the ordering: emergency < braking < warning.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS019 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is correct -- inherits the sensor's ASIL ceiling (SYS-018). The three-zone approach with compile-time configurability and enforced ordering (emergency < braking < warning) is well-structured and prevents misconfiguration. Verification criteria at each threshold are concrete and testable. **Why:** Graduated response provides operator awareness before emergency action, reducing nuisance stops while maintaining safety. **Tradeoff:** Compile-time configurability means threshold changes require firmware rebuild; runtime calibration via UDS would be more flexible but adds safety analysis burden. **Alternative:** Continuous proportional braking (linear function of distance) would be smoother but harder to specify, test, and verify than discrete zones.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS019 -->

---

### SYS-020: Lidar Sensor Plausibility Checking

- **Traces up**: STK-008, STK-016
- **Traces down**: TSR-020, TSR-021, SWR-FZC-015, SWR-FZC-016
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: A stuck sensor (no change > 1 cm over 500 ms) triggers a fault. A UART timeout of 100 ms triggers a communication fault. On any fault, the system substitutes 0 cm (obstacle present) and logs a DTC.
- **Status**: draft

The system shall perform plausibility checks on every lidar reading: range check (2-1200 cm valid), stuck sensor detection (less than 1 cm change over 50 consecutive samples), signal strength check (minimum threshold, default: 100), and UART timeout check (100 ms). On any plausibility failure, the system shall substitute a safe default distance of 0 cm (obstacle present) and log a DTC.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS020 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is correct -- this is the diagnostic coverage mechanism for the lidar path, analogous to pedal plausibility (SYS-002). Four independent checks (range, stuck, signal strength, timeout) provide comprehensive fault detection. Safe default of 0 cm (obstacle present) is correct: triggers braking rather than ignoring a potentially real obstacle. **Why:** Without plausibility checking, a stuck sensor could report false-clear (dangerous); the safe default ensures the dangerous case is covered. **Tradeoff:** 0 cm safe default means any sensor fault triggers emergency braking, reducing availability; last-valid-reading substitution would improve uptime but weakens safety. **Alternative:** Dual-sensor cross-check would allow continued operation on single sensor fault, but the single-sensor architecture makes fail-safe the only viable strategy.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS020 -->

---

## 8. Functional System Requirements — Safety Monitoring

### SYS-021: Heartbeat Transmission by Zone ECUs

- **Traces up**: STK-009, STK-019
- **Traces down**: TSR-025, TSR-026, SWR-CVC-020, SWR-CVC-021, SWR-FZC-021, SWR-FZC-022, SWR-ICU-009, SWR-RZC-021, SWR-RZC-022
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: Each zone ECU (CVC, FZC, RZC) transmits a heartbeat CAN message at 50 ms +/- 5 ms intervals. The message contains ECU ID, alive counter (incrementing by 1 per transmission), operating mode, and CRC-8.
- **Status**: draft

Each zone ECU (CVC, FZC, RZC) shall transmit a heartbeat CAN message at a fixed interval of 50 ms (tolerance: +/- 5 ms). The heartbeat message shall include the ECU identifier, an alive counter, the current operating mode, and CRC-8 E2E protection.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS021 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is appropriate -- heartbeat transmission enables the SC's timeout detection (SYS-022); the safety action (kill relay) is ASIL D via SYS-024, so the monitoring input path can be ASIL C. The 50 ms interval with +/- 5 ms tolerance and E2E (alive counter + CRC-8) are well-specified and testable with CAN bus analyzer. **Why:** Heartbeats are the only mechanism for the listen-only SC to detect zone ECU liveness. **Tradeoff:** 50 ms interval adds 3 messages per ECU per second (negligible at 500 kbps); longer interval would reduce load but worsen SC detection latency. **Alternative:** Hardware-based watchdog signals between ECUs would be faster but require physical wiring between every ECU pair, defeating the bus-based architecture.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS021 -->

---

### SYS-022: Heartbeat Timeout Detection by Safety Controller

- **Traces up**: STK-009, STK-019
- **Traces down**: TSR-027, TSR-028, SWR-CVC-022, SWR-SC-004, SWR-SC-006
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: When a zone ECU stops transmitting heartbeats, the SC detects the timeout within 150 ms (3 missed heartbeats). After a 50 ms confirmation period, the SC de-energizes the kill relay (total: 200 ms from last heartbeat to relay open).
- **Status**: draft

The Safety Controller shall monitor heartbeat messages from each zone ECU and detect a timeout when no valid heartbeat (passing E2E check) is received within 150 ms (3 times the heartbeat interval). On timeout detection, the SC shall illuminate the fault LED for the failed ECU, wait 50 ms for confirmation, and then de-energize the kill relay if the heartbeat is still absent.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS022 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is appropriate -- the detection function is ASIL C; the kill relay action is governed by SYS-024 at ASIL D. The 3-missed-heartbeat threshold (150 ms) with 50 ms confirmation totaling 200 ms balances false-positive rejection against detection speed. **Why:** The confirmation period prevents a single missed heartbeat (e.g., CAN arbitration delay) from triggering a kill relay event. **Tradeoff:** 200 ms total is slower than direct hardware monitoring but adequate for the platform's stopping distance at max speed. **Alternative:** Immediate kill relay on first missed heartbeat would halve detection time but cause nuisance shutdowns on transient CAN congestion.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS022 -->

---

### SYS-023: Cross-Plausibility Check — Torque vs. Current

- **Traces up**: STK-009, STK-016
- **Traces down**: TSR-041, TSR-042, SWR-SC-007, SWR-SC-008, SWR-SC-009
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: A simulated 25% deviation between expected current (from torque request) and actual current, sustained for 50 ms, triggers SC kill relay de-energization within 60 ms.
- **Status**: draft

The Safety Controller shall continuously compare the torque request CAN message (from CVC) against the actual motor current CAN message (from RZC) using a torque-to-current lookup table. The SC shall detect a cross-plausibility fault when the deviation exceeds 20% (calibratable) for 50 ms, and shall de-energize the kill relay.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS023 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is appropriate -- cross-plausibility compares two different ECUs' data (CVC torque vs RZC current), providing independent cross-domain fault detection. The 20% threshold with 50 ms debounce accounts for motor transient dynamics. Verification criteria (25% deviation triggers, 60 ms response) are testable via CAN message injection. **Why:** This detects faults invisible to any single ECU (e.g., CVC commands zero but motor draws full current due to RZC fault). **Tradeoff:** Torque-to-current lookup table requires hardware calibration; inaccurate calibration could cause false positives. The 20% threshold provides margin. **Alternative:** Model-based observer on the SC would be more accurate but violates the SC's minimal-complexity design principle for ASIL D lockstep integrity.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS023 -->

---

### SYS-024: Kill Relay Control — Energize-to-Run

- **Traces up**: STK-009, STK-019
- **Traces down**: TSR-029, TSR-030, SWR-SC-006, SWR-SC-010, SWR-SC-011, SWR-SC-012, SWR-SC-019, SWR-SC-026
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: De-asserting GIO_A0 on the SC results in relay opening (12V power removed from actuators) within 10 ms. The relay is not re-energized without a full power cycle and startup self-test pass.
- **Status**: draft

The Safety Controller shall control the kill relay via GIO_A0 GPIO driving a MOSFET gate. The relay shall use an energize-to-run configuration: the SC must actively hold the relay energized to allow power flow. The SC shall de-energize the relay within 5 ms of any confirmed safety violation (heartbeat timeout, cross-plausibility fault, self-test failure, lockstep error, external watchdog timeout). Re-energization shall require a complete system power cycle and successful startup self-test.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS024 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is mandatory -- the kill relay is the ultimate safety mechanism that physically removes power from all actuators. Energize-to-run is the correct fail-safe architecture: any SC failure automatically opens the relay. The 5 ms de-energization and power-cycle-required re-energization are the strongest possible safety guarantees. **Why:** This is the architectural last line of defense; if all software safety mechanisms fail, the relay still provides hardware-enforced safe state. Power cycle + self-test prevents automatic recovery from masking persistent faults. **Tradeoff:** Power-cycle-required re-energization means any SC fault needs human intervention; auto-recovery would improve uptime but weakens the safety argument. **Alternative:** Dual-relay with diverse actuation would add relay-level redundancy, but energize-to-run already ensures fail-safe on relay coil failure.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS024 -->

---

### SYS-025: Safety Controller CAN Listen-Only Mode

- **Traces up**: STK-009, STK-004
- **Traces down**: SWR-SC-001, SWR-SC-002, SWR-SC-026
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: With the SC connected to the CAN bus, no frames are transmitted by the SC. Bus traffic analysis confirms zero TX frames from the SC CAN ID range.
- **Status**: draft

The Safety Controller shall operate in CAN listen-only mode (DCAN TEST register bit 3 set). The SC shall receive all CAN messages for monitoring purposes but shall not transmit any CAN messages. This ensures the SC cannot corrupt the CAN bus or interfere with zone ECU communication.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS025 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is appropriate -- listen-only mode is a freedom-from-interference measure ensuring the SC cannot corrupt the CAN bus carrying ASIL D control messages. The DCAN TEST register bit 3 is hardware-enforced on the TMS570, not software-only. Verification (zero TX frames) is testable with a CAN analyzer. **Why:** If the SC could transmit, a firmware bug could inject spurious messages confusing zone ECUs -- listen-only eliminates this entire fault class. **Tradeoff:** Listen-only means the SC cannot report its status via CAN; the fault LED panel (SYS-046) provides alternative operator feedback. **Alternative:** Allowing SC to transmit on a low-priority ID would provide richer diagnostics but introduces the risk of bus corruption from SC faults.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS025 -->

---

### SYS-026: Safety Controller Lockstep CPU Monitoring

- **Traces up**: STK-004, STK-009
- **Traces down**: SWR-SC-002, SWR-SC-014, SWR-SC-015, SWR-SC-016, SWR-SC-019, SWR-SC-026
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: A fault injected into one CPU core via the SC debug interface triggers a lockstep comparison error and ESM reset within 1 clock cycle. The SC does not re-energize the kill relay after a lockstep fault.
- **Status**: draft

The Safety Controller shall rely on the TMS570LC43x hardware lockstep CPU cores as the primary computation error detection mechanism. A lockstep comparison error shall trigger an immediate CPU reset via the Error Signaling Module (ESM). The SC firmware shall not re-energize the kill relay after a lockstep reset without a full self-test pass.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS026 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is mandatory -- TMS570LC43x lockstep is the hardware mechanism providing ASIL D diagnostic coverage for computation errors on the SC itself. ESM-triggered reset within 1 clock cycle is the fastest possible fault reaction; "no re-energize without self-test" ensures transient faults are not masked. **Why:** Without lockstep, a single-bit CPU error could cause the SC to incorrectly keep the relay energized during a real fault. The TMS570 was selected specifically for this capability. **Tradeoff:** Lockstep halves effective compute throughput, but the SC's monitoring workload is minimal so this is not a constraint. **Alternative:** Software-based diverse redundancy (N-version programming) on a non-lockstep MCU could achieve similar coverage but with significantly higher complexity and no hardware-guaranteed detection latency.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS026 -->

---

### SYS-027: External Watchdog Monitoring (All Physical ECUs)

- **Traces up**: STK-009, STK-019
- **Traces down**: TSR-031, TSR-032, SWR-BSW-021, SWR-CVC-023, SWR-CVC-029, SWR-FZC-023, SWR-FZC-025, SWR-RZC-023, SWR-RZC-025, SWR-SC-019, SWR-SC-022, SWR-SC-026
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: When ECU firmware is halted (debugger pause), the TPS3823 asserts RESET within 1.6 seconds (+/- 10%). The MCU restarts and performs a self-test sequence.
- **Status**: draft

Each physical ECU (CVC, FZC, RZC, SC) shall be monitored by an external TPS3823 watchdog IC. The firmware shall toggle the WDI pin only when the main control loop has completed a full cycle and all critical runtime self-checks have passed. Failure to toggle within the watchdog timeout period (default: 1.6 seconds) shall result in a hardware reset of the MCU.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS027 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- the external TPS3823 is the hardware-independent last resort against firmware hangs on every physical ECU. WDI-toggle-only-on-successful-cycle ensures the watchdog cannot be fed by a partially functional loop. The 1.6s timeout is testable by halting firmware with a debugger. **Why:** An internal watchdog can be defeated by the same fault causing the hang; the TPS3823 has an independent oscillator, providing true hardware independence per ISO 26262 Part 5. **Tradeoff:** 1.6s is slow vs the 10 ms control cycle, but is a TPS3823 hardware constraint; the heartbeat/SC path (SYS-021/022/024) provides faster software-level detection. **Alternative:** Window watchdog IC (e.g., MAX6381) would additionally catch runaway-fast loops, but the TPS3823 is adequate given that the heartbeat mechanism covers the fast-detection role.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS027 -->

---

## 9. Functional System Requirements — Emergency Stop

### SYS-028: E-Stop Detection and Broadcast

- **Traces up**: STK-010, STK-016
- **Traces down**: TSR-033, TSR-034, SWR-CVC-018, SWR-CVC-019
- **Safety relevance**: ASIL B
- **Verification method**: Test
- **Verification criteria**: E-stop button press is detected within 1 ms (GPIO interrupt latency). E-stop CAN message (ID 0x001) is transmitted within 2 ms of detection. All ECUs reach safe state within 12 ms of E-stop press.
- **Status**: draft

The system shall detect E-stop button activation within 1 ms via hardware interrupt on the CVC (PC13, falling edge, hardware debounce). On detection, the CVC shall immediately set the local torque request to zero and broadcast a high-priority E-stop CAN message (ID 0x001) within 1 ms. All receiving ECUs shall react within 10 ms: RZC disables motor, FZC applies full brake and centers steering. The system shall remain in SAFE_STOP until E-stop is released and a manual restart is performed.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS028-ASIL -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL B is defensible. E-stop is an operator-initiated convenience path to safe state, not the primary safety enforcement. The safety chain that meets HARA-derived safety goals is: (1) zone ECU fault detection at ASIL D (SYS-002, SYS-011, SYS-015), (2) SC cross-plausibility + kill relay at ASIL D (SYS-023, SYS-024), (3) TPS3823 hardware watchdog at ASIL D (SYS-027). All three layers achieve safe state automatically without operator intervention. E-stop provides additional controllability (ISO 26262 Part 3 Table 4 C-factor reduction) but is not relied upon by any safety goal. The implementation is simple (GPIO interrupt → CAN broadcast) — upgrading to ASIL D would impose MC/DC, independent verification, and formal methods on trivial logic with no proportional safety benefit. **Audit preparation:** if an assessor argues E-stop is the only operator-initiated safe-state path, conceding to ASIL C is acceptable. ASIL B is the floor, ASIL C is the fallback, ASIL D is disproportionate. **Why not higher:** E-stop does not appear as a required safety mechanism in any safety goal's FTTI chain — the automatic paths (Layers 1-3) satisfy all safety goals independently.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS028-ASIL -->

---

## 10. Functional System Requirements — Vehicle State Management

### SYS-029: Vehicle State Machine

- **Traces up**: STK-005, STK-006, STK-007, STK-016, STK-017
- **Traces down**: TSR-035, TSR-036, TSR-037, SWR-BCM-002, SWR-BSW-022, SWR-BSW-026, SWR-CVC-009, SWR-CVC-010, SWR-CVC-011, SWR-CVC-012, SWR-CVC-013, SWR-CVC-029, SWR-FZC-025, SWR-ICU-007, SWR-RZC-025
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: Only valid transitions as defined in the transition table are accepted. An invalid transition attempt (e.g., LIMP to RUN) is rejected and logged. E-stop forces transition from any state to SAFE_STOP. State is persisted in flash and restored on power-up.
- **Status**: draft

The CVC shall maintain a deterministic vehicle state machine with states: INIT, RUN, DEGRADED, LIMP, SAFE_STOP, SHUTDOWN. The state machine shall enforce valid transitions only (no undefined transitions), support E-stop override from any state to SAFE_STOP, support SC override from any state to SHUTDOWN, broadcast the current state on CAN every 10 ms with E2E protection, and persist state in non-volatile memory. Each state shall define maximum operational limits for torque, speed, steering range, and steering rate.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS029 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is justified because the state machine governs all actuator limits and is the single authority for safe-state transitions across the vehicle. Verification criteria are concrete and testable: invalid transition rejection, E-stop override from any state, flash persistence across power cycles, and CAN broadcast timing. **Why:** A deterministic state machine is the standard ISO 26262 pattern for mode management in drive-by-wire; ambiguity here would propagate to every downstream safety function (SYS-001 through SYS-017). **Tradeoff:** Centralizing state on CVC simplifies reasoning but creates a single point of failure -- mitigated by SC override (SYS-024) and per-ECU autonomous safe states (SYS-034). **Alternative:** Distributed consensus among ECUs was considered but adds complexity disproportionate to a single-vehicle platform.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS029 -->

---

### SYS-030: Coordinated Mode Management via BswM

- **Traces up**: STK-005, STK-006, STK-007, STK-003
- **Traces down**: SWR-BCM-002, SWR-BSW-001, SWR-BSW-022, SWR-BSW-026, SWR-CVC-010
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: A CVC state transition to DEGRADED results in all ECUs (FZC, RZC) acknowledging the new mode via heartbeat within 100 ms. Failure to acknowledge causes escalation to LIMP.
- **Status**: draft

The BSW Mode Manager (BswM) on each ECU shall synchronize operating modes across the system. The CVC shall broadcast the vehicle state, and each ECU's BswM shall adjust its local operational limits accordingly (RZC: current/PWM limits, FZC: servo/rate limits, BCM: hazard lights). Each ECU shall acknowledge the mode transition via its heartbeat message. If any ECU fails to acknowledge within 100 ms, the CVC shall escalate to the next degradation level.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS030 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is appropriate because BswM synchronizes operational limits across all actuator ECUs -- a mode desync (e.g., RZC at RUN limits while CVC is in LIMP) could violate safety goals. The 100 ms acknowledgment window and escalation-on-failure are testable and concrete. **Why:** AUTOSAR BswM is the industry-standard pattern for cross-ECU mode coordination; using it here aligns with the AUTOSAR-like BSW architecture already chosen (ADR in project). **Tradeoff:** Adds CAN overhead for heartbeat-embedded mode acknowledgment, but this piggybacks on the existing 50 ms heartbeat (SYS-021) so marginal cost is near zero. **Alternative:** Implicit mode sync (each ECU derives mode from its own fault status) would eliminate acknowledgment but risks split-brain scenarios.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS030 -->

---

## 11. Functional System Requirements — CAN Communication

### SYS-031: CAN Bus Configuration

- **Traces up**: STK-022
- **Traces down**: SWR-BCM-001, SWR-BSW-001, SWR-BSW-002, SWR-BSW-003, SWR-BSW-011, SWR-BSW-013, SWR-CVC-016, SWR-CVC-017, SWR-FZC-026, SWR-FZC-027, SWR-ICU-001, SWR-RZC-026, SWR-RZC-027, SWR-TCU-001
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: CAN bus operates at 500 kbps with all 7+ nodes connected. Bit error rate is below 10^-6. Bus utilization under normal operation is below 60%.
- **Status**: draft

The system shall operate a CAN 2.0B bus at 500 kbps with 11-bit standard CAN IDs. The bus shall connect all ECU nodes (CVC, FZC, RZC, SC, BCM, ICU, TCU) plus the Pi gateway. Bus termination shall be 120 ohm at each physical end. CAN transceivers shall be TJA1051T/3 for STM32 ECUs and SN65HVD230 for the TMS570.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS031 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- the CAN bus is the sole communication path for all safety-critical control messages (torque, steering, brake); a bus failure or misconfiguration could violate every drive-by-wire safety goal. Verification criteria (500 kbps, BER < 10^-6, utilization < 60%) are measurable and industry-standard. **Why:** 500 kbps CAN 2.0B is the proven automotive choice for this message count and cycle time; CAN FD would be overkill given the 8-byte payloads and 7-node topology. **Tradeoff:** Specifying different transceiver ICs (TJA1051T/3 vs SN65HVD230) adds BOM complexity but is necessary because the TMS570 requires 3.3V I/O-compatible transceivers. **Alternative:** CAN FD at 2 Mbps was considered but rejected -- no payload exceeds 8 bytes and all ECUs support classic CAN natively.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS031 -->

---

### SYS-032: E2E Protection on Safety-Critical Messages

- **Traces up**: STK-020
- **Traces down**: TSR-022, TSR-023, TSR-024, SWR-BSW-002, SWR-BSW-003, SWR-BSW-011, SWR-BSW-013, SWR-BSW-015, SWR-BSW-016, SWR-BSW-023, SWR-BSW-024, SWR-BSW-025, SWR-CVC-014, SWR-CVC-015, SWR-FZC-019, SWR-FZC-020, SWR-RZC-019, SWR-RZC-020, SWR-SC-003
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: A message with a corrupted CRC is discarded by the receiver. A message with a repeated alive counter is discarded. Three consecutive E2E failures result in safe default substitution within 30 ms.
- **Status**: draft

All safety-critical CAN messages shall include E2E protection: CRC-8 (polynomial 0x1D, SAE J1850) over payload and data ID, 4-bit alive counter (incrementing by 1 per transmission, modulo 16), and 8-bit data ID unique per message type. On E2E failure, the receiver shall use the last valid value for one additional cycle; after 3 consecutive failures, the receiver shall substitute safe defaults (zero torque, full brake, center steering).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS032 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is mandatory -- E2E protection is the primary defense against all CAN communication faults (corruption, repetition, loss, masquerade) that could lead to unintended actuation. CRC-8 with SAE J1850 polynomial, 4-bit alive counter, and data ID align with AUTOSAR E2E Profile 1, which is the accepted standard for classic CAN. **Why:** The 3-consecutive-failure threshold before safe-default substitution balances robustness (single CRC glitch does not trigger false alarm) against latency (30 ms worst-case is well within FTTI). **Tradeoff:** Using last-valid-value for one cycle after E2E failure introduces a brief stale-data window, but the alternative (immediate safe default) would cause nuisance shutdowns on transient EMC events. **Alternative:** E2E Profile 2 (CRC-32) was considered but is designed for CAN FD payloads and is unnecessary for 8-byte classic CAN frames.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS032 -->

---

### SYS-033: CAN Message Priority Assignment

- **Traces up**: STK-022
- **Traces down**: SWR-BSW-003, SWR-CVC-016, SWR-CVC-017
- **Safety relevance**: ASIL D
- **Verification method**: Analysis
- **Verification criteria**: CAN arbitration analysis confirms that the E-stop message (ID 0x001) wins arbitration against all other messages. ASIL D messages have lower CAN IDs (higher priority) than ASIL C, B, A, and QM messages.
- **Status**: draft

CAN message IDs shall be assigned by safety priority: E-stop (0x001, highest priority), ASIL D control messages (0x010-0x0FF), ASIL C monitoring messages (0x100-0x1FF), ASIL B status messages (0x200-0x2FF), QM comfort/telemetry messages (0x300-0x3FF), and UDS diagnostic messages (0x7DF, 0x7E0-0x7E6 per ISO 14229).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS033 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct because CAN ID assignment directly determines whether safety-critical messages win arbitration under bus contention -- a misordered ID scheme could delay E-stop or torque commands beyond FTTI. Verification by analysis (not test) is appropriate since this is a static design property provable by CAN scheduling analysis. **Why:** Mapping ASIL levels to CAN ID ranges (lower ID = higher priority = higher ASIL) is the standard automotive practice and ensures deterministic worst-case latency for safety messages. **Tradeoff:** Reserving large ID ranges per ASIL level wastes address space but simplifies future message additions without re-analysis. **Alternative:** Dynamic priority assignment was rejected -- it would require runtime arbitration analysis and violate determinism requirements.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS033 -->

---

### SYS-034: CAN Bus Loss Detection per ECU

- **Traces up**: STK-009, STK-017
- **Traces down**: TSR-038, TSR-039, SWR-BSW-004, SWR-BSW-005, SWR-BSW-012, SWR-CVC-022, SWR-CVC-024, SWR-CVC-025, SWR-FZC-024, SWR-FZC-028, SWR-RZC-016, SWR-RZC-024, SWR-SC-023
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: When the CAN bus is disconnected from an ECU, the ECU detects bus loss within 200 ms and autonomously transitions to its safe state (CVC: SAFE_STOP, FZC: auto-brake + steer center, RZC: motor disable, SC: kill relay open).
- **Status**: draft

Each ECU shall independently detect CAN bus loss (bus-off condition, no messages received for 200 ms, or CAN error counter exceeding warning threshold) and autonomously transition to its ECU-specific safe state without relying on commands from other ECUs.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS034 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is appropriate -- CAN bus loss is a monitoring/detection function, not a direct actuation path; the actual safe-state transitions it triggers (motor disable, auto-brake, kill relay) are governed by their own ASIL D requirements (SYS-024, SYS-016). The 200 ms detection window is consistent with the heartbeat timeout in SYS-022 (150 ms + 50 ms confirmation). **Why:** Autonomous per-ECU detection ensures no single ECU failure can prevent others from reaching safe state -- this is a fundamental freedom-from-interference argument. **Tradeoff:** 200 ms is slower than the 10 ms control cycle but fast enough for the steering/braking FTTI budget. **Alternative:** Dedicated CAN bus health monitor hardware was considered but adds cost; software-based detection using existing CAN controller error counters is sufficient at ASIL C.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS034 -->

---

## 12. Functional System Requirements — Body Control

### SYS-035: Automatic Headlight Control

- **Traces up**: STK-011
- **Traces down**: SWR-BCM-001, SWR-BCM-002, SWR-BCM-003, SWR-BCM-004, SWR-BCM-005, SWR-BCM-009, SWR-BCM-010, SWR-BCM-011, SWR-BCM-012
- **Safety relevance**: QM
- **Verification method**: Demonstration
- **Verification criteria**: BCM activates headlight output when vehicle state is RUN and speed is above 0. Headlights deactivate when vehicle state is OFF or SHUTDOWN.
- **Status**: draft

The BCM shall automatically activate headlight output when the vehicle is in RUN state and speed is above zero. Headlights shall be deactivated when the vehicle is in OFF or SHUTDOWN state. The headlight control shall be based on vehicle state CAN messages.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS035 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is appropriate -- headlights on this platform are a comfort/visibility feature, not a safety-critical function; the vehicle operates in controlled environments (test track, lab) where ambient lighting is not a hazard factor. Verification by demonstration is adequate for a simple state-driven GPIO output. **Why:** Headlight failure does not contribute to any safety goal violation identified in the HARA. **Tradeoff:** If the platform were deployed on public roads, headlights would require at least ASIL A per ECE R48; for current scope QM is correct. **Alternative:** No alternative ASIL was seriously considered given the controlled operating environment.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS035 -->

---

### SYS-036: Turn Indicator and Hazard Light Control

- **Traces up**: STK-011, STK-026
- **Traces down**: SWR-BCM-002, SWR-BCM-006, SWR-BCM-007, SWR-BCM-008, SWR-BCM-010, SWR-BCM-011
- **Safety relevance**: QM
- **Verification method**: Demonstration
- **Verification criteria**: Hazard lights activate within 100 ms of vehicle state transition to DEGRADED or higher. Turn indicators follow steering angle polarity.
- **Status**: draft

The BCM shall control turn indicators based on steering angle polarity (CAN message from CVC) and activate hazard lights when the vehicle transitions to DEGRADED, LIMP, SAFE_STOP, or SHUTDOWN states. Hazard light activation on fault states shall be automatic and shall not require operator action.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS036 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is appropriate -- hazard lights provide operator awareness of degraded states but are not relied upon by any safety goal to achieve a safe state; the automatic safe-state transitions (SYS-029, SYS-034) occur independently of light activation. The 100 ms response criterion is testable by demonstration. **Why:** Hazard lights improve situational awareness but do not reduce the ASIL of any other requirement via decomposition. **Tradeoff:** Making hazard lights automatic (no operator action) is a good design choice -- it eliminates a controllability dependency. **Alternative:** Elevating to ASIL A was considered if hazard lights were used as a required warning concept in the FSC, but they are supplementary only.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS036 -->

---

## 13. Functional System Requirements — Diagnostics

### SYS-037: UDS Diagnostic Session Control

- **Traces up**: STK-012
- **Traces down**: SWR-BSW-013, SWR-BSW-017, SWR-CVC-033, SWR-FZC-030, SWR-RZC-029, SWR-TCU-001, SWR-TCU-002, SWR-TCU-011, SWR-TCU-012, SWR-TCU-013, SWR-TCU-014, SWR-TCU-015
- **Safety relevance**: QM
- **Verification method**: Test
- **Verification criteria**: A UDS 0x10 request with sub-function 0x01 (default session) returns a positive response (0x50 0x01). Sub-function 0x02 (programming session) requires Security Access first.
- **Status**: draft

The system shall implement UDS Diagnostic Session Control (service 0x10) per ISO 14229. The TCU shall support at least default session (0x01), programming session (0x02), and extended diagnostic session (0x03). Session transitions shall enforce security access requirements for privileged sessions.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS037 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is correct -- UDS diagnostic session control is a workshop/service function that does not operate during normal driving; it cannot contribute to a safety goal violation. Security access gating for programming session prevents unauthorized calibration changes. **Why:** ISO 14229 compliance is an industry expectation for OBD/diagnostics but is not safety-critical per se. **Tradeoff:** Implementing full UDS stack adds firmware complexity, but the AUTOSAR-like Dcm module in BSW amortizes this across all ECUs. **Alternative:** Proprietary serial diagnostic protocol would be simpler but would not satisfy OEM interoperability expectations or ISO 14229 compliance.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS037 -->

---

### SYS-038: UDS Read and Clear DTC Services

- **Traces up**: STK-012, STK-013
- **Traces down**: SWR-BSW-017, SWR-CVC-033, SWR-CVC-034, SWR-FZC-030, SWR-ICU-008, SWR-RZC-029, SWR-TCU-002, SWR-TCU-003, SWR-TCU-004, SWR-TCU-010, SWR-TCU-011
- **Safety relevance**: QM
- **Verification method**: Test
- **Verification criteria**: After a pedal sensor fault, a UDS 0x19 request returns the associated DTC with freeze-frame data. A subsequent 0x14 request clears the DTC. A re-read confirms the DTC is cleared.
- **Status**: draft

The system shall implement UDS Read DTC Information (service 0x19) and Clear Diagnostic Information (service 0x14) per ISO 14229. The system shall report active, confirmed, and stored DTCs with associated freeze-frame data (timestamp, vehicle state, sensor readings at time of fault). DTC clearing shall require the fault condition to have been resolved.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS038 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is correct -- reading and clearing DTCs is a maintenance/diagnostic activity, not a runtime safety function. The verification criteria are well-specified: fault-triggered DTC with freeze-frame, clear via 0x14, and re-read confirmation form a complete round-trip test. **Why:** DTC services support post-incident analysis and field serviceability; they operate outside the safety-critical execution path. **Tradeoff:** Requiring fault resolution before DTC clearing prevents masking of active faults, which is good practice even at QM. **Alternative:** No alternative ASIL considered; the requirement to clear only resolved faults is a quality measure, not a safety measure.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS038 -->

---

### SYS-039: UDS Read/Write Data by Identifier

- **Traces up**: STK-012
- **Traces down**: SWR-BSW-017, SWR-CVC-031, SWR-CVC-033, SWR-CVC-035, SWR-FZC-030, SWR-RZC-029, SWR-TCU-004, SWR-TCU-005, SWR-TCU-006, SWR-TCU-014
- **Safety relevance**: QM
- **Verification method**: Test
- **Verification criteria**: A UDS 0x22 request for DID 0xF190 (VIN) returns the programmed VIN. A 0x2E write to a writable DID updates the value persistently.
- **Status**: draft

The system shall implement UDS Read Data By Identifier (service 0x22) and Write Data By Identifier (service 0x2E) per ISO 14229. Supported DIDs shall include vehicle identification number (0xF190), hardware version (0xF191), software version (0xF195), and calibration data identifiers. Write access shall require Security Access.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS039 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is appropriate -- reading/writing data identifiers is a diagnostic function gated by Security Access; it does not execute during normal vehicle operation. Verification criteria cover both read (VIN retrieval) and write (persistent update) paths with concrete DID values per ISO 14229 Annex F. **Why:** Write-protecting calibration DIDs behind Security Access prevents accidental or unauthorized parameter changes that could indirectly affect safety-relevant thresholds. **Tradeoff:** Supporting writable calibration DIDs enables field tuning but requires careful management of which DIDs are writable -- safety-relevant thresholds should be read-only in production. **Alternative:** Restricting all DIDs to read-only would be safer but would prevent field calibration entirely.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS039 -->

---

### SYS-040: UDS Security Access

- **Traces up**: STK-012
- **Traces down**: SWR-BSW-017, SWR-CVC-033, SWR-TCU-005, SWR-TCU-007
- **Safety relevance**: QM
- **Verification method**: Test
- **Verification criteria**: A Security Access sequence with correct seed-key pair grants access. An incorrect key is rejected with NRC 0x35. After 3 failed attempts, access is locked for 10 seconds.
- **Status**: draft

The system shall implement UDS Security Access (service 0x27) per ISO 14229 with a seed-and-key mechanism. Security Access shall be required before accessing programming session, writing calibration data, or clearing DTCs. The system shall enforce lockout after 3 consecutive failed attempts (minimum 10 second delay).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS040 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is correct -- Security Access is an anti-tampering/authorization mechanism, not a safety function. The seed-key challenge-response, NRC 0x35 rejection, and 3-attempt lockout with 10-second delay are standard ISO 14229 patterns and are concretely testable. **Why:** Lockout prevents brute-force key guessing; 10 seconds is sufficient for this platform given physical CAN access is required. **Tradeoff:** A 10-second lockout is mild compared to production vehicles (often 10 minutes or escalating) but appropriate for a development/demo platform where lockout-induced frustration outweighs the low attack surface risk. **Alternative:** Certificate-based authentication (ISO 14229-1:2020 Annex C) would be stronger but is disproportionate for a portfolio project with physical-access-only diagnostics.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS040 -->

---

### SYS-041: DTC Storage and Persistence

- **Traces up**: STK-013
- **Traces down**: SWR-BSW-004, SWR-BSW-018, SWR-BSW-019, SWR-BSW-020, SWR-CVC-030, SWR-CVC-034, SWR-FZC-031, SWR-ICU-008, SWR-RZC-030, SWR-TCU-008, SWR-TCU-009
- **Safety relevance**: QM
- **Verification method**: Test
- **Verification criteria**: A DTC stored during operation is present after power cycle. Freeze-frame data includes timestamp, vehicle state, and relevant sensor values. At least 50 DTCs can be stored simultaneously.
- **Status**: draft

The system shall store diagnostic trouble codes in non-volatile memory (flash) with freeze-frame data. DTCs shall survive power cycles. The system shall support a minimum of 50 concurrent DTCs. Each DTC shall record: DTC number (per SAE J2012), fault status byte, occurrence counter, first/last occurrence timestamp, and freeze-frame snapshot (vehicle state, speed, relevant sensor readings).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS041 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is correct -- DTC storage is a diagnostic/traceability function that supports post-incident analysis, not a runtime safety mechanism. The verification criteria are thorough: power-cycle persistence, freeze-frame content, and 50-DTC capacity are all concretely testable. **Why:** 50 concurrent DTCs provides ample headroom for a 7-ECU system where each ECU might generate 5-10 distinct fault types. SAE J2012 DTC numbering ensures interoperability with standard diagnostic tools. **Tradeoff:** Flash-based DTC storage introduces wear concerns, but 50 DTCs at typical fault rates will not approach flash endurance limits within the product lifetime. **Alternative:** External EEPROM for DTC storage was considered but adds hardware cost; internal flash with wear-leveling is standard practice for this storage volume.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS041 -->

---

## 14. Functional System Requirements — Telemetry and Cloud

### SYS-042: MQTT Telemetry to AWS IoT Core

- **Traces up**: STK-014
- **Traces down**: SWR-GW-001, SWR-GW-002
- **Safety relevance**: QM
- **Verification method**: Demonstration
- **Verification criteria**: The Pi gateway publishes a telemetry JSON message to `vehicle/telemetry` topic at 1 message per 5 seconds. Messages arrive at AWS IoT Core and are visible in Timestream/Grafana.
- **Status**: draft

The Raspberry Pi gateway shall publish vehicle telemetry to AWS IoT Core via MQTT v3.1.1 over TLS 1.2 (port 8883) using X.509 client certificate authentication. Telemetry shall be batched at 1 message per 5 seconds to stay within AWS free tier limits. Topics shall include `vehicle/telemetry`, `vehicle/dtc/new`, `vehicle/dtc/soft`, and `vehicle/alerts`.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS042 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is correct -- cloud telemetry is purely informational and runs on the Pi gateway, which is architecturally isolated from safety-critical ECU firmware. TLS 1.2 with X.509 client certificates is the AWS IoT Core minimum and provides adequate transport security. **Why:** Batching at 1 message per 5 seconds is a pragmatic AWS free-tier constraint that does not affect safety; real-time telemetry for safety purposes is handled on-vehicle via CAN (SYS-031/032). **Tradeoff:** 5-second batching means cloud-side anomaly detection lags by at least 5 seconds -- acceptable for fleet monitoring but not for real-time safety intervention (which is handled locally). **Alternative:** Azure IoT Hub or self-hosted MQTT broker were considered; AWS was chosen for free-tier availability but the MQTT abstraction allows vendor swap per vendor-independence rules.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS042 -->

---

### SYS-043: Edge ML Anomaly Detection

- **Traces up**: STK-015
- **Traces down**: SWR-GW-003, SWR-GW-004
- **Safety relevance**: QM
- **Verification method**: Demonstration
- **Verification criteria**: A simulated anomalous motor current pattern (e.g., sinusoidal oscillation not matching normal profile) triggers an anomaly alert published to `vehicle/alerts` within 5 seconds.
- **Status**: draft

The Raspberry Pi gateway shall run machine learning inference (scikit-learn) on CAN bus data to detect anomalous patterns in motor current, temperature, and CAN message timing. Detected anomalies shall be published to the cloud as alerts. ML inference shall not be used for safety-critical decisions (QM only, informational).

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS043 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is correct and the requirement explicitly states ML inference shall not be used for safety-critical decisions -- this is the right architectural boundary. Running ML on the Pi gateway (Linux, non-deterministic) rather than on safety-critical MCUs preserves freedom from interference. **Why:** Edge ML anomaly detection is a predictive maintenance / fleet analytics feature, not a safety mechanism; its output is advisory only. **Tradeoff:** scikit-learn on Pi is simple to implement but limited in model complexity; for a portfolio demo this is sufficient, production would require a more robust ML pipeline. **Alternative:** Running anomaly detection in the cloud only (no edge) was considered but would lose functionality during connectivity outages and increase cloud compute costs.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS043 -->

---

## 15. Functional System Requirements — Operator Interface

### SYS-044: OLED Status Display

- **Traces up**: STK-026
- **Traces down**: SWR-CVC-002, SWR-CVC-026, SWR-CVC-027, SWR-CVC-028, SWR-ICU-002, SWR-ICU-003, SWR-ICU-004, SWR-ICU-005, SWR-ICU-006, SWR-ICU-007, SWR-ICU-008, SWR-ICU-009, SWR-ICU-010
- **Safety relevance**: QM
- **Verification method**: Demonstration
- **Verification criteria**: The OLED displays the current vehicle state (RUN, DEGRADED, LIMP, SAFE_STOP) and active fault code within 100 ms of state change.
- **Status**: draft

The CVC shall drive the SSD1306 OLED display (128x64 pixels, I2C at 0x3C, 400 kHz) to show: current vehicle state, active fault code (if any), operational restrictions (speed limit, torque limit), and basic telemetry (speed, torque percentage). The display shall be updated every 100 ms.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS044 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is appropriate -- the OLED display provides operator awareness but is not relied upon by any safety goal; all safe-state transitions occur automatically without requiring the operator to read display information. The 100 ms update rate and I2C bus specification (0x3C, 400 kHz) are concrete and testable by demonstration. **Why:** Display latency of 100 ms is adequate for human perception; faster updates would consume CVC CPU budget for no perceptible benefit. **Tradeoff:** Running the OLED on the CVC's I2C bus shares MCU resources with the safety-critical control loop -- if I2C hangs, it could affect timing. This should be mitigated by I2C timeout handling and bus recovery logic. **Alternative:** Dedicated display MCU would fully isolate display from CVC but adds cost and complexity for a non-safety function.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS044 -->

---

### SYS-045: Audible Warning via Buzzer

- **Traces up**: STK-026
- **Traces down**: SWR-FZC-005, SWR-FZC-017, SWR-FZC-018
- **Safety relevance**: QM
- **Verification method**: Demonstration
- **Verification criteria**: Distinct buzzer patterns are produced for DEGRADED (single beep), LIMP (slow repeating), SAFE_STOP (fast repeating), and emergency (continuous) within 200 ms of state change.
- **Status**: draft

The FZC shall drive a piezo buzzer (GPIO) with distinct warning patterns: single beep for DEGRADED transition, slow repeating beep for LIMP, fast repeating beep for SAFE_STOP, and continuous tone for emergency (obstacle in emergency zone or E-stop activated). Buzzer patterns shall be based on CAN state messages from CVC and local lidar detections.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS045 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is appropriate -- the buzzer is an operator notification mechanism, not a safety-critical actuator. Distinct patterns per degradation level (single beep, slow repeat, fast repeat, continuous) are testable by demonstration and provide clear audible differentiation. **Why:** Buzzer placement on the FZC (rather than CVC) allows local lidar obstacle warnings without CAN round-trip latency, which is a good architectural choice. **Tradeoff:** 200 ms response time for buzzer activation is adequate for human perception but means the buzzer lags behind the actual state transition -- acceptable for an advisory function. **Alternative:** Elevating to ASIL A was considered if the buzzer were part of the warning concept in the FSC, but it is supplementary to the automatic safe-state mechanisms.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS045 -->

---

### SYS-046: Fault LED Panel on Safety Controller

- **Traces up**: STK-026, STK-019
- **Traces down**: SWR-ICU-006, SWR-ICU-009, SWR-SC-003, SWR-SC-005, SWR-SC-013
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: When a zone ECU heartbeat times out, the corresponding LED on the SC panel illuminates within 200 ms. The LED is driven directly by SC GPIO, independent of CAN communication to other ECUs.
- **Status**: draft

The Safety Controller shall drive a fault LED panel (4 LEDs on GIO_A1-A4) with one LED per zone ECU (CVC, FZC, RZC) and one system LED. LED states: off = no fault, steady on = fault detected, blinking = heartbeat lost. The LEDs shall be driven directly by SC GPIO, independent of CAN communication, providing operator feedback even during total CAN bus failure.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS046 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is justified because the fault LED panel is the only operator-visible indicator that functions independently of CAN communication -- during total bus failure it is the sole feedback mechanism confirming SC has detected the fault. Direct GPIO drive from the SC (no CAN dependency) is a strong design choice. **Why:** The SC already monitors heartbeats at ASIL C (SYS-022); the LED panel is a direct output of that monitoring, so inheriting ASIL C is consistent. **Tradeoff:** 4 LEDs (3 ECU + 1 system) provide limited diagnostic granularity, but for a field operator knowing which ECU failed is the most actionable information. **Alternative:** A more detailed display (e.g., small LCD on SC) would provide richer diagnostics but would require SC to run I2C/SPI drivers, adding complexity to the safety-critical SC firmware.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS046 -->

---

## 16. Interface Requirements

### SYS-047: SPI Interface — Pedal and Steering Sensors

- **Traces up**: STK-005, STK-006, STK-023
- **Traces down**: SWR-BSW-005, SWR-BSW-006, SWR-BSW-014, SWR-CVC-001, SWR-FZC-001
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: Both pedal sensors on SPI1 are read within 500 us total. SPI clock is 10 MHz. CRC validation of AS5048A data frame passes for all valid readings.
- **Status**: draft

The system shall interface with AS5048A sensors via SPI: CVC SPI1 for dual pedal sensors (CS pins PA4 and PA15), FZC SPI2 for the steering sensor (CS pin PB12). SPI clock speed shall be 10 MHz. The AS5048A 14-bit angle data and diagnostic fields shall be read and validated (CRC check) every control cycle.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS047 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is mandatory -- pedal and steering sensors are the primary inputs to ASIL D control functions (SYS-001, SYS-010); a corrupted SPI read could produce an unintended torque or steering command. CRC validation of every AS5048A frame and the 500 us total read time for dual sensors are concrete, measurable criteria. **Why:** 10 MHz SPI clock is within the AS5048A datasheet maximum (10.5 MHz) with adequate margin; slower clocks would increase read time and could exceed the 10 ms control cycle budget. **Tradeoff:** Using separate CS lines (PA4, PA15) for dual pedal sensors adds GPIO usage but enables independent sensor reads without bus contention -- essential for plausibility monitoring (SYS-002). **Alternative:** I2C sensors were considered but SPI provides higher throughput and deterministic timing, which are critical for ASIL D.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS047 -->

---

### SYS-048: UART Interface — Lidar Sensor

- **Traces up**: STK-008, STK-023
- **Traces down**: SWR-BSW-006, SWR-FZC-013
- **Safety relevance**: ASIL C
- **Verification method**: Test
- **Verification criteria**: TFMini-S UART frames are received at 100 Hz on USART1. Frame parsing extracts distance and signal strength. Frame checksum validation rejects corrupted frames.
- **Status**: draft

The system shall interface with the TFMini-S lidar sensor via USART1 on the FZC at 115200 baud. The UART driver shall receive 9-byte TFMini-S frames using DMA, validate the frame header and checksum, and extract distance (cm) and signal strength values. Frame reception timeout shall be monitored.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS048 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL C is correct -- the lidar feeds the graduated obstacle response (SYS-019) and emergency braking (SYS-017), both ASIL C. The UART interface inherits the ASIL of its consuming safety function. DMA-based reception at 115200 baud with frame header/checksum validation and timeout monitoring are all testable and well-specified. **Why:** The TFMini-S native 100 Hz update rate provides sufficient temporal resolution for obstacle detection at the vehicle speeds this platform operates; frame-level checksum validation catches UART bit errors. **Tradeoff:** UART is a point-to-point interface with no built-in arbitration or error recovery like CAN -- DMA and timeout monitoring compensate, but a stuck UART line requires explicit timeout detection (addressed in SYS-020). **Alternative:** I2C interface to the TFMini-S was available but UART provides higher throughput and simpler DMA configuration on the STM32.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS048 -->

---

### SYS-049: ADC Interface — Current, Temperature, Voltage

- **Traces up**: STK-005, STK-023
- **Traces down**: SWR-BSW-007, SWR-BSW-014, SWR-RZC-005, SWR-RZC-008, SWR-RZC-009, SWR-RZC-017
- **Safety relevance**: ASIL A
- **Verification method**: Test
- **Verification criteria**: ADC channels on RZC (CH1-CH4) convert at 12-bit resolution with sample rate of at least 1 kHz for current (CH1) and 10 Hz for temperature (CH2, CH3) and voltage (CH4).
- **Status**: draft

The system shall read analog signals on the RZC via ADC1: CH1 for motor current (ACS723, 0-3.3V mapping to 0-30A), CH2 and CH3 for motor/winding temperature (NTC, 0-3.3V mapping to -40C to +125C), and CH4 for battery voltage (resistor divider, 0-3.3V mapping to 0-16V). ADC resolution shall be 12-bit. Current sampling rate shall be at least 1 kHz.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS049 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL A is appropriate -- motor current monitoring (SYS-005) and temperature derating (SYS-006) are both ASIL A, and the ADC interface inherits the ASIL of its consuming functions. 12-bit resolution provides adequate precision for the 0-30A current range (7.3 mA/LSB) and 0-16V voltage range (3.9 mV/LSB). **Why:** 1 kHz sampling for current is necessary to detect overcurrent events within the 10 ms debounce window (SYS-005); 10 Hz for temperature and voltage is sufficient given their slow dynamics. **Tradeoff:** Sharing ADC1 across four channels means sequential sampling with channel multiplexing, which introduces inter-channel latency -- acceptable at these sample rates but would need redesign if current sensing moved to higher ASIL. **Alternative:** Dedicated external ADC per channel was considered but adds hardware cost and SPI bus contention for marginal accuracy improvement.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS049 -->

---

### SYS-050: PWM Interface — Motor and Servos

- **Traces up**: STK-005, STK-006, STK-007
- **Traces down**: SWR-BSW-008, SWR-BSW-009, SWR-BSW-014, SWR-FZC-008, SWR-FZC-009, SWR-RZC-003, SWR-RZC-004
- **Safety relevance**: ASIL D
- **Verification method**: Test
- **Verification criteria**: Motor PWM (TIM3) operates at 20 kHz with 0-100% duty cycle control. Servo PWM (TIM2) operates at 50 Hz with 1-2 ms pulse width. All PWM channels are independently controllable.
- **Status**: draft

The system shall generate PWM outputs: RZC TIM3_CH1 and TIM3_CH2 for BTS7960 RPWM/LPWM at 20 kHz, FZC TIM2_CH1 for steering servo at 50 Hz (1-2 ms pulse), and FZC TIM2_CH2 for brake servo at 50 Hz (1-2 ms pulse). PWM duty cycle resolution shall be at least 10 bits. PWM outputs shall be independently disable-able via software.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS050 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is mandatory -- PWM directly controls the motor H-bridge (BTS7960) and steering/brake servos; an incorrect PWM output could cause unintended acceleration, loss of steering, or loss of braking. Independent disable-ability per channel is critical for fault isolation (e.g., disabling motor PWM while keeping brake servo active). **Why:** 20 kHz motor PWM is above the audible range, preventing buzzing; 50 Hz servo PWM matches standard RC servo protocol. 10-bit duty cycle resolution provides 0.1% torque granularity, which is adequate for the BTS7960 driver. **Tradeoff:** Using separate timers (TIM3 for motor, TIM2 for servos) on different ECUs (RZC, FZC) provides hardware-level independence but means servo and motor timing are not synchronized -- acceptable since they operate on independent control loops. **Alternative:** A single high-resolution timer with multiple compare channels was considered but would couple motor and servo outputs on a single fault domain.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS050 -->

---

## 17. Non-Functional System Requirements

### SYS-051: MISRA C Compliance

- **Traces up**: STK-030
- **Traces down**: SWR-ALL-001
- **Safety relevance**: ASIL D
- **Verification method**: Analysis
- **Verification criteria**: Static analysis (cppcheck or equivalent) reports zero mandatory MISRA C:2012 rule violations. All deviations are documented with formal deviation records.
- **Status**: draft

All firmware source code shall comply with MISRA C:2012 (with 2023 amendments). Mandatory rules shall have zero violations. Required rules shall have documented deviations. Advisory rules shall be followed where practical. Static analysis enforcement shall be part of the CI build pipeline.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS051 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- MISRA C compliance is referenced by ISO 26262 Part 6 Table 1 as a highly recommended language subset for all ASIL levels, and is effectively mandatory at ASIL D. Zero mandatory rule violations is a hard requirement; documented deviations for required rules with the formal deviation process (DEV-001, DEV-002 already in misra-deviation-register.md) follow the standard pattern. **Why:** CI-enforced static analysis (cppcheck with MISRA addon, already operational per plan-misra-pipeline.md) catches violations before merge, preventing regression. **Tradeoff:** MISRA compliance adds development overhead (stricter coding patterns, deviation paperwork) but eliminates entire classes of undefined behavior. **Alternative:** CERT C was considered as an alternative coding standard but MISRA C is the industry expectation for ISO 26262 projects and has broader tool support.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS051 -->

---

### SYS-052: Static RAM Only — No Dynamic Allocation

- **Traces up**: STK-016, STK-030
- **Traces down**: SWR-ALL-002
- **Safety relevance**: ASIL D
- **Verification method**: Analysis
- **Verification criteria**: No calls to malloc, calloc, realloc, or free exist in any firmware source file. Linker map confirms all data segments are statically sized. Heap size is configured to zero.
- **Status**: draft

All firmware shall use static memory allocation only. Dynamic memory allocation functions (malloc, calloc, realloc, free) shall not be used in any production firmware. All buffers, queues, and data structures shall be statically sized at compile time. The heap shall be configured to zero size in the linker script.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS052 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- dynamic memory allocation introduces non-deterministic timing (fragmentation, allocation failure) that cannot be statically analyzed for WCET, making it incompatible with ASIL D timing requirements (SYS-053). MISRA C:2012 Directive 4.12 and Rule 21.3 also prohibit it. Verification criteria are concrete: grep for banned functions, linker map inspection, and zero heap size are all automatable checks. **Why:** Static-only allocation enables complete memory usage analysis at compile time and eliminates an entire class of runtime failure modes (heap exhaustion, fragmentation, double-free). **Tradeoff:** Static allocation requires pre-sizing all buffers at compile time, which may waste RAM on underutilized buffers -- but STM32G474RE has 128 KB SRAM, which is generous for this application. **Alternative:** Fixed-size memory pools (deterministic allocation) were considered as a compromise but add complexity without clear benefit when all buffer sizes are known at design time.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS052 -->

---

### SYS-053: WCET Within Deadline Margin

- **Traces up**: STK-021, STK-018
- **Traces down**: SWR-ALL-003, SWR-BSW-010, SWR-BSW-027, SWR-CVC-032, SWR-FZC-029, SWR-RZC-028, SWR-SC-025
- **Safety relevance**: ASIL D
- **Verification method**: Analysis
- **Verification criteria**: Measured WCET of the main control loop task on each ECU is below 80% of its scheduling deadline (8 ms for a 10 ms cycle). Measurement is performed on target hardware with all interrupts enabled.
- **Status**: draft

The worst-case execution time (WCET) of each safety-critical task shall not exceed 80% of its scheduling deadline. For the 10 ms main control loop, WCET shall be below 8 ms. WCET shall be measured on target hardware under worst-case conditions and documented in the timing analysis report.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS053 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is mandatory -- ISO 26262 Part 6 requires WCET analysis for all safety-critical tasks, and exceeding the scheduling deadline would violate the FTTI for torque, steering, and braking control loops. The 80% margin (8 ms for a 10 ms cycle) provides 2 ms headroom for interrupt jitter and worst-case code paths. **Why:** Measurement-based WCET on target hardware with all interrupts enabled is the most representative method for Cortex-M4 (STM32G474RE) where static analysis tools (e.g., AbsInt aiT) are expensive and may not support the exact core variant. **Tradeoff:** 80% margin is conservative (industry typical is 70-90%); tighter margins would allow more complex algorithms but reduce resilience to code growth. **Alternative:** Static WCET analysis (AbsInt aiT) would provide formal upper bounds but the tool cost is disproportionate for a portfolio project; measurement-based with documented worst-case stimulus is the practical choice.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS053 -->

---

### SYS-054: Flash Memory Utilization

- **Traces up**: STK-021
- **Traces down**: SWR-ALL-004
- **Safety relevance**: QM
- **Verification method**: Analysis
- **Verification criteria**: Linker map output shows total flash usage below 80% of available flash on each MCU (STM32G474RE: 512 KB, TMS570LC43x: 4 MB).
- **Status**: draft

Flash memory utilization shall not exceed 80% of the available flash on each MCU to provide margin for future updates, safety analyses, and DTC storage. The linker output shall be checked as part of the build process.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS054 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is appropriate -- flash utilization is a resource planning constraint, not a safety function. Exceeding 80% does not directly violate a safety goal; it reduces margin for future updates and DTC storage. Verification criteria are concrete: linker map output with specific MCU flash sizes (512 KB, 4 MB) provides an automatable CI check. **Why:** 80% threshold is industry standard for embedded systems; it reserves 20% for OTA update staging, DTC storage, and future feature growth without reflash. **Tradeoff:** A stricter threshold (e.g., 70%) would provide more margin but would unnecessarily constrain the firmware feature set at this stage. **Alternative:** No alternative ASIL considered; flash utilization monitoring is a good engineering practice but has no safety-critical failure mode.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS054 -->

---

### SYS-055: Bidirectional Traceability Chain

- **Traces up**: STK-031
- **Traces down**: (this requirement governs the traceability process itself)
- **Safety relevance**: ASIL D
- **Verification method**: Inspection
- **Verification criteria**: The traceability matrix in the traceability document shows complete forward and backward trace for every SYS requirement: upward to STK, downward to TSR/SWR, and further to source code and test cases. No orphan requirements exist.
- **Status**: draft

Bidirectional traceability shall be maintained from stakeholder requirements (STK) through system requirements (SYS), technical safety requirements (TSR), software requirements (SWR), source code, unit tests, integration tests, and system tests. Every requirement shall trace down to implementation and test. Every test shall trace back to a requirement. The traceability matrix shall be maintained in `docs/aspice/traceability/traceability-matrix.md`.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS055 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ASIL D is correct -- bidirectional traceability is a fundamental ISO 26262 Part 8 requirement and an ASPICE Level 2 prerequisite. Incomplete traceability at ASIL D is a compliance-blocking finding in any safety assessment. Verification by inspection is the correct method since traceability is a documentation property, not a runtime behavior. **Why:** The requirement covers the full V-model chain (STK -> SYS -> TSR/SWR -> code -> test) which is exactly what ISO 26262 Part 8 Section 7 demands. No orphan requirements and no untested code are the key completeness criteria. **Tradeoff:** Manual traceability maintenance in markdown is labor-intensive compared to tool-based (DOORS, Polarion), but avoids vendor lock-in and tool qualification costs for a portfolio project. **Alternative:** Automated traceability extraction from code tags (e.g., @verifies, @traces_to) could reduce manual maintenance; this is partially implemented via the test-first hook pattern.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS055 -->

---

### SYS-056: SAP QM Mock Integration

- **Traces up**: STK-032
- **Traces down**: SWR-GW-005, SWR-GW-006
- **Safety relevance**: QM
- **Verification method**: Demonstration
- **Verification criteria**: A DTC event triggers an HTTP POST to the SAP QM mock API. The mock returns a Q-Meldung number. An 8D report template is auto-generated.
- **Status**: draft

The Raspberry Pi gateway shall forward new DTCs to a mock SAP QM API endpoint via HTTP REST. The mock shall create a quality notification (Q-Meldung) and auto-generate an 8D report template containing the DTC details, freeze-frame data, and suggested corrective actions.

<!-- HITL-LOCK START:COMMENT-BLOCK-SYS056 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** QM is correct -- SAP QM integration is a quality management / enterprise process feature, not a vehicle safety function. It runs on the Pi gateway (Linux, non-real-time) and communicates via HTTP REST, fully isolated from safety-critical firmware. Verification by demonstration is appropriate for a mock API endpoint. **Why:** The mock SAP QM integration demonstrates end-to-end DTC-to-quality-notification workflow for the portfolio, showing automotive-to-enterprise connectivity. Production SAP QM would require RFC/BAPI integration, but the mock serves the demo purpose. **Tradeoff:** Using a mock rather than a real SAP QM instance limits the fidelity of the demo but avoids SAP licensing costs and infrastructure complexity. **Alternative:** Direct SAP S/4HANA Cloud API integration was considered but requires an SAP tenant and ODATA configuration that is out of scope for a portfolio demo.
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS056 -->

---

## 18. Traceability Matrix — STK to SYS

The following matrix provides complete bidirectional traceability from stakeholder requirements to system requirements. Every STK requirement traces to at least one SYS requirement. No SYS requirement exists without an STK parent.

| STK | SYS Requirements |
|-----|------------------|
| STK-001 | SYS-029, SYS-032, SYS-041, SYS-055 |
| STK-002 | SYS-055 |
| STK-003 | SYS-030, SYS-032 |
| STK-004 | SYS-025, SYS-026 |
| STK-005 | SYS-001, SYS-002, SYS-003, SYS-004, SYS-005, SYS-006, SYS-007, SYS-008, SYS-009, SYS-029, SYS-030, SYS-047, SYS-049, SYS-050 |
| STK-006 | SYS-010, SYS-011, SYS-012, SYS-013, SYS-029, SYS-030, SYS-047, SYS-050 |
| STK-007 | SYS-014, SYS-015, SYS-016, SYS-017, SYS-029, SYS-030, SYS-050 |
| STK-008 | SYS-017, SYS-018, SYS-019, SYS-020, SYS-048 |
| STK-009 | SYS-021, SYS-022, SYS-023, SYS-024, SYS-025, SYS-026, SYS-027, SYS-034, SYS-046 |
| STK-010 | SYS-028 |
| STK-011 | SYS-035, SYS-036 |
| STK-012 | SYS-037, SYS-038, SYS-039, SYS-040 |
| STK-013 | SYS-038, SYS-041 |
| STK-014 | SYS-042 |
| STK-015 | SYS-043 |
| STK-016 | SYS-001, SYS-002, SYS-005, SYS-006, SYS-007, SYS-011, SYS-013, SYS-015, SYS-020, SYS-023, SYS-024, SYS-028, SYS-029, SYS-051, SYS-052 |
| STK-017 | SYS-012, SYS-016, SYS-024, SYS-029, SYS-034 |
| STK-018 | SYS-002, SYS-011, SYS-015, SYS-053 |
| STK-019 | SYS-022, SYS-024, SYS-027, SYS-046 |
| STK-020 | SYS-032 |
| STK-021 | SYS-053, SYS-054 |
| STK-022 | SYS-004, SYS-031, SYS-033 |
| STK-023 | SYS-047, SYS-048, SYS-049 |
| STK-024 | SYS-024 |
| STK-025 | SYS-029 |
| STK-026 | SYS-036, SYS-044, SYS-045, SYS-046 |
| STK-027 | (addressed by build system and documentation — no specific SYS requirement; process requirement) |
| STK-028 | SYS-029, SYS-032, SYS-051, SYS-055 |
| STK-029 | SYS-055 |
| STK-030 | SYS-051, SYS-052 |
| STK-031 | SYS-055 |
| STK-032 | SYS-056 |

### 18.1 Reverse Traceability — SYS to STK

| SYS | STK Parents |
|-----|-------------|
| SYS-001 | STK-005, STK-016 |
| SYS-002 | STK-005, STK-016, STK-018 |
| SYS-003 | STK-005 |
| SYS-004 | STK-005, STK-022 |
| SYS-005 | STK-005, STK-016 |
| SYS-006 | STK-005, STK-016 |
| SYS-007 | STK-005, STK-016 |
| SYS-008 | STK-005 |
| SYS-009 | STK-005 |
| SYS-010 | STK-006 |
| SYS-011 | STK-006, STK-016, STK-018 |
| SYS-012 | STK-006, STK-017 |
| SYS-013 | STK-006, STK-016 |
| SYS-014 | STK-007 |
| SYS-015 | STK-007, STK-016, STK-018 |
| SYS-016 | STK-007, STK-017 |
| SYS-017 | STK-007, STK-008 |
| SYS-018 | STK-008 |
| SYS-019 | STK-008 |
| SYS-020 | STK-008, STK-016 |
| SYS-021 | STK-009 |
| SYS-022 | STK-009, STK-019 |
| SYS-023 | STK-009, STK-016 |
| SYS-024 | STK-009, STK-016, STK-017, STK-019, STK-024 |
| SYS-025 | STK-004, STK-009 |
| SYS-026 | STK-004, STK-009 |
| SYS-027 | STK-009, STK-019 |
| SYS-028 | STK-010, STK-016 |
| SYS-029 | STK-001, STK-005, STK-006, STK-007, STK-016, STK-017, STK-025, STK-028 |
| SYS-030 | STK-003, STK-005, STK-006, STK-007 |
| SYS-031 | STK-022 |
| SYS-032 | STK-001, STK-003, STK-020, STK-028 |
| SYS-033 | STK-022 |
| SYS-034 | STK-009, STK-017 |
| SYS-035 | STK-011 |
| SYS-036 | STK-011, STK-026 |
| SYS-037 | STK-012 |
| SYS-038 | STK-012, STK-013 |
| SYS-039 | STK-012 |
| SYS-040 | STK-012 |
| SYS-041 | STK-001, STK-013 |
| SYS-042 | STK-014 |
| SYS-043 | STK-015 |
| SYS-044 | STK-026 |
| SYS-045 | STK-026 |
| SYS-046 | STK-009, STK-019, STK-026 |
| SYS-047 | STK-005, STK-006, STK-023 |
| SYS-048 | STK-008, STK-023 |
| SYS-049 | STK-005, STK-023 |
| SYS-050 | STK-005, STK-006, STK-007 |
| SYS-051 | STK-028, STK-030 |
| SYS-052 | STK-016, STK-030 |
| SYS-053 | STK-018, STK-021 |
| SYS-054 | STK-021 |
| SYS-055 | STK-001, STK-002, STK-028, STK-029, STK-031 |
| SYS-056 | STK-032 |

### 18.2 Traceability Completeness Check

| Check | Result |
|-------|--------|
| All 32 STK requirements trace to at least one SYS? | YES — 31 of 32 trace directly. STK-027 is a process requirement (build/demo) addressed by project infrastructure, not a system requirement. |
| All 56 SYS requirements trace to at least one STK? | YES — every SYS-NNN has at least one STK parent. |
| No orphan SYS requirements (no STK parent)? | YES — 0 orphans. |
| No orphan STK requirements (no SYS child)? | YES — 0 orphans (STK-027 acknowledged as process scope). |

---

## 19. Requirements Summary

### 19.1 By Category

| Category | Count | SYS Range |
|----------|-------|-----------|
| Drive-by-Wire | 9 | SYS-001 to SYS-009 |
| Steering | 4 | SYS-010 to SYS-013 |
| Braking | 4 | SYS-014 to SYS-017 |
| Obstacle Detection | 3 | SYS-018 to SYS-020 |
| Safety Monitoring | 7 | SYS-021 to SYS-027 |
| Emergency Stop | 1 | SYS-028 |
| State Management | 2 | SYS-029 to SYS-030 |
| CAN Communication | 4 | SYS-031 to SYS-034 |
| Body Control | 2 | SYS-035 to SYS-036 |
| Diagnostics | 5 | SYS-037 to SYS-041 |
| Telemetry / Cloud | 2 | SYS-042 to SYS-043 |
| Operator Interface | 3 | SYS-044 to SYS-046 |
| Interface | 4 | SYS-047 to SYS-050 |
| Non-Functional | 6 | SYS-051 to SYS-056 |
| **Total** | **56** | |

### 19.2 By Safety Relevance

| ASIL | Count | SYS IDs |
|------|-------|---------|
| ASIL D | 24 | SYS-001, SYS-002, SYS-003, SYS-004, SYS-010, SYS-011, SYS-012, SYS-014, SYS-015, SYS-016, SYS-024, SYS-026, SYS-027, SYS-029, SYS-030, SYS-031, SYS-032, SYS-033, SYS-047, SYS-050, SYS-051, SYS-052, SYS-053, SYS-055 |
| ASIL C | 13 | SYS-007, SYS-013, SYS-017, SYS-018, SYS-019, SYS-020, SYS-021, SYS-022, SYS-023, SYS-025, SYS-034, SYS-046, SYS-048 |
| ASIL B | 1 | SYS-028 |
| ASIL A | 3 | SYS-005, SYS-006, SYS-049 |
| QM | 15 | SYS-008, SYS-009, SYS-035, SYS-036, SYS-037, SYS-038, SYS-039, SYS-040, SYS-041, SYS-042, SYS-043, SYS-044, SYS-045, SYS-054, SYS-056 |

### 19.3 By Verification Method

| Method | Count |
|--------|-------|
| Test (T) | 42 |
| Analysis (A) | 6 |
| Demonstration (D) | 7 |
| Inspection (I) | 1 |

---

## 20. Open Items and Assumptions

### 20.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| SYS-A-001 | CAN message cycle times are achievable at 500 kbps with defined bus load | CAN scheduling analysis needed during SYS.3 |
| SYS-A-002 | Calibratable thresholds (pedal, steering, current, temperature, lidar) use engineering estimates pending hardware characterization | Final values determined during integration testing |
| SYS-A-003 | STM32G474RE FDCAN controller in classic mode is interoperable with TMS570LC43x DCAN at 500 kbps | Confirmed in hardware feasibility analysis |
| SYS-A-004 | Docker containers with SocketCAN can emulate CAN communication for simulated ECUs | Confirmed in architecture decision |

### 20.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| SYS-O-001 | Derive Technical Safety Requirements (TSR) from safety-relevant SYS requirements | System Engineer | TSC phase |
| SYS-O-002 | Derive Software Requirements (SWR) per ECU from SYS requirements | SW Engineer | SWE.1 phase |
| SYS-O-003 | Perform CAN bus scheduling and utilization analysis | System Architect | SYS.3 phase |
| SYS-O-004 | Define complete CAN message matrix (IDs, cycle times, DLC, signals) | System Architect | SYS.3 phase |
| SYS-O-005 | Characterize calibratable thresholds on target hardware | Integration Engineer | Integration phase |
| SYS-O-006 | Complete ASIL classification review for all 56 SYS requirements | FSE | Before SWE.1 |

---

## 21. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete system requirements: 56 requirements (SYS-001 to SYS-056), bidirectional traceability matrix (STK to SYS and SYS to STK), completeness check, verification criteria for all requirements |
