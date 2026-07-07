/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_runtime_lower.c
 * @brief   FIQ runtime lower-register tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_scheduler_return_captures_live_task_lower(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskLowerSnapshotType outgoing = {41u, 42u, 43u, 44u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareTaskContext(
            OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&outgoing));
    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextRestore();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(outgoing.R0, first_ctx->RuntimeFrame.TaskLower.R0);
    TEST_ASSERT_EQUAL_UINT32(outgoing.R3, first_ctx->RuntimeFrame.TaskLower.R3);
}

void test_Os_Port_Tms570_RegisterFiqRuntimeLowerTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_scheduler_return_captures_live_task_lower);
}
