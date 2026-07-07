/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restore_frame.c
 * @brief   IRQ restore-frame tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_restore_frame_helpers_follow_current_runtime_frame(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x12345670u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp((uintptr_t)(&first_stack[120])));

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)&first_ctx->RuntimeFrame, (void*)Os_Port_Tms570_PeekRestoreTaskFrame());
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeFrame.Sp, (void*)Os_Port_Tms570_PeekRestoreTaskSp());
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)Os_Port_Tms570_PeekRestoreTaskReturnAddress());
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INITIAL_CPSR, Os_Port_Tms570_PeekRestoreTaskCpsr());
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INITIAL_STACK_TYPE,
                             Os_Port_Tms570_PeekRestoreTaskStackType());
}

void test_Os_Port_Tms570_restore_frame_helpers_prefer_selected_task_runtime_frame(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)&second_ctx->RuntimeFrame, (void*)Os_Port_Tms570_PeekRestoreTaskFrame());
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->RuntimeFrame.Sp, (void*)Os_Port_Tms570_PeekRestoreTaskSp());
    TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)dummy_task_entry_alt,
                          (void*)Os_Port_Tms570_PeekRestoreTaskReturnAddress());
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INITIAL_CPSR, Os_Port_Tms570_PeekRestoreTaskCpsr());
}

void test_Os_Port_Tms570_RegisterIrqRestoreFrameTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_restore_frame_helpers_follow_current_runtime_frame);
    RUN_TEST(test_Os_Port_Tms570_restore_frame_helpers_prefer_selected_task_runtime_frame);
}
