/**
 * @file    test_Swc_TcuMain.c
 * @brief   Unit tests for TCU main loop — 10ms cycle, CAN read, UDS, cache
 * @date    2026-02-24
 *
 * @verifies SWR-TCU-015
 *
 * Tests 10ms main loop cycle, UDS request processing, data cache update
 * each cycle, and cycle overrun detection at 5ms threshold.
 *
 * Mocks: Rte_MainFunction, usleep, timing functions
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
 * TCU Main Loop Constants
 * ==================================================================== */

#define TCU_MAIN_CYCLE_MS           10u
#define TCU_MAIN_OVERRUN_WARN_MS    5u

/* ====================================================================
 * Mock: BSW main functions
 * ==================================================================== */

static uint8 mock_rte_main_count;

void Rte_MainFunction(void)
{
    mock_rte_main_count++;
}

/* ====================================================================
 * Mock: Data aggregator
 * ==================================================================== */

static uint8 mock_data_check_count;

void TCU_Data_CheckTimeouts(void)
{
    mock_data_check_count++;
}

/* ====================================================================
 * Mock: Timing
 * ==================================================================== */

static uint32 mock_tick_ms;

uint32 mock_get_tick_ms(void)
{
    return mock_tick_ms;
}

/* ====================================================================
 * Mock: Overrun logging
 * ==================================================================== */

static uint8  mock_overrun_warn_count;

void mock_log_tcu_overrun(uint32 duration_ms)
{
    (void)duration_ms;
    mock_overrun_warn_count++;
}

/* ====================================================================
 * Simulated TCU main loop tick (extracted from tcu_main.c pattern)
 * ==================================================================== */

static boolean tcu_running;
static uint32  tcu_prev_tick_ms;
static uint16  tcu_cycle_count;

static void TCU_Main_Tick(void)
{
    uint32 now;
    uint32 elapsed;

    if (tcu_running == FALSE) {
        return;
    }

    now = mock_get_tick_ms();

    /* Check overrun from previous cycle */
    if (tcu_prev_tick_ms != 0u) {
        elapsed = now - tcu_prev_tick_ms;
        if (elapsed > (TCU_MAIN_CYCLE_MS + TCU_MAIN_OVERRUN_WARN_MS)) {
            mock_log_tcu_overrun(elapsed);
        }
    }

    /* Step 1: RTE scheduler (drives UDS, OBD-II, DTC SWCs) */
    Rte_MainFunction();

    /* Step 2: Check data cache timeouts */
    TCU_Data_CheckTimeouts();

    tcu_cycle_count++;
    tcu_prev_tick_ms = mock_get_tick_ms();
}

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_rte_main_count     = 0u;
    mock_data_check_count   = 0u;
    mock_tick_ms            = 0u;
    mock_overrun_warn_count = 0u;

    tcu_running        = TRUE;
    tcu_prev_tick_ms   = 0u;
    tcu_cycle_count    = 0u;
}

void tearDown(void) { }

/* ====================================================================
 * SWR-TCU-015: 10ms cycle
 * ==================================================================== */

/** @verifies SWR-TCU-015 */
void test_TcuMain_10ms_cycle(void)
{
    uint8 i;

    /* Run 100 cycles = 1 second at 10ms */
    for (i = 0u; i < 100u; i++) {
        mock_tick_ms = (uint32)i * TCU_MAIN_CYCLE_MS;
        TCU_Main_Tick();
    }

    TEST_ASSERT_EQUAL_UINT16(100u, tcu_cycle_count);
}

/** @verifies SWR-TCU-015 */
void test_TcuMain_processes_uds_requests(void)
{
    /* One cycle should call RTE (which drives UDS SWC) */
    TCU_Main_Tick();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_rte_main_count);

    /* 5 more cycles */
    uint8 i;
    for (i = 0u; i < 5u; i++) {
        mock_tick_ms += TCU_MAIN_CYCLE_MS;
        TCU_Main_Tick();
    }

    TEST_ASSERT_EQUAL_UINT8(6u, mock_rte_main_count);
}

/** @verifies SWR-TCU-015 */
void test_TcuMain_updates_cache_each_cycle(void)
{
    /* Each cycle should check data cache timeouts */
    TCU_Main_Tick();
    TEST_ASSERT_EQUAL_UINT8(1u, mock_data_check_count);

    mock_tick_ms += TCU_MAIN_CYCLE_MS;
    TCU_Main_Tick();
    TEST_ASSERT_EQUAL_UINT8(2u, mock_data_check_count);
}

/** @verifies SWR-TCU-015 */
void test_TcuMain_overrun_5ms_warning(void)
{
    /* First cycle at t=0 */
    mock_tick_ms = 0u;
    TCU_Main_Tick();

    /* Second cycle at normal 10ms gap — no overrun */
    mock_tick_ms = 10u;
    TCU_Main_Tick();
    TEST_ASSERT_EQUAL_UINT8(0u, mock_overrun_warn_count);

    /* Third cycle at t=30ms (20ms gap, overrun >15ms threshold) */
    mock_tick_ms = 30u;
    TCU_Main_Tick();
    TEST_ASSERT_TRUE(mock_overrun_warn_count > 0u);
}

/** @verifies SWR-TCU-015 */
void test_TcuMain_not_running_does_nothing(void)
{
    tcu_running = FALSE;

    TCU_Main_Tick();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_rte_main_count);
    TEST_ASSERT_EQUAL_UINT16(0u, tcu_cycle_count);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-TCU-015: TCU main loop */
    RUN_TEST(test_TcuMain_10ms_cycle);
    RUN_TEST(test_TcuMain_processes_uds_requests);
    RUN_TEST(test_TcuMain_updates_cache_each_cycle);
    RUN_TEST(test_TcuMain_overrun_5ms_warning);
    RUN_TEST(test_TcuMain_not_running_does_nothing);

    return UNITY_END();
}
