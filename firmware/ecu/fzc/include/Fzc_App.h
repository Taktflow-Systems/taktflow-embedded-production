/**
 * @file    Fzc_App.h
 * @brief   FZC application-specific constants — hand-authored, NOT generated
 * @date    2026-02-23
 *
 * @details Application-level constants for the Front Zone Controller:
 *          steering, brake, lidar, buzzer patterns, heartbeat, self-test,
 *          fault bitmasks, and compatibility aliases.
 *
 *          PDU IDs, signal IDs, DTC IDs, and E2E data IDs are in the
 *          generated Fzc_Cfg.h — do NOT duplicate them here.
 *
 * @safety_req SWR-FZC-001 to SWR-FZC-032
 * @traces_to  SSR-FZC-001 to SSR-FZC-024, TSR-022, TSR-030, TSR-038, TSR-046
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef FZC_APP_H
#define FZC_APP_H

/* Platform-specific constants (selected via -I path in Makefile) */
#include "Fzc_Cfg_Platform.h"

/* ====================================================================
 * RTE Signal IDs (SWC-level short names)
 * These map to internal RTE signal slots used by Rte_Read/Rte_Write.
 * ==================================================================== */

#define FZC_SIG_STEER_CMD          16u
#define FZC_SIG_STEER_ANGLE        17u
#define FZC_SIG_STEER_FAULT        18u
#define FZC_SIG_BRAKE_CMD          19u
#define FZC_SIG_BRAKE_POS          20u
#define FZC_SIG_BRAKE_FAULT        21u
#define FZC_SIG_LIDAR_DIST         22u
#define FZC_SIG_LIDAR_SIGNAL       23u
#define FZC_SIG_LIDAR_ZONE         24u
#define FZC_SIG_LIDAR_FAULT        25u
#define FZC_SIG_VEHICLE_STATE      26u
#define FZC_SIG_ESTOP_ACTIVE       27u
#define FZC_SIG_BUZZER_PATTERN     28u
#define FZC_SIG_MOTOR_CUTOFF       29u
#define FZC_SIG_FAULT_MASK         30u
#define FZC_SIG_STEER_PWM_DISABLE  31u
#define FZC_SIG_BRAKE_PWM_DISABLE  32u
#define FZC_SIG_SELF_TEST_RESULT   33u
#define FZC_SIG_HEARTBEAT_ALIVE    34u
#define FZC_SIG_SAFETY_STATUS      35u

/* ====================================================================
 * Com TX Signal IDs (index into Com signal config table)
 * NOTE: These are SIGNAL IDs, not PDU IDs. Com_SendSignal() takes a
 * signal ID.  The PDU IDs are in generated Fzc_Cfg.h.
 * ==================================================================== */

#define FZC_COM_SIG_TX_BRAKE_FAULT     FZC_COM_SIG_BRAKE_FAULT_FAULT_TYPE       /* 25u */
#define FZC_COM_SIG_TX_MOTOR_CUTOFF    FZC_COM_SIG_MOTOR_CUTOFF_REQ_REQUEST_TYPE /* 31u */

/* ====================================================================
 * Com Signal IDs for Virtual Sensors (RX from plant-sim, SIL only)
 * ==================================================================== */

/* Use generated signal IDs from Fzc_Cfg.h (DBC→ARXML→codegen) */
#define FZC_COM_SIG_RX_VIRT_STEER_ANGLE   FZC_COM_SIG_FZC_VIRTUAL_SENSORS_VSENSOR_STEER_ANGLE_RAW
#define FZC_COM_SIG_RX_VIRT_BRAKE_POS     FZC_COM_SIG_FZC_VIRTUAL_SENSORS_VSENSOR_BRAKE_POS_ADC
#define FZC_COM_SIG_RX_VIRT_BRAKE_CURRENT FZC_COM_SIG_FZC_VIRTUAL_SENSORS_VSENSOR_BRAKE_CURRENT

/* ADC group/channel for brake position injection (SIL) */
#define FZC_BRAKE_ADC_GROUP    3u   /* Must match iohwab_config.BrakePositionAdcGroup */
#define FZC_BRAKE_ADC_CHANNEL  0u   /* Channel 0 within the group */

/* ====================================================================
 * Steering Constants (ASIL D)
 * ==================================================================== */

