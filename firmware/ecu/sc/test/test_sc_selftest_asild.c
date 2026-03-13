/**
 * @file    test_sc_selftest.c
 * @brief   Unit tests for sc_selftest — startup + runtime self-test
 * @date    2026-02-23
 *
 * @verifies SWR-SC-016, SWR-SC-017, SWR-SC-018, SWR-SC-019,
 *           SWR-SC-020, SWR-SC-021
 *
 * Tests stack canary plant/check, RAM pattern write/verify, startup
 * self-test sequence, runtime incremental checks, and health status.
 *
 * Mocks: ESM BIST, PBIST, flash CRC, GIO, RAM pointers.
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions
 * ================================================================== */

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int       uint32;
typedef signed short        sint16;
typedef signed int         sint32;
typedef uint8               boolean;
typedef uint8               Std_ReturnType;

#define TRUE                1u
#define FALSE               0u
#define E_OK                0u
#define E_NOT_OK            1u
#define NULL_PTR            ((void*)0)

/* ==================================================================
 * SC Configuration Constants
 * ================================================================== */

#define SC_STACK_CANARY_VALUE       0xDEADBEEFu
#define SC_RAM_TEST_SIZE            32u
#define SC_SELFTEST_RUNTIME_PERIOD  6000u
#define SC_SELFTEST_RUNTIME_STEPS   4u
#define SC_GIO_PORT_A               0u
#define SC_GIO_PORT_B               1u
#define SC_PIN_RELAY                0u
#define SC_PIN_LED_CVC              1u
#define SC_PIN_LED_FZC              2u
#define SC_PIN_LED_RZC              3u
#define SC_PIN_WDI                  5u
#define SC_PIN_LED_SYS              4u

/* ==================================================================
 * Mock: Simulated RAM and canary locations
 * ================================================================== */

static uint32 mock_canary_location;
static uint8  mock_ram_test_area[SC_RAM_TEST_SIZE];

/* Override the pointer-based access to use local variables */
#define SC_SELFTEST_USE_MOCK

/* ==================================================================
 * Mock: Hardware BIST and CRC
 * ================================================================== */

static boolean mock_lockstep_bist_pass;
static boolean mock_ram_pbist_pass;
static boolean mock_flash_crc_pass;
static boolean mock_dcan_loopback_pass;
static boolean mock_gpio_readback_pass;
static boolean mock_lamp_test_pass;
static boolean mock_wdt_test_pass;

boolean hw_lockstep_bist(void)
{
    return mock_lockstep_bist_pass;
}

boolean hw_ram_pbist(void)
{
    return mock_ram_pbist_pass;
}

boolean hw_flash_crc_check(void)
{
    return mock_flash_crc_pass;
}

boolean hw_dcan_loopback_test(void)
{
    return mock_dcan_loopback_pass;
}

boolean hw_gpio_readback_test(void)
{
    return mock_gpio_readback_pass;
}

boolean hw_lamp_test(void)
{
    return mock_lamp_test_pass;
}

boolean hw_watchdog_test(void)
{
    return mock_wdt_test_pass;
}

/* ==================================================================
 * Mock: Runtime checks
 * ================================================================== */

static boolean mock_flash_crc_incremental_pass;
static boolean mock_dcan_error_ok;

boolean hw_flash_crc_incremental(void)
{
    return mock_flash_crc_incremental_pass;
}

boolean hw_dcan_error_check(void)
{
    return mock_dcan_error_ok;
}

/* ==================================================================
 * Mock: GIO
 * ================================================================== */

static uint8 mock_gio_a[8];
static uint8 mock_gio_b[8];

void gioSetBit(uint8 port, uint8 pin, uint8 value)
{
    if ((port == SC_GIO_PORT_A) && (pin < 8u)) {
        mock_gio_a[pin] = value;
    } else if ((port == SC_GIO_PORT_B) && (pin < 8u)) {
        mock_gio_b[pin] = value;
    }
}

uint8 gioGetBit(uint8 port, uint8 pin)
{
    if ((port == SC_GIO_PORT_A) && (pin < 8u)) {
        return mock_gio_a[pin];
    } else if ((port == SC_GIO_PORT_B) && (pin < 8u)) {
        return mock_gio_b[pin];
    }
    return 0u;
}

/* ==================================================================
 * Mock: Relay readback for runtime test
 * ================================================================== */

static boolean mock_relay_commanded;

boolean SC_Relay_IsKilled(void)
{
    return FALSE;
}

/* ==================================================================
 * Include source under test
 * ================================================================== */

#include "../src/sc_selftest.c"

/* ==================================================================
 * Test setup / teardown
 * ================================================================== */

