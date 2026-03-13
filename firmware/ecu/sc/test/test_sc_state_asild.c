/**
 * @file    test_sc_state_asild.c
 * @brief   ASIL D unit tests for SC state machine (GAP-SC-006)
 * @date    2026-03-09
 */

/* Include source directly (same pattern as other SC tests) */
#include "../src/sc_state.c"
#include "unity.h"

void setUp(void)
{
    SC_State_Init();
}

void tearDown(void) {}

/* ==================================================================
 * Init tests
 * ================================================================== */

void test_State_init_is_INIT(void)
{
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_INIT, SC_State_Get());
}

/* ==================================================================
 * Valid transitions
 * ================================================================== */

void test_State_INIT_to_MONITORING(void)
{
    TEST_ASSERT_TRUE(SC_State_Transition(SC_STATE_MONITORING));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_MONITORING, SC_State_Get());
}

void test_State_MONITORING_to_FAULT(void)
{
    (void)SC_State_Transition(SC_STATE_MONITORING);
    TEST_ASSERT_TRUE(SC_State_Transition(SC_STATE_FAULT));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_FAULT, SC_State_Get());
}

void test_State_MONITORING_to_KILL(void)
{
    (void)SC_State_Transition(SC_STATE_MONITORING);
    TEST_ASSERT_TRUE(SC_State_Transition(SC_STATE_KILL));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_KILL, SC_State_Get());
}

void test_State_FAULT_to_KILL(void)
{
    (void)SC_State_Transition(SC_STATE_MONITORING);
    (void)SC_State_Transition(SC_STATE_FAULT);
    TEST_ASSERT_TRUE(SC_State_Transition(SC_STATE_KILL));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_KILL, SC_State_Get());
}

/* ==================================================================
 * Invalid transitions — state must not change
 * ================================================================== */

void test_State_INIT_to_FAULT_rejected(void)
{
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_FAULT));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_INIT, SC_State_Get());
}

void test_State_INIT_to_KILL_rejected(void)
{
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_KILL));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_INIT, SC_State_Get());
}

void test_State_MONITORING_to_INIT_rejected(void)
{
    (void)SC_State_Transition(SC_STATE_MONITORING);
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_INIT));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_MONITORING, SC_State_Get());
}

void test_State_FAULT_to_INIT_rejected(void)
{
    (void)SC_State_Transition(SC_STATE_MONITORING);
    (void)SC_State_Transition(SC_STATE_FAULT);
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_INIT));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_FAULT, SC_State_Get());
}

void test_State_FAULT_to_MONITORING_rejected(void)
{
    (void)SC_State_Transition(SC_STATE_MONITORING);
    (void)SC_State_Transition(SC_STATE_FAULT);
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_MONITORING));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_FAULT, SC_State_Get());
}

void test_State_KILL_terminal(void)
{
    (void)SC_State_Transition(SC_STATE_MONITORING);
    (void)SC_State_Transition(SC_STATE_KILL);

    /* No transition out of KILL */
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_INIT));
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_MONITORING));
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_FAULT));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_KILL, SC_State_Get());
}

/* ==================================================================
 * Edge case: duplicate transition to same state
 * ================================================================== */

void test_State_MONITORING_to_MONITORING_rejected(void)
{
    (void)SC_State_Transition(SC_STATE_MONITORING);
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_MONITORING));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_MONITORING, SC_State_Get());
}

void test_State_FAULT_to_FAULT_rejected(void)
{
    (void)SC_State_Transition(SC_STATE_MONITORING);
    (void)SC_State_Transition(SC_STATE_FAULT);
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_FAULT));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_FAULT, SC_State_Get());
}

/* ==================================================================
 * Edge case: invalid state value forces KILL (fail-closed)
 * ================================================================== */

void test_State_unknown_forces_KILL(void)
{
    /* Corrupt internal state directly */
    sc_state = 0xFFu;
    TEST_ASSERT_FALSE(SC_State_Transition(SC_STATE_MONITORING));
    TEST_ASSERT_EQUAL_UINT8(SC_STATE_KILL, SC_State_Get());
}

/* ==================================================================
 * Test Runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Init */
    RUN_TEST(test_State_init_is_INIT);

    /* Valid transitions */
    RUN_TEST(test_State_INIT_to_MONITORING);
    RUN_TEST(test_State_MONITORING_to_FAULT);
    RUN_TEST(test_State_MONITORING_to_KILL);
    RUN_TEST(test_State_FAULT_to_KILL);

    /* Invalid transitions */
    RUN_TEST(test_State_INIT_to_FAULT_rejected);
    RUN_TEST(test_State_INIT_to_KILL_rejected);
    RUN_TEST(test_State_MONITORING_to_INIT_rejected);
    RUN_TEST(test_State_FAULT_to_INIT_rejected);
    RUN_TEST(test_State_FAULT_to_MONITORING_rejected);
    RUN_TEST(test_State_KILL_terminal);

    /* Edge cases */
    RUN_TEST(test_State_MONITORING_to_MONITORING_rejected);
    RUN_TEST(test_State_FAULT_to_FAULT_rejected);
    RUN_TEST(test_State_unknown_forces_KILL);

    return UNITY_END();
}
