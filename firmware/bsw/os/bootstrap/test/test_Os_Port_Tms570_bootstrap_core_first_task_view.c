/**
 * @file    test_Os_Port_Tms570_bootstrap_core_first_task_view.c
 * @brief   First-task launch view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_begin_first_task_start_exposes_pending_first_task_frame_view(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskFrameViewType* pending;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_BeginFirstTaskStart());
    pending = Os_Port_Tms570_PeekPendingFirstTaskFrameView();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekPendingRestoreTaskFrameView());
    TEST_ASSERT_EQUAL_PTR((void*)&pending->Frame, (void*)Os_Port_Tms570_PeekRestoreTaskFrame());
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeFrame.Sp, (void*)pending->Frame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeFrame.ReturnAddress, (void*)pending->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES, pending->FrameBytes);

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->FirstTaskStartInProgress);
    TEST_ASSERT_FALSE(state->FirstTaskStarted);

    Os_Port_Tms570_FinishFirstTaskStart();
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingFirstTaskFrameView());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->FirstTaskStartInProgress);
    TEST_ASSERT_TRUE(state->FirstTaskStarted);
}

void test_Os_Port_Tms570_begin_first_task_start_drives_generic_restore_scalars(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_TaskContextType* first_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_BeginFirstTaskStart());
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeFrame.Sp,
                          (void*)Os_Port_Tms570_PeekRestoreTaskSp());
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeFrame.ReturnAddress,
                          (void*)Os_Port_Tms570_PeekRestoreTaskReturnAddress());
    TEST_ASSERT_EQUAL_UINT32(first_ctx->RuntimeFrame.Cpsr, Os_Port_Tms570_PeekRestoreTaskCpsr());
    TEST_ASSERT_EQUAL_UINT32(first_ctx->RuntimeFrame.StackType,
                             Os_Port_Tms570_PeekRestoreTaskStackType());
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES,
                             Os_Port_Tms570_PeekRestoreTaskFrameBytes());
    TEST_ASSERT_EQUAL_PTR((void*)Os_Port_Tms570_PeekPendingFirstTaskFrameView(),
                          (void*)Os_Port_Tms570_PeekPendingRestoreTaskFrameView());

    Os_Port_Tms570_FinishFirstTaskStart();
}

void test_Os_Port_Tms570_RegisterCoreFirstTaskViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_begin_first_task_start_exposes_pending_first_task_frame_view);
    RUN_TEST(test_Os_Port_Tms570_begin_first_task_start_drives_generic_restore_scalars);
}
