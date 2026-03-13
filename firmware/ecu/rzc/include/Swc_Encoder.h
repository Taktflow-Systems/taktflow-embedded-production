/**
 * @file    Swc_Encoder.h
 * @brief   RZC quadrature encoder SWC — speed measurement, stall detection,
 *          direction plausibility (ASIL C, TIM4)
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-012, SWR-RZC-013, SWR-RZC-014
 * @traces_to  SSR-RZC-012, SSR-RZC-013, SSR-RZC-014
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_ENCODER_H
#define SWC_ENCODER_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

void Swc_Encoder_Init(void);
void Swc_Encoder_MainFunction(void);

#endif /* SWC_ENCODER_H */
