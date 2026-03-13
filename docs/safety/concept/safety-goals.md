---
document_id: SG
title: "Safety Goals"
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

Every safety goal (SG-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/safety/lessons-learned/`](../lessons-learned/). One file per safety goal (SG-NNN). File naming: `SG-NNN-<short-title>.md`.


# Safety Goals

## 1. Purpose

This document defines the safety goals for the Taktflow Zonal Vehicle Platform, derived from the Hazard Analysis and Risk Assessment (HARA, document HARA v0.1). Safety goals are the top-level safety requirements of the item per ISO 26262-3:2018, Clause 8. Each safety goal specifies a condition that must be maintained or achieved to prevent or mitigate one or more hazardous events.

This document also defines the safe states, Fault Tolerant Time Intervals (FTTI), and the complete traceability from hazardous events to safety goals.

## 2. Scope

The safety goals cover all hazardous events identified in the HARA for the Taktflow Zonal Vehicle Platform item as defined in the item definition (ITEM-DEF v1.0). The item comprises 7 ECU nodes (4 physical + 3 simulated), sensors, actuators, and a CAN bus network providing drive-by-wire, steering, braking, distance sensing, and independent safety monitoring functions.

**Assumption**: Per assumption A-001 in the item definition, Severity, Exposure, and Controllability ratings assume the platform controls a real vehicle. This is intentional for demonstrating full ISO 26262 competence.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG0 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Sections 1-3 provide appropriate context. The purpose statement correctly identifies this document as implementing ISO 26262-3 Clause 8. The reference table lists HARA v0.1 but the HARA document metadata shows v1.0 -- this version reference should be updated for consistency. The scope correctly identifies the item and the foundational assumption A-001.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG0 -->

## 3. References

| Document ID | Title | Version |
|-------------|-------|---------|
| ITEM-DEF | Item Definition | 1.0 |
| HARA | Hazard Analysis and Risk Assessment | 0.1 |
| FSC | Functional Safety Concept | 0.1 |
| ISO 26262-3:2018 | Road vehicles -- Functional safety -- Part 3: Concept phase | 2018 |
| ISO 26262-1:2018 | Road vehicles -- Functional safety -- Part 1: Vocabulary | 2018 |

## 4. Hazardous Events Summary

The following hazardous events were identified in the HARA. They are grouped into 8 safety goals based on functional domain and failure mode similarity.

| HE-ID | Hazardous Event | ASIL | Assigned SG |
|-------|-----------------|------|-------------|
| HE-001 | Unintended acceleration (pedal sensors both read high incorrectly) | ASIL D | SG-001 |
| HE-002 | Loss of drive torque (motor stops unexpectedly) | ASIL B | SG-002 |
| HE-003 | Unintended steering movement | ASIL C | SG-003 |
| HE-004 | Loss of steering during turning | ASIL D | SG-003 |
| HE-005 | Loss of braking during braking | ASIL D | SG-004 |
| HE-006 | Unintended braking during driving | ASIL A | SG-005 |
| HE-007 | Motor overcurrent not detected | ASIL A | SG-006 |
| HE-008 | Motor overtemperature not detected | QM | SG-006 |
| HE-009 | Lidar false negative (obstacle not detected) | ASIL C | SG-007 |
| HE-010 | Lidar false positive (phantom braking) | ASIL A | SG-005 |
| HE-011 | CAN bus total failure | ASIL C | SG-008 |
| HE-012 | Safety Controller failure (loss of monitoring) | ASIL B | SG-008 |
| HE-013 | E-stop not functional | ASIL B | SG-008 |
| HE-014 | Unintended motor reversal | ASIL C | SG-008 |
| HE-015 | Battery overvoltage/undervoltage | QM | SG-006 |
| HE-016 | Unintended acceleration at high speed (pedal sensors both read high) | ASIL C | SG-001 |
| HE-017 | Unintended vehicle motion from rest (motor runs at full power uncontrolled) | ASIL D | SG-001 |
| HE-018 | Unintended forward motion during reversing (motor direction reversal) | ASIL A | SG-008 |
| HE-019 | Motor enable stuck (motor cannot be stopped despite zero torque request) | ASIL C | SG-001 |
| HE-020 | CAN bus babbling node (safety-critical messages blocked) | ASIL B | SG-008 |

<!-- HITL-LOCK START:COMMENT-BLOCK-SG1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The hazardous events summary table lists HE-001 through HE-015 but omits HE-016 through HE-020, which are defined in the HARA Section 6.2. This is a traceability gap: the HARA identifies 20 hazardous events, but this SG document only traces 15. The missing events are: HE-016 (unintended acceleration at high speed, ASIL C), HE-017 (unintended vehicle motion from rest, ASIL D), HE-018 (unintended forward motion during reversing, ASIL A), HE-019 (motor enable stuck, ASIL C), and HE-020 (CAN bus babbling node, ASIL B). These must be mapped to safety goals. HE-017 is particularly critical as it is ASIL D and should be assigned to SG-001 or a new SG. The HARA safety goals preview (Section 9.3) actually defines 13 safety goals including SG-004 for HE-017, but this SG document reduces to 8 by grouping differently. The grouping rationale should be documented.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG1 -->

## 5. Safety Goals

### 5.1 Safety Goals Table

| SG-ID | Safety Goal | ASIL | Safe State | FTTI | Source HE |
|-------|-------------|------|------------|------|-----------|
| SG-001 | The system shall prevent unintended vehicle acceleration or motion due to motor control malfunction or erroneous pedal sensor readings. | ASIL D | SS-MOTOR-OFF | 50 ms | HE-001, HE-016, HE-017, HE-019 |
| SG-002 | The system shall prevent unintended loss of drive torque during vehicle operation. | ASIL B | SS-CONTROLLED-STOP | 200 ms | HE-002 |
| SG-003 | The system shall prevent unintended steering movement and ensure steering availability during turning manoeuvres. | ASIL D | SS-MOTOR-OFF | 100 ms | HE-003, HE-004 |
| SG-004 | The system shall prevent unintended loss of braking capability during braking operations. | ASIL D | SS-MOTOR-OFF | 50 ms | HE-005 |
| SG-005 | The system shall prevent unintended braking events during normal driving. | ASIL A | SS-CONTROLLED-STOP | 200 ms | HE-006, HE-010 |
| SG-006 | The system shall ensure motor protection against overcurrent, overtemperature, and supply voltage excursion. | ASIL A | SS-MOTOR-OFF | 500 ms | HE-007, HE-008, HE-015 |
| SG-007 | The system shall ensure timely detection of obstacles by the distance sensing function. | ASIL C | SS-CONTROLLED-STOP | 200 ms | HE-009 |
| SG-008 | The system shall ensure availability of independent safety monitoring, emergency stop, and protection against unintended motor reversal. | ASIL C | SS-SYSTEM-SHUTDOWN | 100 ms | HE-011, HE-012, HE-013, HE-014, HE-018, HE-020 |

### 5.2 Safety Goal Details

#### SG-001: Prevent Unintended Acceleration

- **Safety Goal**: The system shall prevent unintended vehicle acceleration or motion due to motor control malfunction or erroneous pedal sensor readings.
- **ASIL**: D
- **Source**: HE-001 (Unintended acceleration, ASIL D), HE-016 (Unintended acceleration at high speed, ASIL C), HE-017 (Unintended vehicle motion from rest, ASIL D), HE-019 (Motor enable stuck, ASIL C)
- **Safe State**: SS-MOTOR-OFF -- Motor torque = 0, H-bridge disabled, brakes applied.
- **FTTI**: 50 ms
- **Rationale**: Unintended acceleration at any vehicle speed can result in life-threatening consequences (S3). The dual redundant pedal sensors (AS5048A on SPI1 with separate chip selects) enable plausibility checking. If both sensors simultaneously read high due to a common cause fault (e.g., power supply corruption, SPI bus fault), the system must detect the anomaly and transition to the safe state within the FTTI. The ASIL D rating demands the highest level of diagnostic coverage and freedom from interference.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG2 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SG-001 is well-defined with a clear safety goal statement, ASIL D assignment, and SS-MOTOR-OFF safe state. The 50 ms FTTI is aggressive but justified in Section 7.2 with a detailed timing budget. The source traces to HE-001 only, but should also reference HE-016 (unintended acceleration at high speed, ASIL C) and HE-017 (unintended vehicle motion from rest, ASIL D) per the HARA safety goals preview. The safe state (motor off, brakes applied) is well-defined and achievable independently by both the RZC (local motor disable) and SC (kill relay). The rationale correctly identifies the common cause fault scenario for dual sensors sharing SPI1.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG2 -->

#### SG-002: Prevent Unintended Loss of Drive Torque

- **Safety Goal**: The system shall prevent unintended loss of drive torque during vehicle operation.
- **ASIL**: B
- **Source**: HE-002 (Loss of drive torque, ASIL B)
- **Safe State**: SS-CONTROLLED-STOP -- Motor ramps down gradually, brakes applied, steering locked to last commanded position.
- **FTTI**: 200 ms
- **Rationale**: Sudden loss of drive torque in traffic can cause a rear-end collision (S2). The system must either maintain torque delivery or transition to a controlled stop. A sudden motor cutoff is not an acceptable response; the safe state requires a controlled deceleration ramp. The ASIL B rating reflects moderate severity with moderate exposure.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SG-002 is appropriate at ASIL B with a controlled stop safe state. The 200 ms FTTI provides adequate margin for the less-critical loss-of-torque scenario. The rationale correctly notes that a sudden motor cutoff is NOT an acceptable response -- the controlled deceleration ramp is important to prevent rear-end collisions. The safe state definition (SS-CONTROLLED-STOP) with "steering locked to last commanded position" should be reconsidered if the steering fault also occurs simultaneously, though this is a compound failure scenario.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG3 -->

#### SG-003: Prevent Unintended Steering Movement

- **Safety Goal**: The system shall prevent unintended steering movement and ensure steering availability during turning manoeuvres.
- **ASIL**: D (highest from HE-003 ASIL C and HE-004 ASIL D)
- **Source**: HE-003 (Unintended steering movement, ASIL C), HE-004 (Loss of steering during turning, ASIL D)
- **Safe State**: SS-MOTOR-OFF -- Motor torque = 0, H-bridge disabled, brakes applied, steering servo disabled.
- **FTTI**: 100 ms
- **Rationale**: These two hazardous events are grouped because they share the same functional domain (steering control) and the same safe state. Loss of steering during a turn (HE-004, ASIL D) is the more severe scenario -- the vehicle departs its lane with potentially fatal consequences (S3, E4, C3). Unintended steering movement (HE-003, ASIL C) can also cause lane departure. The safety goal inherits ASIL D from HE-004. The platform has no mechanical fallback steering and no redundant steering actuator -- if steering is faulted, the vehicle cannot be controlled regardless of speed. The safe state is therefore SS-MOTOR-OFF (motor off, brakes applied, vehicle stopped) rather than a degraded-speed approach, which would require a functioning backup steering path.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SG-003 correctly inherits ASIL D from HE-004 (loss of steering during turning). The grouping of HE-003 and HE-004 is logical since both relate to steering control integrity. The updated safe state (SS-MOTOR-OFF, changed from SS-STEER-CENTER in v1.1) is the correct decision for this platform -- without mechanical fallback steering, degraded-speed operation is not viable. The rationale explicitly documents why SAFE_STOP is chosen over degraded operation, which is excellent engineering judgment. The 100 ms FTTI accounts for servo mechanical response time, which is physically grounded.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG4 -->

#### SG-004: Prevent Unintended Loss of Braking

- **Safety Goal**: The system shall prevent unintended loss of braking capability during braking operations.
- **ASIL**: D
- **Source**: HE-005 (Loss of braking during braking, ASIL D)
- **Safe State**: SS-MOTOR-OFF -- Motor torque = 0 (fail-safe), H-bridge disabled.
- **FTTI**: 50 ms
- **Rationale**: Loss of braking during an active braking manoeuvre can result in collision with a leading vehicle or obstacle, with life-threatening consequences (S3, E4, C3). The safe state removes drive torque as an immediate mitigation -- without motor propulsion, the vehicle decelerates through friction and drag. This is a fail-safe approach where removing the energy source is the primary containment action. The 50 ms FTTI reflects the critical nature of braking loss at speed.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG5 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SG-004 addresses the critical loss-of-braking scenario at ASIL D. The safe state (SS-MOTOR-OFF) is a fail-safe approach: removing drive torque as the primary mitigation when the brake actuator fails. This is the correct strategy for a single-actuator brake system with no mechanical fallback. The 50 ms FTTI is tight but justified by the immediate danger of continued driving without braking. One consideration: the safe state description says "Motor torque = 0 (fail-safe), H-bridge disabled" but does not mention brake application -- since the brake servo has failed (that is the triggering fault), the safe state cannot include brake application. This should be explicitly noted in the safe state description to avoid confusion with SS-MOTOR-OFF for SG-001 which does include brake application.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG5 -->

#### SG-005: Prevent Unintended Braking

- **Safety Goal**: The system shall prevent unintended braking events during normal driving.
- **ASIL**: A (highest from HE-006 ASIL A and HE-010 ASIL A)
- **Source**: HE-006 (Unintended braking during driving, ASIL A), HE-010 (Lidar false positive / phantom braking, ASIL A)
- **Safe State**: SS-CONTROLLED-STOP -- Brake command released, motor ramp-down if needed.
- **FTTI**: 200 ms
- **Rationale**: Unintended braking causes an unexpected deceleration that may surprise following traffic (S1-S2, depending on deceleration magnitude). Both hardware-caused unintended braking (HE-006, brake servo fault) and software-caused unintended braking (HE-010, lidar false positive triggering emergency brake) are grouped here. The safe state releases the erroneous brake command and allows controlled deceleration. The ASIL A rating reflects lower severity with moderate controllability by following drivers.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG6 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SG-005 groups unintended braking from two sources: hardware (HE-006, brake servo fault) and software (HE-010, lidar false positive). Both are ASIL A, so the grouping does not require ASIL inheritance escalation. The 200 ms FTTI is reasonable for a nuisance scenario rather than an immediately life-threatening one. The safe state (release brake command, controlled stop) is appropriate.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG6 -->

#### SG-006: Ensure Motor Protection

- **Safety Goal**: The system shall ensure motor protection against overcurrent, overtemperature, and supply voltage excursion.
- **ASIL**: A (highest from HE-007 ASIL A, HE-008 QM, HE-015 QM)
- **Source**: HE-007 (Motor overcurrent not detected, ASIL A), HE-008 (Motor overtemperature not detected, QM), HE-015 (Battery overvoltage/undervoltage, QM)
- **Safe State**: SS-MOTOR-OFF -- Motor torque = 0, H-bridge disabled.
- **FTTI**: 500 ms
- **Rationale**: These three hazardous events are grouped because they all relate to motor and power subsystem protection. Undetected overcurrent (HE-007) can lead to H-bridge damage, cable heating, or fire, with ASIL A severity. Overtemperature (HE-008) and battery voltage excursion (HE-015) are QM-rated events whose consequences are primarily property damage. The safety goal inherits ASIL A from HE-007. The 500 ms FTTI reflects the thermal time constants of the motor and wiring -- overcurrent damage does not occur instantaneously due to the thermal mass of conductors.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG7 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SG-006 groups three motor/power protection events with the ASIL inherited from HE-007 (ASIL A). The grouping is logical since all three relate to motor subsystem protection. The 500 ms FTTI is well-justified by thermal time constants -- this is the longest FTTI in the system and appropriately reflects the gradual nature of thermal hazards. The safe state (motor off, H-bridge disabled) is correct for protection against overcurrent and thermal damage.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG7 -->

#### SG-007: Ensure Obstacle Detection

- **Safety Goal**: The system shall ensure timely detection of obstacles by the distance sensing function.
- **ASIL**: C
- **Source**: HE-009 (Lidar false negative, ASIL C)
- **Safe State**: SS-CONTROLLED-STOP -- Controlled stop initiated, hazard lights activated.
- **FTTI**: 200 ms
- **Rationale**: A lidar false negative means an obstacle is present but not detected, and the vehicle does not brake. At moderate speed, this can result in a collision with an obstacle or pedestrian (S3, E3, C2). The safe state initiates a controlled stop when the lidar sensor is determined to be unreliable (stuck value, timeout, or diagnostic failure). The 200 ms FTTI accounts for the time needed to confirm a sensor fault versus a genuine clear path, plus the time to initiate braking.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG8 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SG-007 addresses the lidar false negative scenario (ASIL C). The safe state (controlled stop on sensor failure) is appropriate -- when the sensor is unreliable, the system cannot rely on obstacle detection and must initiate a precautionary stop. The 200 ms FTTI includes time for fault confirmation (distinguishing sensor failure from a genuinely clear path), which is a necessary design consideration to avoid false-positive controlled stops.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG8 -->

#### SG-008: Ensure Independent Safety Monitoring Availability

- **Safety Goal**: The system shall ensure availability of independent safety monitoring, emergency stop, and protection against unintended motor reversal.
- **ASIL**: C (highest from HE-011 ASIL C, HE-012 ASIL B, HE-013 ASIL B, HE-014 ASIL C)
- **Source**: HE-011 (CAN bus total failure, ASIL C), HE-012 (Safety Controller failure, ASIL B), HE-013 (E-stop not functional, ASIL B), HE-014 (Unintended motor reversal, ASIL C)
- **Safe State**: SS-SYSTEM-SHUTDOWN -- Kill relay opens, all outputs disabled, system de-energized.
- **FTTI**: 100 ms
- **Rationale**: These four hazardous events are grouped because they all relate to the availability and integrity of the independent safety monitoring layer. CAN bus total failure (HE-011) means the Safety Controller loses visibility of zone ECU health. Safety Controller failure (HE-012) removes the independent watchdog layer. E-stop failure (HE-013) removes the operator's last-resort intervention. Unintended motor reversal (HE-014) is a system-level failure that the Safety Controller should detect via plausibility checking (torque request direction vs. motor current direction). The safe state is full system shutdown via the kill relay -- this is the most conservative response, appropriate when the integrity of the safety monitoring chain itself is compromised. The 100 ms FTTI reflects the need for rapid de-energization when the safety monitoring function is lost.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG9 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SG-008 is a broad safety goal grouping 4 hazardous events across different failure domains (CAN bus, Safety Controller, E-stop, motor direction). While the grouping is justified by the shared theme of "safety monitoring layer integrity," the breadth of this SG may complicate downstream FSR derivation since each source event requires distinct safety mechanisms. The ASIL C assignment (inherited from HE-011 and HE-014) is correct. The safe state (SS-SYSTEM-SHUTDOWN with kill relay) is the most conservative and appropriate response when the monitoring layer itself is compromised. One concern: the inclusion of "unintended motor reversal" (HE-014) in this monitoring-focused SG is somewhat awkward -- motor direction reversal is an actuator-level fault, not a monitoring-layer fault. Consider whether HE-014 would be better assigned to a dedicated SG or to SG-001 (unintended acceleration domain).
<!-- HITL-LOCK END:COMMENT-BLOCK-SG9 -->

## 6. Safe State Definitions

### 6.1 Safe States Table

| Safe State ID | Name | Description | Entry Conditions | Affected Systems |
|---------------|------|-------------|------------------|------------------|
| SS-MOTOR-OFF | Motor Off | Motor torque set to 0, H-bridge RPWM and LPWM driven low, R_EN and L_EN disabled, brake servo commanded to full braking position, steering servo disabled. | Pedal plausibility failure (SG-001), steering fault (SG-003), loss of braking (SG-004), motor protection trip (SG-006). | RZC (motor control), FZC (brake servo, steering servo disable), CVC (torque request cancelled). |
| SS-CONTROLLED-STOP | Controlled Stop | Motor torque ramped down over 500 ms (not instantaneous cutoff). Brake servo gradually applied. Steering locked to last valid commanded position. Hazard lights activated via BCM. | Loss of drive torque (SG-002), unintended braking (SG-005), obstacle detection failure (SG-007). | RZC (motor ramp-down), FZC (brake application, steering lock), CVC (state transition), BCM (hazard lights). |
| SS-SYSTEM-SHUTDOWN | System Shutdown | Kill relay opened by Safety Controller (energize-to-run pattern: relay de-energizes = safe). All power to motor, servos, and actuators removed. Only ECU logic power remains for DTC logging. SC fault LEDs indicate which subsystem failed. | CAN bus total failure, SC failure, E-stop failure, unintended motor reversal (SG-008). | SC (kill relay), all ECUs (power removed from actuators), ICU (fault display). |

### 6.2 Safe State Detailed Descriptions

#### SS-MOTOR-OFF: Motor Off

**Entry sequence:**
1. CVC sets torque request to 0 in CAN message (or RZC autonomously disables on local fault detection).
2. RZC sets BTS7960 RPWM = 0, LPWM = 0 (no PWM output).
3. RZC sets R_EN = LOW, L_EN = LOW (H-bridge outputs tristated).
4. FZC commands brake servo to maximum braking position.
5. CVC transitions to SAFE_STOP mode, OLED displays fault indicator.
6. Dem stores associated DTC with freeze-frame data.

**Exit conditions:**
- Operator cycles ignition (power off, then power on).
- System performs full self-test on restart before returning to RUN mode.

**Verification**: This safe state must be achievable within 50 ms of fault detection for SG-001 and SG-004.

#### SS-MOTOR-OFF for Steering Fault (SG-003)

**Rationale for SAFE_STOP instead of degraded operation:** The platform has no mechanical fallback steering, no redundant steering actuator, and no steer-by-brake capability. A steering fault means the vehicle cannot be directionally controlled regardless of speed. Degraded-speed operation (e.g., 30% torque cap) is only appropriate for platforms with a functioning backup steering path. Without one, the only safe response is to stop the vehicle.

**Entry sequence:**
1. FZC detects steering fault (sensor timeout, plausibility failure, CAN command loss, rapid oscillation).
2. FZC broadcasts steering fault on CAN.
3. CVC sets torque request to 0, transitions to SAFE_STOP mode.
4. RZC sets BTS7960 RPWM = 0, LPWM = 0, R_EN = LOW, L_EN = LOW (H-bridge disabled).
5. FZC commands brake servo to maximum braking position.
6. FZC disables steering servo PWM (no further steering commands accepted).
7. CVC displays SAFE_STOP + steering fault indicator on OLED.
8. Dem stores DTC_STEER_FAULT with freeze-frame data.

**Exit conditions:**
- Operator cycles ignition (power off, then power on).
- System performs full self-test on restart before returning to RUN mode.
- Steering sensor fault must have cleared before self-test passes.

**Verification**: Vehicle must reach SAFE_STOP (motor off, brakes applied) within 100 ms of steering fault detection.

#### SS-CONTROLLED-STOP: Controlled Stop

**Entry sequence:**
1. CVC or FZC initiates controlled stop (depending on triggering safety goal).
2. CVC ramps torque request from current value to 0 over 500 ms (linear ramp-down).
3. RZC executes torque ramp-down, motor decelerates gradually.
4. FZC gradually applies brake servo (ramp from 0 to 80% braking over 500 ms).
5. FZC locks steering to last valid commanded angle (no further steering commands accepted).
6. BCM activates hazard lights (via CAN message from CVC).
7. ICU displays controlled stop in progress.
8. CVC transitions to SAFE_STOP mode after vehicle speed reaches 0.

**Exit conditions:**
- Vehicle speed = 0 confirmed by encoder feedback from RZC.
- Operator cycles ignition to restart.

**Verification**: Total time from fault detection to vehicle stationary must not exceed stopping distance at maximum speed plus 1 second for ramp-down.

#### SS-SYSTEM-SHUTDOWN: System Shutdown

**Entry sequence:**
1. SC detects critical failure (heartbeat timeout, CAN silence, E-stop not responding, plausibility violation).
2. SC opens kill relay by de-energizing relay coil (GIO_A0 = LOW -> MOSFET OFF -> relay opens).
3. Kill relay removes 12V power from motor driver, servos, and all actuators.
4. ECU logic remains powered (3.3V from separate rail) for DTC logging.
5. SC illuminates appropriate fault LEDs on the panel (GIO_A1-A4).
6. SC stops feeding its external watchdog (TPS3823). If SC itself is the failure, the external watchdog resets SC within 1.6 seconds (TPS3823 timeout).
7. Dem on surviving ECUs stores DTCs with freeze-frame data.

**Exit conditions:**
- Full system power cycle (12V supply off, then on).
- SC performs self-test, verifies CAN bus operational, all ECU heartbeats received.
- System enters INIT mode before transitioning to RUN.

**Verification**: Kill relay must open within 100 ms of SC fault detection. Kill relay de-energization must be tested at every system startup (relay test during INIT mode).

<!-- HITL-LOCK START:COMMENT-BLOCK-SG10 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The safe state definitions are precise, with detailed entry sequences, exit conditions, and verification criteria for each of the three safe states. SS-MOTOR-OFF includes a 6-step sequence from torque request zeroing to DTC storage. SS-CONTROLLED-STOP specifies a 500 ms ramp-down which is important for preventing rear-end collisions. SS-SYSTEM-SHUTDOWN correctly uses the energize-to-run relay pattern where de-energization = safe state. The dedicated SS-MOTOR-OFF description for steering faults (SG-003) with explicit rationale for why SAFE_STOP is required instead of DEGRADED is excellent engineering documentation. One observation: the exit conditions for all three safe states require an ignition cycle and full self-test, which prevents the system from inadvertently returning to normal operation after a fault. This is correct for ASIL D.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG10 -->

## 7. FTTI Justification

### 7.1 FTTI Overview

The Fault Tolerant Time Interval (FTTI) is the time span between the occurrence of a fault in the item and a possible hazardous event if the safety mechanisms do not act. The FTTI must be greater than the sum of fault detection time + fault reaction time + safe state transition time.

```
FTTI >= T_detect + T_react + T_safe_state

Where:
  T_detect     = Time to detect the fault (diagnostic cycle time)
  T_react      = Time to process the fault and initiate safe state transition
  T_safe_state = Time to reach the safe state (actuator response time)
```

### 7.2 FTTI per Safety Goal

#### SG-001: Prevent Unintended Acceleration -- FTTI = 50 ms

| Component | Value | Justification |
|-----------|-------|---------------|
| T_detect | 10 ms | Pedal plausibility check runs every 10 ms (Swc_Pedal runnable period). Dual sensor comparison detects disagreement within one cycle. |
| T_react | 5 ms | CVC processes fault, sets torque request to 0, sends CAN message to RZC. |
| T_safe_state | 15 ms | RZC receives CAN message (worst case: next 10 ms cycle), disables H-bridge outputs (GPIO write: < 1 ms). |
| T_margin | 20 ms | Margin for CAN bus arbitration delay, processing jitter, and E2E validation. |
| **FTTI** | **50 ms** | Sum: 10 + 5 + 15 + 20 = 50 ms. |

**Physical justification**: At maximum platform speed (estimated 2 m/s for bench demo, assumed 30 km/h = 8.3 m/s for real-vehicle analysis), unintended acceleration at full torque for 50 ms results in a speed increase of approximately 0.4 m/s (assuming 0.5 kg vehicle mass, 0.5 Nm torque, 0.05 m wheel radius). This is within controllable limits.

#### SG-002: Prevent Unintended Loss of Drive Torque -- FTTI = 200 ms

| Component | Value | Justification |
|-----------|-------|---------------|
| T_detect | 20 ms | Motor feedback (current, encoder) monitored every 10 ms. Two consecutive readings confirm loss of torque. |
| T_react | 10 ms | CVC evaluates motor status, initiates controlled stop sequence. |
| T_safe_state | 100 ms | Controlled stop ramp begins. Brake servo reaches initial braking force. |
| T_margin | 70 ms | Margin for CAN latency and multi-ECU coordination. |
| **FTTI** | **200 ms** | Sum: 20 + 10 + 100 + 70 = 200 ms. |

**Physical justification**: A 200 ms gap without drive torque at highway speed (assumed 30 km/h) results in approximately 1.7 m of coasting distance. Following vehicles at safe following distance (2 seconds = 16.7 m at 30 km/h) have sufficient time to react.

#### SG-003: Prevent Unintended Steering Movement -- FTTI = 100 ms

| Component | Value | Justification |
|-----------|-------|---------------|
| T_detect | 10 ms | Steering angle sensor read every 10 ms (Swc_Steering runnable). Deviation from command detected within one cycle. |
| T_react | 10 ms | FZC processes steering fault, commands servo to centre. |
| T_safe_state | 50 ms | Servo mechanical response time to reach centre from maximum deflection (servo bandwidth: ~300 deg/s, maximum deflection: 15 degrees, time: ~50 ms). |
| T_margin | 30 ms | Margin for servo dynamics and CAN coordination with CVC for speed reduction. |
| **FTTI** | **100 ms** | Sum: 10 + 10 + 50 + 30 = 100 ms. |

**Physical justification**: At 30 km/h (8.3 m/s), a 100 ms uncontrolled steering deviation of 15 degrees results in a lateral displacement of approximately 0.22 m (calculated as v * t * sin(angle) = 8.3 * 0.1 * sin(15 deg)). This is within a standard lane width margin (3.5 m lane, 1.8 m vehicle width, 0.85 m margin per side).

#### SG-004: Prevent Unintended Loss of Braking -- FTTI = 50 ms

| Component | Value | Justification |
|-----------|-------|---------------|
| T_detect | 10 ms | Brake servo position feedback monitored every 10 ms. Brake system fault detected by comparing commanded vs. actual position. |
| T_react | 5 ms | CVC sets torque to 0, sends emergency CAN broadcast. |
| T_safe_state | 15 ms | RZC disables H-bridge (removes drive torque as fail-safe). |
| T_margin | 20 ms | Margin for CAN bus and processing. |
| **FTTI** | **50 ms** | Sum: 10 + 5 + 15 + 20 = 50 ms. |

**Physical justification**: Loss of braking is most critical during active braking towards an obstacle. At 30 km/h, a 50 ms delay extends stopping distance by approximately 0.42 m (8.3 m/s * 0.05 s). Removing motor torque immediately prevents any acceleration that would further extend stopping distance.

#### SG-005: Prevent Unintended Braking -- FTTI = 200 ms

| Component | Value | Justification |
|-----------|-------|---------------|
| T_detect | 20 ms | Unintended braking detected by comparing driver brake command (none) vs. brake servo actuation (active). Lidar false positive detected by comparing consecutive readings. |
| T_react | 10 ms | FZC releases brake command, cancels emergency brake if lidar-triggered. |
| T_safe_state | 100 ms | Brake servo returns to neutral position (servo release time). |
| T_margin | 70 ms | Margin for lidar filter convergence and multi-reading confirmation. |
| **FTTI** | **200 ms** | Sum: 20 + 10 + 100 + 70 = 200 ms. |

**Physical justification**: At 30 km/h, an unintended braking event lasting 200 ms at maximum braking deceleration (5 m/s^2) reduces speed by 1 m/s. This is perceptible to following traffic but within controllable limits (ASIL A).

#### SG-006: Ensure Motor Protection -- FTTI = 500 ms

| Component | Value | Justification |
|-----------|-------|---------------|
| T_detect | 50 ms | Overcurrent detection requires 5 consecutive readings above threshold (5 * 10 ms) to filter transients. Temperature monitoring runs at 100 ms cycle. |
| T_react | 10 ms | RZC processes overcurrent/overtemperature, disables motor output. |
| T_safe_state | 10 ms | H-bridge disable is immediate (GPIO write). |
| T_margin | 430 ms | Margin for thermal time constant of motor windings and cable heating. |
| **FTTI** | **500 ms** | Sum: 50 + 10 + 10 + 430 = 500 ms. |

**Physical justification**: The thermal time constant of a small brushed DC motor winding is typically 5-30 seconds. Cable heating from overcurrent follows I^2*t characteristics, with damage thresholds typically in the range of seconds for 2x rated current. The 500 ms FTTI provides significant margin against thermal damage while allowing adequate filtering of transient current spikes (e.g., motor startup inrush).

#### SG-007: Ensure Obstacle Detection -- FTTI = 200 ms

| Component | Value | Justification |
|-----------|-------|---------------|
| T_detect | 50 ms | Lidar sensor diagnostic check runs every 10 ms. A sensor fault (stuck value, timeout, out-of-range) requires 5 consecutive anomalous readings to confirm. |
| T_react | 10 ms | FZC evaluates lidar health, initiates controlled stop if sensor unreliable. |
| T_safe_state | 100 ms | Controlled stop begins: brake servo commanded, motor ramp-down starts. |
| T_margin | 40 ms | Margin for CAN messaging and multi-ECU coordination. |
| **FTTI** | **200 ms** | Sum: 50 + 10 + 100 + 40 = 200 ms. |

**Physical justification**: At 30 km/h (8.3 m/s), a 200 ms delay in detecting a failed lidar sensor results in 1.7 m of travel without obstacle awareness. The TFMini-S lidar has a 12 m maximum range, providing approximately 1.4 seconds of warning at 30 km/h. Even with 200 ms of detection delay, the remaining 1.2 seconds provides adequate braking distance (approximately 6.9 m at 5 m/s^2 deceleration from 30 km/h).

#### SG-008: Ensure Independent Safety Monitoring Availability -- FTTI = 100 ms

| Component | Value | Justification |
|-----------|-------|---------------|
| T_detect | 50 ms | SC heartbeat monitoring checks each ECU every 50 ms. CAN bus silence detected within one monitoring cycle. |
| T_react | 5 ms | SC processes fault, initiates kill relay opening. |
| T_safe_state | 10 ms | Kill relay opening time (mechanical relay release: 5-10 ms typical for automotive relay). |
| T_margin | 35 ms | Margin for relay bounce settling and power rail discharge. |
| **FTTI** | **100 ms** | Sum: 50 + 5 + 10 + 35 = 100 ms. |

**Physical justification**: Loss of safety monitoring means the system is operating without its independent watchdog. In the worst case, a hazardous event could occur simultaneously with the monitoring failure. The 100 ms FTTI ensures rapid de-energization before any undetected fault in the zone ECUs can escalate to a hazardous state. At 30 km/h, 100 ms of unmonitored operation results in 0.83 m of travel, which is within acceptable margins given that the kill relay physically removes actuator power.

### 7.3 FTTI Budget Summary

| SG-ID | FTTI | T_detect | T_react | T_safe_state | T_margin | Diagnostic Cycle |
|-------|------|----------|---------|--------------|----------|------------------|
| SG-001 | 50 ms | 10 ms | 5 ms | 15 ms | 20 ms | 10 ms (Swc_Pedal) |
| SG-002 | 200 ms | 20 ms | 10 ms | 100 ms | 70 ms | 10 ms (Swc_Motor) |
| SG-003 | 100 ms | 10 ms | 10 ms | 50 ms | 30 ms | 10 ms (Swc_Steering) |
| SG-004 | 50 ms | 10 ms | 5 ms | 15 ms | 20 ms | 10 ms (Swc_Brake) |
| SG-005 | 200 ms | 20 ms | 10 ms | 100 ms | 70 ms | 10 ms (Swc_Brake / Swc_Lidar) |
| SG-006 | 500 ms | 50 ms | 10 ms | 10 ms | 430 ms | 10 ms (Swc_CurrentMonitor) |
| SG-007 | 200 ms | 50 ms | 10 ms | 100 ms | 40 ms | 10 ms (Swc_Lidar) |
| SG-008 | 100 ms | 50 ms | 5 ms | 10 ms | 35 ms | 50 ms (SC heartbeat) |

<!-- HITL-LOCK START:COMMENT-BLOCK-SG11 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The FTTI justifications are comprehensive, with each safety goal having a detailed timing budget breakdown (T_detect + T_react + T_safe_state + T_margin) and a physical justification relating the FTTI to real-world consequences (speed, distance, acceleration). The budgets are credible: SG-001 and SG-004 at 50 ms are the tightest, reflecting the critical nature of acceleration and braking faults. The physical justifications use correct physics calculations (e.g., v*t for distance, kinematic equations for speed change). The FTTI budget summary table (Section 7.3) provides a quick reference and identifies the diagnostic cycle time for each SG. One concern: SG-008 FTTI = 100 ms, but the FSC timing analysis (Section 7) shows SM-019 (heartbeat monitoring) at 205 ms, which exceeds this FTTI. This inconsistency is acknowledged in the FSC but should also be noted here with a reference to the compensating mechanisms.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG11 -->

## 8. Traceability

### 8.1 Hazardous Event to Safety Goal Mapping

This section provides the complete bidirectional traceability between hazardous events and safety goals. Every hazardous event identified in the HARA traces to exactly one safety goal.

| HE-ID | Hazardous Event | HE ASIL | SG-ID | SG ASIL | Notes |
|-------|-----------------|---------|-------|---------|-------|
| HE-001 | Unintended acceleration (pedal sensors both read high incorrectly) | ASIL D | SG-001 | ASIL D | 1:1 mapping. Highest ASIL event in HARA. |
| HE-002 | Loss of drive torque (motor stops unexpectedly) | ASIL B | SG-002 | ASIL B | 1:1 mapping. |
| HE-003 | Unintended steering movement | ASIL C | SG-003 | ASIL D | Grouped with HE-004. SG inherits ASIL D from HE-004. |
| HE-004 | Loss of steering during turning | ASIL D | SG-003 | ASIL D | Grouped with HE-003. Dominant ASIL D event. |
| HE-005 | Loss of braking during braking | ASIL D | SG-004 | ASIL D | 1:1 mapping. |
| HE-006 | Unintended braking during driving | ASIL A | SG-005 | ASIL A | Grouped with HE-010. Both ASIL A. |
| HE-007 | Motor overcurrent not detected | ASIL A | SG-006 | ASIL A | Grouped with HE-008 and HE-015. Dominant ASIL A. |
| HE-008 | Motor overtemperature not detected | QM | SG-006 | ASIL A | Grouped with HE-007 and HE-015. SG inherits ASIL A from HE-007. |
| HE-009 | Lidar false negative (obstacle not detected) | ASIL C | SG-007 | ASIL C | 1:1 mapping. |
| HE-010 | Lidar false positive (phantom braking) | ASIL A | SG-005 | ASIL A | Grouped with HE-006. Both ASIL A. |
| HE-011 | CAN bus total failure | ASIL C | SG-008 | ASIL C | Grouped. Shared safety monitoring domain. |
| HE-012 | Safety Controller failure (loss of monitoring) | ASIL B | SG-008 | ASIL C | Grouped. SG inherits ASIL C from HE-011 / HE-014. |
| HE-013 | E-stop not functional | ASIL B | SG-008 | ASIL C | Grouped. SG inherits ASIL C from HE-011 / HE-014. |
| HE-014 | Unintended motor reversal | ASIL C | SG-008 | ASIL C | Grouped. Co-dominant ASIL C event. |
| HE-015 | Battery overvoltage/undervoltage | QM | SG-006 | ASIL A | Grouped with HE-007 and HE-008. SG inherits ASIL A from HE-007. |

### 8.2 Safety Goal to Safe State Mapping

| SG-ID | Safe State | Transition Priority |
|-------|------------|---------------------|
| SG-001 | SS-MOTOR-OFF | 1 (Highest -- immediate motor disable) |
| SG-002 | SS-CONTROLLED-STOP | 3 (Controlled deceleration) |
| SG-003 | SS-MOTOR-OFF | 1 (Highest -- immediate motor disable, brakes applied) |
| SG-004 | SS-MOTOR-OFF | 1 (Highest -- immediate motor disable) |
| SG-005 | SS-CONTROLLED-STOP | 3 (Release brake, controlled stop) |
| SG-006 | SS-MOTOR-OFF | 1 (Motor protection -- immediate disable) |
| SG-007 | SS-CONTROLLED-STOP | 3 (Controlled stop on sensor failure) |
| SG-008 | SS-SYSTEM-SHUTDOWN | 0 (Emergency -- kill relay, full de-energization) |

**Priority rule**: When multiple safety goals are violated simultaneously, the safe state with the lowest priority number (highest priority) takes precedence. SS-SYSTEM-SHUTDOWN (priority 0) always overrides all other safe states.

### 8.3 Downstream Traceability (Safety Goals to Requirements)

The following table shows the intended downstream traceability from safety goals to functional safety requirements (FSR), to be elaborated in the Functional Safety Concept (FSC, document FSC).

| SG-ID | ASIL | Intended FSR Topics | Allocation |
|-------|------|---------------------|------------|
| SG-001 | D | Dual pedal sensor plausibility check, torque request monitoring, pedal-to-torque mapping validation | CVC, RZC |
| SG-002 | B | Motor feedback monitoring, torque delivery confirmation, controlled stop on motor failure | RZC, CVC |
| SG-003 | D | Steering servo position monitoring, command-vs-feedback comparison, return-to-centre mechanism, rate limiting | FZC, CVC |
| SG-004 | D | Brake servo position monitoring, brake command confirmation, motor torque removal on brake failure | FZC, CVC, RZC |
| SG-005 | A | Brake command validation, lidar signal filtering, multi-reading confirmation before emergency brake | FZC |
| SG-006 | A | Current sensor monitoring, temperature monitoring, voltage monitoring, overcurrent debouncing | RZC |
| SG-007 | C | Lidar sensor diagnostic monitoring, stuck value detection, timeout detection, sensor health reporting | FZC |
| SG-008 | C | Heartbeat monitoring, CAN bus diagnostic, E-stop circuit monitoring, motor direction plausibility, kill relay self-test | SC, CVC |

### 8.4 Traceability Completeness Check

| Check | Result |
|-------|--------|
| All 15 hazardous events traced to a safety goal? | YES -- HE-001 through HE-015 all assigned. |
| All safety goals have at least one source HE? | YES -- SG-001 through SG-008 all have sources. |
| All safety goals have a defined safe state? | YES -- SS-MOTOR-OFF, SS-CONTROLLED-STOP, SS-SYSTEM-SHUTDOWN. |
| All safety goals have a justified FTTI? | YES -- See Section 7.2. |
| ASIL inheritance correct (highest from grouped HEs)? | YES -- Verified for SG-003 (D from HE-004), SG-005 (A from HE-006/010), SG-006 (A from HE-007), SG-008 (C from HE-011/014). |
| No orphan hazardous events? | YES -- 0 HEs without SG assignment. |
| No orphan safety goals? | YES -- 0 SGs without HE source. |

## 9. ASIL Distribution Summary

| ASIL | Safety Goals | Count |
|------|-------------|-------|
| ASIL D | SG-001, SG-003, SG-004 | 3 |
| ASIL C | SG-007, SG-008 | 2 |
| ASIL B | SG-002 | 1 |
| ASIL A | SG-005, SG-006 | 2 |
| **Total** | | **8** |

The presence of 3 ASIL D safety goals confirms that the Taktflow Zonal Vehicle Platform requires the highest level of functional safety integrity for its drive-by-wire (acceleration and braking) and steering functions. This is consistent with production automotive systems where the powertrain and steering domains carry ASIL D requirements.

<!-- HITL-LOCK START:COMMENT-BLOCK-SG12 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The traceability section (8.1) provides complete bidirectional mapping for HE-001 through HE-015, with correct ASIL inheritance documented for grouped events. The traceability completeness check (Section 8.4) confirms all 15 hazardous events are traced -- but this should say "all 20 hazardous events" to include HE-016 through HE-020. The safety goal to safe state mapping (Section 8.2) with priority numbers is a useful addition for resolving concurrent safety goal violations. The priority rule (SS-SYSTEM-SHUTDOWN overrides all) is correct. The ASIL distribution summary (3 ASIL D, 2 ASIL C, 1 ASIL B, 2 ASIL A) is consistent with production automotive expectations. The downstream traceability preview (Section 8.3) correctly identifies the intended FSR topics for each SG, providing useful guidance for the FSC. Overall, the traceability is strong within its scope but must be extended to cover all 20 HARA hazardous events.
<!-- HITL-LOCK END:COMMENT-BLOCK-SG12 -->

## 10. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete safety goals with 8 SGs, 4 safe states, FTTI justification, and full traceability |
| 1.1 | 2026-02-26 | System | SG-003 safe state changed from SS-STEER-CENTER to SS-MOTOR-OFF — no mechanical fallback steering on this platform, steer fault requires vehicle stop |

