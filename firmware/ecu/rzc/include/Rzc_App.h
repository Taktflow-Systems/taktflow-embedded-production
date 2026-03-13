/**
 * @file    Rzc_App.h
 * @brief   RZC application-specific constants — hand-authored, NOT generated
 * @date    2026-02-23
 *
 * @details Application-level constants for the Rear Zone Controller:
 *          motor, current, temperature, encoder, battery, heartbeat,
 *          self-test, fault bitmasks, and compatibility aliases.
 *
 *          PDU IDs, signal IDs, DTC IDs, and E2E data IDs are in the
 *          generated Rzc_Cfg.h — do NOT duplicate them here.
 *
 * @safety_req SWR-RZC-001 to SWR-RZC-030
 * @traces_to  SSR-RZC-001 to SSR-RZC-017, TSR-022, TSR-030, TSR-038, TSR-046
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef RZC_APP_H
#define RZC_APP_H

/* ====================================================================
 * RTE Signal IDs (SWC-level short names)
 * These map to internal RTE signal slots used by Rte_Read/Rte_Write.
 * ==================================================================== */

#define RZC_SIG_TORQUE_CMD         16u
#define RZC_SIG_TORQUE_ECHO        17u
#define RZC_SIG_MOTOR_SPEED        18u
#define RZC_SIG_MOTOR_DIR          19u
#define RZC_SIG_MOTOR_ENABLE       20u
#define RZC_SIG_MOTOR_FAULT        21u
#define RZC_SIG_CURRENT_MA         22u
#define RZC_SIG_OVERCURRENT        23u
#define RZC_SIG_TEMP1_DC           24u
#define RZC_SIG_TEMP2_DC           25u
#define RZC_SIG_DERATING_PCT       26u
#define RZC_SIG_TEMP_FAULT         27u
#define RZC_SIG_BATTERY_MV         28u
#define RZC_SIG_BATTERY_STATUS     29u
#define RZC_SIG_ENCODER_SPEED      30u
#define RZC_SIG_ENCODER_DIR        31u
#define RZC_SIG_ENCODER_STALL      32u
#define RZC_SIG_VEHICLE_STATE      33u
#define RZC_SIG_ESTOP_ACTIVE       34u
#define RZC_SIG_FAULT_MASK         35u
#define RZC_SIG_SELF_TEST_RESULT   36u
#define RZC_SIG_HEARTBEAT_ALIVE    37u
#define RZC_SIG_SAFETY_STATUS      38u
#define RZC_SIG_CMD_TIMEOUT        39u
#define RZC_SIG_BATTERY_SOC        40u

/* ====================================================================
 * Com Signal IDs for Virtual Sensors (RX from plant-sim, SIL only)
 * ==================================================================== */

#define RZC_COM_SIG_RX_VIRT_MOTOR_CURRENT   18u  /* uint16 LE, mA */
#define RZC_COM_SIG_RX_VIRT_MOTOR_TEMP      19u  /* uint16 LE, 0.1C */
#define RZC_COM_SIG_RX_VIRT_BATTERY_VOLTAGE 20u  /* uint16 LE, mV */
#define RZC_COM_SIG_RX_VIRT_MOTOR_RPM      21u  /* uint16 LE, RPM */

/* ADC group/channel mapping for virtual sensor injection (SIL) */
#define RZC_MOTOR_CURRENT_ADC_GROUP    0u
#define RZC_MOTOR_CURRENT_ADC_CH       0u
#define RZC_MOTOR_TEMP_ADC_GROUP       1u
#define RZC_MOTOR_TEMP_ADC_CH          0u
#define RZC_BATTERY_VOLTAGE_ADC_GROUP  2u
#define RZC_BATTERY_VOLTAGE_ADC_CH     0u

/* ====================================================================
 * Motor Constants (ASIL D — BTS7960 H-Bridge)
 * ==================================================================== */

/** TIM1 base frequency for 20kHz switching */
#define RZC_MOTOR_PWM_FREQ_HZ     20000u

/** Maximum duty cycle (95% cap — BTS7960 bootstrap capacitor) */
#define RZC_MOTOR_MAX_DUTY_PCT       95u

/** Dead-time between direction change (10 microseconds) */
#define RZC_MOTOR_DEADTIME_US        10u

/** Mode-based torque limits (percent of max torque) */
#define RZC_MOTOR_LIMIT_RUN         100u
#define RZC_MOTOR_LIMIT_DEGRADED     75u
#define RZC_MOTOR_LIMIT_LIMP         30u
#define RZC_MOTOR_LIMIT_SAFE_STOP     0u

/** Command timeout: 100ms with no valid torque command */
#define RZC_MOTOR_CMD_TIMEOUT_MS    100u

/** Recovery: 5 valid messages to re-enable after timeout */
#define RZC_MOTOR_CMD_RECOVERY        5u

