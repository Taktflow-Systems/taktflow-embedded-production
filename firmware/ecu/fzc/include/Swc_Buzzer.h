/**
 * @file    Swc_Buzzer.h
 * @brief   FZC buzzer warning patterns SWC
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-017, SWR-FZC-018
 * @traces_to  SSR-FZC-017, SSR-FZC-018, TSR-030
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_BUZZER_H
#define SWC_BUZZER_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

void Swc_Buzzer_Init(void);
void Swc_Buzzer_MainFunction(void);

#endif /* SWC_BUZZER_H */
