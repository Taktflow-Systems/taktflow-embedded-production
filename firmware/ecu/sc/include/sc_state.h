/**
 * @file    sc_state.h
 * @brief   Authoritative SC runtime state machine (GAP-SC-006)
 * @date    2026-03-09
 *
 * @details Single source of truth for SC operating state. All modules
 *          that need the current mode read it from here instead of
 *          re-deriving it from scattered flags.
 *
 *          Valid transitions:
 *            INIT → MONITORING   (startup self-test passed, relay energized)
 *            MONITORING → FAULT  (heartbeat/plausibility/E2E fault detected)
 *            MONITORING → KILL   (relay de-energized by any trigger)
 *            FAULT → KILL        (fault escalated to relay kill)
 *
 *          Invalid transitions are rejected and logged (fail-closed).
 *
 * @safety_req SWR-SC-025
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_STATE_H
#define SC_STATE_H

#include "sc_types.h"

/**
 * @brief  Initialize state machine to SC_STATE_INIT
 */
void SC_State_Init(void);

/**
 * @brief  Get current SC operating state
 * @return SC_STATE_INIT, SC_STATE_MONITORING, SC_STATE_FAULT, or SC_STATE_KILL
 */
uint8 SC_State_Get(void);

/**
 * @brief  Request a state transition
 *
 * Only valid transitions are accepted. Invalid transitions are rejected
 * (state unchanged) and return FALSE.
 *
 * @param  new_state  Target state (SC_STATE_*)
 * @return TRUE if transition accepted, FALSE if rejected
 */
boolean SC_State_Transition(uint8 new_state);

#endif /* SC_STATE_H */
