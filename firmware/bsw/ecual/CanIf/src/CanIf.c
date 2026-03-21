/**
 * @file    CanIf.c
 * @brief   CAN Interface implementation
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-011, SWR-BSW-012
 * @traces_to  TSR-022, TSR-023, TSR-024
 *
 * @standard AUTOSAR_SWS_CANInterface, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "CanIf.h"
#include "CanSM.h"
#include "Det.h"

/* ---- Internal State ---- */

static const CanIf_ConfigType* canif_config = NULL_PTR;
static boolean canif_initialized = FALSE;

/* ---- API Implementation ---- */

void CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CANIF, 0u, CANIF_API_INIT, DET_E_PARAM_POINTER);
        canif_initialized = FALSE;
        canif_config = NULL_PTR;
        return;
    }

    canif_config = ConfigPtr;
    canif_initialized = TRUE;
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    Can_PduType can_pdu;

    if ((canif_initialized == FALSE) || (canif_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_CANIF, 0u, CANIF_API_TRANSMIT, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CANIF, 0u, CANIF_API_TRANSMIT, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (TxPduId >= canif_config->txPduCount) {
        Det_ReportError(DET_MODULE_CANIF, 0u, CANIF_API_TRANSMIT, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    /* Debug counter */
    {
        static volatile uint32 canif_tx_count[16] = {0};
        if (TxPduId < 16u) { canif_tx_count[TxPduId]++; }
    }

    /* Map PDU ID to CAN ID */
    const CanIf_TxPduConfigType* tx_cfg = &canif_config->txPduConfig[TxPduId];

    can_pdu.id     = tx_cfg->CanId;
    can_pdu.length = PduInfoPtr->SduLength;
    can_pdu.sdu    = PduInfoPtr->SduDataPtr;

    Can_ReturnType result = Can_Write(tx_cfg->Hth, &can_pdu);

    return (result == CAN_OK) ? E_OK : E_NOT_OK;
}

void CanIf_RxIndication(Can_IdType CanId, const uint8* SduPtr, uint8 Dlc)
{
    PduInfoType pdu_info;
    uint8 i;

    if ((canif_initialized == FALSE) || (canif_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_CANIF, 0u, CANIF_API_RX_INDICATION, DET_E_UNINIT);
        return;
    }

    if (SduPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CANIF, 0u, CANIF_API_RX_INDICATION, DET_E_PARAM_POINTER);
        return;
    }

    /* Look up CAN ID in RX routing table */
#ifdef SIL_DIAG
    if (CanId == 0x550u) {
        fprintf(stderr, "[CANIF] RX CAN 0x%03X dlc=%u rxCount=%u\n",
                (unsigned)CanId, (unsigned)Dlc, (unsigned)canif_config->rxPduCount);
    }
#endif
    for (i = 0u; i < canif_config->rxPduCount; i++) {
        if (canif_config->rxPduConfig[i].CanId == CanId) {
            /* Optional E2E RX check — drop frame if callback returns E_NOT_OK */
            if (canif_config->e2eRxCheck != NULL_PTR) {
                if (canif_config->e2eRxCheck(
                        canif_config->rxPduConfig[i].UpperPduId,
                        SduPtr, Dlc) != E_OK) {
                    return;  /* E2E check failed — drop frame */
                }
            }

            /* Found — route to PduR */
            pdu_info.SduDataPtr = (uint8*)SduPtr; /* const-cast for AUTOSAR API */
            pdu_info.SduLength  = Dlc;

            PduR_CanIfRxIndication(canif_config->rxPduConfig[i].UpperPduId,
                                   &pdu_info);
            return;
        }
    }

    /* Unknown CAN ID — silently discard per SWR-BSW-011 */
}

void CanIf_ControllerBusOff(uint8 controllerId)
{
    (void)controllerId;
    /* Forward to CanSM for L1/L2 bus-off recovery */
    CanSM_ControllerBusOff();
}
