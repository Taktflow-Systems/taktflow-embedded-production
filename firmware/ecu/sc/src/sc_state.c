/**
 * @file    sc_state.c
 * @brief   Authoritative SC runtime state machine (GAP-SC-006)
 * @date    2026-03-09
 *
 * @safety_req SWR-SC-025
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_state.h"
#include "Sc_Hw_Cfg.h"

/* ==================================================================
 * Module State
 * ================================================================== */

/** Authoritative runtime state — single variable, no derived logic */
static uint8 sc_state;

/* ==================================================================
 * Public API
 * ================================================================== */

void SC_State_Init(void)
{
    sc_state = SC_STATE_INIT;
}

uint8 SC_State_Get(void)
{
    return sc_state;
}

boolean SC_State_Transition(uint8 new_state)
{
    /* Validate transition against allowed edges */
    switch (sc_state) {
    case SC_STATE_INIT:
        if (new_state == SC_STATE_MONITORING) {
            sc_state = new_state;
            return TRUE;
        }
        break;

    case SC_STATE_MONITORING:
        if ((new_state == SC_STATE_FAULT) ||
            (new_state == SC_STATE_KILL)) {
            sc_state = new_state;
            return TRUE;
        }
        break;

    case SC_STATE_FAULT:
        if (new_state == SC_STATE_KILL) {
            sc_state = new_state;
            return TRUE;
        }
        break;

    case SC_STATE_KILL:
        /* Terminal state — no transitions out (power cycle only) */
        break;

    default:
        /* Unknown state — force to KILL (fail-closed) */
        sc_state = SC_STATE_KILL;
        break;
    }

    return FALSE;
}
