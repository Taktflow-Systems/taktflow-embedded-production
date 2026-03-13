/**
 * @file    Swc_TempMonitor.h
 * @brief   RZC temperature monitoring SWC — NTC readout, derating curve,
 *          hysteresis recovery, CAN broadcast
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-009, SWR-RZC-010, SWR-RZC-011
 * @traces_to  SSR-RZC-009, SSR-RZC-010, SSR-RZC-011
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_TEMPMONITOR_H
#define SWC_TEMPMONITOR_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

void Swc_TempMonitor_Init(void);
void Swc_TempMonitor_MainFunction(void);

#endif /* SWC_TEMPMONITOR_H */
