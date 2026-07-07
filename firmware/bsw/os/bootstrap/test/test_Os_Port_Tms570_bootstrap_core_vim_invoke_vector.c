/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_invoke_vector.c
 * @brief   Active IRQ vector invocation tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_invoke_active_irq_vector_core_returns_nofunc_without_selection(void)
{
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Port_Tms570_InvokeActiveIrqVectorCore());
}

void test_Os_Port_Tms570_invoke_active_irq_vector_core_runs_rti_service_core(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SelectPendingIrq());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_InvokeActiveIrqVectorCore());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
}

void test_Os_Port_Tms570_RegisterCoreVimInvokeVectorTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_invoke_active_irq_vector_core_returns_nofunc_without_selection);
    RUN_TEST(test_Os_Port_Tms570_invoke_active_irq_vector_core_runs_rti_service_core);
}
