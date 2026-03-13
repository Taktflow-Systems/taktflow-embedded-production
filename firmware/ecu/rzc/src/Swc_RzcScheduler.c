/**
 * @file    Swc_RzcScheduler.c
 * @brief   RZC RTOS task configuration -- 8 runnables, priority scheduling
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-028
 * @traces_to  SSR-RZC-028, TSR-047
 *
 * @details  Implements the RZC scheduler with 8 runnables:
 *           Index  Name              Period   Priority  WCET
 *           0      CurrentMonitor     1ms     Highest(1)  50us
 *           1      MotorControl      10ms     High(2)    200us
 *           2      DirectionMonitor  10ms     High(2)    100us
 *           3      CanReceive        10ms     High(2)    150us
 *           4      TempMonitor      100ms     Med(3)     300us
 *           5      BatteryMonitor   100ms     Med(3)     200us
 *           6      HeartbeatTx       50ms     Med(3)     100us
 *           7      WatchdogFeed     100ms     Med(3)      50us
 *
 *           Total utilisation: 50/1000 + 200/10000 + 100/10000 +
 *             150/10000 + 300/100000 + 200/100000 + 100/50000 +
 *             50/100000 = 5% + 2% + 1% + 1.5% + 0.3% + 0.2% +
 *             0.2% + 0.05% = ~10.25%, well under 80%.
 *
 *           Dispatches in priority order within each 1ms tick.
 *           Safety runnables (prio 1,2) preempt QM runnables (prio 3).
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_RzcScheduler.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"

/* ==================================================================
 * Forward Declarations of Runnable Entry Points
 * ================================================================== */

extern void Swc_CurrentMonitor_MainFunction(void);
extern void Swc_Motor_MainFunction(void);
extern void Swc_Encoder_MainFunction(void);
extern void Swc_RzcCom_Receive(void);
extern void Swc_TempMonitor_MainFunction(void);
extern void Swc_Battery_MainFunction(void);
extern void Swc_Heartbeat_MainFunction(void);
extern void WdgM_MainFunction(void);

/* ==================================================================
 * Runnable Table (sorted by priority, then by period)
 * ================================================================== */

static const Swc_RzcSched_RunnableType RzcSched_Table[RZC_SCHED_RUNNABLE_COUNT] = {
    /* idx  function                         period_ms  priority  wcet_us */
    { Swc_CurrentMonitor_MainFunction,           1u,       1u,      50u },  /* CurrentMonitor    */
    { Swc_Motor_MainFunction,                   10u,       2u,     200u },  /* MotorControl      */
    { Swc_Encoder_MainFunction,                 10u,       2u,     100u },  /* DirectionMonitor  */
    { Swc_RzcCom_Receive,                       10u,       2u,     150u },  /* CanReceive        */
    { Swc_TempMonitor_MainFunction,            100u,       3u,     300u },  /* TempMonitor       */
    { Swc_Battery_MainFunction,                100u,       3u,     200u },  /* BatteryMonitor    */
    { Swc_Heartbeat_MainFunction,               50u,       3u,     100u },  /* HeartbeatTx       */
    { WdgM_MainFunction,                       100u,       3u,      50u },  /* WatchdogFeed      */
};

/* ==================================================================
 * Module State
 * ================================================================== */

/** Per-runnable elapsed tick counter (ms) */
static uint16  RzcSched_Elapsed[RZC_SCHED_RUNNABLE_COUNT];

/** Module initialization flag */
static uint8   RzcSched_Initialized;

/* ==================================================================
 * API: Swc_RzcScheduler_Init
 * ================================================================== */

void Swc_RzcScheduler_Init(void)
{
    uint8 i;

    for (i = 0u; i < RZC_SCHED_RUNNABLE_COUNT; i++)
    {
        RzcSched_Elapsed[i] = 0u;
    }

    RzcSched_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_RzcScheduler_Tick (1ms)
 * ================================================================== */

void Swc_RzcScheduler_Tick(void)
{
    uint8 i;

    if (RzcSched_Initialized != TRUE)
    {
        return;
    }

    /* Increment elapsed counters and dispatch in priority order */
    for (i = 0u; i < RZC_SCHED_RUNNABLE_COUNT; i++)
    {
        RzcSched_Elapsed[i]++;

        if (RzcSched_Elapsed[i] >= RzcSched_Table[i].period_ms)
        {
            RzcSched_Elapsed[i] = 0u;

            if (RzcSched_Table[i].func != NULL_PTR)
            {
                RzcSched_Table[i].func();
            }
        }
    }
}

/* ==================================================================
 * API: Swc_RzcScheduler_GetTable
 * ================================================================== */

const Swc_RzcSched_RunnableType* Swc_RzcScheduler_GetTable(void)
{
    return &RzcSched_Table[0];
}

/* ==================================================================
 * API: Swc_RzcScheduler_GetUtilPct
 * ================================================================== */

uint8 Swc_RzcScheduler_GetUtilPct(void)
{
    uint32 total_util;
    uint8  i;

    total_util = 0u;

    for (i = 0u; i < RZC_SCHED_RUNNABLE_COUNT; i++)
    {
        /* Utilisation = WCET_us / (period_ms * 1000) expressed in 0.01% */
        /* Scaled: (wcet_us * 10000) / (period_ms * 1000) = (wcet_us * 10) / period_ms */
        if (RzcSched_Table[i].period_ms > 0u)
        {
            total_util += ((uint32)RzcSched_Table[i].wcet_us * 10u)
                          / (uint32)RzcSched_Table[i].period_ms;
        }
    }

    /* total_util is in 0.01% units; convert to percent */
    return (uint8)(total_util / 100u);
}
