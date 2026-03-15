/**
 * @file    test_Os_TimingProt.c
 * @brief   Unit tests for AUTOSAR OS timing protection (Phase 3B)
 * @date    2026-03-14
 *
 * @details Tests for execution budget enforcement, inter-arrival time
 *          checking, and ProtectionHook invocation per AUTOSAR OS §7.10.
 *
 * @standard AUTOSAR OS §7.10, OSEK/VDX OS 2.2.3
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "Os.h"
#include "Os_Internal.h"

/* ==================================================================
 * Port spy — UNIT_TEST replaces real hardware timer with spy state
 * ================================================================== */

extern uint32 os_test_tp_armed_budget_us;
extern boolean os_test_tp_armed;
extern uint32 os_test_tp_elapsed_us;

/* ==================================================================
 * ProtectionHook spy
 * ================================================================== */

static StatusType observed_protection_error = E_OK;
static uint8 protection_hook_call_count = 0u;
static ProtectionReturnType protection_hook_return_value = PRO_TERMINATETASKISR;

static ProtectionReturnType test_protection_hook(StatusType FatalError)
{
    observed_protection_error = FatalError;
    protection_hook_call_count++;
    return protection_hook_return_value;
}

/* ==================================================================
 * Task stubs
 * ================================================================== */

static void task_stub(void) { /* no-op */ }

/* ==================================================================
 * Helpers
 * ================================================================== */

static void setup_single_task(void)
{
    Os_TaskConfigType cfg[1] = {
        { "Task0", task_stub, 2u, 2u, 0x01u, FALSE, FULL }
    };

    Os_TestReset();
    (void)Os_TestConfigureTasks(cfg, 1u);
}


/* ==================================================================
 * setUp / tearDown
 * ================================================================== */

void setUp(void)
{
    observed_protection_error = E_OK;
    protection_hook_call_count = 0u;
    protection_hook_return_value = PRO_TERMINATETASKISR;
}

void tearDown(void) { }

/* ==================================================================
 * TP-01: Config — per-task budget and inter-arrival
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Timing protection config with zero budget disables enforcement.
 */
