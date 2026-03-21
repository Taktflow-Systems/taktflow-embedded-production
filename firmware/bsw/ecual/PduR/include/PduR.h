/**
 * @file    PduR.h
 * @brief   PDU Router — routes PDUs between CanIf and Com/Dcm
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-013: PDU routing between CanIf, Com, Dcm
 * @traces_to  TSR-022, TSR-023, TSR-024
 *
 * @standard AUTOSAR_SWS_PDURouter, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef PDUR_H
#define PDUR_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ---- Configuration Types ---- */

/** Destination module for routing */
typedef enum {
    PDUR_DEST_COM   = 0u,
    PDUR_DEST_DCM   = 1u,
    PDUR_DEST_CANTP = 2u,   /**< Route through ISO-TP transport layer */
    PDUR_DEST_XCP   = 3u    /**< Route to XCP slave (measurement/calibration) */
} PduR_DestType;

/** Single routing table entry */
typedef struct {
    PduIdType       RxPduId;        /**< CanIf RX PDU ID                */
    PduR_DestType   Destination;    /**< Target module (COM or DCM)     */
    PduIdType       UpperPduId;     /**< PDU ID in target module        */
} PduR_RoutingTableType;

/** PduR module configuration */
typedef struct {
    const PduR_RoutingTableType*  routingTable;
    uint8                         routingCount;
} PduR_ConfigType;

/* ---- Upper-layer callbacks (provided by Com, Dcm, CanTp, Xcp) ---- */
extern void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
extern void Dcm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
extern void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
extern void Xcp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/* ---- Lower-layer interface (provided by CanIf) ---- */
extern Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/* ---- API Functions ---- */

/**
 * @brief Initialize PduR with routing configuration
 * @param ConfigPtr  Routing table (must not be NULL)
 */
void PduR_Init(const PduR_ConfigType* ConfigPtr);

/**
 * @brief Route a received PDU from CanIf to Com or Dcm
 * @param RxPduId     CanIf RX PDU ID
 * @param PduInfoPtr  Received data
 */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/**
 * @brief Transmit a PDU from upper layer through CanIf
 * @param TxPduId     TX PDU ID
 * @param PduInfoPtr  Data to transmit
 * @return E_OK on success, E_NOT_OK on error
 */
Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/**
 * @brief DCM-specific transmit path — routes through PduR_Transmit
 * @param TxPduId     TX PDU ID
 * @param PduInfoPtr  Data to transmit
 * @return E_OK on success, E_NOT_OK on error
 */
Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/**
 * @brief CanTp lower-layer transmit — routes single CAN frames through CanIf
 * @param TxPduId     TX PDU ID
 * @param PduInfoPtr  CAN frame data (max 8 bytes)
 * @return E_OK on success, E_NOT_OK on error
 */
Std_ReturnType PduR_CanTpTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

#endif /* PDUR_H */
