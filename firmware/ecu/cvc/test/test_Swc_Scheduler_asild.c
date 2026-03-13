/**
 * @file    test_Swc_Scheduler.c
 * @brief   Unit tests for Swc_Scheduler — RTOS task configuration table
 * @date    2026-02-24
 *
 * @verifies SWR-CVC-032
 *
 * Tests: runnable count, priority ordering, WCET budgets within cycle,
 * safety tasks preempt QM tasks.
 *
 * Mocks: None required (pure configuration)
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * Scheduler Constants (mirrors header)
 * ================================================================== */

#define SCHED_MAX_RUNNABLES     16u
#define SCHED_ASIL_QM            0u
#define SCHED_ASIL_A             1u
#define SCHED_ASIL_B             2u
#define SCHED_ASIL_C             3u
#define SCHED_ASIL_D             4u

/* ==================================================================
 * Scheduler Types (mirrors header)
 * ================================================================== */

typedef struct {
    uint8   runnableId;
    uint16  periodMs;
    uint8   priority;
    uint16  wcetUs;
    uint8   asilLevel;
} Swc_Scheduler_RunnableType;

typedef struct {
    const Swc_Scheduler_RunnableType*  runnables;
    uint8                              runnableCount;
} Swc_Scheduler_ConfigType;

/* Swc_Scheduler API declarations */
extern void                            Swc_Scheduler_Init(const Swc_Scheduler_ConfigType* ConfigPtr);
extern const Swc_Scheduler_ConfigType* Swc_Scheduler_GetConfig(void);
extern uint8                           Swc_Scheduler_GetRunnableCount(void);

/* ==================================================================
 * Test Configuration Data
 * ================================================================== */

static const Swc_Scheduler_RunnableType test_runnables[] = {
    { 0u,  10u, 10u,  200u, SCHED_ASIL_D },  /* Swc_Pedal         */
    { 1u,  10u, 10u,  100u, SCHED_ASIL_D },  /* Swc_VehicleState  */
    { 2u,  10u, 11u,   50u, SCHED_ASIL_D },  /* Swc_EStop         */
    { 3u,  50u,  8u,  150u, SCHED_ASIL_B },  /* Swc_Heartbeat     */
    { 4u, 200u,  3u,  500u, SCHED_ASIL_QM},  /* Swc_Dashboard     */
    { 5u,  10u,  9u,  100u, SCHED_ASIL_D },  /* Swc_CanMonitor    */
    { 6u, 100u,  9u,   50u, SCHED_ASIL_D },  /* Swc_Watchdog      */
    { 7u,  10u, 10u,  200u, SCHED_ASIL_D },  /* Swc_CvcCom        */
};

#define TEST_RUNNABLE_COUNT   8u

static Swc_Scheduler_ConfigType test_config;

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    test_config.runnables     = test_runnables;
    test_config.runnableCount = TEST_RUNNABLE_COUNT;

    Swc_Scheduler_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-CVC-032: Scheduler Configuration Tests
 * ================================================================== */

/** @verifies SWR-CVC-032 — Correct number of runnables configured */
void test_Scheduler_runnable_count(void)
{
    uint8 count;

    count = Swc_Scheduler_GetRunnableCount();

    TEST_ASSERT_EQUAL_UINT8(TEST_RUNNABLE_COUNT, count);
}

/** @verifies SWR-CVC-032 — Safety tasks have higher priority than QM tasks */
void test_Scheduler_priority_ordering(void)
{
    const Swc_Scheduler_ConfigType* cfg;
    uint8 i;
    uint8 minSafetyPrio;
    uint8 maxQmPrio;

    cfg = Swc_Scheduler_GetConfig();
    TEST_ASSERT_NOT_NULL(cfg);

    minSafetyPrio = 0xFFu;
    maxQmPrio     = 0u;

    for (i = 0u; i < cfg->runnableCount; i++)
    {
        if (cfg->runnables[i].asilLevel >= SCHED_ASIL_B)
        {
            /* Safety task */
            if (cfg->runnables[i].priority < minSafetyPrio)
            {
                minSafetyPrio = cfg->runnables[i].priority;
            }
        }
        else
        {
            /* QM task */
            if (cfg->runnables[i].priority > maxQmPrio)
            {
                maxQmPrio = cfg->runnables[i].priority;
            }
        }
    }

    /* Every safety task priority should be > every QM task priority */
    TEST_ASSERT_TRUE(minSafetyPrio > maxQmPrio);
}

