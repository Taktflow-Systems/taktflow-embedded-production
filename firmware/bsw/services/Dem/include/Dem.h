/**
 * @file    Dem.h
 * @brief   Diagnostic Event Manager — DTC storage and debouncing
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-017, SWR-BSW-018
 * @traces_to  TSR-038, TSR-039
 *
 * @standard AUTOSAR_SWS_DiagnosticEventManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef DEM_H
#define DEM_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ---- Constants ---- */

#define DEM_MAX_EVENTS          32u
#define DEM_DEBOUNCE_FAIL_THRESHOLD   3
#define DEM_DEBOUNCE_PASS_THRESHOLD   (-3)

/* DTC Status Bits (ISO 14229) */
#define DEM_STATUS_TEST_FAILED          0x01u
#define DEM_STATUS_PENDING_DTC          0x04u
#define DEM_STATUS_CONFIRMED_DTC        0x08u

/* ---- Types ---- */

typedef uint8  Dem_EventIdType;

typedef enum {
    DEM_EVENT_STATUS_PASSED = 0u,
    DEM_EVENT_STATUS_FAILED = 1u
} Dem_EventStatusType;

/* ---- API Functions ---- */

void           Dem_Init(const void* ConfigPtr);
void           Dem_ReportErrorStatus(Dem_EventIdType EventId,
                                     Dem_EventStatusType EventStatus);
Std_ReturnType Dem_GetEventStatus(Dem_EventIdType EventId, uint8* StatusPtr);
Std_ReturnType Dem_GetOccurrenceCounter(Dem_EventIdType EventId, uint32* CountPtr);
Std_ReturnType Dem_ClearAllDTCs(void);

/**
 * @brief  Periodic DTC broadcast function
 *
 * Scans for newly confirmed DTCs and broadcasts them via Com on CAN 0x500.
 * Call from the 100ms periodic task. Reusable across all ECUs.
 */
void           Dem_MainFunction(void);

/**
 * @brief  Set the ECU source ID for DTC broadcasts
 * @param  EcuId  ECU identifier (e.g. 0x10=CVC, 0x20=FZC, 0x30=RZC)
 */
void           Dem_SetEcuId(uint8 EcuId);

/**
 * @brief  Set the CanIf TX PDU ID for DTC broadcast transmission
 * @param  TxPduId  CanIf TX PDU ID mapped to CAN 0x500
 *
 * @details Must be called after Dem_Init and before Dem_MainFunction.
 *          If not called, Dem_MainFunction will skip PduR_Transmit
 *          (guard against unconfigured broadcast on zone controllers).
 */
void           Dem_SetBroadcastPduId(PduIdType TxPduId);

/**
 * @brief  Register a UDS DTC code for an event ID
 * @param  EventId   Event ID (0 .. DEM_MAX_EVENTS-1)
 * @param  DtcCode   24-bit UDS DTC code (e.g. 0xC00100)
 */
void           Dem_SetDtcCode(Dem_EventIdType EventId, uint32 DtcCode);

#endif /* DEM_H */
