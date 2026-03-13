/**
 * @file    test_sc_led.c
 * @brief   Unit tests for sc_led — fault LED panel driver
 * @date    2026-02-23
 *
 * @verifies SWR-SC-013
 *
 * Tests LED initialization, steady ON, blink pattern, and OFF state.
 *
 * Mocks: GIO registers.
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions
 * ================================================================== */

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int       uint32;
typedef uint8               boolean;

#define TRUE                1u
#define FALSE               0u

/* ==================================================================
 * SC Configuration Constants
 * ================================================================== */

#define SC_GIO_PORT_A               0u
#define SC_GIO_PORT_B               1u
#define SC_PIN_LED_CVC              1u
#define SC_PIN_LED_FZC              2u
#define SC_PIN_LED_RZC              3u
#define SC_PIN_LED_SYS              4u
#define SC_LED_BLINK_ON_TICKS       25u
#define SC_LED_BLINK_OFF_TICKS      25u
#define SC_LED_BLINK_PERIOD         50u

#define SC_LED_OFF                  0u
#define SC_LED_BLINK                1u
#define SC_LED_ON                   2u

/* ==================================================================
 * Mock: GIO Register Access
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

/* ==================================================================
 * Include source under test
 * ================================================================== */

#include "../src/sc_led.c"

/* ==================================================================
 * Test setup / teardown
 * ================================================================== */

void setUp(void)
{
    uint8 i;
    for (i = 0u; i < 8u; i++) {
        mock_gio_a[i] = 0u;
        mock_gio_b[i] = 0u;
    }
    SC_LED_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-SC-013: LED States
 * ================================================================== */

/** @verifies SWR-SC-013 -- Init turns all LEDs off */
void test_LED_Init_all_off(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_CVC]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_FZC]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_RZC]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_SYS]);
}

/** @verifies SWR-SC-013 -- Steady ON drives LED HIGH */
void test_LED_steady_on(void)
{
    SC_LED_SetState(SC_LED_IDX_CVC, SC_LED_ON);
    SC_LED_Update();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_CVC]);
}

/** @verifies SWR-SC-013 -- Blink toggles with 500ms period */
void test_LED_blink_pattern(void)
{
    uint8 i;
    SC_LED_SetState(SC_LED_IDX_FZC, SC_LED_BLINK);

    /* First 25 ticks: ON */
    for (i = 0u; i < SC_LED_BLINK_ON_TICKS; i++) {
        SC_LED_Update();
    }
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_FZC]);

    /* Next 25 ticks: OFF */
    for (i = 0u; i < SC_LED_BLINK_OFF_TICKS; i++) {
        SC_LED_Update();
    }
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_FZC]);
}

/** @verifies SWR-SC-013 -- OFF state keeps LED low */
void test_LED_off_stays_low(void)
{
    SC_LED_SetState(SC_LED_IDX_RZC, SC_LED_ON);
    SC_LED_Update();
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_RZC]);

    SC_LED_SetState(SC_LED_IDX_RZC, SC_LED_OFF);
    SC_LED_Update();
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_RZC]);
}

/* ==================================================================
 * HARDENED TESTS — Boundary Values, Fault Injection
 * ================================================================== */

/** @verifies SWR-SC-013
 *  Equivalence class: Boundary — blink transition at exactly tick 25 (ON->OFF boundary) */
void test_led_blink_boundary_on_to_off(void)
{
    uint8 i;
    SC_LED_SetState(SC_LED_IDX_CVC, SC_LED_BLINK);

    /* Run exactly 25 ticks — should be ON during the 25th tick */
    for (i = 0u; i < SC_LED_BLINK_ON_TICKS; i++) {
        SC_LED_Update();
    }
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_CVC]);

    /* One more tick transitions to OFF phase */
    SC_LED_Update();
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_CVC]);
}

/** @verifies SWR-SC-013
 *  Equivalence class: Boundary — blink transition at exactly tick 50 (OFF->ON boundary) */
void test_led_blink_boundary_off_to_on(void)
{
    uint8 i;
    SC_LED_SetState(SC_LED_IDX_CVC, SC_LED_BLINK);

    /* Run full period: 25 ON + 25 OFF = 50 ticks */
    for (i = 0u; i < SC_LED_BLINK_PERIOD; i++) {
        SC_LED_Update();
    }
    /* After full period, should be OFF (end of OFF phase) */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_CVC]);

    /* One more tick starts the next ON phase */
    SC_LED_Update();
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_CVC]);
}