/** @verifies SWR-CVC-032 — Total WCET budget fits within the shortest cycle */
void test_Scheduler_wcet_budgets_within_cycle(void)
{
    const Swc_Scheduler_ConfigType* cfg;
    uint8  i;
    uint32 totalWcetUs;
    uint16 shortestPeriodMs;
    uint32 shortestPeriodUs;

    cfg = Swc_Scheduler_GetConfig();
    TEST_ASSERT_NOT_NULL(cfg);

    totalWcetUs       = 0u;
    shortestPeriodMs  = 0xFFFFu;

    for (i = 0u; i < cfg->runnableCount; i++)
    {
        totalWcetUs += (uint32)cfg->runnables[i].wcetUs;

        if (cfg->runnables[i].periodMs < shortestPeriodMs)
        {
            shortestPeriodMs = cfg->runnables[i].periodMs;
        }
    }

    shortestPeriodUs = (uint32)shortestPeriodMs * 1000u;

    /* Total worst-case execution must fit within one shortest cycle
     * This is a simplified check; real scheduling analysis would be
     * more sophisticated (response time analysis). */
    TEST_ASSERT_TRUE(totalWcetUs < shortestPeriodUs);
}

/** @verifies SWR-CVC-032 — Safety tasks (ASIL D) can preempt QM tasks */
void test_Scheduler_safety_preempts_qm(void)
{
    const Swc_Scheduler_ConfigType* cfg;
    uint8 i;
    uint8 dashboardPrio;
    uint8 eStopPrio;

    cfg = Swc_Scheduler_GetConfig();
    TEST_ASSERT_NOT_NULL(cfg);

    dashboardPrio = 0u;
    eStopPrio     = 0u;

    for (i = 0u; i < cfg->runnableCount; i++)
    {
        if (cfg->runnables[i].runnableId == 4u)  /* Dashboard (QM) */
        {
            dashboardPrio = cfg->runnables[i].priority;
        }
        if (cfg->runnables[i].runnableId == 2u)  /* EStop (ASIL D) */
        {
            eStopPrio = cfg->runnables[i].priority;
        }
    }

    /* E-stop (ASIL D) must have higher priority than Dashboard (QM) */
    TEST_ASSERT_TRUE(eStopPrio > dashboardPrio);
}

/** @verifies SWR-CVC-032 — NULL config rejects initialization */
void test_Scheduler_null_config_rejects(void)
{
    Swc_Scheduler_Init(NULL_PTR);

    TEST_ASSERT_EQUAL_UINT8(0u, Swc_Scheduler_GetRunnableCount());
    TEST_ASSERT_NULL(Swc_Scheduler_GetConfig());
}

/** @verifies SWR-CVC-032 — GetConfig returns the configured table */
void test_Scheduler_get_config_returns_table(void)
{
    const Swc_Scheduler_ConfigType* cfg;

    cfg = Swc_Scheduler_GetConfig();

    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_UINT8(TEST_RUNNABLE_COUNT, cfg->runnableCount);
    TEST_ASSERT_EQUAL_UINT16(10u, cfg->runnables[0].periodMs);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Scheduler_runnable_count);
    RUN_TEST(test_Scheduler_priority_ordering);
    RUN_TEST(test_Scheduler_wcet_budgets_within_cycle);
    RUN_TEST(test_Scheduler_safety_preempts_qm);
    RUN_TEST(test_Scheduler_null_config_rejects);
    RUN_TEST(test_Scheduler_get_config_returns_table);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_SCHEDULER_H

#include "../src/Swc_Scheduler.c"
