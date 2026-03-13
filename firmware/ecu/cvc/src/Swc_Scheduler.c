/**
 * @file    Swc_Scheduler.c
 * @brief   RTOS task configuration SWC — runnable table with periods/priorities/WCET
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-032
 * @traces_to  SSR-CVC-032, TSR-046, TSR-047
 *
 * @details  Manages the CVC runnable configuration table containing:
 *           - Task name, period, priority, WCET budget
 *           - ASIL level per task
 *           - Safety-critical tasks (ASIL D) have higher priority than QM tasks
 *
 *           The default configuration table:
 *             ID 0: Swc_Pedal          10ms  prio 10  WCET 200us  ASIL D
 *             ID 1: Swc_VehicleState   10ms  prio 10  WCET 100us  ASIL D
 *             ID 2: Swc_EStop          10ms  prio 11  WCET  50us  ASIL D
 *             ID 3: Swc_Heartbeat      50ms  prio  8  WCET 150us  ASIL B
 *             ID 4: Swc_Dashboard     200ms  prio  3  WCET 500us  QM
 *             ID 5: Swc_CanMonitor     10ms  prio  9  WCET 100us  ASIL D
 *             ID 6: Swc_Watchdog      100ms  prio  9  WCET  50us  ASIL D
 *             ID 7: Swc_CvcCom         10ms  prio 10  WCET 200us  ASIL D
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR RTE pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Scheduler.h"

/* ==================================================================
 * Module State (all static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8                            Sched_Initialized;
static const Swc_Scheduler_ConfigType*  Sched_CfgPtr;

/* ==================================================================
 * API: Swc_Scheduler_Init
 * ================================================================== */

void Swc_Scheduler_Init(const Swc_Scheduler_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR)
    {
        Sched_Initialized = FALSE;
        Sched_CfgPtr      = NULL_PTR;
        return;
    }

    if (ConfigPtr->runnables == NULL_PTR)
    {
        Sched_Initialized = FALSE;
        Sched_CfgPtr      = NULL_PTR;
        return;
    }

    if (ConfigPtr->runnableCount == 0u)
    {
        Sched_Initialized = FALSE;
        Sched_CfgPtr      = NULL_PTR;
        return;
    }

    Sched_CfgPtr      = ConfigPtr;
    Sched_Initialized  = TRUE;
}

/* ==================================================================
 * API: Swc_Scheduler_GetConfig
 * ================================================================== */

const Swc_Scheduler_ConfigType* Swc_Scheduler_GetConfig(void)
{
    if (Sched_Initialized != TRUE)
    {
        return NULL_PTR;
    }

    return Sched_CfgPtr;
}

/* ==================================================================
 * API: Swc_Scheduler_GetRunnableCount
 * ================================================================== */

uint8 Swc_Scheduler_GetRunnableCount(void)
{
    if (Sched_Initialized != TRUE)
    {
        return 0u;
    }

    if (Sched_CfgPtr == NULL_PTR)
    {
        return 0u;
    }

    return Sched_CfgPtr->runnableCount;
}
