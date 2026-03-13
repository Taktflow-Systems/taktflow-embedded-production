/**
 * @file    Swc_DoorLock.h
 * @brief   DoorLock SWC — manual and automatic door lock control
 * @date    2026-02-23
 *
 * @safety_req SWR-BCM-009
 * @traces_to  SSR-BCM-009
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_DOORLOCK_H
#define SWC_DOORLOCK_H

#include "Std_Types.h"

/**
 * @brief  Initialize door lock SWC — doors unlocked
 */
void Swc_DoorLock_Init(void);

/**
 * @brief  100ms cyclic — manual lock, auto-lock on speed, auto-unlock on park
 */
void Swc_DoorLock_100ms(void);

#endif /* SWC_DOORLOCK_H */
