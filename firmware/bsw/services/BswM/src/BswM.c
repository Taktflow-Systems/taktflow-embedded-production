/**
 * @file    BswM.c
 * @brief   BSW Mode Manager implementation
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-022
 * @traces_to  TSR-046, TSR-047, TSR-048
 *
 * Manages ECU operating modes with a forward-only state machine:
 * STARTUP -> RUN -> DEGRADED -> SAFE_STOP -> SHUTDOWN
 *
 * Each mode has configurable action callbacks executed cyclically
 * by BswM_MainFunction. Safety-critical transitions (to SAFE_STOP)
 * are always allowed from any operational state.
 *
 * @standard AUTOSAR_SWS_BSWModeManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "BswM.h"
#include "Det.h"

/* ---- Internal State ---- */

static const BswM_ConfigType*  bswm_config = NULL_PTR;
static boolean                 bswm_initialized = FALSE;
static BswM_ModeType           bswm_current_mode;

/* ---- Private Helpers ---- */

/**
 * @brief  Validate that a mode transition is allowed
 * @param  current    Current ECU mode
 * @param  requested  Requested target mode
 * @return TRUE if transition is valid, FALSE otherwise
 *
 * Transition rules (forward-only with safety exceptions):
 *   STARTUP  -> RUN (only valid transition from STARTUP)
 *   RUN      -> DEGRADED, SAFE_STOP, SHUTDOWN
 *   DEGRADED -> SAFE_STOP, SHUTDOWN
 *   SAFE_STOP -> SHUTDOWN (only valid transition from SAFE_STOP)
 *   SHUTDOWN -> (no transitions — terminal state)
 */
static boolean bswm_is_transition_valid(BswM_ModeType current, BswM_ModeType requested)
{
    /* Same mode — no-op, but allowed */
    if (current == requested) {
        return TRUE;
    }

    /* Terminal state — no exit */
    if (current == BSWM_SHUTDOWN) {
        return FALSE;
    }

    /* Forward-only: requested must be strictly greater than current */
    if ((uint8)requested <= (uint8)current) {
        return FALSE;
    }

    /* STARTUP can only go to RUN */
    if ((current == BSWM_STARTUP) && (requested != BSWM_RUN)) {
        return FALSE;
    }

    return TRUE;
}

/* ---- API Implementation ---- */

void BswM_Init(const BswM_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_BSWM, 0u, BSWM_API_INIT, DET_E_PARAM_POINTER);
        bswm_initialized = FALSE;
        bswm_config = NULL_PTR;
        /* Keep mode at STARTUP as safe default */
        bswm_current_mode = BSWM_STARTUP;
        return;
    }

    bswm_config = ConfigPtr;
    bswm_current_mode = BSWM_STARTUP;
    bswm_initialized = TRUE;
}

void BswM_MainFunction(void)
{
    uint8 i;

    if ((bswm_initialized == FALSE) || (bswm_config == NULL_PTR)) {
        return;
    }

    /* Execute all action callbacks matching the current mode */
    for (i = 0u; i < bswm_config->ActionCount; i++) {
        if ((bswm_config->ModeActions[i].Mode == bswm_current_mode) &&
            (bswm_config->ModeActions[i].Action != NULL_PTR)) {
            bswm_config->ModeActions[i].Action();
        }
    }
}

Std_ReturnType BswM_RequestMode(BswM_RequesterIdType RequesterId,
                                 BswM_ModeType RequestedMode)
{
    (void)RequesterId;  /* Reserved for future logging/arbitration */

    if ((bswm_initialized == FALSE) || (bswm_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_BSWM, 0u, BSWM_API_REQUEST_MODE, DET_E_UNINIT);
        return E_NOT_OK;
    }

    /* Validate requested mode is within enum range */
    if ((uint8)RequestedMode > (uint8)BSWM_SHUTDOWN) {
        Det_ReportError(DET_MODULE_BSWM, 0u, BSWM_API_REQUEST_MODE, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    /* Check transition validity */
    if (bswm_is_transition_valid(bswm_current_mode, RequestedMode) == FALSE) {
        Det_ReportError(DET_MODULE_BSWM, 0u, BSWM_API_REQUEST_MODE, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    bswm_current_mode = RequestedMode;

    return E_OK;
}

BswM_ModeType BswM_GetCurrentMode(void)
{
    return bswm_current_mode;
}
