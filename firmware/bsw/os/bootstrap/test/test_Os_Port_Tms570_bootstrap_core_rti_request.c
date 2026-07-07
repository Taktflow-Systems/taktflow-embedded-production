/**
 * @file    test_Os_Port_Tms570_bootstrap_core_rti_request.c
 * @brief   RTI-to-VIM request latch tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_raising_rti_compare0_latches_vim_request_when_delivery_is_open(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG, state->RtiIntflag);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK, state->VimIntreq0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TickInterruptCount);
}

void test_Os_Port_Tms570_raising_rti_compare0_keeps_only_source_pending_when_vim_is_blocked(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetVimChannelEnabled(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL, FALSE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG, state->RtiIntflag);
    TEST_ASSERT_EQUAL_HEX32(0u, state->VimIntreq0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TickInterruptCount);
}

void test_Os_Port_Tms570_RegisterCoreRtiRequestTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_raising_rti_compare0_latches_vim_request_when_delivery_is_open);
    RUN_TEST(test_Os_Port_Tms570_raising_rti_compare0_keeps_only_source_pending_when_vim_is_blocked);
}
