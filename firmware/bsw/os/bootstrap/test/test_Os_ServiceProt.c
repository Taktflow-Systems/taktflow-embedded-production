/**
 * @file    test_Os_ServiceProt.c
 * @brief   Unit tests for OSEK service protection (call-level validation)
 * @date    2026-03-14
 *
 * @details Tests the OSEK/VDX OS call-level matrix (Table 13.1):
 *          Each API is only allowed at specific call levels (TASK, ISR2,
 *          ErrorHook, PreTaskHook, PostTaskHook, StartupHook, ShutdownHook).
 *          Calling an API at the wrong level returns E_OS_CALLEVEL.
 *
 * @standard OSEK/VDX OS 2.2.3 Table 13.1, AUTOSAR OS §7.9.1
 * @copyright Taktflow Systems 2026
 */

#include "unity.h"
#include "Os.h"

/* Service protection internals for test observability */
extern uint8 os_call_level;

/* Call-level constants (must match Os_ServiceProt.c) */
#define OS_CALLLEVEL_TASK               0u
#define OS_CALLLEVEL_ISR2               1u
#define OS_CALLLEVEL_ERROR_HOOK         2u
#define OS_CALLLEVEL_PRE_TASK_HOOK      3u
#define OS_CALLLEVEL_POST_TASK_HOOK     4u
#define OS_CALLLEVEL_STARTUP_HOOK       5u
#define OS_CALLLEVEL_SHUTDOWN_HOOK      6u

/* --- Helpers to set up minimal task config --- */

static void dummy_task(void) {}

static const Os_TaskConfigType one_task_cfg[] = {
    { "T0", dummy_task, 1u, 1u, 1u, FALSE, FULL }
};

void setUp(void)
{
    Os_TestReset();
    Os_TestConfigureTasks(one_task_cfg, 1u);
    StartOS(OSDEFAULTAPPMODE);

    /* After StartOS, we should be at TASK level.
     * Force a running task so API calls have a valid context. */
    Os_TestSetCurrentTaskRunning(0u);
}

void tearDown(void)
{
}

/* ==================================================================
 * Call-level tracking basics
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement After StartOS, the default call level shall be TASK.
 */
void test_default_call_level_is_task(void)
{
    TEST_ASSERT_EQUAL_UINT8(OS_CALLLEVEL_TASK, os_call_level);
}

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement Os_TestReset shall reset call level to TASK.
 */
void test_reset_restores_task_level(void)
{
    os_call_level = OS_CALLLEVEL_ISR2;
    Os_TestReset();
    TEST_ASSERT_EQUAL_UINT8(OS_CALLLEVEL_TASK, os_call_level);
}

/* ==================================================================
 * TerminateTask — only allowed at TASK level
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement TerminateTask called from ISR Cat2 shall return E_OS_CALLEVEL.
 */
void test_TerminateTask_from_ISR2_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_ISR2;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, TerminateTask());
    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement TerminateTask called from ErrorHook shall return E_OS_CALLEVEL.
 */
void test_TerminateTask_from_ErrorHook_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_ERROR_HOOK;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, TerminateTask());
    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * Schedule — only allowed at TASK level
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement Schedule called from ISR Cat2 shall return E_OS_CALLEVEL.
 */
void test_Schedule_from_ISR2_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_ISR2;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, Schedule());
    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * WaitEvent — only allowed at TASK level
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement WaitEvent called from ISR Cat2 shall return E_OS_CALLEVEL.
 */
void test_WaitEvent_from_ISR2_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_ISR2;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, WaitEvent(0x01u));
    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement WaitEvent called from PreTaskHook shall return E_OS_CALLEVEL.
 */
void test_WaitEvent_from_PreTaskHook_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_PRE_TASK_HOOK;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, WaitEvent(0x01u));
    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * ClearEvent — only allowed at TASK level
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement ClearEvent called from ISR Cat2 shall return E_OS_CALLEVEL.
 */
void test_ClearEvent_from_ISR2_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_ISR2;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, ClearEvent(0x01u));
    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * ActivateTask — allowed at TASK, ISR2, ErrorHook
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement ActivateTask called from PreTaskHook shall return E_OS_CALLEVEL.
 */
void test_ActivateTask_from_PreTaskHook_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_PRE_TASK_HOOK;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, ActivateTask(0u));
    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement ActivateTask called from StartupHook shall return E_OS_CALLEVEL.
 */
void test_ActivateTask_from_StartupHook_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_STARTUP_HOOK;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, ActivateTask(0u));
    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement ActivateTask from TASK level shall succeed (not E_OS_CALLEVEL).
 */
void test_ActivateTask_from_TASK_succeeds(void)
{
    StatusType status = ActivateTask(0u);
    TEST_ASSERT_TRUE(status != E_OS_CALLEVEL);
}

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement ActivateTask from ISR2 shall succeed (not E_OS_CALLEVEL).
 */
