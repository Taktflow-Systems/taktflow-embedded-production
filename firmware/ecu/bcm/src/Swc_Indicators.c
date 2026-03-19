/**
 * @file    Swc_Indicators.c
 * @brief   Indicators SWC — turn signals and hazard lights with flash control
 * @date    2026-02-23
 *
 * @safety_req SWR-BCM-006, SWR-BCM-007, SWR-BCM-008
 * @traces_to  SSR-BCM-006, SSR-BCM-007, SSR-BCM-008
 *
 * @details  QM-rated body control SWC. Controls left/right turn indicators
 *           and hazard lights with a ~1.5Hz flash pattern (33 ticks on,
 *           33 ticks off at 10ms period).
 *
 *           Turn signal from body_control_cmd bits 1-2:
 *             0 = off, 1 = left, 2 = right
 *           Hazard from body_control_cmd bit 3 OR estop_active signal.
 *           Hazard overrides turn signal — both left and right flash together.
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#include "Swc_Indicators.h"
#include "Bcm_Cfg.h"

/* ====================================================================
 * BSW includes
 * ==================================================================== */

#include "Rte.h"
#include "Dem.h"

/* ====================================================================
 * Internal constants
 * ==================================================================== */

/** Total flash period (on + off ticks) */
#define INDICATOR_FLASH_PERIOD  (BCM_INDICATOR_FLASH_ON + BCM_INDICATOR_FLASH_OFF)

/** Turn signal command extraction from body_control_cmd bits 1-2 */
#define TURN_CMD_MASK           0x06u  /* Bits 1-2 */
#define TURN_CMD_SHIFT          1u
#define TURN_CMD_OFF            0u
#define TURN_CMD_LEFT           1u
#define TURN_CMD_RIGHT          2u

/** Hazard command extraction from body_control_cmd bit 3 */
#define HAZARD_CMD_MASK         0x08u  /* Bit 3 */

/* ====================================================================
 * Static module state
 * ==================================================================== */

static boolean initialized;
static uint8   flash_counter;    /**< Counts 10ms ticks within flash period */
static boolean flash_on;         /**< Current flash phase: TRUE = on, FALSE = off */

/* ====================================================================
 * Public functions
 * ==================================================================== */

/**
 * @brief  Initialize indicators SWC — all outputs off, flash counter reset
 */
void Swc_Indicators_Init(void)
{
    flash_counter = 0u;
    flash_on      = TRUE;  /* Start with ON phase */
    initialized   = TRUE;
}

/**
 * @brief  10ms cyclic — decode turn/hazard commands, manage flash timing
 *
 * @details  Execution flow:
 *   1. Read body control command and E-stop status from RTE
 *   2. Decode turn signal direction (bits 1-2) and hazard (bit 3 or E-stop)
 *   3. Advance flash counter: 33 ticks ON, 33 ticks OFF (~1.5Hz)
 *   4. If hazard active: both left and right flash together
 *   5. If turn signal active (and no hazard): only selected side flashes
 *   6. If no command: all indicators off, reset flash counter
 *   7. Write left, right, and hazard_active signals to RTE
 */
void Swc_Indicators_10ms(void)
{
    uint32  turn_signal_cmd;
    uint32  hazard_cmd;
    uint32  estop_active;
    uint8   turn_cmd;
    boolean hazard_requested;
    uint32  left_output;
    uint32  right_output;
    uint32  hazard_output;
    boolean any_active;

    if (initialized == FALSE) {
        return;
    }

    /* --- 1. Read input signals ---------------------------------------- */
    (void)Rte_Read(BCM_SIG_BODY_CONTROL_CMD_TURN_SIGNAL_CMD, &turn_signal_cmd);
    (void)Rte_Read(BCM_SIG_BODY_CONTROL_CMD_HAZARD_ACTIVE, &hazard_cmd);
    (void)Rte_Read(BCM_SIG_ESTOP_BROADCAST_ACTIVE, &estop_active);

    /* --- 2. Decode commands ------------------------------------------- */
    turn_cmd = (uint8)turn_signal_cmd;  /* 0=off, 1=left, 2=right */
    hazard_requested = ((hazard_cmd != 0u) ||
                        (estop_active != 0u)) ? TRUE : FALSE;

    /* Determine if any indicator is active */
    any_active = ((hazard_requested == TRUE) ||
                  (turn_cmd != TURN_CMD_OFF)) ? TRUE : FALSE;

    /* --- 3. Flash timing ---------------------------------------------- */
    if (any_active == TRUE) {
        flash_counter++;
        if (flash_on == TRUE) {
            if (flash_counter > BCM_INDICATOR_FLASH_ON) {
                flash_on = FALSE;
                flash_counter = 1u;
            }
        } else {
            if (flash_counter > BCM_INDICATOR_FLASH_OFF) {
                flash_on = TRUE;
                flash_counter = 1u;
            }
        }
    } else {
        /* No indicator active — reset flash state */
        flash_counter = 0u;
        flash_on      = TRUE;
    }

    /* --- 4. Determine outputs ----------------------------------------- */
    left_output   = 0u;
    right_output  = 0u;
    hazard_output = 0u;

    if (hazard_requested == TRUE) {
        /* Hazard overrides turn signal: both sides flash */
        hazard_output = 1u;
        if (flash_on == TRUE) {
            left_output  = 1u;
            right_output = 1u;
        }
    } else if (turn_cmd == TURN_CMD_LEFT) {
        if (flash_on == TRUE) {
            left_output = 1u;
        }
    } else if (turn_cmd == TURN_CMD_RIGHT) {
        if (flash_on == TRUE) {
            right_output = 1u;
        }
    } else {
        /* All off — already initialized to 0 */
    }

    /* --- 5. Write output signals -------------------------------------- */
    (void)Rte_Write(BCM_SIG_INDICATOR_STATE_LEFT_ON, left_output);
    (void)Rte_Write(BCM_SIG_INDICATOR_STATE_RIGHT_ON, right_output);
    (void)Rte_Write(BCM_SIG_INDICATOR_STATE_HAZARD_ACTIVE, hazard_output);
}
