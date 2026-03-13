/**
 * @file    Swc_RzcSafety.h
 * @brief   RZC local safety monitoring -- watchdog, fault aggregation, CAN bus loss
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-023, SWR-RZC-024
 * @traces_to  SSR-RZC-023, SSR-RZC-024, TSR-046
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_RZC_SAFETY_H
#define SWC_RZC_SAFETY_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

void Swc_RzcSafety_Init(void);
void Swc_RzcSafety_MainFunction(void);
uint8 Swc_RzcSafety_GetStatus(void);

/**
 * @brief  Notify that a CAN message was successfully received
 * @note   Called by the COM layer on every valid RX cycle.
 *         Resets the CAN silence counter to prevent false CAN-loss latch.
 */
void Swc_RzcSafety_NotifyCanRx(void);

#endif /* SWC_RZC_SAFETY_H */
