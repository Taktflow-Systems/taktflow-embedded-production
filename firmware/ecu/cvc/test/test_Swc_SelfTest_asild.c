/**
 * @file    test_Swc_SelfTest.c
 * @brief   Unit tests for Swc_SelfTest — startup self-test sequence
 * @date    2026-02-24
 *
 * @verifies SWR-CVC-029
 *
 * Tests: all 7 steps pass, individual step failures (SPI, CAN, NVM, MPU,
 * canary, RAM), and non-critical OLED failure.
 *
 * Mocks: SelfTest_Hw_* hardware test functions, Dem_ReportErrorStatus
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
 * Self-test constants (mirrors header)
 * ================================================================== */

#define SELF_TEST_PASSED       1u
#define SELF_TEST_FAILED       0u

#define SELFTEST_STEP_SPI      0x01u
#define SELFTEST_STEP_CAN      0x02u
#define SELFTEST_STEP_NVM      0x04u
#define SELFTEST_STEP_OLED     0x08u
#define SELFTEST_STEP_MPU      0x10u
#define SELFTEST_STEP_CANARY   0x20u
#define SELFTEST_STEP_RAM      0x40u
#define SELFTEST_ALL_PASSED    0x7Fu

/* DTC IDs */
#define CVC_DTC_SELF_TEST_FAIL    16u
#define CVC_DTC_NVM_CRC_FAIL      15u
#define CVC_DTC_DISPLAY_COMM      17u

/* Dem status */
#define DEM_EVENT_STATUS_PASSED    0u
#define DEM_EVENT_STATUS_FAILED    1u

/* Swc_SelfTest API declarations */
extern uint8 Swc_SelfTest_Startup(void);
extern uint8 Swc_SelfTest_GetResults(void);

/* ==================================================================
 * Mock: Hardware test functions
 * ================================================================== */

static Std_ReturnType mock_spi_result;
static Std_ReturnType mock_can_result;
static Std_ReturnType mock_nvm_result;
static Std_ReturnType mock_oled_result;
static Std_ReturnType mock_mpu_result;
static Std_ReturnType mock_canary_result;
static Std_ReturnType mock_ram_result;

Std_ReturnType SelfTest_Hw_SpiLoopback(void)  { return mock_spi_result; }
Std_ReturnType SelfTest_Hw_CanLoopback(void)  { return mock_can_result; }
Std_ReturnType SelfTest_Hw_NvmCheck(void)     { return mock_nvm_result; }
Std_ReturnType SelfTest_Hw_OledAck(void)      { return mock_oled_result; }
Std_ReturnType SelfTest_Hw_MpuVerify(void)    { return mock_mpu_result; }
Std_ReturnType SelfTest_Hw_CanaryCheck(void)  { return mock_canary_result; }
Std_ReturnType SelfTest_Hw_RamPattern(void)   { return mock_ram_result; }

/* ==================================================================
 * Mock: Dem_ReportErrorStatus
 * ================================================================== */

typedef uint8 Dem_EventIdType;

typedef enum {
    DEM_EVT_PASSED = 0u,
    DEM_EVT_FAILED = 1u
} Dem_EventStatusType;

static uint8  mock_dem_call_count;
static uint8  mock_dem_last_event_id;
static uint8  mock_dem_last_status;

void Dem_ReportErrorStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    mock_dem_call_count++;
    mock_dem_last_event_id = EventId;
    mock_dem_last_status   = (uint8)EventStatus;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    mock_spi_result    = E_OK;
    mock_can_result    = E_OK;
    mock_nvm_result    = E_OK;
    mock_oled_result   = E_OK;
    mock_mpu_result    = E_OK;
    mock_canary_result = E_OK;
    mock_ram_result    = E_OK;

    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;
}

void tearDown(void) { }

/* ==================================================================
 * SWR-CVC-029: Self-Test Tests
 * ================================================================== */

