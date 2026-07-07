/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_pending_save_context.c
 * @brief   FIQ pending save-context tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_first_entry_exposes_pending_save_context(void)
{
    uint8 action;
    const Os_Port_Tms570_InterruptContextType* pending;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {51u, 52u, 53u, 54u, 510u, 512u};

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x51525354u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x73000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&saved));

    action = Os_Port_Tms570_BeginFiqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY, action);
    pending = Os_Port_Tms570_PeekPendingFiqSaveContext();
    TEST_ASSERT_NOT_NULL(pending);
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekPendingSaveInterruptContext());
    TEST_ASSERT_EQUAL_PTR((void*)pending, (void*)Os_Port_Tms570_PeekSavedFiqContext());
    TEST_ASSERT_EQUAL_PTR((void*)0x51525354u, (void*)pending->ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x73000013u, pending->Cpsr);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES, pending->FrameBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, pending->Scratch.R10);

    Os_Port_Tms570_FinishFiqContextSave(action);
    TEST_ASSERT_NULL(Os_Port_Tms570_PeekPendingFiqSaveContext());
}

void test_Os_Port_Tms570_RegisterFiqPendingSaveContextTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_first_entry_exposes_pending_save_context);
}
