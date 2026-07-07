/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_pulse_mask.c
 * @brief   Active IRQ mask pulse tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_pulse_active_irq_mask_returns_nofunc_without_selection(void)
{
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Port_Tms570_PulseActiveIrqMask());
}

void test_Os_Port_Tms570_pulse_active_irq_mask_pulses_reqmask_for_selected_rti_channel(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SelectPendingIrq());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PulseActiveIrqMask());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK, state->VimReqmaskclr0);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK,
                            state->VimReqmaskset0 & OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK);
}

void test_Os_Port_Tms570_RegisterCoreVimPulseMaskTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_pulse_active_irq_mask_returns_nofunc_without_selection);
    RUN_TEST(test_Os_Port_Tms570_pulse_active_irq_mask_pulses_reqmask_for_selected_rti_channel);
}
