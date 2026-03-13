/**
 * @file    Swc_FzcCanMonitor.c
 * @brief   FZC CAN bus loss detection — bus-off, silence, error warning
 * @date    2026-02-24
 *
 * @safety_req SWR-FZC-024
 * @traces_to  SSR-FZC-024
 *
 * @details  Monitors the CAN bus for loss conditions:
 *           1. Bus-off detection via CanIf_GetControllerMode
 *           2. Silence detection: no message received for 200ms (20 cycles)
 *           3. Error warning: sustained error counter threshold (50 cycles)
 *
 *           On any detection:
 *           - Auto-brake 100%
 *           - Center steering (0 deg)
 *           - Continuous buzzer
 *           - NO recovery — maintain safe state until power cycle
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_FzcCanMonitor.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"
#include "Can.h"
#include "Dem.h"

/* ==================================================================
 * Module State (static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8   CanMon_Initialized;

/** Current CAN monitor status */
static uint8   CanMon_Status;

/** Silence counter: cycles since last RX notification */
static uint16  CanMon_SilenceCount;

/** Error warning sustained counter */
static uint16  CanMon_ErrWarnCount;

/** Latched safe state flag — once set, never cleared (power cycle only) */
static uint8   CanMon_SafeLatched;

/* ==================================================================
 * API: Swc_FzcCanMonitor_Init
 * ================================================================== */

void Swc_FzcCanMonitor_Init(void)
{
    CanMon_Status       = FZC_CAN_OK;
    CanMon_SilenceCount = 0u;
    CanMon_ErrWarnCount = 0u;
    CanMon_SafeLatched  = FALSE;
    CanMon_Initialized  = TRUE;
}

/* ==================================================================
 * Private: Apply safe state outputs
 * ================================================================== */

/**
 * @brief  Force safe state: 100% brake, center steering, continuous buzzer
 */
static void CanMon_ApplySafeState(void)
{
    /* Auto-brake 100% */
    (void)Rte_Write(FZC_SIG_BRAKE_CMD, (uint32)100u);

    /* Center steering */
    (void)Rte_Write(FZC_SIG_STEER_CMD, (uint32)((uint16)((sint16)0)));

    /* Continuous buzzer */
    (void)Rte_Write(FZC_SIG_BUZZER_PATTERN, (uint32)FZC_BUZZER_CONTINUOUS);

    /* Report CAN bus-off DTC */
    Dem_ReportErrorStatus(FZC_DTC_CAN_BUS_OFF, DEM_EVENT_STATUS_FAILED);
}

/* ==================================================================
 * API: Swc_FzcCanMonitor_Check (10ms cyclic)
 * ================================================================== */

void Swc_FzcCanMonitor_Check(void)
{
    Can_StateType canMode;
    uint8 tec;
    uint8 rec;

    if (CanMon_Initialized != TRUE) {
        return;
    }

    /* Once safe state is latched, maintain it forever (no recovery) */
    if (CanMon_SafeLatched == TRUE) {
        CanMon_ApplySafeState();
        return;
    }

    /* ---- Check 1: Bus-off via CAN controller mode ---- */
    canMode = Can_GetControllerMode(0u);
    if (canMode == CAN_CS_STOPPED) {
        CanMon_Status = FZC_CAN_BUS_OFF;
        CanMon_SafeLatched = TRUE;
        CanMon_ApplySafeState();
        return;
    }

    /* ---- Check 2: Silence detection ---- */
    CanMon_SilenceCount++;
    if (CanMon_SilenceCount >= FZC_CAN_SILENCE_CYCLES) {
        CanMon_Status = FZC_CAN_SILENCE;
        CanMon_SafeLatched = TRUE;
        CanMon_ApplySafeState();
        return;
    }

    /* ---- Check 3: Error warning sustained via error counters ---- */
    tec = 0u;
    rec = 0u;
    (void)Can_GetErrorCounters(0u, &tec, &rec);
    if ((tec >= 96u) || (rec >= 96u)) {
        CanMon_ErrWarnCount++;
        if (CanMon_ErrWarnCount >= FZC_CAN_ERR_WARN_CYCLES) {
            CanMon_Status = FZC_CAN_ERROR_WARNING;
            CanMon_SafeLatched = TRUE;
            CanMon_ApplySafeState();
            return;
        }
    } else {
        CanMon_ErrWarnCount = 0u;
    }

    /* No fault detected this cycle */
    CanMon_Status = FZC_CAN_OK;
}

/* ==================================================================
 * API: Swc_FzcCanMonitor_GetStatus
 * ================================================================== */

uint8 Swc_FzcCanMonitor_GetStatus(void)
{
    return CanMon_Status;
}

/* ==================================================================
 * API: Swc_FzcCanMonitor_NotifyRx
 * ================================================================== */

void Swc_FzcCanMonitor_NotifyRx(void)
{
    CanMon_SilenceCount = 0u;
}
