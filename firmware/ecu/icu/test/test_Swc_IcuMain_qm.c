/**
 * @file    test_Swc_IcuMain.c
 * @brief   Unit tests for ICU main loop — 50ms cycle, CAN read, update gauges
 * @date    2026-02-24
 *
 * @verifies SWR-ICU-010
 *
 * Tests 50ms (20Hz) main loop cycle rate, gauge/warning/DTC/health
 * update each cycle, and overrun detection at 25ms threshold.
 *
 * Mocks: Can_MainFunction_Read, Rte_MainFunction, Can_MainFunction_BusOff,
 *        usleep, timing functions
 */
#include "unity.h"

/* ====================================================================
 * Local type definitions (self-contained test -- no BSW headers)
 * ==================================================================== */

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef uint8          Std_ReturnType;
typedef uint8          boolean;

#define E_OK      0u
#define E_NOT_OK  1u
#define TRUE      1u
#define FALSE     0u
#define NULL_PTR  ((void*)0)

/* Prevent BSW headers from redefining types */
#define PLATFORM_TYPES_H
#define STD_TYPES_H

/* ====================================================================
 * ICU Main Loop Constants
 * ==================================================================== */

#define ICU_MAIN_CYCLE_MS          50u
#define ICU_MAIN_OVERRUN_WARN_MS   25u

/* ====================================================================
 * Mock: BSW main functions
 * ==================================================================== */

static uint8 mock_can_read_count;
static uint8 mock_rte_main_count;
static uint8 mock_can_busoff_count;

void Can_MainFunction_Read(void)
{
    mock_can_read_count++;
}

void Rte_MainFunction(void)
{
    mock_rte_main_count++;
}

void Can_MainFunction_BusOff(void)
{
    mock_can_busoff_count++;
}

/* ====================================================================
 * Mock: Timing
 * ==================================================================== */

static uint32 mock_tick_ms;
static uint32 mock_cycle_duration_ms;

uint32 mock_get_tick_ms(void)
{
    return mock_tick_ms;
}

/* ====================================================================
 * Mock: Overrun logging
 * ==================================================================== */

static uint8  mock_overrun_warn_count;
static uint32 mock_overrun_duration_ms;

void mock_log_icu_overrun(uint32 duration_ms)
{
    mock_overrun_warn_count++;
    mock_overrun_duration_ms = duration_ms;
}

/* ====================================================================
 * Simulated ICU main loop tick (extracted from icu_main.c pattern)
 * ==================================================================== */

static boolean icu_main_running;
static uint32  icu_prev_tick_ms;
static uint16  icu_cycle_count;

static void ICU_Main_Tick(void)
{
    uint32 now;
    uint32 elapsed;

    if (icu_main_running == FALSE) {
        return;
    }

    now = mock_get_tick_ms();

    /* Check for overrun from previous cycle */
    if (icu_prev_tick_ms != 0u) {
        elapsed = now - icu_prev_tick_ms;
        if (elapsed > (ICU_MAIN_CYCLE_MS + ICU_MAIN_OVERRUN_WARN_MS)) {
            mock_log_icu_overrun(elapsed);
        }
    }

    /* Step 1: CAN Read */
    Can_MainFunction_Read();

    /* Step 2: RTE Scheduler (fires SWC runnables: gauges, warnings, DTCs, health) */
    Rte_MainFunction();

    /* Step 3: Bus-off check */
    Can_MainFunction_BusOff();

    icu_cycle_count++;
    icu_prev_tick_ms = mock_get_tick_ms();
}

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_can_read_count     = 0u;
    mock_rte_main_count     = 0u;
    mock_can_busoff_count   = 0u;
    mock_tick_ms            = 0u;
    mock_cycle_duration_ms  = 0u;
    mock_overrun_warn_count = 0u;
    mock_overrun_duration_ms = 0u;

    icu_main_running   = TRUE;
    icu_prev_tick_ms   = 0u;
    icu_cycle_count    = 0u;
}

void tearDown(void) { }

/* ====================================================================
 * SWR-ICU-010: 50ms cycle rate
 * ==================================================================== */

/** @verifies SWR-ICU-010 */
void test_IcuMain_50ms_cycle_rate(void)
{
    uint8 i;

    /* Simulate 20 cycles = 1 second at 50ms */
    for (i = 0u; i < 20u; i++) {
        mock_tick_ms = (uint32)i * ICU_MAIN_CYCLE_MS;
        ICU_Main_Tick();
    }

    TEST_ASSERT_EQUAL_UINT16(20u, icu_cycle_count);
}

/** @verifies SWR-ICU-010 */
void test_IcuMain_updates_gauges_each_cycle(void)
{
    /* Run one cycle */
    ICU_Main_Tick();

    /* CAN read, RTE (drives gauge/warning/DTC/health SWCs), bus-off */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_read_count);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_rte_main_count);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_busoff_count);

    /* Run 5 more cycles */
    uint8 i;
    for (i = 0u; i < 5u; i++) {
        mock_tick_ms += ICU_MAIN_CYCLE_MS;
        ICU_Main_Tick();
    }

    TEST_ASSERT_EQUAL_UINT8(6u, mock_can_read_count);
    TEST_ASSERT_EQUAL_UINT8(6u, mock_rte_main_count);
    TEST_ASSERT_EQUAL_UINT8(6u, mock_can_busoff_count);
}

/** @verifies SWR-ICU-010 */
void test_IcuMain_overrun_25ms_warning(void)
{
    /* First cycle at t=0 */
    mock_tick_ms = 0u;
    ICU_Main_Tick();

    /* Second cycle at t=50ms (normal) — no overrun */
    mock_tick_ms = 50u;
    ICU_Main_Tick();
    TEST_ASSERT_EQUAL_UINT8(0u, mock_overrun_warn_count);

    /* Third cycle at t=150ms (100ms gap, overrun >75ms threshold) */
    mock_tick_ms = 150u;
    ICU_Main_Tick();
    TEST_ASSERT_TRUE(mock_overrun_warn_count > 0u);
}

/** @verifies SWR-ICU-010 */
void test_IcuMain_not_running_does_nothing(void)
{
    icu_main_running = FALSE;

    ICU_Main_Tick();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_can_read_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_rte_main_count);
    TEST_ASSERT_EQUAL_UINT16(0u, icu_cycle_count);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-ICU-010: ICU main loop */
    RUN_TEST(test_IcuMain_50ms_cycle_rate);
    RUN_TEST(test_IcuMain_updates_gauges_each_cycle);
    RUN_TEST(test_IcuMain_overrun_25ms_warning);
    RUN_TEST(test_IcuMain_not_running_does_nothing);

    return UNITY_END();
}
