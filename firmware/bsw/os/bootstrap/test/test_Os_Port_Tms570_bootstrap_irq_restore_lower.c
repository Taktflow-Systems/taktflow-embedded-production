/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restore_lower.c
 * @brief   IRQ restore lower-register helper tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_restore_lower_helper_follows_selected_runtime_frame(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_TaskLowerSnapshotType selected = {31u, 32u, 33u, 34u};
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareTaskContext(
            OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetTaskSavedLower(
                          OS_PORT_TMS570_SECOND_TASK_ID, &selected));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)&second_ctx->RuntimeFrame.TaskLower,
                          (void*)Os_Port_Tms570_PeekRestoreTaskLower());
}

void test_Os_Port_Tms570_RegisterIrqRestoreLowerTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_restore_lower_helper_follows_selected_runtime_frame);
}
