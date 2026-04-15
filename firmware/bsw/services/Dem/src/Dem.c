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
#ifdef SIL_DIAG
#include <stdio.h>
#endif

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

/* Track which DTCs have been broadcast — stores the occurrenceCounter at
 * last broadcast.  Re-broadcasts when a new occurrence is detected.
 * Prevents re-sending the same DTC, but allows new fault instances
 * (e.g. NvM-restored DTCs at boot vs. runtime faults) to broadcast. */
static uint32 dem_broadcast_occ[DEM_MAX_EVENTS];

/* ---- Internal State ---- */

typedef struct {
    sint16  debounceCounter;
    uint8   statusByte;
    uint32  occurrenceCounter;
} Dem_EventDataType;

typedef struct {
    boolean         active;
    uint8           statusMask;
    boolean         reportSupportedOnly;
    Dem_EventIdType nextEventId;
} Dem_FilterStateType;

static Dem_EventDataType dem_events[DEM_MAX_EVENTS];
static Dem_FilterStateType dem_filter;

/* ECU source ID for DTC broadcast (set via Dem_SetEcuId, default 0x00) */
static uint8 dem_ecu_id;

/* CanIf TX PDU ID for DTC broadcast (set via Dem_SetBroadcastPduId) */
static PduIdType dem_broadcast_pdu_id;

static void dem_clear_event(Dem_EventIdType EventId)
{
    dem_events[EventId].debounceCounter   = 0;
    dem_events[EventId].statusByte        = 0u;
    dem_events[EventId].occurrenceCounter = 0u;
    dem_broadcast_occ[EventId]            = 0u;
}

static Std_ReturnType dem_persist_events_to_nvm(void)
{
    uint8 nvm_wr[NVM_BLOCK_SIZE];

    (void)memset(nvm_wr, 0u, sizeof(nvm_wr));
    (void)memcpy(nvm_wr, dem_events, sizeof(dem_events));
    return NvM_WriteBlock(DEM_NVM_BLOCK_ID, (const void*)nvm_wr);
}

static boolean dem_filter_match_event(Dem_EventIdType EventId, uint8* StatusPtr)
{
    uint8 status;

    if (dem_dtc_codes[EventId] == 0u) {
        return FALSE;
    }

    status = dem_events[EventId].statusByte;

    if (dem_filter.reportSupportedOnly == FALSE) {
        if (status == 0u) {
            return FALSE;
        }

        if ((dem_filter.statusMask != 0u) &&
            ((status & dem_filter.statusMask) == 0u)) {
            return FALSE;
        }
    }

    if (StatusPtr != NULL_PTR) {
        *StatusPtr = status;
    }

    return TRUE;
}

/* ---- API Implementation ---- */

void Dem_Init(const void* ConfigPtr)
{
    uint8 i;
    (void)ConfigPtr;

    for (i = 0u; i < DEM_MAX_EVENTS; i++) {
        dem_events[i].debounceCounter   = 0;
        dem_events[i].statusByte        = 0u;
        dem_events[i].occurrenceCounter = 0u;
        dem_broadcast_occ[i]            = 0u;
    }
    dem_ecu_id = 0u;
    dem_broadcast_pdu_id = 0xFFFFu;  /* Unconfigured sentinel */
    dem_filter.active = FALSE;
    dem_filter.statusMask = 0u;
    dem_filter.reportSupportedOnly = FALSE;
    dem_filter.nextEventId = 0u;

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

#ifdef SIL_DIAG
    /* Only trace event 1 (overtemp) to avoid boot DTC noise */
    if (EventId == 1u) {
        (void)fprintf(stderr, "[DEM] ev=1 status=%u deb=%u stb=0x%02X\n",
                      (unsigned)EventStatus,
                      (unsigned)ev->debounceCounter, (unsigned)ev->statusByte);
    }
#endif

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
#ifdef SIL_DIAG
            (void)fprintf(stderr, "[DEM] CONFIRMED ev=%u dtc=0x%06X occ=%u\n",
                          (unsigned)EventId, (unsigned)dem_dtc_codes[EventId],
                          (unsigned)ev->occurrenceCounter);
#endif
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
        dem_clear_event(i);
    }
    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

    return E_OK;
}

Dem_ClearDtcResultType Dem_ClearDTC(uint32 Selector)
{
    Dem_EventIdType event_id;
    boolean matched = FALSE;

    SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();
    if (Selector == 0xFFFFFFu) {
        for (event_id = 0u; event_id < DEM_MAX_EVENTS; event_id++) {
            dem_clear_event(event_id);
        }
        matched = TRUE;
    } else {
        for (event_id = 0u; event_id < DEM_MAX_EVENTS; event_id++) {
            if (dem_dtc_codes[event_id] == Selector) {
                dem_clear_event(event_id);
                matched = TRUE;
            }
        }
    }
    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

    if (matched == FALSE) {
        return DEM_CLEAR_DTC_INVALID_SELECTOR;
    }

    if (dem_persist_events_to_nvm() != E_OK) {
        return DEM_CLEAR_DTC_NVM_FAILED;
    }

    return DEM_CLEAR_DTC_OK;
}

Std_ReturnType Dem_SetDTCFilter(uint8 StatusMask, boolean ReportSupportedOnly)
{
    SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();
    dem_filter.active = TRUE;
    dem_filter.statusMask = StatusMask;
    dem_filter.reportSupportedOnly = ReportSupportedOnly;
    dem_filter.nextEventId = 0u;
    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

    return E_OK;
}

