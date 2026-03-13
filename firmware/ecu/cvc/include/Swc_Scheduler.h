/**
 * @file    Swc_Scheduler.h
 * @brief   RTOS task configuration SWC — runnable table with periods/priorities/WCET
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-032
 * @traces_to  SSR-CVC-032, TSR-046, TSR-047
 *
 * @details  Defines the CVC runnable configuration table containing:
 *           - Task name, period, priority, WCET budget
 *           - ASIL level per task
 *           - Safety tasks preempt QM tasks
 *
 *           The scheduler configuration is compile-time const and drives
 *           the RTE dispatcher for time-partitioned execution.
 *
 * @standard AUTOSAR RTE pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_SCHEDULER_H
#define SWC_SCHEDULER_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

#define SCHED_MAX_RUNNABLES     16u

/** ASIL classification per task */
#define SCHED_ASIL_QM            0u
#define SCHED_ASIL_A             1u
#define SCHED_ASIL_B             2u
#define SCHED_ASIL_C             3u
#define SCHED_ASIL_D             4u

/* ==================================================================
 * Types
 * ================================================================== */

/** Runnable configuration entry */
typedef struct {
    uint8   runnableId;       /**< Unique runnable identifier          */
    uint16  periodMs;         /**< Execution period in milliseconds    */
    uint8   priority;         /**< Priority (higher = more important)  */
    uint16  wcetUs;           /**< Worst-case execution time budget us */
    uint8   asilLevel;        /**< ASIL classification                 */
} Swc_Scheduler_RunnableType;

/** Scheduler configuration */
typedef struct {
    const Swc_Scheduler_RunnableType*  runnables;     /**< Runnable table      */
    uint8                              runnableCount; /**< Number of runnables */
} Swc_Scheduler_ConfigType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the scheduler with configuration table
 * @param  ConfigPtr  Pointer to scheduler configuration (must not be NULL)
 */
void Swc_Scheduler_Init(const Swc_Scheduler_ConfigType* ConfigPtr);

/**
 * @brief  Get the scheduler configuration
 * @return Pointer to current configuration, or NULL_PTR if not initialized
 */
const Swc_Scheduler_ConfigType* Swc_Scheduler_GetConfig(void);

/**
 * @brief  Get the number of configured runnables
 * @return Runnable count, or 0 if not initialized
 */
uint8 Swc_Scheduler_GetRunnableCount(void);

#endif /* SWC_SCHEDULER_H */
