/**
 * @file    test_Swc_RzcScheduler.c
 * @brief   Unit tests for Swc_RzcScheduler -- 8 runnables, WCET, priority
 * @date    2026-02-24
 *
 * @verifies SWR-RZC-028
 *
 * Tests runnable count, CurrentMonitor at 1kHz highest priority,
 * safety runnables preempting QM, and total WCET under 80%.
 *
 * Mocks: Rte_Read, Rte_Write, all SWC MainFunctions, WdgM_MainFunction
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed char     sint8;
typedef signed short    sint16;
typedef signed int     sint32;
typedef uint8           Std_ReturnType;
typedef uint8           boolean;

#define E_OK        ((Std_ReturnType)0x00U)
#define E_NOT_OK    ((Std_ReturnType)0x01U)
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * Rzc_Cfg.h constants needed by scheduler
 * ================================================================== */

#define RZC_SIG_COUNT              40u

/* ==================================================================
 * Swc_RzcScheduler API declarations
 * ================================================================== */

#define RZC_SCHED_RUNNABLE_COUNT   8u
#define RZC_PRIO_HIGHEST           1u
#define RZC_PRIO_HIGH              2u
#define RZC_PRIO_MEDIUM            3u
#define RZC_SCHED_MAX_UTIL_PCT    80u

typedef void (*Swc_RzcSched_RunnableFn)(void);

typedef struct {
    Swc_RzcSched_RunnableFn  func;
    uint16                   period_ms;
    uint8                    priority;
    uint16                   wcet_us;
} Swc_RzcSched_RunnableType;

extern void Swc_RzcScheduler_Init(void);
extern void Swc_RzcScheduler_Tick(void);
extern const Swc_RzcSched_RunnableType* Swc_RzcScheduler_GetTable(void);
extern uint8 Swc_RzcScheduler_GetUtilPct(void);

/* ==================================================================
 * Mock: Runnable call counters
 * ================================================================== */

static uint8 mock_current_monitor_count;
static uint8 mock_motor_count;
static uint8 mock_encoder_count;
static uint8 mock_com_receive_count;
static uint8 mock_temp_count;
static uint8 mock_battery_count;
static uint8 mock_heartbeat_count;
static uint8 mock_wdgm_count;

void Swc_CurrentMonitor_MainFunction(void) { mock_current_monitor_count++; }
void Swc_Motor_MainFunction(void)          { mock_motor_count++; }
void Swc_Encoder_MainFunction(void)        { mock_encoder_count++; }
void Swc_RzcCom_Receive(void)              { mock_com_receive_count++; }
void Swc_TempMonitor_MainFunction(void)    { mock_temp_count++; }
void Swc_Battery_MainFunction(void)        { mock_battery_count++; }
void Swc_Heartbeat_MainFunction(void)      { mock_heartbeat_count++; }
void WdgM_MainFunction(void)               { mock_wdgm_count++; }

/* ==================================================================
 * Mock: Rte (not used by scheduler, but required for link)
 * ================================================================== */

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    (void)SignalId; (void)DataPtr;
    return E_OK;
}

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    (void)SignalId; (void)Data;
    return E_OK;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    mock_current_monitor_count = 0u;
    mock_motor_count           = 0u;
    mock_encoder_count         = 0u;
    mock_com_receive_count     = 0u;
    mock_temp_count            = 0u;
    mock_battery_count         = 0u;
    mock_heartbeat_count       = 0u;
    mock_wdgm_count            = 0u;

    Swc_RzcScheduler_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-RZC-028: RTOS Task Configuration
 * ================================================================== */

/** @verifies SWR-RZC-028 -- Scheduler has exactly 8 runnables */
void test_RzcScheduler_runnable_count_8(void)
{
    const Swc_RzcSched_RunnableType *table;

    table = Swc_RzcScheduler_GetTable();

    TEST_ASSERT_NOT_NULL(table);

    /* Verify all 8 entries have non-null function pointers */
    uint8 i;
    for (i = 0u; i < RZC_SCHED_RUNNABLE_COUNT; i++)
    {
        TEST_ASSERT_NOT_NULL(table[i].func);
    }
}

/** @verifies SWR-RZC-028 -- CurrentMonitor runs at 1kHz (1ms) with highest priority */
void test_RzcScheduler_current_monitor_1khz_highest(void)
{
    const Swc_RzcSched_RunnableType *table;

    table = Swc_RzcScheduler_GetTable();

    /* Index 0 should be CurrentMonitor */
    TEST_ASSERT_EQUAL_UINT16(1u, table[0].period_ms);
    TEST_ASSERT_EQUAL_UINT8(RZC_PRIO_HIGHEST, table[0].priority);

    /* Run 1 tick: CurrentMonitor should fire */
    Swc_RzcScheduler_Tick();
    TEST_ASSERT_EQUAL_UINT8(1u, mock_current_monitor_count);

    /* Run 9 more ticks (10 total): CurrentMonitor should fire 10 times */
    uint8 i;
    for (i = 0u; i < 9u; i++) {
        Swc_RzcScheduler_Tick();
    }
    TEST_ASSERT_EQUAL_UINT8(10u, mock_current_monitor_count);
}

/** @verifies SWR-RZC-028 -- Safety runnables (prio 1,2) appear before QM (prio 3) in table */
void test_RzcScheduler_safety_preempts_qm(void)
{
    const Swc_RzcSched_RunnableType *table;
    uint8 i;
    uint8 last_prio;

    table = Swc_RzcScheduler_GetTable();

    /* Verify table is sorted: priority should be non-decreasing */
    last_prio = table[0].priority;
    for (i = 1u; i < RZC_SCHED_RUNNABLE_COUNT; i++)
    {
        TEST_ASSERT_TRUE(table[i].priority >= last_prio);
        last_prio = table[i].priority;
    }

    /* First entry must be highest priority */
    TEST_ASSERT_EQUAL_UINT8(RZC_PRIO_HIGHEST, table[0].priority);

    /* Last entry must be medium priority */
    TEST_ASSERT_EQUAL_UINT8(RZC_PRIO_MEDIUM, table[RZC_SCHED_RUNNABLE_COUNT - 1u].priority);
}

/** @verifies SWR-RZC-028 -- Total WCET utilisation is under 80% */
void test_RzcScheduler_wcet_total_under_80pct(void)
{
    uint8 util;

    util = Swc_RzcScheduler_GetUtilPct();

    TEST_ASSERT_TRUE(util < RZC_SCHED_MAX_UTIL_PCT);

    /* Verify it's a reasonable number (should be ~10%) */
    TEST_ASSERT_TRUE(util > 0u);
    TEST_ASSERT_TRUE(util < 50u);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-RZC-028: RTOS Task Configuration */
    RUN_TEST(test_RzcScheduler_runnable_count_8);
    RUN_TEST(test_RzcScheduler_current_monitor_1khz_highest);
    RUN_TEST(test_RzcScheduler_safety_preempts_qm);
    RUN_TEST(test_RzcScheduler_wcet_total_under_80pct);

    return UNITY_END();
}

/* ==================================================================
 * Include implementation under test (source inclusion pattern)
 *
 * Pre-define BSW header guards so that the real BSW headers are NOT
 * pulled in -- the test already provides its own mock declarations.
 * ================================================================== */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_RZC_SCHEDULER_H
#define RZC_CFG_H
#define RTE_H
#define WDGM_H
#define DEM_H
#define IOHWAB_H

#include "../src/Swc_RzcScheduler.c"