void test_zero_budget_means_no_enforcement(void)
{
    Os_TimingProtConfigType tp_cfg = { 0u, 0u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    /* Simulate elapsed time beyond any reasonable budget */
    os_test_tp_elapsed_us = 1000000u;

    /* Budget expiry should not trigger anything because budget is 0 (disabled) */
    Os_TimingProtBudgetExpired();
    TEST_ASSERT_EQUAL_UINT8(0u, protection_hook_call_count);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Non-zero budget arms port timer on task dispatch.
 */
void test_nonzero_budget_arms_timer_on_dispatch(void)
{
    Os_TimingProtConfigType tp_cfg = { 5000u, 0u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    Os_TestSetProtectionHook(test_protection_hook);
    StartOS(OSDEFAULTAPPMODE);

    /* Task ran and completed (disarmed). Manually start it to verify arming. */
    Os_TestSetCurrentTaskRunning(0u);
    Os_TimingProtStart(0u);
    TEST_ASSERT_TRUE(os_test_tp_armed);
    TEST_ASSERT_EQUAL_UINT32(5000u, os_test_tp_armed_budget_us);
}

/* ==================================================================
 * TP-02/03: Start/Stop — arm/disarm on dispatch/preemption
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Task termination disarms timing protection.
 */
void test_task_termination_disarms_timer(void)
{
    Os_TimingProtConfigType tp_cfg = { 5000u, 0u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    /* Terminating the task should disarm the timer */
    TerminateTask();
    TEST_ASSERT_FALSE(os_test_tp_armed);
}

/* ==================================================================
 * TP-04: Budget expired — ProtectionHook invoked
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Budget expiry calls ProtectionHook with E_OS_PROTECTION_TIME.
 */
void test_budget_expired_calls_protection_hook(void)
{
    Os_TimingProtConfigType tp_cfg = { 5000u, 0u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    Os_TestSetProtectionHook(test_protection_hook);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    /* Simulate timer expiry */
    os_test_tp_elapsed_us = 6000u;
    Os_TimingProtBudgetExpired();

    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_TIME, observed_protection_error);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Budget expiry with no ProtectionHook calls ShutdownOS.
 */
void test_budget_expired_no_hook_shuts_down(void)
{
    Os_TimingProtConfigType tp_cfg = { 5000u, 0u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    /* No protection hook set */
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    os_test_tp_elapsed_us = 6000u;
    Os_TimingProtBudgetExpired();

    TEST_ASSERT_TRUE(os_shutdown_requested);
}

/* ==================================================================
 * TP-05: Inter-arrival time check
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Activation too soon returns E_OS_PROTECTION_ARRIVAL.
 */
void test_inter_arrival_violation_rejects_activate(void)
{
    Os_TimingProtConfigType tp_cfg = { 0u, 10000u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    Os_TestSetProtectionHook(test_protection_hook);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    /* First activation already happened via StartOS autostart.
     * Second activation too soon (elapsed=5000, need 10000). */
    os_test_tp_elapsed_us = 5000u;
    StatusType status = ActivateTask(0u);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_ARRIVAL, status);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Activation after sufficient interval succeeds.
 */
void test_inter_arrival_ok_allows_activate(void)
{
    Os_TimingProtConfigType tp_cfg = { 0u, 10000u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    Os_TestSetProtectionHook(test_protection_hook);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    /* Enough time has passed */
    os_test_tp_elapsed_us = 15000u;
    StatusType status = ActivateTask(0u);
    TEST_ASSERT_TRUE(status != E_OS_PROTECTION_ARRIVAL);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Zero inter-arrival time disables the check.
 */
void test_zero_inter_arrival_disables_check(void)
{
    Os_TimingProtConfigType tp_cfg = { 0u, 0u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    os_test_tp_elapsed_us = 1u;
    StatusType status = ActivateTask(0u);
    TEST_ASSERT_TRUE(status != E_OS_PROTECTION_ARRIVAL);
}

/* ==================================================================
 * TP-06/07: ProtectionHook return actions
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10.1
 * @requirement PRO_TERMINATETASKISR kills the offending task, OS continues.
 */
void test_protection_hook_terminate_task(void)
{
    Os_TimingProtConfigType tp_cfg = { 5000u, 0u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    Os_TestSetProtectionHook(test_protection_hook);
    protection_hook_return_value = PRO_TERMINATETASKISR;
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    os_test_tp_elapsed_us = 6000u;
    Os_TimingProtBudgetExpired();

    /* Task should be terminated, OS continues */
    TEST_ASSERT_FALSE(os_shutdown_requested);
    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
}

/**
 * @spec AUTOSAR OS §7.10.1
 * @requirement PRO_SHUTDOWN calls ShutdownOS.
 */
void test_protection_hook_shutdown(void)
{
    Os_TimingProtConfigType tp_cfg = { 5000u, 0u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    Os_TestSetProtectionHook(test_protection_hook);
    protection_hook_return_value = PRO_SHUTDOWN;
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    os_test_tp_elapsed_us = 6000u;
    Os_TimingProtBudgetExpired();

    TEST_ASSERT_TRUE(os_shutdown_requested);
}

/* ==================================================================
 * TP-RESET: Reset clears timing protection state
 * ================================================================== */

/**
 * @spec Internal
 * @requirement Os_TimingProtReset clears all per-task timing state.
 */
void test_reset_clears_timing_state(void)
{
    Os_TimingProtConfigType tp_cfg = { 5000u, 10000u };

    setup_single_task();
    Os_TimingProtConfigure(0u, &tp_cfg);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    Os_TestReset();

    /* After reset, no timer should be armed */
    TEST_ASSERT_FALSE(os_test_tp_armed);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Budget expiry for task without budget configured is ignored.
 */
void test_budget_expired_no_config_is_noop(void)
{
    setup_single_task();
    /* No timing protection configured */
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    os_test_tp_elapsed_us = 999999u;
    Os_TimingProtBudgetExpired();

    TEST_ASSERT_EQUAL_UINT8(0u, protection_hook_call_count);
    TEST_ASSERT_FALSE(os_shutdown_requested);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Config */
    RUN_TEST(test_zero_budget_means_no_enforcement);
    RUN_TEST(test_nonzero_budget_arms_timer_on_dispatch);

    /* Start/Stop */
    RUN_TEST(test_task_termination_disarms_timer);

    /* Budget expired */
    RUN_TEST(test_budget_expired_calls_protection_hook);
    RUN_TEST(test_budget_expired_no_hook_shuts_down);
    RUN_TEST(test_budget_expired_no_config_is_noop);

    /* Inter-arrival */
    RUN_TEST(test_inter_arrival_violation_rejects_activate);
    RUN_TEST(test_inter_arrival_ok_allows_activate);
    RUN_TEST(test_zero_inter_arrival_disables_check);

    /* ProtectionHook actions */
    RUN_TEST(test_protection_hook_terminate_task);
    RUN_TEST(test_protection_hook_shutdown);

    /* Reset */
    RUN_TEST(test_reset_clears_timing_state);

    return UNITY_END();
}
