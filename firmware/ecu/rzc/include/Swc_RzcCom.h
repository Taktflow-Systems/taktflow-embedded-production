/**
 * @file    Swc_RzcCom.h
 * @brief   RZC CAN communication SWC -- E2E protection, message RX/TX tables
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-019, SWR-RZC-020, SWR-RZC-026, SWR-RZC-027
 * @traces_to  SSR-RZC-019, SSR-RZC-020, SSR-RZC-026, SSR-RZC-027
 *
 * @details  Implements the RZC CAN communication SWC:
 *           - E2E transmit protection (CRC-8 0x1D, RZC-specific Data IDs,
 *             16-entry alive counter array) per SWR-RZC-019
 *           - E2E receive verification (CRC check + alive counter, 3-fail
 *             safe default = zero torque for torque cmd) per SWR-RZC-020
 *           - CAN message reception table per SWR-RZC-026:
 *               0x001 E-stop -> disable motor
 *               0x100 vehicle state + torque -> zero torque after 100ms
 *           - CAN message transmission table per SWR-RZC-027:
 *               0x012 heartbeat every 50ms
 *               0x301 motor status every 10ms (current/temp/speed/battery)
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_RZC_COM_H
#define SWC_RZC_COM_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the Swc_RzcCom module
 * @note   Resets alive counters, failure counters, timeout state.
 */
void Swc_RzcCom_Init(void);

/**
 * @brief  Apply E2E protection to a TX buffer
 * @param  pduId   PDU index (selects Data ID and alive counter slot)
 * @param  data    Pointer to 8-byte CAN payload (CRC written to byte 0)
 * @param  length  Payload length in bytes
 * @return E_OK on success, E_NOT_OK on invalid parameters
 *
 * @safety_req SWR-RZC-019
 */
Std_ReturnType Swc_RzcCom_E2eProtect(uint8 pduId, uint8 *data, uint8 length);

/**
 * @brief  Verify E2E protection on an RX buffer
 * @param  pduId   PDU index (selects Data ID and alive counter slot)
 * @param  data    Pointer to 8-byte CAN payload (CRC expected in byte 0)
 * @param  length  Payload length in bytes
 * @return E_OK if valid, E_NOT_OK if CRC or alive counter check fails
 *
 * @safety_req SWR-RZC-020
 */
Std_ReturnType Swc_RzcCom_E2eCheck(uint8 pduId, const uint8 *data, uint8 length);

/**
 * @brief  Process received CAN messages and update RTE signals
 * @note   Called every 10ms. Handles 0x001 E-stop and 0x100 vehicle
 *         state + torque. Enforces 100ms torque command timeout.
 *
 * @safety_req SWR-RZC-026
 */
void Swc_RzcCom_Receive(void);

/**
 * @brief  Schedule and transmit CAN messages per TX table
 * @note   Called every 10ms. Manages 50ms heartbeat and 10ms motor
 *         status transmission cycles.
 *
 * @safety_req SWR-RZC-027
 */
void Swc_RzcCom_TransmitSchedule(void);

/**
 * @brief  E2E RX validation callback for CanIf
 *
 * Only E2E-checks PDUs with configured Data IDs (E-stop, Vehicle_State).
 * Virtual sensors bypass E2E.
 *
 * @param  PduId   Upper-layer PDU ID
 * @param  Data    Pointer to received data
 * @param  Length  Data length
 * @return E_OK to accept, E_NOT_OK to drop
 *
 * @safety_req SWR-RZC-020
 */
Std_ReturnType Rzc_E2eRxCheck(uint8 PduId, const uint8* Data, uint8 Length);

#endif /* SWC_RZC_COM_H */
