/**
 * @file    Dem.c
 * @brief   Diagnostic Event Manager implementation
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-017, SWR-BSW-018
 * @traces_to  TSR-038, TSR-039
 *
 * @standard AUTOSAR_SWS_DiagnosticEventManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Dem.h"
#include "SchM.h"
#include "Det.h"
#include "ComStack_Types.h"
#include "NvM.h"

#include <string.h>

/* ---- Forward declaration for PduR_Transmit (avoids circular include) ---- */
extern Std_ReturnType PduR_Transmit(PduIdType TxPduId,
                                     const PduInfoType* PduInfoPtr);

/* ---- NvM block ID for DTC persistence ---- */
#define DEM_NVM_BLOCK_ID            1u

/* ---- DTC-to-UDS code mapping (configurable per ECU via Dem_SetDtcCode) ---- */
static uint32 dem_dtc_codes[DEM_MAX_EVENTS] = {
    0xC00100u, /* 0:  Pedal plausibility */
    0xC00200u, /* 1:  Pedal sensor 1 fail */
    0xC00300u, /* 2:  Pedal sensor 2 fail */
    0xC00400u, /* 3:  Pedal stuck */
    0xC10100u, /* 4:  CAN FZC timeout */
    0xC10200u, /* 5:  CAN RZC timeout */
    0xC10300u, /* 6:  CAN bus-off */
    0xC20100u, /* 7:  Motor overcurrent */
    0xC20200u, /* 8:  Motor overtemp */
    0xC20300u, /* 9:  Motor cutoff RX */
    0xC30100u, /* 10: Brake fault RX */
    0xC30200u, /* 11: Steering fault RX */
    0xC40100u, /* 12: E-stop activated */
    0xC50100u, /* 13: Battery undervolt */
    0xC50200u, /* 14: Battery overvolt */
    0xC60100u, /* 15: NVM CRC fail */
    0xC60200u, /* 16: Self-test fail */
    0xC70100u, /* 17: Display comm */
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u  /* 18-31: reserved */
};

/* Track which DTCs have been broadcast (avoid re-broadcasting same DTC) */
static uint8 dem_broadcast_sent[DEM_MAX_EVENTS];

/* ---- Internal State ---- */

typedef struct {
    sint16  debounceCounter;
    uint8   statusByte;
    uint32  occurrenceCounter;
} Dem_EventDataType;

static Dem_EventDataType dem_events[DEM_MAX_EVENTS];

/* ECU source ID for DTC broadcast (set via Dem_SetEcuId, default 0x00) */
static uint8 dem_ecu_id;

/* CanIf TX PDU ID for DTC broadcast (set via Dem_SetBroadcastPduId) */
static PduIdType dem_broadcast_pdu_id;

/* ---- API Implementation ---- */

void Dem_Init(const void* ConfigPtr)
{
    uint8 i;
    (void)ConfigPtr;

    for (i = 0u; i < DEM_MAX_EVENTS; i++) {
        dem_events[i].debounceCounter   = 0;
        dem_events[i].statusByte        = 0u;
        dem_events[i].occurrenceCounter = 0u;
        dem_broadcast_sent[i]           = 0u;
    }
    dem_ecu_id = 0u;
    dem_broadcast_pdu_id = 0xFFFFu;  /* Unconfigured sentinel */

    /* Restore occurrence counters from NvM (persistence across power cycles).
     * NVM_BLOCK_SIZE (1024) exceeds sizeof(dem_events) (~224 bytes).
     * Read into a properly-sized temp buffer, copy only valid data to
     * prevent overflow into adjacent BSS statics. */
    {
        uint8 nvm_tmp[NVM_BLOCK_SIZE];
        (void)memset(nvm_tmp, 0u, sizeof(nvm_tmp));
        (void)NvM_ReadBlock(DEM_NVM_BLOCK_ID, (void*)nvm_tmp);
        (void)memcpy(dem_events, nvm_tmp, sizeof(dem_events));
    }

    for (i = 0u; i < DEM_MAX_EVENTS; i++) {
        dem_events[i].debounceCounter = 0;
        dem_events[i].statusByte      = 0u;
        /* occurrenceCounter preserved from NvM */
    }
}

