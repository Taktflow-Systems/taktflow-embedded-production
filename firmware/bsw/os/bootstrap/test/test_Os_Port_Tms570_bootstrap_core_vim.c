/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim.c
 * @brief   VIM channel bootstrap tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_vim_channel2_invokes_rti_tick_handler_when_enabled(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestInvokeVimChannel(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->VimRtiCompare0HandlerAddress != (uintptr_t)0u);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_CHANCTRL0_DEFAULT, state->VimChanctrl0);
    TEST_ASSERT_EQUAL_HEX32(0u, state->VimIntreq0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->VimIrqIndex);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->VimIrqVecReg);
    TEST_ASSERT_TRUE(state->VimRtiCompare0HandlerAddress == state->VimLastIrqVecReg);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL, state->VimLastServicedChannel);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->RtiCompare0AckCount);
}

void test_Os_Port_Tms570_disabled_vim_channel2_blocks_rti_tick_handler(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetVimChannelEnabled(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL, FALSE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());

    TEST_ASSERT_EQUAL(E_OS_NOFUNC,
                      Os_Port_Tms570_TestInvokeVimChannel(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(0u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->RtiCompare0AckCount);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK, state->VimReqmaskclr0);
    TEST_ASSERT_EQUAL_HEX32(0u, state->VimIntreq0);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_VIM_NO_CHANNEL, state->VimLastServicedChannel);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->VimIrqVecReg);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->VimLastIrqVecReg);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG, state->RtiIntflag);
}

void test_Os_Port_Tms570_RegisterCoreVimTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_vim_channel2_invokes_rti_tick_handler_when_enabled);
    RUN_TEST(test_Os_Port_Tms570_disabled_vim_channel2_blocks_rti_tick_handler);
}
