/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_resume_context_metadata.c
 * @brief   IRQ resume metadata sync tests for packed restored context
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_resume_uses_packed_restored_context_metadata_not_saved_frame_metadata(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {191u, 192u, 193u, 194u, 1910u, 1912u};
    const Os_Port_Tms570_IrqScratchSnapshotType corrupted = {201u, 202u, 203u, 204u, 2010u, 2012u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x11121314u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x7D100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved));
    Os_Port_Tms570_IrqContextSave();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedReturnAddress(OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)0x21222324u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedCpsr(OS_PORT_TMS570_FIRST_TASK_ID, 0x7D200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedIrqScratch(OS_PORT_TMS570_FIRST_TASK_ID, &corrupted));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(0x11121314u, (uint32)state->LastRestoredTaskReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x7D100013u, state->LastRestoredTaskCpsr);
    TEST_ASSERT_EQUAL_UINT32(saved.R10, state->LastRestoredTaskIrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(saved.R12, state->LastRestoredTaskIrqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, state->LastRestoredTaskFrameBytes);
}

void test_Os_Port_Tms570_RegisterIrqResumeContextMetadataTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_resume_uses_packed_restored_context_metadata_not_saved_frame_metadata);
}
