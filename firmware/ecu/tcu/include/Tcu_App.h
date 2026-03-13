/**
 * @file    Tcu_App.h
 * @brief   TCU application-specific signal aliases and thresholds
 *
 * Included by Tcu_Cfg.h via __has_include hook. These defines bridge
 * the ARXML-generated granular signal IDs to the simplified IDs used
 * by SWC application code (Swc_DtcStore, etc.).
 */
#ifndef TCU_APP_H
#define TCU_APP_H

/* --- Simplified signal IDs for SWC code --- */
#define TCU_SIG_VEHICLE_SPEED     16u
#define TCU_SIG_MOTOR_TEMP        17u
#define TCU_SIG_BATTERY_VOLTAGE   18u
#define TCU_SIG_MOTOR_CURRENT     19u
#define TCU_SIG_TORQUE_PCT        20u
#define TCU_SIG_MOTOR_RPM         21u
#define TCU_SIG_DTC_BROADCAST     22u

/* --- DTC aging threshold (override codegen DTC event ID with actual cycle count) --- */
#undef  TCU_DTC_AGING_CLEAR_CYCLES
#define TCU_DTC_AGING_CLEAR_CYCLES  40u

#endif /* TCU_APP_H */
