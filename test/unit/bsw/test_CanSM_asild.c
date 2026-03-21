/**
 * @file    test_CanSM_asild.c
 * @brief   Unit tests for CAN State Manager — bus-off recovery
 * @date    2026-03-21
 *
 * @verifies SSR-CVC-016, SSR-FZC-019, SWR-BSW-026
 *
 * TDD: Tests written FIRST to define desired behavior.
 */
#include "unity.h"
#include "CanSM.h"

/* ==================================================================
 * Mock: Can driver
 * ================================================================== */

static uint8 mock_can_mode;
static uint8 mock_can_mode_count;

Std_ReturnType Can_SetControllerMode(uint8 Controller, uint8 Mode)
{
    (void)Controller;
    mock_can_mode = Mode;
    mock_can_mode_count++;
    return E_OK;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static const CanSM_ConfigType test_config = {
    .L1_RecoveryTimeMs = 10u,
    .L1_MaxAttempts    = 3u,
    .L2_RecoveryTimeMs = 100u,
    .L2_MaxAttempts    = 2u,
};

void setUp(void)
{
    mock_can_mode = 0xFFu;
    mock_can_mode_count = 0u;
    CanSM_Init(&test_config);
}

void tearDown(void) {}

/* ==================================================================
 * Tests: Initialization
 * ================================================================== */

void test_CanSM_Init_sets_STOPPED(void)
{
    TEST_ASSERT_EQUAL(CANSM_STOPPED, CanSM_GetState());
    TEST_ASSERT_FALSE(CanSM_IsCommunicationAllowed());
}

void test_CanSM_Init_null_stays_uninit(void)
{
    CanSM_Init(NULL_PTR);
    TEST_ASSERT_EQUAL(CANSM_UNINIT, CanSM_GetState());
}

/* ==================================================================
 * Tests: Normal State Transitions
 * ================================================================== */

void test_CanSM_RequestComMode_transitions_to_STARTED(void)
{
    Std_ReturnType ret = CanSM_RequestComMode();
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(CANSM_STARTED, CanSM_GetState());
    TEST_ASSERT_TRUE(CanSM_IsCommunicationAllowed());
}

void test_CanSM_RequestComMode_twice_is_ok(void)
{
    CanSM_RequestComMode();
    Std_ReturnType ret = CanSM_RequestComMode();
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(CANSM_STARTED, CanSM_GetState());
}

/* ==================================================================
 * Tests: Bus-Off Recovery L1
 * ================================================================== */

void test_CanSM_BusOff_transitions_to_BUS_OFF(void)
{
    CanSM_RequestComMode();
    CanSM_ControllerBusOff();
    TEST_ASSERT_EQUAL(CANSM_BUS_OFF, CanSM_GetState());
    TEST_ASSERT_FALSE(CanSM_IsCommunicationAllowed());
}

void test_CanSM_L1_recovery_succeeds_after_10ms(void)
{
    CanSM_RequestComMode();
    CanSM_ControllerBusOff();

    /* Run MainFunction once (10ms) — L1 timer fires */
    CanSM_MainFunction();

    /* L1 attempt 1: controller restarted */
    TEST_ASSERT_EQUAL(CANSM_STARTED, CanSM_GetState());
}

void test_CanSM_L1_escalates_to_L2_after_max_attempts(void)
{
    CanSM_RequestComMode();

    /* Exhaust L1: 3 bus-off + recovery cycles */
    uint8 i;
    for (i = 0u; i < 3u; i++) {
        CanSM_ControllerBusOff();
        CanSM_MainFunction();  /* L1 recovery attempt */
        /* CanSM goes STARTED, then bus-off again */
    }

    /* 4th bus-off: L1 exhausted, should escalate to L2 */
    CanSM_ControllerBusOff();
    CanSM_MainFunction();  /* L1 attempt 4 — exceeds L1_MaxAttempts=3 */

    /* Still in BUS_OFF (L2 timer is 100ms, not fired yet) */
    TEST_ASSERT_EQUAL(CANSM_BUS_OFF, CanSM_GetState());
}

/* ==================================================================
 * Tests: Bus-Off Recovery L2
 * ================================================================== */

void test_CanSM_L2_recovery_after_100ms(void)
{
    CanSM_RequestComMode();

    /* Exhaust L1 */
    uint8 i;
    for (i = 0u; i < 4u; i++) {
        CanSM_ControllerBusOff();
        CanSM_MainFunction();
    }

    /* Now in L2. Run 10 MainFunction cycles (10ms each = 100ms) */
    for (i = 0u; i < 10u; i++) {
        CanSM_MainFunction();
    }

    /* L2 recovery attempt 1 should restart */
    TEST_ASSERT_EQUAL(CANSM_STARTED, CanSM_GetState());
}

/* ==================================================================
 * Tests: MainFunction does nothing outside BUS_OFF
 * ================================================================== */

void test_CanSM_MainFunction_noop_when_STARTED(void)
{
    CanSM_RequestComMode();
    uint8 prev_count = mock_can_mode_count;
    CanSM_MainFunction();
    /* No Can_SetControllerMode call — already STARTED */
    TEST_ASSERT_EQUAL(prev_count, mock_can_mode_count);
}

void test_CanSM_MainFunction_noop_when_STOPPED(void)
{
    uint8 prev_count = mock_can_mode_count;
    CanSM_MainFunction();
    TEST_ASSERT_EQUAL(prev_count, mock_can_mode_count);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_CanSM_Init_sets_STOPPED);
    RUN_TEST(test_CanSM_Init_null_stays_uninit);
    RUN_TEST(test_CanSM_RequestComMode_transitions_to_STARTED);
    RUN_TEST(test_CanSM_RequestComMode_twice_is_ok);
    RUN_TEST(test_CanSM_BusOff_transitions_to_BUS_OFF);
    RUN_TEST(test_CanSM_L1_recovery_succeeds_after_10ms);
    RUN_TEST(test_CanSM_L1_escalates_to_L2_after_max_attempts);
    RUN_TEST(test_CanSM_L2_recovery_after_100ms);
    RUN_TEST(test_CanSM_MainFunction_noop_when_STARTED);
    RUN_TEST(test_CanSM_MainFunction_noop_when_STOPPED);

    return UNITY_END();
}
