/**
 * @file    test_Os_Port_Tms570_bootstrap_core_rti_counter.c
 * @brief   RTI counter gate tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_disabled_rti_counter0_blocks_vim_delivery(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiCounter0Enabled(FALSE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiCompare0NotificationEnabled(TRUE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());

    TEST_ASSERT_EQUAL(E_OS_NOFUNC,
                      Os_Port_Tms570_TestInvokeVimChannel(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(0u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_HEX32(0u, state->RtiGctrl & OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE);
    TEST_ASSERT_EQUAL_HEX32(0u, state->VimIntreq0);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG, state->RtiIntflag);
}

void test_Os_Port_Tms570_reenabled_rti_counter0_allows_vim_delivery(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiCounter0Enabled(FALSE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiCounter0Enabled(TRUE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestInvokeVimChannel(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE,
                            state->RtiGctrl & OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE);
    TEST_ASSERT_EQUAL_UINT32(1u, state->RtiCompare0AckCount);
}

void test_Os_Port_Tms570_RegisterCoreRtiCounterTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_disabled_rti_counter0_blocks_vim_delivery);
    RUN_TEST(test_Os_Port_Tms570_reenabled_rti_counter0_allows_vim_delivery);
}
