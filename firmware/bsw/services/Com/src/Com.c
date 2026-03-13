/**
 * @file    Com.c
 * @brief   Communication module implementation
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-015, SWR-BSW-016
 * @traces_to  TSR-022, TSR-023, TSR-024
 *
 * @standard AUTOSAR_SWS_COMModule, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Com.h"
#include "SchM.h"
#include "Det.h"

/* ---- Internal State ---- */

static const Com_ConfigType*  com_config = NULL_PTR;
static boolean                com_initialized = FALSE;

/* TX PDU buffers */
static uint8  com_tx_pdu_buf[COM_MAX_PDUS][COM_PDU_SIZE];
static boolean com_tx_pending[COM_MAX_PDUS];

/* RX PDU buffers */
static uint8  com_rx_pdu_buf[COM_MAX_PDUS][COM_PDU_SIZE];

/* RX deadline monitoring: cycles since last Com_RxIndication per PDU */
static uint16 com_rx_timeout_cnt[COM_MAX_PDUS];

/* RX deadline monitoring period: 10ms per cycle (matches RTE scheduler) */
#define COM_RX_CYCLE_MS   10u

/* ---- Private Helpers ---- */

static uint8 com_get_byte_offset(uint8 bitPosition)
{
    return bitPosition / 8u;
}

/* ---- API Implementation ---- */

void Com_Init(const Com_ConfigType* ConfigPtr)
{
    uint8 i;
    uint8 j;

    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_INIT, DET_E_PARAM_POINTER);
        com_initialized = FALSE;
        com_config = NULL_PTR;
        return;
    }

    com_config = ConfigPtr;

    /* Clear PDU buffers */
    for (i = 0u; i < COM_MAX_PDUS; i++) {
        for (j = 0u; j < COM_PDU_SIZE; j++) {
            com_tx_pdu_buf[i][j] = 0u;
            com_rx_pdu_buf[i][j] = 0u;
        }
        com_tx_pending[i] = FALSE;
        com_rx_timeout_cnt[i] = 0u;
    }

    com_initialized = TRUE;
}

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_SEND_SIGNAL, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (SignalId >= com_config->signalCount) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_SEND_SIGNAL, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    if (SignalDataPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_SEND_SIGNAL, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const Com_SignalConfigType* sig = &com_config->signalConfig[SignalId];

    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();

    /* Copy signal value to shadow buffer */
    switch (sig->Type) {
    case COM_UINT8:
    case COM_BOOL:
        *((uint8*)sig->ShadowBuffer) = *((const uint8*)SignalDataPtr);
        break;
    case COM_UINT16:
        *((uint16*)sig->ShadowBuffer) = *((const uint16*)SignalDataPtr);
        break;
    case COM_SINT16:
        *((sint16*)sig->ShadowBuffer) = *((const sint16*)SignalDataPtr);
        break;
    default:
        SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
        return E_NOT_OK;
    }

    /* Pack signal into TX PDU buffer */
    if (sig->PduId < COM_MAX_PDUS) {
        uint8 byte_offset = com_get_byte_offset(sig->BitPosition);

        if (sig->BitSize <= 8u) {
            com_tx_pdu_buf[sig->PduId][byte_offset] = *((const uint8*)SignalDataPtr);
        } else if (sig->BitSize <= 16u) {
            /* Little-endian packing */
            uint16 val = *((const uint16*)SignalDataPtr);
            com_tx_pdu_buf[sig->PduId][byte_offset]     = (uint8)(val & 0xFFu);
            com_tx_pdu_buf[sig->PduId][byte_offset + 1u] = (uint8)((val >> 8u) & 0xFFu);
        } else {
            /* BitSize > 16 not supported — no action */
        }

        com_tx_pending[sig->PduId] = TRUE;
    }

    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    return E_OK;
}

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RECEIVE_SIGNAL, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (SignalId >= com_config->signalCount) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RECEIVE_SIGNAL, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    if (SignalDataPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RECEIVE_SIGNAL, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const Com_SignalConfigType* sig = &com_config->signalConfig[SignalId];

    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();

    /* Copy from shadow buffer to caller */
    switch (sig->Type) {
    case COM_UINT8:
    case COM_BOOL:
        *((uint8*)SignalDataPtr) = *((const uint8*)sig->ShadowBuffer);
        break;
    case COM_UINT16:
        *((uint16*)SignalDataPtr) = *((const uint16*)sig->ShadowBuffer);
        break;
    case COM_SINT16:
        *((sint16*)SignalDataPtr) = *((const sint16*)sig->ShadowBuffer);
        break;
    default:
        SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
        return E_NOT_OK;
    }

    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    return E_OK;
}