void Dem_ReportErrorStatus(Dem_EventIdType EventId,
                           Dem_EventStatusType EventStatus)
{
    if (EventId >= DEM_MAX_EVENTS) {
        Det_ReportError(DET_MODULE_DEM, 0u, DEM_API_REPORT_ERROR_STATUS, DET_E_PARAM_VALUE);
        return;
    }

    SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();

    Dem_EventDataType* ev = &dem_events[EventId];

    if (EventStatus == DEM_EVENT_STATUS_FAILED) {
        /* Increment debounce counter toward fail threshold */
        if (ev->debounceCounter < DEM_DEBOUNCE_FAIL_THRESHOLD) {
            ev->debounceCounter++;
        }

        /* Set testFailed and pendingDTC on first failure (AUTOSAR DEM) */
        ev->statusByte |= DEM_STATUS_TEST_FAILED;
        ev->statusByte |= DEM_STATUS_PENDING_DTC;

        /* Confirm DTC when threshold reached */
        if (ev->debounceCounter >= DEM_DEBOUNCE_FAIL_THRESHOLD) {
            ev->statusByte |= DEM_STATUS_CONFIRMED_DTC;
            ev->occurrenceCounter++;
            Det_ReportRuntimeError(DET_MODULE_DEM, (uint8)EventId,
                                   DEM_API_REPORT_ERROR_STATUS, DET_E_DBG_DTC_CONFIRMED);
        }
    } else {
        /* Decrement debounce counter toward pass threshold */
        if (ev->debounceCounter > DEM_DEBOUNCE_PASS_THRESHOLD) {
            ev->debounceCounter--;
        }

        /* Clear testFailed when counter reaches zero (healed) */
        if (ev->debounceCounter <= 0) {
            ev->statusByte &= (uint8)(~DEM_STATUS_TEST_FAILED);
        }

        /* Clamp at pass threshold */
        if (ev->debounceCounter <= DEM_DEBOUNCE_PASS_THRESHOLD) {
            ev->debounceCounter = DEM_DEBOUNCE_PASS_THRESHOLD;
        }
    }

    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();
}

