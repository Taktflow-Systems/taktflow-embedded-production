/**
 * @file    test_Os_Port_Tms570_bootstrap_core_target_hw.c
 * @brief   RTI/VIM bootstrap hardware-model tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec Local TMS570 reference: firmware/ecu/sc/halcogen/source/HL_rti.c,
 *       firmware/ecu/sc/halcogen/source/HL_sys_vim.c
 * @requirement The bootstrap TMS570 port shall model the local RTI compare 0
 *              and VIM channel 2 setup used for the system tick.
 * @verify Target init programs the RTI/VIM register image for compare 0 as an
 *         IRQ-driven 10 ms tick source.
 */
void test_Os_Port_Tms570_target_init_configures_rti_compare0_on_vim_channel2(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_HEX32(0u, state->VimFirqpr0 & OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK, state->VimReqmaskset0);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_CHANCTRL0_DEFAULT, state->VimChanctrl0);
    TEST_ASSERT_EQUAL_HEX32(
        OS_PORT_TMS570_RTI_GCTRL_CLOCK_SOURCE | OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE,
        state->RtiGctrl);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPCTRL_DEFAULT, state->RtiCompctrl);
    TEST_ASSERT_EQUAL_UINT32(0u, state->RtiCounter0Value);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_RTI_COMPARE0_PERIOD, state->RtiCmp0Comp);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_RTI_COMPARE0_PERIOD, state->RtiCmp0Udcp);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG, state->RtiSetintena);
    TEST_ASSERT_EQUAL_HEX32(0u, state->RtiIntflag);
    TEST_ASSERT_EQUAL_UINT32(0u, state->RtiCompare0AckCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->VimLastIrqIndex);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->VimIrqVecReg);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->VimLastIrqVecReg);
}

/**
 * @spec Local TMS570 reference: firmware/ecu/sc/halcogen/source/HL_rti.c
 * @requirement The bootstrap TMS570 RTI tick wrapper shall acknowledge the
 *              compare 0 interrupt flag with write-1-to-clear semantics.
 * @verify RtiTickHandler clears only the compare 0 flag bit and records one
 *         compare 0 acknowledgement.
 */
void test_Os_Port_Tms570_rti_tick_handler_acknowledges_compare0_flag(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_TestSetRtiIntFlag(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG | 0x08u));

    Os_Port_Tms570_RtiTickHandler();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_HEX32(0x08u, state->RtiIntflag);
    TEST_ASSERT_EQUAL_UINT32(1u, state->RtiCompare0AckCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
}

/**
 * @spec Local TMS570 reference: firmware/ecu/sc/halcogen/source/HL_rti.c
 * @requirement The bootstrap TMS570 RTI compare-0 model shall advance the
 *              programmed compare value by the update compare value on each
 *              acknowledged compare-0 match.
 * @verify A compare-0 tick acknowledgement advances `CMP0COMP` by `UDCP0`.
 */
void test_Os_Port_Tms570_rti_tick_handler_advances_compare0_by_update_value(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiIntFlag(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG));

    Os_Port_Tms570_RtiTickHandler();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(
        OS_PORT_TMS570_RTI_COMPARE0_PERIOD + OS_PORT_TMS570_RTI_COMPARE0_PERIOD,
        state->RtiCmp0Comp);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_RTI_COMPARE0_PERIOD, state->RtiCmp0Udcp);
}

void test_Os_Port_Tms570_RegisterCoreTargetHwTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_target_init_configures_rti_compare0_on_vim_channel2);
    RUN_TEST(test_Os_Port_Tms570_rti_tick_handler_acknowledges_compare0_flag);
    RUN_TEST(test_Os_Port_Tms570_rti_tick_handler_advances_compare0_by_update_value);
}
