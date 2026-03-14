/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_saved_context.c
 * @brief   IRQ packed saved-context view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_save_exposes_packed_saved_context_view(void)
{
    const Os_Port_Tms570_InterruptContextType* saved;
    const Os_Port_Tms570_IrqScratchSnapshotType scratch = {11u, 12u, 13u, 14u, 110u, 112u};

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x12345678u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x71000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&scratch));

    Os_Port_Tms570_IrqContextSave();

    saved = Os_Port_Tms570_PeekSavedIrqContext();
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT_TRUE(saved->Valid);
    TEST_ASSERT_EQUAL_UINT32(0x12345678u, (uint32)saved->ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x71000013u, saved->Cpsr);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, saved->FrameBytes);
    TEST_ASSERT_EQUAL_UINT32(scratch.R0, saved->Scratch.R0);
    TEST_ASSERT_EQUAL_UINT32(scratch.R10, saved->Scratch.R10);
    TEST_ASSERT_EQUAL_UINT32(scratch.R12, saved->Scratch.R12);
}

void test_Os_Port_Tms570_RegisterIrqSavedContextTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_save_exposes_packed_saved_context_view);
}
