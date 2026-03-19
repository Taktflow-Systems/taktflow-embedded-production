/**
 * @file    PduR.c
 * @brief   PDU Router implementation
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-013
 * @traces_to  TSR-022, TSR-023, TSR-024
 *
 * @standard AUTOSAR_SWS_PDURouter, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "PduR.h"
#include "Det.h"

/* ---- Internal State ---- */

static const PduR_ConfigType* pdur_config = NULL_PTR;
static boolean pdur_initialized = FALSE;

/* ---- API Implementation ---- */

void PduR_Init(const PduR_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_PDUR, 0u, PDUR_API_INIT, DET_E_PARAM_POINTER);
        pdur_initialized = FALSE;
        pdur_config = NULL_PTR;
        return;
    }

    pdur_config = ConfigPtr;
    pdur_initialized = TRUE;
}

void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    uint8 i;

    if ((pdur_initialized == FALSE) || (pdur_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_PDUR, 0u, PDUR_API_CANIF_RX_INDICATION, DET_E_UNINIT);
        return;
    }

    if (PduInfoPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_PDUR, 0u, PDUR_API_CANIF_RX_INDICATION, DET_E_PARAM_POINTER);
        return;
    }

    /* Look up RxPduId in routing table */
    for (i = 0u; i < pdur_config->routingCount; i++) {
        if (pdur_config->routingTable[i].RxPduId == RxPduId) {
            PduIdType upper_id = pdur_config->routingTable[i].UpperPduId;

            switch (pdur_config->routingTable[i].Destination) {
            case PDUR_DEST_COM:
                Com_RxIndication(upper_id, PduInfoPtr);
                break;

            case PDUR_DEST_DCM:
                Dcm_RxIndication(upper_id, PduInfoPtr);
                break;

            case PDUR_DEST_CANTP:
                CanTp_RxIndication(upper_id, PduInfoPtr);
                break;

            default:
                /* Unknown destination — discard */
                break;
            }
            return;
        }
    }

    /* Unknown PDU ID — silently discard */
}

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    if ((pdur_initialized == FALSE) || (pdur_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_PDUR, 0u, PDUR_API_TRANSMIT, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_PDUR, 0u, PDUR_API_TRANSMIT, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* Route TX through CanIf */
    return CanIf_Transmit(TxPduId, PduInfoPtr);
}

Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    /* DCM transmit path — route through generic PduR_Transmit */
    return PduR_Transmit(TxPduId, PduInfoPtr);
}

Std_ReturnType PduR_CanTpTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    /* CanTp lower-layer transmit — single CAN frames to CanIf */
    return PduR_Transmit(TxPduId, PduInfoPtr);
}
