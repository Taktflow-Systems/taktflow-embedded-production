/**
 * @file    Swc_Battery.h
 * @brief   RZC battery voltage monitoring SWC -- QM classification
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-017, SWR-RZC-018
 * @traces_to  SSR-RZC-017, SSR-RZC-018, TSR-038
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_BATTERY_H
#define SWC_BATTERY_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

void Swc_Battery_Init(void);
void Swc_Battery_MainFunction(void);

#endif /* SWC_BATTERY_H */
