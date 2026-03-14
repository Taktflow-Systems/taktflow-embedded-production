/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_pending_save_context.c
 * @brief   IRQ pending save-context tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_capture_current_exposes_pending_save_context(void)
{
    uint8 first_stack[160];
    uint8 action;
    uintptr_t live_sp = (uintptr_t)(&first_stack[120]);
    const Os_Port_Tms570_InterruptContextType* pending;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {31u, 32u, 33u, 34u, 310u, 312u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(live_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x31323334u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x71000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved));

    action = Os_Port_Tms570_BeginIrqContextSave(live_sp);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CAPTURE_CURRENT, action);
    pending = Os_Port_Tms570_PeekPendingIrqSaveContext();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekPendingSaveInterruptContext());
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekSavedIrqContext());
    TEST_ASSERT_EQUAL_PTR((void*)0x31323334u, (void*)pending->ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x71000013u, pending->Cpsr);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, pending->FrameBytes);
    TEST_ASSERT_EQUAL_UINT32(saved.R10, pending->Scratch.R10);

    Os_Port_Tms570_FinishIrqContextSave(action);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingIrqSaveContext());
}

void test_Os_Port_Tms570_RegisterIrqPendingSaveContextTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_capture_current_exposes_pending_save_context);
}
