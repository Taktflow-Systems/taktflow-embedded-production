/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_context_cpsr.c
 * @brief   IRQ context CPSR stack tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_save_restore_tracks_saved_cpsr_in_lifo_order(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x11111111u));
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(0x11111111u, state->LastSavedIrqContextCpsr);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x22222222u));
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(0x22222222u, state->LastSavedIrqContextCpsr);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x33333333u));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(0x22222222u, state->CurrentIrqSavedCpsr);
    TEST_ASSERT_EQUAL_HEX32(0x22222222u, state->LastRestoredIrqContextCpsr);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x44444444u));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(0x11111111u, state->CurrentIrqSavedCpsr);
    TEST_ASSERT_EQUAL_HEX32(0x11111111u, state->LastRestoredIrqContextCpsr);
}

void test_Os_Port_Tms570_RegisterIrqContextCpsrTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_save_restore_tracks_saved_cpsr_in_lifo_order);
}
