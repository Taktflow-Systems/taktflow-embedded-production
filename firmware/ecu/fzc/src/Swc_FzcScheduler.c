/**
 * @file    Swc_FzcScheduler.c
 * @brief   FZC RTOS task configuration — 7 runnables with periods/priorities/WCET
 * @date    2026-02-24
 *
 * @safety_req SWR-FZC-029
 * @traces_to  SSR-FZC-029
 *
 * @details  Defines and manages the FZC runnable table:
 *
 *           Index  Name              Period  Prio  WCET(us)  ASIL
 *           0      SteeringMonitor   10ms    High  800       D
 *           1      BrakeMonitor      10ms    High  600       D
 *           2      LidarMonitor      10ms    High  500       C
 *           3      CanReceive        10ms    High  400       D
 *           4      HeartbeatTx       50ms    Med   200       B
 *           5      WatchdogFeed      100ms   High  100       D
 *           6      BuzzerDriver      10ms    Low   300       QM
 *
 *           Total WCET for 10ms slot (all High + Low runnables):
 *           800 + 600 + 500 + 400 + 300 = 2600us = 26% of 10ms
 *           Well under 80% utilization cap.
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_FzcScheduler.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * ASIL Level Constants (for readability)
 * ================================================================== */

#define ASIL_QM     0u
#define ASIL_A      1u
#define ASIL_B      2u
#define ASIL_C      3u
#define ASIL_D      4u

/* ==================================================================
 * Runnable Table (const — read-only, no dynamic memory)
 * ================================================================== */

static const Swc_FzcScheduler_RunnableType FzcSched_Table[FZC_SCHED_RUNNABLE_COUNT] = {
    /* 0: SteeringMonitor — ASIL D, 10ms, High priority */
    {
        "SteeringMonitor",
        10u,                        /* periodMs  */
        FZC_SCHED_PRIO_HIGH,        /* priority  */
        800u,                        /* wcetUs    */
        ASIL_D                       /* asilLevel */
    },
    /* 1: BrakeMonitor — ASIL D, 10ms, High priority */
    {
        "BrakeMonitor",
        10u,
        FZC_SCHED_PRIO_HIGH,
        600u,
        ASIL_D
    },
    /* 2: LidarMonitor — ASIL C, 10ms, High priority */
    {
        "LidarMonitor",
        10u,
        FZC_SCHED_PRIO_HIGH,
        500u,
        ASIL_C
    },
    /* 3: CanReceive — ASIL D, 10ms, High priority */
    {
        "CanReceive",
        10u,
        FZC_SCHED_PRIO_HIGH,
        400u,
        ASIL_D
    },
    /* 4: HeartbeatTx — ASIL B, 50ms, Med priority */
    {
        "HeartbeatTx",
        50u,
        FZC_SCHED_PRIO_MED,
        200u,
        ASIL_B
    },
    /* 5: WatchdogFeed — ASIL D, 100ms, High priority */
    {
        "WatchdogFeed",
        100u,
        FZC_SCHED_PRIO_HIGH,
        100u,
        ASIL_D
    },
    /* 6: BuzzerDriver — QM, 10ms, Low priority */
    {
        "BuzzerDriver",
        10u,
        FZC_SCHED_PRIO_LOW,
        300u,
        ASIL_QM
    }
};

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8 FzcSched_Initialized;

/* ==================================================================
 * API: Swc_FzcScheduler_Init
 * ================================================================== */

void Swc_FzcScheduler_Init(void)
{
    FzcSched_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_FzcScheduler_GetTable
 * ================================================================== */

const Swc_FzcScheduler_RunnableType* Swc_FzcScheduler_GetTable(void)
{
    if (FzcSched_Initialized != TRUE) {
        return NULL_PTR;
    }

    return FzcSched_Table;
}

/* ==================================================================
 * API: Swc_FzcScheduler_GetCount
 * ================================================================== */

uint8 Swc_FzcScheduler_GetCount(void)
{
    return FZC_SCHED_RUNNABLE_COUNT;
}
