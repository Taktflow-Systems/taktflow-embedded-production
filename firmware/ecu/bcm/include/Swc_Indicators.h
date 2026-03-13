/**
 * @file    Swc_Indicators.h
 * @brief   Indicators SWC — turn signals and hazard lights
 * @date    2026-02-23
 *
 * @safety_req SWR-BCM-006, SWR-BCM-007, SWR-BCM-008
 * @traces_to  SSR-BCM-006, SSR-BCM-007, SSR-BCM-008
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_INDICATORS_H
#define SWC_INDICATORS_H

#include "Std_Types.h"

/**
 * @brief  Initialize indicators SWC — all indicators off
 */
void Swc_Indicators_Init(void);

/**
 * @brief  10ms cyclic — turn signals and hazard flash logic
 */
void Swc_Indicators_10ms(void);

#endif /* SWC_INDICATORS_H */
