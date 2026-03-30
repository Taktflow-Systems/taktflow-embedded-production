/**
 * @file    CanSM.c
 * @brief   CAN State Manager implementation
 * @date    2026-03-21
 *
 * @details Two-level bus-off recovery per AUTOSAR:
 *          L1 (fast): retry every L1_RecoveryTimeMs, up to L1_MaxAttempts
 *          L2 (slow): retry every L2_RecoveryTimeMs, up to L2_MaxAttempts
 *          If L2 exhausted: remain in BUS_OFF (permanent, needs power cycle)
 *
 * @safety_req SWR-BSW-026
 * @standard AUTOSAR_SWS_CANStateManager
 * @copyright Taktflow Systems 2026
 */
#include "CanSM.h"
#include "Can.h"
#include "Det.h"

/* ---- Internal State ---- */

static const CanSM_ConfigType* cansm_config = NULL_PTR;
static boolean      cansm_initialized = FALSE;
static CanSM_StateType cansm_state = CANSM_UNINIT;

/* Bus-off recovery state */
static uint8  cansm_recovery_level;     /* 1 = L1 (fast), 2 = L2 (slow) */
static uint8  cansm_recovery_attempt;
static uint16 cansm_recovery_timer;

/* Debug counters */
volatile uint32 g_dbg_cansm_busoff_count = 0u;
volatile uint32 g_dbg_cansm_recovery_count = 0u;

/* ---- Constants ---- */
#define CANSM_MAIN_PERIOD_MS  10u  /**< Must match scheduler call rate */

/* ---- API Implementation ---- */

void CanSM_Init(const CanSM_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        cansm_initialized = FALSE;
        cansm_state = CANSM_UNINIT;
        return;
    }

    cansm_config = ConfigPtr;
    cansm_state = CANSM_STOPPED;
    cansm_recovery_level = 0u;
    cansm_recovery_attempt = 0u;
    cansm_recovery_timer = 0u;
    cansm_initialized = TRUE;
    g_dbg_cansm_busoff_count = 0u;
    g_dbg_cansm_recovery_count = 0u;
}

Std_ReturnType CanSM_RequestComMode(void)
{
    if (cansm_initialized == FALSE) {
        return E_NOT_OK;
    }

    if (cansm_state == CANSM_STOPPED) {
        (void)Can_SetControllerMode(0u, CAN_CS_STARTED);
        cansm_state = CANSM_STARTED;
        return E_OK;
    }

    return (cansm_state == CANSM_STARTED) ? E_OK : E_NOT_OK;
}

void CanSM_ControllerBusOff(void)
{
    if (cansm_initialized == FALSE) {
        return;
    }

    g_dbg_cansm_busoff_count++;
    cansm_state = CANSM_BUS_OFF;

    /* Only reset to L1 if not already in recovery (first bus-off).
     * If re-entering from a failed recovery attempt, keep the attempt
     * counter to allow escalation from L1 → L2 → permanent. */
    if (cansm_recovery_level == 0u) {
        cansm_recovery_level = 1u;
        cansm_recovery_attempt = 0u;
    }
    cansm_recovery_timer = 0u;

    /* Stop controller immediately */
    (void)Can_SetControllerMode(0u, CAN_CS_STOPPED);
}

void CanSM_MainFunction(void)
{
    if ((cansm_initialized == FALSE) || (cansm_config == NULL_PTR)) {
        return;
    }

    if (cansm_state != CANSM_BUS_OFF) {
        return;
    }

    /* Advance recovery timer — (uint16) cast documents narrowing per MISRA Rule 10.3;
     * implicit widening of cansm_recovery_timer in the addition covered by global 10.3/10.7
     * suppression in tools/misra/suppressions.txt. */
    cansm_recovery_timer = (uint16)(cansm_recovery_timer + CANSM_MAIN_PERIOD_MS);

    if (cansm_recovery_level == 1u) {
        /* L1: fast recovery */
        if (cansm_recovery_timer >= cansm_config->L1_RecoveryTimeMs) {
            cansm_recovery_timer = 0u;
            cansm_recovery_attempt++;

            if (cansm_recovery_attempt <= cansm_config->L1_MaxAttempts) {
                /* Attempt recovery: restart controller */
                (void)Can_SetControllerMode(0u, CAN_CS_STARTED);
                cansm_state = CANSM_STARTED;
                g_dbg_cansm_recovery_count++;
                /* If bus-off recurs, CanSM_ControllerBusOff will be called again */
            } else {
                /* L1 exhausted → escalate to L2 (slow) */
                cansm_recovery_level = 2u;
                cansm_recovery_attempt = 0u;
                cansm_recovery_timer = 0u;
            }
        }
    } else if (cansm_recovery_level == 2u) {
        /* L2: slow recovery */
        if (cansm_recovery_timer >= cansm_config->L2_RecoveryTimeMs) {
            cansm_recovery_timer = 0u;
            cansm_recovery_attempt++;

            if (cansm_recovery_attempt <= cansm_config->L2_MaxAttempts) {
                (void)Can_SetControllerMode(0u, CAN_CS_STARTED);
                cansm_state = CANSM_STARTED;
                g_dbg_cansm_recovery_count++;
            } else {
                /* L2 exhausted → permanent bus-off, needs power cycle */
                cansm_state = CANSM_BUS_OFF;
                cansm_recovery_level = 0u;  /* Stop retrying */
            }
        }
    } else {
        /* Recovery exhausted — stay in BUS_OFF */
    }
}

CanSM_StateType CanSM_GetState(void)
{
    return cansm_state;
}

boolean CanSM_IsCommunicationAllowed(void)
{
    return (cansm_state == CANSM_STARTED) ? TRUE : FALSE;
}
