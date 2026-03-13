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

/* --- TX PDU aliases (tcu_main.c uses short names) --- */
#define TCU_COM_TX_HEARTBEAT      TCU_COM_TX_TCU_HEARTBEAT
#define TCU_COM_TX_UDS_RSP        TCU_COM_TX_UDS_RESP_TCU

/* --- RX PDU aliases --- */
#define TCU_COM_RX_UDS_FUNC       TCU_COM_RX_UDS_FUNC_REQUEST
#define TCU_COM_RX_UDS_PHYS       TCU_COM_RX_UDS_PHYS_REQ_TCU
#define TCU_COM_RX_MOTOR_TEMP     TCU_COM_RX_MOTOR_TEMPERATURE
#define TCU_COM_RX_BATTERY        TCU_COM_RX_BATTERY_STATUS
#define TCU_COM_RX_DTC_BCAST      TCU_COM_RX_DTC_BROADCAST
#define TCU_COM_RX_HB_CVC         TCU_COM_RX_CVC_HEARTBEAT
#define TCU_COM_RX_HB_FZC         TCU_COM_RX_FZC_HEARTBEAT
#define TCU_COM_RX_HB_RZC         TCU_COM_RX_RZC_HEARTBEAT

/* --- DTC aging threshold (override codegen DTC event ID with actual cycle count) --- */
#undef  TCU_DTC_AGING_CLEAR_CYCLES
#define TCU_DTC_AGING_CLEAR_CYCLES  40u

/* --- UDS security / session constants --- */
#define TCU_UDS_SESSION_TIMEOUT_TICKS   500u
#define TCU_UDS_SECURITY_LOCKOUT_TICKS 1000u
#define TCU_UDS_MAX_SECURITY_ATTEMPTS     3u

#define TCU_SECURITY_LEVEL1_XOR  0xA5A5A5A5u
#define TCU_SECURITY_LEVEL3_XOR  0x5A5A5A5Au

/* --- VIN constants --- */
#define TCU_VIN_LENGTH             17u
#define TCU_VIN_DEFAULT            "TAKTFLOW000000001"

#endif /* TCU_APP_H */
