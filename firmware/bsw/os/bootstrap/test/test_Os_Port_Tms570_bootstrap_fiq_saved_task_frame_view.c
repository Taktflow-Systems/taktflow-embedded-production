/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_saved_task_frame_view.c
 * @brief   FIQ/task save frame-view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_scheduler_return_exposes_packed_saved_task_frame_view(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t running_sp = (uintptr_t)(&first_stack[124]);
    uintptr_t expected_saved_sp = running_sp - (uintptr_t)OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES;
    const Os_Port_Tms570_TaskFrameViewType* saved;
    const Os_Port_Tms570_TaskLowerSnapshotType lower = {31u, 32u, 33u, 34u};
    const Os_Port_Tms570_PreservedSnapshotType preserved = {41u, 42u, 43u, 44u, 45u, 46u, 47u, 48u, 49u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(running_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(9u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&lower));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&preserved));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0xBEEFF001u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x66666660u));
    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextRestore();

    saved = Os_Port_Tms570_PeekSavedTaskFrameView();
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT_TRUE(saved->Frame.Valid);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES, saved->FrameBytes);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)saved->Frame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)0x66666660u, (void*)saved->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xBEEFF001u, (void*)saved->Frame.LinkRegister);
    TEST_ASSERT_EQUAL_UINT32(9u, saved->Frame.TimeSlice);
    TEST_ASSERT_EQUAL_UINT32(lower.R0, saved->Frame.TaskLower.R0);
    TEST_ASSERT_EQUAL_UINT32(preserved.R11, saved->Frame.Preserved.R11);
    TEST_ASSERT_EQUAL_PTR((void*)saved->Frame.Sp, (void*)Os_Port_Tms570_PeekSavedTaskSp());
    TEST_ASSERT_EQUAL_PTR((void*)saved->Frame.ReturnAddress, (void*)Os_Port_Tms570_PeekSavedTaskReturnAddress());
    TEST_ASSERT_EQUAL_UINT32(saved->FrameBytes, Os_Port_Tms570_PeekSavedTaskFrameBytes());
    TEST_ASSERT_EQUAL_UINT32(lower.R0, Os_Port_Tms570_PeekSavedTaskLower()->R0);
    TEST_ASSERT_EQUAL_UINT32(preserved.R11, Os_Port_Tms570_PeekSavedTaskPreserved()->R11);
}

void test_Os_Port_Tms570_RegisterFiqSavedTaskFrameViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_scheduler_return_exposes_packed_saved_task_frame_view);
}
