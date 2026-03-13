/**
 * @file    Swc_EStop.h
 * @brief   Emergency stop detection and broadcast SWC
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-018 to SWR-CVC-020
 * @traces_to  SSR-CVC-018 to SSR-CVC-020, TSR-022, TSR-046
 *
 * @details  Reads E-stop button via IoHwAb, debounces the signal,
 *           latches activation (never clears), broadcasts on CAN
 *           with E2E protection, and reports DTC via Dem.
 *           10ms cyclic execution.
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (ASIL D)
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_ESTOP_H
#define SWC_ESTOP_H

#include "Std_Types.h"

/**
 * @brief  Initialise E-stop SWC — reset all internal state
 * @note   Must be called once before Swc_EStop_MainFunction
 */
void Swc_EStop_Init(void);

/**
 * @brief  E-stop cyclic main function — 10ms period
 * @note   Reads button, debounces, latches, broadcasts CAN
 */
void Swc_EStop_MainFunction(void);

/**
 * @brief  Query whether E-stop is currently active (latched)
 * @return TRUE if E-stop has been activated, FALSE otherwise
 */
boolean Swc_EStop_IsActive(void);

#endif /* SWC_ESTOP_H */
