/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_saved_context.c
 * @brief   FIQ packed saved-context view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_save_exposes_packed_saved_context_view(void)
{
    const Os_Port_Tms570_InterruptContextType* saved;
    const Os_Port_Tms570_IrqScratchSnapshotType scratch = {21u, 22u, 23u, 24u, 210u, 212u};

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x23456789u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x72000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&scratch));

    Os_Port_Tms570_FiqContextSave();

    saved = Os_Port_Tms570_PeekSavedFiqContext();
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT_TRUE(saved->Valid);
    TEST_ASSERT_EQUAL_UINT32(0x23456789u, (uint32)saved->ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x72000013u, saved->Cpsr);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES, saved->FrameBytes);
    TEST_ASSERT_EQUAL_UINT32(scratch.R0, saved->Scratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, saved->Scratch.R10);
    TEST_ASSERT_EQUAL_UINT32(0u, saved->Scratch.R12);
}

void test_Os_Port_Tms570_RegisterFiqSavedContextTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_save_exposes_packed_saved_context_view);
}