/** @verifies SWR-SC-013
 *  Equivalence class: Boundary — multiple LEDs simultaneously in different states */
void test_led_multiple_states(void)
{
    uint8 i;
    SC_LED_SetState(SC_LED_IDX_CVC, SC_LED_ON);
    SC_LED_SetState(SC_LED_IDX_FZC, SC_LED_OFF);
    SC_LED_SetState(SC_LED_IDX_RZC, SC_LED_BLINK);
    SC_LED_SetState(SC_LED_IDX_SYS, SC_LED_ON);

    SC_LED_Update();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_CVC]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_FZC]);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_RZC]);  /* First blink tick = ON */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_SYS]);

    /* Run through blink ON phase into OFF phase */
    for (i = 0u; i < SC_LED_BLINK_ON_TICKS; i++) {
        SC_LED_Update();
    }

    /* CVC and SYS stay ON, FZC stays OFF, RZC is now in OFF phase */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_CVC]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_FZC]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_RZC]);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_SYS]);
}

/** @verifies SWR-SC-013
 *  Equivalence class: Boundary — state transition ON -> BLINK resets blink counter */
void test_led_on_to_blink_transition(void)
{
    uint8 i;
    SC_LED_SetState(SC_LED_IDX_CVC, SC_LED_ON);
    SC_LED_Update();
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_CVC]);

    /* Switch to blink — should start a fresh blink cycle */
    SC_LED_SetState(SC_LED_IDX_CVC, SC_LED_BLINK);

    /* First blink cycle ON phase */
    for (i = 0u; i < SC_LED_BLINK_ON_TICKS; i++) {
        SC_LED_Update();
    }
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_CVC]);

    /* Should transition to OFF */
    SC_LED_Update();
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_CVC]);
}

/** @verifies SWR-SC-013
 *  Equivalence class: Boundary — BLINK -> OFF immediately turns LED off */
void test_led_blink_to_off(void)
{
    uint8 i;
    SC_LED_SetState(SC_LED_IDX_FZC, SC_LED_BLINK);

    /* Run a few ticks into ON phase */
    for (i = 0u; i < 10u; i++) {
        SC_LED_Update();
    }
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_FZC]);

    /* Switch to OFF mid-blink */
    SC_LED_SetState(SC_LED_IDX_FZC, SC_LED_OFF);
    SC_LED_Update();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_FZC]);
}

/** @verifies SWR-SC-013
 *  Equivalence class: Boundary — system LED (SYS) steady ON */
void test_led_sys_steady_on(void)
{
    SC_LED_SetState(SC_LED_IDX_SYS, SC_LED_ON);
    SC_LED_Update();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_SYS]);
}

/** @verifies SWR-SC-013
 *  Equivalence class: Boundary — long blink sequence maintains periodicity */
void test_led_blink_long_sequence(void)
{
    uint8 i;
    uint8 cycle;
    SC_LED_SetState(SC_LED_IDX_RZC, SC_LED_BLINK);

    /* Run 5 full blink cycles */
    for (cycle = 0u; cycle < 5u; cycle++) {
        /* ON phase */
        for (i = 0u; i < SC_LED_BLINK_ON_TICKS; i++) {
            SC_LED_Update();
        }
        TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a[SC_PIN_LED_RZC]);

        /* OFF phase */
        for (i = 0u; i < SC_LED_BLINK_OFF_TICKS; i++) {
            SC_LED_Update();
        }
        TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_RZC]);
    }
}

/** @verifies SWR-SC-013
 *  Equivalence class: Boundary — Update with no state changes is safe */
void test_led_update_no_state(void)
{
    /* All LEDs are OFF after Init, just call Update repeatedly */
    uint8 i;
    for (i = 0u; i < 100u; i++) {
        SC_LED_Update();
    }

    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_CVC]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_FZC]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_RZC]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a[SC_PIN_LED_SYS]);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_LED_Init_all_off);
    RUN_TEST(test_LED_steady_on);
    RUN_TEST(test_LED_blink_pattern);
    RUN_TEST(test_LED_off_stays_low);

    /* Hardened tests — boundary values, fault injection */
    RUN_TEST(test_led_blink_boundary_on_to_off);
    RUN_TEST(test_led_blink_boundary_off_to_on);
    RUN_TEST(test_led_multiple_states);
    RUN_TEST(test_led_on_to_blink_transition);
    RUN_TEST(test_led_blink_to_off);
    RUN_TEST(test_led_sys_steady_on);
    RUN_TEST(test_led_blink_long_sequence);
    RUN_TEST(test_led_update_no_state);

    return UNITY_END();
}
