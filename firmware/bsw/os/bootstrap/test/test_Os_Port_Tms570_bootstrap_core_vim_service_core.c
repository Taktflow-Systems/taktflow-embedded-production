/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_service_core.c
 * @brief   Active IRQ service core tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_service_active_irq_core_handles_selected_rti_without_irq_wrapper_counts(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SelectPendingIrq());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_ServiceActiveIrqCore());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
}

void test_Os_Port_Tms570_RegisterCoreVimServiceCoreTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_service_active_irq_core_handles_selected_rti_without_irq_wrapper_counts);
}
