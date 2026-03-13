/**
 * @file    CanTp.h
 * @brief   CAN Transport Protocol — ISO 15765-2 (ISO-TP)
 * @date    2026-03-03
 *
 * @details Implements segmented CAN transfer for payloads > 7 bytes.
 *          Supports Single Frame (SF), First Frame (FF), Consecutive
 *          Frame (CF), and Flow Control (FC) as defined in ISO 15765-2.
 *
 * @safety_req SWR-BSW-042: Multi-frame CAN transport for UDS diagnostics
 * @traces_to  TSR-038, TSR-039, TSR-040
 *
 * @standard ISO 15765-2:2016, AUTOSAR_SWS_CANTransportLayer
 * @copyright Taktflow Systems 2026
 */
#ifndef CANTP_H
#define CANTP_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ---- Configuration Constants ---- */

/** Maximum TP payload size (bytes) */
#define CANTP_MAX_PAYLOAD       128u

/** N_Cr timeout — time to wait for next CF after FC sent (ms) */
#define CANTP_N_CR_TIMEOUT_MS   1000u

/** N_Bs timeout — time to wait for FC from receiver (ms) */
#define CANTP_N_BS_TIMEOUT_MS   1000u

/** STmin — minimum separation time between CFs (ms) */
#define CANTP_STMIN_MS          10u

/** Block Size — 0 = no flow control pause (continuous) */
#define CANTP_BLOCK_SIZE        0u

/** CAN frame data length (CAN 2.0B) */
#define CANTP_CAN_DL            8u

/** MainFunction call period (ms) */
#define CANTP_MAIN_CYCLE_MS     10u

/* ---- ISO 15765-2 Frame Type Nibbles ---- */

#define CANTP_N_PCI_SF          0x00u   /**< Single Frame       */
#define CANTP_N_PCI_FF          0x10u   /**< First Frame        */
#define CANTP_N_PCI_CF          0x20u   /**< Consecutive Frame  */
#define CANTP_N_PCI_FC          0x30u   /**< Flow Control       */
#define CANTP_N_PCI_MASK        0xF0u   /**< Upper nibble mask  */

/* ---- Flow Control Status Bytes ---- */

#define CANTP_FC_CTS            0x00u   /**< Continue To Send   */
#define CANTP_FC_WAIT           0x01u   /**< Wait               */
#define CANTP_FC_OVERFLOW       0x02u   /**< Overflow / Abort   */

/* ---- DET API IDs ---- */

#define CANTP_API_INIT          0x00u
#define CANTP_API_TRANSMIT      0x01u
#define CANTP_API_RXINDICATION  0x02u
#define CANTP_API_MAINFUNCTION  0x03u

/* ---- Types ---- */

/** CanTp channel state */
typedef enum {
    CANTP_STATE_IDLE        = 0u,
    CANTP_STATE_RX_WAIT_CF  = 1u,   /**< Receiving multi-frame: waiting for CF */
    CANTP_STATE_TX_WAIT_FC  = 2u,   /**< Transmitting multi-frame: waiting for FC */
    CANTP_STATE_TX_CF       = 3u    /**< Transmitting multi-frame: sending CFs */
} CanTp_StateType;

/** CanTp channel data — one RX + one TX channel */
typedef struct {
    /* RX state */
    CanTp_StateType  rxState;
    uint8            rxBuf[CANTP_MAX_PAYLOAD];  /**< Reassembly buffer    */
    PduLengthType    rxTotal;                   /**< Total expected bytes */
    PduLengthType    rxCount;                   /**< Bytes received so far*/
    uint8            rxSn;                      /**< Expected sequence #  */
    uint16           rxTimer;                   /**< N_Cr timeout counter */

    /* TX state */
    CanTp_StateType  txState;
    uint8            txBuf[CANTP_MAX_PAYLOAD];  /**< Segmentation buffer  */
    PduLengthType    txTotal;                   /**< Total bytes to send  */
    PduLengthType    txOffset;                  /**< Bytes sent so far    */
    uint8            txSn;                      /**< Sequence number      */
    uint16           txTimer;                   /**< N_Bs timeout counter */
    uint16           txStminTimer;              /**< STmin delay counter  */
    PduIdType        txPduId;                   /**< Lower-layer TX PDU   */
} CanTp_ChannelType;

/** CanTp configuration */
typedef struct {
    PduIdType   rxPduId;        /**< RX PDU ID from PduR        */
    PduIdType   txPduId;        /**< TX PDU ID to CanIf          */
    PduIdType   fcTxPduId;      /**< FC TX PDU ID (response)     */
    PduIdType   upperRxPduId;   /**< Upper-layer RX PDU ID       */
} CanTp_ConfigType;

/* ---- Upper-layer Callbacks ---- */

/**
 * @brief Upper-layer RX indication (called when a complete message is reassembled)
 * @param RxPduId    Upper-layer PDU ID
 * @param PduInfoPtr Reassembled payload
 * @param Result     NTFRSLT_OK or error
 */
extern void Dcm_TpRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr,
                                NotifResultType Result);

/**
 * @brief Lower-layer transmit — send a single CAN frame
 * @param TxPduId    TX PDU ID for CanIf
 * @param PduInfoPtr Frame data (max 8 bytes)
 * @return E_OK or E_NOT_OK
 */
extern Std_ReturnType PduR_CanTpTransmit(PduIdType TxPduId,
                                          const PduInfoType* PduInfoPtr);

/* ---- API Functions ---- */

/**
 * @brief  Initialize CanTp module
 * @param  ConfigPtr  Channel configuration (must not be NULL)
 */
void CanTp_Init(const CanTp_ConfigType* ConfigPtr);

/**
 * @brief  Cyclic main function — manages timers, sends pending CFs
 * @note   Call every CANTP_MAIN_CYCLE_MS (10 ms)
 */
void CanTp_MainFunction(void);

/**
 * @brief  Receive indication from PduR — handles SF/FF/CF/FC frames
 * @param  RxPduId     Received PDU ID
 * @param  PduInfoPtr  Received CAN frame data (up to 8 bytes)
 */
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/**
 * @brief  Request transmission of a TP message (called by upper layer)
 * @param  TxPduId     TX PDU ID
 * @param  PduInfoPtr  Complete payload to segment and transmit
 * @return E_OK if accepted, E_NOT_OK if busy or invalid
 */
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/**
 * @brief  Get current RX channel state (for testing/diagnostics)
 * @return Current CanTp_StateType of the RX channel
 */
CanTp_StateType CanTp_GetRxState(void);

/**
 * @brief  Get current TX channel state (for testing/diagnostics)
 * @return Current CanTp_StateType of the TX channel
 */
CanTp_StateType CanTp_GetTxState(void);

#endif /* CANTP_H */
