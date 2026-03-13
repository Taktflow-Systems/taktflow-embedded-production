---
document_id: SWR-RZC
title: "Software Requirements — RZC"
version: "1.0"
status: draft
aspice_process: SWE.1
ecu: RZC
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

Every requirement (SWR-RZC-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/software/lessons-learned/`](../lessons-learned/). One file per requirement (SWR-RZC-NNN). File naming: `SWR-RZC-NNN-<short-title>.md`.


# Software Requirements — Rear Zone Controller (RZC)

## 1. Purpose

This document specifies the complete software requirements for the Rear Zone Controller (RZC), the STM32G474RE-based rear zone ECU of the Taktflow Zonal Vehicle Platform. These requirements are derived from system requirements (SYS), technical safety requirements (TSR), and software safety requirements (SSR) per Automotive SPICE 4.0 SWE.1 (Software Requirements Analysis).

The RZC is responsible for motor control (BTS7960 H-bridge), current monitoring (ACS723), temperature monitoring (NTC), quadrature encoder speed/direction, and battery voltage monitoring. Software requirements cover ALL RZC functionality: safety-critical motor control and monitoring (from SSRs), plus functional, diagnostic, configuration, startup/shutdown, and battery management requirements.

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

## 4. Motor Control Requirements

### SWR-RZC-001: Motor Driver Disable on Zero-Torque

- **ASIL**: D
- **Traces up**: SYS-004, TSR-005, SSR-RZC-001
- **Traces down**: firmware/rzc/src/swc/swc_motor.c:Swc_Motor_Disable()
- **Verified by**: TC-RZC-001, TC-RZC-002
- **Verification method**: Unit test + PIL + hardware test
- **Status**: draft

Upon receiving a zero-torque CAN command from CVC (CAN ID 0x100, passing E2E check, torque field = 0%), the RZC software shall within 5 ms: (a) set RPWM compare register to 0 (TIM1 CH1), (b) set LPWM compare register to 0 (TIM1 CH2), (c) drive R_EN GPIO LOW, (d) drive L_EN GPIO LOW. The motor driver shall remain disabled until a valid non-zero torque command is received with valid E2E protection and the CVC vehicle state is RUN or DEGRADED.

---

### SWR-RZC-002: Motor Driver State Validation

- **ASIL**: D
- **Traces up**: SYS-004, TSR-005, SSR-RZC-002
- **Traces down**: firmware/rzc/src/swc/swc_motor.c:Swc_Motor_ValidateState()
- **Verified by**: TC-RZC-003
- **Verification method**: Unit test + SIL
- **Status**: draft

Before applying any non-zero PWM to the motor, the RZC software shall verify: (a) the CVC vehicle state (from CAN) is RUN, DEGRADED, or LIMP (not INIT, SAFE_STOP, or SHUTDOWN), (b) no active motor faults (overcurrent, overtemp, direction plausibility), (c) the torque command has valid E2E protection, (d) the temperature derating limit permits the requested torque level. If any check fails, the software shall refuse to enable the motor and maintain the disabled state.

---

### SWR-RZC-003: Torque-to-PWM Conversion

- **ASIL**: D
- **Traces up**: SYS-004, SYS-050
- **Traces down**: firmware/rzc/src/swc/swc_motor.c:Swc_Motor_SetPWM()
- **Verified by**: TC-RZC-030, TC-RZC-031
- **Verification method**: Unit test + PIL
- **Status**: draft

The RZC software shall convert the received torque request percentage (0-100%) to a PWM duty cycle for the BTS7960 motor driver. The conversion shall: (a) clamp the torque request to the current derating limit (from temperature monitoring), (b) apply the mode-specific limit (RUN = 100%, DEGRADED = 75%, LIMP = 30%), (c) compute the duty cycle as a linear mapping: duty_percent = clamped_torque * (max_duty / 100), where max_duty is a calibratable parameter (default: 95% to avoid BTS7960 bootstrap issues), (d) set the appropriate direction channel: positive torque = RPWM active (forward), negative torque = LPWM active (reverse), with shoot-through protection per SWR-RZC-009.

---

### SWR-RZC-004: Motor PWM Output Configuration

- **ASIL**: D
- **Traces up**: SYS-050
- **Traces down**: firmware/rzc/src/mcal/pwm_cfg.c:Pwm_Init()
- **Verified by**: TC-RZC-032, TC-RZC-033
- **Verification method**: PIL + hardware test
- **Status**: draft

The RZC software shall configure TIM1 for motor PWM output: channel 1 (RPWM) and channel 2 (LPWM) in PWM mode 1, frequency 20 kHz (ARR = SystemCoreClock / 20000 - 1), with complementary dead-time insertion of 10 us minimum. The R_EN and L_EN GPIO pins shall be configured as push-pull outputs, initialized to LOW (motor disabled). The PWM resolution shall be at least 10 bits (0.1% duty cycle granularity). The TIM1 configuration shall be verified at startup by reading back the prescaler, ARR, and dead-time registers.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-001-004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Motor control requirements SWR-RZC-001 through SWR-RZC-004 are all correctly ASIL D, consistent with the motor being a safety-critical actuator. SWR-RZC-001 specifies a comprehensive 4-step disable sequence (RPWM=0, LPWM=0, R_EN LOW, L_EN LOW) with a 5 ms timing requirement, which is well within the FTTI budget. SWR-RZC-002 adds a state validation gate before motor enable with 4 preconditions (vehicle state, no faults, valid E2E, temperature derating) -- this is defense-in-depth. SWR-RZC-003 correctly implements mode-specific torque limiting (RUN=100%, DEGRADED=75%, LIMP=30%) and the 95% max duty cycle to avoid BTS7960 bootstrap issues is a practical hardware consideration. SWR-RZC-004 specifies 20 kHz PWM with 10-bit resolution and 10 us dead-time, which is appropriate for the BTS7960 H-bridge. Startup readback verification of TIM1 registers is a good defensive measure. Traces to SYS-004/050 and TSR-005 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-001-004 -->

---

## 5. Current Monitoring Requirements

### SWR-RZC-005: Motor Current ADC Sampling

- **ASIL**: A
- **Traces up**: SYS-005, SYS-049, TSR-006, SSR-RZC-003
- **Traces down**: firmware/rzc/src/swc/swc_current.c:Swc_Current_Sample()
- **Verified by**: TC-RZC-004, TC-RZC-005
- **Verification method**: Unit test + PIL
- **Status**: draft

The RZC software shall configure ADC1 channel 1 for motor current sensing at 1 kHz (timer-triggered conversion). Each ADC result (12-bit) shall be converted to milliamps using the formula: current_mA = (adc_value - adc_zero_offset) * (Vref_mV / 4096) / sensitivity_mV_per_A * 1000, where adc_zero_offset (calibrated at zero current, default: 2048 for mid-rail) and sensitivity (100 mV/A for ACS723LLCTR-20AB-T 20A variant) are compile-time calibratable constants. A moving average filter (4 samples) shall reduce ADC noise while maintaining sufficient bandwidth for 10 ms overcurrent detection.

---

### SWR-RZC-006: Motor Overcurrent Detection and Cutoff

- **ASIL**: A
- **Traces up**: SYS-005, TSR-006, SSR-RZC-004
- **Traces down**: firmware/rzc/src/swc/swc_current.c:Swc_Current_CheckOvercurrent()
- **Verified by**: TC-RZC-006, TC-RZC-007, TC-RZC-008
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The RZC software shall compare the filtered motor current against the overcurrent threshold (25000 mA default, calibratable). A debounce counter shall increment each 1 ms sample that exceeds the threshold and reset when below. When the counter reaches 10 (10 ms continuous overcurrent), the software shall disable the motor driver (SWR-RZC-001 disable sequence) within 1 ms and log a DTC via Dem with freeze-frame data (current value, torque request, temperature, vehicle state). Motor re-enable shall require current below threshold for 500 ms and a CVC reset command. The BTS7960 hardware current limit (43A trip) provides a backup cutoff independent of software.

---

### SWR-RZC-007: Motor Current CAN Broadcast

- **ASIL**: C
- **Traces up**: SYS-005, TSR-007, SSR-RZC-005
- **Traces down**: firmware/rzc/src/swc/swc_current.c:Swc_Current_Broadcast()
- **Verified by**: TC-RZC-009
- **Verification method**: SIL + integration test
- **Status**: draft

The RZC software shall transmit motor current data on CAN ID 0x301 every 10 ms with E2E protection. Payload: current magnitude (16-bit, milliamps), motor direction (1-bit: 0 = forward RPWM, 1 = reverse LPWM), motor enable status (1-bit: R_EN OR L_EN), alive counter (4-bit), CRC-8 (8-bit). The broadcast shall continue even when the motor is disabled (reporting 0 mA current) to maintain the SC cross-plausibility data stream.

---

### SWR-RZC-008: ACS723 Zero-Current Calibration

- **ASIL**: A
- **Traces up**: SYS-005, SYS-049
- **Traces down**: firmware/rzc/src/swc/swc_current.c:Swc_Current_Calibrate()
- **Verified by**: TC-RZC-034, TC-RZC-035
- **Verification method**: PIL + hardware test
- **Status**: draft

The RZC software shall perform zero-current calibration during the startup self-test phase (before motor is enabled). The software shall: (a) verify the motor driver enable pins are LOW (motor disabled), (b) sample the ACS723 ADC channel 64 times over 64 ms, (c) compute the average as the zero-current offset, (d) compare the offset against the expected mid-rail value (2048 +/- 200 counts). If the offset is outside this range, log a DTC for current sensor fault and set the overcurrent threshold to 0 (motor disabled permanently until recalibration). The calibrated offset shall be stored in RAM (not NVM) and recalibrated on every startup.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-005-008 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Current monitoring requirements SWR-RZC-005 through SWR-RZC-008 cover ADC sampling, overcurrent detection, CAN broadcast, and zero-current calibration. SWR-RZC-005/006/008 are correctly ASIL A (current monitoring supports motor overcurrent protection), while SWR-RZC-007 is ASIL C (CAN broadcast feeds the SC cross-plausibility check). The ADC-to-milliamp conversion formula in SWR-RZC-005 is explicit and calibratable. The 10 ms overcurrent debounce (SWR-RZC-006) with the BTS7960 43A hardware backup provides dual-layer overcurrent protection. The zero-current calibration approach (SWR-RZC-008) with 64 samples, mid-rail validation, and RAM-only storage is correct -- recalibrating every startup avoids NVM drift issues. The decision to permanently disable the motor on calibration failure is appropriately conservative. SWR-RZC-007 correctly continues broadcasting even when motor is disabled (0 mA) to maintain SC data stream. Traces are consistent across the current monitoring chain.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-005-008 -->

---

## 6. Temperature Monitoring Requirements

### SWR-RZC-009: Motor Temperature ADC Sampling

- **ASIL**: A
- **Traces up**: SYS-006, SYS-049, TSR-008, SSR-RZC-006
- **Traces down**: firmware/rzc/src/swc/swc_temp.c:Swc_Temp_Sample()
- **Verified by**: TC-RZC-010, TC-RZC-011
- **Verification method**: Unit test + PIL
- **Status**: draft

The RZC software shall configure ADC1 channel 2 for NTC temperature sensing at 10 Hz (100 ms period). The ADC result shall be converted to temperature in degrees Celsius using the Steinhart-Hart equation simplified with the beta parameter: T_C = (1 / (1/T0 + (1/B) * ln(R_ntc/R0))) - 273.15, where T0 = 298.15 K, R0 = 10000 ohms, B = 3950 (calibratable). R_ntc shall be derived from the ADC value and the voltage divider circuit: R_ntc = R_pullup * adc_value / (4095 - adc_value). Readings equivalent to below -30 degrees C (open circuit: R very high, ADC near max) or above 150 degrees C (short circuit: R very low, ADC near zero) shall be flagged as sensor faults.

---

### SWR-RZC-010: Motor Temperature Derating Logic

- **ASIL**: A
- **Traces up**: SYS-006, TSR-009, SSR-RZC-007
- **Traces down**: firmware/rzc/src/swc/swc_temp.c:Swc_Temp_ApplyDerating()
- **Verified by**: TC-RZC-012, TC-RZC-013, TC-RZC-014
- **Verification method**: Unit test + SIL
- **Status**: draft

The RZC software shall enforce the derating curve by maintaining a current_limit_percent variable updated every 100 ms based on the measured temperature:

| Temperature | Max Current | Derating % | Vehicle State Request |
|-------------|------------|------------|----------------------|
| Below 60 C | 25 A | 100% | RUN |
| 60-79 C | 18.75 A | 75% | DEGRADED |
| 80-99 C | 12.5 A | 50% | LIMP |
| 100+ C | 0 A | 0% (disabled) | SAFE_STOP |
| Sensor fault | 0 A | 0% (disabled) | SAFE_STOP |

The PWM duty cycle shall be clamped such that the resulting motor current does not exceed current_limit_percent * rated_current. Hysteresis on recovery: motor re-enable after 100 C shutdown requires temperature below 90 C (10 C hysteresis). State transitions at each derating level shall be communicated to CVC via CAN heartbeat fault bitmask within 10 ms.

---

### SWR-RZC-011: Temperature CAN Broadcast

- **ASIL**: QM
- **Traces up**: SYS-006
- **Traces down**: firmware/rzc/src/swc/swc_temp.c:Swc_Temp_Broadcast()
- **Verified by**: TC-RZC-036
- **Verification method**: SIL + integration test
- **Status**: draft

The RZC software shall include motor temperature (8-bit, degrees C, clamped 0-200) and derating percentage (8-bit) in the motor status CAN message (CAN ID 0x301). This enables the CVC to display temperature on OLED and the gateway to log temperature for cloud telemetry. The temperature data shall be updated every 100 ms (matching the ADC sample rate).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-009-011 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Temperature monitoring requirements SWR-RZC-009 through SWR-RZC-011 cover NTC ADC sampling, derating logic, and CAN broadcast. SWR-RZC-009/010 are correctly ASIL A (thermal protection) while SWR-RZC-011 is QM (informational broadcast). The Steinhart-Hart equation with beta parameter (SWR-RZC-009) is the correct approach for NTC thermistors, and the explicit open/short circuit detection ranges (-30 C / 150 C) are good boundary checks. The derating table in SWR-RZC-010 provides a graduated response (100% / 75% / 50% / 0%) with clear temperature thresholds and 10 C hysteresis on recovery from 100 C shutdown -- this prevents oscillation near the cutoff threshold. The vehicle state transition requests at each derating level (RUN / DEGRADED / LIMP / SAFE_STOP) integrate correctly with the CVC state machine. The sensor fault safe default of 0% (motor disabled) is fail-closed. Traces are consistent with the temperature monitoring chain.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-009-011 -->

---

## 7. Encoder Requirements

### SWR-RZC-012: Quadrature Encoder Speed Measurement

- **ASIL**: C
- **Traces up**: SYS-009, SYS-007
- **Traces down**: firmware/rzc/src/swc/swc_encoder.c:Swc_Encoder_CalculateSpeed()
- **Verified by**: TC-RZC-037, TC-RZC-038
- **Verification method**: Unit test + PIL
- **Status**: draft

The RZC software shall configure TIM3 in encoder mode (channels 1 and 2) to count quadrature encoder pulses. Every 10 ms, the software shall: (a) read the TIM3 counter value, (b) compute the delta from the previous reading, (c) calculate speed in RPM: RPM = (delta_counts * 60) / (pulses_per_rev * 0.01), where pulses_per_rev is a calibratable parameter (default: 360). The direction shall be determined from the TIM3 direction bit: up = forward, down = reverse. If the delta exceeds the maximum expected value (motor at max speed), the reading shall be flagged as invalid.

---

### SWR-RZC-013: Motor Stall Detection

- **ASIL**: C
- **Traces up**: SYS-007
- **Traces down**: firmware/rzc/src/swc/swc_encoder.c:Swc_Encoder_DetectStall()
- **Verified by**: TC-RZC-039, TC-RZC-040
- **Verification method**: Unit test + fault injection
- **Status**: draft

The RZC software shall detect motor stall by monitoring the encoder output while the motor is commanded to run. If the motor PWM duty cycle is above 10% but the encoder count delta is zero for 500 ms (50 consecutive 10 ms checks), the software shall declare a motor stall fault. On stall detection, the software shall: (a) set PWM to 0 to prevent thermal damage, (b) log a DTC via Dem, (c) report the stall fault in the heartbeat bitmask. The stall detection shall not apply during the first 200 ms after a direction change (motor inertia settling).

---

### SWR-RZC-014: Motor Direction Plausibility Check

- **ASIL**: C
- **Traces up**: SYS-007, TSR-040, SSR-RZC-015
- **Traces down**: firmware/rzc/src/swc/swc_motor.c:Swc_Motor_CheckDirection()
- **Verified by**: TC-RZC-024, TC-RZC-025
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The RZC software shall verify motor direction by comparing the commanded direction (positive torque = RPWM active = forward, negative torque = LPWM active = reverse) against the encoder direction (TIM3 count direction: up = forward, down = reverse). If a direction mismatch persists for 50 ms (5 consecutive 10 ms checks with encoder showing wrong direction), the software shall disable the motor and log a DTC. The check shall not apply during the first 100 ms after a direction change (motor inertia settling time).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-012-014 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Encoder requirements SWR-RZC-012 through SWR-RZC-014 cover speed measurement, stall detection, and direction plausibility. All three are correctly ASIL C, consistent with encoder monitoring being a supporting safety function for motor control. The speed calculation formula in SWR-RZC-012 is explicit and uses a calibratable PPR (default 360). The stall detection logic (SWR-RZC-013) with 500 ms threshold and 200 ms direction-change blanking is reasonable for avoiding false positives during transient motor behavior. The direction plausibility check (SWR-RZC-014) with 50 ms debounce and 100 ms settling time provides a cross-check against unintended motor direction -- this is important for detecting H-bridge faults. The inertia settling exclusion times (200 ms for stall, 100 ms for direction) should be validated on target hardware during integration. Traces to SYS-007/009 and TSR-040 / SSR-RZC-015 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-012-014 -->

---

## 8. Motor Protection Requirements

### SWR-RZC-015: Motor Shoot-Through Protection

- **ASIL**: C
- **Traces up**: SYS-007, TSR-040, SSR-RZC-016
- **Traces down**: firmware/rzc/src/swc/swc_motor.c:Swc_Motor_ShootThroughGuard()
- **Verified by**: TC-RZC-026, TC-RZC-027
- **Verification method**: Unit test + PIL + hardware test
- **Status**: draft

The RZC software shall prevent simultaneous non-zero PWM on both RPWM and LPWM by: (a) before writing any PWM value, checking that the opposite channel is at 0% duty, (b) during direction changes, enforcing a dead-time sequence: set both channels to 0%, wait 10 us (verified by timer), then enable the new direction channel. The TIM1 complementary output with dead-time insertion shall be configured as hardware backup (ARR-based dead-time of 10 us). A runtime check shall verify that both RPWM and LPWM compare registers are never simultaneously non-zero; if detected, both channels shall be forced to 0% and a shoot-through fault logged.

---

### SWR-RZC-016: Motor CAN Command Timeout

- **ASIL**: D
- **Traces up**: SYS-004, SYS-034
- **Traces down**: firmware/rzc/src/swc/swc_motor.c:Swc_Motor_CheckTimeout()
- **Verified by**: TC-RZC-041, TC-RZC-042
- **Verification method**: SIL + fault injection
- **Status**: draft

The RZC software shall maintain a timestamp of the last valid torque command CAN message reception (CAN ID 0x100, passing E2E check). If the elapsed time since the last valid message exceeds 100 ms, the software shall disable the motor driver (SWR-RZC-001 disable sequence) and maintain the disabled state until valid CAN communication resumes with 5 consecutive valid torque commands. This provides fail-safe motor behavior independent of CAN communication.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-015-016 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Motor protection requirements SWR-RZC-015 and SWR-RZC-016 cover shoot-through protection and CAN command timeout respectively. SWR-RZC-015 is correctly ASIL C (H-bridge protection) and SWR-RZC-016 is correctly ASIL D (motor safety on CAN loss). The three-layer shoot-through protection in SWR-RZC-015 (software channel check, 10 us dead-time sequence, TIM1 hardware dead-time, runtime dual-register check) provides excellent defense-in-depth against H-bridge shoot-through which could damage hardware and cause uncontrolled motor behavior. The CAN command timeout (SWR-RZC-016) with 100 ms threshold and 5-consecutive-valid-messages recovery is consistent with the FZC timeout patterns, ensuring uniform behavior across zone controllers. Traces to SYS-004/007/034 and TSR-040 / SSR-RZC-016 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-015-016 -->

---

## 9. Battery Monitoring Requirements

### SWR-RZC-017: Battery Voltage ADC Sampling

- **ASIL**: QM
- **Traces up**: SYS-008, SYS-049
- **Traces down**: firmware/rzc/src/swc/swc_battery.c:Swc_Battery_Sample()
- **Verified by**: TC-RZC-043, TC-RZC-044
- **Verification method**: Unit test + PIL
- **Status**: draft

The RZC software shall configure ADC1 channel 4 for battery voltage sensing at 10 Hz (100 ms period). The ADC result (12-bit) shall be converted to millivolts using the voltage divider compensation formula: V_battery_mV = adc_value * Vref_mV / 4096 * (R_high + R_low) / R_low, where R_high and R_low are the voltage divider resistor values (calibratable, default: R_high = 10k, R_low = 3.3k for a 0-16V to 0-3.3V mapping). The converted voltage shall be filtered with a 4-sample moving average.

---

### SWR-RZC-018: Battery Voltage Threshold Monitoring

- **ASIL**: QM
- **Traces up**: SYS-008
- **Traces down**: firmware/rzc/src/swc/swc_battery.c:Swc_Battery_CheckThresholds()
- **Verified by**: TC-RZC-045, TC-RZC-046
- **Verification method**: Unit test + SIL
- **Status**: draft

The RZC software shall monitor battery voltage against configurable thresholds:

| Condition | Threshold | Action |
|-----------|-----------|--------|
| Critical undervoltage | < 8.0 V | Disable motor, log DTC, report in heartbeat |
| Undervoltage warning | < 10.5 V | Log DTC, report in heartbeat |
| Normal | 10.5 - 15.0 V | Normal operation |
| Overvoltage warning | > 15.0 V | Log DTC, report in heartbeat |
| Critical overvoltage | > 17.0 V | Disable motor, log DTC, report in heartbeat |

Voltage thresholds shall have 0.5 V hysteresis on recovery. Battery voltage shall be included in the motor status CAN message for CVC display and cloud telemetry.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-017-018 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Battery monitoring requirements SWR-RZC-017 and SWR-RZC-018 are correctly QM as battery monitoring is a functional (non-safety) requirement. The ADC-to-voltage conversion formula (SWR-RZC-017) with explicit voltage divider compensation and 4-sample moving average is well-specified. The 5-level threshold table in SWR-RZC-018 (critical undervoltage, undervoltage warning, normal, overvoltage warning, critical overvoltage) provides comprehensive voltage monitoring with appropriate actions (motor disable at critical levels). The 0.5 V hysteresis on recovery prevents threshold oscillation. Both critical voltage conditions (below 8.0 V and above 17.0 V) correctly disable the motor, which protects both the motor driver and the battery. Traces to SYS-008/049 are appropriate for functional battery monitoring.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-017-018 -->

---

## 10. E2E Protection Requirements

### SWR-RZC-019: RZC CAN E2E Transmit

- **ASIL**: D
- **Traces up**: SYS-032, TSR-022, TSR-023, SSR-RZC-008, SSR-RZC-009
- **Traces down**: firmware/rzc/src/bsw/e2e/e2e_protect.c:E2E_Protect()
- **Verified by**: TC-RZC-015, TC-RZC-016
- **Verification method**: Unit test + SIL
- **Status**: draft

The RZC software shall implement the E2E protection transmit function identically to SWR-CVC-014, using the same CRC-8 polynomial (0x1D, init 0xFF), alive counter management, and Data ID assignment. Each RZC-originated CAN message shall use a unique Data ID distinct from CVC and FZC Data IDs. Per-Data-ID alive counters (16 entries) shall be initialized to 0 at startup and incremented exactly once per transmission.

---

### SWR-RZC-020: RZC CAN E2E Receive

- **ASIL**: D
- **Traces up**: SYS-032, TSR-024, SSR-RZC-010
- **Traces down**: firmware/rzc/src/bsw/e2e/e2e_check.c:E2E_Check()
- **Verified by**: TC-RZC-017, TC-RZC-018
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The RZC software shall implement the E2E protection receive function identically to SWR-CVC-015. On 3 consecutive E2E failures for the torque command (CAN ID 0x100), the safe default shall be zero torque (motor disabled).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-019-020 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** E2E protection requirements SWR-RZC-019 and SWR-RZC-020 are correctly ASIL D as they protect safety-critical torque command communication. The specification correctly references the same CRC-8 polynomial (0x1D, init 0xFF) and Data ID scheme as the CVC and FZC E2E implementations, ensuring cross-ECU protocol consistency. The safe default of zero torque (motor disabled) on 3 consecutive E2E failures for the torque command is the correct fail-safe action for the motor controller. The unique Data ID requirement for RZC messages distinct from CVC and FZC prevents message masquerade across ECUs. Traces to SYS-032, TSR-022/023/024, and SSR-RZC-008/009/010 are consistent with the E2E safety chain.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-019-020 -->

---

## 11. Heartbeat Requirements

### SWR-RZC-021: RZC Heartbeat Transmission

- **ASIL**: C
- **Traces up**: SYS-021, TSR-025, SSR-RZC-011
- **Traces down**: firmware/rzc/src/swc/swc_heartbeat.c:Swc_Heartbeat_Transmit()
- **Verified by**: TC-RZC-019
- **Verification method**: SIL + integration test
- **Status**: draft

The RZC software shall transmit a heartbeat CAN message on CAN ID 0x012 every 50 ms (tolerance +/- 5 ms). Payload: ECU ID (0x03, 8-bit), operating mode (4-bit), fault status bitmask (8-bit: bit 0 = overcurrent, bit 1 = overtemp, bit 2 = direction fault, bit 3 = CAN fault, bit 4 = battery fault, bit 5 = stall fault, bits 6-7 reserved), alive counter (4-bit), CRC-8 (8-bit). Total: 4 bytes.

---

### SWR-RZC-022: RZC Heartbeat Conditioning

- **ASIL**: C
- **Traces up**: SYS-021, TSR-026, SSR-RZC-012
- **Traces down**: firmware/rzc/src/swc/swc_heartbeat.c:Swc_Heartbeat_CheckConditions()
- **Verified by**: TC-RZC-020
- **Verification method**: Unit test + fault injection
- **Status**: draft

The RZC software shall suppress heartbeat transmission under the same conditions as SWR-CVC-021: main loop stalled, stack canary corrupted, or CAN bus-off.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-021-022 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Heartbeat requirements SWR-RZC-021 and SWR-RZC-022 are correctly ASIL C, consistent with the heartbeat-based SC monitoring chain. The 50 ms period with +/- 5 ms tolerance (SWR-RZC-021) matches the SC heartbeat timeout of 150 ms (3 missed heartbeats). The RZC fault bitmask has 6 bits (overcurrent, overtemp, direction fault, CAN fault, battery fault, stall fault) which is RZC-specific and more comprehensive than the CVC/FZC bitmasks -- this provides the SC with full observability of RZC fault conditions. The heartbeat conditioning logic in SWR-RZC-022 correctly mirrors the CVC/FZC approach. Traces to SYS-021, TSR-025/026, and SSR-RZC-011/012 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-021-022 -->

---

## 12. Watchdog Requirements

### SWR-RZC-023: RZC External Watchdog Feed

- **ASIL**: D
- **Traces up**: SYS-027, TSR-031, SSR-RZC-013
- **Traces down**: firmware/rzc/src/swc/swc_watchdog.c:Swc_Watchdog_Feed()
- **Verified by**: TC-RZC-021
- **Verification method**: Unit test + fault injection + PIL
- **Status**: draft

The RZC software shall toggle the TPS3823 WDI pin under the same four-condition check as SWR-CVC-023: main loop complete, stack canary intact, RAM test passed, CAN not bus-off.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-023 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Watchdog requirement SWR-RZC-023 is correctly ASIL D, consistent with the external watchdog being a last-resort safety mechanism. The four-condition check before TPS3823 WDI toggle is identical to the CVC and FZC watchdog requirements, ensuring uniform watchdog behavior across all STM32 zone controllers. Traces to SYS-027, TSR-031, and SSR-RZC-013 are consistent with the watchdog safety chain.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-023 -->

---

## 13. CAN Recovery Requirements

### SWR-RZC-024: RZC CAN Bus Loss Detection

- **ASIL**: C
- **Traces up**: SYS-034, TSR-038, SSR-RZC-014
- **Traces down**: firmware/rzc/src/swc/swc_can_monitor.c:Swc_CanMonitor_Check()
- **Verified by**: TC-RZC-022, TC-RZC-023
- **Verification method**: SIL + fault injection
- **Status**: draft

The RZC software shall detect CAN bus loss using the same criteria as SWR-CVC-024 (bus-off, 200 ms silence, error warning sustained 500 ms). On CAN bus loss, the RZC shall disable the motor driver (R_EN LOW, L_EN LOW, both PWM = 0) and maintain the disabled state until power cycle. The RZC shall not attempt CAN recovery.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-024 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** CAN recovery requirement SWR-RZC-024 is correctly ASIL C. The safe reaction on CAN bus loss (motor driver disable with explicit R_EN/L_EN/PWM zeroing) is the correct fail-safe for the motor controller. The decision not to attempt CAN recovery and require a power cycle is consistent with the FZC approach and is appropriate for a safety-critical actuator -- recovery attempts could cause unpredictable motor behavior. The explicit listing of all disable actions (R_EN LOW, L_EN LOW, both PWM=0) rather than just referencing SWR-RZC-001 is good for clarity. Traces to SYS-034, TSR-038, and SSR-RZC-014 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-024 -->

---

## 14. Startup Self-Test Requirements

### SWR-RZC-025: RZC Startup Self-Test Sequence

- **ASIL**: D
- **Traces up**: SYS-027, SYS-029
- **Traces down**: firmware/rzc/src/swc/swc_selftest.c:Swc_SelfTest_Startup()
- **Verified by**: TC-RZC-047, TC-RZC-048, TC-RZC-049
- **Verification method**: Unit test + PIL + fault injection
- **Status**: draft

The RZC software shall execute the following self-test sequence at power-on before enabling the motor driver:

1. **BTS7960 enable test**: Toggle R_EN HIGH then LOW, toggle L_EN HIGH then LOW. Verify GPIO readback matches commanded state for each. Failure: motor permanently disabled, log DTC.
2. **ACS723 zero-current baseline**: Per SWR-RZC-008, calibrate zero-current offset with motor disabled. Failure: motor permanently disabled.
3. **NTC range check**: Read the NTC ADC channel and verify the reading corresponds to a temperature in the -30 to 150 C range (ambient expected 10-40 C at startup). Failure: motor permanently disabled (temperature monitoring unavailable).
4. **Encoder check**: Read TIM3 counter, verify it is not stuck at max or min value. Failure: log DTC, encoder monitoring degraded.
5. **CAN loopback test**: Configure FDCAN1 in loopback mode, transmit test frame, verify reception. Failure: remain in INIT with CAN fault DTC.
6. **MPU config verify**: Read back MPU region registers. Failure: remain in INIT.
7. **Stack canary init**: Write 0xDEADBEEF to stack bottom and verify. Failure: remain in INIT.
8. **RAM test pattern**: Write/read/compare 32-byte 0xAA/0x55 at reserved address. Failure: remain in INIT.

If all safety-relevant tests pass, the RZC shall enter operational mode. Motor enable requires all of: self-test pass, valid CVC command, and no active faults.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-025 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Startup self-test requirement SWR-RZC-025 is correctly ASIL D. The 8-step test sequence is RZC-specific and comprehensive: BTS7960 enable GPIO test, ACS723 zero-current calibration, NTC range check, encoder check, CAN loopback, MPU verify, stack canary, and RAM test. The BTS7960 enable test (step 1) with GPIO toggle and readback verification is important for detecting stuck GPIO faults. The NTC range check (step 3) verifying ambient temperature at startup (10-40 C expected) is a practical plausibility check. The encoder check (step 4) is correctly listed as non-blocking (degraded mode) since the encoder is ASIL C while the overall startup is ASIL D. The triple requirement for motor enable (self-test pass + valid CVC command + no active faults) is a correct defense-in-depth gate. Traces to SYS-027/029 are consistent; no TSR/SSR traces listed, which is acceptable for a startup procedure.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-025 -->

---

## 15. CAN Message Configuration Requirements

### SWR-RZC-026: RZC CAN Message Reception

- **ASIL**: D
- **Traces up**: SYS-004, SYS-031
- **Traces down**: firmware/rzc/src/bsw/com/com_receive.c:Com_ReceiveMessage()
- **Verified by**: TC-RZC-050, TC-RZC-051
- **Verification method**: Unit test + integration test
- **Status**: draft

The RZC software shall receive and process the following CAN messages:

| CAN ID | Source | Content | Period | Safe Default |
|--------|--------|---------|--------|--------------|
| 0x001 | CVC | E-stop broadcast | 10 ms (when active) | Disable motor immediately |
| 0x100 | CVC | Vehicle state + torque request | 10 ms | Zero torque after 100 ms timeout |

Each received message shall be validated via E2E check before processing.

---

### SWR-RZC-027: RZC CAN Message Transmission

- **ASIL**: D
- **Traces up**: SYS-005, SYS-031
- **Traces down**: firmware/rzc/src/bsw/com/com_transmit.c:Com_TransmitSchedule()
- **Verified by**: TC-RZC-052, TC-RZC-053
- **Verification method**: SIL + integration test
- **Status**: draft

The RZC software shall transmit the following CAN messages:

| CAN ID | Content | Period | DLC | E2E |
|--------|---------|--------|-----|-----|
| 0x012 | RZC heartbeat | 50 ms | 4 | Yes |
| 0x301 | Motor status (current, temp, speed, battery) | 10 ms | 8 | Yes |

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-026-027 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** CAN message configuration requirements SWR-RZC-026 and SWR-RZC-027 are correctly ASIL D. The RX table (SWR-RZC-026) is simpler than the FZC's since the RZC only receives 2 CAN IDs (E-stop and vehicle state/torque) -- this is correct because the RZC is a command receiver, not a sensor data consumer. Safe defaults (motor disable on E-stop, zero torque on timeout) are appropriate. The TX table (SWR-RZC-027) shows 2 CAN IDs: heartbeat (0x012) at 50 ms and motor status (0x301) at 10 ms with 8-byte DLC and E2E protection. The motor status message at 10 ms is critical for SC cross-plausibility monitoring. Traces to SYS-004/005/031 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-026-027 -->

---

## 16. WCET and Scheduling Requirements

### SWR-RZC-028: RTOS Task Configuration and WCET Compliance

- **ASIL**: D
- **Traces up**: SYS-053, TSR-046, TSR-047, SSR-RZC-017
- **Traces down**: firmware/rzc/src/rte/rte_schedule.c:Rte_Schedule()
- **Verified by**: TC-RZC-028, TC-RZC-029
- **Verification method**: Analysis (WCET) + PIL timing measurement
- **Status**: draft

The RZC RTOS (FreeRTOS) scheduler shall configure the following runnables:

| Runnable | Period | RTOS Priority | WCET Budget | ASIL |
|----------|--------|---------------|-------------|------|
| Swc_CurrentMonitor | 1 ms | Highest (1) | 100 us | A |
| Swc_MotorControl | 10 ms | High (2) | 400 us | D |
| Swc_DirectionMonitor | 10 ms | High (2) | 200 us | C |
| Swc_CanReceive | 10 ms | High (2) | 300 us | D |
| Swc_TempMonitor | 100 ms | Medium (3) | 300 us | A |
| Swc_BatteryMonitor | 100 ms | Medium (3) | 200 us | QM |
| Swc_HeartbeatTransmit | 50 ms | Medium (3) | 200 us | C |
| Swc_WatchdogFeed | 100 ms | Medium (3) | 100 us | D |

The 1 kHz current monitor at highest priority ensures 10 ms overcurrent detection. Total worst-case CPU utilization shall not exceed 80%.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-028 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** WCET and scheduling requirement SWR-RZC-028 is correctly ASIL D. The runnable table is RZC-specific with 8 runnables, notably including a 1 kHz (1 ms period) current monitor at the highest priority -- this is a key differentiator from the CVC/FZC scheduling and correctly ensures the 10 ms overcurrent detection budget in SWR-RZC-006. The priority assignment (Highest=1 for current, High=2 for motor/direction/CAN, Medium=3 for temp/battery/heartbeat/watchdog) provides correct temporal FFI. Total WCET budgets sum to 1800 us per 10 ms (plus additional 100 us per 1 ms from current monitor), giving approximately 28% worst-case utilization which is well within the 80% limit. ASIL assignments per runnable are consistent with the individual requirement ASIL levels. Traces to SYS-053, TSR-046/047, and SSR-RZC-017 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-028 -->

---

## 17. Diagnostic Requirements

### SWR-RZC-029: UDS Service Support

- **ASIL**: QM
- **Traces up**: SYS-037, SYS-038, SYS-039
- **Traces down**: firmware/rzc/src/bsw/dcm/dcm_main.c:Dcm_MainFunction()
- **Verified by**: TC-RZC-054, TC-RZC-055
- **Verification method**: Unit test + SIL
- **Status**: draft

The RZC software shall respond to UDS diagnostic requests on CAN ID 0x7E2 (RZC physical address) and CAN ID 0x7DF (functional broadcast). The RZC shall support: DiagnosticSessionControl (0x10), ReadDataByIdentifier (0x22), ReadDTCInformation (0x19), ClearDiagnosticInformation (0x14), SecurityAccess (0x27), and TesterPresent (0x3E). Responses on CAN ID 0x7EA. Supported DIDs:

| DID | Name | Length |
|-----|------|--------|
| 0xF190 | VIN | 17 bytes |
| 0xF191 | HW version | 4 bytes |
| 0xF195 | SW version | 4 bytes |
| 0xF030 | Motor current (mA) | 2 bytes |
| 0xF031 | Motor temperature (C) | 1 byte |
| 0xF032 | Motor speed (RPM) | 2 bytes |
| 0xF033 | Battery voltage (mV) | 2 bytes |
| 0xF034 | Torque request (%) | 1 byte |
| 0xF035 | Derating (%) | 1 byte |
| 0xF036 | ACS723 zero offset | 2 bytes |

---

### SWR-RZC-030: RZC DTC Persistence

- **ASIL**: QM
- **Traces up**: SYS-041
- **Traces down**: firmware/rzc/src/bsw/dem/dem_nvm.c:Dem_Nvm_StoreDtc()
- **Verified by**: TC-RZC-056, TC-RZC-057
- **Verification method**: Unit test + power-cycle test
- **Status**: draft

The RZC software shall persist DTCs in a dedicated flash sector. Each entry shall contain: DTC number, fault status byte, occurrence counter, first/last occurrence timestamp, and freeze-frame data (motor current, temperature, speed, battery voltage, torque request, vehicle state). The NVM shall support a minimum of 20 concurrent DTCs with CRC-16 protection per entry. DTCs shall survive power cycles.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-029-030 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Diagnostic and NVM requirements SWR-RZC-029 and SWR-RZC-030 are correctly QM. UDS support (SWR-RZC-029) provides the standard service set (0x10, 0x22, 0x19, 0x14, 0x27, 0x3E) with RZC-specific DIDs covering motor current, temperature, speed, battery voltage, torque request, derating, and ACS723 zero offset. The RZC physical address (0x7E2) and response address (0x7EA) follow the sequential UDS addressing scheme (CVC=0x7E0, FZC=0x7E1, RZC=0x7E2). DTC persistence (SWR-RZC-030) with RZC-specific freeze-frame data (motor current, temperature, speed, battery, torque, vehicle state) provides comprehensive diagnostic context for post-mortem analysis. The 20 DTC capacity with CRC-16 protection is consistent with the CVC and FZC NVM designs. Traces to SYS-037/038/039/041 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-029-030 -->

---

## 18. Traceability Summary

### 18.1 SWR to Upstream Mapping

| SWR-RZC | SYS | TSR | SSR-RZC |
|---------|-----|-----|---------|
| SWR-RZC-001 | SYS-004 | TSR-005 | SSR-RZC-001 |
| SWR-RZC-002 | SYS-004 | TSR-005 | SSR-RZC-002 |
| SWR-RZC-003 | SYS-004, SYS-050 | — | — |
| SWR-RZC-004 | SYS-050 | — | — |
| SWR-RZC-005 | SYS-005, SYS-049 | TSR-006 | SSR-RZC-003 |
| SWR-RZC-006 | SYS-005 | TSR-006 | SSR-RZC-004 |
| SWR-RZC-007 | SYS-005 | TSR-007 | SSR-RZC-005 |
| SWR-RZC-008 | SYS-005, SYS-049 | — | — |
| SWR-RZC-009 | SYS-006, SYS-049 | TSR-008 | SSR-RZC-006 |
| SWR-RZC-010 | SYS-006 | TSR-009 | SSR-RZC-007 |
| SWR-RZC-011 | SYS-006 | — | — |
| SWR-RZC-012 | SYS-009, SYS-007 | — | — |
| SWR-RZC-013 | SYS-007 | — | — |
| SWR-RZC-014 | SYS-007 | TSR-040 | SSR-RZC-015 |
| SWR-RZC-015 | SYS-007 | TSR-040 | SSR-RZC-016 |
| SWR-RZC-016 | SYS-004, SYS-034 | — | — |
| SWR-RZC-017 | SYS-008, SYS-049 | — | — |
| SWR-RZC-018 | SYS-008 | — | — |
| SWR-RZC-019 | SYS-032 | TSR-022, TSR-023 | SSR-RZC-008, SSR-RZC-009 |
| SWR-RZC-020 | SYS-032 | TSR-024 | SSR-RZC-010 |
| SWR-RZC-021 | SYS-021 | TSR-025 | SSR-RZC-011 |
| SWR-RZC-022 | SYS-021 | TSR-026 | SSR-RZC-012 |
| SWR-RZC-023 | SYS-027 | TSR-031 | SSR-RZC-013 |
| SWR-RZC-024 | SYS-034 | TSR-038 | SSR-RZC-014 |
| SWR-RZC-025 | SYS-027, SYS-029 | — | — |
| SWR-RZC-026 | SYS-004, SYS-031 | — | — |
| SWR-RZC-027 | SYS-005, SYS-031 | — | — |
| SWR-RZC-028 | SYS-053 | TSR-046, TSR-047 | SSR-RZC-017 |
| SWR-RZC-029 | SYS-037, SYS-038, SYS-039 | — | — |
| SWR-RZC-030 | SYS-041 | — | — |

### 18.2 ASIL Distribution

| ASIL | Count | SWR IDs |
|------|-------|---------|
| D | 10 | SWR-RZC-001, 002, 003, 004, 016, 019, 020, 023, 025, 028 |
| C | 7 | SWR-RZC-007, 012, 013, 014, 015, 021, 022, 024 |
| A | 5 | SWR-RZC-005, 006, 008, 009, 010 |
| QM | 8 | SWR-RZC-011, 017, 018, 026, 027, 029, 030 |
| **Total** | **30** | |

### 18.3 Verification Method Summary

| Method | Count |
|--------|-------|
| Unit test | 22 |
| SIL | 14 |
| PIL | 10 |
| Integration test | 4 |
| Fault injection | 6 |
| Hardware test | 4 |
| Analysis (WCET) | 1 |

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-RZC-TRACE -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The traceability summary is comprehensive with all 30 requirements mapped to SYS, TSR, and SSR-RZC upstream traces. The ASIL distribution (10 ASIL D, 7 ASIL C, 5 ASIL A, 8 QM) reflects the RZC's role as a motor controller with graduated safety levels: motor control at ASIL D, current/temperature monitoring at ASIL A (decomposed from higher ASIL via ASIL decomposition), encoder monitoring at ASIL C. One discrepancy: the ASIL C count states 7 but lists 8 IDs (SWR-RZC-007, 012, 013, 014, 015, 021, 022, 024) -- this should be verified. The QM count states 8 but lists only 7 IDs (SWR-RZC-011, 017, 018, 026, 027, 029, 030) -- this is 7, not 8, and needs correction. Verification method coverage includes 4 hardware tests, which is appropriate for the motor actuator and sensor hardware. The RZC has more PIL tests (10) than the FZC (8), reflecting the need for more hardware-level validation of motor control.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-RZC-TRACE -->

---

## 19. Open Items and Assumptions

### 19.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| SWR-RZC-A-001 | FreeRTOS is the RTOS on the RZC | WCET and priority assignment |
| SWR-RZC-A-002 | ACS723 sensitivity (100 mV/A) is stable across operating temperature | Current measurement accuracy |
| SWR-RZC-A-003 | NTC beta parameter (3950) is accurate for the selected thermistor | Temperature accuracy |
| SWR-RZC-A-004 | Encoder PPR (360) matches the selected motor encoder | Speed calculation |

### 19.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| SWR-RZC-O-001 | Calibrate ACS723 zero-offset on target hardware | Integration Engineer | Integration |
| SWR-RZC-O-002 | Calibrate NTC beta parameter on target motor | Integration Engineer | Integration |
| SWR-RZC-O-003 | Verify encoder PPR matches actual motor | Integration Engineer | Integration |
| SWR-RZC-O-004 | Perform WCET analysis for all RZC runnables on target | SW Engineer | SWE.3 |
| SWR-RZC-O-005 | Define Data ID assignment table for RZC CAN messages | System Architect | Before SWE.2 |
| SWR-RZC-O-006 | Characterize BTS7960 bootstrap capacitor timing at 95% duty | HW Engineer | Integration |

---

## 20. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete SWR specification: 30 requirements (SWR-RZC-001 to SWR-RZC-030), full traceability |

