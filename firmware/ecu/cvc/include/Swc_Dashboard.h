/**
 * @file    Swc_Dashboard.h
 * @brief   OLED dashboard display SWC
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-027, SWR-CVC-028
 * @traces_to  SSR-CVC-027, SSR-CVC-028
 *
 * @note    QM safety level — display fault does not affect vehicle operation
 *
 * @copyright Taktflow Systems 2026
 * @standard  AUTOSAR, ISO 26262 Part 6
 */
#ifndef SWC_DASHBOARD_H
#define SWC_DASHBOARD_H

#include "Std_Types.h"

/* ==================================================================
 * Public API
 * ================================================================== */

/**
 * @brief  Initialize dashboard display
 *
 * Calls Ssd1306_Init and Ssd1306_Clear, resets internal counters.
 */
void Swc_Dashboard_Init(void);

/**
 * @brief  Dashboard cyclic main function — called every 10ms
 *
 * Internally gates display refresh to 200ms (every 20th call).
 * Reads vehicle state, speed, pedal position, and fault mask via RTE,
 * then renders 4 lines on the OLED:
 *   Line 0: "ST:<state>"
 *   Line 2: "SPD:<speed>"
 *   Line 4: "PED:<position>%"
 *   Line 6: "FLT:<mask>" or "FLT:OK"
 *
 * On persistent I2C failure (2+ consecutive refresh failures),
 * sets display_fault and reports DTC CVC_DTC_DISPLAY_COMM.
 * Once faulted, rendering is suppressed but MainFunction continues.
 */
void Swc_Dashboard_MainFunction(void);

#endif /* SWC_DASHBOARD_H */
