/**
 * @file    Swc_VehicleState.h
 * @brief   Vehicle state machine SWC
 * @date    2026-02-21
 *
 * @details Manages the CVC vehicle state machine with 6 states (INIT, RUN,
 *          DEGRADED, LIMP, SAFE_STOP, SHUTDOWN) and 14 event types. Transitions
 *          are defined by a const lookup table. The MainFunction (10ms cyclic)
 *          reads fault signals from RTE, derives events, reports DTCs, and
 *          writes current state back to RTE.
 *
 * @safety_req SWR-CVC-009 to SWR-CVC-013
 * @traces_to  SSR-CVC-009 to SSR-CVC-013, TSR-046, TSR-047
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_VEHICLESTATE_H
#define SWC_VEHICLESTATE_H

#include "Std_Types.h"

/**
 * @brief  Initialize the vehicle state machine
 * @note   Sets state to CVC_STATE_INIT and clears internal flags.
 *         Must be called once at ECU startup before MainFunction.
 */
void Swc_VehicleState_Init(void);

/**
 * @brief  Cyclic main function — 10ms period
 * @note   Reads fault signals from RTE (pedal fault, E-stop, comm status,
 *         motor cutoff, brake fault, steering fault), derives events,
 *         calls OnEvent for state transitions, reports DTCs via Dem,
 *         and writes current state to RTE.
 */
void Swc_VehicleState_MainFunction(void);

/**
 * @brief  Get the current vehicle state
 * @return Current state (CVC_STATE_INIT..CVC_STATE_SHUTDOWN)
 */
uint8 Swc_VehicleState_GetState(void);

/**
 * @brief  Inject an external event into the state machine
 * @param  event  Event ID (CVC_EVT_SELF_TEST_PASS..CVC_EVT_STEERING_FAULT)
 * @note   Looks up transition_table[current_state][event]. If the result
 *         is not CVC_STATE_INVALID, transitions to the new state and
 *         notifies BswM_RequestMode with the corresponding BswM mode.
 *         Used for events that originate outside MainFunction (e.g.,
 *         self-test result, Safety Controller kill signal).
 */
void Swc_VehicleState_OnEvent(uint8 event);

#endif /* SWC_VEHICLESTATE_H */
