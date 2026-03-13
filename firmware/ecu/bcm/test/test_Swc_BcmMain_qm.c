/**
 * @file    test_Swc_BcmMain.c
 * @brief   Unit tests for BCM main loop — 10ms cycle, process, transmit
 * @date    2026-02-24
 *
 * @verifies SWR-BCM-012
 *
 * Tests 10ms main loop processing, transmit every 10th cycle (100ms),
 * and cycle overrun warning logging.
 *
 * Mocks: BCM_CAN_ReceiveState, BCM_CAN_ReceiveCommand, BCM_CAN_TransmitStatus,
 *        timing functions
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
#define SWC_BCMMAIN_H
#define SWC_BCMCAN_H
#define BCM_CFG_H

/* ====================================================================
 * BCM Configuration Constants
 * ==================================================================== */

#define BCM_MAIN_CYCLE_MS           10u
#define BCM_TX_EVERY_N_CYCLES       10u

/* ====================================================================
 * Mock: BCM CAN functions
 * ==================================================================== */

static uint8 mock_receive_state_count;
static uint8 mock_receive_cmd_count;
static uint8 mock_transmit_count;

Std_ReturnType BCM_CAN_ReceiveState(void)
{
    mock_receive_state_count++;
    return E_OK;
}

Std_ReturnType BCM_CAN_ReceiveCommand(void)
{
    mock_receive_cmd_count++;
    return E_OK;
}

Std_ReturnType BCM_CAN_TransmitStatus(void)
{
    mock_transmit_count++;
    return E_OK;
}

/* ====================================================================
 * Mock: Timing
 * ==================================================================== */

static uint32 mock_tick_ms;
static uint32 mock_cycle_start_ms;

uint32 mock_get_tick_ms(void)
{
    return mock_tick_ms;
}

/* ====================================================================
 * Mock: Overrun logging
 * ==================================================================== */

static uint8  mock_overrun_logged;
static uint32 mock_overrun_duration_ms;

void mock_log_overrun(uint32 duration_ms)
{
    mock_overrun_logged++;
    mock_overrun_duration_ms = duration_ms;
}

/* ====================================================================
 * Include source under test
 * ==================================================================== */

#define BCM_MAIN_USE_MOCK
#include "../src/Swc_BcmMain.c"

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_receive_state_count = 0u;
    mock_receive_cmd_count   = 0u;
    mock_transmit_count      = 0u;
    mock_tick_ms             = 0u;
    mock_cycle_start_ms      = 0u;
    mock_overrun_logged      = 0u;
    mock_overrun_duration_ms = 0u;

    bcm_main_cycle_counter   = 0u;
    bcm_main_initialized     = TRUE;
    bcm_main_first_cycle_done = FALSE;
}

void tearDown(void) { }

/* ====================================================================
 * SWR-BCM-012: Main loop processes all steps each cycle
 * ==================================================================== */

/** @verifies SWR-BCM-012 */
void test_BcmMain_cycle_processes_all_steps(void)
{
    /* Run one cycle */
    BCM_Main();

    /* Should have called receive state and receive command */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_receive_state_count);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_receive_cmd_count);
}

/** @verifies SWR-BCM-012 */
void test_BcmMain_transmit_every_10th_cycle(void)
{
    uint8 i;

    /* Run 20 cycles */
    for (i = 0u; i < 20u; i++) {
        BCM_Main();
    }

    /* Should transmit exactly twice (cycle 10 and cycle 20) */
    TEST_ASSERT_EQUAL_UINT8(2u, mock_transmit_count);
}

/** @verifies SWR-BCM-012 */
void test_BcmMain_overrun_warning_logged(void)
{
    /* Simulate a cycle that takes longer than 10ms */
    mock_tick_ms = 0u;

    /* Start of cycle */
    BCM_Main();

    /* Advance time to simulate overrun */
    mock_tick_ms = BCM_MAIN_CYCLE_MS + 5u;

    /* Next cycle should detect overrun from previous */
    BCM_Main();

    TEST_ASSERT_TRUE(mock_overrun_logged > 0u);
}

/** @verifies SWR-BCM-012 */
void test_BcmMain_no_transmit_before_10th_cycle(void)
{
    uint8 i;

    /* Run 9 cycles */
    for (i = 0u; i < 9u; i++) {
        BCM_Main();
    }

    /* Should NOT have transmitted yet */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_transmit_count);
}

/** @verifies SWR-BCM-012 */
void test_BcmMain_not_initialized_does_nothing(void)
{
    bcm_main_initialized = FALSE;

    BCM_Main();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_receive_state_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_receive_cmd_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_transmit_count);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-BCM-012: BCM main loop */
    RUN_TEST(test_BcmMain_cycle_processes_all_steps);
    RUN_TEST(test_BcmMain_transmit_every_10th_cycle);
    RUN_TEST(test_BcmMain_overrun_warning_logged);
    RUN_TEST(test_BcmMain_no_transmit_before_10th_cycle);
    RUN_TEST(test_BcmMain_not_initialized_does_nothing);

    return UNITY_END();
}
