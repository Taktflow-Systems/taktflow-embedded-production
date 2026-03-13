/**
 * @file    sc_led.c
 * @brief   Fault LED panel driver for Safety Controller
 * @date    2026-02-23
 *
 * @safety_req SWR-SC-013
 * @traces_to  SSR-SC-013
 * @note    Safety level: QM
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_led.h"
#include "Sc_Hw_Cfg.h"
#include "sc_gio.h"

/* ==================================================================
 * LED Count and Pin Mapping
 * ================================================================== */

#define SC_LED_COUNT        4u
#define SC_LED_IDX_CVC      0u
#define SC_LED_IDX_FZC      1u
#define SC_LED_IDX_RZC      2u
#define SC_LED_IDX_SYS      3u

/** LED port lookup */
static const uint8 led_port[SC_LED_COUNT] = {
    SC_GIO_PORT_A, SC_GIO_PORT_A, SC_GIO_PORT_A, SC_GIO_PORT_A
};

/** LED pin lookup */
static const uint8 led_pin[SC_LED_COUNT] = {
    SC_PIN_LED_CVC, SC_PIN_LED_FZC, SC_PIN_LED_RZC, SC_PIN_LED_SYS
};

/* ==================================================================
 * Module State
 * ================================================================== */

/** Per-LED requested state */
static uint8 led_state[SC_LED_COUNT];

/** Blink tick counter (shared for all LEDs) */
static uint8 blink_counter;

/* ==================================================================
 * Public API
 * ================================================================== */

void SC_LED_Init(void)
{
    uint8 i;
    for (i = 0u; i < SC_LED_COUNT; i++) {
        led_state[i] = SC_LED_OFF;
        gioSetBit(led_port[i], led_pin[i], 0u);
    }
    blink_counter = 0u;
}

void SC_LED_Update(void)
{
    uint8 i;
    boolean blink_phase;

    /* Determine blink phase: ON for first half, OFF for second */
    blink_phase = (blink_counter < SC_LED_BLINK_ON_TICKS) ? TRUE : FALSE;

    /* Advance blink counter (after phase check) */
    blink_counter++;
    if (blink_counter >= SC_LED_BLINK_PERIOD) {
        blink_counter = 0u;
    }

    for (i = 0u; i < SC_LED_COUNT; i++) {
        if (led_state[i] == SC_LED_ON) {
            gioSetBit(led_port[i], led_pin[i], 1u);
        } else if (led_state[i] == SC_LED_BLINK) {
            gioSetBit(led_port[i], led_pin[i],
                      (blink_phase == TRUE) ? 1u : 0u);
        } else {
            gioSetBit(led_port[i], led_pin[i], 0u);
        }
    }
}

void SC_LED_SetState(uint8 ledIndex, uint8 state)
{
    if (ledIndex < SC_LED_COUNT) {
        if (state == SC_LED_BLINK) {
            /* Reset blink counter for fresh cycle */
            blink_counter = 0u;
        }
        led_state[ledIndex] = state;
    }
}