/** PWM duty range: 0..10000 (0.01% resolution, from IoHwAb) */
#define RZC_MOTOR_PWM_SCALE       10000u

/* ====================================================================
 * Current Monitoring Constants (ASIL A — ACS723)
 * ==================================================================== */

/** Overcurrent threshold in milliamps */
#define RZC_CURRENT_OC_THRESH_MA  25000u

/** Overcurrent debounce: 10ms at 1kHz = 10 consecutive samples */
#define RZC_CURRENT_OC_DEBOUNCE      10u

/** Recovery time: 500ms below threshold */
#define RZC_CURRENT_RECOVERY_MS     500u

/** Zero-cal: 64 samples averaged at startup */
#define RZC_CURRENT_ZEROCAL_SAMPLES  64u

/** Zero-cal expected center (12-bit ADC midpoint, assumes 3.3V supply: Vcc/2 = 1.65V) */
#define RZC_CURRENT_ZEROCAL_CENTER 2048u

/** Zero-cal acceptable offset range from center */
#define RZC_CURRENT_ZEROCAL_RANGE   200u

/** ACS723 sensitivity: 100mV/A for 20A variant (ACS723LLCTR-20AB-T) */
#define RZC_CURRENT_SENSITIVITY_UV  100u

/** Moving average window size */
#define RZC_CURRENT_AVG_WINDOW        4u

/* ====================================================================
 * Temperature Monitoring Constants (ASIL A — NTC Steinhart-Hart)
 * ==================================================================== */

/** Steinhart-Hart reference temperature (25C in Kelvin) */
#define RZC_TEMP_T0_K             29815u   /* 298.15 K * 100 (scaled) */

/** NTC reference resistance at T0 in ohms */
#define RZC_TEMP_R0_OHM           10000u

/** NTC B-coefficient */
#define RZC_TEMP_B_COEFF           3950u

/** Temperature derating curve thresholds (degrees C) */
#define RZC_TEMP_DERATE_NONE_C       60u   /* Below: 100% power */
#define RZC_TEMP_DERATE_75_C         80u   /* 60-79C: 75% power */
#define RZC_TEMP_DERATE_50_C        100u   /* 80-99C: 50% power */
                                           /* >= 100C: 0% power (shutdown) */

/** Derating power percentages */
#define RZC_TEMP_DERATE_100_PCT     100u
#define RZC_TEMP_DERATE_75_PCT       75u
#define RZC_TEMP_DERATE_50_PCT       50u
#define RZC_TEMP_DERATE_0_PCT         0u

/** Recovery hysteresis (10C lower to recover to higher power level) */
#define RZC_TEMP_HYSTERESIS_C        10u

/** Plausible NTC temperature range (deci-degrees C) */
#define RZC_TEMP_MIN_DDC           (-300)  /* -30.0C */
#define RZC_TEMP_MAX_DDC           1500    /* 150.0C */

/* ====================================================================
 * Encoder Constants (ASIL C — Quadrature Encoder TIM4)
 * ==================================================================== */

/** Encoder pulses per revolution */
#define RZC_ENCODER_PPR             360u

/** Stall detection: PWM > 10% but zero speed for 500ms */
#define RZC_ENCODER_STALL_TIMEOUT_MS 500u

/** Stall check period: 10ms per check = 50 checks for 500ms */
#define RZC_ENCODER_STALL_CHECKS     50u

/** Direction mismatch: commanded vs encoder for 50ms */
#define RZC_ENCODER_DIR_MISMATCH_MS  50u

/** Direction mismatch check count at 10ms period */
#define RZC_ENCODER_DIR_CHECKS        5u

/** Grace period after direction change (stall) */
#define RZC_ENCODER_STALL_GRACE_MS  200u

/** Grace period after direction change (direction plausibility) */
#define RZC_ENCODER_DIR_GRACE_MS    100u

/** Minimum PWM % to trigger stall detection */
#define RZC_ENCODER_STALL_MIN_PWM    10u

/* ====================================================================
 * Battery Monitoring Constants (QM — Voltage Divider)
 * ==================================================================== */

/** Battery disable low threshold (mV) */
#define RZC_BATT_DISABLE_LOW_MV    8000u

/** Battery warning low threshold (mV) */
#define RZC_BATT_WARN_LOW_MV      10500u

/** Battery warning high threshold (mV) */
#define RZC_BATT_WARN_HIGH_MV     15000u

/** Battery disable high threshold (mV) */
#define RZC_BATT_DISABLE_HIGH_MV  17000u

/** Hysteresis band (mV) for recovery */
#define RZC_BATT_HYSTERESIS_MV      500u

