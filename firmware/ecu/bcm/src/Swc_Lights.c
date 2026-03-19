/**
 * @file    Swc_Lights.c
 * @brief   Lights SWC — headlamp and tail light control
 * @date    2026-02-23
 *
 * @safety_req SWR-BCM-003, SWR-BCM-004, SWR-BCM-005
 * @traces_to  SSR-BCM-003, SSR-BCM-004, SSR-BCM-005
 *
 * @details  QM-rated body control SWC. Controls headlamp and tail lights
 *           based on vehicle state and speed. Supports manual override via
 *           body control command bit 0. Tail lights always follow headlamp.
 *
 *           Auto headlamp ON: vehicle_state == DRIVING AND speed > 0
 *           Manual override: body_control_cmd bit 0 forces lights ON
 *           Tail lights: mirror headlamp state
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#include "Swc_Lights.h"
#include "Bcm_Cfg.h"

/* ====================================================================
 * BSW includes
 * ==================================================================== */

#include "Rte.h"
#include "Dem.h"

/* ====================================================================
 * Static module state
 * ==================================================================== */

static boolean initialized;

/* ====================================================================
 * Public functions
 * ==================================================================== */

/**
 * @brief  Initialize lights SWC — all outputs off
 */
void Swc_Lights_Init(void)
{
    initialized = TRUE;
}

/**
 * @brief  10ms cyclic — auto headlamp, manual override, tail follows head
 *
 * @details  Execution flow:
 *   1. Read vehicle speed, vehicle state, body control command from RTE
 *   2. Determine headlamp state: ON if (DRIVING AND speed > 0) OR manual override
 *   3. Tail lights follow headlamp state
 *   4. Write headlamp and tail light states to RTE
 */
void Swc_Lights_10ms(void)
{
    uint32 vehicle_speed;
    uint32 vehicle_state;
    uint32 body_cmd;
    uint32 headlamp;
    uint32 manual_override;
    uint32 auto_on;

    if (initialized == FALSE) {
        return;
    }

    /* --- 1. Read input signals ---------------------------------------- */
    (void)Rte_Read(BCM_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM, &vehicle_speed);
    (void)Rte_Read(BCM_SIG_VEHICLE_STATE_MODE, &vehicle_state);
    (void)Rte_Read(BCM_SIG_BODY_CONTROL_CMD_HEADLIGHT_CMD, &body_cmd);

    /* --- 2. Determine headlamp state ---------------------------------- */

    /* Manual override: discrete headlight command signal (0 or 1) */
    manual_override = body_cmd;

    /* Auto headlamp: ON when driving and speed > 0 */
    if ((vehicle_state == BCM_VSTATE_DRIVING) && (vehicle_speed > 0u)) {
        auto_on = 1u;
    } else {
        auto_on = 0u;
    }

    /* Headlamp ON if auto condition met OR manual override */
    if ((auto_on != 0u) || (manual_override != 0u)) {
        headlamp = 1u;
    } else {
        headlamp = 0u;
    }

    /* --- 3. Write output signals -------------------------------------- */
    (void)Rte_Write(BCM_SIG_LIGHT_STATUS_HEADLIGHT_ON, headlamp);
    (void)Rte_Write(BCM_SIG_LIGHT_STATUS_TAIL_LIGHT_ON, headlamp);  /* Tail follows headlamp */
}
