/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_resume_context_sync.c
 * @brief   FIQ resume sync tests for packed restored context
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_resume_prefers_packed_restored_context_over_saved_frame_fields(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {171u, 172u, 173u, 174u, 1710u, 1712u};
    const Os_Port_Tms570_IrqScratchSnapshotType corrupted = {181u, 182u, 183u, 184u, 1810u, 1812u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xE1E2E3E4u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x7C100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&saved));
    Os_Port_Tms570_FiqContextSave();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedReturnAddress(OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)0xF1F2F3F4u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedCpsr(OS_PORT_TMS570_FIRST_TASK_ID, 0x7C200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedIrqScratch(OS_PORT_TMS570_FIRST_TASK_ID, &corrupted));
    Os_Port_Tms570_FiqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(0xE1E2E3E4u, (uint32)state->LastRestoredTaskReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x7C100013u, state->CurrentFiqSavedCpsr);
    TEST_ASSERT_EQUAL_UINT32(0u, state->CurrentFiqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastRestoredTaskIrqScratch.R12);
}

void test_Os_Port_Tms570_RegisterFiqResumeContextSyncTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_resume_prefers_packed_restored_context_over_saved_frame_fields);
}
