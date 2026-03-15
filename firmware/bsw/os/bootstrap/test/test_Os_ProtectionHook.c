/**
 * @file    test_Os_ProtectionHook.c
 * @brief   Phase 3D integration tests — ProtectionHook wired to all 3 domains
 * @date    2026-03-15
 *
 * @details Validates that ProtectionHook is invoked from all three SC3
 *          protection domains:
 *          - Service protection (call-level violations)
 *          - Timing protection (budget expiry + inter-arrival)
 *          - Memory protection (MPU fault)
 *
 *          Phase 3A/3B/3C tested each domain in isolation. These tests
 *          verify the unified hook dispatch path.
 *
 * @standard AUTOSAR OS §7.6.1, §7.9, §7.10
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "Os.h"
#include "Os_Internal.h"

/* ==================================================================
 * Port spy — UNIT_TEST stubs
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

static const Os_TaskConfigType two_task_cfg[] = {
    { "T0", task_stub, 2u, 2u, 0x01u, FALSE, FULL },
    { "T1", task_stub, 1u, 1u, 0x00u, FALSE, FULL }
};

/* ==================================================================
 * setUp / tearDown
 * ================================================================== */

void setUp(void)
{
    observed_protection_error = E_OK;
    protection_hook_call_count = 0u;
    protection_hook_return_value = PRO_TERMINATETASKISR;

    Os_TestReset();
    Os_TestConfigureTasks(two_task_cfg, 2u);
    Os_TestSetProtectionHook(test_protection_hook);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);
}

void tearDown(void) { }

/* ==================================================================
 * 3D-SP: Service protection → ProtectionHook
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement Call-level violation in SC3 shall invoke ProtectionHook
 *              with E_OS_CALLEVEL, not just return an error code.
 * @verify      ProtectionHook called once with E_OS_CALLEVEL
 */
void test_call_level_violation_invokes_protection_hook(void)
{
    os_call_level = OS_CALLLEVEL_ISR2;

    /* Schedule is TASK-only — calling from ISR2 is a violation */
    StatusType status = Schedule();

    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, status);
    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, observed_protection_error);

    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement TerminateTask from ErrorHook → ProtectionHook(E_OS_CALLEVEL).
 */
void test_terminate_task_from_error_hook_invokes_protection_hook(void)
{
    os_call_level = OS_CALLLEVEL_ERROR_HOOK;

    StatusType status = TerminateTask();

    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, status);
    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, observed_protection_error);

    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement WaitEvent from ISR2 → ProtectionHook(E_OS_CALLEVEL).
 */
void test_wait_event_from_isr2_invokes_protection_hook(void)
{
    os_call_level = OS_CALLLEVEL_ISR2;

    StatusType status = WaitEvent(0x01u);

    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, status);
    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, observed_protection_error);

    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement Call-level violation with no ProtectionHook calls ShutdownOS.
 */
void test_call_level_violation_no_hook_shuts_down(void)
{
    Os_TestSetProtectionHook((Os_ProtectionHookType)0);

    os_call_level = OS_CALLLEVEL_ISR2;
    StatusType status = Schedule();

    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, status);
    TEST_ASSERT_TRUE(os_shutdown_requested);

    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement ProtectionHook returning PRO_SHUTDOWN causes ShutdownOS.
 */
void test_call_level_violation_hook_returns_shutdown(void)
{
    protection_hook_return_value = PRO_SHUTDOWN;

    os_call_level = OS_CALLLEVEL_ISR2;
    StatusType status = Schedule();

    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, status);
    TEST_ASSERT_TRUE(os_shutdown_requested);

    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement ProtectionHook returning PRO_TERMINATETASKISR kills task,
 *              OS continues.
 */
void test_call_level_violation_hook_terminates_task(void)
{
    protection_hook_return_value = PRO_TERMINATETASKISR;

    os_call_level = OS_CALLLEVEL_ISR2;
    StatusType status = Schedule();

    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, status);
    TEST_ASSERT_FALSE(os_shutdown_requested);
    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);

    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement Valid call-level does NOT invoke ProtectionHook.
 */
void test_valid_call_level_does_not_invoke_hook(void)
{
    /* ActivateTask is allowed at TASK level */
    StatusType status = ActivateTask(1u);

    TEST_ASSERT_EQUAL_UINT8(0u, protection_hook_call_count);
    (void)status;
}

/* ==================================================================
 * 3D-TP: Inter-arrival violation → ProtectionHook
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Inter-arrival violation shall invoke ProtectionHook
 *              with E_OS_PROTECTION_ARRIVAL.
 */
