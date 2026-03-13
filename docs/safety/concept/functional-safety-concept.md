---
document_id: FSC
title: "Functional Safety Concept"
version: "1.0"
status: draft
iso_26262_part: 3
iso_26262_clause: "8"
aspice_process: SYS.1
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

Every safety concept element in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/safety/lessons-learned/`](../lessons-learned/). One file per safety concept element. File naming: `FSC-<topic>.md`.


# Functional Safety Concept

<!-- DECISION: ADR-003 — Zonal safety mechanism allocation -->

## 1. Purpose

This document defines the functional safety concept for the Taktflow Zonal Vehicle Platform per ISO 26262-3 Clause 8. It derives functional safety requirements (FSR) from the safety goals established in the HARA (document HARA), specifies safety mechanisms to detect and mitigate hazardous conditions, defines the warning and degradation concept, and allocates safety mechanisms to system elements.

The functional safety concept bridges the gap between "what must be prevented" (safety goals) and "how the system prevents it" (technical safety concept). It addresses the WHAT — the required safety behavior — without prescribing the detailed HOW, which is the domain of the technical safety concept (TSC, ISO 26262-4).

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| ITEM-DEF | Item Definition | 1.0 |
| HARA | Hazard Analysis and Risk Assessment | 1.0 |
| SG | Safety Goals | 1.0 |
| FSR | Functional Safety Requirements | 1.0 |
| SP | Safety Plan | 0.1 |
| DFA | Dependent Failure Analysis | 0.1 |

## 3. Safety Goals Summary

The following safety goals were derived from the HARA and form the basis for this functional safety concept.

| SG-ID | Safety Goal | ASIL | Safe State | FTTI |
|-------|-------------|------|------------|------|
| SG-001 | Prevent unintended acceleration | D | Motor off, brakes applied | 50 ms |
| SG-002 | Prevent unintended loss of drive torque | B | Controlled stop | 200 ms |
| SG-003 | Prevent unintended steering movement | D | Motor off, brakes applied (SAFE_STOP) | 100 ms |
| SG-004 | Prevent unintended loss of braking | D | Motor off (fail-safe) | 50 ms |
| SG-005 | Prevent unintended braking | A | Release brake | 200 ms |
| SG-006 | Ensure motor protection (overcurrent/overtemp) | A | Motor off | 500 ms |
| SG-007 | Ensure obstacle detection | C | Controlled stop | 200 ms |
| SG-008 | Ensure independent safety monitoring | C | System shutdown | 100 ms |

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Sections 1-3 establish the FSC context correctly. The purpose statement accurately describes the FSC's role as bridging "what must be prevented" (safety goals) to "how the system prevents it" (safety mechanisms). The safety goals summary table is consistent with the SG document. The referenced documents include ITEM-DEF, HARA, SG, FSR, SP, and DFA, which is the correct set for a functional safety concept. Note: the HARA and SG reference versions should be kept synchronized as those documents evolve.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC1 -->

## 4. Safety Mechanisms per Safety Goal

### 4.1 SG-001: Prevent Unintended Acceleration (ASIL D)

Unintended acceleration is the highest-severity hazard. Multiple independent safety mechanisms are required to achieve ASIL D diagnostic coverage.

#### SM-001: Dual Pedal Sensor Plausibility Check

- **Mechanism**: Two independent AS5048A magnetic angle sensors on the same pedal shaft, read via separate SPI chip-select lines on SPI1. The CVC computes the absolute difference |S1 - S2| every control cycle (10 ms). If the difference exceeds a calibratable threshold (default: 5% of full range) for more than 2 consecutive cycles (debounce), a pedal plausibility fault is declared.
- **Detection**: Analog sensor drift, stuck sensor, wiring fault, SPI communication error. Each sensor reading is also individually range-checked (0.5V to 4.5V equivalent after ADC conversion) to detect open circuit or short to ground/supply.
- **Fault reaction**: On plausibility fault, the CVC sets torque request to zero, transmits a zero-torque CAN message to RZC, and transitions the vehicle state machine to DEGRADED (if first occurrence) or SAFE_STOP (if persistent). The zero-torque command is latched until the fault clears and a manual reset is performed.
- **ECU**: CVC (STM32G474RE)
- **Diagnostic coverage**: 99% (dual redundant sensors with diverse readout paths)
- **FTTI compliance**: Detection within 20 ms (2 control cycles), reaction within 10 ms (next CAN transmit cycle). Total: 30 ms < 50 ms FTTI.
- **Traces to**: FSR-001, FSR-002, FSR-003

#### SM-002: Motor Current Monitoring with Overcurrent Cutoff

- **Mechanism**: The RZC continuously samples motor current via the ACS723 current sensor on ADC1_CH1 at 1 kHz. If current exceeds the maximum rated threshold (calibratable, default: 25A) for more than 10 ms (debounce), the RZC immediately disables both BTS7960 enable lines (R_EN, L_EN) via GPIO, cutting motor power. Additionally, the BTS7960 has built-in overcurrent protection as a secondary independent mechanism.
- **Detection**: Motor stall, short circuit, driver fault, incorrect PWM duty cycle, mechanical overload.
- **Fault reaction**: Motor disabled (both enable lines LOW), DTC logged (DEM), CAN fault status broadcast to CVC and SC. Motor remains disabled until fault clears and a controlled re-enable sequence is completed.
- **ECU**: RZC (STM32G474RE)
- **Diagnostic coverage**: 95% (single current sensor; BTS7960 built-in protection adds diversity)
- **FTTI compliance**: Detection within 10 ms, reaction within 1 ms (GPIO toggle). Total: 11 ms < 50 ms FTTI.
- **Traces to**: FSR-004, FSR-022

#### SM-003: Cross-Plausibility — Torque Request vs. Actual Current

- **Mechanism**: The Safety Controller (SC) receives both the torque request CAN message from CVC and the actual motor current CAN message from RZC. The SC compares the expected current (derived from torque request via a lookup table) against the actual measured current. If the deviation exceeds a calibratable threshold (default: 20% of expected) for more than 50 ms, the SC declares a cross-plausibility fault.
- **Detection**: Unintended acceleration due to stuck PWM driver, CAN message corruption that passed E2E checks, software fault in torque calculation, driver IC runaway.
- **Fault reaction**: SC opens the kill relay (de-energize), forcing all safety-critical actuators to a power-off state. Fault LED panel illuminated. DTC logged.
- **ECU**: SC (TMS570LC43x) — independent, diverse vendor
- **Diagnostic coverage**: 90% (limited by resolution of current-to-torque mapping and CAN bus latency)
- **FTTI compliance**: Detection within 60 ms, reaction within 1 ms (relay de-energize). Total: 61 ms. This exceeds the 50 ms FTTI for SG-001; however, SM-001 and SM-002 provide primary coverage within FTTI. SM-003 is a tertiary defense against faults not caught by the primary mechanisms.
- **Traces to**: FSR-022

#### SM-004: CAN E2E Protection — Torque Messages

- **Mechanism**: All torque-related CAN messages (pedal position, torque request, motor status) include an E2E protection header: CRC-8 (polynomial 0x1D, SAE J1850), alive counter (4-bit, incrementing), and data ID (unique per message). The receiver validates CRC, checks alive counter sequence (no gap > 1, no repeat), and verifies data ID. A failed E2E check results in the message being discarded and a "last known good" value or safe default being used.
- **Detection**: CAN message corruption, repetition, loss, delay, insertion, masquerading.
- **Fault reaction**: On E2E failure, the receiver uses the last valid value for up to one additional cycle. If E2E fails for 3 consecutive cycles, the receiver switches to a safe default (zero torque for torque messages, full brake for brake messages).
- **ECU**: All physical ECUs (CVC, FZC, RZC, SC)
- **Diagnostic coverage**: 99.6% for random bit errors (CRC-8 Hamming distance 4 for payloads up to 8 bytes), plus alive counter coverage for message loss/delay.
- **FTTI compliance**: Detection within one CAN cycle (10 ms), reaction within one additional cycle (10 ms). Total: 20 ms < 50 ms FTTI.
- **Traces to**: FSR-013

#### SM-005: Kill Relay — Energize-to-Run Pattern

- **Mechanism**: The kill relay is wired in an energize-to-run configuration: the relay coil must be actively energized by the SC's GIO_A0 GPIO pin to close the relay and allow power to flow to the motor, servos, and other safety-critical actuators. If the SC loses power, the MCU hangs, or the SC intentionally de-asserts the GPIO, the relay opens and power is cut. This is a fail-safe design — any failure of the SC results in the safe state.
- **Detection**: The kill relay provides an independent hardware-enforced safety boundary. The SC de-energizes the relay whenever any of the following conditions are detected: heartbeat timeout from any zone ECU, cross-plausibility fault, SC self-test failure, external watchdog timeout.
- **Fault reaction**: Relay opens (de-energize), cutting 12V power to motor driver and servo circuits. The system enters SHUTDOWN state. The kill relay cannot be re-closed without a full system restart and self-test pass.
- **ECU**: SC (TMS570LC43x)
- **Diagnostic coverage**: 99% (hardware-enforced fail-safe; relay failure to open is residual risk addressed in FMEA)
- **FTTI compliance**: Relay de-energize time < 5 ms (relay dropout time). Total: detection time + 5 ms.
- **Traces to**: FSR-016

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC2 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4.1 defines 5 safety mechanisms for SG-001 (ASIL D), providing defense-in-depth: SM-001 (dual pedal plausibility, primary), SM-002 (motor overcurrent cutoff, secondary), SM-003 (cross-plausibility torque/current, tertiary), SM-004 (CAN E2E protection), and SM-005 (kill relay). This layered approach is appropriate for ASIL D. SM-001 achieves 99% diagnostic coverage with 30 ms total time, well within the 50 ms FTTI. SM-003 is acknowledged to exceed the 50 ms FTTI at 61 ms, but this is acceptable as a tertiary mechanism. The kill relay (SM-005) with energize-to-run pattern is a strong hardware fail-safe. One concern: SM-001 describes the dual sensors sharing SPI1 bus with separate chip-select lines -- this is a potential common cause failure path (shared SPI peripheral) that should be analyzed in the DFA. The E2E protection (SM-004) specification of CRC-8 with polynomial 0x1D, 4-bit alive counter, and data ID is well-specified and aligns with AUTOSAR E2E Profile P01.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC2 -->

### 4.2 SG-002: Prevent Unintended Loss of Drive Torque (ASIL B)

Loss of drive torque is a lower-severity hazard (ASIL B) because the platform is a bench demo with limited kinetic energy. The primary risk is unexpected motor shutdown during a demonstration.

#### SM-006: Motor Controller Health Monitoring

- **Mechanism**: The RZC performs continuous self-diagnostics on the motor control path: PWM output verification (timer capture feedback), BTS7960 enable status readback, and motor current plausibility (current present when PWM active). If the motor controller path is healthy but torque output unexpectedly drops to zero, the RZC flags a "torque loss" condition.
- **Detection**: PWM timer fault, BTS7960 driver failure, wiring disconnection, encoder feedback loss.
- **Fault reaction**: RZC broadcasts a torque-loss DTC on CAN. CVC transitions to DEGRADED mode. If operator acknowledges, system may attempt controlled restart. If fault persists, system transitions to SAFE_STOP (controlled stop with brakes applied).
- **ECU**: RZC (STM32G474RE)
- **Diagnostic coverage**: 80% (limited by inability to detect all mechanical disconnection modes)
- **FTTI compliance**: Detection within 50 ms (5 control cycles), reaction within 20 ms. Total: 70 ms < 200 ms FTTI.
- **Traces to**: FSR-002 (partial), FSR-019

#### SM-007: CAN Alive Monitoring for RZC

- **Mechanism**: The CVC monitors the alive counter in RZC status messages. If the RZC stops transmitting (CAN timeout) or the alive counter stalls, the CVC detects loss of the motor controller and initiates a controlled stop.
- **Detection**: RZC processor hang, CAN transceiver failure, wiring fault on RZC CAN connection.
- **Fault reaction**: CVC transitions to SAFE_STOP, commands FZC to apply brakes. SC detects RZC heartbeat loss independently and may open kill relay.
- **ECU**: CVC (primary), SC (secondary/independent)
- **Diagnostic coverage**: 95%
- **FTTI compliance**: Detection within 100 ms (heartbeat timeout), reaction within 10 ms. Total: 110 ms < 200 ms FTTI.
- **Traces to**: FSR-014, FSR-020

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4.2 provides 2 safety mechanisms for SG-002 (ASIL B): SM-006 (motor controller health monitoring) and SM-007 (CAN alive monitoring for RZC). The ASIL B rating appropriately requires fewer independent mechanisms than ASIL D. SM-006 at 80% diagnostic coverage is adequate for ASIL B. The ASIL B characterization as "lower-severity hazard because the platform is a bench demo" in the introductory text is inconsistent with assumption A-001 (ratings assume real vehicle) -- the ASIL B rating comes from the S/E/C assessment, not from the bench demo nature.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC3 -->

### 4.3 SG-003: Prevent Unintended Steering Movement (ASIL D)

Unintended steering movement at any speed can cause loss of vehicle control. This is an ASIL D hazard requiring multiple independent safety mechanisms.

#### SM-008: Steering Angle Feedback Monitoring

- **Mechanism**: The FZC reads the steering AS5048A angle sensor via SPI2 at 100 Hz and compares the actual steering angle against the commanded angle (received via CAN from CVC). If the position error exceeds a calibratable threshold (default: 5 degrees) for more than 50 ms, a steering position fault is declared. Additionally, the sensor reading is range-checked (valid mechanical range: -45 to +45 degrees) and rate-of-change checked (maximum slew rate: 360 degrees/second).
- **Detection**: Servo runaway, sensor fault, CAN command corruption that passed E2E, mechanical binding.
- **Fault reaction**: FZC broadcasts a steering fault on CAN. CVC transitions to SAFE_STOP (motor off, brakes applied). FZC disables steering servo PWM. The platform has no mechanical fallback steering — a steering fault means the vehicle cannot be directionally controlled, so SAFE_STOP (not DEGRADED) is required.
- **ECU**: FZC (STM32G474RE)
- **Diagnostic coverage**: 95% (single feedback sensor; residual risk from simultaneous servo and sensor fault)
- **FTTI compliance**: Detection within 50 ms, reaction within 50 ms (servo ramp to center). Total: 100 ms = FTTI.
- **Traces to**: FSR-006, FSR-007

#### SM-009: Steering Rate Limiting

- **Mechanism**: The FZC implements a maximum steering rate limit in software. The commanded steering angle change per control cycle is clamped to a maximum value (calibratable, default: 30 degrees/second). Any CAN command requesting a faster rate is rate-limited before being applied to the servo. This prevents sudden steering inputs from causing loss of control.
- **Detection**: Excessive rate command (from CAN corruption, software fault in CVC, or malicious input).
- **Fault reaction**: Rate-limited command applied instead of raw command. If the raw command consistently exceeds rate limits for more than 500 ms (indicating a systematic fault rather than normal driving), a steering rate fault is logged and reported to CVC.
- **ECU**: FZC (STM32G474RE)
- **Diagnostic coverage**: 60% (prevents rapid steering but does not detect slow drift faults; SM-008 covers those)
- **FTTI compliance**: Rate limiting is applied every control cycle (10 ms). Inherently within FTTI.
- **Traces to**: FSR-008

#### SM-010: Steering Angle Limits (Mechanical + Software)

- **Mechanism**: Software-enforced steering angle limits (-45 to +45 degrees) prevent the servo from commanding positions outside the valid mechanical range. The limits include a software margin (2 degrees inside mechanical stops) to prevent mechanical damage and stalling. If the commanded angle exceeds limits, it is clamped.
- **Detection**: Out-of-range command.
- **Fault reaction**: Clamped to nearest valid limit. If clamping occurs for more than 1 second continuously, a "steering limit" DTC is logged.
- **ECU**: FZC (STM32G474RE)
- **Diagnostic coverage**: 99% (software + mechanical stops)
- **FTTI compliance**: Applied every control cycle (10 ms). Inherently within FTTI.
- **Traces to**: FSR-008

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4.3 provides 3 safety mechanisms for SG-003 (ASIL D): SM-008 (steering angle feedback), SM-009 (steering rate limiting), and SM-010 (steering angle limits). SM-008 is the primary detection mechanism with 95% diagnostic coverage. The updated fault reaction (steer fault leads to SAFE_STOP, not DEGRADED) per v1.1 is correct for a platform without mechanical fallback steering. SM-009 rate limiting at 30 degrees/second prevents sudden steering inputs but has only 60% diagnostic coverage, correctly noting its limitation for slow drift faults. SM-010 provides software angle limits with 99% coverage for out-of-range commands. One gap: there is no diverse/independent sensor for steering position feedback -- the system relies on a single AS5048A sensor. For ASIL D, this is a residual risk that should be documented and addressed in the DFA, potentially requiring a secondary feedback mechanism or acceptance as a residual risk with additional compensating measures.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC4 -->

### 4.4 SG-004: Prevent Unintended Loss of Braking (ASIL D)

Loss of braking is a critical hazard (ASIL D) as the platform cannot decelerate without the brake actuator. The fail-safe strategy combines brake redundancy with motor cutoff.

#### SM-011: Brake Command Monitoring

- **Mechanism**: The FZC monitors the brake servo command path: PWM output is verified via timer capture feedback, servo current is checked for plausibility (current draw expected when brake force applied), and CAN brake commands from CVC are validated via E2E protection. If the brake system is unable to apply the requested braking force (detected via lack of servo current or feedback position mismatch), a brake fault is declared.
- **Detection**: Servo failure, PWM timer fault, wiring disconnection, CAN communication loss.
- **Fault reaction**: FZC broadcasts a brake fault on CAN. CVC commands RZC to cut motor torque to zero (fail-safe alternative braking via motor shutdown). SC independently detects the fault through heartbeat anomaly or cross-plausibility and may open kill relay.
- **ECU**: FZC (STM32G474RE)
- **Diagnostic coverage**: 85%
- **FTTI compliance**: Detection within 20 ms, reaction within 10 ms (motor cutoff via CAN). Total: 30 ms < 50 ms FTTI.
- **Traces to**: FSR-009, FSR-025

#### SM-012: Auto-Brake on CAN Timeout

- **Mechanism**: The FZC maintains an internal CAN message timeout counter for the brake command message from CVC. If no valid brake command (passing E2E check) is received within the timeout period (calibratable, default: 100 ms), the FZC autonomously applies maximum braking force. This ensures that even if the CVC fails or CAN communication is lost, the braking function defaults to a safe state.
- **Detection**: CAN bus failure, CVC processor hang, CAN transceiver failure, wiring fault.
- **Fault reaction**: Full brake applied autonomously by FZC. FZC broadcasts an "auto-brake active" CAN message (if bus is still available). System enters SAFE_STOP state. Motor torque is also cut via the CAN timeout mechanism on RZC (if RZC CAN is also affected, SC kill relay provides backup).
- **ECU**: FZC (STM32G474RE) — autonomous action, no dependency on CVC
- **Diagnostic coverage**: 95%
- **FTTI compliance**: Detection within 100 ms (timeout), reaction within 10 ms (servo command). Total: 110 ms. This exceeds the 50 ms FTTI for SG-004; however, the primary scenario (CAN loss) is a gradual degradation, and SM-011 provides faster detection for sudden brake actuator faults. The 100 ms timeout is a design tradeoff to avoid false triggers from normal CAN jitter.
- **Traces to**: FSR-010

#### SM-013: Motor Cutoff as Backup Deceleration

- **Mechanism**: If braking is lost (SM-011 detects brake fault), the CVC commands the RZC to set motor torque to zero and disable the motor driver. On a bench demo with a DC motor, removing drive torque provides deceleration through motor back-EMF and mechanical friction. This is not equivalent to active braking but provides a secondary deceleration path.
- **Detection**: Triggered by brake fault detection (SM-011) or CAN loss to FZC.
- **Fault reaction**: Motor disabled, system enters SAFE_STOP. SC kill relay provides tertiary backup if CVC-RZC communication also fails.
- **ECU**: CVC (command), RZC (execution), SC (backup via kill relay)
- **Diagnostic coverage**: 90% (motor cutoff is reliable; limited deceleration capability is the constraint)
- **FTTI compliance**: Within 50 ms when initiated by CVC command.
- **Traces to**: FSR-025

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC5 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4.4 provides 3 safety mechanisms for SG-004 (ASIL D): SM-011 (brake command monitoring), SM-012 (auto-brake on CAN timeout), and SM-013 (motor cutoff as backup deceleration). The defense-in-depth strategy is sound: SM-011 detects brake actuator faults (30 ms, within FTTI), SM-012 handles CAN communication loss (110 ms, exceeds 50 ms FTTI -- acknowledged as a design tradeoff), and SM-013 provides backup deceleration via motor cutoff. The auto-brake on CAN timeout (SM-012) is a critical autonomous action by the FZC -- it operates independently of the CVC, which is important when the CVC itself may have failed. The motor cutoff backup (SM-013) correctly acknowledges that motor back-EMF provides limited deceleration compared to active braking. The 100 ms CAN timeout for SM-012 exceeding the 50 ms FTTI is a legitimate design tradeoff to avoid false triggers.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC5 -->

### 4.5 SG-005: Prevent Unintended Braking (ASIL A)

Unintended braking on a bench demo is a low-severity hazard (ASIL A). The primary risk is unexpected mechanical stress or demonstration disruption.

#### SM-014: Brake Command Plausibility

- **Mechanism**: The FZC validates all incoming brake commands against the current vehicle state. If the vehicle is in INIT or OFF mode and a brake command is received, the command is rejected (no braking expected during initialization). Additionally, the brake command value is range-checked (0% to 100% braking force).
- **Detection**: Spurious brake command from CAN corruption, software fault in CVC, or message insertion.
- **Fault reaction**: Invalid brake command discarded. DTC logged. If invalid commands persist for more than 5 consecutive cycles, FZC requests CVC to transition to DEGRADED mode.
- **ECU**: FZC (STM32G474RE)
- **Diagnostic coverage**: 80%
- **FTTI compliance**: Detection within 10 ms, reaction within 10 ms. Total: 20 ms < 200 ms FTTI.
- **Traces to**: FSR-009

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC6 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4.5 provides 1 safety mechanism for SG-005 (ASIL A): SM-014 (brake command plausibility). At ASIL A, a single mechanism with 80% diagnostic coverage is sufficient. The mechanism validates brake commands against the current vehicle state and range-checks the brake command value. The characterization as "low-severity hazard" referencing "bench demo" is again inconsistent with assumption A-001 and should reference the S/E/C ratings instead.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC6 -->

### 4.6 SG-006: Ensure Motor Protection — Overcurrent/Overtemp (ASIL A)

Motor protection prevents hardware damage and secondary hazards (fire, fumes). ASIL A requirements apply.

#### SM-015: Motor Temperature Monitoring and Derating

- **Mechanism**: The RZC reads motor winding temperature via NTC thermistors on ADC1_CH2 and ADC1_CH3 at 100 Hz. A multi-point derating curve reduces maximum allowed motor current as temperature increases:
  - T < 60C: 100% rated current
  - 60C <= T < 80C: 75% rated current (DEGRADED warning)
  - 80C <= T < 100C: 50% rated current (LIMP mode)
  - T >= 100C: Motor disabled (SAFE_STOP)
  - Sensor short/open circuit (reading < -30C or > 150C): Motor disabled (sensor fault)
- **Detection**: Motor overheating, winding degradation, cooling failure, ambient overtemperature.
- **Fault reaction**: Progressive derating reduces thermal stress. At shutdown threshold, motor is disabled and a DTC is logged. Motor re-enable requires temperature to drop below the recovery threshold (hysteresis: 10C below activation threshold) and manual acknowledgment.
- **ECU**: RZC (STM32G474RE)
- **Diagnostic coverage**: 90% (dual NTC sensors with plausibility check)
- **FTTI compliance**: Detection within 100 ms, reaction within 10 ms. Total: 110 ms < 500 ms FTTI.
- **Traces to**: FSR-005

#### SM-016: Motor Current Limiting

- **Mechanism**: The RZC implements a software current limiter that adjusts PWM duty cycle to maintain motor current below the rated maximum. The current control loop runs at 1 kHz using ACS723 ADC readings as feedback. The current limit is further reduced by the derating curve (SM-015). Hardware backup: the BTS7960 has built-in overcurrent shutdown at approximately 43A (well above the software limit of 25A).
- **Detection**: Excessive current draw from motor stall, short circuit, or load increase.
- **Fault reaction**: PWM duty cycle reduced to maintain current within limits. If current cannot be controlled within 50 ms (indicating a hardware fault), motor is disabled.
- **ECU**: RZC (STM32G474RE)
- **Diagnostic coverage**: 95%
- **FTTI compliance**: Control loop response within 1 ms. Total: well within 500 ms FTTI.
- **Traces to**: FSR-004

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC7 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4.6 provides 2 safety mechanisms for SG-006 (ASIL A): SM-015 (motor temperature monitoring with derating) and SM-016 (motor current limiting). The multi-point derating curve in SM-015 is a well-designed progressive response that reduces motor stress before reaching the shutdown threshold. The hysteresis (10C below activation threshold) prevents oscillation between derating levels. SM-016 implements a 1 kHz current control loop with BTS7960 hardware backup at 43A as a diverse secondary protection. Both mechanisms operate well within the 500 ms FTTI. The dual NTC sensors with plausibility check (SM-015) provide appropriate diagnostic coverage for ASIL A.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC7 -->

### 4.7 SG-007: Ensure Obstacle Detection (ASIL C)

Obstacle detection enables emergency braking to prevent collisions. ASIL C requires robust sensor monitoring and plausibility checks.

#### SM-017: Lidar Distance Monitoring

- **Mechanism**: The FZC reads the TFMini-S lidar sensor via USART1 at 100 Hz. Distance readings are compared against three graduated thresholds:
  - Warning zone (calibratable, default: 100 cm): Buzzer warning, CAN alert
  - Braking zone (calibratable, default: 50 cm): Automatic speed reduction via CAN command
  - Emergency zone (calibratable, default: 20 cm): Emergency brake request, motor cutoff request
- **Detection**: Obstacle in vehicle path at various distances.
- **Fault reaction**: Graduated response per threshold. Emergency zone triggers immediate brake application and motor cutoff request via CAN.
- **ECU**: FZC (STM32G474RE)
- **Diagnostic coverage**: 85% (limited by lidar field of view and environmental conditions)
- **FTTI compliance**: Detection within 10 ms (one lidar sample), CAN transmission within 10 ms, CVC/RZC reaction within 10 ms. Total: 30 ms < 200 ms FTTI.
- **Traces to**: FSR-011

#### SM-018: Lidar Signal Plausibility Check

- **Mechanism**: The FZC performs three plausibility checks on lidar data:
  1. **Range check**: Distance must be within valid range (2 cm to 1200 cm for TFMini-S). Readings outside this range are flagged as invalid.
  2. **Stuck sensor detection**: If the distance reading does not change by more than 1 cm over 50 consecutive samples (500 ms), the sensor is flagged as potentially stuck.
  3. **Signal strength check**: TFMini-S reports signal strength (0-65535). Readings with signal strength below the minimum threshold (calibratable, default: 100) are discarded as unreliable.
  4. **Timeout monitoring**: If no valid UART frame is received within 100 ms, a sensor communication fault is declared.
- **Detection**: Sensor failure (stuck, disconnected, obstructed), communication fault, environmental interference (rain, dust, sunlight).
- **Fault reaction**: On plausibility failure, the FZC substitutes a safe default distance (0 cm = obstacle present) to trigger protective braking. A "lidar degraded" DTC is logged. If the fault persists for more than 2 seconds, the system transitions to DEGRADED mode with speed limiting.
- **ECU**: FZC (STM32G474RE)
- **Diagnostic coverage**: 90%
- **FTTI compliance**: Detection within 100 ms (timeout) or 500 ms (stuck detection). Total: within 200 ms FTTI for timeout scenarios; stuck detection at 500 ms exceeds FTTI but is a slow-developing fault not requiring emergency response.
- **Traces to**: FSR-012

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC8 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4.7 provides 2 safety mechanisms for SG-007 (ASIL C): SM-017 (lidar distance monitoring with graduated thresholds) and SM-018 (lidar signal plausibility check). The three-tier graduated response (warning at 100 cm, braking at 50 cm, emergency at 20 cm) is a well-designed approach. SM-018 implements four plausibility checks: range check, stuck sensor detection, signal strength check, and timeout monitoring. The safe default distance of 0 cm (obstacle present) on plausibility failure is the correct fail-safe approach. Note that the stuck sensor detection at 500 ms exceeds the 200 ms FTTI, but this is correctly characterized as a slow-developing fault not requiring emergency response. For ASIL C, the single lidar sensor without diverse redundancy is a limitation that should be documented as a residual risk.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC8 -->

### 4.8 SG-008: Ensure Independent Safety Monitoring (ASIL C)

The Safety Controller provides an independent, diverse monitoring layer. Its failure must be detected and result in a controlled system shutdown.

#### SM-019: Heartbeat Monitoring

- **Mechanism**: Each zone ECU (CVC, FZC, RZC) transmits a heartbeat CAN message at a fixed interval (CVC: 50 ms, FZC: 50 ms, RZC: 50 ms). Two independent monitoring paths exist:
  - **SC (backup)**: The SC monitors each heartbeat with independent timeout counters (100 ms timeout = 10 ticks × 10 ms). If timeout is reached, a confirmation window (30 ms = 3 ticks) prevents false triggers from CAN bus contention. After confirmation, the SC opens the kill relay. Recovery requires 3 consecutive heartbeats (debounce).
  - **CVC (primary)**: The CVC monitors FZC and RZC heartbeats using an AUTOSAR-inspired E2E State Machine with sliding window evaluation. Per-ECU thresholds: FZC 2-miss (100 ms), RZC 3-miss (150 ms). The E2E SM provides INIT/VALID/INVALID state transitions with configurable window-based recovery.
- **Detection**: ECU processor hang (watchdog not yet triggered), CAN transceiver failure, ECU power loss, software crash.
- **Fault reaction**: SC illuminates the fault LED for the specific ECU. If the failed ECU is safety-critical (CVC, FZC, or RZC), the SC opens the kill relay after confirmation. CVC independently transitions to DEGRADED/SAFE_STOP within one RTE cycle (10 ms) of detection. System enters SHUTDOWN state.
- **ECU**: SC (TMS570LC43x, CAN listen-only), CVC (STM32, E2E SM-based monitoring)
- **Diagnostic coverage**: 95%
- **FTTI compliance**: CVC primary path detects FZC failure within 100 ms (2 × 50 ms heartbeat interval), achieving SG-008 FTTI compliance. SC backup path: 100 ms timeout + 30 ms confirmation = 130 ms detection, ~10 ms relay reaction = 140 ms total. SC path exceeds 100 ms FTTI but provides independent hardware diversity. Mitigation: each ECU also has its own external watchdog (SM-020).
- **Traces to**: FSR-014, FSR-015

#### SM-020: External Watchdog per ECU (TPS3823)

- **Mechanism**: Each physical ECU (CVC, FZC, RZC, SC) has a TPS3823 external watchdog IC. The MCU firmware must toggle the WDI pin within the watchdog timeout period (calibratable via external capacitor, default: 1.6 seconds). If the firmware fails to toggle (processor hang, infinite loop, stack overflow), the TPS3823 asserts its RESET output, forcing a hardware reset of the MCU.
- **Detection**: Firmware hang, infinite loop, stack overflow, priority inversion, deadlock.
- **Fault reaction**: Hardware reset of the affected MCU. On reset, the MCU performs a full initialization and self-test sequence before resuming normal operation. During the reset period (approximately 50 ms), the MCU's CAN transmissions cease, which is detected by the SC heartbeat monitoring (SM-019) as a secondary notification.
- **ECU**: All physical ECUs (CVC, FZC, RZC, SC)
- **Diagnostic coverage**: 85% (detects firmware hangs but not all logical faults — firmware could be running but computing incorrect results)
- **FTTI compliance**: Watchdog timeout is 1.6 seconds, which exceeds all FTTI values. The external watchdog is a last-resort mechanism for faults not caught by faster software-level diagnostics. It prevents permanent system hang rather than providing FTTI-compliant response.
- **Traces to**: FSR-017

#### SM-021: SC Self-Test and Lockstep Monitoring

- **Mechanism**: The TMS570LC43x has hardware lockstep CPU cores — two cores execute the same instructions in lockstep, and a hardware comparator checks outputs every clock cycle. A mismatch causes an immediate CPU reset via the ESM (Error Signaling Module). Additionally, the SC firmware performs a self-test at startup: RAM BIST, flash CRC check, CAN controller self-test, GPIO readback verification, and watchdog test.
- **Detection**: CPU computation error (bit flip, SEU), RAM fault, flash corruption, CAN controller fault.
- **Fault reaction**: Lockstep mismatch triggers immediate CPU reset (hardware, < 1 us). Self-test failure at startup prevents the SC from energizing the kill relay (system remains in safe state — relay open). Self-test failure during runtime causes kill relay de-energize and system shutdown.
- **ECU**: SC (TMS570LC43x)
- **Diagnostic coverage**: 99% (lockstep cores provide the highest achievable hardware diagnostic coverage for computation errors)
- **FTTI compliance**: Lockstep comparison is immediate (every clock cycle). Self-test runs at startup and periodically (every 60 seconds for runtime checks).
- **Traces to**: FSR-017

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC9 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4.8 provides 3 safety mechanisms for SG-008 (ASIL C): SM-019 (heartbeat monitoring), SM-020 (external watchdog per ECU), and SM-021 (SC self-test and lockstep). SM-019 heartbeat monitoring at 205 ms total exceeds the 100 ms FTTI -- this is a known gap documented in the timing analysis. The mitigation via faster local mechanisms (SM-020, SM-021) is acknowledged. SM-020 (TPS3823 external watchdog) with 1.6 second timeout is explicitly characterized as a last-resort mechanism, not FTTI-compliant, which is honest and appropriate. SM-021 (TMS570 lockstep cores + ESM) provides the strongest hardware diagnostic at 99% coverage -- the lockstep comparison at every clock cycle is the gold standard for CPU fault detection. The diverse vendor approach (TI TMS570 vs. ST STM32) for the Safety Controller is a strong architectural decision for common cause failure mitigation. One concern: if all three mechanisms (SM-019, SM-020, SM-021) have timing that exceeds the 100 ms FTTI when considered individually, the composite timing argument must be made explicit in the safety case.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC9 -->

### 4.9 CVC State Machine (Cross-Cutting Safety Mechanism)

#### SM-022: Vehicle State Machine Management

- **Mechanism**: The CVC maintains the authoritative vehicle operating state via a deterministic state machine with the following states and transitions:

```
        ┌──────────────────────────────────────────────────────────────┐
        │                                                              │
  ┌─────▼─────┐    self-test    ┌─────────┐    all clear    ┌───────┐ │
  │    INIT    │───────OK──────▶│   RUN    │───────────────▶│ RUN   │ │
  └─────┬─────┘                 └────┬────┘                 └───┬───┘ │
        │                            │                          │     │
    self-test                   minor fault              major fault   │
     failed                         │                          │     │
        │                    ┌──────▼──────┐                   │     │
        │                    │  DEGRADED   │                   │     │
        │                    └──────┬──────┘                   │     │
        │                      persistent                      │     │
        │                       fault                          │     │
        │                    ┌──────▼──────┐                   │     │
        │                    │    LIMP     │◄──────────────────┘     │
        │                    └──────┬──────┘                         │
        │                     critical                               │
        │                      fault                                 │
        │                    ┌──────▼──────┐                         │
        └────────────────────▶  SAFE_STOP  │                         │
                             └──────┬──────┘                         │
                               controlled                            │
                               shutdown                              │
                             ┌──────▼──────┐     power-on restart    │
                             │  SHUTDOWN   ├─────────────────────────┘
                             └─────────────┘