void test_ActivateTask_from_ISR2_succeeds(void)
{
    os_call_level = OS_CALLLEVEL_ISR2;
    StatusType status = ActivateTask(0u);
    TEST_ASSERT_TRUE(status != E_OS_CALLEVEL);
    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * SetEvent — allowed at TASK, ISR2, ErrorHook
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement SetEvent from StartupHook shall return E_OS_CALLEVEL.
 */
void test_SetEvent_from_StartupHook_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_STARTUP_HOOK;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, SetEvent(0u, 0x01u));
    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * GetResource / ReleaseResource — allowed at TASK, ISR2
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement GetResource from ErrorHook shall return E_OS_CALLEVEL.
 */
void test_GetResource_from_ErrorHook_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_ERROR_HOOK;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, GetResource(0u));
    os_call_level = OS_CALLLEVEL_TASK;
}

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement ReleaseResource from ErrorHook shall return E_OS_CALLEVEL.
 */
void test_ReleaseResource_from_ErrorHook_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_ERROR_HOOK;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, ReleaseResource(0u));
    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * ChainTask — only allowed at TASK level
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement ChainTask from ISR Cat2 shall return E_OS_CALLEVEL.
 */
void test_ChainTask_from_ISR2_returns_CALLEVEL(void)
{
    os_call_level = OS_CALLLEVEL_ISR2;
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, ChainTask(0u));
    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * GetActiveApplicationMode — allowed at ALL levels
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 Table 13.1
 * @requirement GetActiveApplicationMode shall work from ShutdownHook.
 */
void test_GetActiveApplicationMode_from_ShutdownHook_succeeds(void)
{
    os_call_level = OS_CALLLEVEL_SHUTDOWN_HOOK;
    AppModeType mode = GetActiveApplicationMode();
    TEST_ASSERT_EQUAL(OSDEFAULTAPPMODE, mode);
    os_call_level = OS_CALLLEVEL_TASK;
}

/* ==================================================================
 * ISR2 entry/exit transitions call level
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement Os_BootstrapEnterIsr2 shall set call level to ISR2.
 */
void test_EnterIsr2_sets_ISR2_call_level(void)
{
    TEST_ASSERT_EQUAL_UINT8(OS_CALLLEVEL_TASK, os_call_level);
    Os_BootstrapEnterIsr2();
    TEST_ASSERT_EQUAL_UINT8(OS_CALLLEVEL_ISR2, os_call_level);
    Os_BootstrapExitIsr2();
    TEST_ASSERT_EQUAL_UINT8(OS_CALLLEVEL_TASK, os_call_level);
}

/**
 * @spec AUTOSAR OS §7.9.1
 * @requirement Nested ISR2 entry shall keep ISR2 level, exit to outermost
 *              restores TASK level.
 */
void test_nested_ISR2_keeps_ISR2_level(void)
{
    Os_BootstrapEnterIsr2();
    Os_BootstrapEnterIsr2();
    TEST_ASSERT_EQUAL_UINT8(OS_CALLLEVEL_ISR2, os_call_level);

    Os_BootstrapExitIsr2();
    TEST_ASSERT_EQUAL_UINT8(OS_CALLLEVEL_ISR2, os_call_level);

    Os_BootstrapExitIsr2();
    TEST_ASSERT_EQUAL_UINT8(OS_CALLLEVEL_TASK, os_call_level);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Call-level tracking */
    RUN_TEST(test_default_call_level_is_task);
    RUN_TEST(test_reset_restores_task_level);

    /* TASK-only APIs rejected at wrong level */
    RUN_TEST(test_TerminateTask_from_ISR2_returns_CALLEVEL);
    RUN_TEST(test_TerminateTask_from_ErrorHook_returns_CALLEVEL);
    RUN_TEST(test_Schedule_from_ISR2_returns_CALLEVEL);
    RUN_TEST(test_WaitEvent_from_ISR2_returns_CALLEVEL);
    RUN_TEST(test_WaitEvent_from_PreTaskHook_returns_CALLEVEL);
    RUN_TEST(test_ClearEvent_from_ISR2_returns_CALLEVEL);
    RUN_TEST(test_ChainTask_from_ISR2_returns_CALLEVEL);

    /* TASK+ISR2+ErrorHook APIs rejected at wrong level */
    RUN_TEST(test_ActivateTask_from_PreTaskHook_returns_CALLEVEL);
    RUN_TEST(test_ActivateTask_from_StartupHook_returns_CALLEVEL);

    /* Positive: APIs allowed at their levels */
    RUN_TEST(test_ActivateTask_from_TASK_succeeds);
    RUN_TEST(test_ActivateTask_from_ISR2_succeeds);
    RUN_TEST(test_SetEvent_from_StartupHook_returns_CALLEVEL);
    RUN_TEST(test_GetResource_from_ErrorHook_returns_CALLEVEL);
    RUN_TEST(test_ReleaseResource_from_ErrorHook_returns_CALLEVEL);
    RUN_TEST(test_GetActiveApplicationMode_from_ShutdownHook_succeeds);

    /* ISR2 call-level transitions */
    RUN_TEST(test_EnterIsr2_sets_ISR2_call_level);
    RUN_TEST(test_nested_ISR2_keeps_ISR2_level);

    return UNITY_END();
}