void test_inter_arrival_violation_invokes_protection_hook(void)
{
    Os_TimingProtConfigType tp_cfg = { 0u, 10000u };
    Os_TimingProtConfigure(0u, &tp_cfg);

    /* Too soon — elapsed only 5000us, need 10000us */
    os_test_tp_elapsed_us = 5000u;
    StatusType status = ActivateTask(0u);

    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_ARRIVAL, status);
    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_ARRIVAL, observed_protection_error);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Inter-arrival violation with no hook calls ShutdownOS.
 */
void test_inter_arrival_violation_no_hook_shuts_down(void)
{
    Os_TimingProtConfigType tp_cfg = { 0u, 10000u };
    Os_TimingProtConfigure(0u, &tp_cfg);
    Os_TestSetProtectionHook((Os_ProtectionHookType)0);

    os_test_tp_elapsed_us = 5000u;
    StatusType status = ActivateTask(0u);

    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_ARRIVAL, status);
    TEST_ASSERT_TRUE(os_shutdown_requested);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Sufficient interval does NOT invoke ProtectionHook.
 */
void test_inter_arrival_ok_does_not_invoke_hook(void)
{
    Os_TimingProtConfigType tp_cfg = { 0u, 10000u };
    Os_TimingProtConfigure(0u, &tp_cfg);

    /* Enough time passed */
    os_test_tp_elapsed_us = 15000u;
    StatusType status = ActivateTask(0u);

    TEST_ASSERT_EQUAL_UINT8(0u, protection_hook_call_count);
    (void)status;
}

/* ==================================================================
 * 3D-MP: Memory fault → ProtectionHook (already wired, regression)
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Memory fault invokes ProtectionHook(E_OS_PROTECTION_MEMORY).
 */
void test_memory_fault_invokes_protection_hook(void)
{
    Os_MemProtFaultHandler(0xDEADBEEFu);

    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_MEMORY, observed_protection_error);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Budget expiry invokes ProtectionHook(E_OS_PROTECTION_TIME).
 */
void test_budget_expiry_invokes_protection_hook(void)
{
    Os_TimingProtConfigType tp_cfg = { 5000u, 0u };
    Os_TimingProtConfigure(0u, &tp_cfg);

    os_test_tp_elapsed_us = 6000u;
    Os_TimingProtBudgetExpired();

    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_TIME, observed_protection_error);
}

/* ==================================================================
 * 3D-CROSS: All three domains dispatch to the same hook
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.6.1
 * @requirement All three protection domains use the same ProtectionHook
 *              instance. Multiple faults accumulate call count.
 */
void test_all_three_domains_use_same_hook(void)
{
    Os_TimingProtConfigType tp_cfg = { 5000u, 0u };
    Os_TimingProtConfigure(0u, &tp_cfg);

    /* Domain 1: timing */
    Os_TimingProtBudgetExpired();
    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_TIME, observed_protection_error);

    /* Restore running task (budget expiry killed it) */
    Os_TestSetCurrentTaskRunning(0u);

    /* Domain 2: memory */
    Os_MemProtFaultHandler(0x1000u);
    TEST_ASSERT_EQUAL_UINT8(2u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_MEMORY, observed_protection_error);

    /* Restore running task */
    Os_TestSetCurrentTaskRunning(0u);

    /* Domain 3: service */
    os_call_level = OS_CALLLEVEL_ISR2;
    Schedule();
    TEST_ASSERT_EQUAL_UINT8(3u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_CALLEVEL, observed_protection_error);

    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Service protection → ProtectionHook */
    RUN_TEST(test_call_level_violation_invokes_protection_hook);
    RUN_TEST(test_terminate_task_from_error_hook_invokes_protection_hook);
    RUN_TEST(test_wait_event_from_isr2_invokes_protection_hook);
    RUN_TEST(test_call_level_violation_no_hook_shuts_down);
    RUN_TEST(test_call_level_violation_hook_returns_shutdown);
    RUN_TEST(test_call_level_violation_hook_terminates_task);
    RUN_TEST(test_valid_call_level_does_not_invoke_hook);

    /* Inter-arrival → ProtectionHook */
    RUN_TEST(test_inter_arrival_violation_invokes_protection_hook);
    RUN_TEST(test_inter_arrival_violation_no_hook_shuts_down);
    RUN_TEST(test_inter_arrival_ok_does_not_invoke_hook);

    /* Memory + timing regression (already wired, verify in integration) */
    RUN_TEST(test_memory_fault_invokes_protection_hook);
    RUN_TEST(test_budget_expiry_invokes_protection_hook);

    /* Cross-domain: all 3 use same hook */
    RUN_TEST(test_all_three_domains_use_same_hook);

    return UNITY_END();
}
