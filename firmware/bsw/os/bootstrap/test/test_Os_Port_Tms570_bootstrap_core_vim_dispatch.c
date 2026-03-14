/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_dispatch.c
 * @brief   Generic pending-IRQ dispatch tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_dispatch_pending_irq_returns_nofunc_without_latched_request(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Port_Tms570_DispatchPendingIrq());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
}

void test_Os_Port_Tms570_dispatch_pending_irq_services_latched_rti_compare0_request(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_DispatchPendingIrq());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_HEX32(0u, state->VimIntreq0);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK, state->VimReqmaskclr0);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK,
                            state->VimReqmaskset0 & OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_VIM_RTI_COMPARE0_IRQINDEX, state->VimLastIrqIndex);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL, state->VimLastServicedChannel);
}

void test_Os_Port_Tms570_RegisterCoreVimDispatchTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_dispatch_pending_irq_returns_nofunc_without_latched_request);
    RUN_TEST(test_Os_Port_Tms570_dispatch_pending_irq_services_latched_rti_compare0_request);
}