Std_ReturnType Dem_GetNumberOfFilteredDTC(uint16* CountPtr)
{
    Dem_EventIdType event_id;
    uint16 count = 0u;

    if (CountPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_DEM, 0u,
                        DEM_API_GET_NUMBER_OF_FILTERED_DTC,
                        DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (dem_filter.active == FALSE) {
        Det_ReportError(DET_MODULE_DEM, 0u,
                        DEM_API_GET_NUMBER_OF_FILTERED_DTC,
                        DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();
    for (event_id = 0u; event_id < DEM_MAX_EVENTS; event_id++) {
        if (dem_filter_match_event(event_id, NULL_PTR) == TRUE) {
            count++;
        }
    }
    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

    *CountPtr = count;
    return E_OK;
}

Std_ReturnType Dem_GetNextFilteredDTC(uint32* DtcPtr, uint8* StatusPtr)
{
    Dem_EventIdType event_id;

    if ((DtcPtr == NULL_PTR) || (StatusPtr == NULL_PTR)) {
        Det_ReportError(DET_MODULE_DEM, 0u,
                        DEM_API_GET_NEXT_FILTERED_DTC,
                        DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (dem_filter.active == FALSE) {
        Det_ReportError(DET_MODULE_DEM, 0u,
                        DEM_API_GET_NEXT_FILTERED_DTC,
                        DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();
    for (event_id = dem_filter.nextEventId; event_id < DEM_MAX_EVENTS; event_id++) {
        if (dem_filter_match_event(event_id, StatusPtr) == TRUE) {
            *DtcPtr = dem_dtc_codes[event_id];
            dem_filter.nextEventId = (Dem_EventIdType)(event_id + 1u);
            SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();
            return E_OK;
        }
    }
    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

    return E_NOT_OK;
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
            (dem_events[i].occurrenceCounter != dem_broadcast_occ[i]))
        {
            dtc_code = dem_dtc_codes[i];

            if (dtc_code == 0u) {
                SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();
                continue;  /* Skip unmapped event IDs */
            }

            /* Pack DTC_Broadcast frame — DBC layout (little-endian):
             *   Byte 0-1: DTC_Broadcast_Number (16-bit LE, bit 7|16@1+)
             *   Byte 3:   DTC_Broadcast_Status (8-bit, bit 24|8)
             *   Byte 4:   DTC_Broadcast_ECU_Source (8-bit, bit 32|8)
             *   Byte 5:   DTC_Broadcast_OccurrenceCount (8-bit, bit 40|8)
             *   Byte 6:   DTC_Broadcast_FreezeFrame0 (8-bit, bit 48|8)
             *   Byte 7:   DTC_Broadcast_FreezeFrame1 (8-bit, bit 56|8)
             * DTC code is 24-bit UDS but DBC Number field is 16-bit.
             * Pack lower 16 bits in LE: byte0=low, byte1=high. */
            {
                uint16 dtc_16 = (uint16)(dtc_code & 0xFFFFu);
                pdu_data[0] = (uint8)((dtc_16 >> 8u) & 0xFFu);  /* DTC high byte */
                pdu_data[1] = (uint8)(dtc_16 & 0xFFu);           /* DTC low byte */
            }
            pdu_data[2] = 0x00u;                                    /* Reserved */
            pdu_data[3] = dem_events[i].statusByte;              /* Status */
            pdu_data[4] = dem_ecu_id;                               /* ECU source */
            pdu_data[5] = (uint8)(dem_events[i].occurrenceCounter & 0xFFu);
            pdu_data[6] = 0x00u;
            pdu_data[7] = 0x00u;

            /* Mark as broadcast — don't re-send until cleared */
            dem_broadcast_occ[i] = dem_events[i].occurrenceCounter;

            SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();

            /* Transmit via PduR -> CanIf -> CAN 0x500 (outside critical section).
             * Guard: skip if broadcast PDU ID not configured (zone controller
             * called Dem_Init but not Dem_SetBroadcastPduId yet). */
            if (dem_broadcast_pdu_id != 0xFFFFu)
            {
#ifdef SIL_DIAG
                (void)fprintf(stderr, "[DEM] BROADCAST ev=%u dtc=0x%06X pdu=%u\n",
                              (unsigned)i, (unsigned)dtc_code,
                              (unsigned)dem_broadcast_pdu_id);
#endif
                Det_ReportRuntimeError(DET_MODULE_DEM, (uint8)i,
                                       DEM_API_MAIN_FUNCTION, DET_E_DBG_DTC_BROADCAST);
                {
                    Std_ReturnType tx_ret = PduR_Transmit(dem_broadcast_pdu_id, &pdu_info);
#ifdef SIL_DIAG
                    (void)fprintf(stderr, "[DEM] PduR_Transmit ret=%u data=[%02X %02X %02X %02X %02X %02X %02X %02X]\n",
                                  (unsigned)tx_ret,
                                  pdu_data[0], pdu_data[1], pdu_data[2], pdu_data[3],
                                  pdu_data[4], pdu_data[5], pdu_data[6], pdu_data[7]);
#endif
                    (void)tx_ret;
                }
            }

            /* Persist to NvM (outside critical section).
             * Use temp buffer to avoid writing adjacent BSS into NvM
             * (NVM_BLOCK_SIZE > sizeof(dem_events)). */
            (void)dem_persist_events_to_nvm();
        } else {
            SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();
        }
    }
}