/** @verifies SWR-CVC-029 — All tests pass returns SELF_TEST_PASSED */
void test_SelfTest_all_pass(void)
{
    uint8 result;

    result = Swc_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(SELF_TEST_PASSED, result);
    TEST_ASSERT_EQUAL_UINT8(SELFTEST_ALL_PASSED, Swc_SelfTest_GetResults());
}

/** @verifies SWR-CVC-029 — SPI failure returns SELF_TEST_FAILED */
void test_SelfTest_spi_fail(void)
{
    uint8 result;

    mock_spi_result = E_NOT_OK;

    result = Swc_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(SELF_TEST_FAILED, result);
    /* SPI step should NOT be in results bitmask */
    TEST_ASSERT_EQUAL_UINT8(0u, Swc_SelfTest_GetResults() & SELFTEST_STEP_SPI);
}

/** @verifies SWR-CVC-029 — CAN loopback failure returns SELF_TEST_FAILED */
void test_SelfTest_can_loopback_fail(void)
{
    uint8 result;

    mock_can_result = E_NOT_OK;

    result = Swc_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(SELF_TEST_FAILED, result);
    /* SPI passed, CAN did not */
    TEST_ASSERT_TRUE((Swc_SelfTest_GetResults() & SELFTEST_STEP_SPI) != 0u);
    TEST_ASSERT_EQUAL_UINT8(0u, Swc_SelfTest_GetResults() & SELFTEST_STEP_CAN);
}

/** @verifies SWR-CVC-029 — NVM both corrupt returns SELF_TEST_FAILED and reports DTC */
void test_SelfTest_nvm_both_corrupt(void)
{
    uint8 result;

    mock_nvm_result = E_NOT_OK;

    result = Swc_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(SELF_TEST_FAILED, result);
    TEST_ASSERT_TRUE(mock_dem_call_count > 0u);
}

/** @verifies SWR-CVC-029 — OLED failure does NOT fail overall self-test (QM) */
void test_SelfTest_oled_fail_noncritical(void)
{
    uint8 result;

    mock_oled_result = E_NOT_OK;

    result = Swc_SelfTest_Startup();

    /* OLED is QM — overall result should still pass */
    TEST_ASSERT_EQUAL_UINT8(SELF_TEST_PASSED, result);
    /* But OLED step bit should be absent */
    TEST_ASSERT_EQUAL_UINT8(0u, Swc_SelfTest_GetResults() & SELFTEST_STEP_OLED);
    /* DTC should be reported for display comm */
    TEST_ASSERT_TRUE(mock_dem_call_count > 0u);
}

/** @verifies SWR-CVC-029 — MPU verify failure returns SELF_TEST_FAILED */
void test_SelfTest_mpu_verify_fail(void)
{
    uint8 result;

    mock_mpu_result = E_NOT_OK;

    result = Swc_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(SELF_TEST_FAILED, result);
}

/** @verifies SWR-CVC-029 — Canary check failure returns SELF_TEST_FAILED */
void test_SelfTest_canary_fail(void)
{
    uint8 result;

    mock_canary_result = E_NOT_OK;

    result = Swc_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(SELF_TEST_FAILED, result);
}

/** @verifies SWR-CVC-029 — RAM pattern failure returns SELF_TEST_FAILED */
void test_SelfTest_ram_fail(void)
{
    uint8 result;

    mock_ram_result = E_NOT_OK;

    result = Swc_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(SELF_TEST_FAILED, result);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_SelfTest_all_pass);
    RUN_TEST(test_SelfTest_spi_fail);
    RUN_TEST(test_SelfTest_can_loopback_fail);
    RUN_TEST(test_SelfTest_nvm_both_corrupt);
    RUN_TEST(test_SelfTest_oled_fail_noncritical);
    RUN_TEST(test_SelfTest_mpu_verify_fail);
    RUN_TEST(test_SelfTest_canary_fail);
    RUN_TEST(test_SelfTest_ram_fail);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_SELFTEST_H
#define CVC_CFG_H
#define DEM_H

#include "../src/Swc_SelfTest.c"
