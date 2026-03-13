/**
 * @file    CanTp.c
 * @brief   CAN Transport Protocol — ISO 15765-2 implementation
 * @date    2026-03-03
 *
 * @details Implements ISO-TP single-frame, multi-frame RX (FF+CF+FC),
 *          and multi-frame TX (FF+CF with FC flow control).
 *          Single-channel design (one concurrent RX, one concurrent TX).
 *
 * @safety_req SWR-BSW-042
 * @traces_to  TSR-038, TSR-039, TSR-040
 *
 * @standard ISO 15765-2:2016, AUTOSAR_SWS_CANTransportLayer
 * @copyright Taktflow Systems 2026
 */
#include "CanTp.h"
#include "Det.h"

#include <string.h>  /* memcpy, memset */

/* ---- Internal State ---- */

static CanTp_ChannelType cantp_channel;
static const CanTp_ConfigType* cantp_config = NULL_PTR;
static boolean cantp_initialized = FALSE;

/* ---- Forward Declarations ---- */

static void CanTp_HandleSF(const PduInfoType* PduInfoPtr);
static void CanTp_HandleFF(const PduInfoType* PduInfoPtr);
static void CanTp_HandleCF(const PduInfoType* PduInfoPtr);
static void CanTp_HandleFC(const PduInfoType* PduInfoPtr);
static void CanTp_SendFC(uint8 fsType);
static void CanTp_SendNextCF(void);
static void CanTp_RxAbort(void);
static void CanTp_TxAbort(void);

/* ---- API Implementation ---- */

void CanTp_Init(const CanTp_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CANTP, 0u, CANTP_API_INIT, DET_E_PARAM_POINTER);
        cantp_initialized = FALSE;
        return;
    }

    cantp_config = ConfigPtr;
    (void)memset(&cantp_channel, 0, sizeof(cantp_channel));
    cantp_channel.rxState = CANTP_STATE_IDLE;
    cantp_channel.txState = CANTP_STATE_IDLE;
    cantp_initialized = TRUE;
}

