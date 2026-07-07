/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_service.c
 * @brief   Active IRQ service tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_service_active_irq_returns_nofunc_without_selection(void)
{
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Port_Tms570_ServiceActiveIrq());
}

void test_Os_Port_Tms570_service_active_irq_handles_selected_rti_compare0(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SelectPendingIrq());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_ServiceActiveIrq());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->VimIrqIndex);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->VimIrqVecReg);
}

void test_Os_Port_Tms570_RegisterCoreVimServiceTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_service_active_irq_returns_nofunc_without_selection);
    RUN_TEST(test_Os_Port_Tms570_service_active_irq_handles_selected_rti_compare0);
}
