/**
 * @file    Det.c
 * @brief   Default Error Tracer implementation
 * @date    2026-03-03
 *
 * @details Ring buffer stores the last DET_LOG_SIZE errors. In POSIX/SIL
 *          builds, every error is also printed to stderr via fprintf
 *          (Docker stdout is fully buffered — stderr is line-buffered).
 *
 * @safety_req SWR-BSW-040
 * @traces_to  TSR-022, TSR-038
 *
 * @standard AUTOSAR_SWS_DefaultErrorTracer, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Det.h"

/* ---- Internal State ---- */

static Det_ErrorEntryType det_log[DET_LOG_SIZE];
static uint8              det_log_head;     /**< Next write position    */
static uint8              det_log_count;    /**< Entries in buffer      */
static uint16             det_error_count;  /**< Total errors reported  */
static boolean            det_initialized;
static Det_CallbackType   det_callback;

/* ---- API Implementation ---- */

void Det_Init(void)
{
    uint8 i;

    for (i = 0u; i < DET_LOG_SIZE; i++) {
        det_log[i].ModuleId   = 0u;
        det_log[i].InstanceId = 0u;
        det_log[i].ApiId      = 0u;
        det_log[i].ErrorId    = 0u;
    }

    det_log_head    = 0u;
    det_log_count   = 0u;
    det_error_count = 0u;
    det_callback    = NULL_PTR;
    det_initialized = TRUE;
}

void Det_ReportError(uint16 ModuleId, uint8 InstanceId,
                     uint8 ApiId, uint8 ErrorId)
{
    if (det_initialized == FALSE) {
        return;
    }

    /* Store in ring buffer */
    det_log[det_log_head].ModuleId   = ModuleId;
    det_log[det_log_head].InstanceId = InstanceId;
    det_log[det_log_head].ApiId      = ApiId;
    det_log[det_log_head].ErrorId    = ErrorId;

    det_log_head = (det_log_head + 1u) % DET_LOG_SIZE;

    if (det_log_count < DET_LOG_SIZE) {
        det_log_count++;
    }

    /* Saturating total counter */
    if (det_error_count < 0xFFFFu) {
        det_error_count++;
    }

    /* User callback (SIL builds register Det_Callout_SilPrint here) */
    if (det_callback != NULL_PTR) {
        det_callback(ModuleId, InstanceId, ApiId, ErrorId);
    }
}

void Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId,
                            uint8 ApiId, uint8 ErrorId)
{
    /* Same implementation — AUTOSAR distinguishes development vs runtime
     * errors conceptually but the logging path is identical */
    Det_ReportError(ModuleId, InstanceId, ApiId, ErrorId);
}

uint16 Det_GetErrorCount(void)
{
    return det_error_count;
}

Std_ReturnType Det_GetLogEntry(uint8 Index, Det_ErrorEntryType* EntryPtr)
{
    uint8 actual_index;

    if (EntryPtr == NULL_PTR) {
        return E_NOT_OK;
    }

    if (Index >= det_log_count) {
        return E_NOT_OK;
    }

    /* Ring buffer: oldest entry is at (head - count), wrapped */
    if (det_log_count < DET_LOG_SIZE) {
        actual_index = Index;
    } else {
        actual_index = (det_log_head + Index) % DET_LOG_SIZE;
    }

    EntryPtr->ModuleId   = det_log[actual_index].ModuleId;
    EntryPtr->InstanceId = det_log[actual_index].InstanceId;
    EntryPtr->ApiId      = det_log[actual_index].ApiId;
    EntryPtr->ErrorId    = det_log[actual_index].ErrorId;

    return E_OK;
}

void Det_SetCallback(Det_CallbackType Callback)
{
    det_callback = Callback;
}
