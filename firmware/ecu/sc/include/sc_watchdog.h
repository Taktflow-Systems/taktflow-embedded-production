/**
 * @file    sc_watchdog.h
 * @brief   External watchdog (TPS3823) feed control for Safety Controller
 * @date    2026-02-23
 *
 * @details Toggles GIO_A4 (WDI) only when ALL monitoring conditions are met.
 *          Conditions: monitoring executed, RAM OK, CAN OK, ESM OK, stack OK.
 *
 * @safety_req SWR-SC-022
 * @traces_to  SSR-SC-015
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_WATCHDOG_H
#define SC_WATCHDOG_H

#include "sc_types.h"

/**
 * @brief  Initialize watchdog module
 */
void SC_Watchdog_Init(void);

/**
 * @brief  Conditionally feed the external watchdog
 *
 * Toggles GIO_A4 only if allChecksOk is TRUE (all conditions met).
 * If FALSE, watchdog starves and TPS3823 will reset the MCU.
 *
 * @param  allChecksOk  TRUE if all monitoring conditions passed this cycle
 */
void SC_Watchdog_Feed(boolean allChecksOk);

#endif /* SC_WATCHDOG_H */
