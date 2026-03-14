/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_pending_restore_task_frame_view.c
 * @brief   IRQ pending effective-restore frame-view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_resume_current_exposes_pending_effective_restore_task_frame_view(void)
{
    uint8 first_stack[160];
    uint8 action;
    const Os_Port_Tms570_TaskFrameViewType* pending;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {231u, 232u, 233u, 234u, 2310u, 2312u};
    const Os_Port_Tms570_IrqScratchSnapshotType corrupted = {241u, 242u, 243u, 244u, 2410u, 2412u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x51525354u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x77100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedReturnAddress(OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)0x61626364u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedCpsr(OS_PORT_TMS570_FIRST_TASK_ID, 0x77200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedIrqScratch(OS_PORT_TMS570_FIRST_TASK_ID, &corrupted));

    action = Os_Port_Tms570_BeginIrqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT, action);
    pending = Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)Os_Port_Tms570_PeekRestoredIrqContext(),
                          (void*)Os_Port_Tms570_PeekPendingRestoreInterruptContext());
    TEST_ASSERT_EQUAL_PTR((void*)&pending->Frame, (void*)Os_Port_Tms570_PeekRestoreTaskFrame());
    TEST_ASSERT_EQUAL_UINT32(0x51525354u, (uint32)pending->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x77100013u, pending->Frame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(saved.R10, pending->Frame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, pending->FrameBytes);
    TEST_ASSERT_EQUAL_PTR((void*)pending->Frame.ReturnAddress, (void*)Os_Port_Tms570_PeekRestoreTaskReturnAddress());
    TEST_ASSERT_EQUAL_HEX32(pending->Frame.Cpsr, Os_Port_Tms570_PeekRestoreTaskCpsr());
    TEST_ASSERT_EQUAL_UINT32(saved.R10, Os_Port_Tms570_PeekRestoreTaskIrqScratch()->R10);
    TEST_ASSERT_EQUAL_UINT32(pending->FrameBytes, Os_Port_Tms570_PeekRestoreTaskFrameBytes());
    TEST_ASSERT_EQUAL_PTR((void*)pending->Frame.Sp, (void*)Os_Port_Tms570_PeekPendingIrqRestoreTaskSp());
    TEST_ASSERT_EQUAL_PTR((void*)pending->Frame.ReturnAddress, (void*)Os_Port_Tms570_PeekPendingIrqRestoreTaskReturnAddress());
    TEST_ASSERT_EQUAL_HEX32(pending->Frame.Cpsr, Os_Port_Tms570_PeekPendingIrqRestoreTaskCpsr());
    TEST_ASSERT_EQUAL_UINT32(pending->FrameBytes, Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameBytes());
    TEST_ASSERT_EQUAL_UINT32(saved.R10, Os_Port_Tms570_PeekPendingIrqRestoreTaskIrqScratch()->R10);
    Os_Port_Tms570_FinishIrqContextRestore(action);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView());
}

void test_Os_Port_Tms570_irq_switch_task_exposes_pending_effective_restore_task_frame_view(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uint8 action;
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
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x71727374u));
    Os_Port_Tms570_IrqContextSave();

    action = Os_Port_Tms570_BeginIrqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK, action);
    pending = Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)Os_Port_Tms570_PeekRestoredIrqContext(),
                          (void*)Os_Port_Tms570_PeekPendingRestoreInterruptContext());
    TEST_ASSERT_EQUAL_PTR((void*)&pending->Frame, (void*)Os_Port_Tms570_PeekRestoreTaskFrame());
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->RuntimeFrame.Sp, (void*)pending->Frame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->RuntimeFrame.ReturnAddress, (void*)pending->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES, pending->FrameBytes);
    TEST_ASSERT_EQUAL_PTR((void*)pending->Frame.ReturnAddress, (void*)Os_Port_Tms570_PeekRestoreTaskReturnAddress());
    TEST_ASSERT_EQUAL_UINT32(pending->FrameBytes, Os_Port_Tms570_PeekRestoreTaskFrameBytes());
    TEST_ASSERT_EQUAL_UINT32(pending->FrameBytes, Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameBytes());
    Os_Port_Tms570_FinishIrqContextRestore(action);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView());
}

void test_Os_Port_Tms570_RegisterIrqPendingRestoreTaskFrameViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_resume_current_exposes_pending_effective_restore_task_frame_view);
    RUN_TEST(test_Os_Port_Tms570_irq_switch_task_exposes_pending_effective_restore_task_frame_view);
}
