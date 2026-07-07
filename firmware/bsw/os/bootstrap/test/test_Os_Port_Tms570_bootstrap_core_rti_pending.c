/**
 * @file    test_Os_Port_Tms570_bootstrap_core_rti_pending.c
 * @brief   RTI pending-flag gate tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_missing_rti_compare0_pending_flag_blocks_vim_delivery(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiIntFlag(0u));

    TEST_ASSERT_EQUAL(E_OS_NOFUNC,
                      Os_Port_Tms570_TestInvokeVimChannel(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(0u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->RtiCompare0AckCount);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_VIM_NO_CHANNEL, state->VimLastServicedChannel);
}

void test_Os_Port_Tms570_acknowledged_rti_compare0_requires_new_pending_flag_for_redelivery(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestInvokeVimChannel(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));
    TEST_ASSERT_EQUAL(E_OS_NOFUNC,
                      Os_Port_Tms570_TestInvokeVimChannel(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->RtiCompare0AckCount);
    TEST_ASSERT_EQUAL_HEX32(0u, state->RtiIntflag & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG);
}

void test_Os_Port_Tms570_RegisterCoreRtiPendingTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_missing_rti_compare0_pending_flag_blocks_vim_delivery);
    RUN_TEST(test_Os_Port_Tms570_acknowledged_rti_compare0_requires_new_pending_flag_for_redelivery);
}
