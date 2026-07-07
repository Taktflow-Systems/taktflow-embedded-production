/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_resume_context_sync.c
 * @brief   IRQ resume sync tests for packed restored context
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_resume_prefers_packed_restored_context_over_saved_frame_fields(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {151u, 152u, 153u, 154u, 1510u, 1512u};
    const Os_Port_Tms570_IrqScratchSnapshotType corrupted = {161u, 162u, 163u, 164u, 1610u, 1612u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0xC1C2C3C4u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x7B100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved));
    Os_Port_Tms570_IrqContextSave();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedReturnAddress(OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)0xD1D2D3D4u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedCpsr(OS_PORT_TMS570_FIRST_TASK_ID, 0x7B200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedIrqScratch(OS_PORT_TMS570_FIRST_TASK_ID, &corrupted));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(0xC1C2C3C4u, (uint32)state->LastRestoredTaskReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x7B100013u, state->CurrentIrqSavedCpsr);
    TEST_ASSERT_EQUAL_UINT32(saved.R12, state->CurrentIrqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(saved.R10, state->LastRestoredTaskIrqScratch.R10);
}

void test_Os_Port_Tms570_RegisterIrqResumeContextSyncTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_resume_prefers_packed_restored_context_over_saved_frame_fields);
}
