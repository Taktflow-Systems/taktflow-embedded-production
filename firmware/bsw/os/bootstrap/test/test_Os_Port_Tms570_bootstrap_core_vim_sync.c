/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_sync.c
 * @brief   VIM request resynchronization tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_enabling_vim_channel2_after_pending_compare0_latches_request(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetVimChannelEnabled(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL, FALSE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetVimChannelEnabled(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL, TRUE));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG, state->RtiIntflag);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK, state->VimIntreq0);
}

void test_Os_Port_Tms570_RegisterCoreVimSyncTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_enabling_vim_channel2_after_pending_compare0_latches_request);
}
