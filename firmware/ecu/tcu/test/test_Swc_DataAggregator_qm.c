/**
 * @file    test_Swc_DataAggregator.c
 * @brief   Unit tests for TCU CAN data aggregation with timeout detection
 * @date    2026-02-24
 *
 * @verifies SWR-TCU-014
 *
 * Tests caching of CAN values from 6 data sources with per-source
 * timeout detection (200ms-2000ms range). Verifies stale data flagging.
 *
 * Mocks: timing functions
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
#define SWC_DATAAGGREGATOR_H
#define TCU_CFG_H

/* ====================================================================
 * Data source IDs and timeouts
 * ==================================================================== */

#define TCU_DATA_VEHICLE_STATE   0u
#define TCU_DATA_MOTOR_CURRENT   1u
#define TCU_DATA_MOTOR_TEMP      2u
#define TCU_DATA_BATTERY         3u
#define TCU_DATA_DTC_BCAST       4u
#define TCU_DATA_HB_CVC          5u
#define TCU_DATA_SOURCE_COUNT    6u

#define TCU_TIMEOUT_VEHICLE_STATE_MS   500u
#define TCU_TIMEOUT_MOTOR_CURRENT_MS   500u
#define TCU_TIMEOUT_MOTOR_TEMP_MS      2000u
#define TCU_TIMEOUT_BATTERY_MS         2000u
#define TCU_TIMEOUT_DTC_BCAST_MS       2000u
#define TCU_TIMEOUT_HB_CVC_MS          200u

/* ====================================================================
 * Mock: Timing
 * ==================================================================== */

static uint32 mock_tick_ms;

uint32 mock_get_tick_ms(void)
{
    return mock_tick_ms;
}

/* ====================================================================
 * Include source under test
 * ==================================================================== */

#define TCU_DATA_USE_MOCK
#include "../src/Swc_DataAggregator.c"

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_tick_ms = 0u;
    TCU_Data_Init();
}

void tearDown(void) { }

/* ====================================================================
 * SWR-TCU-014: Cache CAN values
 * ==================================================================== */

/** @verifies SWR-TCU-014 */
void test_DataAgg_caches_vehicle_state(void)
{
    uint8 data[8] = { 0x02u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
    uint8 out[8];
    boolean stale;

    Std_ReturnType ret = TCU_Data_Update(TCU_DATA_VEHICLE_STATE, data, 8u);
    TEST_ASSERT_EQUAL(E_OK, ret);

    ret = TCU_Data_Get(TCU_DATA_VEHICLE_STATE, out, &stale);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0x02u, out[0]);
    TEST_ASSERT_EQUAL_UINT8(FALSE, stale);
}

/** @verifies SWR-TCU-014 */
void test_DataAgg_caches_motor_status(void)
{
    uint8 data[8] = { 0x10u, 0x20u, 0u, 0u, 0u, 0u, 0u, 0u };
    uint8 out[8];
    boolean stale;

    Std_ReturnType ret = TCU_Data_Update(TCU_DATA_MOTOR_CURRENT, data, 8u);
    TEST_ASSERT_EQUAL(E_OK, ret);

    ret = TCU_Data_Get(TCU_DATA_MOTOR_CURRENT, out, &stale);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0x10u, out[0]);
    TEST_ASSERT_EQUAL_UINT8(0x20u, out[1]);
    TEST_ASSERT_EQUAL_UINT8(FALSE, stale);
}

/* ====================================================================
 * SWR-TCU-014: Timeout detection
 * ==================================================================== */

