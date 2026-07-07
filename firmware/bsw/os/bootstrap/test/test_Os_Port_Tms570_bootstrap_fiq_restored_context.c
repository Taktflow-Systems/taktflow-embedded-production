/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_restored_context.c
 * @brief   FIQ packed restored-context view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_restore_exposes_packed_restored_context_view(void)
{
    const Os_Port_Tms570_InterruptContextType* restored;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {51u, 52u, 53u, 54u, 510u, 512u};
    const Os_Port_Tms570_IrqScratchSnapshotType live = {61u, 62u, 63u, 64u, 610u, 612u};

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x456789ABu));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x75000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&saved));
    Os_Port_Tms570_FiqContextSave();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x76000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&live));
    Os_Port_Tms570_FiqContextRestore();

    restored = Os_Port_Tms570_PeekRestoredFiqContext();
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_TRUE(restored->Valid);
    TEST_ASSERT_EQUAL_UINT32(0x456789ABu, (uint32)restored->ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x75000013u, restored->Cpsr);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES, restored->FrameBytes);
    TEST_ASSERT_EQUAL_UINT32(saved.R0, restored->Scratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, restored->Scratch.R10);
    TEST_ASSERT_EQUAL_UINT32(0u, restored->Scratch.R12);
}

void test_Os_Port_Tms570_RegisterFiqRestoredContextTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_restore_exposes_packed_restored_context_view);
}
