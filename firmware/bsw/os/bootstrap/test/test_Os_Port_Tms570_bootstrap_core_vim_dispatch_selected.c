/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_dispatch_selected.c
 * @brief   Pending-dispatch tests with preselected active IRQ for TMS570
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_dispatch_pending_irq_services_preselected_active_irq(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SelectPendingIrq());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_DispatchPendingIrq());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->VimIrqIndex);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->VimIrqVecReg);
}

void test_Os_Port_Tms570_RegisterCoreVimDispatchSelectedTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_dispatch_pending_irq_services_preselected_active_irq);
}
