/**
 * @file    test_Swc_Watchdog.c
 * @brief   Unit tests for Swc_Watchdog — external watchdog feed with 4-condition gate
 * @date    2026-02-24
 *
 * @verifies SWR-CVC-023
 *
 * Tests: feed succeeds only when all 4 conditions met (loop complete,
 * canary intact, RAM ok, CAN ok). Any single condition failure blocks feed.
 *
 * Mocks: Dio_FlipChannel
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
 * Swc_Watchdog Config Type (mirrors header)
 * ================================================================== */

typedef struct {
    uint8   wdiDioChannel;
} Swc_Watchdog_ConfigType;

/* Swc_Watchdog API declarations */
extern void            Swc_Watchdog_Init(const Swc_Watchdog_ConfigType* ConfigPtr);
extern Std_ReturnType  Swc_Watchdog_Feed(uint8 loopComplete, uint8 canaryOk,
                                          uint8 ramOk, uint8 canOk);

/* ==================================================================
 * Mock: Dio_FlipChannel
 * ================================================================== */

static uint8   mock_dio_flip_count;
static uint8   mock_dio_last_channel;

uint8 Dio_FlipChannel(uint8 ChannelId)
{
    mock_dio_flip_count++;
    mock_dio_last_channel = ChannelId;
    return 1u;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static Swc_Watchdog_ConfigType test_config;

void setUp(void)
{
    mock_dio_flip_count  = 0u;
    mock_dio_last_channel = 0xFFu;

    test_config.wdiDioChannel = 6u;

    Swc_Watchdog_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-CVC-023: Watchdog Feed Tests
 * ================================================================== */

/** @verifies SWR-CVC-023 — Feed succeeds when all 4 conditions are TRUE */
void test_Watchdog_Feed_all_conditions_met_toggles(void)
{
    Std_ReturnType ret;

    ret = Swc_Watchdog_Feed(TRUE, TRUE, TRUE, TRUE);

    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dio_flip_count);
    TEST_ASSERT_EQUAL_UINT8(6u, mock_dio_last_channel);
}

/** @verifies SWR-CVC-023 — Feed blocked when canary is corrupted */
void test_Watchdog_Feed_canary_corrupted_no_toggle(void)
{
    Std_ReturnType ret;

    ret = Swc_Watchdog_Feed(TRUE, FALSE, TRUE, TRUE);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_flip_count);
}

/** @verifies SWR-CVC-023 — Feed blocked when RAM test failed */
void test_Watchdog_Feed_ram_test_failed_no_toggle(void)
{
    Std_ReturnType ret;

    ret = Swc_Watchdog_Feed(TRUE, TRUE, FALSE, TRUE);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_flip_count);
}

/** @verifies SWR-CVC-023 — Feed blocked when CAN bus is in bus-off */
void test_Watchdog_Feed_can_busoff_no_toggle(void)
{
    Std_ReturnType ret;

    ret = Swc_Watchdog_Feed(TRUE, TRUE, TRUE, FALSE);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_flip_count);
}

/** @verifies SWR-CVC-023 — Feed blocked when loop is incomplete */
void test_Watchdog_Feed_loop_incomplete_no_toggle(void)
{
    Std_ReturnType ret;

    ret = Swc_Watchdog_Feed(FALSE, TRUE, TRUE, TRUE);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_flip_count);
}

/** @verifies SWR-CVC-023 — Feed blocked when not initialized */
void test_Watchdog_Feed_not_initialized_no_toggle(void)
{
    Std_ReturnType ret;

    /* Re-init with NULL to de-initialize */
    Swc_Watchdog_Init(NULL_PTR);

    ret = Swc_Watchdog_Feed(TRUE, TRUE, TRUE, TRUE);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_flip_count);
}

/** @verifies SWR-CVC-023 — Multiple feeds increment toggle count */
void test_Watchdog_Feed_multiple_feeds_toggle_count(void)
{
    (void)Swc_Watchdog_Feed(TRUE, TRUE, TRUE, TRUE);
    (void)Swc_Watchdog_Feed(TRUE, TRUE, TRUE, TRUE);
    (void)Swc_Watchdog_Feed(TRUE, TRUE, TRUE, TRUE);

    TEST_ASSERT_EQUAL_UINT8(3u, mock_dio_flip_count);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Watchdog_Feed_all_conditions_met_toggles);
    RUN_TEST(test_Watchdog_Feed_canary_corrupted_no_toggle);
    RUN_TEST(test_Watchdog_Feed_ram_test_failed_no_toggle);
    RUN_TEST(test_Watchdog_Feed_can_busoff_no_toggle);
    RUN_TEST(test_Watchdog_Feed_loop_incomplete_no_toggle);
    RUN_TEST(test_Watchdog_Feed_not_initialized_no_toggle);
    RUN_TEST(test_Watchdog_Feed_multiple_feeds_toggle_count);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_WATCHDOG_H
#define DIO_H

#include "../src/Swc_Watchdog.c"
