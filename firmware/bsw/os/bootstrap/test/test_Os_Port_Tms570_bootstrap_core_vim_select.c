/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_select.c
 * @brief   Active pending-IRQ selection tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_select_pending_irq_returns_nofunc_without_request(void)
{
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Port_Tms570_TestSelectPendingIrq());
}

void test_Os_Port_Tms570_select_pending_irq_latches_active_index_and_vector(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SelectPendingIrq());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_VIM_RTI_COMPARE0_IRQINDEX, state->VimIrqIndex);
    TEST_ASSERT_TRUE(state->VimIrqVecReg == state->VimRtiCompare0HandlerAddress);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TickInterruptCount);
}

void test_Os_Port_Tms570_RegisterCoreVimSelectTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_select_pending_irq_returns_nofunc_without_request);
    RUN_TEST(test_Os_Port_Tms570_select_pending_irq_latches_active_index_and_vector);
}
