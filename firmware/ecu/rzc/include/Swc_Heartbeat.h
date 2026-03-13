/**
 * @file    Swc_Heartbeat.h
 * @brief   RZC heartbeat SWC -- alive counter, fault bitmask, CAN TX
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-021, SWR-RZC-022
 * @traces_to  SSR-RZC-021, SSR-RZC-022, TSR-022
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_HEARTBEAT_H
#define SWC_HEARTBEAT_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

void Swc_Heartbeat_Init(void);
void Swc_Heartbeat_MainFunction(void);

#endif /* SWC_HEARTBEAT_H */
