/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_pending_solicited_save_task_frame_view.c
 * @brief   Pending solicited-save frame-view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_begin_solicited_return_exposes_pending_save_task_frame_view(void)
{
    uint8 first_stack[160];
    uintptr_t running_sp = (uintptr_t)(&first_stack[112]);
    uintptr_t expected_saved_sp;
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskFrameViewType* pending;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(running_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(23u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x60000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0xABCDEF01u));
    expected_saved_sp = running_sp - (uintptr_t)OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_BeginSolicitedSystemReturn());
    pending = Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekPendingSaveTaskFrameView());
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekSavedTaskFrameView());
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)pending->Frame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)0xABCDEF01u, (void*)pending->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x60000013u, pending->Frame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES, pending->FrameBytes);

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->SolicitedSaveInProgress);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);

    Os_Port_Tms570_FinishSolicitedSystemReturn();
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->SolicitedSaveInProgress);
    TEST_ASSERT_EQUAL_UINT32(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_TRUE(state->DispatchRequested);
}

void test_Os_Port_Tms570_RegisterIrqPendingSolicitedSaveTaskFrameViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_begin_solicited_return_exposes_pending_save_task_frame_view);
}
