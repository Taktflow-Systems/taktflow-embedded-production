/**
 * @file    Os_TaskMap.h
 * @brief   COM/RTE → OSEK task mapping for Phase 5 integration
 * @date    2026-03-15
 *
 * @details Defines the mapping between BSW main functions and OSEK tasks
 *          or schedule table expiry points. Each ECU configures its own
 *          task IDs and periods via the generated Os_Cfg.
 *
 *          Typical mapping:
 *            Task_10ms  → Com_MainFunction_Tx, Com_MainFunction_Rx,
 *                         Rte_MainFunction, Can_MainFunction_Read
 *            Task_100ms → WdgM_MainFunction, Dem_MainFunction
 *
 *          On POSIX/SIL the main loop calls these directly.
 *          On STM32/TMS570, schedule table expiry points activate the
 *          mapped tasks at the configured period.
 *
 * @standard AUTOSAR OS §10, AUTOSAR SchM
 * @copyright Taktflow Systems 2026
 */
#ifndef OS_TASKMAP_H
#define OS_TASKMAP_H

#include "Os.h"

typedef void (*Os_BswMainFunctionType)(void);

typedef struct {
    const char* Name;
    Os_BswMainFunctionType MainFunction;
    TaskType MappedTask;
    TickType PeriodTicks;
} Os_TaskMapEntryType;

/**
 * @brief   Call all BSW main functions mapped to the given task.
 * @param   TaskID  The OSEK task that just ran.
 *
 * Iterates the task map and calls each BSW main function whose
 * MappedTask matches TaskID. This provides the SchM → OSEK bridge:
 * instead of SchM calling main functions directly, the OS task
 * entry function calls Os_TaskMap_RunMappedFunctions() to execute
 * all BSW processing assigned to that task.
 */
void Os_TaskMap_RunMappedFunctions(TaskType TaskID);

/**
 * @brief   Initialize the counter driver (Gpt → OSEK counter bridge).
 */
void Os_CounterDriver_Init(void);

/**
 * @brief   Process one system counter tick.
 * @return  TRUE if preemptive dispatch is needed.
 */
boolean Os_CounterDriver_Tick(void);

/**
 * @brief   Get current system counter value.
 */
TickType Os_CounterDriver_GetValue(void);

#endif /* OS_TASKMAP_H */
