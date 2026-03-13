/**
 * @file    Swc_BcmMain.h
 * @brief   BCM main loop — 10ms cycle, CAN processing, periodic transmit
 * @date    2026-02-24
 *
 * @safety_req SWR-BCM-012
 * @traces_to  SSR-BCM-012
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_BCMMAIN_H
#define SWC_BCMMAIN_H

#include "Std_Types.h"

/**
 * @brief  BCM main loop tick — called every 10ms
 *
 * @safety_req SWR-BCM-012
 * @details  10ms cycle: CAN read, process inputs, transmit every 10th cycle
 *           (100ms body status). Logs warning on cycle overrun.
 */
void BCM_Main(void);

#endif /* SWC_BCMMAIN_H */
