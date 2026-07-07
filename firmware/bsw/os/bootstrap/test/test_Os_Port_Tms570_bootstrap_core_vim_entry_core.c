/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_entry_core.c
 * @brief   VIM IRQ entry core tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_vim_irq_entry_core_selects_and_services_pending_rti_without_irq_wrapper_counts(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_VimIrqEntryCore());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
}

void test_Os_Port_Tms570_RegisterCoreVimEntryCoreTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_vim_irq_entry_core_selects_and_services_pending_rti_without_irq_wrapper_counts);
}