Std_ReturnType Dem_GetEventStatus(Dem_EventIdType EventId, uint8* StatusPtr)
{
    if (EventId >= DEM_MAX_EVENTS) {
        Det_ReportError(DET_MODULE_DEM, 0u, DEM_API_REPORT_ERROR_STATUS, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    if (StatusPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_DEM, 0u, DEM_API_REPORT_ERROR_STATUS, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();
    *StatusPtr = dem_events[EventId].statusByte;
    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

    return E_OK;
}

Std_ReturnType Dem_GetOccurrenceCounter(Dem_EventIdType EventId, uint32* CountPtr)
{
    if (EventId >= DEM_MAX_EVENTS) {
        Det_ReportError(DET_MODULE_DEM, 0u, DEM_API_REPORT_ERROR_STATUS, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    if (CountPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_DEM, 0u, DEM_API_REPORT_ERROR_STATUS, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();
    *CountPtr = dem_events[EventId].occurrenceCounter;
    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

    return E_OK;
}

Std_ReturnType Dem_ClearAllDTCs(void)
{
    uint8 i;

    SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();
    for (i = 0u; i < DEM_MAX_EVENTS; i++) {
        dem_events[i].debounceCounter   = 0;
        dem_events[i].statusByte        = 0u;
        dem_events[i].occurrenceCounter = 0u;
        dem_broadcast_sent[i]           = 0u;
    }
    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

    return E_OK;
}

/* ==================================================================
 * API: Dem_SetEcuId / Dem_SetDtcCode
 * ================================================================== */

void Dem_SetEcuId(uint8 EcuId)
{
    dem_ecu_id = EcuId;
}

void Dem_SetDtcCode(Dem_EventIdType EventId, uint32 DtcCode)
{
    if (EventId >= DEM_MAX_EVENTS) {
        Det_ReportError(DET_MODULE_DEM, 0u, DEM_API_SET_ECU_ID, DET_E_PARAM_VALUE);
        return;
    }

    dem_dtc_codes[EventId] = DtcCode;
}

void Dem_SetBroadcastPduId(PduIdType TxPduId)
{
    dem_broadcast_pdu_id = TxPduId;
}

/* ==================================================================
 * API: Dem_MainFunction — periodic DTC broadcast on CAN 0x500
 *
 * DTC_Broadcast frame format (8 bytes):
 *   Byte 0-2: DTC code (24-bit UDS DTC number)
 *   Byte 3:   DTC status byte (ISO 14229)
 *   Byte 4:   ECU source ID (0x10 = CVC)
 *   Byte 5:   Occurrence counter (low byte)
 *   Byte 6-7: Reserved (0x00)
 *
 * @safety_req SWR-BSW-017, SWR-BSW-018
 * ================================================================== */

void Dem_MainFunction(void)
{
    uint8 i;
    uint8 pdu_data[8];
    PduInfoType pdu_info;
    uint32 dtc_code;

    pdu_info.SduDataPtr = pdu_data;
    pdu_info.SduLength  = 8u;

    for (i = 0u; i < DEM_MAX_EVENTS; i++) {
        SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();

        /* Only broadcast newly confirmed DTCs that haven't been sent yet */
        if (((dem_events[i].statusByte & DEM_STATUS_CONFIRMED_DTC) != 0u) &&
            (dem_broadcast_sent[i] == 0u))
        {
            dtc_code = dem_dtc_codes[i];

            if (dtc_code == 0u) {
                SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();
                continue;  /* Skip unmapped event IDs */
            }

            /* Pack DTC_Broadcast frame */
            pdu_data[0] = (uint8)((dtc_code >> 16u) & 0xFFu);  /* DTC high */
            pdu_data[1] = (uint8)((dtc_code >> 8u) & 0xFFu);   /* DTC mid */
            pdu_data[2] = (uint8)(dtc_code & 0xFFu);            /* DTC low */
            pdu_data[3] = dem_events[i].statusByte;              /* Status */
            pdu_data[4] = dem_ecu_id;                               /* ECU source */
            pdu_data[5] = (uint8)(dem_events[i].occurrenceCounter & 0xFFu);
            pdu_data[6] = 0x00u;
            pdu_data[7] = 0x00u;

            /* Mark as broadcast — don't re-send until cleared */
            dem_broadcast_sent[i] = 1u;

            SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

            /* Transmit via PduR -> CanIf -> CAN 0x500 (outside critical section).
             * Guard: skip if broadcast PDU ID not configured (zone controller
             * called Dem_Init but not Dem_SetBroadcastPduId yet). */
            if (dem_broadcast_pdu_id != 0xFFFFu)
            {
                Det_ReportRuntimeError(DET_MODULE_DEM, (uint8)i,
                                       DEM_API_MAIN_FUNCTION, DET_E_DBG_DTC_BROADCAST);
                (void)PduR_Transmit(dem_broadcast_pdu_id, &pdu_info);
            }

            /* Persist to NvM (outside critical section).
             * Use temp buffer to avoid writing adjacent BSS into NvM
             * (NVM_BLOCK_SIZE > sizeof(dem_events)). */
            {
                uint8 nvm_wr[NVM_BLOCK_SIZE];
                (void)memset(nvm_wr, 0u, sizeof(nvm_wr));
                (void)memcpy(nvm_wr, dem_events, sizeof(dem_events));
                (void)NvM_WriteBlock(DEM_NVM_BLOCK_ID, (const void*)nvm_wr);
            }
        } else {
            SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();
        }
    }
}
