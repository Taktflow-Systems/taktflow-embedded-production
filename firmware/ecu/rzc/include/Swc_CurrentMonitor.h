/**
 * @file    Swc_CurrentMonitor.h
 * @brief   RZC current monitor SWC — 1kHz ACS723 sampling, overcurrent, recovery
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-005, SWR-RZC-006, SWR-RZC-007, SWR-RZC-008
 * @traces_to  SSR-RZC-005, SSR-RZC-006, SSR-RZC-007, SSR-RZC-008
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_CURRENTMONITOR_H
#define SWC_CURRENTMONITOR_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

void Swc_CurrentMonitor_Init(void);
void Swc_CurrentMonitor_MainFunction(void);

#endif /* SWC_CURRENTMONITOR_H */