/** @verifies SWR-TCU-014 */
void test_DataAgg_timeout_500ms_vehicle_state(void)
{
    uint8 data[8] = { 0x01u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
    uint8 out[8];
    boolean stale;

    /* Update at t=0 */
    mock_tick_ms = 0u;
    TCU_Data_Update(TCU_DATA_VEHICLE_STATE, data, 8u);

    /* Check at t=499ms — should not be stale */
    mock_tick_ms = 499u;
    TCU_Data_CheckTimeouts();
    TCU_Data_Get(TCU_DATA_VEHICLE_STATE, out, &stale);
    TEST_ASSERT_EQUAL_UINT8(FALSE, stale);

    /* Check at t=501ms — should be stale */
    mock_tick_ms = 501u;
    TCU_Data_CheckTimeouts();
    TCU_Data_Get(TCU_DATA_VEHICLE_STATE, out, &stale);
    TEST_ASSERT_EQUAL_UINT8(TRUE, stale);
}

/** @verifies SWR-TCU-014 */
void test_DataAgg_timeout_2000ms_temperature(void)
{
    uint8 data[8] = { 60u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
    uint8 out[8];
    boolean stale;

    /* Update at t=0 */
    mock_tick_ms = 0u;
    TCU_Data_Update(TCU_DATA_MOTOR_TEMP, data, 8u);

    /* Check at t=1999ms — not stale */
    mock_tick_ms = 1999u;
    TCU_Data_CheckTimeouts();
    TCU_Data_Get(TCU_DATA_MOTOR_TEMP, out, &stale);
    TEST_ASSERT_EQUAL_UINT8(FALSE, stale);

    /* Check at t=2001ms — stale */
    mock_tick_ms = 2001u;
    TCU_Data_CheckTimeouts();
    TCU_Data_Get(TCU_DATA_MOTOR_TEMP, out, &stale);
    TEST_ASSERT_EQUAL_UINT8(TRUE, stale);
}

/** @verifies SWR-TCU-014 */
void test_DataAgg_stale_data_flagged(void)
{
    uint8 data[8] = { 0xAAu, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
    uint8 out[8];
    boolean stale;

    /* Update heartbeat at t=0 (200ms timeout) */
    mock_tick_ms = 0u;
    TCU_Data_Update(TCU_DATA_HB_CVC, data, 8u);

    /* After 201ms without update — stale */
    mock_tick_ms = 201u;
    TCU_Data_CheckTimeouts();
    TCU_Data_Get(TCU_DATA_HB_CVC, out, &stale);
    TEST_ASSERT_EQUAL_UINT8(TRUE, stale);

    /* Data is still available (last known) */
    TEST_ASSERT_EQUAL_UINT8(0xAAu, out[0]);
}

/** @verifies SWR-TCU-014 */
void test_DataAgg_update_clears_stale(void)
{
    uint8 data[8] = { 0x01u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
    uint8 out[8];
    boolean stale;

    /* Mark as stale */
    mock_tick_ms = 0u;
    TCU_Data_Update(TCU_DATA_HB_CVC, data, 8u);
    mock_tick_ms = 300u;
    TCU_Data_CheckTimeouts();
    TCU_Data_Get(TCU_DATA_HB_CVC, out, &stale);
    TEST_ASSERT_EQUAL_UINT8(TRUE, stale);

    /* New update clears stale flag */
    TCU_Data_Update(TCU_DATA_HB_CVC, data, 8u);
    TCU_Data_Get(TCU_DATA_HB_CVC, out, &stale);
    TEST_ASSERT_EQUAL_UINT8(FALSE, stale);
}

/** @verifies SWR-TCU-014 */
void test_DataAgg_invalid_source_rejected(void)
{
    uint8 data[8] = { 0u };

    Std_ReturnType ret = TCU_Data_Update(99u, data, 8u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-TCU-014 */
void test_DataAgg_get_before_any_update(void)
{
    uint8 out[8];
    boolean stale;

    Std_ReturnType ret = TCU_Data_Get(TCU_DATA_VEHICLE_STATE, out, &stale);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-TCU-014: CAN data aggregation */
    RUN_TEST(test_DataAgg_caches_vehicle_state);
    RUN_TEST(test_DataAgg_caches_motor_status);
    RUN_TEST(test_DataAgg_timeout_500ms_vehicle_state);
    RUN_TEST(test_DataAgg_timeout_2000ms_temperature);
    RUN_TEST(test_DataAgg_stale_data_flagged);
    RUN_TEST(test_DataAgg_update_clears_stale);
    RUN_TEST(test_DataAgg_invalid_source_rejected);
    RUN_TEST(test_DataAgg_get_before_any_update);

    return UNITY_END();
}
