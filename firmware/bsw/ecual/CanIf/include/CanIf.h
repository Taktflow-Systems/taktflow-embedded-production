/**
 * @file    CanIf.h
 * @brief   CAN Interface — hardware-independent CAN PDU routing
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-011: PDU routing
 * @safety_req SWR-BSW-012: Controller mode management
 * @traces_to  TSR-022, TSR-023, TSR-024, TSR-038
 *
 * @standard AUTOSAR_SWS_CANInterface, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef CANIF_H
#define CANIF_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/* Re-use CAN driver types needed by CanIf */
#include "Can.h"

/* ---- Configuration Types ---- */

/** RX PDU config: maps CAN ID to upper-layer PDU ID */
typedef struct {
    Can_IdType  CanId;          /**< CAN message ID to match         */
    PduIdType   UpperPduId;     /**< PduR PDU ID for this message    */
    uint8       Dlc;            /**< Expected DLC                    */
    boolean     IsExtended;     /**< Extended CAN ID flag            */
} CanIf_RxPduConfigType;

/** TX PDU config: maps upper-layer PDU ID to CAN ID + Hth */
typedef struct {
    Can_IdType  CanId;          /**< CAN ID for transmission         */
    PduIdType   UpperPduId;     /**< Upper-layer PDU ID (index)      */
    uint8       Dlc;            /**< DLC                             */
    uint8       Hth;            /**< Hardware transmit handle        */
} CanIf_TxPduConfigType;

/** Optional E2E RX validation callback.
 *  Returns E_OK to accept frame, E_NOT_OK to drop it. */
typedef Std_ReturnType (*CanIf_E2eRxCheckType)(uint8 PduId, const uint8* Data, uint8 Length);

/** CanIf module configuration */
typedef struct {
    const CanIf_TxPduConfigType*  txPduConfig;
    uint8                         txPduCount;
    const CanIf_RxPduConfigType*  rxPduConfig;
    uint8                         rxPduCount;
    CanIf_E2eRxCheckType          e2eRxCheck;   /**< Optional E2E RX callback (NULL = no check) */
} CanIf_ConfigType;

/* ---- Upper-layer callbacks (provided by PduR) ---- */
extern void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/* ---- API Functions ---- */

/**
 * @brief Initialize CanIf with routing configuration
 * @param ConfigPtr  Routing tables (must not be NULL)
 */
void CanIf_Init(const CanIf_ConfigType* ConfigPtr);

/**
 * @brief Transmit a PDU via CAN
 * @param TxPduId     Logical TX PDU ID (index into TX config table)
 * @param PduInfoPtr  Data to transmit
 * @return E_OK on success, E_NOT_OK on error
 */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/**
 * @brief RX indication from CAN driver
 *
 * Called by Can_MainFunction_Read when a message is received.
 * Maps CAN ID to PDU ID and forwards to PduR.
 *
 * @param CanId   Received CAN identifier
 * @param SduPtr  Pointer to received data
 * @param Dlc     Data length
 */
void CanIf_RxIndication(Can_IdType CanId, const uint8* SduPtr, uint8 Dlc);

#endif /* CANIF_H */
