/**
 * @file    Icu_App.h
 * @brief   ICU application-specific signal aliases and thresholds
 *
 * Included by Icu_Cfg.h via __has_include hook. These defines bridge
 * the ARXML-generated granular signal IDs to the simplified IDs used
 * by SWC application code (Swc_Dashboard, etc.).
 */
#ifndef ICU_APP_H
#define ICU_APP_H

/* --- Simplified signal IDs for SWC code --- */
#define ICU_SIG_MOTOR_RPM         16u
#define ICU_SIG_TORQUE_PCT        17u
#define ICU_SIG_MOTOR_TEMP        18u
#define ICU_SIG_BATTERY_VOLTAGE   19u
#define ICU_SIG_VEHICLE_STATE     20u
#define ICU_SIG_ESTOP_ACTIVE      21u
#define ICU_SIG_HEARTBEAT_CVC     22u
#define ICU_SIG_HEARTBEAT_FZC     23u
#define ICU_SIG_HEARTBEAT_RZC     24u
#define ICU_SIG_OVERCURRENT_FLAG  25u
#define ICU_SIG_LIGHT_STATUS      26u
#define ICU_SIG_INDICATOR_STATE   27u
#define ICU_SIG_DTC_BROADCAST     28u

/* --- Temperature zone thresholds (°C raw counts) --- */
#define ICU_TEMP_GREEN_MAX        59u   /* GREEN: 0-59    */
#define ICU_TEMP_YELLOW_MAX       79u   /* YELLOW: 60-79  */
#define ICU_TEMP_ORANGE_MAX       99u   /* ORANGE: 80-99  */

/* --- Battery voltage zone thresholds (mV) --- */
#define ICU_BATT_GREEN_MIN      11000u  /* GREEN: > 11000 */
#define ICU_BATT_YELLOW_MIN     10000u  /* YELLOW: 10000-11000 */

/* --- Heartbeat timeout (ticks at 50ms period) --- */
#define ICU_HB_TIMEOUT_TICKS       4u   /* 200ms */

#endif /* ICU_APP_H */
