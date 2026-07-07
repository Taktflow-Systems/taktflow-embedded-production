/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_solicited_save_snapshot.c
 * @brief   Solicited save snapshot tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_solicited_save_window_freezes_pending_frame_before_finish(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* context;
    const Os_Port_Tms570_TaskFrameViewType* pending;
    const Os_Port_Tms570_PreservedSnapshotType initial_preserved = {11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u};
    const Os_Port_Tms570_PreservedSnapshotType dirty_preserved = {31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u};
    uintptr_t running_sp = (uintptr_t)(&first_stack[112]);
    uintptr_t expected_saved_sp = running_sp - (uintptr_t)OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(running_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(23u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x60000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0xABCDEF01u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&initial_preserved));

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_BeginSolicitedSystemReturn());
    pending = Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)pending->Frame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)0xABCDEF01u, (void*)pending->Frame.LinkRegister);
    TEST_ASSERT_EQUAL_UINT32(23u, pending->Frame.TimeSlice);
    TEST_ASSERT_EQUAL_UINT32(initial_preserved.R4, pending->Frame.Preserved.R4);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[96])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(77u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x70000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0x01020304u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&dirty_preserved));

    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView()->Frame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)0xABCDEF01u, (void*)Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView()->Frame.LinkRegister);
    TEST_ASSERT_EQUAL_UINT32(23u, Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView()->Frame.TimeSlice);
    TEST_ASSERT_EQUAL_UINT32(initial_preserved.R4, Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView()->Frame.Preserved.R4);

    Os_Port_Tms570_FinishSolicitedSystemReturn();
    state = Os_Port_Tms570_GetBootstrapState();
    context = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)context->RuntimeFrame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)0xABCDEF01u, (void*)context->RuntimeFrame.LinkRegister);
    TEST_ASSERT_EQUAL_UINT32(23u, context->RuntimeFrame.TimeSlice);
    TEST_ASSERT_EQUAL_UINT32(initial_preserved.R4, context->RuntimeFrame.Preserved.R4);
    TEST_ASSERT_EQUAL_UINT32(23u, state->LastSavedTimeSlice);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)state->LastSavedTaskSp);
}

void test_Os_Port_Tms570_RegisterIrqSolicitedSaveSnapshotTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_solicited_save_window_freezes_pending_frame_before_finish);
}
