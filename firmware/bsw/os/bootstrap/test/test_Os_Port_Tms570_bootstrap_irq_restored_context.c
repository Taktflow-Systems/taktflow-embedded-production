/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restored_context.c
 * @brief   IRQ packed restored-context view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_restore_exposes_packed_restored_context_view(void)
{
    const Os_Port_Tms570_InterruptContextType* restored;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {31u, 32u, 33u, 34u, 310u, 312u};
    const Os_Port_Tms570_IrqScratchSnapshotType live = {41u, 42u, 43u, 44u, 410u, 412u};

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x3456789Au));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x73000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved));
    Os_Port_Tms570_IrqContextSave();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x74000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&live));
    Os_Port_Tms570_IrqContextRestore();

    restored = Os_Port_Tms570_PeekRestoredIrqContext();
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_TRUE(restored->Valid);
    TEST_ASSERT_EQUAL_UINT32(0x3456789Au, (uint32)restored->ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x73000013u, restored->Cpsr);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, restored->FrameBytes);
    TEST_ASSERT_EQUAL_UINT32(saved.R10, restored->Scratch.R10);
    TEST_ASSERT_EQUAL_UINT32(saved.R12, restored->Scratch.R12);
}

void test_Os_Port_Tms570_RegisterIrqRestoredContextTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_restore_exposes_packed_restored_context_view);
}
