/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_context_scratch.c
 * @brief   IRQ context scratch-stack tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_save_restore_tracks_saved_scratch_in_lifo_order(void)
{
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_IrqScratchSnapshotType outer = {1u, 2u, 3u, 4u, 10u, 12u};
    const Os_Port_Tms570_IrqScratchSnapshotType nested = {21u, 22u, 23u, 24u, 30u, 32u};

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&outer));
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(outer.R1, state->LastSavedIrqContextScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(outer.R12, state->LastSavedIrqContextScratch.R12);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&nested));
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(nested.R1, state->LastSavedIrqContextScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(nested.R12, state->LastSavedIrqContextScratch.R12);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&outer));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(nested.R1, state->CurrentIrqScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(nested.R12, state->CurrentIrqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(nested.R1, state->LastRestoredIrqContextScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(nested.R12, state->LastRestoredIrqContextScratch.R12);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&nested));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(outer.R1, state->CurrentIrqScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(outer.R12, state->CurrentIrqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(outer.R1, state->LastRestoredIrqContextScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(outer.R12, state->LastRestoredIrqContextScratch.R12);
}

void test_Os_Port_Tms570_RegisterIrqContextScratchTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_save_restore_tracks_saved_scratch_in_lifo_order);
}
