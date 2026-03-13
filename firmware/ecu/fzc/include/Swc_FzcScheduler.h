/**
 * @file    Swc_FzcScheduler.h
 * @brief   FZC RTOS task configuration — 7 runnables with periods/priorities/WCET
 * @date    2026-02-24
 *
 * @safety_req SWR-FZC-029
 * @traces_to  SSR-FZC-029
 *
 * @details  Defines the FZC RTOS runnable table:
 *           - SteeringMonitor  10ms  High   ASIL D
 *           - BrakeMonitor     10ms  High   ASIL D
 *           - LidarMonitor     10ms  High   ASIL C
 *           - CanReceive       10ms  High   ASIL D
 *           - HeartbeatTx      50ms  Med    ASIL B
 *           - WatchdogFeed     100ms High   ASIL D
 *           - BuzzerDriver     10ms  Low    QM
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_FZC_SCHEDULER_H
#define SWC_FZC_SCHEDULER_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** Total number of runnables in the FZC scheduler */
#define FZC_SCHED_RUNNABLE_COUNT     7u

/** Priority levels (higher number = higher priority) */
#define FZC_SCHED_PRIO_LOW           1u
#define FZC_SCHED_PRIO_MED           2u
#define FZC_SCHED_PRIO_HIGH          3u

/** WCET budget utilization cap (80% of period) */
#define FZC_SCHED_WCET_UTIL_MAX_PCT  80u

/* ==================================================================
 * Runnable Configuration Type
 * ================================================================== */

/**
 * @brief  Runnable descriptor for the FZC scheduler
 */
typedef struct {
    const char* name;       /**< Human-readable name for traceability         */
    uint16      periodMs;   /**< Execution period in milliseconds             */
    uint8       priority;   /**< Priority level (FZC_SCHED_PRIO_xxx)          */
    uint16      wcetUs;     /**< Worst-case execution time budget (us)        */
    uint8       asilLevel;  /**< ASIL classification (0=QM,1=A,2=B,3=C,4=D)  */
} Swc_FzcScheduler_RunnableType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the FZC scheduler and register all runnables
 * @note   Must be called once at startup before RTOS start.
 */
void Swc_FzcScheduler_Init(void);

/**
 * @brief  Get the runnable table
 * @return Pointer to the array of runnable descriptors (FZC_SCHED_RUNNABLE_COUNT entries)
 */
const Swc_FzcScheduler_RunnableType* Swc_FzcScheduler_GetTable(void);

/**
 * @brief  Get the number of runnables
 * @return FZC_SCHED_RUNNABLE_COUNT
 */
uint8 Swc_FzcScheduler_GetCount(void);

#endif /* SWC_FZC_SCHEDULER_H */
