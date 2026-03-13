/**
 * @file    sc_esm.h
 * @brief   ESM (Error Signaling Module) lockstep handler for Safety Controller
 * @date    2026-02-23
 *
 * @details Enables ESM group 1 channel 2 for lockstep CPU compare error.
 *          High-level ISR immediately de-energizes relay and enters safe state.
 *
 * @safety_req SWR-SC-014, SWR-SC-015
 * @traces_to  SSR-SC-014, SSR-SC-015
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_ESM_H
#define SC_ESM_H

#include "sc_types.h"

/**
 * @brief  Initialize ESM — enable lockstep compare error monitoring
 */
void SC_ESM_Init(void);

/**
 * @brief  ESM high-level interrupt handler
 *
 * Immediately de-energizes relay (GIO_A0 = LOW), sets latch,
 * drives system LED (GIO_B1 = HIGH), clears ESM flag.
 * Must execute in < 100 clock cycles.
 */
void SC_ESM_HighLevelInterrupt(void);

/**
 * @brief  Check if ESM error is currently active
 * @return TRUE if lockstep compare error detected
 */
boolean SC_ESM_IsErrorActive(void);

#endif /* SC_ESM_H */
