---
document_id: TSR
title: "Technical Safety Requirements"
version: "1.1"
status: draft
iso_26262_part: 4
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

Every requirement (TSR-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/safety/lessons-learned/`](../lessons-learned/). One file per requirement (TSR-NNN). File naming: `TSR-NNN-<short-title>.md`.


# Technical Safety Requirements

## 1. Purpose

This document specifies the technical safety requirements (TSR) for the Taktflow Zonal Vehicle Platform, derived from the functional safety requirements (FSR) per ISO 26262-4 Clause 6. Technical safety requirements define HOW the system elements shall implement the functional safety requirements. Each TSR is allocated to one or more specific ECUs and specifies measurable, testable implementation details including timing, thresholds, interfaces, and protocols.

The TSR document bridges the gap between WHAT the system must do (FSR) and the detailed software/hardware requirements (SSR/HSR) that implement each TSR.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SG | Safety Goals | 1.0 |
| FSC | Functional Safety Concept | 1.0 |
| FSR | Functional Safety Requirements | 1.0 |
| ITEM-DEF | Item Definition | 1.0 |
| HARA | Hazard Analysis and Risk Assessment | 1.0 |
| DFA | Dependent Failure Analysis | 0.1 |
| HSI | Hardware-Software Interface Specification | 0.1 |

## 3. Requirement Conventions

### 3.1 Requirement Structure

Each requirement follows the format:

- **ID**: TSR-NNN (sequential)
- **Title**: Descriptive name
- **ASIL**: Inherited from parent FSR (or highest if multiple parents)
- **Traces up**: FSR-xxx (and SG-xxx)
- **Traces down**: SSR-{ECU}-xxx, HSR-{ECU}-xxx
- **Allocated to**: Specific ECU(s)
- **Verification method**: Inspection, analysis, test, or simulation
- **Status**: draft | reviewed | approved | implemented | verified

### 3.2 Requirement Language

Requirements use "shall" for mandatory behavior, "should" for recommended behavior, and "may" for optional behavior. All TSRs in this document use "shall" unless noted otherwise.

### 3.3 ASIL Inheritance

Each TSR inherits the ASIL of its parent FSR. Where a TSR traces to multiple FSRs, it inherits the highest ASIL among them.

---

## 4. Technical Safety Requirements

### 4.1 Drive-by-Wire Domain (SG-001, SG-002)

---

#### TSR-001: CVC Dual Pedal Sensor SPI Acquisition

- **ASIL**: D
- **Traces up**: FSR-001 (SG-001)
- **Traces down**: SSR-CVC-001, SSR-CVC-002, HSR-CVC-001
- **Allocated to**: CVC
- **Verification method**: Test (SIL + PIL)
- **Status**: draft

The CVC shall read both AS5048A pedal position sensors via SPI1 at a fixed rate of 100 Hz (10 ms period). Each sensor shall be addressed by its dedicated chip-select line (CS1 on PA4, CS2 on PA15). The SPI clock shall be configured at 1 MHz. Each SPI transaction shall include a 16-bit read of the angle register (0x3FFF) followed by CRC validation of the sensor response. If a SPI transaction fails (no response within 500 us, CRC error, or parity error), the transaction shall be retried once within the same 10 ms cycle before declaring a communication fault.

**Rationale**: Separate chip-select lines ensure independent addressing of each sensor on the shared SPI bus. The 10 ms read rate provides sufficient diagnostic update rate for the 50 ms FTTI of SG-001. SPI CRC validation detects communication errors at the physical layer.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement is verifiable with specific SPI configuration (1 MHz, PA4/PA15 chip-selects), 100 Hz read rate, CRC validation, and 500 us timeout with single retry. ASIL D is correct for FSR-001/SG-001 trace. The 10 ms read rate provides 4 cycles within the 50 ms FTTI for detection + reaction. Traces down to SSR-CVC-001, SSR-CVC-002, and HSR-CVC-001 are consistent. The shared SPI bus for both sensors is a potential common-cause concern -- SPI bus failure affects both sensors simultaneously. This is acceptable because the SPI bus fault detection (CRC + retry) triggers the safe-state reaction rather than relying on cross-sensor comparison alone.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR001 -->

---

#### TSR-002: CVC Dual Pedal Sensor Plausibility Comparison

- **ASIL**: D
- **Traces up**: FSR-001, FSR-002 (SG-001)
- **Traces down**: SSR-CVC-003, SSR-CVC-004
- **Allocated to**: CVC
- **Verification method**: Test (unit test + SIL)
- **Status**: draft

The CVC shall compare the angle values from both AS5048A sensors every 10 ms and flag a pedal plausibility fault if the absolute difference between the two readings exceeds 5% of the 14-bit full-scale range (819 counts) for 2 or more consecutive control cycles (20 ms debounce). Additionally, each individual sensor reading shall be range-checked: values below 819 counts (5% of range) or above 15564 counts (95% of range) when the pedal is expected to be at rest shall be flagged as sensor stuck-at faults. The fault detection latency from fault occurrence to fault flag assertion shall not exceed 20 ms.

**Rationale**: The 5% threshold and 2-cycle debounce balance sensitivity against transient noise. The 20 ms detection time provides 30 ms margin within the 50 ms FTTI for SG-001 (20 ms detection + 10 ms CAN reaction + 20 ms margin).

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement is verifiable with precise threshold (819 counts = 5%), debounce (2 cycles = 20 ms), range-check boundaries (5% and 95%), and explicit detection latency (20 ms max). ASIL D is correct for FSR-001/FSR-002 traces. The FTTI budget breakdown (20 ms detect + 10 ms CAN + 20 ms margin = 50 ms) is explicitly documented and correct. Traces down to SSR-CVC-003 and SSR-CVC-004 are consistent. The stuck-at fault detection using range checks at rest position is a good diagnostic. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR002 -->

---

#### TSR-003: CVC Pedal Sensor Individual Diagnostics

- **ASIL**: D
- **Traces up**: FSR-002 (SG-001)
- **Traces down**: SSR-CVC-005
- **Allocated to**: CVC
- **Verification method**: Test (unit test + fault injection)
- **Status**: draft

The CVC shall perform the following individual diagnostic checks on each pedal sensor reading within every 10 ms control cycle:

1. Range check: The raw 14-bit angle value shall be within the valid mechanical range (calibratable, default: 0 to 16383 counts). A reading of exactly 0x0000 or 0x3FFF for 3 consecutive cycles shall be flagged as a stuck-at fault.
2. Rate-of-change check: The angle change between consecutive readings shall not exceed the maximum physical slew rate of the pedal mechanism (calibratable, default: 1000 counts per 10 ms). Exceeding this rate indicates a sensor glitch or electrical noise.
3. SPI diagnostic check: The AS5048A diagnostic register (0x3FFD) shall be read at least every 100 ms to detect magnetic field strength faults (AGC out of range), CORDIC overflow, or parity errors reported by the sensor.

**Rationale**: Individual diagnostics detect sensor-specific faults (stuck, range, communication) that may not be caught by dual-sensor comparison alone (e.g., both sensors stuck at the same value due to a shared magnetic field fault).

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement provides comprehensive per-sensor diagnostics: range check (stuck-at 0x0000/0x3FFF for 3 cycles), rate-of-change check (1000 counts/10 ms), and AS5048A diagnostic register check (every 100 ms). ASIL D is correct for FSR-002/SG-001. The rationale correctly identifies the limitation of dual-sensor comparison alone -- both sensors sharing the same magnet could fail identically. Traces down to SSR-CVC-005 are consistent. The 3-cycle stuck-at debounce at the extreme values is appropriate to distinguish between legitimate end-of-travel readings and sensor faults.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR003 -->

---

#### TSR-004: CVC Zero-Torque Command on Pedal Fault

- **ASIL**: D
- **Traces up**: FSR-003 (SG-001)
- **Traces down**: SSR-CVC-006, SSR-CVC-007
- **Allocated to**: CVC
- **Verification method**: Test (SIL + PIL)
- **Status**: draft

Upon detection of a pedal sensor plausibility fault (TSR-002) or individual sensor diagnostic fault (TSR-003), the CVC shall set the internal torque request to 0% within 1 ms and transmit a zero-torque CAN message (CAN ID 0x100, E2E protected) to the RZC within the next CAN transmission cycle (10 ms maximum). The zero-torque command shall be latched: the CVC shall not accept any non-zero torque request until all of the following conditions are met: (a) both pedal sensor readings are within the valid range for 50 consecutive cycles (500 ms), (b) the plausibility comparison passes for 50 consecutive cycles (500 ms), (c) the operator has performed a manual reset (E-stop release and re-engagement sequence).

**Rationale**: Immediate internal zeroing (1 ms) prevents any local torque calculation from using a faulty pedal value. The latching with 500 ms recovery window prevents intermittent faults from repeatedly engaging and disengaging torque. Manual reset ensures operator awareness.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies a comprehensive fault reaction with three conditions for fault clearance: 50 fault-free cycles (500 ms), plausibility pass for 50 cycles, and manual reset (E-stop release/re-engage). ASIL D is correct for FSR-003/SG-001. The 1 ms internal zeroing is achievable within a single control cycle. The latching design with three-condition recovery is appropriately conservative for ASIL D. Traces down to SSR-CVC-006 and SSR-CVC-007 are consistent. The E2E-protected CAN transmission of zero-torque is correctly specified. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR004 -->

---

#### TSR-005: RZC Motor Disable on Zero-Torque Command

- **ASIL**: D
- **Traces up**: FSR-003 (SG-001)
- **Traces down**: SSR-RZC-001, SSR-RZC-002
- **Allocated to**: RZC
- **Verification method**: Test (PIL + hardware test)
- **Status**: draft

Upon receiving a zero-torque CAN message from the CVC (CAN ID 0x100, passing E2E validation), the RZC shall set the BTS7960 PWM duty cycle to 0% (RPWM = 0, LPWM = 0) and disable both motor driver enable lines (R_EN = LOW, L_EN = LOW) within 5 ms of message reception. The motor driver shall remain disabled until a valid non-zero torque command is received from the CVC with valid E2E protection.

**Rationale**: Disabling both PWM outputs and both enable lines provides defense-in-depth: even if one mechanism fails (PWM stuck), the enable line disable prevents current flow. The 5 ms execution time contributes to meeting the 50 ms FTTI for SG-001.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies defense-in-depth motor disable with four independent actions (RPWM=0, LPWM=0, R_EN=LOW, L_EN=LOW) within 5 ms. ASIL D is correct for FSR-003/SG-001. The re-enable condition requiring valid non-zero torque with E2E protection prevents spurious motor activation. Traces down to SSR-RZC-001 and SSR-RZC-002 are consistent. The 5 ms execution time leaves ample margin within the 50 ms FTTI. The defense-in-depth approach (PWM + enable lines) correctly addresses the stuck-PWM failure mode.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR005 -->

---

#### TSR-006: RZC Motor Current Sampling and Overcurrent Detection

- **ASIL**: A
- **Traces up**: FSR-004 (SG-006)
- **Traces down**: SSR-RZC-003, SSR-RZC-004, HSR-RZC-001
- **Allocated to**: RZC
- **Verification method**: Test (PIL + hardware test)
- **Status**: draft

The RZC shall sample motor current via the ACS723 current sensor on ADC1 channel 1 at a minimum rate of 1 kHz (1 ms period). The ADC shall be configured for 12-bit resolution with a conversion time of less than 5 us. The raw ADC value shall be converted to milliamps using the ACS723 sensitivity (100 mV/A for ACS723LLCTR-20AB-T 20A variant, calibratable). If the converted current exceeds the overcurrent threshold (calibratable, default: 25000 mA) for a continuous debounce period of 10 ms (10 consecutive samples), the RZC shall disable the motor driver (R_EN = LOW, L_EN = LOW, RPWM = 0, LPWM = 0) within 1 ms of debounce expiry.

**Rationale**: 1 kHz sampling ensures at least 10 samples within the 10 ms debounce window. The debounce prevents false triggers from motor inrush current (which can reach 3x rated current for less than 5 ms). The ACS723 provides galvanic isolation between the motor power path and the MCU.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement is verifiable with specific ADC configuration (12-bit, 1 kHz, 5 us conversion), ACS723 sensitivity (100 mV/A), overcurrent threshold (25000 mA), and debounce (10 ms, 10 samples). ASIL A is appropriate for FSR-004/SG-006 (motor protection). The 10 ms debounce correctly prevents false triggers from motor inrush current. Traces down to SSR-RZC-003, SSR-RZC-004, and HSR-RZC-001 are consistent. The galvanic isolation of ACS723 is a key safety feature. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR006 -->

---

#### TSR-007: RZC Motor Current Broadcast to SC

- **ASIL**: C
- **Traces up**: FSR-004, FSR-022 (SG-001, SG-006)
- **Traces down**: SSR-RZC-005
- **Allocated to**: RZC
- **Verification method**: Test (SIL)
- **Status**: draft

The RZC shall transmit the measured motor current value in a CAN message (CAN ID 0x301, E2E protected) at a fixed rate of 100 Hz (10 ms period). The message shall contain: current magnitude (16-bit, milliamps), motor direction (1-bit, 0 = forward, 1 = reverse), motor enable status (1-bit), alive counter (4-bit), and CRC-8 (8-bit). The total message payload shall not exceed 8 bytes.

**Rationale**: The SC requires motor current data for cross-plausibility checking (TSR-041). The 10 ms broadcast rate matches the CVC torque command rate, enabling synchronized comparison by the SC. The message format includes direction and enable status for comprehensive plausibility analysis.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR007 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies the CAN message format for motor current broadcast (CAN ID 0x301, E2E protected, 100 Hz) with direction, enable status, alive counter, and CRC-8. ASIL C is elevated from ASIL A (FSR-004) due to also tracing to FSR-022/SG-001. The 10 ms broadcast rate matching the CVC torque command rate enables synchronized SC comparison. Traces down to SSR-RZC-005 are consistent. The 8-byte payload constraint is met. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR007 -->

---

#### TSR-008: RZC Motor Temperature Sampling

- **ASIL**: A
- **Traces up**: FSR-005 (SG-006)
- **Traces down**: SSR-RZC-006, HSR-RZC-002
- **Allocated to**: RZC
- **Verification method**: Test (PIL + hardware test)
- **Status**: draft

The RZC shall sample motor winding temperature via the NTC thermistor on ADC1 channel 2 at a minimum rate of 10 Hz (100 ms period). The ADC reading shall be converted to degrees Celsius using the NTC beta-parameter equation (B = 3950 typical for 10k NTC, calibratable). The sensor range check shall flag readings equivalent to below -30 degrees C or above 150 degrees C as sensor faults (open circuit or short circuit).

**Rationale**: The 100 ms sample rate is sufficient given the motor thermal time constant (seconds). The sensor fault thresholds detect the two most common NTC failure modes: open circuit (reads as very cold) and short to ground (reads as very hot).

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR008 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies NTC temperature sensing (ADC1 CH2, 10 Hz, beta=3950) with sensor fault detection (below -30 C or above 150 C). ASIL A is appropriate for FSR-005/SG-006. The 100 ms sample rate is justified by the motor thermal time constant. The open/short circuit detection via extreme temperature ranges is a standard and effective NTC diagnostic. Traces down to SSR-RZC-006 and HSR-RZC-002 are consistent. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR008 -->

---

#### TSR-009: RZC Motor Temperature Derating Enforcement

- **ASIL**: A
- **Traces up**: FSR-005 (SG-006)
- **Traces down**: SSR-RZC-007
- **Allocated to**: RZC
- **Verification method**: Test (unit test + SIL)
- **Status**: draft

The RZC shall enforce a motor current derating curve based on the measured motor temperature (TSR-008):

| Temperature Range | Maximum Permitted Current | Vehicle State |
|-------------------|--------------------------|---------------|
| Below 60 degrees C | 100% of rated current (25A) | RUN |
| 60 to 79 degrees C | 75% of rated current (18.75A) | DEGRADED |
| 80 to 99 degrees C | 50% of rated current (12.5A) | LIMP |
| 100 degrees C and above | 0% (motor disabled) | SAFE_STOP |
| Sensor fault (below -30 or above 150 degrees C) | 0% (motor disabled) | SAFE_STOP |

The RZC shall enforce a 10 degrees C hysteresis on recovery: motor re-enable after a thermal shutdown requires the temperature to drop below 90 degrees C (not 100 degrees C). State transitions at each derating level shall be communicated to the CVC via CAN within 10 ms.

**Rationale**: Progressive derating extends the operating envelope while preventing thermal damage. The hysteresis prevents oscillation at temperature boundaries.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR009 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies a clear derating curve with four temperature bands, hysteresis (10 C on recovery), and state transitions communicated via CAN within 10 ms. ASIL A is appropriate for FSR-005/SG-006. The progressive derating (100%/75%/50%/0%) with vehicle state transitions (RUN/DEGRADED/LIMP/SAFE_STOP) provides proportional response. The 10 C hysteresis correctly prevents oscillation at temperature boundaries. Traces down to SSR-RZC-007 are consistent. The sensor fault case (below -30 or above 150) correctly triggers 0% current (most conservative), which is fail-closed. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR009 -->

---

### 4.2 Steering Domain (SG-003)

---

#### TSR-010: FZC Steering Angle Sensor Acquisition

- **ASIL**: D
- **Traces up**: FSR-006 (SG-003)
- **Traces down**: SSR-FZC-001, HSR-FZC-001
- **Allocated to**: FZC
- **Verification method**: Test (PIL + hardware test)
- **Status**: draft

The FZC shall read the steering angle feedback sensor (AS5048A) via SPI1 at a fixed rate of 100 Hz (10 ms period). The SPI clock shall be configured at 1 MHz. Each read shall include CRC validation of the sensor response. On SPI communication failure (no response within 500 us, CRC error, or parity error), the transaction shall be retried once. If the retry also fails, a steering sensor communication fault shall be declared.

**Rationale**: The 10 ms read rate provides sufficient update rate for the 100 ms FTTI of SG-003. CRC validation detects communication corruption at the physical layer.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR010 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies steering angle sensor acquisition via SPI1 at 100 Hz with CRC validation and single retry. ASIL D is correct for FSR-006/SG-003. The 10 ms read rate provides 10 cycles within the 100 ms FTTI. Traces down to SSR-FZC-001 and HSR-FZC-001 are consistent. Note: unlike the pedal sensors (dual redundant), steering uses a single AS5048A. The 95% diagnostic coverage relies on software command-vs-feedback comparison rather than hardware redundancy -- this is acceptable if documented in the FMEDA with the residual risk.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR010 -->

---

#### TSR-011: FZC Steering Angle Feedback Plausibility

- **ASIL**: D
- **Traces up**: FSR-006 (SG-003)
- **Traces down**: SSR-FZC-002, SSR-FZC-003
- **Allocated to**: FZC
- **Verification method**: Test (unit test + SIL + fault injection)
- **Status**: draft

The FZC shall perform the following plausibility checks on the steering angle feedback sensor every 10 ms:

1. Command-vs-feedback comparison: The absolute difference between the commanded steering angle (from CVC via CAN) and the measured steering angle shall not exceed 5 degrees for more than 50 ms (5 consecutive cycles). Exceeding this threshold triggers a steering position fault.
2. Mechanical range check: The measured angle shall be within the valid mechanical range of -45 to +45 degrees. A reading outside this range for 2 consecutive cycles triggers a range fault.
3. Slew rate check: The rate of change of the measured angle shall not exceed 360 degrees per second. Exceeding this rate indicates a sensor glitch or servo runaway.
4. SPI diagnostic check: The AS5048A diagnostic register shall be checked every 100 ms for magnetic field and CORDIC faults.

On any steering fault, the FZC shall initiate the return-to-center procedure (TSR-012).

**Rationale**: Multiple plausibility checks ensure diverse fault detection. The 50 ms debounce for command-vs-feedback accounts for servo settling time during normal position tracking.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR011 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies four diverse plausibility checks: command-vs-feedback (5 deg, 50 ms debounce), mechanical range (-45 to +45 deg), slew rate (360 deg/s), and AS5048A diagnostic register. ASIL D is correct for FSR-006/SG-003. The multi-check approach provides defense-in-depth for steering sensor monitoring. The 50 ms debounce for command-vs-feedback accounts for servo settling dynamics. Traces down to SSR-FZC-002 and SSR-FZC-003 are consistent. The fault action (invoke return-to-center) is appropriate for steering. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR011 -->

---

#### TSR-012: FZC Steering Return-to-Center Command

- **ASIL**: D
- **Traces up**: FSR-007 (SG-003)
- **Traces down**: SSR-FZC-004, SSR-FZC-005
- **Allocated to**: FZC
- **Verification method**: Test (SIL + PIL)
- **Status**: draft

Upon detection of a steering fault (TSR-011), the FZC shall command the steering servo to the center position (0 degrees) using a rate-limited ramp not exceeding 30 degrees per second. The servo PWM output (TIM1, channel 1) shall be updated every 10 ms with the interpolated position toward center. If the steering angle feedback sensor indicates the servo has not reached within 2 degrees of center within 200 ms of initiating return-to-center, the FZC shall disable the steering servo PWM output entirely (duty cycle = 0%, timer output disabled).

**Rationale**: Rate-limited return prevents the correction from itself being a hazard (sudden steering movement). The 200 ms fallback to PWM disable handles mechanical binding or feedback sensor fault.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR012 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies rate-limited return-to-center (30 deg/s, 10 ms updates) with a 200 ms fallback to PWM disable if center is not reached. ASIL D is correct for FSR-007/SG-003. The rate-limited ramp prevents the correction itself from being hazardous. The 200 ms fallback timeout handles mechanical binding or feedback sensor fault. Traces down to SSR-FZC-004 and SSR-FZC-005 are consistent. The 2-degree tolerance for center detection is reasonable for servo accuracy.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR012 -->

---

#### TSR-013: FZC Steering Servo PWM Disable on Persistent Fault

- **ASIL**: D
- **Traces up**: FSR-007 (SG-003)
- **Traces down**: SSR-FZC-006
- **Allocated to**: FZC
- **Verification method**: Test (PIL + fault injection)
- **Status**: draft

If the steering return-to-center (TSR-012) fails to achieve the center position within 200 ms, the FZC shall disable the TIM1 PWM output for the steering servo by: (a) setting the timer compare register to 0 (0% duty), (b) disabling the timer output enable bit, and (c) configuring the output pin as a GPIO driven LOW. The servo PWM shall not be re-enabled until a full system restart and self-test pass.

**Rationale**: Three-level PWM disable (duty = 0, timer disable, GPIO override) provides defense-in-depth against timer peripheral faults. Requiring system restart prevents automated recovery from a potentially dangerous servo fault.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR013 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies three-level PWM disable (compare register zero, timer output disable, GPIO force LOW) with system restart required for re-enable. ASIL D is correct for FSR-007/SG-003. The defense-in-depth approach addresses timer peripheral faults at three independent levels. Traces down to SSR-FZC-006 are consistent. The system restart requirement prevents automated recovery from a potentially dangerous servo state. This is the most conservative approach for a steering actuator fault.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR013 -->

---

#### TSR-014: FZC Steering Rate Limiting

- **ASIL**: C
- **Traces up**: FSR-008 (SG-003)
- **Traces down**: SSR-FZC-007
- **Allocated to**: FZC
- **Verification method**: Test (unit test + SIL)
- **Status**: draft

The FZC shall limit the rate of change of the steering angle command to a maximum of 30 degrees per second. For each 10 ms control cycle, the maximum angle change per cycle shall be clamped to 0.3 degrees (30 degrees/second multiplied by 0.01 seconds). Additionally, the FZC shall clamp the absolute steering angle command to the range -43 to +43 degrees (2-degree margin inside the -45 to +45 degree mechanical stops).

**Rationale**: Rate limiting prevents sudden steering inputs from CAN corruption or software faults. The angle clamp prevents the servo from reaching mechanical end stops, avoiding stalling and overheating.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR014 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies steering rate limiting (30 deg/s, 0.3 deg per 10 ms cycle) and mechanical range clamping (-43 to +43 deg with 2-degree margin from stops). ASIL C is appropriate for FSR-008/SG-003. The 0.3 deg/cycle calculation is arithmetically correct (30 * 0.01). The mechanical stop margin prevents servo stalling and overheating. Traces down to SSR-FZC-007 are consistent. Note that the 30 deg/s rate limit here matches the return-to-center rate in TSR-012, which is good for consistency but means the return-to-center maneuver operates at the maximum allowed rate with zero margin.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR014 -->

---

### 4.3 Braking Domain (SG-004, SG-005)

---

#### TSR-015: FZC Brake Servo PWM Command Verification

- **ASIL**: D
- **Traces up**: FSR-009 (SG-004, SG-005)
- **Traces down**: SSR-FZC-008, HSR-FZC-002
- **Allocated to**: FZC
- **Verification method**: Test (PIL + hardware test)
- **Status**: draft

The FZC shall verify the brake servo PWM output by reading back the TIM2 timer capture register and comparing it to the commanded duty cycle. The measured duty cycle shall agree with the commanded duty cycle within 2% tolerance. If the discrepancy exceeds 2% for 3 consecutive control cycles (30 ms), a brake PWM fault shall be declared. The brake servo PWM frequency shall be 50 Hz (20 ms period), and the duty cycle shall map linearly from 0% braking (1.0 ms pulse) to 100% braking (2.0 ms pulse).

**Rationale**: Timer capture feedback provides independent verification that the PWM peripheral is producing the commanded output. The 3-cycle debounce prevents false detection from PWM measurement jitter.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR015 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies brake servo PWM readback verification via TIM2 capture register with 2% duty cycle tolerance and 3-cycle (30 ms) debounce. ASIL D is correct for FSR-009/SG-004/SG-005. The approach provides independent verification of the PWM peripheral output. The 50 Hz servo frequency and 1.0-2.0 ms pulse mapping are standard for hobby servos. Traces down to SSR-FZC-008 and HSR-FZC-002 are consistent. The 30 ms detection time is well within the SG-004 FTTI of 50 ms (leaves 20 ms for reaction + actuation per TSR-046).
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR015 -->

---

#### TSR-016: FZC Brake System Fault Notification

- **ASIL**: D
- **Traces up**: FSR-009, FSR-025 (SG-004)
- **Traces down**: SSR-FZC-009
- **Allocated to**: FZC
- **Verification method**: Test (SIL)
- **Status**: draft

Upon detection of a brake system fault (PWM mismatch per TSR-015, or loss of brake servo response), the FZC shall transmit a brake fault CAN message (CAN ID 0x210, E2E protected, highest priority in the braking domain) within 10 ms of fault detection. The message shall include: fault type (2-bit: PWM fault, servo fault, command fault), commanded brake value (8-bit), measured feedback value (8-bit), alive counter (4-bit), and CRC-8 (8-bit).

**Rationale**: Rapid brake fault notification enables the CVC to command motor cutoff as a backup deceleration mechanism within the 50 ms FTTI for SG-004.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR016 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies brake fault CAN notification (ID 0x210, E2E protected, highest braking domain priority) within 10 ms of fault detection. ASIL D is correct for FSR-009/FSR-025/SG-004. The message content (fault type, commanded value, measured feedback, alive counter, CRC-8) provides sufficient diagnostic information for the CVC to react. The 10 ms transmission deadline is achievable and leaves 40 ms within the SG-004 FTTI for CVC reaction and motor cutoff. Traces down to SSR-FZC-009 are consistent. The 2-bit fault type encoding should be documented (which values map to PWM fault, servo fault, command fault) to ensure unambiguous implementation.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR016 -->

---

#### TSR-017: FZC Auto-Brake on CAN Command Timeout

- **ASIL**: D
- **Traces up**: FSR-010 (SG-004)
- **Traces down**: SSR-FZC-010
- **Allocated to**: FZC
- **Verification method**: Test (SIL + fault injection)
- **Status**: draft

The FZC shall maintain a timeout counter for the brake command CAN message from the CVC (CAN ID 0x200). If no valid brake command (passing E2E verification with correct data ID, sequential alive counter, and valid CRC-8) is received within 100 ms, the FZC shall autonomously command maximum braking force (100% brake servo, 2.0 ms pulse width). The auto-brake shall remain applied until the CVC re-establishes valid CAN communication and explicitly sends a brake release command with E2E protection. The auto-brake state shall be latching: a single valid CAN message shall not release auto-brake; the CVC must send 5 consecutive valid brake commands to confirm communication recovery.

**Rationale**: The 100 ms timeout is a tradeoff between false trigger avoidance (normal CAN jitter is under 20 ms) and timely response to CAN loss. The 5-message recovery requirement prevents a single lucky message from releasing auto-brake during intermittent bus failure.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR017 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies autonomous maximum braking on CAN brake command timeout (100 ms) with latching recovery (5 consecutive valid messages required). ASIL D is correct for FSR-010/SG-004. The 100 ms timeout provides good margin over normal CAN jitter (under 20 ms). The latching auto-brake with 5-message recovery is a strong fail-safe design that prevents intermittent bus faults from causing oscillating brake behavior. Traces down to SSR-FZC-010 are consistent. The E2E verification on incoming brake commands (CRC-8, alive counter, Data ID) ensures that only fully valid messages are accepted. This is well-designed for the fail-closed principle.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR017 -->

---

### 4.4 Obstacle Detection Domain (SG-007)

---

#### TSR-018: FZC Lidar Sensor UART Acquisition

- **ASIL**: C
- **Traces up**: FSR-011, FSR-012 (SG-007)
- **Traces down**: SSR-FZC-011, HSR-FZC-003
- **Allocated to**: FZC
- **Verification method**: Test (PIL + hardware test)
- **Status**: draft

The FZC shall receive TFMini-S lidar distance data via UART2 at 115200 baud, 8N1 configuration. The TFMini-S transmits a 9-byte frame at 100 Hz. Each frame shall be validated by: (a) checking the header bytes (0x59, 0x59), (b) computing and verifying the frame checksum (low byte of sum of bytes 0-7), and (c) extracting the distance (bytes 2-3, little-endian, centimeters) and signal strength (bytes 4-5, little-endian). Frames failing header or checksum validation shall be discarded. If no valid frame is received within 100 ms, a sensor communication timeout shall be declared.

**Rationale**: Frame-level validation ensures data integrity beyond UART-level error detection. The 100 ms timeout corresponds to 10 missed frames and reliably detects a disconnected or failed sensor.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR018 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies TFMini-S lidar UART acquisition at 115200 baud with frame validation (header check 0x59/0x59, checksum, distance/strength extraction). ASIL C is appropriate for FSR-011/FSR-012/SG-007. The 9-byte frame format and checksum algorithm match the TFMini-S datasheet. The 100 ms timeout (10 missed frames at 100 Hz) is a reasonable detection threshold. Traces down to SSR-FZC-011 and HSR-FZC-003 are consistent. Note: UART has no hardware-level error correction like CAN, so frame-level validation is the primary data integrity mechanism. The checksum (low byte of sum) provides weak protection -- consider whether this is sufficient for ASIL C or if an additional software-level CRC would be warranted.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR018 -->

---

#### TSR-019: FZC Lidar Graduated Response

- **ASIL**: C
- **Traces up**: FSR-011 (SG-007)
- **Traces down**: SSR-FZC-012
- **Allocated to**: FZC
- **Verification method**: Test (SIL + simulation)
- **Status**: draft

The FZC shall implement the following graduated response to obstacle distance readings from the TFMini-S lidar:

| Zone | Distance Threshold | Action |
|------|-------------------|--------|
| Warning | 100 cm (calibratable) | Activate buzzer (single beep pattern), transmit warning CAN message (ID 0x220) to CVC |
| Braking | 50 cm (calibratable) | Activate buzzer (fast beep), transmit speed reduction request CAN message to CVC, apply 50% braking force |
| Emergency | 20 cm (calibratable) | Activate buzzer (continuous), transmit emergency stop request CAN message to CVC, apply 100% braking force, request motor cutoff |

The distance thresholds shall enforce the invariant: emergency < braking < warning. If a threshold configuration violates this invariant, the system shall use the default values. The response shall use the closest (most critical) zone when the obstacle is within multiple thresholds.

**Rationale**: Graduated response provides proportional reaction. The compile-time adjustability allows tuning for different demonstration environments.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR019 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies three graduated response zones (warning at 100 cm, braking at 50 cm, emergency at 20 cm) with calibratable thresholds and an invariant enforcement (emergency < braking < warning). ASIL C is appropriate for FSR-011/SG-007. The three-zone approach provides proportional response. The invariant check with fallback to defaults is good defensive design. Traces down to SSR-FZC-012 are consistent. The buzzer patterns match TSR-044. Consider documenting hysteresis behavior -- what happens when an obstacle oscillates around a threshold boundary (e.g., at exactly 50 cm). Without hysteresis, the system may rapidly alternate between zones.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR019 -->

---

#### TSR-020: FZC Lidar Range and Stuck Sensor Check

- **ASIL**: C
- **Traces up**: FSR-012 (SG-007)
- **Traces down**: SSR-FZC-013
- **Allocated to**: FZC
- **Verification method**: Test (unit test + fault injection)
- **Status**: draft

The FZC shall perform the following lidar plausibility checks on every valid frame received from the TFMini-S:

1. Range check: Distance values below 2 cm or above 1200 cm shall be flagged as out-of-range. The out-of-range reading shall be replaced with the safe default distance of 0 cm.
2. Signal strength check: Readings with signal strength below 100 (calibratable) shall be discarded and replaced with the safe default distance of 0 cm.
3. Stuck sensor check: If the absolute value of (current distance minus previous distance) is less than 1 cm for 50 consecutive valid samples (approximately 500 ms at 100 Hz), the sensor shall be flagged as stuck and the reading replaced with 0 cm.

**Rationale**: Safe default of 0 cm (obstacle present) is a fail-closed design: any sensor anomaly triggers the most protective response (emergency braking per TSR-019). This ensures the system errs on the side of caution.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR020 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies three lidar plausibility checks: range (2-1200 cm), signal strength (minimum 100), and stuck sensor (less than 1 cm change for 50 consecutive samples). ASIL C is appropriate for FSR-012/SG-007. The safe default of 0 cm (fail-closed to emergency braking) is a sound safety design. Traces down to SSR-FZC-013 are consistent. The stuck sensor check at 500 ms (50 samples at 100 Hz) is reasonable for detecting a frozen sensor. The 1200 cm upper range matches the TFMini-S specification. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR020 -->

---

#### TSR-021: FZC Lidar Sensor Fault Safe Default

- **ASIL**: C
- **Traces up**: FSR-012 (SG-007)
- **Traces down**: SSR-FZC-014
- **Allocated to**: FZC
- **Verification method**: Test (unit test + SIL)
- **Status**: draft

On any lidar sensor fault (communication timeout per TSR-018, range violation per TSR-020, stuck sensor per TSR-020, or signal strength failure per TSR-020), the FZC shall substitute a safe default distance of 0 cm (obstacle present at minimum range) and log a diagnostic trouble code via the Dem module. If the lidar fault persists for more than 2 seconds, the FZC shall request the CVC to transition to DEGRADED mode with speed limiting.

**Rationale**: The safe default of 0 cm causes the graduated response (TSR-019) to invoke emergency braking, which is the most conservative action. Persistent fault escalation to DEGRADED prevents indefinite emergency braking from a permanently failed sensor.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR021 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies lidar fault safe default (0 cm = obstacle present) with DTC logging and escalation to DEGRADED mode after 2 seconds of persistent fault. ASIL C is appropriate for FSR-012/SG-007. The 0 cm default triggers the emergency zone in TSR-019, which is the most conservative action. The 2-second escalation to DEGRADED with speed limiting prevents indefinite emergency braking from a permanently failed sensor, which is a well-considered design. Traces down to SSR-FZC-014 are consistent. The Dem module integration for DTC logging aligns with the AUTOSAR-like BSW architecture.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR021 -->

---

### 4.5 Communication Domain (SG-001 through SG-008)

---

#### TSR-022: E2E Protection Header Format

- **ASIL**: D
- **Traces up**: FSR-013 (SG-001 through SG-008)
- **Traces down**: SSR-CVC-008, SSR-FZC-015, SSR-RZC-008, SSR-SC-001
- **Allocated to**: CVC, FZC, RZC, SC
- **Verification method**: Test (unit test + SIL)
- **Status**: draft

All safety-critical CAN messages shall include an E2E protection header in the first 2 bytes of the CAN payload with the following format:

| Byte | Bits | Field | Description |
|------|------|-------|-------------|
| 0 | 7:4 | Alive counter | 4-bit counter, increments by 1 per transmission (wraps 0-15) |
| 0 | 3:0 | Data ID | 4-bit unique identifier per message type |
| 1 | 7:0 | CRC-8 | CRC-8/SAE-J1850 (polynomial 0x1D, init 0xFF), computed over bytes 2-7 plus the Data ID |

The remaining 6 bytes (bytes 2-7) contain the application data payload. The E2E header format shall be consistent across all ECUs to enable interoperability.

**Rationale**: Placing E2E in a fixed header location simplifies implementation across all ECUs. The CRC-8/SAE-J1850 polynomial provides Hamming distance 4 for payloads up to 8 bytes, detecting all 1-3 bit errors and most multi-bit errors.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR022 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies a fixed 2-byte E2E header format: alive counter (4-bit), Data ID (4-bit), CRC-8/SAE-J1850 (polynomial 0x1D, init 0xFF). ASIL D is correct for FSR-013/SG-001 through SG-008. The fixed header location simplifies cross-ECU implementation. The CRC-8 with Hamming distance 4 is appropriate for 8-byte CAN payloads. Traces down to all four physical ECU SSR sets (CVC, FZC, RZC, SC) are consistent. The CRC computation covering bytes 2-7 plus the Data ID provides protection against both data corruption and message misidentification. The 4-bit Data ID limits the system to 16 distinct message types, which should be verified against the CAN matrix to confirm it is sufficient.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR022 -->

---

#### TSR-023: E2E Transmitter Behavior

- **ASIL**: D
- **Traces up**: FSR-013 (SG-001 through SG-008)
- **Traces down**: SSR-CVC-009, SSR-FZC-016, SSR-RZC-009
- **Allocated to**: CVC, FZC, RZC
- **Verification method**: Test (unit test)
- **Status**: draft

Each E2E-protected CAN message transmitter shall: (a) increment the alive counter by exactly 1 for each transmission (wrapping from 15 to 0), (b) compute the CRC-8 over the data payload concatenated with the Data ID before each transmission, and (c) maintain a separate alive counter per message type (Data ID). The transmitter shall never transmit a message with the same alive counter value as the previous transmission of the same message type.

**Rationale**: Per-message alive counters enable the receiver to detect message loss, repetition, and delay independently for each message type.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR023 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies E2E transmitter behavior: per-message alive counter (increment by 1, wrap 15 to 0), CRC-8 computed per transmission, separate counter per Data ID. ASIL D is correct for FSR-013. The per-message type alive counter is important for independent fault detection across message types. The prohibition against transmitting the same alive counter value twice is a strong anti-repetition measure. Traces down to SSR-CVC-009, SSR-FZC-016, SSR-RZC-009 are consistent. Note: SC is not listed as a transmitter here (only receiver per TSR-024), which is correct since the SC only monitors and does not command.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR023 -->

---

#### TSR-024: E2E Receiver Behavior

- **ASIL**: D
- **Traces up**: FSR-013 (SG-001 through SG-008)
- **Traces down**: SSR-CVC-010, SSR-FZC-017, SSR-RZC-010, SSR-SC-002
- **Allocated to**: CVC, FZC, RZC, SC
- **Verification method**: Test (unit test + fault injection)
- **Status**: draft

Each E2E-protected CAN message receiver shall perform the following checks upon message reception:

1. CRC verification: Recompute CRC-8 over received data payload plus Data ID and compare with received CRC. Mismatch indicates data corruption.
2. Alive counter check: Verify the alive counter has incremented by exactly 1 from the previous reception. A gap greater than 1 indicates message loss; a repeated value indicates message duplication; a decrement indicates message delay or reordering.
3. Data ID check: Verify the Data ID matches the expected value for the CAN message ID.

On E2E verification failure, the receiver shall use the last valid value for up to 1 additional control cycle (10 ms). If E2E verification fails for 3 consecutive receptions, the receiver shall substitute the safe default value for that message type (zero torque, full brake, center steering, as applicable).

**Rationale**: The 3-cycle failure threshold prevents a single corrupted frame from triggering a safe default while ensuring timely detection of persistent communication faults.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR024 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies E2E receiver behavior with three checks (CRC verification, alive counter sequence, Data ID match) and a graduated fault reaction (use last valid for 1 cycle, then safe default after 3 consecutive failures). ASIL D is correct for FSR-013/SG-001 through SG-008. The 3-cycle failure threshold (30 ms) balances between false trigger avoidance and timely detection. The safe default values (zero torque, full brake, center steering) are appropriate fail-closed actions. Traces down to all four ECUs including SC are consistent. The 1-cycle hold of last valid value provides continuity for transient single-frame errors. Well-designed requirement with clear fault classification (corruption, loss, duplication, delay, reordering).
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR024 -->

---

### 4.6 Independent Monitoring Domain (SG-008)

---

#### TSR-025: Zone ECU Heartbeat Transmission

- **ASIL**: C
- **Traces up**: FSR-014 (SG-008)
- **Traces down**: SSR-CVC-011, SSR-FZC-018, SSR-RZC-011
- **Allocated to**: CVC, FZC, RZC
- **Verification method**: Test (SIL)
- **Status**: draft

Each zone ECU (CVC, FZC, RZC) shall transmit a heartbeat CAN message at a fixed interval of 50 ms (tolerance: +/- 5 ms). The heartbeat CAN message shall use the following CAN IDs: CVC = 0x010, FZC = 0x011, RZC = 0x012. Each heartbeat message shall contain: ECU identifier (8-bit), operating mode (4-bit: INIT/RUN/DEGRADED/LIMP/SAFE_STOP/SHUTDOWN), fault status summary (8-bit bitmask), alive counter (4-bit), and CRC-8 (8-bit). Total payload: 4 bytes.

**Rationale**: Fixed CAN IDs per ECU simplify SC filtering. The operating mode field enables the SC to verify that all ECUs are in the expected state. The fault status summary provides the SC with a snapshot of each ECU's health.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR025 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies zone ECU heartbeat at 50 ms intervals (+/- 5 ms) with fixed CAN IDs (0x010/0x011/0x012) containing ECU ID, operating mode, fault status, alive counter, and CRC-8. ASIL C is appropriate for FSR-014/SG-008. The 4-byte payload is compact and efficient for CAN 2.0B. The 50 ms interval gives the SC 3 heartbeat opportunities within the 150 ms timeout window (TSR-027). Traces down to SSR-CVC-011, SSR-FZC-018, SSR-RZC-011 are consistent. The operating mode and fault status fields enable the SC to perform mode-aware monitoring beyond simple alive detection. The +/- 5 ms tolerance (10%) is reasonable for RTOS scheduling jitter.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR025 -->

---

#### TSR-026: Zone ECU Heartbeat Conditioning on Self-Check

- **ASIL**: C
- **Traces up**: FSR-014, FSR-017 (SG-008)
- **Traces down**: SSR-CVC-012, SSR-FZC-019, SSR-RZC-012
- **Allocated to**: CVC, FZC, RZC
- **Verification method**: Test (unit test + fault injection)
- **Status**: draft

Each zone ECU shall only transmit a heartbeat message when the following conditions are met within the current heartbeat period: (a) the main control loop has completed at least one full iteration, (b) the stack canary value is intact (not overwritten), and (c) the CAN controller status register does not indicate bus-off state. If any of these conditions fail, the ECU shall not transmit the heartbeat, which will be detected as a heartbeat timeout by the SC (TSR-027).

**Rationale**: Conditioning the heartbeat on successful self-checks ensures that a heartbeat transmission implies the ECU firmware is executing correctly. A hung or corrupted ECU will fail to meet these conditions and will stop transmitting heartbeats, triggering SC detection.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR026 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies three preconditions for heartbeat transmission: main loop completion, stack canary integrity, and CAN controller not in bus-off state. ASIL C is appropriate for FSR-014/FSR-017/SG-008. This is an excellent design pattern -- conditioning the heartbeat on self-checks transforms it from a simple alive signal into a firmware health indicator. A hung, stack-corrupted, or CAN-faulted ECU will automatically stop transmitting, triggering SC detection. Traces down to SSR-CVC-012, SSR-FZC-019, SSR-RZC-012 are consistent. The pattern is consistent with TSR-031 (watchdog conditioning) -- both use the same self-check paradigm for independent monitoring paths.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR026 -->

---

#### TSR-027: SC Heartbeat Timeout Detection

- **ASIL**: C
- **Traces up**: FSR-015 (SG-008)
- **Traces down**: SSR-SC-003, SSR-SC-004
- **Allocated to**: SC
- **Verification method**: Test (SIL + fault injection)
- **Status**: draft

The Safety Controller shall maintain an independent timeout counter for each zone ECU heartbeat (CVC, FZC, RZC). The SC shall declare a heartbeat timeout for a specific ECU when no valid heartbeat message (passing E2E CRC and alive counter verification) is received from that ECU within 150 ms (3 heartbeat periods). On heartbeat timeout detection, the SC shall illuminate the fault LED corresponding to the failed ECU (GIO_A1 = CVC, GIO_A2 = FZC, GIO_A3 = RZC).

**Rationale**: The 150 ms timeout (3 missed heartbeats) provides margin for CAN bus scheduling jitter while enabling timely detection. Independent timeout counters per ECU enable the SC to identify which specific ECU has failed.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR027 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies SC heartbeat timeout detection at 150 ms (3 missed heartbeats at 50 ms interval) with independent timeout counters per ECU and fault LED indication (GIO_A1/A2/A3). ASIL C is appropriate for FSR-015/SG-008. The 150 ms timeout provides 3 missed heartbeat opportunities, which is a reasonable balance between false-trigger avoidance and detection speed. Per-ECU LED indication is valuable for diagnostics. Traces down to SSR-SC-003 and SSR-SC-004 are consistent. The E2E verification of heartbeats (CRC + alive counter) ensures the SC only accepts valid heartbeats, preventing a spoofed message from masking a real failure.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR027 -->

---

#### TSR-028: SC Heartbeat Timeout Confirmation and Kill Relay

- **ASIL**: D
- **Traces up**: FSR-015, FSR-016 (SG-008, SG-001, SG-003, SG-004)
- **Traces down**: SSR-SC-005
- **Allocated to**: SC
- **Verification method**: Test (SIL + hardware test)
- **Status**: draft

After detecting a heartbeat timeout (TSR-027), the SC shall wait for a confirmation period of 50 ms. If the heartbeat from the failed ECU is still absent after the confirmation period, the SC shall de-energize the kill relay by driving GIO_A0 LOW within 1 ms. The confirmation delay shall not apply if a lockstep CPU comparison error is detected (immediate relay de-energize). The kill relay shall not be re-energized without a complete system power cycle.

**Rationale**: The 50 ms confirmation period reduces false-positive relay trips caused by transient CAN bus contention or overload. Total detection time: 150 ms timeout + 50 ms confirmation = 200 ms, plus 5 ms relay dropout = 205 ms. While exceeding the 100 ms FTTI for SG-008, the SC self-test and lockstep (TSR-038) provide faster coverage for SC-internal faults.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR028 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies a 50 ms confirmation period after heartbeat timeout before kill relay de-energize, with an exception for lockstep CPU errors (immediate). ASIL D is correct for FSR-015/FSR-016/SG-008/SG-001/SG-003/SG-004. The total detection time of 205 ms (150 ms timeout + 50 ms confirmation + 5 ms relay dropout) exceeds the SG-008 FTTI of 100 ms -- the rationale acknowledges this and argues that lockstep covers SC-internal faults faster. This FTTI exceedance should be formally documented in the safety case with the argument that the heartbeat monitoring path is a backup to the zone ECU's own internal diagnostics. Traces down to SSR-SC-005 are consistent. The power-cycle-only re-energize requirement prevents automated recovery.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR028 -->

---

#### TSR-029: SC Kill Relay GPIO Control

- **ASIL**: D
- **Traces up**: FSR-016 (SG-001, SG-003, SG-004, SG-008)
- **Traces down**: SSR-SC-006, HSR-SC-001
- **Allocated to**: SC
- **Verification method**: Test (hardware test + inspection)
- **Status**: draft

The SC shall control the kill relay via GIO_A0 configured as a push-pull digital output. The relay circuit shall use an energize-to-run configuration: GIO_A0 HIGH drives an N-channel MOSFET gate, which energizes the relay coil, closing the relay contacts and allowing 12V power to flow to the motor driver and servo circuits. GIO_A0 LOW (or any loss of drive) de-energizes the relay, opening the contacts and removing actuator power. The GPIO output shall be verified by reading back the GIO_A0 data-in register after each state change to confirm the pin is in the commanded state.

**Rationale**: Energize-to-run ensures that any SC failure (power loss, hang, GPIO fault) results in the safe state (relay open, power removed). GPIO readback provides a software-level diagnostic of the output driver.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR029 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies kill relay GPIO control via GIO_A0 with energize-to-run configuration (N-channel MOSFET gate driver) and GPIO readback verification. ASIL D is correct for FSR-016/SG-001/SG-003/SG-004/SG-008. The energize-to-run design is the gold standard for safety relays -- any failure mode (power loss, hang, GPIO tri-state) defaults to safe state (relay open). The GPIO readback via data-in register provides a software diagnostic of the output driver state. Traces down to SSR-SC-006 and HSR-SC-001 are consistent. The MOSFET gate drive approach is appropriate for the relay coil current. This is a well-designed safety-critical actuator path.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR029 -->

---

#### TSR-030: SC Kill Relay De-energize Conditions

- **ASIL**: D
- **Traces up**: FSR-016 (SG-001, SG-003, SG-004, SG-008)
- **Traces down**: SSR-SC-007
- **Allocated to**: SC
- **Verification method**: Test (SIL + fault injection)
- **Status**: draft

The SC shall de-energize the kill relay (GIO_A0 = LOW) within 5 ms of any of the following confirmed conditions:

1. Heartbeat timeout from any zone ECU (after 50 ms confirmation per TSR-028).
2. Cross-plausibility fault between torque request and motor current (TSR-041).
3. SC startup self-test failure (TSR-037).
4. SC runtime self-test failure (TSR-038).
5. Lockstep CPU comparison error (detected by ESM — immediate, no confirmation delay).
6. SC external watchdog timeout (TPS3823 RESET assertion — hardware-initiated).

Once de-energized, the kill relay shall not be re-energized until a complete system power cycle and successful startup self-test. The SC shall set an internal latch flag to prevent re-energization by any software path.

**Rationale**: Multiple independent trigger conditions ensure that any detected safety-relevant fault results in actuator power removal. The software latch prevents automated recovery from a potentially dangerous state.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR030 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies six independent kill relay de-energize conditions: heartbeat timeout, cross-plausibility fault, startup self-test failure, runtime self-test failure, lockstep CPU error (immediate), and external watchdog timeout (hardware-initiated). ASIL D is correct for FSR-016/SG-001/SG-003/SG-004/SG-008. The 5 ms reaction time from confirmed condition to relay de-energize is achievable and tight. The software latch preventing re-energization without power cycle is essential for safety. Traces down to SSR-SC-007 are consistent. The distinction between confirmation-required (conditions 1-4) and immediate (conditions 5-6) is well-reasoned -- lockstep and watchdog faults are hardware-detected and need no software confirmation.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR030 -->

---

### 4.7 Watchdog Domain (SG-008)

---

#### TSR-031: External Watchdog Feed Conditioning

- **ASIL**: D
- **Traces up**: FSR-017 (SG-008)
- **Traces down**: SSR-CVC-013, SSR-FZC-020, SSR-RZC-013, SSR-SC-008
- **Allocated to**: CVC, FZC, RZC, SC
- **Verification method**: Test (unit test + fault injection)
- **Status**: draft

Each physical ECU shall toggle the TPS3823 WDI (watchdog input) pin at a rate sufficient to prevent watchdog timeout (toggle period less than 1.6 seconds). The watchdog toggle shall be executed only when ALL of the following conditions are met within the current main loop iteration:

1. The main control loop has completed one full iteration (all runnables executed).
2. The stack canary value (a known constant written at stack initialization) matches the expected value (not overwritten by stack overflow).
3. A RAM integrity check of a 32-byte test pattern has passed (verifies RAM is not corrupted).
4. The CAN controller status register indicates the controller is not in bus-off state.

If any condition fails, the watchdog shall not be toggled, and the TPS3823 shall reset the MCU after the timeout period expires.

**Rationale**: Conditioning the watchdog feed on multiple self-check conditions transforms the watchdog from a simple hang detector into a broader firmware health monitor. A corrupted stack, RAM fault, or CAN failure will prevent the watchdog feed and cause a hardware reset.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR031 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies four preconditions for watchdog toggle: main loop completion, stack canary integrity, RAM test pattern verification, and CAN controller not bus-off. ASIL D is correct for FSR-017/SG-008. This mirrors the heartbeat conditioning pattern (TSR-026) but adds a RAM integrity check (condition 3), which is appropriate since the watchdog is the last-resort safety mechanism. The four conditions together transform the TPS3823 from a hang detector into a comprehensive firmware health monitor. Traces down to SSR-CVC-013, SSR-FZC-020, SSR-RZC-013, SSR-SC-008 are consistent across all four physical ECUs. The 1.6-second timeout provides substantial margin over the 10-50 ms main loop period.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR031 -->

---

#### TSR-032: External Watchdog Hardware Configuration

- **ASIL**: D
- **Traces up**: FSR-017 (SG-008)
- **Traces down**: HSR-CVC-002, HSR-FZC-004, HSR-RZC-003, HSR-SC-002
- **Allocated to**: CVC, FZC, RZC, SC
- **Verification method**: Inspection (schematic review) + hardware test
- **Status**: draft

Each ECU shall be equipped with a TPS3823 external watchdog IC. The TPS3823 shall be configured with an external capacitor to set the watchdog timeout period to 1.6 seconds (+/- 20% tolerance). The TPS3823 RESET output shall be connected to the MCU's hardware reset pin (NRST for STM32G474RE, nRST for TMS570LC43x). The TPS3823 shall have its own power supply derived from the same 3.3V rail as the MCU, ensuring it remains powered whenever the MCU is powered.

**Rationale**: The TPS3823 is an independent IC with its own internal oscillator, providing true independence from the MCU's clock domain. The 1.6 second timeout provides ample margin for the main loop (which should complete within 10-50 ms) while detecting firmware hangs within a reasonable time.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR032 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies TPS3823 external watchdog hardware configuration: 1.6 second timeout (capacitor-set), RESET connected to MCU hardware reset pin, powered from same 3.3V rail. ASIL D is correct for FSR-017/SG-008. The TPS3823 with its independent internal oscillator provides true clock-domain independence from the MCU. The hardware reset connection ensures that a watchdog timeout results in a full MCU reset, not just a software interrupt. Traces down to HSR-CVC-002, HSR-FZC-004, HSR-RZC-003, HSR-SC-002 are consistent. The +/- 20% tolerance on the 1.6 second timeout (1.28-1.92 seconds) should be considered in timing analysis -- worst case 1.28 seconds is still well above the main loop period.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR032 -->

---

### 4.8 Emergency Stop Domain (SG-001, SG-008)

---

#### TSR-033: CVC E-Stop GPIO Detection

- **ASIL**: B
- **Traces up**: FSR-018 (SG-001, SG-008)
- **Traces down**: SSR-CVC-014, HSR-CVC-003
- **Allocated to**: CVC
- **Verification method**: Test (hardware test)
- **Status**: draft

The CVC shall detect E-stop button activation via a hardware interrupt on GPIO PC13 configured as falling-edge triggered with internal pull-up enabled. An external RC hardware debounce circuit (R = 10k ohm, C = 100 nF, time constant = 1 ms) shall filter contact bounce. The interrupt service routine shall confirm the E-stop state by re-reading the GPIO pin after 1 ms to verify the level is stable LOW. The total detection time from button press to confirmed E-stop state shall not exceed 2 ms.

**Rationale**: Hardware interrupt provides minimum detection latency. The RC debounce and software confirmation prevent false triggers from contact bounce (typical switch bounce: 1-10 ms).

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR033 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies E-stop detection via GPIO PC13 falling-edge interrupt with hardware RC debounce (R=10k, C=100nF, tau=1ms) and 1 ms software confirmation re-read. ASIL B is appropriate for FSR-018/SG-001/SG-008. The dual-layer debounce (hardware RC + software confirmation) provides robust false trigger rejection. The 2 ms total detection time is well within any FTTI. Traces down to SSR-CVC-014 and HSR-CVC-003 are consistent. The internal pull-up configuration ensures the pin defaults to HIGH (not-pressed) when the button is open. The time constant of 1 ms is appropriate for mechanical switch bounce filtering.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR033 -->

---

#### TSR-034: CVC E-Stop CAN Broadcast and Local Reaction

- **ASIL**: B
- **Traces up**: FSR-018 (SG-001, SG-008)
- **Traces down**: SSR-CVC-015
- **Allocated to**: CVC (broadcast), FZC and RZC (reception)
- **Verification method**: Test (SIL + PIL)
- **Status**: draft

Upon confirmed E-stop detection (TSR-033), the CVC shall within 1 ms: (a) set the internal torque request to zero, (b) transition the vehicle state machine to SAFE_STOP, and (c) transmit a high-priority E-stop CAN message (CAN ID 0x001, highest priority on the bus) with E2E protection. The E-stop CAN message shall be transmitted at 10 ms intervals until the E-stop button is released. All receiving ECUs shall react within 10 ms of E-stop CAN message reception:

- RZC: Disable motor driver (R_EN = LOW, L_EN = LOW, RPWM = 0, LPWM = 0).
- FZC: Apply maximum braking force (100%), command steering to center (0 degrees), activate continuous buzzer.

The system shall remain in SAFE_STOP until the E-stop is released AND a manual restart sequence is performed (full ignition cycle).

**Rationale**: CAN ID 0x001 has the highest priority in CAN 2.0B arbitration (lowest ID wins), ensuring transmission even under high bus load. Repeated transmission every 10 ms provides redundancy against a single lost message.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR034 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies CVC E-stop reaction (1 ms local: zero torque + SAFE_STOP + CAN broadcast) and ECU-level reactions (RZC: motor disable, FZC: max brake + center steering + buzzer) within 10 ms. ASIL B is appropriate for FSR-018/SG-001/SG-008. CAN ID 0x001 as highest priority ensures transmission under bus contention. The repeated 10 ms transmission provides message redundancy. The SAFE_STOP latching until ignition cycle prevents accidental resumption. Traces down to SSR-CVC-015 are consistent. The multi-ECU reaction specification is comprehensive, covering motor, braking, steering, and audible warning. The "ignition cycle" release requirement should be clearly defined in terms of the physical mechanism (power relay cycle).
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR034 -->

---

### 4.9 State Management Domain (SG-001 through SG-008)

---

#### TSR-035: CVC Vehicle State Machine Implementation

- **ASIL**: D
- **Traces up**: FSR-019 (SG-001 through SG-008)
- **Traces down**: SSR-CVC-016, SSR-CVC-017
- **Allocated to**: CVC
- **Verification method**: Test (unit test + SIL + formal verification of transition table)
- **Status**: draft

The CVC shall implement a deterministic vehicle state machine with the following states: INIT (0), RUN (1), DEGRADED (2), LIMP (3), SAFE_STOP (4), SHUTDOWN (5). The state machine shall be implemented as a transition table with explicit enumeration of all valid (state, event) pairs. Any (state, event) pair not present in the transition table shall be rejected, and the attempted invalid transition shall be logged as a DTC. The only valid transitions are:

| Current State | Event | Next State |
|---------------|-------|------------|
| INIT | Self-test passed | RUN |
| INIT | Self-test failed | SAFE_STOP |
| RUN | Minor fault | DEGRADED |
| RUN | Critical fault | SAFE_STOP |
| RUN | E-stop | SAFE_STOP |
| DEGRADED | All faults cleared + 5s recovery | RUN |
| DEGRADED | Persistent fault (5s) | LIMP |
| DEGRADED | Critical fault | SAFE_STOP |
| DEGRADED | E-stop | SAFE_STOP |
| LIMP | Critical fault | SAFE_STOP |
| LIMP | E-stop | SAFE_STOP |
| SAFE_STOP | Controlled shutdown complete | SHUTDOWN |
| SAFE_STOP | E-stop | SAFE_STOP (remain) |
| Any | SC kill relay | SHUTDOWN |

**Rationale**: A transition table implementation is deterministic and verifiable. Explicit rejection of undefined transitions prevents the system from entering an undefined state. The limited set of transitions ensures a monotonic progression toward safer states (with the sole exception of DEGRADED to RUN recovery).

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR035 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies a deterministic vehicle state machine with 6 states (INIT, RUN, DEGRADED, LIMP, SAFE_STOP, SHUTDOWN) and an explicit transition table with DTC logging for invalid transitions. ASIL D is correct for FSR-019/SG-001 through SG-008. The transition table approach is highly verifiable and suitable for formal verification. The monotonic progression toward safer states (with only the DEGRADED-to-RUN recovery exception requiring 5-second clear window) is a sound safety design. Traces down to SSR-CVC-016 and SSR-CVC-017 are consistent. The "Any state + SC kill relay = SHUTDOWN" rule ensures the hardware safety monitor always wins. The 5-second recovery timer from DEGRADED to RUN prevents oscillation. This is a well-structured safety state machine.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR035 -->

---

#### TSR-036: CVC Vehicle State CAN Broadcast

- **ASIL**: D
- **Traces up**: FSR-019 (SG-001 through SG-008)
- **Traces down**: SSR-CVC-018
- **Allocated to**: CVC
- **Verification method**: Test (SIL)
- **Status**: draft

The CVC shall broadcast the current vehicle state on CAN message ID 0x100 at a fixed rate of 100 Hz (10 ms period) with E2E protection. The message shall contain: vehicle state (4-bit), active fault bitmask (16-bit), torque limit percentage (8-bit), speed limit percentage (8-bit), alive counter (4-bit), and CRC-8 (8-bit). Total payload: 6 bytes. All ECUs shall use this message to synchronize their local operating mode via BswM.

**Rationale**: 10 ms broadcast rate ensures all ECUs receive state updates within one control cycle. The torque and speed limit fields enable RZC and FZC to enforce the correct operating limits for each degradation level without requiring local computation.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR036 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies vehicle state CAN broadcast (ID 0x100) at 100 Hz with E2E protection, containing state, fault bitmask, torque/speed limits, alive counter, and CRC-8. ASIL D is correct for FSR-019/SG-001 through SG-008. The 10 ms broadcast rate ensures all ECUs are synchronized within one control cycle. Including torque and speed limit percentages in the broadcast eliminates the need for distributed limit computation, reducing complexity in RZC and FZC. Traces down to SSR-CVC-018 are consistent. The 6-byte payload fits within CAN 2.0B's 8-byte limit with room for E2E overhead. The BswM synchronization mechanism is appropriate for the AUTOSAR-like architecture.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR036 -->

---

#### TSR-037: CVC State Persistence in Non-Volatile Memory

- **ASIL**: D
- **Traces up**: FSR-019 (SG-001 through SG-008)
- **Traces down**: SSR-CVC-019
- **Allocated to**: CVC
- **Verification method**: Test (unit test + power-cycle test)
- **Status**: draft

The CVC shall store the current vehicle state machine state and the active fault bitmask in a dedicated non-volatile memory region (flash sector or backup SRAM with battery) before entering SAFE_STOP or SHUTDOWN. On startup, the CVC shall read the persisted state. If the persisted state is SAFE_STOP or SHUTDOWN with active faults, the CVC shall not transition to RUN until the faults have been acknowledged and cleared. The NVM write shall use a dual-copy scheme with CRC protection: two copies of the state are written, and on read, both copies are compared; if they disagree, the copy with a valid CRC is used.

**Rationale**: State persistence prevents "reset-washing" of faults, where a quick power cycle clears the fault state and allows the system to resume without addressing the underlying problem. The dual-copy CRC scheme protects against NVM corruption.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR037 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies NVM state persistence with dual-copy CRC protection, preventing reset-washing of faults. ASIL D is correct for FSR-019/SG-001 through SG-008. The anti-reset-washing design is critical for safety -- without it, an operator could simply cycle power to clear a fault without addressing the root cause. The dual-copy CRC scheme provides protection against NVM corruption during write (power loss mid-write). Traces down to SSR-CVC-019 are consistent. Consider specifying the flash wear-leveling strategy -- if state transitions happen frequently, a single flash sector could wear out. Also specify behavior when both NVM copies have invalid CRCs (fresh device or double corruption).
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR037 -->

---

### 4.10 CAN Bus Loss Domain (SG-008)

---

#### TSR-038: Per-ECU CAN Bus Loss Detection

- **ASIL**: C
- **Traces up**: FSR-020 (SG-008)
- **Traces down**: SSR-CVC-020, SSR-FZC-021, SSR-RZC-014, SSR-SC-009
- **Allocated to**: CVC, FZC, RZC, SC
- **Verification method**: Test (SIL + fault injection)
- **Status**: draft

Each ECU shall monitor the CAN controller error counters and the message reception activity. A CAN bus loss shall be declared when any of the following conditions is detected:

1. CAN controller enters bus-off state (transmit error counter exceeds 255).
2. No CAN messages of any type are received for more than 200 ms.
3. CAN error counter exceeds the warning threshold (96) for more than 500 ms.

On CAN bus loss detection, each ECU shall independently enter its local safe state:
- CVC: Transition to SAFE_STOP, set torque to zero, display CAN fault on OLED.
- FZC: Apply auto-brake (per TSR-017), return steering to center (per TSR-012).
- RZC: Disable motor driver, set torque to zero.
- SC: After heartbeat timeout (TSR-027), de-energize kill relay.

**Rationale**: Each ECU must handle CAN loss independently because the CAN bus is the single communication path. The 200 ms timeout is longer than individual message timeouts to avoid false triggers from single message loss.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR038 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies three CAN bus loss detection methods (bus-off state, 200 ms message silence, error counter warning for 500 ms) with per-ECU independent safe state actions. ASIL C is appropriate for FSR-020/SG-008. The three detection methods cover different CAN failure modes: bus-off (hard fault), silence (disconnection), and error count (degrading transceiver). The per-ECU safe state actions are well-specified and appropriate for each ECU's function. Traces down to SSR-CVC-020, SSR-FZC-021, SSR-RZC-014, SSR-SC-009 are consistent. The 200 ms timeout is correctly differentiated from individual message timeouts. The SC path through heartbeat timeout (TSR-027) provides an independent backup for CAN loss detection.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR038 -->

---

#### TSR-039: CVC CAN Bus Recovery

- **ASIL**: C
- **Traces up**: FSR-020 (SG-008)
- **Traces down**: SSR-CVC-021
- **Allocated to**: CVC
- **Verification method**: Test (SIL + fault injection)
- **Status**: draft

Upon CAN bus loss detection, the CVC shall attempt automatic recovery per the CAN 2.0B bus-off recovery protocol (128 occurrences of 11 consecutive recessive bits). The CVC shall limit recovery attempts to 3 within a 10-second window. If recovery fails after 3 attempts, the CVC shall transition to SHUTDOWN and cease all CAN transmission attempts. During recovery attempts, the CVC shall maintain the SAFE_STOP state (motor disabled, brakes applied).

**Rationale**: Automatic recovery handles transient bus faults (e.g., EMI burst). Limiting recovery attempts prevents an ECU with a faulty CAN transceiver from disrupting the bus with repeated error frames.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR039 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies CAN bus-off recovery per CAN 2.0B protocol with 3-attempt limit in a 10-second window, transitioning to SHUTDOWN on exhaustion. ASIL C is appropriate for FSR-020/SG-008. Limiting recovery attempts to 3 is a good balance between allowing transient fault recovery and preventing a faulty transceiver from disrupting the bus. The SAFE_STOP maintenance during recovery attempts ensures safety. Traces down to SSR-CVC-021 are consistent. The SHUTDOWN transition after 3 failed attempts is appropriate -- a CAN bus that cannot recover after 3 attempts likely has a hardware fault requiring service.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR039 -->

---

### 4.11 Motor Direction Safety Domain (SG-001)

---

#### TSR-040: RZC Motor Direction Plausibility and Shoot-Through Protection

- **ASIL**: C
- **Traces up**: FSR-021 (SG-001)
- **Traces down**: SSR-RZC-015, SSR-RZC-016
- **Allocated to**: RZC
- **Verification method**: Test (PIL + hardware test)
- **Status**: draft

The RZC shall verify motor rotation direction by comparing the commanded direction (derived from the CVC torque request CAN message: positive = forward, negative = reverse) against the actual direction determined by the quadrature encoder on TIM3. If the actual direction does not match the commanded direction within 50 ms of the command, the RZC shall disable the motor driver and log a DTC.

Additionally, the RZC shall enforce shoot-through protection: simultaneous non-zero values on both RPWM and LPWM shall be prevented by hardware timer configuration (complementary output with forced dead-time) and software guards (mutual exclusion check before writing PWM registers). A minimum dead-time of 10 microseconds shall be enforced between direction changes (both PWM outputs = 0% for at least 10 us before the new direction PWM is applied).

**Rationale**: Direction verification using encoder feedback provides independent confirmation of motor behavior. Shoot-through protection prevents simultaneous conduction of both H-bridge legs, which would cause a short circuit through the BTS7960.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR040 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies motor direction plausibility via quadrature encoder (TIM3) comparison with CAN command direction, and H-bridge shoot-through protection via hardware dead-time and software mutual exclusion. ASIL C is appropriate for FSR-021/SG-001. The 50 ms direction verification window is reasonable for motor inertia. The dual-layer shoot-through protection (hardware complementary output with dead-time + software guards) provides defense-in-depth against H-bridge short circuits. The 10 microsecond dead-time between direction changes is standard for BTS7960 applications. Traces down to SSR-RZC-015 and SSR-RZC-016 are consistent. Consider specifying the minimum motor speed threshold below which direction verification is not meaningful (motor stall or very low speed may produce unreliable encoder readings).
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR040 -->

---

### 4.12 Cross-Plausibility Domain (SG-001)

---

#### TSR-041: SC Torque-Current Cross-Plausibility Check

- **ASIL**: C
- **Traces up**: FSR-022 (SG-001)
- **Traces down**: SSR-SC-010, SSR-SC-011
- **Allocated to**: SC
- **Verification method**: Test (SIL + fault injection)
- **Status**: draft

The SC shall receive the torque request CAN message from the CVC (CAN ID 0x100) and the motor current CAN message from the RZC (CAN ID 0x301), both with E2E verification. The SC shall maintain a 16-entry torque-to-current lookup table mapping torque request percentage (0-100%) to expected motor current (0-25000 mA). The SC shall compute the expected current for the received torque request and compare it to the actual measured current. If the absolute difference between expected and actual current exceeds 20% (calibratable) of the expected value for a continuous period of 50 ms (5 consecutive 10 ms comparisons), the SC shall declare a cross-plausibility fault.

**Rationale**: The lookup table approach is simpler and more verifiable than a continuous mathematical model, appropriate for the TMS570's role as an independent safety monitor. The 50 ms debounce allows for motor transient dynamics during acceleration and deceleration.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR041 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies SC torque-current cross-plausibility using a 16-entry lookup table with 20% tolerance and 50 ms (5-cycle) debounce. ASIL C is appropriate for FSR-022/SG-001. The lookup table approach is well-suited for the TMS570 SC role -- simple, verifiable, and deterministic. The 20% tolerance (calibratable) accounts for motor parameter variation and mechanical load differences. The 50 ms debounce is reasonable for motor transient dynamics. Traces down to SSR-SC-010 and SSR-SC-011 are consistent. E2E verification on both input CAN messages (torque from CVC, current from RZC) ensures the SC is working with valid data. The 16-entry table granularity should be verified as sufficient for the motor's torque-current characteristic.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR041 -->

---

#### TSR-042: SC Cross-Plausibility Fault Reaction

- **ASIL**: C
- **Traces up**: FSR-022 (SG-001)
- **Traces down**: SSR-SC-012
- **Allocated to**: SC
- **Verification method**: Test (SIL + hardware test)
- **Status**: draft

Upon cross-plausibility fault detection (TSR-041), the SC shall: (a) illuminate the system fault LED (GIO_A4), (b) de-energize the kill relay (GIO_A0 = LOW) within 5 ms, and (c) enter the SHUTDOWN state internally. The SC shall not re-energize the kill relay without a complete system power cycle.

**Rationale**: Cross-plausibility fault indicates a discrepancy in the motor control path that cannot be explained by normal operation. The most conservative response (kill relay de-energize) is appropriate because the SC cannot determine which signal (torque request or motor current) is incorrect.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR042 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies SC cross-plausibility fault reaction: system fault LED (GIO_A4), kill relay de-energize (GIO_A0 LOW within 5 ms), and SHUTDOWN state entry. ASIL C is appropriate for FSR-022/SG-001. The kill relay de-energize is the correct response since the SC cannot determine which signal (torque or current) is incorrect. The 5 ms reaction time is consistent with TSR-030 condition 2. Traces down to SSR-SC-012 are consistent. The power-cycle-only recovery is appropriate for a condition that indicates a potential control path discrepancy. The fault LED provides visual indication to the operator for diagnostics.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR042 -->

---

### 4.13 Operator Warning Domain (SG-001 through SG-008)

---

#### TSR-043: CVC OLED Warning Display

- **ASIL**: B
- **Traces up**: FSR-023 (SG-001 through SG-008)
- **Traces down**: SSR-CVC-022
- **Allocated to**: CVC
- **Verification method**: Test (PIL)
- **Status**: draft

The CVC shall update the OLED display (SSD1306, 128x64, I2C) within 200 ms of any vehicle state transition from RUN to a degraded state (DEGRADED, LIMP, SAFE_STOP, SHUTDOWN). The display shall show: current vehicle state name (line 1), triggering fault code (line 2), and operational restrictions (torque limit %, speed limit %) (line 3). The OLED update shall occur at a minimum rate of 5 Hz (200 ms) during any non-RUN state.

**Rationale**: The 200 ms update latency meets the FSR-023 requirement for operator warning within 200 ms. The three-line display format provides the operator with essential information: what state the system is in, why, and what limits apply.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR043 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies CVC OLED display update (SSD1306, 128x64, I2C) within 200 ms of state transition, showing state name, fault code, and operational restrictions at minimum 5 Hz. ASIL B is appropriate for FSR-023/SG-001 through SG-008. The 200 ms latency meets the FSR-023 requirement. The three-line display format is efficient for operator situational awareness. Traces down to SSR-CVC-022 are consistent. The I2C interface to the SSD1306 is a non-safety-critical path (display is informational only), so ASIL B is sufficient. Consider specifying display behavior when I2C communication with the SSD1306 fails -- the system should log the display fault but not transition to a safer state for a display-only failure.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR043 -->

---

#### TSR-044: FZC Buzzer Warning Patterns

- **ASIL**: B
- **Traces up**: FSR-023 (SG-001 through SG-008)
- **Traces down**: SSR-FZC-022
- **Allocated to**: FZC
- **Verification method**: Test (PIL)
- **Status**: draft

The FZC shall drive the piezo buzzer via GPIO using the following patterns based on the vehicle state received from the CVC state broadcast (TSR-036) or local fault detection:

| Vehicle State | Buzzer Pattern |
|---------------|---------------|
| DEGRADED | Single beep: 100 ms ON, then OFF |
| LIMP | Slow repeating: 500 ms ON, 500 ms OFF |
| SAFE_STOP | Fast repeating: 100 ms ON, 100 ms OFF |
| Emergency (E-stop or obstacle emergency zone) | Continuous ON |

The buzzer shall be activated within 100 ms of state change detection. The FZC shall also activate the buzzer locally (without CAN command) when the lidar detects an obstacle in the warning zone (single beep), braking zone (fast beep), or emergency zone (continuous).

**Rationale**: Distinct buzzer patterns enable the operator to identify the severity level by sound alone, without visual attention. Local activation for lidar ensures audible warning even if CAN communication is degraded.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR044 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies four distinct buzzer patterns (single beep, slow repeat, fast repeat, continuous) mapped to vehicle states and local lidar detection. ASIL B is appropriate for FSR-023/SG-001 through SG-008. The distinct patterns enable severity identification by sound alone. The dual activation path (CAN-based state change + local lidar detection) ensures audible warning even during CAN degradation. Traces down to SSR-FZC-022 are consistent. The 100 ms buzzer activation deadline is well within human perception limits. The local lidar buzzer activation independent of CAN is a good fail-independent design.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR044 -->

---

#### TSR-045: SC Fault LED Indication

- **ASIL**: B
- **Traces up**: FSR-023 (SG-001 through SG-008)
- **Traces down**: SSR-SC-013, HSR-SC-003
- **Allocated to**: SC
- **Verification method**: Test (hardware test)
- **Status**: draft

The SC shall drive 4 fault LEDs on dedicated GPIO pins (GIO_A1 = CVC fault, GIO_A2 = FZC fault, GIO_A3 = RZC fault, GIO_A4 = system fault) to indicate detected faults. The LEDs shall be driven independently of CAN communication (direct GPIO from SC). LED behavior:

- Steady ON: Heartbeat timeout detected for the corresponding ECU, or system fault (cross-plausibility, self-test failure).
- Blinking (500 ms period): Heartbeat received but ECU reports degraded state.
- OFF: ECU operating normally.

All fault LEDs shall be tested during SC startup by briefly illuminating each LED for 500 ms (lamp test).

**Rationale**: GPIO-driven LEDs provide a CAN-independent warning channel. The lamp test during startup verifies LED functionality before the system enters the RUN state.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR045 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies 4 fault LEDs (GIO_A1-A4) with three states (steady ON, blinking 500 ms, OFF) and startup lamp test (500 ms each). ASIL B is appropriate for FSR-023/SG-001 through SG-008. The GPIO-driven LEDs provide a CAN-independent warning channel, which is critical since CAN failure is itself a fault condition. The lamp test verifies LED functionality before RUN. Traces down to SSR-SC-013 and HSR-SC-003 are consistent. The three LED states (ON/blink/OFF) provide clear visual differentiation. Per-ECU fault LEDs (A1=CVC, A2=FZC, A3=RZC, A4=system) enable rapid identification of the failing component during diagnostics.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR045 -->

---

### 4.14 Safe State Timing Domain (SG-001 through SG-008)

---

#### TSR-046: FTTI Compliance Verification per Safety Goal

- **ASIL**: D
- **Traces up**: FSR-024 (SG-001 through SG-008)
- **Traces down**: SSR-CVC-023, SSR-FZC-023, SSR-RZC-017, SSR-SC-014
- **Allocated to**: All ECUs
- **Verification method**: Analysis (WCET analysis) + Test (timing measurement)
- **Status**: draft

Each ECU shall ensure that the total time from fault occurrence to safe state achievement does not exceed the FTTI for the applicable safety goal. The following timing budget shall be enforced:

| Safety Goal | FTTI | Detection Budget | Reaction Budget | Actuation Budget | Margin |
|-------------|------|-----------------|-----------------|------------------|--------|
| SG-001 | 50 ms | 20 ms (CVC pedal check) | 10 ms (CAN + RZC) | 5 ms (H-bridge disable) | 15 ms |
| SG-002 | 200 ms | 50 ms (RZC motor health) | 20 ms (CAN + coordination) | 100 ms (controlled ramp) | 30 ms |
| SG-003 | 100 ms | 50 ms (FZC steering check) | 10 ms (servo command) | 30 ms (servo travel) | 10 ms |
| SG-004 | 50 ms | 10 ms (FZC brake check) | 5 ms (CAN + CVC) | 15 ms (motor disable) | 20 ms |
| SG-005 | 200 ms | 20 ms (FZC brake plausibility) | 10 ms (brake release) | 100 ms (servo return) | 70 ms |
| SG-006 | 500 ms | 100 ms (RZC temp/current) | 10 ms (motor disable) | 1 ms (GPIO write) | 389 ms |
| SG-007 | 200 ms | 50 ms (FZC lidar check) | 10 ms (CAN + brake) | 100 ms (brake ramp) | 40 ms |
| SG-008 | 100 ms | 50 ms (SC heartbeat) | 5 ms (relay command) | 10 ms (relay dropout) | 35 ms |

WCET analysis shall be performed for all safety-critical runnables to confirm that detection and reaction times do not exceed the budgets specified above.

**Rationale**: Explicit timing budgets per safety goal enable independent verification of each timing component. The margin column ensures headroom for worst-case scheduling and hardware variability.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR046 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies FTTI compliance timing budgets per safety goal with detection, reaction, actuation, and margin columns. ASIL D is correct for FSR-024/SG-001 through SG-008. This is the critical timing requirement that ties all safety mechanisms together. The timing budgets are generally achievable, with SG-006 having the most margin (389 ms of 500 ms) and SG-003 having the least (10 ms of 100 ms). Traces down to all four ECU SSR sets are consistent. The SG-008 entry shows 50 ms detection + 5 ms reaction + 10 ms actuation = 65 ms with 35 ms margin within 100 ms FTTI. However, the SC heartbeat path (TSR-028) takes 205 ms total, which exceeds SG-008 FTTI -- this discrepancy should be resolved or explicitly justified in the safety case as noted in the TSR-028 review.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR046 -->

---

#### TSR-047: Diagnostic Cycle Time Configuration

- **ASIL**: D
- **Traces up**: FSR-024 (SG-001 through SG-008)
- **Traces down**: SSR-CVC-023, SSR-FZC-023, SSR-RZC-017
- **Allocated to**: CVC, FZC, RZC
- **Verification method**: Analysis (RTOS scheduling analysis)
- **Status**: draft

Each safety-critical diagnostic runnable shall be scheduled at a period that ensures fault detection within the allocated detection budget (TSR-046). The following diagnostic cycle times shall be configured:

| Runnable | ECU | Period | Detection Budget |
|----------|-----|--------|-----------------|
| Swc_PedalPlausibility | CVC | 10 ms | 20 ms (2 cycles) |
| Swc_SteeringMonitor | FZC | 10 ms | 50 ms (5 cycles) |
| Swc_BrakeMonitor | FZC | 10 ms | 10 ms (1 cycle) |
| Swc_LidarMonitor | FZC | 10 ms | 50 ms (5 cycles) |
| Swc_CurrentMonitor | RZC | 1 ms | 10 ms (10 cycles) |
| Swc_TempMonitor | RZC | 100 ms | 100 ms (1 cycle) |
| Swc_DirectionMonitor | RZC | 10 ms | 50 ms (5 cycles) |
| SC_HeartbeatMonitor | SC | 10 ms | 150 ms (timeout) |

The RTOS task priority for safety-critical runnables shall be higher than QM tasks to prevent priority inversion.

**Rationale**: Fixed diagnostic cycle times ensure deterministic fault detection. Higher RTOS priority for safety tasks ensures temporal freedom from interference.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR047 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies diagnostic runnable periods and detection budgets per safety-critical function, with RTOS priority ordering for temporal FFI. ASIL D is correct for FSR-024/SG-001 through SG-008. The cycle times are well-chosen to provide multiple detection opportunities within each detection budget (e.g., Swc_CurrentMonitor at 1 ms with 10 ms budget = 10 cycles). The RTOS priority rule ensures safety tasks are not preempted by QM tasks, providing temporal freedom from interference per ISO 26262 Part 6. Traces down to CVC/FZC/RZC SSR sets are consistent. Note that SC_HeartbeatMonitor at 10 ms period is listed but the SC uses bare-metal cooperative loop (not FreeRTOS) -- this should be documented as a fixed polling interval rather than an RTOS task.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR047 -->

---

### 4.15 Loss of Braking Domain (SG-004)

---

#### TSR-048: FZC Loss-of-Braking Detection and Motor Cutoff Request

- **ASIL**: D
- **Traces up**: FSR-025 (SG-004)
- **Traces down**: SSR-FZC-024
- **Allocated to**: FZC
- **Verification method**: Test (SIL + fault injection)
- **Status**: draft

When the FZC detects a brake servo fault (PWM feedback mismatch per TSR-015, or brake servo not drawing current when a non-zero brake force is commanded), the FZC shall immediately transmit a brake fault CAN message (TSR-016) and a motor cutoff request CAN message (CAN ID 0x211, E2E protected) to the CVC. The motor cutoff request shall be transmitted within 10 ms of brake fault detection. The CVC shall relay the motor cutoff to the RZC by setting the torque request to zero in the next CAN transmission cycle.

**Rationale**: Motor cutoff provides a secondary deceleration path through back-EMF and friction when the primary brake actuator is unavailable. The multi-ECU chain (FZC detect, CVC command, RZC execute) completes within 30 ms.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR048 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies loss-of-braking detection (PWM mismatch or servo current absence) with motor cutoff request via CAN (ID 0x211, E2E protected) within 10 ms. ASIL D is correct for FSR-025/SG-004. The multi-ECU chain (FZC detect -> CVC command -> RZC execute) completing within 30 ms is well within the SG-004 FTTI of 50 ms. Motor cutoff via back-EMF and friction provides a viable secondary deceleration path. Traces down to SSR-FZC-024 are consistent. The servo current check (no current when braking is commanded) is a valuable addition to PWM feedback -- it detects servo disconnection or mechanical failure. The E2E protection on the motor cutoff request prevents false cutoff from CAN corruption.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR048 -->

---

#### TSR-049: SC Backup Motor Cutoff on Loss of Braking

- **ASIL**: D
- **Traces up**: FSR-025 (SG-004)
- **Traces down**: SSR-SC-015
- **Allocated to**: SC
- **Verification method**: Test (SIL + hardware test)
- **Status**: draft

If the FZC-CVC-RZC motor cutoff chain (TSR-048) fails due to CAN communication loss, the SC shall provide backup motor cutoff via the kill relay. The SC shall detect the failure through: (a) observing that the RZC motor current remains non-zero for more than 100 ms after the FZC reported a brake fault, or (b) heartbeat timeout from the CVC or RZC. On either condition, the SC shall de-energize the kill relay (per TSR-030).

**Rationale**: The SC kill relay provides a hardware-enforced backup path for motor cutoff that does not depend on CAN communication between zone ECUs. This four-layer response (FZC, CVC, RZC, SC) ensures motor cutoff even under multiple simultaneous failures.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR049 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies SC backup motor cutoff via kill relay when the FZC-CVC-RZC chain fails, detected by non-zero motor current 100 ms after brake fault report or heartbeat timeout. ASIL D is correct for FSR-025/SG-004. The four-layer defense (FZC, CVC, RZC, SC) provides exceptional redundancy for motor cutoff. The SC's ability to detect chain failure by monitoring motor current (non-zero 100 ms after brake fault) is a well-designed cross-check. Traces down to SSR-SC-015 are consistent. The kill relay provides CAN-independent motor cutoff, which is critical since CAN failure itself could be the reason the FZC-CVC-RZC chain fails. This requirement demonstrates the value of the independent safety controller architecture.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR049 -->

---

### 4.16 SC Self-Test Domain (SG-008)

---

#### TSR-050: SC Startup Self-Test Sequence

- **ASIL**: C
- **Traces up**: FSR-016, FSR-017 (SG-008)
- **Traces down**: SSR-SC-016
- **Allocated to**: SC
- **Verification method**: Test (PIL + hardware test)
- **Status**: draft

The SC shall perform the following self-test sequence at every power-on before energizing the kill relay:

1. CPU lockstep diagnostic: Trigger a built-in lockstep self-test via the ESM module and verify the ESM correctly reports no fault.
2. RAM BIST: Execute the TMS570 built-in RAM self-test (PBIST) covering the full RAM region.
3. Flash CRC: Compute a CRC-32 over the application flash region and compare against the stored reference CRC.
4. CAN controller test: Initialize DCAN1, perform a self-test transmission (internal loopback), and verify successful reception.
5. GPIO readback: Drive each output pin (GIO_A0 through GIO_A4) to a known state and read back via the data-in register.
6. Fault LED lamp test: Illuminate all 4 fault LEDs for 500 ms.
7. Watchdog test: Verify the TPS3823 WDI pin can be toggled and the RESET pin is de-asserted.

If any self-test step fails, the SC shall not energize the kill relay (system remains in safe state) and shall illuminate the system fault LED (GIO_A4) with a specific blink pattern indicating which test failed.

**Rationale**: Comprehensive startup self-test verifies the integrity of the SC before it assumes safety monitoring responsibility. Failing to energize the kill relay on self-test failure ensures the system cannot operate with a faulty safety monitor.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR050 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies a 7-step SC startup self-test sequence: lockstep diagnostic, RAM BIST (PBIST), flash CRC-32, CAN loopback, GPIO readback, fault LED lamp test, and watchdog pin test. ASIL C is appropriate for FSR-016/FSR-017/SG-008. The sequence is comprehensive and tests all safety-critical hardware components before the SC assumes monitoring responsibility. The kill relay not being energized until all tests pass is the correct fail-safe default. Traces down to SSR-SC-016 are consistent. The blink pattern for failed test step identification is valuable for field diagnostics. The TMS570 PBIST and ESM lockstep test are hardware-supported features, adding confidence. This self-test sequence is well-ordered -- testing the CPU and memory before testing peripherals that depend on them.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR050 -->

---

#### TSR-051: SC Runtime Periodic Self-Test

- **ASIL**: C
- **Traces up**: FSR-017 (SG-008)
- **Traces down**: SSR-SC-017
- **Allocated to**: SC
- **Verification method**: Test (SIL)
- **Status**: draft

The SC shall perform a subset of self-test checks periodically during runtime at a 60-second interval:

1. Flash CRC verification (same as startup).
2. RAM integrity check (32-byte test pattern write/read/compare in a reserved test region).
3. CAN controller status check (verify not in bus-off, error counters within warning threshold).
4. GPIO readback of GIO_A0 (verify kill relay GPIO is in the expected state).

If any runtime self-test fails, the SC shall de-energize the kill relay (TSR-030, condition 4) and illuminate the system fault LED.

**Rationale**: Runtime self-tests detect latent faults that develop after startup (e.g., flash bit rot, RAM degradation, CAN transceiver fault). The 60-second interval balances fault detection latency against CPU overhead.

<!-- HITL-LOCK START:COMMENT-BLOCK-TSR051 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement specifies runtime periodic self-test (60-second interval) covering flash CRC, RAM test pattern, CAN controller status, and GPIO readback. ASIL C is appropriate for FSR-017/SG-008. The runtime self-test detects latent faults (flash bit rot, RAM degradation) that would not be caught by the startup test alone. The 60-second interval is reasonable for latent fault detection -- ISO 26262 LFM calculation considers the test interval relative to the assumed vehicle operating time. Traces down to SSR-SC-017 are consistent. The subset of tests (4 of the 7 startup tests) is appropriate for runtime -- the excluded tests (lockstep diagnostic, PBIST, lamp test) are destructive or disruptive and should only run at startup. Kill relay de-energize on failure (TSR-030 condition 4) is the correct response.
<!-- HITL-LOCK END:COMMENT-BLOCK-TSR051 -->

---

#### TSR-052: SC Standstill Motor Current Cross-Plausibility

- **ASIL**: D
- **Traces up**: FSR-026 (SG-001)
- **Traces down**: SSR-SC-018
- **Allocated to**: SC
- **Verification method**: Test (SIL, HIL)
- **Status**: draft

The SC shall compare the CVC torque command (Vehicle_State CAN 0x100, byte 4 TorquePct) against the RZC motor current measurement (Motor_Current CAN 0x301, bytes 2-3 MotorCurrent_mA). If torque command equals zero AND motor current exceeds 500 mA for 2 or more consecutive SC main cycles (20 ms), the SC shall de-energize the kill relay within 5 ms, transitioning the system to SAFE_STOP.

This cross-plausibility check detects hardware faults in the motor driver (e.g., BTS7960 FET gate-source short) that cause unintended motor current despite zero torque command from the control layer.

**Rationale**: A motor driver FET short-circuit bypasses all software-layer torque controls. Only the SC, operating independently on a separate MCU with direct relay control, can detect and react to this fault class. The 500 mA threshold discriminates genuine faults from measurement noise; the 2-cycle debounce prevents false trips from transient current spikes.

---

## 5. Requirements Traceability Summary

### 5.1 FSR to TSR Mapping

| FSR | ASIL | TSRs Derived |
|-----|------|-------------|
| FSR-001 | D | TSR-001, TSR-002 |
| FSR-002 | D | TSR-002, TSR-003 |
| FSR-003 | D | TSR-004, TSR-005 |
| FSR-004 | A | TSR-006, TSR-007 |
| FSR-005 | A | TSR-008, TSR-009 |
| FSR-006 | D | TSR-010, TSR-011 |
| FSR-007 | D | TSR-012, TSR-013 |
| FSR-008 | C | TSR-014 |
| FSR-009 | D | TSR-015, TSR-016 |
| FSR-010 | D | TSR-017 |
| FSR-011 | C | TSR-018, TSR-019 |
| FSR-012 | C | TSR-020, TSR-021 |
| FSR-013 | D | TSR-022, TSR-023, TSR-024 |
| FSR-014 | C | TSR-025, TSR-026 |
| FSR-015 | C | TSR-027, TSR-028 |
| FSR-016 | D | TSR-029, TSR-030 |
| FSR-017 | D | TSR-031, TSR-032 |
| FSR-018 | B | TSR-033, TSR-034 |
| FSR-019 | D | TSR-035, TSR-036, TSR-037 |
| FSR-020 | C | TSR-038, TSR-039 |
| FSR-021 | C | TSR-040 |
| FSR-022 | C | TSR-041, TSR-042 |
| FSR-023 | B | TSR-043, TSR-044, TSR-045 |
| FSR-024 | D | TSR-046, TSR-047 |
| FSR-025 | D | TSR-048, TSR-049 |
| FSR-026 | D | TSR-052 |

### 5.2 TSR to ECU Allocation Summary

| ECU | TSRs Allocated |
|-----|---------------|
| CVC (STM32G474RE) | TSR-001, TSR-002, TSR-003, TSR-004, TSR-022, TSR-023, TSR-024, TSR-025, TSR-026, TSR-031, TSR-033, TSR-034, TSR-035, TSR-036, TSR-037, TSR-038, TSR-039, TSR-043, TSR-046, TSR-047 |
| FZC (STM32G474RE) | TSR-010, TSR-011, TSR-012, TSR-013, TSR-014, TSR-015, TSR-016, TSR-017, TSR-018, TSR-019, TSR-020, TSR-021, TSR-022, TSR-023, TSR-024, TSR-025, TSR-026, TSR-031, TSR-038, TSR-044, TSR-046, TSR-047, TSR-048 |
| RZC (STM32G474RE) | TSR-005, TSR-006, TSR-007, TSR-008, TSR-009, TSR-022, TSR-023, TSR-024, TSR-025, TSR-026, TSR-031, TSR-038, TSR-040, TSR-046, TSR-047 |
| SC (TMS570LC43x) | TSR-022, TSR-024, TSR-027, TSR-028, TSR-029, TSR-030, TSR-031, TSR-038, TSR-041, TSR-042, TSR-045, TSR-046, TSR-049, TSR-050, TSR-051, TSR-052 |

### 5.3 ASIL Distribution

| ASIL | Count | TSRs |
|------|-------|------|
| D | 27 | TSR-001, TSR-002, TSR-003, TSR-004, TSR-005, TSR-010, TSR-011, TSR-012, TSR-013, TSR-015, TSR-016, TSR-017, TSR-022, TSR-023, TSR-024, TSR-028, TSR-029, TSR-030, TSR-031, TSR-032, TSR-035, TSR-036, TSR-037, TSR-046, TSR-047, TSR-048, TSR-049, TSR-052 |
| C | 17 | TSR-007, TSR-014, TSR-018, TSR-019, TSR-020, TSR-021, TSR-025, TSR-026, TSR-027, TSR-038, TSR-039, TSR-040, TSR-041, TSR-042, TSR-045, TSR-050, TSR-051 |
| B | 4 | TSR-033, TSR-034, TSR-043, TSR-044 |
| A | 4 | TSR-006, TSR-008, TSR-009 |
| **Total** | **52** | |

## 6. Open Items and Assumptions

### 6.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| TSR-A-001 | SPI1 bus can reliably service both AS5048A sensors within the 10 ms control cycle on CVC | TSR-001 timing depends on SPI bus availability |
| TSR-A-002 | Steering servo mechanical bandwidth is sufficient for 30 deg/s rate-limited return-to-center within 200 ms | TSR-012 fallback depends on servo speed |
| TSR-A-003 | CAN bus utilization at 500 kbps with all messages at specified rates does not exceed 50% bus load | E2E timing and heartbeat reliability depends on acceptable bus load |
| TSR-A-004 | TMS570LC43x DCAN1 can operate in listen-only mode without affecting bus arbitration | SC independence depends on silent mode operation |
| TSR-A-005 | ACS723 sensitivity (100 mV/A) is stable across the operating temperature range | TSR-006 current measurement accuracy depends on sensor stability |

### 6.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| TSR-O-001 | Perform CAN bus utilization analysis with all message rates specified | System Architect | Before SWE.1 |
| TSR-O-002 | Validate SPI timing for dual AS5048A on shared bus (CVC) | Integration Engineer | Hardware integration |
| TSR-O-003 | Characterize ACS723 accuracy across temperature range | Hardware Engineer | Hardware integration |
| TSR-O-004 | WCET analysis for all safety-critical runnables | SW Engineer | SWE.3 phase |
| TSR-O-005 | Verify TFMini-S UART frame format and checksum against actual sensor | Integration Engineer | Hardware integration |
| TSR-O-006 | Confirm TPS3823 timeout with actual capacitor value | Hardware Engineer | Hardware integration |

## 7. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete TSR specification: 51 requirements (TSR-001 to TSR-051), full traceability, ECU allocation |
| 1.1 | 2026-03-10 | An Dao | Added TSR-052 (SC Standstill Motor Current Cross-Plausibility) to close FSR-026/SSR-SC-018 traceability gap |

