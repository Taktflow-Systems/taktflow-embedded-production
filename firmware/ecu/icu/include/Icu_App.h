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

/* Signal IDs are generated in Icu_Cfg.h — do not duplicate here */

/* --- TX PDU aliases (icu_main.c uses short names) --- */
#define ICU_COM_TX_HEARTBEAT      ICU_COM_TX_ICU_HEARTBEAT

/* --- RX PDU aliases --- */
#define ICU_COM_RX_ESTOP          ICU_COM_RX_ESTOP_BROADCAST
#define ICU_COM_RX_HB_CVC         ICU_COM_RX_CVC_HEARTBEAT
#define ICU_COM_RX_HB_FZC         ICU_COM_RX_FZC_HEARTBEAT
#define ICU_COM_RX_HB_RZC         ICU_COM_RX_RZC_HEARTBEAT
#define ICU_COM_RX_TORQUE_REQ     ICU_COM_RX_TORQUE_REQUEST
#define ICU_COM_RX_MOTOR_TEMP     ICU_COM_RX_MOTOR_TEMPERATURE
#define ICU_COM_RX_BATTERY        ICU_COM_RX_BATTERY_STATUS
#define ICU_COM_RX_INDICATOR      ICU_COM_RX_INDICATOR_STATE
#define ICU_COM_RX_DOOR_LOCK      ICU_COM_RX_DOOR_LOCK_STATUS
#define ICU_COM_RX_DTC_BCAST      ICU_COM_RX_DTC_BROADCAST

/* --- ncurses color pair indices --- */
#define ICU_COLOR_GREEN            1
#define ICU_COLOR_YELLOW           2
#define ICU_COLOR_ORANGE           3
#define ICU_COLOR_RED              4
#define ICU_COLOR_WHITE            5

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