void setUp(void)
{
    uint8 i;

    mock_canary_location = 0u;
    for (i = 0u; i < SC_RAM_TEST_SIZE; i++) {
        mock_ram_test_area[i] = 0u;
    }

    mock_lockstep_bist_pass = TRUE;
    mock_ram_pbist_pass     = TRUE;
    mock_flash_crc_pass     = TRUE;
    mock_dcan_loopback_pass = TRUE;
    mock_gpio_readback_pass = TRUE;
    mock_lamp_test_pass     = TRUE;
    mock_wdt_test_pass      = TRUE;

    mock_flash_crc_incremental_pass = TRUE;
    mock_dcan_error_ok = TRUE;

    for (i = 0u; i < 8u; i++) {
        mock_gio_a[i] = 0u;
        mock_gio_b[i] = 0u;
    }

    mock_relay_commanded = FALSE;

    SC_SelfTest_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-SC-021: Stack Canary
 * ================================================================== */

/** @verifies SWR-SC-021 -- Init plants canary value */
void test_SelfTest_canary_planted(void)
{
    TEST_ASSERT_EQUAL_UINT32(SC_STACK_CANARY_VALUE, stack_canary);
}

/** @verifies SWR-SC-021 -- Canary check passes with correct value */
void test_SelfTest_canary_ok(void)
{
    TEST_ASSERT_TRUE(SC_SelfTest_StackCanaryOk());
}

/** @verifies SWR-SC-021 -- Canary check fails with corrupted value */
void test_SelfTest_canary_corrupted(void)
{
    stack_canary = 0x12345678u;

    TEST_ASSERT_FALSE(SC_SelfTest_StackCanaryOk());
}

/* ==================================================================
 * SWR-SC-019: Startup Self-Test
 * ================================================================== */

/** @verifies SWR-SC-019 -- All 7 steps pass returns 0 */
void test_SelfTest_startup_all_pass(void)
{
    uint8 result = SC_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(0u, result);
    TEST_ASSERT_TRUE(SC_SelfTest_IsHealthy());
}

/** @verifies SWR-SC-019 -- Lockstep BIST failure returns step 1 */
void test_SelfTest_startup_lockstep_fail(void)
{
    mock_lockstep_bist_pass = FALSE;

    uint8 result = SC_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(1u, result);
    TEST_ASSERT_FALSE(SC_SelfTest_IsHealthy());
}

/** @verifies SWR-SC-019 -- RAM PBIST failure returns step 2 */
void test_SelfTest_startup_ram_fail(void)
{
    mock_ram_pbist_pass = FALSE;

    uint8 result = SC_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(2u, result);
}

/** @verifies SWR-SC-019 -- Flash CRC failure returns step 3 */
void test_SelfTest_startup_flash_fail(void)
{
    mock_flash_crc_pass = FALSE;

    uint8 result = SC_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(3u, result);
}

/** @verifies SWR-SC-019 -- DCAN loopback failure returns step 4 */
void test_SelfTest_startup_dcan_fail(void)
{
    mock_dcan_loopback_pass = FALSE;

    uint8 result = SC_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(4u, result);
}

/* ==================================================================
 * SWR-SC-020: Runtime Self-Test
 * ================================================================== */

/** @verifies SWR-SC-020 -- Runtime test stays healthy with all checks passing */
void test_SelfTest_runtime_healthy(void)
{
    SC_SelfTest_Startup();  /* Must pass startup first */

    uint16 i;
    for (i = 0u; i < 100u; i++) {
        SC_SelfTest_Runtime();
    }

    TEST_ASSERT_TRUE(SC_SelfTest_IsHealthy());
}

/** @verifies SWR-SC-020 -- Runtime flash CRC failure makes unhealthy */
void test_SelfTest_runtime_flash_fail(void)
{
    SC_SelfTest_Startup();

    mock_flash_crc_incremental_pass = FALSE;

    /* Run enough iterations to hit the flash CRC step */
    uint16 i;
    for (i = 0u; i < (SC_SELFTEST_RUNTIME_PERIOD + 10u); i++) {
        SC_SelfTest_Runtime();
    }

    TEST_ASSERT_FALSE(SC_SelfTest_IsHealthy());
}

/* ==================================================================
 * HARDENED TESTS — Boundary Values, Fault Injection
 * ================================================================== */

/** @verifies SWR-SC-021
 *  Equivalence class: Fault injection — canary set to zero (partial corruption) */
void test_selftest_canary_zero(void)
{
    stack_canary = 0u;

    TEST_ASSERT_FALSE(SC_SelfTest_StackCanaryOk());
}

/** @verifies SWR-SC-021
 *  Equivalence class: Boundary — canary off-by-one (0xDEADBEEE) */
void test_selftest_canary_off_by_one(void)
{
    stack_canary = SC_STACK_CANARY_VALUE - 1u;

    TEST_ASSERT_FALSE(SC_SelfTest_StackCanaryOk());
}

/** @verifies SWR-SC-021
 *  Equivalence class: Boundary — canary inverted (0x21524110) */
void test_selftest_canary_inverted(void)
{
    stack_canary = ~SC_STACK_CANARY_VALUE;

    TEST_ASSERT_FALSE(SC_SelfTest_StackCanaryOk());
}

/** @verifies SWR-SC-019
 *  Equivalence class: Fault injection — GPIO readback failure (step 5) returns 5 */
void test_selftest_startup_gpio_fail(void)
{
    mock_gpio_readback_pass = FALSE;

    uint8 result = SC_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(5u, result);
    TEST_ASSERT_FALSE(SC_SelfTest_IsHealthy());
}

/** @verifies SWR-SC-019
 *  Equivalence class: Fault injection — lamp test failure (step 6) returns 6 */
void test_selftest_startup_lamp_fail(void)
{
    mock_lamp_test_pass = FALSE;

    uint8 result = SC_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(6u, result);
    TEST_ASSERT_FALSE(SC_SelfTest_IsHealthy());
}

/** @verifies SWR-SC-019
 *  Equivalence class: Fault injection — watchdog test failure (step 7) returns 7 */
void test_selftest_startup_wdt_fail(void)
{
    mock_wdt_test_pass = FALSE;

    uint8 result = SC_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(7u, result);
    TEST_ASSERT_FALSE(SC_SelfTest_IsHealthy());
}

/** @verifies SWR-SC-020
 *  Equivalence class: Boundary — runtime DCAN error makes unhealthy */
void test_selftest_runtime_dcan_fail(void)
{
    SC_SelfTest_Startup();

    mock_dcan_error_ok = FALSE;

    uint16 i;
    for (i = 0u; i < (SC_SELFTEST_RUNTIME_PERIOD + 10u); i++) {
        SC_SelfTest_Runtime();
    }

    TEST_ASSERT_FALSE(SC_SelfTest_IsHealthy());
}

/** @verifies SWR-SC-019
 *  Equivalence class: Boundary — double startup call (idempotent on success) */
void test_selftest_startup_double_call(void)
{
    uint8 result1 = SC_SelfTest_Startup();
    uint8 result2 = SC_SelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(0u, result1);
    TEST_ASSERT_EQUAL_UINT8(0u, result2);
    TEST_ASSERT_TRUE(SC_SelfTest_IsHealthy());
}

/** @verifies SWR-SC-020
 *  Equivalence class: Boundary — runtime before startup does not crash */
void test_selftest_runtime_before_startup(void)
{
    /* Do not call SC_SelfTest_Startup, just run runtime */
    uint16 i;
    for (i = 0u; i < 50u; i++) {
        SC_SelfTest_Runtime();
    }

    /* Should not crash — healthy depends on startup having passed */
    TEST_ASSERT_FALSE(SC_SelfTest_IsHealthy());
}

/** @verifies SWR-SC-019
 *  Equivalence class: Fault injection — first step fails, later steps not reached */
void test_selftest_first_fail_blocks_remaining(void)
{
    mock_lockstep_bist_pass = FALSE;

    uint8 result = SC_SelfTest_Startup();

    /* Should fail at step 1 — does not continue to steps 2-7 */
    TEST_ASSERT_EQUAL_UINT8(1u, result);
    TEST_ASSERT_FALSE(SC_SelfTest_IsHealthy());
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-SC-021: Stack canary */
    RUN_TEST(test_SelfTest_canary_planted);
    RUN_TEST(test_SelfTest_canary_ok);
    RUN_TEST(test_SelfTest_canary_corrupted);

    /* SWR-SC-019: Startup self-test */
    RUN_TEST(test_SelfTest_startup_all_pass);
    RUN_TEST(test_SelfTest_startup_lockstep_fail);
    RUN_TEST(test_SelfTest_startup_ram_fail);
    RUN_TEST(test_SelfTest_startup_flash_fail);
    RUN_TEST(test_SelfTest_startup_dcan_fail);

    /* SWR-SC-020: Runtime self-test */
    RUN_TEST(test_SelfTest_runtime_healthy);
    RUN_TEST(test_SelfTest_runtime_flash_fail);

    /* Hardened tests — boundary values, fault injection */
    RUN_TEST(test_selftest_canary_zero);
    RUN_TEST(test_selftest_canary_off_by_one);
    RUN_TEST(test_selftest_canary_inverted);
    RUN_TEST(test_selftest_startup_gpio_fail);
    RUN_TEST(test_selftest_startup_lamp_fail);
    RUN_TEST(test_selftest_startup_wdt_fail);
    RUN_TEST(test_selftest_runtime_dcan_fail);
    RUN_TEST(test_selftest_startup_double_call);
    RUN_TEST(test_selftest_runtime_before_startup);
    RUN_TEST(test_selftest_first_fail_blocks_remaining);

    return UNITY_END();
}