void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    uint8 pciType;

    if (cantp_initialized == FALSE) {
        Det_ReportError(DET_MODULE_CANTP, 0u, CANTP_API_RXINDICATION, DET_E_UNINIT);
        return;
    }
    if (PduInfoPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CANTP, 0u, CANTP_API_RXINDICATION, DET_E_PARAM_POINTER);
        return;
    }
    if (PduInfoPtr->SduDataPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CANTP, 0u, CANTP_API_RXINDICATION, DET_E_PARAM_POINTER);
        return;
    }
    if (PduInfoPtr->SduLength == 0u) {
        return;  /* Empty frame — ignore */
    }

    (void)RxPduId;  /* Single-channel: PDU ID not used for routing */

    pciType = PduInfoPtr->SduDataPtr[0] & CANTP_N_PCI_MASK;

    switch (pciType) {
    case CANTP_N_PCI_SF:
        CanTp_HandleSF(PduInfoPtr);
        break;
    case CANTP_N_PCI_FF:
        CanTp_HandleFF(PduInfoPtr);
        break;
    case CANTP_N_PCI_CF:
        CanTp_HandleCF(PduInfoPtr);
        break;
    case CANTP_N_PCI_FC:
        CanTp_HandleFC(PduInfoPtr);
        break;
    default:
        /* Unknown PCI type — ignore */
        break;
    }
}

Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    if (cantp_initialized == FALSE) {
        Det_ReportError(DET_MODULE_CANTP, 0u, CANTP_API_TRANSMIT, DET_E_UNINIT);
        return E_NOT_OK;
    }
    if (PduInfoPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CANTP, 0u, CANTP_API_TRANSMIT, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (PduInfoPtr->SduDataPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CANTP, 0u, CANTP_API_TRANSMIT, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (PduInfoPtr->SduLength == 0u) {
        Det_ReportError(DET_MODULE_CANTP, 0u, CANTP_API_TRANSMIT, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }
    if (PduInfoPtr->SduLength > CANTP_MAX_PAYLOAD) {
        Det_ReportError(DET_MODULE_CANTP, 0u, CANTP_API_TRANSMIT, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    /* Reject if TX channel is busy */
    if (cantp_channel.txState != CANTP_STATE_IDLE) {
        return E_NOT_OK;
    }

    cantp_channel.txPduId = TxPduId;

    if (PduInfoPtr->SduLength <= 7u) {
        /* Single Frame — send immediately */
        uint8 frame[CANTP_CAN_DL];
        PduInfoType txPdu;

        (void)memset(frame, 0xCCu, CANTP_CAN_DL);  /* Padding byte */
        frame[0] = (uint8)(CANTP_N_PCI_SF | (PduInfoPtr->SduLength & 0x0Fu));
        (void)memcpy(&frame[1], PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);

        txPdu.SduDataPtr = frame;
        txPdu.SduLength  = CANTP_CAN_DL;

        return PduR_CanTpTransmit(TxPduId, &txPdu);
    }

    /* Multi-frame: copy payload to TX buffer, send FF */
    (void)memcpy(cantp_channel.txBuf, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
    cantp_channel.txTotal  = PduInfoPtr->SduLength;
    cantp_channel.txOffset = 0u;
    cantp_channel.txSn     = 1u;

    /* Build and send First Frame */
    {
        uint8 frame[CANTP_CAN_DL];
        PduInfoType txPdu;
        PduLengthType ffDataLen = 6u;  /* FF has 2 PCI bytes + 6 data bytes */

        frame[0] = (uint8)(CANTP_N_PCI_FF | ((cantp_channel.txTotal >> 8) & 0x0Fu));
        frame[1] = (uint8)(cantp_channel.txTotal & 0xFFu);
        (void)memcpy(&frame[2], cantp_channel.txBuf, ffDataLen);
        cantp_channel.txOffset = ffDataLen;

        txPdu.SduDataPtr = frame;
        txPdu.SduLength  = CANTP_CAN_DL;

        if (PduR_CanTpTransmit(TxPduId, &txPdu) != E_OK) {
            return E_NOT_OK;
        }
    }

    /* Wait for Flow Control from receiver */
    cantp_channel.txState = CANTP_STATE_TX_WAIT_FC;
    cantp_channel.txTimer = CANTP_N_BS_TIMEOUT_MS / CANTP_MAIN_CYCLE_MS;

    return E_OK;
}

void CanTp_MainFunction(void)
{
    if (cantp_initialized == FALSE) {
        return;
    }

    /* ---- RX timeout management ---- */
    if (cantp_channel.rxState == CANTP_STATE_RX_WAIT_CF) {
        if (cantp_channel.rxTimer > 0u) {
            cantp_channel.rxTimer--;
        }
        if (cantp_channel.rxTimer == 0u) {
            CanTp_RxAbort();
        }
    }

    /* ---- TX state management ---- */
    if (cantp_channel.txState == CANTP_STATE_TX_WAIT_FC) {
        if (cantp_channel.txTimer > 0u) {
            cantp_channel.txTimer--;
        }
        if (cantp_channel.txTimer == 0u) {
            CanTp_TxAbort();
        }
    } else if (cantp_channel.txState == CANTP_STATE_TX_CF) {
        /* Handle STmin delay */
        if (cantp_channel.txStminTimer > 0u) {
            cantp_channel.txStminTimer--;
            return;  /* Wait for STmin to elapse */
        }
        CanTp_SendNextCF();
    } else {
        /* MISRA 15.7: terminal else — no action for other TX states */
    }
}

CanTp_StateType CanTp_GetRxState(void)
{
    return cantp_channel.rxState;
}

CanTp_StateType CanTp_GetTxState(void)
{
    return cantp_channel.txState;
}

/* ---- Frame Handlers ---- */

static void CanTp_HandleSF(const PduInfoType* PduInfoPtr)
{
    PduLengthType sfLen;
    PduInfoType upperPdu;

    sfLen = (PduLengthType)(PduInfoPtr->SduDataPtr[0] & 0x0Fu);

    /* Validate SF_DL */
    if ((sfLen == 0u) || (sfLen > 7u)) {
        return;  /* Invalid SF length */
    }
    if ((PduLengthType)(sfLen + 1u) > PduInfoPtr->SduLength) {
        return;  /* Frame too short for declared length */
    }
    if (sfLen > CANTP_MAX_PAYLOAD) {
        return;  /* Exceeds buffer (shouldn't happen for SF, but be safe) */
    }

    /* If mid-reception, abort it — SF takes priority */
    if (cantp_channel.rxState != CANTP_STATE_IDLE) {
        CanTp_RxAbort();
    }

    /* Copy payload and indicate upper layer */
    (void)memcpy(cantp_channel.rxBuf, &PduInfoPtr->SduDataPtr[1], sfLen);

    upperPdu.SduDataPtr = cantp_channel.rxBuf;
    upperPdu.SduLength  = sfLen;

    Dcm_TpRxIndication(cantp_config->upperRxPduId, &upperPdu, NTFRSLT_OK);
}

static void CanTp_HandleFF(const PduInfoType* PduInfoPtr)
{
    PduLengthType ffLen;
    PduLengthType ffDataLen;

    if (PduInfoPtr->SduLength < 2u) {
        return;  /* FF requires at least 2 PCI bytes */
    }

    /* Decode 12-bit length from FF PCI */
    ffLen = (PduLengthType)(((uint16)(PduInfoPtr->SduDataPtr[0] & 0x0Fu) << 8) |
                             (uint16)PduInfoPtr->SduDataPtr[1]);

    /* Validate FF_DL */
    if (ffLen < 8u) {
        return;  /* FF_DL must be > 7 (otherwise use SF) */
    }
    if (ffLen > CANTP_MAX_PAYLOAD) {
        /* Buffer overflow — send FC with overflow status */
        CanTp_SendFC(CANTP_FC_OVERFLOW);
        return;
    }

    /* If mid-reception, abort — new FF takes priority */
    if (cantp_channel.rxState != CANTP_STATE_IDLE) {
        CanTp_RxAbort();
    }

    /* Store FF data (bytes after 2-byte PCI) */
    ffDataLen = PduInfoPtr->SduLength - 2u;
    if (ffDataLen > 6u) {
        ffDataLen = 6u;  /* CAN 2.0B: max 6 data bytes in FF */
    }

    (void)memcpy(cantp_channel.rxBuf, &PduInfoPtr->SduDataPtr[2], ffDataLen);
    cantp_channel.rxTotal = ffLen;
    cantp_channel.rxCount = ffDataLen;
    cantp_channel.rxSn    = 1u;  /* First CF starts at SN=1 */
    cantp_channel.rxState = CANTP_STATE_RX_WAIT_CF;
    cantp_channel.rxTimer = CANTP_N_CR_TIMEOUT_MS / CANTP_MAIN_CYCLE_MS;

    /* Send Flow Control: Continue To Send */
    CanTp_SendFC(CANTP_FC_CTS);
}

static void CanTp_HandleCF(const PduInfoType* PduInfoPtr)
{
    uint8 sn;
    PduLengthType remaining;
    PduLengthType copyLen;

    if (cantp_channel.rxState != CANTP_STATE_RX_WAIT_CF) {
        return;  /* Unexpected CF — ignore */
    }

    /* Check sequence number */
    sn = PduInfoPtr->SduDataPtr[0] & 0x0Fu;
    if (sn != cantp_channel.rxSn) {
        /* Sequence error — abort reception */
        CanTp_RxAbort();
        return;
    }

    /* Copy CF data */
    remaining = cantp_channel.rxTotal - cantp_channel.rxCount;
    copyLen = PduInfoPtr->SduLength - 1u;  /* Subtract PCI byte */
    if (copyLen > 7u) {
        copyLen = 7u;  /* CAN 2.0B: max 7 data bytes in CF */
    }
    if (copyLen > remaining) {
        copyLen = remaining;
    }

    (void)memcpy(&cantp_channel.rxBuf[cantp_channel.rxCount],
                 &PduInfoPtr->SduDataPtr[1], copyLen);
    cantp_channel.rxCount += copyLen;

    /* Advance sequence number (wraps 0-F) */
    cantp_channel.rxSn = (cantp_channel.rxSn + 1u) & 0x0Fu;

    /* Check if reception is complete */
    if (cantp_channel.rxCount >= cantp_channel.rxTotal) {
        PduInfoType upperPdu;

        cantp_channel.rxState = CANTP_STATE_IDLE;

        upperPdu.SduDataPtr = cantp_channel.rxBuf;
        upperPdu.SduLength  = cantp_channel.rxTotal;

        Dcm_TpRxIndication(cantp_config->upperRxPduId, &upperPdu, NTFRSLT_OK);
    } else {
        /* Reset N_Cr timer for next CF */
        cantp_channel.rxTimer = CANTP_N_CR_TIMEOUT_MS / CANTP_MAIN_CYCLE_MS;
    }
}

static void CanTp_HandleFC(const PduInfoType* PduInfoPtr)
{
    uint8 fsType;

    if (cantp_channel.txState != CANTP_STATE_TX_WAIT_FC) {
        return;  /* Unexpected FC — ignore */
    }

    if (PduInfoPtr->SduLength < 3u) {
        CanTp_TxAbort();
        return;
    }

    fsType = PduInfoPtr->SduDataPtr[0] & 0x0Fu;

    switch (fsType) {
    case CANTP_FC_CTS:
        /* Continue To Send — start sending CFs */
        cantp_channel.txState      = CANTP_STATE_TX_CF;
        cantp_channel.txStminTimer = 0u;  /* Send first CF immediately */
        break;

    case CANTP_FC_WAIT:
        /* Reset N_Bs timer and wait for another FC */
        cantp_channel.txTimer = CANTP_N_BS_TIMEOUT_MS / CANTP_MAIN_CYCLE_MS;
        break;

    case CANTP_FC_OVERFLOW:
        /* Receiver can't handle it — abort */
        CanTp_TxAbort();
        break;

    default:
        /* Invalid FS — abort */
        CanTp_TxAbort();
        break;
    }
}

/* ---- Internal Helpers ---- */

static void CanTp_SendFC(uint8 fsType)
{
    uint8 frame[CANTP_CAN_DL];
    PduInfoType txPdu;

    (void)memset(frame, 0xCCu, CANTP_CAN_DL);
    frame[0] = (uint8)(CANTP_N_PCI_FC | (fsType & 0x0Fu));
    frame[1] = CANTP_BLOCK_SIZE;
    frame[2] = CANTP_STMIN_MS;

    txPdu.SduDataPtr = frame;
    txPdu.SduLength  = CANTP_CAN_DL;

    (void)PduR_CanTpTransmit(cantp_config->fcTxPduId, &txPdu);
}

static void CanTp_SendNextCF(void)
{
    uint8 frame[CANTP_CAN_DL];
    PduInfoType txPdu;
    PduLengthType remaining;
    PduLengthType copyLen;

    remaining = cantp_channel.txTotal - cantp_channel.txOffset;
    copyLen = 7u;  /* CAN 2.0B: max 7 data bytes in CF */
    if (copyLen > remaining) {
        copyLen = remaining;
    }

    (void)memset(frame, 0xCCu, CANTP_CAN_DL);
    frame[0] = (uint8)(CANTP_N_PCI_CF | (cantp_channel.txSn & 0x0Fu));
    (void)memcpy(&frame[1], &cantp_channel.txBuf[cantp_channel.txOffset], copyLen);

    txPdu.SduDataPtr = frame;
    txPdu.SduLength  = CANTP_CAN_DL;

    if (PduR_CanTpTransmit(cantp_channel.txPduId, &txPdu) != E_OK) {
        CanTp_TxAbort();
        return;
    }

    cantp_channel.txOffset += copyLen;
    cantp_channel.txSn = (cantp_channel.txSn + 1u) & 0x0Fu;

    if (cantp_channel.txOffset >= cantp_channel.txTotal) {
        /* Transmission complete */
        cantp_channel.txState = CANTP_STATE_IDLE;
    } else {
        /* Set STmin delay for next CF */
        cantp_channel.txStminTimer = CANTP_STMIN_MS / CANTP_MAIN_CYCLE_MS;
        if (cantp_channel.txStminTimer == 0u) {
            cantp_channel.txStminTimer = 1u;  /* At least 1 tick */
        }
    }
}

static void CanTp_RxAbort(void)
{
    cantp_channel.rxState = CANTP_STATE_IDLE;
    cantp_channel.rxCount = 0u;
    cantp_channel.rxTotal = 0u;
    cantp_channel.rxTimer = 0u;
}

static void CanTp_TxAbort(void)
{
    cantp_channel.txState = CANTP_STATE_IDLE;
    cantp_channel.txOffset = 0u;
    cantp_channel.txTotal  = 0u;
    cantp_channel.txTimer  = 0u;
}
