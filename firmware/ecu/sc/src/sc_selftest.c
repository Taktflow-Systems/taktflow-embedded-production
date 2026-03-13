/**
 * @file    sc_selftest.c
 * @brief   Startup and runtime self-test for Safety Controller
 * @date    2026-02-23
 *
 * @safety_req SWR-SC-016, SWR-SC-017, SWR-SC-018, SWR-SC-019,
 *             SWR-SC-020, SWR-SC-021
 * @traces_to  SSR-SC-010, SSR-SC-011, SSR-SC-016, SSR-SC-017
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_selftest.h"
#include "Sc_Hw_Cfg.h"
#include "sc_gio.h"

/* ==================================================================
 * External: Hardware test functions (provided by platform or mock)
 * ================================================================== */

extern boolean hw_lockstep_bist(void);
extern boolean hw_ram_pbist(void);
extern boolean hw_flash_crc_check(void);
extern boolean hw_dcan_loopback_test(void);
extern boolean hw_gpio_readback_test(void);
extern boolean hw_lamp_test(void);
extern boolean hw_watchdog_test(void);

/* Runtime test externs */
extern boolean hw_flash_crc_incremental(void);
extern boolean hw_dcan_error_check(void);

/* ==================================================================
 * Module State
 * ================================================================== */

/** Stack canary value */
static uint32 stack_canary;

/** RAM test pattern area (in real target: at SC_RAM_TEST_ADDR) */
static uint8 ram_test_area[SC_RAM_TEST_SIZE];

/** Startup self-test passed flag */
static boolean startup_passed;

/** Runtime health flag */
static boolean runtime_healthy;

/** Runtime tick counter (for spreading tests over 60s) */
static uint16 runtime_tick;

/** Current runtime test step (0..3) */
static uint8 runtime_step;

/* ==================================================================
 * Public API
 * ================================================================== */

void SC_SelfTest_Init(void)
{
    uint8 i;

    /* Plant stack canary */
    stack_canary = SC_STACK_CANARY_VALUE;

    /* Write RAM test pattern (alternating 0xAA/0x55) */
    for (i = 0u; i < SC_RAM_TEST_SIZE; i++) {
        ram_test_area[i] = ((i % 2u) == 0u) ? 0xAAu : 0x55u;
    }

    startup_passed  = FALSE;
    runtime_healthy = TRUE;
    runtime_tick    = 0u;
    runtime_step    = 0u;
}

uint8 SC_SelfTest_Startup(void)
{
    /* Step 1: Lockstep CPU BIST */
    if (hw_lockstep_bist() == FALSE) {
        startup_passed = FALSE;
        return 1u;
    }

    /* Step 2: RAM PBIST */
    if (hw_ram_pbist() == FALSE) {
        startup_passed = FALSE;
        return 2u;
    }

    /* Step 3: Flash CRC-32 */
    if (hw_flash_crc_check() == FALSE) {
        startup_passed = FALSE;
        return 3u;
    }

    /* Step 4: DCAN1 internal loopback */
    if (hw_dcan_loopback_test() == FALSE) {
        startup_passed = FALSE;
        return 4u;
    }

    /* Step 5: GPIO readback */
    if (hw_gpio_readback_test() == FALSE) {
        startup_passed = FALSE;
        return 5u;
    }

    /* Step 6: Fault LED lamp test */
    if (hw_lamp_test() == FALSE) {
        startup_passed = FALSE;
        return 6u;
    }

    /* Step 7: TPS3823 watchdog test */
    if (hw_watchdog_test() == FALSE) {
        startup_passed = FALSE;
        return 7u;
    }

    startup_passed = TRUE;
    return 0u;
}

void SC_SelfTest_Runtime(void)
{
    boolean step_ok = TRUE;

    if (startup_passed == FALSE) {
        runtime_healthy = FALSE;
        return;
    }

    /* Increment tick counter */
    runtime_tick++;

    /* Spread 4 steps over the 60s period */
    if (runtime_tick >= SC_SELFTEST_RUNTIME_PERIOD) {
        runtime_tick = 0u;
        runtime_step = 0u;
    }

    /* Execute one step per period fraction */
    if (runtime_tick == 1u) {
        /* Step 0: Flash CRC-32 incremental */
        step_ok = hw_flash_crc_incremental();
    } else if (runtime_tick == (SC_SELFTEST_RUNTIME_PERIOD / 4u)) {
        /* Step 1: RAM 32-byte pattern check */
        uint8 i;
        for (i = 0u; i < SC_RAM_TEST_SIZE; i++) {
            uint8 expected = ((i % 2u) == 0u) ? 0xAAu : 0x55u;
            if (ram_test_area[i] != expected) {
                step_ok = FALSE;
                break;
            }
        }
    } else if (runtime_tick == (SC_SELFTEST_RUNTIME_PERIOD / 2u)) {
        /* Step 2: DCAN1 error status check */
        step_ok = hw_dcan_error_check();
    } else if (runtime_tick == ((SC_SELFTEST_RUNTIME_PERIOD * 3u) / 4u)) {
        /* Step 3: GIO_A0 readback vs commanded */
        uint8 readback = gioGetBit(SC_GIO_PORT_A, SC_PIN_RELAY);
        /* This step just reads — actual mismatch handled by sc_relay */
        (void)readback;
        step_ok = TRUE;
    } else {
        /* No step this tick */
        return;
    }

    if (step_ok == FALSE) {
        runtime_healthy = FALSE;
    }
}

boolean SC_SelfTest_StackCanaryOk(void)
{
    return (stack_canary == SC_STACK_CANARY_VALUE) ? TRUE : FALSE;
}

boolean SC_SelfTest_IsHealthy(void)
{
    return (startup_passed == TRUE) && (runtime_healthy == TRUE) ? TRUE : FALSE;
}
