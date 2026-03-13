---
document_id: SWR-FZC
title: "Software Requirements — FZC"
version: "1.0"
status: draft
aspice_process: SWE.1
ecu: FZC
asil: D
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

Every requirement (SWR-FZC-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/software/lessons-learned/`](../lessons-learned/). One file per requirement (SWR-FZC-NNN). File naming: `SWR-FZC-NNN-<short-title>.md`.


# Software Requirements — Front Zone Controller (FZC)

## 1. Purpose

This document specifies the complete software requirements for the Front Zone Controller (FZC), the STM32G474RE-based front zone ECU of the Taktflow Zonal Vehicle Platform. These requirements are derived from system requirements (SYS), technical safety requirements (TSR), and software safety requirements (SSR) per Automotive SPICE 4.0 SWE.1 (Software Requirements Analysis).

The FZC is responsible for steering servo control, brake servo control, TFMini-S lidar obstacle detection, and buzzer warning generation. Software requirements cover ALL FZC functionality: safety-critical steering/braking and lidar (from SSRs), plus functional, diagnostic, configuration, startup/shutdown, and buzzer requirements.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SYS | System Requirements Specification | 1.0 |
| TSR | Technical Safety Requirements | 1.0 |
| SSR | Software Safety Requirements | 1.0 |
| HSI | Hardware-Software Interface Specification | 0.1 |
| SYSARCH | System Architecture | 1.0 |
| CAN-MATRIX | CAN Message Matrix | 0.1 |

## 3. Requirement Conventions

Same conventions as SWR-CVC document section 3.

---

## 4. Steering Servo Requirements

### SWR-FZC-001: Steering Angle Sensor SPI Read

- **ASIL**: D
- **Traces up**: SYS-011, SYS-047, TSR-010, SSR-FZC-001
- **Traces down**: firmware/fzc/src/swc/swc_steering.c:Swc_Steering_ReadSensor()
- **Verified by**: TC-FZC-001, TC-FZC-002
- **Verification method**: Unit test + PIL
- **Status**: draft

The FZC software shall read the steering AS5048A angle sensor via SPI1 every 10 ms. The read sequence shall: (a) assert chip-select LOW (PB12), (b) transmit 16-bit read command for angle register (0x3FFF), (c) receive 16-bit response, (d) de-assert chip-select HIGH, (e) validate parity and extract 14-bit angle, (f) convert to degrees using the calibration offset (center position = 0 degrees, +/- 45 degrees mechanical range mapped to 14-bit range). If SPI fails within 500 us, retry once. If retry fails, set sensor status to COMM_FAULT and use last valid value.

---

### SWR-FZC-002: Steering Command-vs-Feedback Comparison

- **ASIL**: D
- **Traces up**: SYS-011, TSR-011, SSR-FZC-002
- **Traces down**: firmware/fzc/src/swc/swc_steering.c:Swc_Steering_CheckFeedback()
- **Verified by**: TC-FZC-003, TC-FZC-004, TC-FZC-005
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The FZC software shall compare the commanded steering angle (received from CVC via CAN ID 0x201) with the measured steering angle (from AS5048A) every 10 ms. The software shall maintain a debounce counter that increments when |commanded - measured| exceeds 5 degrees and resets when within threshold. When the counter reaches 5 (50 ms), the software shall declare a steering position fault and initiate return-to-center (SWR-FZC-004).

---

### SWR-FZC-003: Steering Sensor Range and Rate Check

- **ASIL**: D
- **Traces up**: SYS-011, TSR-011, SSR-FZC-003
- **Traces down**: firmware/fzc/src/swc/swc_steering.c:Swc_Steering_DiagCheck()
- **Verified by**: TC-FZC-006, TC-FZC-007
- **Verification method**: Unit test + fault injection
- **Status**: draft

The FZC software shall check every steering angle reading for: (a) mechanical range violation (outside -45 to +45 degrees for 2 consecutive cycles = range fault), (b) slew rate violation (change exceeding 3.6 degrees per 10 ms cycle, equivalent to 360 degrees/second = rate fault), (c) AS5048A diagnostic register check every 100 ms for magnetic field or CORDIC errors. On any fault, invoke return-to-center (SWR-FZC-004) and log a DTC.

---

### SWR-FZC-004: Steering Return-to-Center Execution

- **ASIL**: D
- **Traces up**: SYS-012, TSR-012, SSR-FZC-004
- **Traces down**: firmware/fzc/src/swc/swc_steering.c:Swc_Steering_ReturnToCenter()
- **Verified by**: TC-FZC-008, TC-FZC-009, TC-FZC-010
- **Verification method**: Unit test + SIL + PIL
- **Status**: draft

Upon steering fault detection, the FZC software shall compute a rate-limited trajectory from the current angle to 0 degrees (center) at a maximum rate of 30 degrees per second. Every 10 ms, the software shall: (a) compute the next target angle (move 0.3 degrees toward center), (b) convert the target angle to servo PWM duty cycle (1.5 ms pulse = center at 50 Hz), (c) write the duty cycle to TIM1 channel 1 compare register. The software shall monitor the AS5048A feedback; if the measured angle does not reach within 2 degrees of center within 200 ms, invoke PWM disable (SWR-FZC-006).

---

### SWR-FZC-005: Steering Return-to-Center CAN Notification

- **ASIL**: D
- **Traces up**: SYS-012, TSR-012, SSR-FZC-005
- **Traces down**: firmware/fzc/src/swc/swc_steering.c:Swc_Steering_NotifyCVC()
- **Verified by**: TC-FZC-011
- **Verification method**: SIL + integration test
- **Status**: draft

Upon initiating return-to-center, the FZC software shall transmit a steering fault CAN message (CAN ID 0x211, E2E protected) to the CVC within 10 ms. The message shall contain: fault type (4-bit: position, range, rate, sensor), current measured angle (16-bit), target angle (16-bit, = 0 for center), alive counter (4-bit), CRC-8 (8-bit). The CVC shall use this to transition to DEGRADED and reduce speed.

---

### SWR-FZC-006: Steering Servo PWM Disable

- **ASIL**: D
- **Traces up**: SYS-012, TSR-013, SSR-FZC-006
- **Traces down**: firmware/fzc/src/swc/swc_steering.c:Swc_Steering_DisablePWM()
- **Verified by**: TC-FZC-012, TC-FZC-013
- **Verification method**: PIL + fault injection
- **Status**: draft

If return-to-center fails (200 ms timeout without reaching center), the FZC software shall disable the steering servo PWM by: (a) writing 0 to the TIM1 channel 1 compare register (0% duty), (b) clearing the TIM1 CCER output enable bit for channel 1, (c) reconfiguring the TIM1 channel 1 output pin as a GPIO output and driving it LOW. The software shall set an internal latch preventing PWM re-enable until system restart. This three-level disable provides defense-in-depth.

---

### SWR-FZC-007: Steering Rate Limiter

- **ASIL**: C
- **Traces up**: SYS-013, TSR-014, SSR-FZC-007
- **Traces down**: firmware/fzc/src/swc/swc_steering.c:Swc_Steering_RateLimit()
- **Verified by**: TC-FZC-014, TC-FZC-015
- **Verification method**: Unit test + SIL
- **Status**: draft

The FZC software shall apply rate limiting to the steering angle command before converting to PWM. Each 10 ms cycle, the maximum permitted change shall be 0.3 degrees (30 degrees/second * 0.01 s). If the delta between the new command and the current position exceeds 0.3 degrees, the software shall clamp the output to current_position +/- 0.3 degrees. The software shall also clamp the absolute angle to -43 to +43 degrees (2-degree margin from mechanical stops at +/-45 degrees).

---

### SWR-FZC-008: Steering Servo PWM Output

- **ASIL**: D
- **Traces up**: SYS-010, SYS-050
- **Traces down**: firmware/fzc/src/swc/swc_steering.c:Swc_Steering_WritePWM()
- **Verified by**: TC-FZC-066, TC-FZC-067
- **Verification method**: PIL + hardware test
- **Status**: draft

The FZC software shall command the steering servo via TIM1 channel 1 PWM at 50 Hz. The duty cycle shall map linearly from the angle range: -45 degrees = 1.0 ms pulse (5% duty at 50 Hz), 0 degrees = 1.5 ms pulse (7.5% duty), +45 degrees = 2.0 ms pulse (10% duty). The angle-to-pulse mapping shall use the formula: pulse_us = 1500 + (angle_deg * 500 / 45). The software shall verify the TIM1 configuration (prescaler, ARR, output mode) at startup and reject any mismatched configuration.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-001-008 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Steering servo requirements SWR-FZC-001 through SWR-FZC-008 comprehensively cover CAN reception, rate limiting (30 deg/s), saturation (+/- 45 deg), return-to-center on fault, angle feedback via SPI, sensor fault detection, angle feedback CAN TX, and PWM output formula. All safety-critical requirements (001-006, 008) are correctly ASIL D. The rate limiting formula (max 3 degrees per 10 ms at 30 deg/s) is precisely specified with a clear rationale for preventing sudden steering input. The return-to-center behavior on sensor fault (SWR-FZC-004) is a correct fail-safe action. The PWM mapping formula (pulse_us = 1500 + angle_deg * 500/45) is concrete and testable. One concern: SWR-FZC-008 references TIM1 but sw-architecture.md shows TIM2 for FZC servos -- this should be verified for consistency.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-001-008 -->

---

## 5. Brake Servo Requirements

### SWR-FZC-009: Brake Servo PWM Output and Feedback

- **ASIL**: D
- **Traces up**: SYS-014, SYS-050, TSR-015, SSR-FZC-008
- **Traces down**: firmware/fzc/src/swc/swc_brake.c:Swc_Brake_WritePWM()
- **Verified by**: TC-FZC-016, TC-FZC-017
- **Verification method**: PIL + hardware test
- **Status**: draft

The FZC software shall command the brake servo via TIM2 channel 1 PWM at 50 Hz. The duty cycle shall map linearly: 0% braking = 1.0 ms pulse (5% duty at 50 Hz), 100% braking = 2.0 ms pulse (10% duty at 50 Hz). The software shall read back the TIM2 input capture register to measure the actual output pulse width. If |commanded - measured| duty cycle exceeds 2% for 3 consecutive cycles (30 ms), the software shall declare a brake PWM fault.

---

### SWR-FZC-010: Brake Fault CAN Notification

- **ASIL**: D
- **Traces up**: SYS-015, TSR-016, SSR-FZC-009
- **Traces down**: firmware/fzc/src/swc/swc_brake.c:Swc_Brake_NotifyFault()
- **Verified by**: TC-FZC-018
- **Verification method**: SIL + integration test
- **Status**: draft

Upon brake system fault detection (PWM mismatch or servo non-response), the FZC software shall transmit a brake fault CAN message (CAN ID 0x210, E2E protected) within 10 ms. The message shall contain: fault type (2-bit), commanded brake value (8-bit, percent), measured feedback (8-bit, percent), alive counter (4-bit), CRC-8 (8-bit). The CVC shall react by commanding motor cutoff to the RZC.

---

### SWR-FZC-011: Auto-Brake on CAN Command Timeout

- **ASIL**: D
- **Traces up**: SYS-016, TSR-017, SSR-FZC-010
- **Traces down**: firmware/fzc/src/swc/swc_brake.c:Swc_Brake_AutoBrake()
- **Verified by**: TC-FZC-019, TC-FZC-020, TC-FZC-021
- **Verification method**: SIL + fault injection
- **Status**: draft

The FZC software shall maintain a timestamp of the last valid brake command CAN message reception (CAN ID 0x200, passing E2E check). If the elapsed time since the last valid message exceeds 100 ms, the software shall command maximum braking (100%, 2.0 ms pulse on TIM2). The auto-brake state shall be latching: release requires 5 consecutive valid CAN brake commands from the CVC with sequential E2E alive counters, confirming stable communication recovery.

---

### SWR-FZC-012: Loss-of-Braking Motor Cutoff Request

- **ASIL**: D
- **Traces up**: SYS-015, TSR-048, SSR-FZC-024
- **Traces down**: firmware/fzc/src/swc/swc_brake.c:Swc_Brake_RequestMotorCutoff()
- **Verified by**: TC-FZC-045, TC-FZC-046
- **Verification method**: SIL + fault injection
- **Status**: draft

When the FZC detects a brake servo fault (SWR-FZC-009 PWM mismatch), the software shall transmit both a brake fault CAN message (SWR-FZC-010) and a motor cutoff request CAN message (CAN ID 0x211, E2E protected) within 10 ms. The motor cutoff request shall contain: request type (motor_cutoff = 0x01), reason (brake_fault = 0x02), alive counter (4-bit), CRC-8 (8-bit). The FZC shall continue transmitting the motor cutoff request every 10 ms until the CVC acknowledges receipt or CAN timeout occurs.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-009-012 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Brake servo requirements SWR-FZC-009 through SWR-FZC-012 cover PWM output with feedback, fault CAN notification, auto-brake on CAN timeout, and loss-of-braking motor cutoff request. All four requirements are correctly ASIL D given the braking safety criticality. The PWM feedback via TIM2 input capture readback (SWR-FZC-009) is a solid diagnostic mechanism with a concrete 2% / 30 ms debounce threshold. Auto-brake on 100 ms CAN timeout (SWR-FZC-011) with a latching release requiring 5 consecutive valid messages is appropriately conservative. SWR-FZC-012 adds defense-in-depth by requesting motor cutoff from CVC when braking fails. Traces to SYS-014/015/016 and TSR-015/016/017 are consistent with the braking safety chain. Minor note: SWR-FZC-010 is marked QM in the ASIL distribution table (Section 18.2) but its traces include TSR-016 and SSR-FZC-009 -- this should be reviewed for ASIL consistency.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-009-012 -->

---

## 6. Lidar Requirements

### SWR-FZC-013: Lidar UART Frame Reception

- **ASIL**: C
- **Traces up**: SYS-018, SYS-048, TSR-018, SSR-FZC-011
- **Traces down**: firmware/fzc/src/swc/swc_lidar.c:Swc_Lidar_ReceiveFrame()
- **Verified by**: TC-FZC-022, TC-FZC-023
- **Verification method**: Unit test + PIL
- **Status**: draft

The FZC software shall receive TFMini-S data via UART2 configured at 115200 baud, 8N1. The software shall parse the 9-byte frame by: (a) scanning for the header bytes 0x59, 0x59, (b) reading the remaining 7 bytes, (c) computing the checksum (low byte of sum of bytes 0-7) and comparing with byte 8. Valid frames shall extract: distance (bytes 2-3, little-endian, centimeters) and signal strength (bytes 4-5, little-endian). Invalid frames shall be discarded. The software shall use DMA for UART reception to avoid CPU polling overhead. The software shall maintain a reception timestamp; if no valid frame is received for 100 ms, a sensor timeout shall be declared.

---

### SWR-FZC-014: Lidar Graduated Response Logic

- **ASIL**: C
- **Traces up**: SYS-017, SYS-019, TSR-019, SSR-FZC-012
- **Traces down**: firmware/fzc/src/swc/swc_lidar.c:Swc_Lidar_EvaluateDistance()
- **Verified by**: TC-FZC-024, TC-FZC-025, TC-FZC-026
- **Verification method**: Unit test + SIL
- **Status**: draft

The FZC software shall evaluate the lidar distance against three compile-time configurable thresholds (default: warning = 100 cm, braking = 50 cm, emergency = 20 cm). The software shall enforce the invariant emergency < braking < warning at compile time via static assertions. The response shall be:

| Zone | Threshold | Brake Command | CAN Action | Buzzer |
|------|-----------|---------------|------------|--------|
| Emergency | <= 20 cm | 100% | Motor cutoff request (0x211) | Continuous |
| Braking | <= 50 cm | 50% | Speed reduction (0x220) | Fast beep |
| Warning | <= 100 cm | None | Warning CAN (0x220) | Single beep |
| Clear | > 100 cm | Per CVC command | None | Silent |

The software shall always act on the most critical (smallest distance) zone.

---

### SWR-FZC-015: Lidar Range and Stuck Sensor Detection

- **ASIL**: C
- **Traces up**: SYS-020, TSR-020, SSR-FZC-013
- **Traces down**: firmware/fzc/src/swc/swc_lidar.c:Swc_Lidar_PlausibilityCheck()
- **Verified by**: TC-FZC-027, TC-FZC-028, TC-FZC-029
- **Verification method**: Unit test + fault injection
- **Status**: draft

The FZC software shall check each valid lidar frame for: (a) range: distance below 2 cm or above 1200 cm flagged as out-of-range and replaced with 0 cm safe default, (b) signal strength: values below 100 flagged as unreliable and replaced with 0 cm, (c) stuck detection: maintain a counter that increments when |current - previous| < 1 cm, resets otherwise. When counter reaches 50 (approximately 500 ms), flag as stuck sensor and replace with 0 cm.

---

### SWR-FZC-016: Lidar Fault Safe Default Substitution

- **ASIL**: C
- **Traces up**: SYS-020, TSR-021, SSR-FZC-014
- **Traces down**: firmware/fzc/src/swc/swc_lidar.c:Swc_Lidar_FaultHandler()
- **Verified by**: TC-FZC-030, TC-FZC-031
- **Verification method**: Unit test + SIL
- **Status**: draft

On any lidar fault (timeout, range, stuck, signal strength), the FZC software shall: (a) substitute distance = 0 cm (triggers emergency zone per SWR-FZC-014), (b) log a DTC via Dem with fault type and last valid reading, (c) if fault persists for 200 consecutive cycles (2 seconds), transmit a degradation request to CVC via CAN. The safe default of 0 cm is fail-closed: sensor failure causes the system to assume the worst case (obstacle at minimum distance).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-013-016 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Lidar requirements SWR-FZC-013 through SWR-FZC-016 cover UART frame reception, graduated response logic, plausibility checks, and fault safe-default substitution. All four are correctly ASIL C, consistent with lidar being a supplemental safety function (not primary braking/steering). The DMA-based UART reception (SWR-FZC-013) is appropriate for avoiding CPU polling overhead. The graduated response table (SWR-FZC-014) with compile-time static assertions enforcing emergency < braking < warning is a well-designed invariant check. Stuck sensor detection (SWR-FZC-015) with a 50-count / 500 ms threshold is reasonable. The fail-closed safe default of 0 cm (SWR-FZC-016) correctly triggers the worst-case emergency zone, which is the right design for an obstacle detection sensor failure. Traces to SYS-017/018/019/020 and TSR-018/019/020/021 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-013-016 -->

---

## 7. Buzzer Requirements

### SWR-FZC-017: Buzzer Pattern Generation

- **ASIL**: QM
- **Traces up**: SYS-045, TSR-044, SSR-FZC-022
- **Traces down**: firmware/fzc/src/swc/swc_buzzer.c:Swc_Buzzer_SetPattern()
- **Verified by**: TC-FZC-041, TC-FZC-042
- **Verification method**: PIL + demonstration
- **Status**: draft

The FZC software shall drive the piezo buzzer GPIO with the following patterns:

| Pattern | On Time | Off Time | Trigger |
|---------|---------|----------|---------|
| Single beep | 100 ms | One-shot | DEGRADED transition, lidar warning zone |
| Slow repeat | 500 ms | 500 ms | LIMP state |
| Fast repeat | 100 ms | 100 ms | SAFE_STOP state, lidar braking zone |
| Continuous | Always on | — | E-stop, lidar emergency zone |
| Silent | Off | — | RUN normal, INIT |

The buzzer pattern shall be driven by a software timer running at 10 ms resolution. The buzzer shall be activated within 100 ms of state change or local lidar detection. The buzzer pattern priority (highest to lowest): continuous > fast repeat > slow repeat > single beep > silent.

---

### SWR-FZC-018: Buzzer Pattern from CAN State

- **ASIL**: QM
- **Traces up**: SYS-045
- **Traces down**: firmware/fzc/src/swc/swc_buzzer.c:Swc_Buzzer_UpdateFromState()
- **Verified by**: TC-FZC-068
- **Verification method**: SIL + demonstration
- **Status**: draft

The FZC software shall update the buzzer pattern based on the vehicle state received from the CVC state broadcast (CAN ID 0x100). When the vehicle state field changes, the buzzer shall switch to the corresponding pattern per SWR-FZC-017. Local lidar detection shall override the CAN-based pattern if the local pattern has higher priority (e.g., lidar emergency overrides CAN DEGRADED).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-017-018 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Buzzer requirements SWR-FZC-017 and SWR-FZC-018 are correctly classified as QM since the buzzer is an auditory warning with no safety-critical function. The pattern table in SWR-FZC-017 is clearly specified with on/off timing and trigger conditions. The 100 ms activation latency requirement is reasonable. The priority system (continuous > fast > slow > single > silent) and the local lidar override logic in SWR-FZC-018 ensure the most critical warning is always presented. These requirements are well-structured and testable. SWR-FZC-017 traces to SYS-045 / TSR-044 / SSR-FZC-022 -- the SSR trace for a QM requirement is unusual and should be verified (SSR requirements typically carry ASIL ratings).
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-017-018 -->

---

## 8. E2E Protection Requirements

### SWR-FZC-019: FZC CAN E2E Transmit

- **ASIL**: D
- **Traces up**: SYS-032, TSR-022, TSR-023, SSR-FZC-015, SSR-FZC-016
- **Traces down**: firmware/fzc/src/bsw/e2e/e2e_protect.c:E2E_Protect()
- **Verified by**: TC-FZC-032, TC-FZC-033
- **Verification method**: Unit test + SIL
- **Status**: draft

The FZC software shall implement the E2E protection transmit function identically to SWR-CVC-014, using the same CRC-8 polynomial (0x1D, init 0xFF), alive counter management, and Data ID assignment. Each FZC-originated CAN message shall use a unique Data ID distinct from CVC and RZC Data IDs. Per-Data-ID alive counters (16 entries) shall be initialized to 0 at startup and incremented exactly once per transmission.

---

### SWR-FZC-020: FZC CAN E2E Receive

- **ASIL**: D
- **Traces up**: SYS-032, TSR-024, SSR-FZC-017
- **Traces down**: firmware/fzc/src/bsw/e2e/e2e_check.c:E2E_Check()
- **Verified by**: TC-FZC-034, TC-FZC-035
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The FZC software shall implement the E2E protection receive function identically to SWR-CVC-015, using the same CRC-8 polynomial, alive counter verification, and Data ID check. On 3 consecutive E2E failures for brake commands, the safe default shall be maximum braking (100%). On 3 consecutive failures for steering commands, the safe default shall be center position (0 degrees).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-019-020 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** E2E protection requirements SWR-FZC-019 and SWR-FZC-020 are correctly ASIL D as they protect safety-critical steering and brake command communication. The specification correctly references the same CRC-8 polynomial (0x1D, init 0xFF) and Data ID scheme as SWR-CVC-014/015, ensuring protocol consistency across ECUs. The safe defaults on E2E failure are appropriate: maximum braking (100%) for brake command loss and center position (0 degrees) for steering command loss -- both are correct fail-safe actions. The 3-consecutive-failure threshold before safe default activation provides noise tolerance while maintaining safety. Traces to SYS-032, TSR-022/023/024, and SSR-FZC-015/016/017 are consistent with the E2E safety chain.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-019-020 -->

---

## 9. Heartbeat Requirements

### SWR-FZC-021: FZC Heartbeat Transmission

- **ASIL**: C
- **Traces up**: SYS-021, TSR-025, SSR-FZC-018
- **Traces down**: firmware/fzc/src/swc/swc_heartbeat.c:Swc_Heartbeat_Transmit()
- **Verified by**: TC-FZC-036
- **Verification method**: SIL + integration test
- **Status**: draft

The FZC software shall transmit a heartbeat CAN message on CAN ID 0x011 every 50 ms (tolerance +/- 5 ms). Payload: ECU ID (0x02, 8-bit), operating mode (4-bit), fault status bitmask (8-bit: bit 0 = steering fault, bit 1 = brake fault, bit 2 = lidar fault, bit 3 = CAN fault, bits 4-7 reserved), alive counter (4-bit), CRC-8 (8-bit). Total: 4 bytes.

---

### SWR-FZC-022: FZC Heartbeat Conditioning

- **ASIL**: C
- **Traces up**: SYS-021, TSR-026, SSR-FZC-019
- **Traces down**: firmware/fzc/src/swc/swc_heartbeat.c:Swc_Heartbeat_CheckConditions()
- **Verified by**: TC-FZC-037
- **Verification method**: Unit test + fault injection
- **Status**: draft

The FZC software shall suppress heartbeat transmission under the same conditions as SWR-CVC-021: main loop not iterating, stack canary corrupted, or CAN bus-off state. This ensures the SC detects a hung or corrupted FZC.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-021-022 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Heartbeat requirements SWR-FZC-021 and SWR-FZC-022 are correctly ASIL C, consistent with heartbeat monitoring being part of the SC-based safety monitoring chain. The 50 ms period with +/- 5 ms tolerance (SWR-FZC-021) matches the SC heartbeat timeout of 150 ms (3 missed heartbeats). The fault bitmask payload with 4 specific fault bits (steering, brake, lidar, CAN) provides good diagnostic visibility to the SC. The heartbeat conditioning logic in SWR-FZC-022 correctly mirrors SWR-CVC-021 to ensure a hung or corrupted FZC stops broadcasting, which allows the SC to detect the failure via heartbeat timeout. Traces to SYS-021, TSR-025/026, and SSR-FZC-018/019 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-021-022 -->

---

## 10. Watchdog Requirements

### SWR-FZC-023: FZC External Watchdog Feed

- **ASIL**: D
- **Traces up**: SYS-027, TSR-031, SSR-FZC-020
- **Traces down**: firmware/fzc/src/swc/swc_watchdog.c:Swc_Watchdog_Feed()
- **Verified by**: TC-FZC-038
- **Verification method**: Unit test + fault injection + PIL
- **Status**: draft

The FZC software shall toggle the TPS3823 WDI pin under the same four-condition check as SWR-CVC-023: main loop complete, stack canary intact, RAM test passed, CAN not bus-off. The toggle shall alternate HIGH/LOW each iteration.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-023 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Watchdog requirement SWR-FZC-023 is correctly ASIL D, consistent with the external watchdog being a last-resort safety mechanism. The four-condition check (main loop, stack canary, RAM test, CAN status) before toggling the TPS3823 WDI mirrors the CVC approach in SWR-CVC-023, ensuring design consistency across zone ECUs. The alternating HIGH/LOW toggle pattern ensures the watchdog detects a stuck GPIO as well as a stuck main loop. Traces to SYS-027, TSR-031, and SSR-FZC-020 are consistent with the watchdog safety chain.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-023 -->

---

## 11. CAN Recovery Requirements

### SWR-FZC-024: FZC CAN Bus Loss Detection

- **ASIL**: C
- **Traces up**: SYS-034, TSR-038, SSR-FZC-021
- **Traces down**: firmware/fzc/src/swc/swc_can_monitor.c:Swc_CanMonitor_Check()
- **Verified by**: TC-FZC-039, TC-FZC-040
- **Verification method**: SIL + fault injection
- **Status**: draft

The FZC software shall detect CAN bus loss using the same criteria as SWR-CVC-024 (bus-off, 200 ms silence, error warning sustained 500 ms). On CAN bus loss, the FZC shall: (a) apply auto-brake (100% per SWR-FZC-011), (b) command steering to center (per SWR-FZC-004), (c) activate continuous buzzer. The FZC shall not attempt CAN recovery -- it shall maintain the safe state until power cycle.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-024 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** CAN recovery requirement SWR-FZC-024 is correctly ASIL C. The three-pronged safe reaction on CAN bus loss (auto-brake + steering center + continuous buzzer) is comprehensive and appropriate for the FZC's role as the front zone actuator controller. The decision not to attempt CAN recovery (maintain safe state until power cycle) is conservative and correct for a safety-critical zone controller -- recovery attempts could introduce unpredictable behavior. The use of identical detection criteria as SWR-CVC-024 ensures consistent bus loss detection across ECUs. Traces to SYS-034, TSR-038, and SSR-FZC-021 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-024 -->

---

## 12. Startup Self-Test Requirements

### SWR-FZC-025: FZC Startup Self-Test Sequence

- **ASIL**: D
- **Traces up**: SYS-027, SYS-029
- **Traces down**: firmware/fzc/src/swc/swc_selftest.c:Swc_SelfTest_Startup()
- **Verified by**: TC-FZC-047, TC-FZC-048, TC-FZC-049
- **Verification method**: Unit test + PIL + fault injection
- **Status**: draft

The FZC software shall execute the following self-test sequence at power-on before entering operational mode:

1. **Servo neutral calibration**: Command steering servo to center position (1.5 ms pulse) and brake servo to release position (1.0 ms pulse). Verify TIM1 and TIM2 are generating expected PWM output via input capture readback. Failure: disable servo outputs, remain in INIT.
2. **SPI sensor check**: Read the AS5048A NOP register from the steering sensor via SPI1. The sensor shall respond with a valid parity bit within 500 us. Failure: remain in INIT with steering sensor fault DTC.
3. **UART lidar handshake**: Receive at least 3 consecutive valid TFMini-S frames within 500 ms of UART2 initialization. Failure: log lidar DTC, continue with lidar in fault mode (safe default 0 cm active).
4. **CAN loopback test**: Configure FDCAN1 in loopback mode, transmit test frame, verify reception. Failure: remain in INIT with CAN fault DTC.
5. **MPU config verify**: Read back MPU region registers and verify stack guard and peripheral guard match expected values. Failure: remain in INIT.
6. **Stack canary init**: Write 0xDEADBEEF to stack bottom and verify readback. Failure: remain in INIT.
7. **RAM test pattern**: Write/read/compare 32-byte 0xAA/0x55 pattern at reserved address. Failure: remain in INIT.

If all safety-relevant tests pass, the FZC shall enter operational mode. Lidar failure is non-blocking (operates with safe default).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-025 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Startup self-test requirement SWR-FZC-025 is correctly ASIL D. The 7-step test sequence is comprehensive and FZC-specific: servo neutral calibration, SPI sensor check, UART lidar handshake, CAN loopback, MPU config verify, stack canary init, and RAM test. The decision to make lidar failure non-blocking (continue with safe default 0 cm) is correct since lidar is ASIL C supplemental safety, while the core FZC functions (steering/braking) are ASIL D. The 500 ms timeout for SPI sensor response and 500 ms window for 3 valid lidar frames are reasonable boot-time budgets. One note: step 1 references both TIM1 and TIM2 input capture readback, which should be verified against the actual hardware configuration. Traces to SYS-027/029 are correct; no TSR/SSR traces listed, which is acceptable for a startup procedure requirement.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-025 -->

---

## 13. CAN Message Configuration Requirements

### SWR-FZC-026: FZC CAN Message Reception

- **ASIL**: D
- **Traces up**: SYS-010, SYS-014, SYS-031
- **Traces down**: firmware/fzc/src/bsw/com/com_receive.c:Com_ReceiveMessage()
- **Verified by**: TC-FZC-050, TC-FZC-051
- **Verification method**: Unit test + integration test
- **Status**: draft

The FZC software shall receive and process the following CAN messages:

| CAN ID | Source | Content | Period | Safe Default |
|--------|--------|---------|--------|--------------|
| 0x001 | CVC | E-stop broadcast | 10 ms (when active) | Apply max brake + center steering |
| 0x100 | CVC | Vehicle state + torque request | 10 ms | SAFE_STOP assumed |
| 0x200 | CVC | Brake command | 10 ms | Auto-brake (100%) after 100 ms timeout |
| 0x201 | CVC | Steering command | 10 ms | Return-to-center after 100 ms timeout |

Each received message shall be validated via E2E check before processing.

---

### SWR-FZC-027: FZC CAN Message Transmission

- **ASIL**: D
- **Traces up**: SYS-015, SYS-031
- **Traces down**: firmware/fzc/src/bsw/com/com_transmit.c:Com_TransmitSchedule()
- **Verified by**: TC-FZC-052, TC-FZC-053
- **Verification method**: SIL + integration test
- **Status**: draft

The FZC software shall transmit the following CAN messages:

| CAN ID | Content | Period | DLC | E2E |
|--------|---------|--------|-----|-----|
| 0x011 | FZC heartbeat | 50 ms | 4 | Yes |
| 0x210 | Brake fault notification | Event-driven | 4 | Yes |
| 0x211 | Motor cutoff / steering fault | Event-driven | 6 | Yes |
| 0x220 | Lidar warning | Event-driven | 4 | Yes |

Event-driven messages shall be transmitted within 10 ms of the triggering event.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-026-027 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** CAN message configuration requirements SWR-FZC-026 and SWR-FZC-027 are correctly ASIL D as they define the communication interface for safety-critical data. The RX table (SWR-FZC-026) correctly lists all 4 CAN IDs the FZC receives (E-stop, vehicle state, brake command, steering command) with appropriate safe defaults: max brake + center steering for E-stop, SAFE_STOP assumed for state timeout, auto-brake for brake timeout, and return-to-center for steering timeout. The TX table (SWR-FZC-027) correctly lists 4 CAN IDs with appropriate E2E protection on all messages. The 10 ms latency requirement for event-driven messages is consistent with the FTTI analysis. Traces to SYS-010/014/015/031 are consistent with the CAN communication architecture.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-026-027 -->

---

## 14. Steering Command Timeout

### SWR-FZC-028: Steering Command CAN Timeout

- **ASIL**: D
- **Traces up**: SYS-010, SYS-034
- **Traces down**: firmware/fzc/src/swc/swc_steering.c:Swc_Steering_CheckTimeout()
- **Verified by**: TC-FZC-054, TC-FZC-055
- **Verification method**: SIL + fault injection
- **Status**: draft

The FZC software shall maintain a timestamp of the last valid steering command CAN message reception (CAN ID 0x201, passing E2E check). If the elapsed time since the last valid message exceeds 100 ms, the software shall initiate return-to-center (SWR-FZC-004) and set the steering to local control mode (CAN commands ignored until 5 consecutive valid messages received). This provides fail-safe steering behavior independent of CAN communication.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-028 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Steering command timeout requirement SWR-FZC-028 is correctly ASIL D. The 100 ms timeout threshold matches the brake command timeout in SWR-FZC-011, providing consistent CAN timeout behavior across FZC actuators. The return-to-center action on timeout is the correct fail-safe for steering. The local control mode with 5-consecutive-valid-messages recovery requirement mirrors the auto-brake release logic, ensuring stable communication is confirmed before returning to CVC control. This provides a defense-in-depth layer beyond E2E protection. Traces to SYS-010 and SYS-034 are appropriate; no TSR/SSR traces are listed, which could be an intentional omission for a derived functional requirement.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-028 -->

---

## 15. WCET and Scheduling Requirements

### SWR-FZC-029: RTOS Task Configuration and WCET Compliance

- **ASIL**: D
- **Traces up**: SYS-053, TSR-046, TSR-047, SSR-FZC-023
- **Traces down**: firmware/fzc/src/rte/rte_schedule.c:Rte_Schedule()
- **Verified by**: TC-FZC-043, TC-FZC-044
- **Verification method**: Analysis (WCET) + PIL timing measurement
- **Status**: draft

The FZC RTOS (FreeRTOS) scheduler shall configure the following runnables:

| Runnable | Period | RTOS Priority | WCET Budget | ASIL |
|----------|--------|---------------|-------------|------|
| Swc_SteeringMonitor | 10 ms | High (2) | 400 us | D |
| Swc_BrakeMonitor | 10 ms | High (2) | 300 us | D |
| Swc_LidarMonitor | 10 ms | High (2) | 400 us | C |
| Swc_CanReceive | 10 ms | High (2) | 300 us | D |
| Swc_HeartbeatTransmit | 50 ms | Medium (3) | 200 us | C |
| Swc_WatchdogFeed | 100 ms | Medium (3) | 100 us | D |
| Swc_BuzzerDriver | 10 ms | Low (4) | 100 us | QM |

Safety runnables (priority 2) shall preempt QM runnables (priority 4). Total worst-case CPU utilization shall not exceed 80%.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-029 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** WCET and scheduling requirement SWR-FZC-029 is correctly ASIL D. The runnable table is FZC-specific with 7 runnables covering steering, brake, lidar, CAN receive, heartbeat, watchdog, and buzzer. WCET budgets sum to 1800 us per 10 ms period, well under the 80% utilization limit. The priority assignment is correct: safety-critical runnables (steering, brake, lidar, CAN at priority 2) preempt QM buzzer (priority 4), ensuring temporal freedom from interference. The ASIL assignments per runnable are consistent with the individual requirement ASIL levels. One observation: the sum of all priority-2 WCET budgets (400+300+400+300=1400 us) represents 14% utilization at 10 ms, which is conservative and leaves substantial margin. Traces to SYS-053, TSR-046/047, and SSR-FZC-023 are consistent with the scheduling safety chain.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-029 -->

---

## 16. Diagnostic Requirements

### SWR-FZC-030: UDS Service Support

- **ASIL**: QM
- **Traces up**: SYS-037, SYS-038, SYS-039
- **Traces down**: firmware/fzc/src/bsw/dcm/dcm_main.c:Dcm_MainFunction()
- **Verified by**: TC-FZC-056, TC-FZC-057
- **Verification method**: Unit test + SIL
- **Status**: draft

The FZC software shall respond to UDS diagnostic requests on CAN ID 0x7E1 (FZC physical address) and CAN ID 0x7DF (functional broadcast). The FZC shall support: DiagnosticSessionControl (0x10), ReadDataByIdentifier (0x22), ReadDTCInformation (0x19), ClearDiagnosticInformation (0x14), SecurityAccess (0x27), and TesterPresent (0x3E). Responses on CAN ID 0x7E9. Supported DIDs:

| DID | Name | Length |
|-----|------|--------|
| 0xF190 | VIN | 17 bytes |
| 0xF191 | HW version | 4 bytes |
| 0xF195 | SW version | 4 bytes |
| 0xF020 | Steering angle | 2 bytes |
| 0xF021 | Brake position | 1 byte |
| 0xF022 | Lidar distance | 2 bytes |
| 0xF023 | Lidar signal strength | 2 bytes |
| 0xF024 | Buzzer pattern active | 1 byte |

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-030 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** UDS diagnostic requirement SWR-FZC-030 is correctly QM as diagnostics do not directly affect safety functions. The UDS service set (0x10, 0x22, 0x19, 0x14, 0x27, 0x3E) is standard and consistent with the CVC's diagnostic capability. The FZC physical address (0x7E1) and response address (0x7E9) follow the standard UDS addressing scheme. The DID table includes all relevant FZC sensor readings (steering angle, brake position, lidar distance/strength, buzzer pattern) which provides good diagnostic observability. The functional broadcast address (0x7DF) support is correct for fleet-wide diagnostic operations.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-030 -->

---

## 17. NVM Management

### SWR-FZC-031: FZC DTC Persistence

- **ASIL**: QM
- **Traces up**: SYS-041
- **Traces down**: firmware/fzc/src/bsw/dem/dem_nvm.c:Dem_Nvm_StoreDtc()
- **Verified by**: TC-FZC-058, TC-FZC-059
- **Verification method**: Unit test + power-cycle test
- **Status**: draft

The FZC software shall persist DTCs in a dedicated flash sector. Each entry shall contain: DTC number, fault status byte, occurrence counter, first/last occurrence timestamp, and freeze-frame data (steering angle, brake position, lidar distance, vehicle state, CAN error counters). The NVM shall support a minimum of 20 concurrent DTCs with CRC-16 protection per entry. DTCs shall survive power cycles.

---

### SWR-FZC-032: Servo Calibration Data Storage

- **ASIL**: QM
- **Traces up**: SYS-010, SYS-014
- **Traces down**: firmware/fzc/src/bsw/nvm/nvm_cal.c:Nvm_Cal_ReadWrite()
- **Verified by**: TC-FZC-060
- **Verification method**: Unit test
- **Status**: draft

The FZC software shall store servo calibration parameters in NVM with CRC-16 protection: steering center offset (default: 0 degrees), steering gain (default: 500 us per 45 degrees), brake servo zero offset (default: 1.0 ms), brake servo full offset (default: 2.0 ms), lidar warning threshold (default: 100 cm), lidar braking threshold (default: 50 cm), lidar emergency threshold (default: 20 cm). On startup, load from NVM; if CRC invalid, use compile-time defaults and log DTC.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-031-032 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** NVM management requirements SWR-FZC-031 and SWR-FZC-032 are correctly QM. DTC persistence (SWR-FZC-031) with 20 concurrent DTCs, CRC-16 protection, and comprehensive freeze-frame data (steering angle, brake position, lidar distance, vehicle state, CAN error counters) is well-specified and consistent with the CVC NVM design. Servo calibration storage (SWR-FZC-032) includes both steering and brake servo parameters plus all three lidar thresholds -- the CRC-16 protection with compile-time default fallback is a robust approach. The FZC-specific calibration parameters (steering gain, servo offsets, lidar thresholds) are all appropriate for the FZC's sensor/actuator set. Traces to SYS-041 for DTC persistence and SYS-010/014 for calibration are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-031-032 -->

---

## 18. Traceability Summary

### 18.1 SWR to Upstream Mapping

| SWR-FZC | SYS | TSR | SSR-FZC |
|---------|-----|-----|---------|
| SWR-FZC-001 | SYS-011, SYS-047 | TSR-010 | SSR-FZC-001 |
| SWR-FZC-002 | SYS-011 | TSR-011 | SSR-FZC-002 |
| SWR-FZC-003 | SYS-011 | TSR-011 | SSR-FZC-003 |
| SWR-FZC-004 | SYS-012 | TSR-012 | SSR-FZC-004 |
| SWR-FZC-005 | SYS-012 | TSR-012 | SSR-FZC-005 |
| SWR-FZC-006 | SYS-012 | TSR-013 | SSR-FZC-006 |
| SWR-FZC-007 | SYS-013 | TSR-014 | SSR-FZC-007 |
| SWR-FZC-008 | SYS-010, SYS-050 | — | — |
| SWR-FZC-009 | SYS-014, SYS-050 | TSR-015 | SSR-FZC-008 |
| SWR-FZC-010 | SYS-015 | TSR-016 | SSR-FZC-009 |
| SWR-FZC-011 | SYS-016 | TSR-017 | SSR-FZC-010 |
| SWR-FZC-012 | SYS-015 | TSR-048 | SSR-FZC-024 |
| SWR-FZC-013 | SYS-018, SYS-048 | TSR-018 | SSR-FZC-011 |
| SWR-FZC-014 | SYS-019 | TSR-019 | SSR-FZC-012 |
| SWR-FZC-015 | SYS-020 | TSR-020 | SSR-FZC-013 |
| SWR-FZC-016 | SYS-020 | TSR-021 | SSR-FZC-014 |
| SWR-FZC-017 | SYS-045 | TSR-044 | SSR-FZC-022 |
| SWR-FZC-018 | SYS-045 | — | — |
| SWR-FZC-019 | SYS-032 | TSR-022, TSR-023 | SSR-FZC-015, SSR-FZC-016 |
| SWR-FZC-020 | SYS-032 | TSR-024 | SSR-FZC-017 |
| SWR-FZC-021 | SYS-021 | TSR-025 | SSR-FZC-018 |
| SWR-FZC-022 | SYS-021 | TSR-026 | SSR-FZC-019 |
| SWR-FZC-023 | SYS-027 | TSR-031 | SSR-FZC-020 |
| SWR-FZC-024 | SYS-034 | TSR-038 | SSR-FZC-021 |
| SWR-FZC-025 | SYS-027, SYS-029 | — | — |
| SWR-FZC-026 | SYS-010, SYS-014, SYS-031 | — | — |
| SWR-FZC-027 | SYS-015, SYS-031 | — | — |
| SWR-FZC-028 | SYS-010, SYS-034 | — | — |
| SWR-FZC-029 | SYS-053 | TSR-046, TSR-047 | SSR-FZC-023 |
| SWR-FZC-030 | SYS-037, SYS-038, SYS-039 | — | — |
| SWR-FZC-031 | SYS-041 | — | — |
| SWR-FZC-032 | SYS-010, SYS-014 | — | — |

### 18.2 ASIL Distribution

| ASIL | Count | SWR IDs |
|------|-------|---------|
| D | 16 | SWR-FZC-001 to 006, 008, 009, 011, 012, 019, 020, 023, 025, 028, 029 |
| C | 8 | SWR-FZC-007, 013, 014, 015, 016, 021, 022, 024 |
| QM | 8 | SWR-FZC-010, 017, 018, 026, 027, 030, 031, 032 |
| **Total** | **32** | |

### 18.3 Verification Method Summary

| Method | Count |
|--------|-------|
| Unit test | 22 |
| SIL | 16 |
| PIL | 8 |
| Integration test | 6 |
| Fault injection | 8 |
| Demonstration | 2 |
| Analysis (WCET) | 1 |

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-FZC-TRACE -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The traceability summary is comprehensive with all 32 requirements mapped to SYS, TSR, and SSR-FZC upstream traces. The ASIL distribution (16 ASIL D, 8 ASIL C, 8 QM) is consistent with the FZC's dual role as safety-critical actuator controller (steering/braking at ASIL D) and supplemental safety sensor (lidar at ASIL C). One discrepancy noted: the ASIL C count lists 8 IDs but only shows SWR-FZC-007, 013, 014, 015, 016, 021, 022, 024 -- this is 8, which is correct. The QM count also shows 8 but lists SWR-FZC-010, 017, 018, 026, 027, 030, 031, 032 -- SWR-FZC-010 (Brake Fault CAN Notification) traces to TSR-016 and SSR-FZC-009, which suggests it should carry an ASIL rating rather than QM. This discrepancy should be resolved. Verification method coverage is broad with 22 unit tests, 16 SIL, 8 PIL, 6 integration, 8 fault injection, and 2 demonstrations.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-FZC-TRACE -->

---

## 19. Open Items and Assumptions

### 19.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| SWR-FZC-A-001 | FreeRTOS is the RTOS on the FZC | WCET and priority assignment |
| SWR-FZC-A-002 | Steering servo mechanical bandwidth supports 30 deg/s | Return-to-center timing |
| SWR-FZC-A-003 | TFMini-S outputs frames at 100 Hz continuously after power-on | Lidar timeout detection |
| SWR-FZC-A-004 | Brake servo feedback is measurable via TIM2 input capture | Brake PWM verification |

### 19.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| SWR-FZC-O-001 | Calibrate steering center offset on target hardware | Integration Engineer | Integration |
| SWR-FZC-O-002 | Verify lidar UART frame format against actual TFMini-S unit | Integration Engineer | PIL |
| SWR-FZC-O-003 | Perform WCET analysis for all FZC runnables on target | SW Engineer | SWE.3 |
| SWR-FZC-O-004 | Define Data ID assignment table for FZC CAN messages | System Architect | Before SWE.2 |

---

## 20. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete SWR specification: 32 requirements (SWR-FZC-001 to SWR-FZC-032), full traceability |

