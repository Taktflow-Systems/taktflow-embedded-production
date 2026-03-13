/**
 * @file    test_Swc_FzcScheduler.c
 * @brief   Unit tests for Swc_FzcScheduler — RTOS task configuration
 * @date    2026-02-24
 *
 * @verifies SWR-FZC-029
 *
 * Tests runnable count (7), safety-critical priority ordering (ASIL D/C
 * runnables at High priority, above QM), and WCET total utilization
 * under 80% of the shortest period.
 *
 * No mocks needed — scheduler is a pure data table with accessors.
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed short    sint16;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * Scheduler Constants (from header)
 * ================================================================== */

#define FZC_SCHED_RUNNABLE_COUNT     7u
#define FZC_SCHED_PRIO_LOW           1u
#define FZC_SCHED_PRIO_MED           2u
#define FZC_SCHED_PRIO_HIGH          3u
#define FZC_SCHED_WCET_UTIL_MAX_PCT  80u

/* ASIL levels */
#define ASIL_QM     0u
#define ASIL_A      1u
#define ASIL_B      2u
#define ASIL_C      3u
#define ASIL_D      4u

/* ==================================================================
 * Scheduler types and API declarations
 * ================================================================== */

typedef struct {
    const char* name;
    uint16      periodMs;
    uint8       priority;
    uint16      wcetUs;
    uint8       asilLevel;
} Swc_FzcScheduler_RunnableType;

extern void Swc_FzcScheduler_Init(void);
extern const Swc_FzcScheduler_RunnableType* Swc_FzcScheduler_GetTable(void);
extern uint8 Swc_FzcScheduler_GetCount(void);

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    Swc_FzcScheduler_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-FZC-029: Runnable Count (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-029 — Exactly 7 runnables configured */
void test_FzcScheduler_runnable_count_7(void)
{
    uint8 count = Swc_FzcScheduler_GetCount();
    TEST_ASSERT_EQUAL_UINT8(FZC_SCHED_RUNNABLE_COUNT, count);
    TEST_ASSERT_EQUAL_UINT8(7u, count);

    /* Also verify table is accessible */
    const Swc_FzcScheduler_RunnableType* table = Swc_FzcScheduler_GetTable();
    TEST_ASSERT_NOT_NULL(table);
}

/* ==================================================================
 * SWR-FZC-029: Safety Priority Ordering (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-029 — All safety-critical (ASIL C/D) runnables have priority >= Med,
 *  and all ASIL D runnables have higher or equal priority than QM runnables */
void test_FzcScheduler_safety_priority_above_qm(void)
{
    const Swc_FzcScheduler_RunnableType* table;
    uint8 i;
    uint8 qm_max_prio;
    uint8 safety_min_prio;

    table = Swc_FzcScheduler_GetTable();
    TEST_ASSERT_NOT_NULL(table);

    /* Find maximum QM priority and minimum safety (ASIL C/D) priority */
    qm_max_prio     = 0u;
    safety_min_prio  = 0xFFu;

    for (i = 0u; i < FZC_SCHED_RUNNABLE_COUNT; i++) {
        if (table[i].asilLevel == ASIL_QM) {
            if (table[i].priority > qm_max_prio) {
                qm_max_prio = table[i].priority;
            }
        }
        if ((table[i].asilLevel == ASIL_C) || (table[i].asilLevel == ASIL_D)) {
            if (table[i].priority < safety_min_prio) {
                safety_min_prio = table[i].priority;
            }
        }
    }

    /* assert: all safety-critical runnables have higher priority than QM */
    TEST_ASSERT_TRUE(safety_min_prio > qm_max_prio);

    /* assert: QM runnables are Low priority */
    TEST_ASSERT_EQUAL_UINT8(FZC_SCHED_PRIO_LOW, qm_max_prio);

    /* assert: safety runnables are at least High */
    TEST_ASSERT_TRUE(safety_min_prio >= FZC_SCHED_PRIO_HIGH);
}

/* ==================================================================
 * SWR-FZC-029: WCET Budget Under 80% (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-029 — Total WCET for 10ms runnables under 80% of 10ms period */
void test_FzcScheduler_wcet_total_under_80pct(void)
{
    const Swc_FzcScheduler_RunnableType* table;
    uint8 i;
    uint32 total_wcet_10ms;
    uint32 period_us;
    uint32 utilization_pct;

    table = Swc_FzcScheduler_GetTable();
    TEST_ASSERT_NOT_NULL(table);

    /* Sum WCET of all runnables that execute in the 10ms slot.
     * Runnables with longer periods also execute in 10ms slots
     * but less frequently. For worst-case, assume ALL runnables
     * could coincide in the same 10ms slot (hyperperiod alignment). */
    total_wcet_10ms = 0u;
    for (i = 0u; i < FZC_SCHED_RUNNABLE_COUNT; i++) {
        total_wcet_10ms += (uint32)table[i].wcetUs;
    }

    /* Worst-case: all runnables in same 10ms slot */
    period_us = 10000u;  /* 10ms = 10000us */

    /* Utilization = (total_wcet / period) * 100 */
    utilization_pct = (total_wcet_10ms * 100u) / period_us;

    /* assert: under 80% cap */
    TEST_ASSERT_TRUE(utilization_pct < FZC_SCHED_WCET_UTIL_MAX_PCT);

    /* For reference: expected total ~2900us = 29% */
    TEST_ASSERT_TRUE(total_wcet_10ms < 8000u);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-FZC-029: RTOS Task Configuration */
    RUN_TEST(test_FzcScheduler_runnable_count_7);
    RUN_TEST(test_FzcScheduler_safety_priority_above_qm);
    RUN_TEST(test_FzcScheduler_wcet_total_under_80pct);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_FZC_SCHEDULER_H
#define FZC_CFG_H

#include "../src/Swc_FzcScheduler.c"