#define FZC_STEER_PLAUS_THRESHOLD_DEG    5u    /* 5 degree command vs feedback */
/* FZC_STEER_PLAUS_DEBOUNCE defined in Fzc_Cfg_Platform.h */
#define FZC_STEER_RATE_LIMIT_DEG_10MS    3u    /* 0.3 deg per 10ms = 3 tenths */
#define FZC_STEER_CMD_TIMEOUT_MS       100u    /* 100ms command timeout */
#define FZC_STEER_RTC_RATE_DEG_S        30u    /* 30 deg/s return-to-center rate */
#define FZC_STEER_ANGLE_MIN           (-45)    /* Minimum steering angle (degrees) */
#define FZC_STEER_ANGLE_MAX             45     /* Maximum steering angle (degrees) */
#define FZC_STEER_PWM_CENTER_US       1500u    /* 1.5ms center = neutral */
#define FZC_STEER_PWM_MIN_US          1000u    /* 1.0ms full left */
#define FZC_STEER_PWM_MAX_US          2000u    /* 2.0ms full right */
#define FZC_STEER_LATCH_CLEAR_CYCLES    50u    /* Fault-free cycles to clear latch */

/* ====================================================================
 * Brake Constants (ASIL D)
 * ==================================================================== */

#define FZC_BRAKE_AUTO_TIMEOUT_MS      100u    /* 100ms auto-brake timeout */
#define FZC_BRAKE_PWM_FAULT_THRESH       2u    /* 2% PWM fault threshold */
/* FZC_BRAKE_FAULT_DEBOUNCE defined in Fzc_Cfg_Platform.h */
#define FZC_BRAKE_CUTOFF_REPEAT_MS      10u    /* Motor cutoff CAN repeat period */
#define FZC_BRAKE_CUTOFF_REPEAT_COUNT   10u    /* Number of cutoff CAN repeats */
#define FZC_BRAKE_PWM_MIN                0u    /* 0% = no brake */
#define FZC_BRAKE_PWM_MAX              100u    /* 100% = full brake */
#define FZC_BRAKE_LATCH_CLEAR_CYCLES    50u    /* Fault-free cycles to clear latch */

/* Brake oscillation detection (ASIL D — command plausibility) */
#define FZC_BRAKE_OSCILLATION_DELTA_THRESH  30u   /* 30% min jump per cycle */
#define FZC_BRAKE_OSCILLATION_DEBOUNCE       4u   /* 4 consecutive = fault (40ms) */

/* ====================================================================
 * Lidar Constants (ASIL C)
 * ==================================================================== */

#define FZC_LIDAR_WARN_CM              100u    /* Warning zone: <= 100cm */
#define FZC_LIDAR_BRAKE_CM              50u    /* Braking zone: <= 50cm */
#define FZC_LIDAR_EMERGENCY_CM          20u    /* Emergency zone: <= 20cm */
#define FZC_LIDAR_TIMEOUT_MS           100u    /* 100ms frame timeout */
#define FZC_LIDAR_STUCK_CYCLES          50u    /* 50 identical readings = stuck */
#define FZC_LIDAR_RANGE_MIN_CM           2u    /* TFMini-S minimum range */
#define FZC_LIDAR_RANGE_MAX_CM        1200u    /* TFMini-S maximum range */
#define FZC_LIDAR_SIGNAL_MIN           100u    /* Minimum signal strength */
#define FZC_LIDAR_FRAME_SIZE             9u    /* TFMini-S frame size bytes */
#define FZC_LIDAR_HEADER_BYTE         0x59u    /* TFMini-S frame header */
#define FZC_LIDAR_DEGRADE_CYCLES       200u    /* Persistent fault cycles before degradation request */

/* Lidar zone enum */
#define FZC_LIDAR_ZONE_CLEAR            0u
#define FZC_LIDAR_ZONE_WARNING          1u
#define FZC_LIDAR_ZONE_BRAKING          2u
#define FZC_LIDAR_ZONE_EMERGENCY        3u
#define FZC_LIDAR_ZONE_FAULT            4u

/* ====================================================================
 * Buzzer Pattern Enum
 * ==================================================================== */

