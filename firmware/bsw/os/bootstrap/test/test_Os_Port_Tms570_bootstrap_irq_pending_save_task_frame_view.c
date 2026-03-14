/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_pending_save_task_frame_view.c
 * @brief   IRQ pending save task-frame view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_capture_current_exposes_pending_save_task_frame_view(void)
{
    uint8 first_stack[160];
    uint8 action;
    uintptr_t live_sp = (uintptr_t)(&first_stack[120]);
    uintptr_t expected_saved_sp = live_sp - (uintptr_t)OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES;
    const Os_Port_Tms570_TaskFrameViewType* pending;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {41u, 42u, 43u, 44u, 410u, 412u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(live_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x41424344u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x72000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved));

    action = Os_Port_Tms570_BeginIrqContextSave(live_sp);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CAPTURE_CURRENT, action);
    pending = Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekPendingSaveTaskFrameView());
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekSavedTaskFrameView());
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)pending->Frame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)0x41424344u, (void*)pending->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x72000013u, pending->Frame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(saved.R10, pending->Frame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, pending->FrameBytes);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)Os_Port_Tms570_PeekPendingIrqSaveTaskSp());
    TEST_ASSERT_EQUAL_PTR((void*)0x41424344u,
                          (void*)Os_Port_Tms570_PeekPendingIrqSaveTaskReturnAddress());
    TEST_ASSERT_EQUAL_HEX32(0x72000013u, Os_Port_Tms570_PeekPendingIrqSaveTaskCpsr());
    TEST_ASSERT_EQUAL_UINT32(saved.R10, Os_Port_Tms570_PeekPendingIrqSaveTaskIrqScratch()->R10);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES,
                             Os_Port_Tms570_PeekPendingIrqSaveTaskFrameBytes());
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)Os_Port_Tms570_PeekSavedTaskSp());
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, Os_Port_Tms570_PeekSavedTaskFrameBytes());

    Os_Port_Tms570_FinishIrqContextSave(action);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView());
}

void test_Os_Port_Tms570_RegisterIrqPendingSaveTaskFrameViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_capture_current_exposes_pending_save_task_frame_view);
}
