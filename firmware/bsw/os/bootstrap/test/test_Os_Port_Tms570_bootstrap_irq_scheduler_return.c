/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_scheduler_return.c
 * @brief   IRQ scheduler-return window tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_switch_restore_begins_scheduler_return_window(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uint8 action;
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* second_ctx;
    const Os_Port_Tms570_TaskFrameViewType* pending;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextSave();

    action = Os_Port_Tms570_BeginIrqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK, action);
    Os_Port_Tms570_FinishIrqContextRestore(action);
    state = Os_Port_Tms570_GetBootstrapState();
    pending = Os_Port_Tms570_PeekPendingIrqSchedulerReturnTaskFrameView();

    TEST_ASSERT_TRUE(state->IrqSchedulerReturnInProgress);
    TEST_ASSERT_FALSE(state->IrqRestoreInProgress);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqSchedulerReturnCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingRestoreInterruptContext());
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekPendingRestoreTaskFrameView());
    TEST_ASSERT_EQUAL_PTR((void*)&pending->Frame, (void*)Os_Port_Tms570_PeekRestoreTaskFrame());
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->RuntimeFrame.Sp, (void*)pending->Frame.Sp);

    Os_Port_Tms570_FinishIrqSchedulerReturn();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->IrqSchedulerReturnInProgress);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
}

void test_Os_Port_Tms570_irq_idle_restore_begins_scheduler_return_without_task_frame(void)
{
    uint8 action;
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x45464748u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x73100013u));
    Os_Port_Tms570_IrqContextSave();

    action = Os_Port_Tms570_BeginIrqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_IDLE_SYSTEM, action);
    Os_Port_Tms570_FinishIrqContextRestore(action);
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->IrqSchedulerReturnInProgress);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqIdleSystemReturnCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingIrqSchedulerReturnTaskFrameView());
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingRestoreTaskFrameView());

    Os_Port_Tms570_FinishIrqSchedulerReturn();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->IrqSchedulerReturnInProgress);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
}

void test_Os_Port_Tms570_RegisterIrqSchedulerReturnTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_switch_restore_begins_scheduler_return_window);
    RUN_TEST(test_Os_Port_Tms570_irq_idle_restore_begins_scheduler_return_without_task_frame);
}
