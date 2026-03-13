/**
 * @file    Swc_CanMonitor.c
 * @brief   CAN bus loss detection and recovery SWC
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-024, SWR-CVC-025
 * @traces_to  SSR-CVC-024, SSR-CVC-025, TSR-022, TSR-023
 *
 * @details  Monitors the CAN bus for fault conditions:
 *           - Bus-off detection (immediate SAFE_STOP)
 *           - 200ms silence (no messages received) -> SAFE_STOP
 *           - Error warning sustained 500ms -> SAFE_STOP
 *
 *           Recovery logic: up to 3 attempts within a 10s window.
 *           If all 3 fail, transition to SHUTDOWN.
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_CanMonitor.h"

/* ==================================================================
 * Module State (all static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8   CanMon_Initialized;
static uint8   CanMon_Status;

/* Silence detection */
static uint32  CanMon_LastRxCount;
static uint32  CanMon_LastRxTimeMs;

/* Error warning detection */
static uint32  CanMon_ErrorWarnStartMs;
static uint8   CanMon_ErrorWarnActive;

/* Recovery tracking */
static uint8   CanMon_RecoveryAttempts;
static uint32  CanMon_RecoveryWindowStartMs;

/* ==================================================================
 * API: Swc_CanMonitor_Init
 * ================================================================== */

void Swc_CanMonitor_Init(void)
{
    CanMon_Status              = CANMON_STATUS_OK;
    CanMon_LastRxCount         = 0u;
    CanMon_LastRxTimeMs        = 0u;
    CanMon_ErrorWarnStartMs    = 0u;
    CanMon_ErrorWarnActive     = FALSE;
    CanMon_RecoveryAttempts    = 0u;
    CanMon_RecoveryWindowStartMs = 0u;
    CanMon_Initialized         = TRUE;
}

/* ==================================================================
 * API: Swc_CanMonitor_Check
 * ================================================================== */

/**
 * @safety_req SWR-CVC-024
 */
uint8 Swc_CanMonitor_Check(uint8  isBusOff,
                            uint32 rxMsgCount,
                            uint8  errorWarning,
                            uint32 currentTimeMs)
{
    uint32 silenceMs;
    uint32 errorWarnMs;

    if (CanMon_Initialized != TRUE)
    {
        return CANMON_STATUS_OK;
    }

    /* Already in terminal state? */
    if (CanMon_Status == CANMON_STATUS_SHUTDOWN)
    {
        return CANMON_STATUS_SHUTDOWN;
    }

    /* ---- Check 1: Bus-off (immediate) ---- */
    if (isBusOff == TRUE)
    {
        CanMon_Status = CANMON_STATUS_BUSOFF;
        return CANMON_STATUS_SAFE_STOP;
    }

    /* ---- Check 2: 200ms silence ---- */
    if (rxMsgCount != CanMon_LastRxCount)
    {
        /* New message received — reset silence timer */
        CanMon_LastRxCount  = rxMsgCount;
        CanMon_LastRxTimeMs = currentTimeMs;
    }
    else
    {
        /* No new messages */
        silenceMs = currentTimeMs - CanMon_LastRxTimeMs;
        if (silenceMs >= CANMON_SILENCE_TIMEOUT_MS)
        {
            CanMon_Status = CANMON_STATUS_SILENCE;
            return CANMON_STATUS_SAFE_STOP;
        }
    }

    /* ---- Check 3: Error warning sustained 500ms ---- */
    if (errorWarning == TRUE)
    {
        if (CanMon_ErrorWarnActive == FALSE)
        {
            /* Start tracking */
            CanMon_ErrorWarnActive  = TRUE;
            CanMon_ErrorWarnStartMs = currentTimeMs;
        }
        else
        {
            errorWarnMs = currentTimeMs - CanMon_ErrorWarnStartMs;
            if (errorWarnMs >= CANMON_ERROR_WARN_TIMEOUT_MS)
            {
                CanMon_Status = CANMON_STATUS_ERROR_WARNING;
                return CANMON_STATUS_SAFE_STOP;
            }
        }
    }
    else
    {
        /* Error warning cleared — reset tracking */
        CanMon_ErrorWarnActive = FALSE;
    }

    CanMon_Status = CANMON_STATUS_OK;
    return CANMON_STATUS_OK;
}

/* ==================================================================
 * API: Swc_CanMonitor_Recovery
 * ================================================================== */

/**
 * @safety_req SWR-CVC-025
 */
Std_ReturnType Swc_CanMonitor_Recovery(uint32 currentTimeMs)
{
    uint32 windowElapsed;

    if (CanMon_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    /* Already in SHUTDOWN — no more recovery */
    if (CanMon_Status == CANMON_STATUS_SHUTDOWN)
    {
        return E_NOT_OK;
    }

    /* Reset window if expired */
    windowElapsed = currentTimeMs - CanMon_RecoveryWindowStartMs;
    if (windowElapsed >= CANMON_RECOVERY_WINDOW_MS)
    {
        CanMon_RecoveryAttempts       = 0u;
        CanMon_RecoveryWindowStartMs  = currentTimeMs;
    }

    /* Start window on first attempt */
    if (CanMon_RecoveryAttempts == 0u)
    {
        CanMon_RecoveryWindowStartMs = currentTimeMs;
    }

    CanMon_RecoveryAttempts++;

    if (CanMon_RecoveryAttempts > CANMON_MAX_RECOVERY_ATTEMPTS)
    {
        /* Max attempts exhausted within window — SHUTDOWN */
        CanMon_Status = CANMON_STATUS_SHUTDOWN;
        return E_NOT_OK;
    }

    /* Recovery attempt initiated — reset bus fault state */
    CanMon_Status          = CANMON_STATUS_OK;
    CanMon_ErrorWarnActive = FALSE;

    return E_OK;
}

/* ==================================================================
 * API: Swc_CanMonitor_GetStatus
 * ================================================================== */

uint8 Swc_CanMonitor_GetStatus(void)
{
    return CanMon_Status;
}