/** Voltage divider ratio: (R_H + R_L) / R_L = (47k + 10k) / 10k = 5.7 */
#define RZC_BATT_DIVIDER_RH       47000u   /* High-side resistor ohms */
#define RZC_BATT_DIVIDER_RL       10000u   /* Low-side resistor ohms */

/** Nominal battery voltage (mV) — used to seed the average buffer so
 *  the first few samples don't produce a false undervoltage fault. */
#define RZC_BATT_NOMINAL_MV       12600u

/** Moving average window */
#define RZC_BATT_AVG_WINDOW           4u

/** Battery status codes */
#define RZC_BATT_STATUS_DISABLE_LOW   0u
#define RZC_BATT_STATUS_WARN_LOW      1u
#define RZC_BATT_STATUS_NORMAL        2u
#define RZC_BATT_STATUS_WARN_HIGH     3u
#define RZC_BATT_STATUS_DISABLE_HIGH  4u

/* ====================================================================
 * Heartbeat Constants
 * ==================================================================== */

#define RZC_HB_TX_PERIOD_MS        50u     /* TX every 50ms */
#define RZC_HB_TIMEOUT_MS         150u     /* 3x TX period */
#define RZC_HB_MAX_MISS             3u     /* Consecutive misses before timeout */
#define RZC_HB_ALIVE_MAX           15u     /* 4-bit alive counter wraps at 15 */

#define RZC_ECU_ID               0x03u     /* RZC ECU identifier */

/* Vehicle state enums (RZC_STATE_*) are in generated Rzc_Cfg.h via sidecar */

/* ====================================================================
 * Self-Test Constants
 * ==================================================================== */

#define RZC_SELF_TEST_PASS          1u
#define RZC_SELF_TEST_FAIL          0u

/** Number of self-test items (8 for RZC) */
#define RZC_SELF_TEST_ITEMS         8u

/* ====================================================================
 * Fault Bitmask Positions
 * ==================================================================== */

#define RZC_FAULT_NONE           0x00u
#define RZC_FAULT_OVERCURRENT    0x01u
#define RZC_FAULT_OVERTEMP       0x02u
#define RZC_FAULT_DIRECTION      0x04u
#define RZC_FAULT_CAN            0x08u
#define RZC_FAULT_WATCHDOG       0x10u
#define RZC_FAULT_SELF_TEST      0x20u
#define RZC_FAULT_BATTERY        0x40u
#define RZC_FAULT_STALL          0x80u

/* ====================================================================
 * Motor Fault Enum
 * ==================================================================== */

#define RZC_MOTOR_NO_FAULT          0u
#define RZC_MOTOR_SHOOT_THROUGH     1u
#define RZC_MOTOR_CMD_TIMEOUT       2u
#define RZC_MOTOR_OVERCURRENT       3u
#define RZC_MOTOR_OVERTEMP          4u
#define RZC_MOTOR_STALL             5u
#define RZC_MOTOR_DIRECTION         6u

/* ====================================================================
 * Motor Direction Enum (matches IoHwAb)
 * ==================================================================== */

#define RZC_DIR_FORWARD             0u
#define RZC_DIR_REVERSE             1u
#define RZC_DIR_STOP                2u

/* ====================================================================
 * Safety WDI Pin
 * ==================================================================== */

#define RZC_SAFETY_WDI_CHANNEL      4u     /* PB4 — TPS3823 WDI pin */

/* ====================================================================
 * BTS7960 Enable Pins (independent disable path)
 * ==================================================================== */

#define RZC_MOTOR_R_EN_CHANNEL      5u     /* R_EN GPIO */
#define RZC_MOTOR_L_EN_CHANNEL      6u     /* L_EN GPIO */

/* ====================================================================
 * CAN Bus Loss Constants
 * ==================================================================== */

#define RZC_CAN_SILENCE_TIMEOUT_MS 200u    /* 200ms CAN silence -> disable */
#define RZC_CAN_ERR_WARN_TIMEOUT_MS 500u   /* Error warning > 500ms -> disable */

/* ====================================================================
 * Compatibility Aliases — short names used in SWC code → generated names
 * ==================================================================== */

/* TX PDU short names (SWC source uses these) */
#define RZC_COM_TX_HEARTBEAT       RZC_COM_TX_RZC_HEARTBEAT
#define RZC_COM_TX_MOTOR_TEMP      RZC_COM_TX_MOTOR_TEMPERATURE

/* RX PDU short names */
#define RZC_COM_RX_ESTOP           RZC_COM_RX_ESTOP_BROADCAST
#define RZC_COM_RX_VEHICLE_TORQUE  RZC_COM_RX_VEHICLE_STATE  /* legacy alias */

/* Virtual sensors PDU (SIL only, not in DBC/ARXML) */
#define RZC_COM_RX_VIRT_SENSORS    28u   /* CAN 0x601 — plant-sim virtual sensors */

#endif /* RZC_APP_H */