#define FZC_BUZZER_SILENT               0u
#define FZC_BUZZER_SINGLE_BEEP          1u
#define FZC_BUZZER_SLOW_REPEAT          2u
#define FZC_BUZZER_FAST_REPEAT          3u
#define FZC_BUZZER_CONTINUOUS           4u
#define FZC_BUZZER_PATTERN_COUNT        5u

/* ====================================================================
 * Heartbeat Constants
 * ==================================================================== */

#define FZC_HB_TX_PERIOD_MS            50u    /* TX every 50ms */
#define FZC_HB_TIMEOUT_MS             150u    /* 3x TX period */
#define FZC_HB_MAX_MISS                 3u    /* Consecutive misses before timeout */
#define FZC_HB_ALIVE_MAX               15u    /* 4-bit alive counter wraps at 15 */

#define FZC_ECU_ID                   0x02u    /* FZC ECU identifier */

/* Vehicle state enums (FZC_STATE_*) are in generated Fzc_Cfg.h via sidecar */

/* ====================================================================
 * Self-Test Constants
 * ==================================================================== */

#define FZC_SELF_TEST_PASS              1u
#define FZC_SELF_TEST_FAIL              0u

/* Number of self-test items */
#define FZC_SELF_TEST_ITEMS             7u

/* ====================================================================
 * FZC Safety Startup Grace Period
 * Suppresses motor cutoff assertion for N cycles after boot to absorb
 * startup transients (SC E-Stop, lidar timeout, brake stabilization).
 * Bare metal: 0 (transparent).  Platform-equivalent code path.
 * ==================================================================== */

/* FZC_POST_INIT_GRACE_CYCLES defined in Fzc_Cfg_Platform.h */

/* ====================================================================
 * Fault Bitmask Positions
 * ==================================================================== */

#define FZC_FAULT_NONE               0x00u
#define FZC_FAULT_STEER            0x01u
#define FZC_FAULT_BRAKE            0x02u
#define FZC_FAULT_LIDAR            0x04u
#define FZC_FAULT_CAN              0x08u
#define FZC_FAULT_WATCHDOG         0x10u
#define FZC_FAULT_SELF_TEST        0x20u
#define FZC_FAULT_CAN_BUS_OFF     0x0100u  /* bit 8: transport-layer fault, outside 8-bit payload range */

/* ====================================================================
 * Steering Fault Enum
 * ==================================================================== */

#define FZC_STEER_NO_FAULT              0u
#define FZC_STEER_PLAUSIBILITY          1u
#define FZC_STEER_OUT_OF_RANGE          2u
#define FZC_STEER_RATE_EXCEEDED         3u
#define FZC_STEER_CMD_TIMEOUT           4u
#define FZC_STEER_SPI_FAIL              5u

/* ====================================================================
 * Brake Fault Enum
 * ==================================================================== */

#define FZC_BRAKE_NO_FAULT              0u
#define FZC_BRAKE_PWM_DEVIATION         1u
#define FZC_BRAKE_CMD_TIMEOUT           2u
#define FZC_BRAKE_LATCHED               3u
#define FZC_BRAKE_CMD_OSCILLATION       4u

/* ====================================================================
 * Compatibility Aliases — short names used in SWC code → generated names
 * ==================================================================== */

/* TX PDU short names (SWC source uses these) */
#define FZC_COM_TX_HEARTBEAT       FZC_COM_TX_FZC_HEARTBEAT
#define FZC_COM_TX_STEER_STATUS    FZC_COM_TX_STEERING_STATUS
#define FZC_COM_TX_MOTOR_CUTOFF    FZC_COM_TX_MOTOR_CUTOFF_REQ
#define FZC_COM_TX_LIDAR           FZC_COM_TX_LIDAR_DISTANCE

/* RX PDU short names */
#define FZC_COM_RX_ESTOP           FZC_COM_RX_ESTOP_BROADCAST
#define FZC_COM_RX_STEER_CMD       FZC_COM_RX_STEER_COMMAND
#define FZC_COM_RX_BRAKE_CMD       FZC_COM_RX_BRAKE_COMMAND

/* Virtual sensors PDU — now in DBC/ARXML, use generated ID */
#define FZC_COM_RX_VIRT_SENSORS    FZC_COM_RX_FZC_VIRTUAL_SENSORS

/* Det module and API IDs are in Det_ErrIds.h */

#endif /* FZC_APP_H */
