/**
 * @file    SchM.h
 * @brief   Schedule Manager — critical section abstraction
 * @date    2026-03-03
 *
 * @details Provides per-module exclusive area enter/exit macros.
 *          - POSIX/SIL (non-test): empty macros (cooperative = no preemption)
 *          - STM32:     __disable_irq() / __enable_irq() with nesting counter
 *          - UNIT_TEST: real implementation with mock IRQ state
 *
 * @safety_req SWR-BSW-041: Critical section protection for shared state
 * @traces_to  TSR-022, TSR-023
 *
 * @standard AUTOSAR_SWS_ScheduleManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SCHM_H
#define SCHM_H

#include "Std_Types.h"

/* ====================================================================
 * POSIX/SIL non-test build — no-op macros (cooperative, no ISR risk)
 * Unit tests and STM32 use the real nesting implementation.
 * ==================================================================== */

#if defined(PLATFORM_POSIX) && !defined(UNIT_TEST)

/* Com exclusive areas */
#define SchM_Enter_Com_COM_EXCLUSIVE_AREA_0()   ((void)0)
#define SchM_Exit_Com_COM_EXCLUSIVE_AREA_0()    ((void)0)

/* Rte exclusive areas */
#define SchM_Enter_Rte_RTE_EXCLUSIVE_AREA_0()   ((void)0)
#define SchM_Exit_Rte_RTE_EXCLUSIVE_AREA_0()    ((void)0)

/* Dem exclusive areas */
#define SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0()   ((void)0)
#define SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0()    ((void)0)

/* WdgM exclusive areas */
#define SchM_Enter_WdgM_WDGM_EXCLUSIVE_AREA_0() ((void)0)
#define SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0()  ((void)0)

/* Can exclusive areas */
#define SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0()   ((void)0)
#define SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0()    ((void)0)

#else /* STM32 or UNIT_TEST */

/**
 * @brief  Enter critical section (disable interrupts, nesting-safe)
 */
void SchM_Enter_Exclusive(void);

/**
 * @brief  Exit critical section (re-enable interrupts when nesting reaches 0)
 */
void SchM_Exit_Exclusive(void);

/**
 * @brief  Get current nesting depth (for testing/diagnostics)
 * @return Current nesting counter value
 */
uint8 SchM_GetNestingDepth(void);

/* All modules use the same underlying enter/exit */
#define SchM_Enter_Com_COM_EXCLUSIVE_AREA_0()    SchM_Enter_Exclusive()
#define SchM_Exit_Com_COM_EXCLUSIVE_AREA_0()     SchM_Exit_Exclusive()

#define SchM_Enter_Rte_RTE_EXCLUSIVE_AREA_0()    SchM_Enter_Exclusive()
#define SchM_Exit_Rte_RTE_EXCLUSIVE_AREA_0()     SchM_Exit_Exclusive()

#define SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0()    SchM_Enter_Exclusive()
#define SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0()     SchM_Exit_Exclusive()

#define SchM_Enter_WdgM_WDGM_EXCLUSIVE_AREA_0() SchM_Enter_Exclusive()
#define SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0()  SchM_Exit_Exclusive()

#define SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0()    SchM_Enter_Exclusive()
#define SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0()     SchM_Exit_Exclusive()

#endif /* PLATFORM_POSIX && !UNIT_TEST */

/* ---- Test Helpers ---- */
#if defined(UNIT_TEST)
boolean SchM_Test_IsIrqDisabled(void);
#endif

#endif /* SCHM_H */
