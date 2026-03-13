/**
 * @file    sc_led.h
 * @brief   Fault LED panel driver for Safety Controller
 * @date    2026-02-23
 *
 * @details 4 LEDs: GIO_A1 (CVC), GIO_A2 (FZC), GIO_A3 (RZC), GIO_B1 (system).
 *          Off = normal, blink 500ms = degraded, steady ON = fault/timeout.
 *
 * @safety_req SWR-SC-013
 * @traces_to  SSR-SC-013
 * @note    Safety level: QM
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_LED_H
#define SC_LED_H

#include "sc_types.h"

/** LED state enum */
#define SC_LED_OFF              0u
#define SC_LED_BLINK            1u
#define SC_LED_ON               2u

/**
 * @brief  Initialize LED module — all LEDs off
 */
void SC_LED_Init(void);

/**
 * @brief  10ms cyclic LED update — drives blink pattern
 */
void SC_LED_Update(void);

/**
 * @brief  Set LED state for a specific LED
 * @param  ledIndex  0=CVC, 1=FZC, 2=RZC, 3=system
 * @param  state     SC_LED_OFF, SC_LED_BLINK, or SC_LED_ON
 */
void SC_LED_SetState(uint8 ledIndex, uint8 state);

#endif /* SC_LED_H */