```

- **Detection**: The state machine aggregates fault reports from all safety mechanisms via CAN (fault status messages from FZC, RZC) and local monitoring (pedal plausibility, E-stop GPIO).
- **Fault reaction**: Each state has defined operational limits:
  - RUN: Full performance, all functions active
  - DEGRADED: Speed limited to 50%, torque limited to 75%, warning displayed on OLED, buzzer warning
  - LIMP: Speed limited to 20%, torque limited to 30%, continuous buzzer, OLED shows LIMP warning
  - SAFE_STOP: Motor disabled, brakes applied, buzzer continuous, OLED shows STOP warning
  - SHUTDOWN: All actuators disabled, kill relay opened, DTCs persisted to flash
- **ECU**: CVC (STM32G474RE), with BswM mode management coordinating all ECUs
- **Traces to**: FSR-019, FSR-024

#### SM-023: E-Stop Broadcast

- **Mechanism**: The CVC monitors the E-stop button on GPIO PC13 (active low, hardware pullup, hardware debounce via RC filter). On E-stop activation (falling edge interrupt), the CVC immediately broadcasts a high-priority E-stop CAN message (highest priority CAN ID: 0x001) and simultaneously sets the local torque request to zero. All receiving ECUs (FZC, RZC) immediately transition to SAFE_STOP on receipt of the E-stop message.
- **Detection**: Hardware interrupt on GPIO — lowest latency detection path.
- **Fault reaction**: Immediate motor cutoff (RZC), brake application (FZC), all operations halted. System remains in SAFE_STOP until E-stop is released and a manual restart sequence is performed.
- **ECU**: CVC (detection and broadcast), all ECUs (reaction)
- **Diagnostic coverage**: 99% (hardwired button with hardware debounce — independent of software)
- **FTTI compliance**: Detection within 1 ms (interrupt latency), CAN transmission within 1 ms, reaction within 10 ms. Total: 12 ms < 50 ms FTTI.
- **Traces to**: FSR-018

#### SM-024: Standstill Torque Cross-Plausibility (Creep Guard)

- **Mechanism**: The SC independently monitors two CAN signals: CVC torque command (Torque_Request.TorqueRequest, CAN 0x101) and RZC motor current (Motor_Current.MotorCurrent_mA, CAN 0x301). If motor current exceeds a standstill threshold (e.g., 500 mA) while CVC torque command is zero for more than 2 consecutive SC cycles (20 ms), the SC detects a cross-plausibility violation indicating a hardware fault (BTS7960 FET short-circuit, PWM stuck at duty > 0, or motor controller enable stuck). The SC opens the kill relay (SM-005) to remove power from the motor driver, achieving SS-MOTOR-OFF.
- **Detection**: SC CAN-listen monitoring — compares torque command vs. actual current. Detects faults invisible to CVC software (downstream of the torque command output).
- **Fault reaction**: Kill relay opened (SM-005). Motor driver loses power supply. Vehicle transitions to SAFE_STOP. DTC 0xE312 (standstill creep fault) logged.
- **ECU**: SC (TMS570LC43x, CAN listen-only mode)
- **Diagnostic coverage**: 90% — covers BTS7960 FET short, PWM stuck faults, motor enable stuck. Does not cover motor back-drive from external forces (not an electrical fault).
- **FTTI compliance**: Detection within 20 ms (2 × 10 ms SC cycle), relay reaction within 5 ms. Total: 25 ms < 50 ms FTTI for SG-001.
- **Traces to**: FSR-026
- **Safety Goal**: SG-001 (prevents unintended vehicle motion from HE-017, HE-019)

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC10 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4.9 defines the cross-cutting mechanisms SM-022 (vehicle state machine) and SM-023 (E-stop broadcast). The state machine diagram is clear and shows all valid state transitions with entry conditions. SM-022 correctly defines operational limits per state (RUN, DEGRADED, LIMP, SAFE_STOP, SHUTDOWN). SM-023 (E-stop) achieves 12 ms total response time with a high-priority CAN ID (0x001), well within the 50 ms FTTI. The E-stop being hardware interrupt-driven (GPIO PC13 with hardware debounce) provides the lowest latency detection path. The state machine coordinates with BswM on all ECUs for synchronized degradation response (described in Section 5.3), which is an important AUTOSAR-like design pattern. One observation: the state machine allows DEGRADED to transition back to NORMAL after fault clearance and a 5-second recovery timer, but LIMP cannot transition directly to NORMAL -- this asymmetry is appropriate and conservative.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC10 -->

## 5. Warning and Degradation Concept

### 5.1 Warning Strategy

The warning concept implements a layered multi-channel notification strategy to inform the operator of system faults and degradation.

#### 5.1.1 Visual Warnings

| Channel | ECU | Purpose | Details |
|---------|-----|---------|---------|
| OLED display (SSD1306, 128x64) | CVC | Primary operator display | Shows current state (RUN/DEGRADED/LIMP/SAFE_STOP), fault codes, speed/torque limits. Updated every 100 ms. |
| Fault LED panel (4 LEDs) | SC | Independent fault indication | One LED per zone ECU (CVC, FZC, RZC) + one system LED. Steady = fault detected, blinking = heartbeat lost. Independent of CAN — driven directly by SC GPIO. |
| Status LEDs (2 per ECU) | CVC, FZC, RZC | Local ECU status | Green = running normally. Red = fault detected. Both off = ECU not powered. |
| ICU dashboard (simulated) | ICU (Docker) | Rich diagnostic display | Speed, torque, temperature gauges, DTC list, system state. For demonstration and monitoring only — not a safety function (QM). |

#### 5.1.2 Audible Warnings

| Pattern | Duration | Meaning | Trigger |
|---------|----------|---------|---------|
| Single short beep (100 ms) | Once | Information/acknowledgment | State transition to DEGRADED |
| Double beep (100 ms on, 100 ms off, 100 ms on) | Once | Warning | Sensor plausibility threshold approaching, obstacle in warning zone |
| Continuous slow beep (500 ms on, 500 ms off) | Repeating | Fault — LIMP mode | Any condition causing LIMP mode transition |
| Continuous fast beep (100 ms on, 100 ms off) | Repeating | Critical — SAFE_STOP | Any condition causing SAFE_STOP transition |
| Continuous solid tone | Until cleared | Emergency — obstacle in emergency zone | Lidar emergency zone detection, E-stop activated |

Buzzer location: FZC (piezo buzzer on GPIO). Driven by FZC firmware based on CAN state messages from CVC and local lidar detections.

#### 5.1.3 Haptic Warnings

Not applicable for this bench demonstration platform. In a production vehicle, steering wheel vibration, brake pedal pulsation, or seat vibration would be considered.

### 5.2 Degradation Levels

The system implements five operational levels with progressively restricted capabilities.

| Level | State | Motor | Steering | Braking | Lidar | Monitoring | Entry Criteria |
|-------|-------|-------|----------|---------|-------|------------|----------------|
| 0 | NORMAL | 100% torque, 100% speed | Full range, full rate | Full authority | Active, all thresholds | All systems nominal | Startup self-test passed, no active faults |
| 1 | DEGRADED | 75% torque, 50% speed | Full range, 50% rate | Full authority | Active, reduced thresholds | Warning active | Battery undervoltage warning, intermittent CAN error, temperature warning |
| 2 | LIMP | 30% torque, 20% speed | Center +/- 15 deg, 25% rate | Full authority | Active, emergency only | Continuous warning | Persistent sensor fault, dual sensor disagreement, repeated CAN errors |
| 3 | SAFE_STOP | Disabled (0%) | Disabled (servo off) | Full brake applied | Active | Emergency warning | Steering fault, brake fault, overcurrent, E-stop, safety goal violation, kill relay imminent |
| 4 | SHUTDOWN | Disabled, power cut | Disabled | Mechanical hold | Disabled | Off (LEDs last state) | Kill relay opened, controlled power-down complete |

#### 5.2.1 Degradation Transition Rules

1. **Forward only (mostly)**: State transitions generally move from NORMAL toward SHUTDOWN. The only reverse transition allowed is DEGRADED back to NORMAL, and only when all triggering faults have cleared AND a recovery timer has expired (calibratable, default: 5 seconds of fault-free operation).
2. **No skip-back**: A system in LIMP cannot transition directly to NORMAL. It must first transition to DEGRADED, then to NORMAL (each requiring fault clearance and recovery timer).
3. **Irreversible shutdown**: Once the system enters SHUTDOWN (kill relay opened), it cannot be restarted without a full power cycle and startup self-test.
4. **E-stop override**: The E-stop button transitions from any state directly to SAFE_STOP, bypassing intermediate states.
5. **SC authority**: The Safety Controller can force a transition from any state to SHUTDOWN by opening the kill relay. This authority is independent of the CVC state machine.

### 5.3 Coordinated Degradation via BswM

The BSW Mode Manager (BswM) on each ECU synchronizes the degradation response across the system:

1. CVC detects a fault and transitions the vehicle state machine (SM-022).
2. CVC transmits the new state on the CAN bus (vehicle state message, 10 ms cycle).
3. Each ECU's BswM module receives the state message and adjusts its local operating mode:
   - RZC BswM: Adjusts current limits and PWM limits per degradation level
   - FZC BswM: Adjusts servo limits, rate limits, and buzzer pattern per degradation level
   - BCM BswM: Activates hazard lights in DEGRADED and above
4. Each ECU acknowledges the mode transition via its heartbeat message (mode field).
5. The CVC verifies all ECUs have transitioned within 100 ms. If any ECU fails to acknowledge, the CVC escalates to the next degradation level.

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC11 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The warning and degradation concept (Section 5) is comprehensive. The multi-channel warning strategy covers visual (OLED, fault LEDs, status LEDs, ICU dashboard), audible (5 distinct buzzer patterns), and notes the absence of haptic warnings for the bench platform. The 5 degradation levels (NORMAL through SHUTDOWN) with specific operational limits per level (motor torque %, speed %, steering range, braking authority) provide precise guidance for implementation. The degradation transition rules are conservative and correct: forward-only transitions (except DEGRADED to NORMAL), no skip-back, irreversible shutdown, E-stop override from any state, and SC authority to force SHUTDOWN. The BswM coordination mechanism (Section 5.3) ensures all ECUs transition synchronously with a 100 ms acknowledgment timeout and escalation on failure. One observation: the degradation table shows "Full authority" for braking in all states except SHUTDOWN -- this means braking is never reduced, which is the correct approach since braking is always needed for safety.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC11 -->

## 6. Safety Mechanism Allocation Matrix

The following matrix shows which safety mechanisms are allocated to which ECU. "P" = Primary implementer, "S" = Secondary/backup, "M" = Monitor.

| Safety Mechanism | CVC | FZC | RZC | SC | Safety Goals Addressed |
|------------------|-----|-----|-----|----|----------------------|
| SM-001: Dual pedal plausibility | P | — | — | — | SG-001 |
| SM-002: Motor current overcurrent | — | — | P | — | SG-001, SG-006 |
| SM-003: Cross-plausibility torque/current | — | — | — | P | SG-001 |
| SM-004: CAN E2E protection | P | P | P | M | SG-001 to SG-008 |
| SM-005: Kill relay energize-to-run | — | — | — | P | SG-001, SG-003, SG-004, SG-008 |
| SM-006: Motor controller health | — | — | P | — | SG-002 |
| SM-007: CAN alive monitoring (RZC) | P | — | — | S | SG-002 |
| SM-008: Steering angle feedback | — | P | — | — | SG-003 |
| SM-009: Steering rate limiting | — | P | — | — | SG-003 |
| SM-010: Steering angle limits | — | P | — | — | SG-003 |
| SM-011: Brake command monitoring | — | P | — | — | SG-004 |
| SM-012: Auto-brake on CAN timeout | — | P | — | — | SG-004 |
| SM-013: Motor cutoff backup decel | P | — | P | S | SG-004 |
| SM-014: Brake command plausibility | — | P | — | — | SG-005 |
| SM-015: Motor temp derating | — | — | P | — | SG-006 |
| SM-016: Motor current limiting | — | — | P | — | SG-006 |
| SM-017: Lidar distance monitoring | — | P | — | — | SG-007 |
| SM-018: Lidar plausibility check | — | P | — | — | SG-007 |
| SM-019: Heartbeat monitoring | — | — | — | P | SG-008 |
| SM-020: External watchdog (TPS3823) | P | P | P | P | SG-008 |
| SM-021: SC self-test and lockstep | — | — | — | P | SG-008 |
| SM-022: Vehicle state machine | P | S | S | M | SG-001 to SG-008 |
| SM-023: E-stop broadcast | P | S | S | M | SG-001, SG-008 |
| SM-024: Standstill torque cross-plausibility | — | — | — | P | SG-001 |

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC12 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The safety mechanism allocation matrix (Section 6) provides a clear P/S/M assignment for all 23 safety mechanisms across the 4 ECUs. Key observations: SM-004 (CAN E2E) is correctly assigned as P to all physical ECUs and M to SC. SM-005 (kill relay) is P to SC only, which is correct for the energize-to-run pattern. SM-020 (external watchdog) is P to all physical ECUs, reflecting its per-ECU implementation. The "Safety Goals Addressed" column provides quick traceability. One observation: SM-022 (vehicle state machine) and SM-023 (E-stop) address "SG-001 to SG-008" and "SG-001, SG-008" respectively, which confirms these are cross-cutting mechanisms. The matrix correctly shows the SC as the only ECU with hardware-enforced independent safety authority (SM-005 kill relay).
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC12 -->

## 7. Safety Mechanism Timing Analysis

The following table verifies that each safety mechanism achieves its safe state within the applicable Fault Tolerant Time Interval (FTTI).

| Mechanism | Detection Time | Reaction Time | Total | FTTI | Margin | Compliant? |
|-----------|---------------|---------------|-------|------|--------|------------|
| SM-001: Dual pedal plausibility | 20 ms | 10 ms | 30 ms | 50 ms | 20 ms | Yes |
| SM-002: Motor current overcurrent | 10 ms | 1 ms | 11 ms | 50 ms | 39 ms | Yes |
| SM-003: Cross-plausibility | 50 ms | 6 ms (relay) | 56 ms | 50 ms | -6 ms | Marginal (Note 1) |
| SM-004: CAN E2E (torque) | 10 ms | 10 ms | 20 ms | 50 ms | 30 ms | Yes |
| SM-005: Kill relay | < 1 ms | 5 ms | 6 ms | 50 ms | 44 ms | Yes |
| SM-006: Motor controller health | 50 ms | 20 ms | 70 ms | 200 ms | 130 ms | Yes |
| SM-007: CAN alive (RZC) | 100 ms | 10 ms | 110 ms | 200 ms | 90 ms | Yes |
| SM-008: Steering angle feedback | 50 ms | 50 ms | 100 ms | 100 ms | 0 ms | Yes (boundary) |
| SM-009: Steering rate limiting | 10 ms | 0 ms (inline) | 10 ms | 100 ms | 90 ms | Yes |
| SM-010: Steering angle limits | 10 ms | 0 ms (inline) | 10 ms | 100 ms | 90 ms | Yes |
| SM-011: Brake command monitoring | 20 ms | 10 ms | 30 ms | 50 ms | 20 ms | Yes |
| SM-012: Auto-brake on CAN timeout | 100 ms | 10 ms | 110 ms | 50 ms | -60 ms | No (Note 2) |
| SM-013: Motor cutoff backup | 20 ms | 10 ms | 30 ms | 50 ms | 20 ms | Yes |
| SM-014: Brake plausibility | 10 ms | 10 ms | 20 ms | 200 ms | 180 ms | Yes |
| SM-015: Motor temp derating | 100 ms | 10 ms | 110 ms | 500 ms | 390 ms | Yes |
| SM-016: Motor current limiting | 1 ms | 1 ms | 2 ms | 500 ms | 498 ms | Yes |
| SM-017: Lidar distance | 10 ms | 10 ms | 20 ms | 200 ms | 180 ms | Yes |
| SM-018: Lidar plausibility | 100 ms | 10 ms | 110 ms | 200 ms | 90 ms | Yes |
| SM-019: Heartbeat monitoring (SC) | 130 ms | 10 ms | 140 ms | 100 ms | -40 ms | No (Note 3) |
| SM-020: Ext. watchdog (TPS3823) | 1600 ms | 50 ms | 1650 ms | 100 ms | -1550 ms | No (Note 4) |
| SM-021: SC self-test/lockstep | < 0.001 ms | < 0.001 ms | < 0.001 ms | 100 ms | ~100 ms | Yes |
| SM-022: Vehicle state machine | 10 ms | 10 ms | 20 ms | 50 ms | 30 ms | Yes |
| SM-023: E-stop broadcast | 1 ms | 10 ms | 11 ms | 50 ms | 39 ms | Yes |
| SM-024: Standstill cross-plausibility | 20 ms | 5 ms (relay) | 25 ms | 50 ms | 25 ms | Yes |

### Timing Notes

**Note 1 — SM-003 (Cross-plausibility)**: Total time (56 ms) marginally exceeds the 50 ms FTTI for SG-001. This is acceptable because SM-003 is a tertiary defense mechanism. SM-001 (dual pedal plausibility, 30 ms) and SM-002 (overcurrent cutoff, 11 ms) provide FTTI-compliant primary and secondary coverage for SG-001. SM-003 addresses residual faults not caught by the primary mechanisms.

**Note 2 — SM-012 (Auto-brake on CAN timeout)**: The 100 ms timeout exceeds the 50 ms FTTI for SG-004 (loss of braking). This is a design tradeoff: a shorter timeout would cause false triggers due to normal CAN bus jitter. SM-011 (brake command monitoring, 30 ms) provides primary FTTI-compliant detection for brake actuator faults. SM-012 specifically addresses CAN communication loss, which is a gradual degradation scenario where the full 100 ms timeout is justified.

**Note 3 — SM-019 (Heartbeat monitoring)**: The SC backup path (140 ms total) exceeds the 100 ms FTTI for SG-008. However, the CVC primary detection path achieves FTTI compliance: FZC failure is detected within 100 ms (2-miss E2E State Machine window × 50 ms heartbeat interval), with CVC SAFE_STOP transition within 110 ms. The SC path was reduced from 205 ms to 140 ms through timeout tuning (150→100 ms) and confirmation window reduction (50→30 ms). The SC provides independent hardware-diverse backup monitoring. Together, the CVC (FTTI-compliant primary) and SC (independent backup) provide defense-in-depth heartbeat monitoring for SG-008.

**Note 4 — SM-020 (External watchdog)**: The TPS3823 timeout (1.6 seconds) is intentionally long to avoid false resets during normal operation. It is not intended to provide FTTI-compliant response. It is a last-resort mechanism that prevents permanent firmware hang. Faster software-level diagnostics (SM-001 through SM-019) provide FTTI-compliant coverage for all safety goals.

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC13 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The timing analysis table (Section 7) is a critical verification artifact. Of the 23 safety mechanisms, 4 are flagged as non-compliant with their FTTI: SM-003 (marginal, -6 ms), SM-012 (-60 ms), SM-019 (-105 ms), and SM-020 (-1550 ms). Each non-compliance is documented with a timing note explaining the mitigation strategy. The key argument is that these are secondary/tertiary mechanisms complemented by faster primary mechanisms. This argument is valid for SM-003 (backed by SM-001 at 30 ms) and SM-012 (backed by SM-011 at 30 ms). For SM-019 and SM-020, the argument is weaker since they are the primary mechanisms for SG-008 -- the composite timing argument must demonstrate that at least one mechanism can achieve safe state within 100 ms for any SG-008 fault scenario. SM-021 (lockstep, near-zero latency) covers CPU faults but not all SG-008 fault types (e.g., CAN bus total failure). SM-008 timing is noted as "boundary" at exactly 100 ms with 0 ms margin -- this should be validated on target hardware.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC13 -->

## 8. Freedom from Interference (FFI) Summary

Per ISO 26262-4, elements of different ASIL levels sharing resources must demonstrate freedom from interference. The following FFI mechanisms are employed:

| Shared Resource | ASIL Levels | FFI Mechanism |
|----------------|-------------|---------------|
| CAN bus | ASIL D + ASIL A + QM | E2E protection on safety messages, CAN ID priority assignment (highest priority for ASIL D messages), SC independent monitoring in listen-only mode |
| CVC MCU (pedal ASIL D + OLED QM) | D + QM | MPU-enforced memory partitioning between safety-critical and QM tasks, separate task priorities, temporal isolation via RTOS scheduling |
| FZC MCU (brake ASIL D + buzzer QM) | D + QM + ASIL C | MPU-enforced memory partitioning, separate task priorities, QM function cannot starve safety tasks |
| RZC MCU (motor + current + temp) | D + A | MPU-enforced partitioning, independent ADC channels, separate interrupt priorities |
| 12V power rail | All | Independent voltage regulators per ECU, brown-out detection, SC power-independent (separate supply path from kill relay to SC) |
| SC (independent monitor) | ASIL C (inherits) | Diverse vendor (TI vs. ST), diverse architecture (lockstep vs. single core), CAN listen-only (cannot corrupt bus), independent power path |

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC14 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The FFI summary (Section 8) identifies the key shared resources and their FFI mechanisms: CAN bus (E2E + priority + SC listen-only), MCU partitioning (MPU-enforced memory isolation), 12V power rail (independent voltage regulators + brown-out detection), and SC independence (diverse vendor TI vs. ST, diverse architecture lockstep vs. single core, CAN listen-only). The FFI mechanisms are appropriate for the identified interference types (spatial, temporal, communication). One critical assumption: "MPU-enforced memory partitioning" is listed for CVC, FZC, and RZC -- this requires the STM32G474RE MPU to be correctly configured, which is documented as assumption FSC-A-001. The MPU configuration must be verified through testing (fault injection) as part of the FFI evidence. The SC's "independent power path from kill relay to SC" is important -- the SC must remain powered even when it opens the kill relay to continue monitoring and displaying fault status.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC14 -->

## 9. Assumptions and Open Items

### 9.1 Assumptions

| ID | Assumption | Impact on FSC |
|----|-----------|---------------|
| FSC-A-001 | STM32G474RE MPU is correctly configured to enforce memory isolation | FFI between ASIL D and QM tasks on CVC/FZC/RZC depends on this |
| FSC-A-002 | CAN bus electrical integrity is maintained (proper termination, shielding) | E2E protection is designed for random bit errors, not systematic wiring faults |
| FSC-A-003 | TPS3823 external watchdog timeout is correctly configured via external capacitor | Last-resort watchdog function depends on correct RC time constant |
| FSC-A-004 | Kill relay contacts do not weld closed under normal operating currents | Relay stuck-closed is a residual risk addressed in FMEA |
| FSC-A-005 | BTS7960 built-in overcurrent protection activates before motor damage occurs | Hardware backup for software current limiting |

### 9.2 Open Items

| ID | Item | Target Resolution |
|----|------|-------------------|
| FSC-O-001 | Verify SM-003 cross-plausibility timing can be reduced to meet 50 ms FTTI | TSC phase — optimize SC processing or reduce CAN cycle time |
| FSC-O-002 | Validate external watchdog timeout value on hardware | Hardware integration testing |
| FSC-O-003 | Determine if SM-012 auto-brake timeout should be reduced from 100 ms | TSC phase — CAN bus jitter analysis |
| FSC-O-004 | Verify kill relay dropout time on actual hardware | Hardware integration testing |
| FSC-O-005 | Define detailed CAN message schedule (IDs, priorities, cycle times) | TSC phase |

<!-- HITL-LOCK START:COMMENT-BLOCK-FSC-SEC15 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The assumptions (Section 9.1) and open items (Section 9.2) are transparently documented. FSC-A-001 (MPU configuration) is the highest-impact assumption since FFI between ASIL D and QM tasks depends on it. FSC-A-004 (relay contact welding) identifies the most significant residual risk in the kill relay design -- relay stuck-closed is the single failure that defeats the hardware fail-safe. The 5 open items are all targeted for TSC phase or hardware integration testing, which is appropriate at the FSC stage. FSC-O-001 (reduce SM-003 cross-plausibility timing to meet 50 ms FTTI) and FSC-O-003 (reduce SM-012 auto-brake timeout) are the most important open items for FTTI compliance. Overall, the FSC is a thorough and well-structured document that covers all 8 safety goals with 23 safety mechanisms, appropriate diagnostic coverage levels, timing analysis with honest non-compliance documentation, and clear FFI considerations.
<!-- HITL-LOCK END:COMMENT-BLOCK-FSC-SEC15 -->

## 10. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete functional safety concept: 23 safety mechanisms across 8 safety goals, warning/degradation concept, allocation matrix, timing analysis, FFI summary |
| 1.1 | 2026-02-26 | System | SM-008 fault reaction updated: steer fault → SAFE_STOP (not DEGRADED) — no mechanical fallback steering on this platform. Degradation table updated accordingly. |