void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr)
{
    uint8 i;

    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RX_INDICATION, DET_E_UNINIT);
        return;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR)) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RX_INDICATION, DET_E_PARAM_POINTER);
        return;
    }

    if (PduInfoPtr->SduLength == 0u) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RX_INDICATION, DET_E_PARAM_VALUE);
        return;
    }

    if (ComRxPduId >= COM_MAX_PDUS) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RX_INDICATION, DET_E_PARAM_VALUE);
        return;
    }

    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();

    /* Reset RX deadline counter — fresh data arrived */
    com_rx_timeout_cnt[ComRxPduId] = 0u;

    /* Store received PDU data */
    for (i = 0u; (i < PduInfoPtr->SduLength) && (i < COM_PDU_SIZE); i++) {
        com_rx_pdu_buf[ComRxPduId][i] = PduInfoPtr->SduDataPtr[i];
    }

    /* Unpack signals belonging to this RX PDU */
    for (i = 0u; i < com_config->signalCount; i++) {
        const Com_SignalConfigType* sig = &com_config->signalConfig[i];

        if (sig->PduId == ComRxPduId) {
            uint8 byte_offset = com_get_byte_offset(sig->BitPosition);

            if (sig->BitSize <= 8u) {
                *((uint8*)sig->ShadowBuffer) = com_rx_pdu_buf[ComRxPduId][byte_offset];
            } else if (sig->BitSize <= 16u) {
                uint16 val = (uint16)com_rx_pdu_buf[ComRxPduId][byte_offset] |
                             ((uint16)com_rx_pdu_buf[ComRxPduId][byte_offset + 1u] << 8u);
                *((uint16*)sig->ShadowBuffer) = val;
            } else {
                /* BitSize > 16 not supported — no action */
            }
        }
    }

    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
}

void Com_MainFunction_Tx(void)
{
    uint8 i;

    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        return;
    }

    /* Transmit all pending TX PDUs */
    for (i = 0u; i < com_config->txPduCount; i++) {
        PduIdType pdu_id = com_config->txPduConfig[i].PduId;

        SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
        if ((pdu_id < COM_MAX_PDUS) && (com_tx_pending[pdu_id] == TRUE)) {
            PduInfoType pdu_info;
            pdu_info.SduDataPtr = com_tx_pdu_buf[pdu_id];
            pdu_info.SduLength  = com_config->txPduConfig[i].Dlc;
            SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();

            if (PduR_Transmit(pdu_id, &pdu_info) == E_OK) {
                SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
                com_tx_pending[pdu_id] = FALSE;
                SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
            }
        } else {
            SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
        }
    }
}

void Com_MainFunction_Rx(void)
{
    uint8 i;
    uint8 j;

    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        return;
    }

    /* RX deadline monitoring: for each configured RX PDU with a non-zero
     * timeout, increment the cycle counter. If the counter exceeds the
     * configured timeout, replace shadow buffer signals with initial
     * values (zero) — matches AUTOSAR ComRxDataTimeoutAction = REPLACE. */
    for (i = 0u; i < com_config->rxPduCount; i++) {
        PduIdType pdu_id   = com_config->rxPduConfig[i].PduId;
        uint16    timeout  = com_config->rxPduConfig[i].TimeoutMs;

        if ((pdu_id >= COM_MAX_PDUS) || (timeout == 0u)) {
            continue;
        }

        SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();

        if (com_rx_timeout_cnt[pdu_id] < 0xFFFFu) {
            com_rx_timeout_cnt[pdu_id]++;
        }

        if ((com_rx_timeout_cnt[pdu_id] * COM_RX_CYCLE_MS) >= timeout) {
            /* Timeout: zero-fill shadow buffers for all signals on this PDU */
            for (j = 0u; j < com_config->signalCount; j++) {
                const Com_SignalConfigType* sig = &com_config->signalConfig[j];

                if (sig->PduId == pdu_id) {
                    switch (sig->Type) {
                    case COM_UINT8:
                    case COM_BOOL:
                        *((uint8*)sig->ShadowBuffer) = 0u;
                        break;
                    case COM_UINT16:
                        *((uint16*)sig->ShadowBuffer) = 0u;
                        break;
                    case COM_SINT16:
                        *((sint16*)sig->ShadowBuffer) = 0;
                        break;
                    default:
                        break;
                    }
                }
            }

            /* Also clear the PDU buffer */
            for (j = 0u; j < COM_PDU_SIZE; j++) {
                com_rx_pdu_buf[pdu_id][j] = 0u;
            }
        }

        SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    }
}
