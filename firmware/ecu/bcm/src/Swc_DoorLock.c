/**
 * @file    Swc_DoorLock.c
 * @brief   DoorLock SWC — manual and automatic door lock control
 * @date    2026-02-23
 *
 * @safety_req SWR-BCM-009
 * @traces_to  SSR-BCM-009
 *
 * @details  QM-rated body control SWC. Controls door lock state based on
 *           manual commands and automatic speed/state logic.
 *
 *           Manual lock/unlock: body_control_cmd byte 1 bit 0
 *             0 = unlock, 1 = lock
 *           Auto-lock: activates when vehicle_speed > BCM_AUTO_LOCK_SPEED (10)
 *           Auto-unlock: activates when vehicle transitions to parked state
 *             (vehicle_state <= BCM_VSTATE_READY) from non-parked state
 *           Previous state tracking for transition detection.
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#include "Swc_DoorLock.h"
#include "Bcm_Cfg.h"

/* ====================================================================
 * BSW includes
 * ==================================================================== */

#include "Rte.h"
#include "Dem.h"

/* ====================================================================
 * Internal constants
 * ==================================================================== */

/** Lock state values */
#define DOOR_UNLOCKED   0u
#define DOOR_LOCKED     1u

/* ====================================================================
 * Static module state
 * ==================================================================== */

static boolean initialized;
static uint32  lock_state;         /**< Current lock state: 0=unlocked, 1=locked */
static uint32  prev_vehicle_state; /**< Previous vehicle state for transition detection */

/* ====================================================================
 * Internal helper
 * ==================================================================== */

/**
 * @brief  Check if vehicle state represents parked condition
 * @param  state  Vehicle state value
 * @return TRUE if parked (INIT or READY), FALSE otherwise
 */
static boolean Is_Parked(uint32 state)
{
    return (state <= BCM_VSTATE_READY) ? TRUE : FALSE;
}

/* ====================================================================
 * Public functions
 * ==================================================================== */

/**
 * @brief  Initialize door lock SWC — doors unlocked
 */
void Swc_DoorLock_Init(void)
{
    lock_state         = DOOR_UNLOCKED;
    prev_vehicle_state = BCM_VSTATE_INIT;
    initialized        = TRUE;
}

/**
 * @brief  100ms cyclic — manual lock, auto-lock on speed, auto-unlock on park
 *
 * @details  Execution flow:
 *   1. Read body control command, vehicle speed, vehicle state from RTE
 *   2. Check manual lock command (byte 1 bit 0)
 *   3. Check auto-lock: speed > BCM_AUTO_LOCK_SPEED
 *   4. Check auto-unlock: transition from non-parked to parked state
 *   5. Update previous state for next cycle
 *   6. Write door lock state to RTE
 */
void Swc_DoorLock_100ms(void)
{
    uint32  body_cmd;
    uint32  vehicle_speed;
    uint32  vehicle_state;
    boolean manual_lock;
    boolean was_parked;
    boolean now_parked;

    if (initialized == FALSE) {
        return;
    }

    /* --- 1. Read input signals ---------------------------------------- */
    (void)Rte_Read(BCM_SIG_BODY_CONTROL_CMD_DOOR_LOCK_CMD, &body_cmd);
    (void)Rte_Read(BCM_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM, &vehicle_speed);
    (void)Rte_Read(BCM_SIG_VEHICLE_STATE_VEHICLE_STATE, &vehicle_state);

    /* --- 2. Manual lock command (discrete signal: 0=unlock, 1=lock) --- */
    manual_lock = (body_cmd != 0u) ? TRUE : FALSE;

    if (manual_lock == TRUE) {
        lock_state = DOOR_LOCKED;
    }

    /* --- 3. Auto-lock: speed > threshold ------------------------------ */
    if (vehicle_speed > BCM_AUTO_LOCK_SPEED) {
        lock_state = DOOR_LOCKED;
    }

    /* --- 4. Auto-unlock: transition to parked state ------------------- */
    was_parked = Is_Parked(prev_vehicle_state);
    now_parked = Is_Parked(vehicle_state);

    if ((was_parked == FALSE) && (now_parked == TRUE)) {
        lock_state = DOOR_UNLOCKED;
    }

    /* --- 5. Update previous state ------------------------------------- */
    prev_vehicle_state = vehicle_state;

    /* --- 6. Write output ---------------------------------------------- */
    (void)Rte_Write(BCM_SIG_DOOR_LOCK_STATUS_FRONT_LEFT_LOCK, lock_state);
}
