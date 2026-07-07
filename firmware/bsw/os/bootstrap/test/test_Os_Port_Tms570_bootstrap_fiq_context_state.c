/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_context_state.c
 * @brief   FIQ stacked context-state tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_save_restore_tracks_saved_cpsr_and_scratch_in_lifo_order(void)
{
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_IrqScratchSnapshotType outer = {31u, 32u, 33u, 34u, 310u, 312u};
    const Os_Port_Tms570_IrqScratchSnapshotType nested = {41u, 42u, 43u, 44u, 410u, 412u};

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x71000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&outer));
    Os_Port_Tms570_FiqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(0x71000013u, state->LastSavedFiqContextCpsr);
    TEST_ASSERT_EQUAL_UINT32(outer.R0, state->LastSavedFiqContextScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastSavedFiqContextScratch.R12);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x72000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&nested));
    Os_Port_Tms570_FiqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(0x72000013u, state->LastSavedFiqContextCpsr);
    TEST_ASSERT_EQUAL_UINT32(nested.R0, state->LastSavedFiqContextScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(nested.R12, state->LastSavedFiqContextScratch.R12);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x73000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&outer));
    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(0x72000013u, state->CurrentFiqSavedCpsr);
    TEST_ASSERT_EQUAL_HEX32(0x72000013u, state->LastRestoredFiqContextCpsr);
    TEST_ASSERT_EQUAL_UINT32(nested.R0, state->CurrentFiqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(nested.R12, state->CurrentFiqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(nested.R0, state->LastRestoredFiqContextScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(nested.R12, state->LastRestoredFiqContextScratch.R12);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x74000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&nested));
    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(0x71000013u, state->CurrentFiqSavedCpsr);
    TEST_ASSERT_EQUAL_HEX32(0x71000013u, state->LastRestoredFiqContextCpsr);
    TEST_ASSERT_EQUAL_UINT32(outer.R0, state->CurrentFiqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->CurrentFiqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(outer.R0, state->LastRestoredFiqContextScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastRestoredFiqContextScratch.R12);
}

void test_Os_Port_Tms570_RegisterFiqContextStateTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_save_restore_tracks_saved_cpsr_and_scratch_in_lifo_order);
}
