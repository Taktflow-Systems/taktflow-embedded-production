/**
 * @file    sc_can.h
 * @brief   DCAN1 listen-only CAN driver for Safety Controller
 * @date    2026-02-23
 *
 * @details Configures DCAN1 in silent mode (500 kbps), polls 6 mailboxes,
 *          validates E2E on received data, monitors bus silence and errors.
 *          SC never transmits on CAN.
 *
 * @safety_req SWR-SC-001, SWR-SC-002, SWR-SC-023
 * @traces_to  SSR-SC-001, SSR-SC-002
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_CAN_H
#define SC_CAN_H

#include "sc_types.h"

/**
 * @brief  Initialize DCAN1 in silent mode with 6 receive mailboxes
 */
void SC_CAN_Init(void);

/**
 * @brief  Poll all mailboxes for new data, validate E2E, notify heartbeat
 *
 * Called every 10ms from main loop.
 */
void SC_CAN_Receive(void);

/**
 * @brief  Monitor bus silence and error status
 *
 * Increments silence counter each tick. Checks DCAN error status register.
 * Called every 10ms from main loop.
 */
void SC_CAN_MonitorBus(void);

/**
 * @brief  Retrieve last valid message for a mailbox
 *
 * @param  mbIndex  Mailbox index (0-based)
 * @param  data     Output buffer (8 bytes minimum)
 * @param  dlc      Output: data length code
 * @return TRUE if valid data available, FALSE otherwise
 */
boolean SC_CAN_GetMessage(uint8 mbIndex, uint8* data, uint8* dlc);

/**
 * @brief  Check if bus has been silent beyond threshold
 * @return TRUE if no valid CAN reception for >= 200ms
 */
boolean SC_CAN_IsBusSilent(void);

/**
 * @brief  Check if DCAN1 is in bus-off state
 * @return TRUE if bus-off detected
 */
boolean SC_CAN_IsBusOff(void);

/**
 * @brief  Check if E-Stop command is active
 *
 * Parses byte 2 (EStop_Active) from validated EStop_Broadcast (CAN 0x001).
 * Updated on each valid E-Stop reception after E2E check.
 *
 * @return TRUE if last received E-Stop frame had EStop_Active != 0
 * @safety_req SWR-SC-035 (GAP-SC-001 closure)
 */
boolean SC_CAN_IsEStopActive(void);

/**
 * @brief  Transmit SC_Status frame on DCAN1 mailbox 7 (SWR-SC-029, SWR-SC-030)
 *
 * The only TX function in the SC firmware. Mailbox 7 is pre-configured
 * as TX with CAN ID 0x013. Called exclusively by sc_monitoring.c.
 *
 * @param  payload  4-byte SC_Status frame (built by SC_Monitoring_Update)
 * @param  dlc      Data length code (must be SC_RELAY_STATUS_DLC = 4)
 * @note   ASIL C — diagnostic TX path. Not on the ASIL D RX monitoring path.
 */
void SC_CAN_TransmitStatus(const uint8* payload, uint8 dlc);

#endif /* SC_CAN_H */
