/**
 * @file    Swc_Lights.h
 * @brief   Lights SWC — headlamp and tail light control
 * @date    2026-02-23
 *
 * @safety_req SWR-BCM-003, SWR-BCM-004, SWR-BCM-005
 * @traces_to  SSR-BCM-003, SSR-BCM-004, SSR-BCM-005
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_LIGHTS_H
#define SWC_LIGHTS_H

#include "Std_Types.h"

/**
 * @brief  Initialize lights SWC — all outputs off
 */
void Swc_Lights_Init(void);

/**
 * @brief  10ms cyclic — evaluate auto/manual headlamp, tail follows headlamp
 */
void Swc_Lights_10ms(void);

#endif /* SWC_LIGHTS_H */
