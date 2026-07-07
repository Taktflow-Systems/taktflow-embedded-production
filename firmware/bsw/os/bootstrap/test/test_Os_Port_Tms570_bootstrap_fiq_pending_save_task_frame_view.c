/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_pending_save_task_frame_view.c
 * @brief   FIQ pending save task-frame view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_first_entry_exposes_pending_save_task_frame_view(void)
{
    uint8 action;
    uintptr_t live_sp = (uintptr_t)0x20001230u;
    uintptr_t expected_saved_sp = live_sp - (uintptr_t)OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES;
    const Os_Port_Tms570_TaskFrameViewType* pending;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {61u, 62u, 63u, 64u, 610u, 612u};

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(live_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x61626364u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x74000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&saved));

    action = Os_Port_Tms570_BeginFiqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY, action);
    pending = Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekPendingSaveTaskFrameView());
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekSavedTaskFrameView());
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)pending->Frame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)0x61626364u, (void*)pending->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x74000013u, pending->Frame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(0u, pending->Frame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES, pending->FrameBytes);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)Os_Port_Tms570_PeekPendingFiqSaveTaskSp());
    TEST_ASSERT_EQUAL_PTR((void*)0x61626364u,
                          (void*)Os_Port_Tms570_PeekPendingFiqSaveTaskReturnAddress());
    TEST_ASSERT_EQUAL_HEX32(0x74000013u, Os_Port_Tms570_PeekPendingFiqSaveTaskCpsr());
    TEST_ASSERT_EQUAL_UINT32(0u, Os_Port_Tms570_PeekPendingFiqSaveTaskIrqScratch()->R10);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES,
                             Os_Port_Tms570_PeekPendingFiqSaveTaskFrameBytes());
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)Os_Port_Tms570_PeekSavedTaskSp());
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES, Os_Port_Tms570_PeekSavedTaskFrameBytes());

    Os_Port_Tms570_FinishFiqContextSave(action);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView());
}

void test_Os_Port_Tms570_RegisterFiqPendingSaveTaskFrameViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_first_entry_exposes_pending_save_task_frame_view);
}
