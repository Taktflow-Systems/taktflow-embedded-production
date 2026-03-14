/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_pending_restore_task_frame_view.c
 * @brief   FIQ pending effective-restore frame-view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_resume_previous_mode_exposes_pending_effective_restore_task_frame_view(void)
{
    uint8 first_stack[160];
    uint8 action;
    const Os_Port_Tms570_TaskFrameViewType* pending;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {251u, 252u, 253u, 254u, 2510u, 2512u};
    const Os_Port_Tms570_IrqScratchSnapshotType corrupted = {261u, 262u, 263u, 264u, 2610u, 2612u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x81828384u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x78100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&saved));
    Os_Port_Tms570_FiqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedReturnAddress(OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)0x91929394u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedCpsr(OS_PORT_TMS570_FIRST_TASK_ID, 0x78200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedIrqScratch(OS_PORT_TMS570_FIRST_TASK_ID, &corrupted));

    action = Os_Port_Tms570_BeginFiqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE, action);
    pending = Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)Os_Port_Tms570_PeekRestoredFiqContext(),
                          (void*)Os_Port_Tms570_PeekPendingRestoreInterruptContext());
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekPendingRestoreTaskFrameView());
    TEST_ASSERT_EQUAL_PTR((void*)&pending->Frame, (void*)Os_Port_Tms570_PeekRestoreTaskFrame());
    TEST_ASSERT_EQUAL_UINT32(0x81828384u, (uint32)pending->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x78100013u, pending->Frame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(saved.R0, pending->Frame.IrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, pending->Frame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES, pending->FrameBytes);
    TEST_ASSERT_EQUAL_PTR((void*)pending->Frame.ReturnAddress, (void*)Os_Port_Tms570_PeekRestoreTaskReturnAddress());
    TEST_ASSERT_EQUAL_HEX32(pending->Frame.Cpsr, Os_Port_Tms570_PeekRestoreTaskCpsr());
    TEST_ASSERT_EQUAL_UINT32(saved.R0, Os_Port_Tms570_PeekRestoreTaskIrqScratch()->R0);
    TEST_ASSERT_EQUAL_UINT32(pending->FrameBytes, Os_Port_Tms570_PeekRestoreTaskFrameBytes());
    TEST_ASSERT_EQUAL_PTR((void*)pending->Frame.Sp, (void*)Os_Port_Tms570_PeekPendingFiqRestoreTaskSp());
    TEST_ASSERT_EQUAL_PTR((void*)pending->Frame.ReturnAddress, (void*)Os_Port_Tms570_PeekPendingFiqRestoreTaskReturnAddress());
    TEST_ASSERT_EQUAL_HEX32(pending->Frame.Cpsr, Os_Port_Tms570_PeekPendingFiqRestoreTaskCpsr());
    TEST_ASSERT_EQUAL_UINT32(pending->FrameBytes, Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameBytes());
    TEST_ASSERT_EQUAL_UINT32(saved.R0, Os_Port_Tms570_PeekPendingFiqRestoreTaskIrqScratch()->R0);
    Os_Port_Tms570_FinishFiqContextRestore(action);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView());
}

void test_Os_Port_Tms570_fiq_scheduler_return_has_no_pending_task_restore_frame_view(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uint8 action;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xA1A2A3A4u));
    Os_Port_Tms570_FiqContextSave();

    action = Os_Port_Tms570_BeginFiqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER, action);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView());
    TEST_ASSERT_EQUAL_UINT32(0u, Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameBytes());
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingFiqRestoreTaskIrqScratch());
    Os_Port_Tms570_FinishFiqContextRestore(action);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView());
}

void test_Os_Port_Tms570_RegisterFiqPendingRestoreTaskFrameViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_resume_previous_mode_exposes_pending_effective_restore_task_frame_view);
    RUN_TEST(test_Os_Port_Tms570_fiq_scheduler_return_has_no_pending_task_restore_frame_view);
}
