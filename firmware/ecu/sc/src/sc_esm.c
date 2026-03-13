/**
 * @file    sc_esm.c
 * @brief   ESM (Error Signaling Module) lockstep handler for Safety Controller
 * @date    2026-02-23
 *
 * @safety_req SWR-SC-014, SWR-SC-015
 * @traces_to  SSR-SC-014, SSR-SC-015
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_esm.h"
#include "Sc_Hw_Cfg.h"
#include "sc_gio.h"

/* ==================================================================
 * External: ESM hardware register access (HALCoGen-style)
 * ================================================================== */

extern void    esm_enable_group1_channel(uint8 channel);
extern void    esm_clear_flag(uint8 group, uint8 channel);
extern boolean esm_is_flag_set(uint8 group, uint8 channel);

/* ==================================================================
 * Constants
 * ================================================================== */

#define ESM_GROUP1              1u
#define ESM_CHANNEL_LOCKSTEP    2u     /* Lockstep compare error */

/* ==================================================================
 * Module State
 * ================================================================== */

/** ESM error active flag (latched) */
static boolean esm_error_active;

/* ==================================================================
 * Public API
 * ================================================================== */

void SC_ESM_Init(void)
{
    esm_error_active = FALSE;

    /* Enable ESM group 1 channel 2 for lockstep compare error */
    esm_enable_group1_channel(ESM_CHANNEL_LOCKSTEP);
}

void SC_ESM_HighLevelInterrupt(void)
{
    /* CRITICAL: This ISR must execute in < 100 clock cycles.
     * Immediately de-energize relay, set system LED, latch error. */

    /* De-energize relay (direct GIO write — no function call overhead) */
    gioSetBit(SC_GIO_PORT_A, SC_PIN_RELAY, 0u);

    /* System fault LED ON */
    gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_SYS, 1u);

    /* Latch error */
    esm_error_active = TRUE;

    /* Clear ESM flag to acknowledge */
    esm_clear_flag(ESM_GROUP1, ESM_CHANNEL_LOCKSTEP);

    /* Enter infinite loop — TPS3823 watchdog will reset the MCU
     * because we are no longer feeding WDI from the main loop */
    for (;;) {
        /* Intentional infinite loop — watchdog reset expected */
    }
}

boolean SC_ESM_IsErrorActive(void)
{
    return esm_error_active;
}
