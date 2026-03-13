/**
 * @file    Swc_CvcCom.h
 * @brief   CVC CAN communication — E2E protect/check, RX routing, TX scheduling
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-014, SWR-CVC-015, SWR-CVC-016, SWR-CVC-017
 * @traces_to  SSR-CVC-014, SSR-CVC-015, SSR-CVC-016, SSR-CVC-017,
 *             TSR-022, TSR-023, TSR-024
 *
 * @details  CVC application-layer CAN communication:
 *
 *           E2E Protect (SWR-CVC-014):
 *           - CRC-8 0x1D, alive counter, Data ID per message type
 *           - Applied to all safety-critical TX messages
 *
 *           E2E Check (SWR-CVC-015):
 *           - CRC verify, alive counter verify, 3-fail safe default
 *           - Applied to all safety-critical RX messages
 *
 *           RX Routing (SWR-CVC-016):
 *           - Table of CAN IDs from FZC/RZC with E2E check
 *
 *           TX Scheduling (SWR-CVC-017):
 *           - Table of CAN IDs with periods:
 *             0x001 (E-stop), 0x010 (heartbeat), 0x100 (vehicle state),
 *             0x200 (torque req), 0x201 (steer cmd)
 *
 * @standard AUTOSAR Com/E2E integration, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_CVCCOM_H
#define SWC_CVCCOM_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

#define CVCCOM_MAX_TX_MSGS          8u
#define CVCCOM_MAX_RX_MSGS          8u
#define CVCCOM_E2E_FAIL_THRESHOLD   3u    /**< Consecutive E2E failures for safe default */
#define CVCCOM_PDU_SIZE             8u

/* ==================================================================
 * Types
 * ================================================================== */

/** TX message schedule entry */
typedef struct {
    uint16  canId;            /**< CAN arbitration ID                  */
    uint16  periodMs;         /**< Transmission period in ms           */
    uint8   dataId;           /**< E2E Data ID                         */
    uint8   dlc;              /**< Data length code                    */
} Swc_CvcCom_TxEntryType;

/** RX message routing entry */
typedef struct {
    uint16  canId;            /**< Expected CAN arbitration ID         */
    uint8   dataId;           /**< E2E Data ID for verification        */
    uint8   dlc;              /**< Expected data length                */
} Swc_CvcCom_RxEntryType;

/** E2E check result for a single RX message */
typedef struct {
    uint8   failCount;        /**< Consecutive E2E failures            */
    uint8   useSafeDefault;   /**< TRUE if safe default active         */
} Swc_CvcCom_RxStatusType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the CVC communication SWC
 */
void Swc_CvcCom_Init(void);

/**
 * @brief  Apply E2E protection to a TX message
 * @param  txIndex   Index into TX message table
 * @param  payload   Pointer to 8-byte PDU buffer (bytes 2..7 = payload)
 * @param  length    PDU length in bytes
 * @return E_OK on success, E_NOT_OK on invalid parameters
 */
Std_ReturnType Swc_CvcCom_E2eProtect(uint8 txIndex, uint8* payload,
                                      uint8 length);

/**
 * @brief  Verify E2E protection of a received message
 * @param  rxIndex   Index into RX message table
 * @param  payload   Pointer to received 8-byte PDU buffer
 * @param  length    PDU length in bytes
 * @return E_OK if valid, E_NOT_OK if E2E check failed
 */
Std_ReturnType Swc_CvcCom_E2eCheck(uint8 rxIndex, const uint8* payload,
                                    uint8 length);

/**
 * @brief  Process a received CAN message through routing table
 * @param  canId     Received CAN ID
 * @param  payload   Received PDU data
 * @param  length    PDU length
 * @return E_OK if routed successfully, E_NOT_OK if unknown CAN ID
 */
Std_ReturnType Swc_CvcCom_Receive(uint16 canId, const uint8* payload,
                                   uint8 length);

/**
 * @brief  Check TX schedule and transmit due messages
 * @param  currentTimeMs  Current system time in milliseconds
 */
void Swc_CvcCom_TransmitSchedule(uint32 currentTimeMs);

/**
 * @brief  Get the RX status for a message index
 * @param  rxIndex   Index into RX message table
 * @param  status    Output: RX status for that message
 * @return E_OK if valid index, E_NOT_OK otherwise
 */
Std_ReturnType Swc_CvcCom_GetRxStatus(uint8 rxIndex,
                                       Swc_CvcCom_RxStatusType* status);

/**
 * @brief  Bridge Com RX fault signals to RTE for VehicleState consumption
 *
 * Reads brake_fault, motor_cutoff, FZC/RZC heartbeat alive counters from
 * Com shadow buffers and writes them to corresponding RTE signals.
 * Call periodically from the 10ms task.
 */
void Swc_CvcCom_BridgeRxToRte(void);

#endif /* SWC_CVCCOM_H */
