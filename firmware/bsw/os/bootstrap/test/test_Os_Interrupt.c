/**
 * @file    test_Os_Interrupt.c
 * @brief   Unit tests for OSEK interrupt control APIs
 * @date    2026-03-14
 *
 * @details Tests the 6 mandatory OSEK/VDX OS interrupt control functions:
 *          - DisableAllInterrupts / EnableAllInterrupts (non-nestable)
 *          - SuspendAllInterrupts / ResumeAllInterrupts (nestable)
 *          - SuspendOSInterrupts / ResumeOSInterrupts (nestable, Cat2 only)
 *
 * @standard OSEK/VDX OS 2.2.3 Section 13.3
 * @copyright Taktflow Systems 2026
 */

#include "unity.h"
#include "Os.h"

/* Port-layer spy counters (set by UNIT_TEST stubs in Os_Interrupt.c) */
extern uint8 os_test_all_interrupts_disabled;
extern uint8 os_test_os_interrupts_disabled;

void setUp(void)
{
    Os_TestReset();
}

void tearDown(void)
{
}

/* ==================================================================
 * DisableAllInterrupts / EnableAllInterrupts — non-nestable pair
 * ================================================================== */

/**
 * @brief   DisableAllInterrupts disables, EnableAllInterrupts re-enables
 */
void test_DisableAllInterrupts_then_EnableAllInterrupts(void)
{
    DisableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_all_interrupts_disabled);

    EnableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_all_interrupts_disabled);
}

/**
 * @brief   Non-nestable: second Disable without Enable is an error (no crash)
 *          OSEK says behavior is undefined, but we should at least not corrupt state.
 */
void test_DisableAllInterrupts_not_nestable(void)
{
    DisableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_all_interrupts_disabled);

    /* Second call — should stay disabled, not increment a counter */
    DisableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_all_interrupts_disabled);

    /* One Enable should restore */
    EnableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_all_interrupts_disabled);
}

/**
 * @brief   EnableAllInterrupts without prior Disable is a no-op
 */
void test_EnableAllInterrupts_without_Disable_is_noop(void)
{
    EnableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_all_interrupts_disabled);
}

/* ==================================================================
 * SuspendAllInterrupts / ResumeAllInterrupts — nestable pair
 * ================================================================== */

/**
 * @brief   Basic suspend/resume pair
 */
void test_SuspendAllInterrupts_then_ResumeAllInterrupts(void)
{
    SuspendAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_all_interrupts_disabled);

    ResumeAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_all_interrupts_disabled);
}

/**
 * @brief   Nestable: 3 suspends require 3 resumes
 */
void test_SuspendAllInterrupts_nesting(void)
{
    SuspendAllInterrupts();
    SuspendAllInterrupts();
    SuspendAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_all_interrupts_disabled);

    ResumeAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_all_interrupts_disabled);

    ResumeAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_all_interrupts_disabled);

    ResumeAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_all_interrupts_disabled);
}

/**
 * @brief   ResumeAllInterrupts without prior Suspend is a no-op
 */
void test_ResumeAllInterrupts_without_Suspend_is_noop(void)
{
    ResumeAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_all_interrupts_disabled);
}

/* ==================================================================
 * SuspendOSInterrupts / ResumeOSInterrupts — nestable, Cat2 only
 * ================================================================== */

/**
 * @brief   Basic OS-level suspend/resume pair
 */
void test_SuspendOSInterrupts_then_ResumeOSInterrupts(void)
{
    SuspendOSInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_os_interrupts_disabled);

    ResumeOSInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_os_interrupts_disabled);
}

/**
 * @brief   Nestable: 2 suspends require 2 resumes
 */
void test_SuspendOSInterrupts_nesting(void)
{
    SuspendOSInterrupts();
    SuspendOSInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_os_interrupts_disabled);

    ResumeOSInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_os_interrupts_disabled);

    ResumeOSInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_os_interrupts_disabled);
}

/**
 * @brief   ResumeOSInterrupts without prior Suspend is a no-op
 */
void test_ResumeOSInterrupts_without_Suspend_is_noop(void)
{
    ResumeOSInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_os_interrupts_disabled);
}

/**
 * @brief   SuspendOS only disables Cat2, not all — verify spy counters
 */
void test_SuspendOSInterrupts_does_not_disable_all(void)
{
    SuspendOSInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_os_interrupts_disabled);
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_all_interrupts_disabled);

    ResumeOSInterrupts();
}

/* ==================================================================
 * Cross-pair independence
 * ================================================================== */

/**
 * @brief   SuspendAll and SuspendOS are independent nesting counters
 */
void test_SuspendAll_and_SuspendOS_independent(void)
{
    SuspendAllInterrupts();
    SuspendOSInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_all_interrupts_disabled);
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_os_interrupts_disabled);

    ResumeOSInterrupts();
    TEST_ASSERT_EQUAL_UINT8(1u, os_test_all_interrupts_disabled);
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_os_interrupts_disabled);

    ResumeAllInterrupts();
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_all_interrupts_disabled);
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_os_interrupts_disabled);
}

/**
 * @brief   Os_TestReset clears all interrupt state
 */
void test_Reset_clears_interrupt_state(void)
{
    SuspendAllInterrupts();
    SuspendOSInterrupts();
    DisableAllInterrupts();

    Os_TestReset();

    TEST_ASSERT_EQUAL_UINT8(0u, os_test_all_interrupts_disabled);
    TEST_ASSERT_EQUAL_UINT8(0u, os_test_os_interrupts_disabled);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* DisableAllInterrupts / EnableAllInterrupts */
    RUN_TEST(test_DisableAllInterrupts_then_EnableAllInterrupts);
    RUN_TEST(test_DisableAllInterrupts_not_nestable);
    RUN_TEST(test_EnableAllInterrupts_without_Disable_is_noop);

    /* SuspendAllInterrupts / ResumeAllInterrupts */
    RUN_TEST(test_SuspendAllInterrupts_then_ResumeAllInterrupts);
    RUN_TEST(test_SuspendAllInterrupts_nesting);
    RUN_TEST(test_ResumeAllInterrupts_without_Suspend_is_noop);

    /* SuspendOSInterrupts / ResumeOSInterrupts */
    RUN_TEST(test_SuspendOSInterrupts_then_ResumeOSInterrupts);
    RUN_TEST(test_SuspendOSInterrupts_nesting);
    RUN_TEST(test_ResumeOSInterrupts_without_Suspend_is_noop);
    RUN_TEST(test_SuspendOSInterrupts_does_not_disable_all);

    /* Cross-pair */
    RUN_TEST(test_SuspendAll_and_SuspendOS_independent);
    RUN_TEST(test_Reset_clears_interrupt_state);

    return UNITY_END();
}
