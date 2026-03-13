/**
 * @file    Swc_RzcScheduler.h
 * @brief   RZC RTOS task configuration -- 8 runnables, priority scheduling
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-028
 * @traces_to  SSR-RZC-028, TSR-047
 *
 * @details  Defines the 8-runnable task table for the RZC:
 *             CurrentMonitor  1ms   Highest(1)
 *             MotorControl    10ms  High(2)
 *             DirectionMonitor 10ms High(2)
 *             CanReceive      10ms  High(2)
 *             TempMonitor     100ms Med(3)
 *             BatteryMonitor  100ms Med(3)
 *             HeartbeatTx     50ms  Med(3)
 *             WatchdogFeed    100ms Med(3)
 *
 *           WCET budgets enforced; total utilisation must be < 80%.
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_RZC_SCHEDULER_H
#define SWC_RZC_SCHEDULER_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** Total number of runnables in the RZC scheduler */
#define RZC_SCHED_RUNNABLE_COUNT   8u

/** Priority levels (lower number = higher priority) */
#define RZC_PRIO_HIGHEST           1u
#define RZC_PRIO_HIGH              2u
#define RZC_PRIO_MEDIUM            3u

/** Maximum total CPU utilisation (percent) */
#define RZC_SCHED_MAX_UTIL_PCT    80u

/* ==================================================================
 * Types
 * ================================================================== */

/** Function pointer type for a runnable entry point */
typedef void (*Swc_RzcSched_RunnableFn)(void);

/** Runnable configuration entry */
typedef struct {
    Swc_RzcSched_RunnableFn  func;         /**< Runnable entry point       */
    uint16                   period_ms;    /**< Period in milliseconds     */
    uint8                    priority;     /**< Priority (1=highest)       */
    uint16                   wcet_us;      /**< WCET budget in microseconds*/
} Swc_RzcSched_RunnableType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the RZC scheduler
 * @note   Resets tick counters for all 8 runnables.
 */
void Swc_RzcScheduler_Init(void);

/**
 * @brief  1ms tick entry -- dispatches runnables by period and priority
 * @note   Called from 1ms SysTick. Iterates the runnable table in
 *         priority order, dispatching each runnable whose period has
 *         elapsed.
 *
 * @safety_req SWR-RZC-028
 */
void Swc_RzcScheduler_Tick(void);

/**
 * @brief  Get the runnable configuration table
 * @return Pointer to the first element of the 8-entry runnable table
 */
const Swc_RzcSched_RunnableType* Swc_RzcScheduler_GetTable(void);

/**
 * @brief  Compute total CPU utilisation (percent)
 * @return Sum of (WCET / period) for all runnables, in percent
 */
uint8 Swc_RzcScheduler_GetUtilPct(void);

#endif /* SWC_RZC_SCHEDULER_H */
